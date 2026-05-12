# Real-time in-memory value compression — Phase 0 transparency fixture.
#
# The compression feature is disabled by default in Phase 0. This fixture
# verifies the two invariants that MUST hold before any code is allowed
# to modify the hot path in Phase 1:
#
#   1. All compression-* knobs exist, parse, and have the defaults
#      documented in design §2.12.
#   2. The feature-off server behaves IDENTICALLY to a baseline server
#      for a representative slice of STRING commands (transparency).
#
# Design of record:
#   .agents/planning/realtime-data-compression/design/detailed-design.md
#   .agents/planning/realtime-data-compression/implementation/plan.md §5
#
# This file is intentionally short. The §7 "transparency mode" harness
# (Phase 1 — dev infra subsystem) adds a `--compression` flag to runtest
# that re-runs the entire existing Tcl corpus against a server with
# aggressive compression enabled; that is where the real coverage will
# come from.

start_server {tags {"compression"}} {
    test {COMPRESSION STATUS returns disabled state} {
        set status [r compression status]
        assert_match "*compression_enabled:0*" $status
        assert_match "*compression_state:disabled*" $status
        assert_match "*compression_active_dict_id:0*" $status
    }

    test {COMPRESSION HELP returns helpful text} {
        set help [r compression help]
        assert {[llength $help] > 0}
    }

    test {INFO compression section renders} {
        set info [r info compression]
        assert_match "*# Compression*" $info
        assert_match "*compression_enabled:0*" $info
    }

    test {All 16 compression config knobs are registered with documented defaults} {
        # Primary (5) — see design/detailed-design.md §2.12
        assert_equal [lindex [r config get compression-enabled] 1] "no"
        assert_equal [lindex [r config get compression-threads] 1] "1"
        assert_equal [lindex [r config get compression-min-value-size] 1] "256"
        assert_equal [lindex [r config get compression-max-value-size] 1] "131072"
        assert_equal [lindex [r config get compression-dict-size] 1] "102400"
        # Advanced (11)
        assert_equal [lindex [r config get compression-sweep-max-cpu-pct] 1] "25"
        assert_equal [lindex [r config get compression-cpulist] 1] ""
        assert_equal [lindex [r config get compression-min-savings-ratio] 1] "10"
        assert_equal [lindex [r config get compression-retry-interval] 1] "3600"
        assert_equal [lindex [r config get compression-lfu-threshold] 1] "5"
        assert_equal [lindex [r config get compression-min-idle-seconds] 1] "60"
        assert_equal [lindex [r config get compression-settle-seconds] 1] "60"
        assert_equal [lindex [r config get compression-dict-first-training-keys-count] 1] "10000"
        assert_equal [lindex [r config get compression-dict-drift-ratio] 1] "70"
        assert_equal [lindex [r config get compression-dict-refresh-interval] 1] "0"
        assert_equal [lindex [r config get compression-dict-max-versions] 1] "4"
    }

    test {Feature-off transparency: basic STRING round-trip is unchanged} {
        r set foo "bar"
        assert_equal [r get foo] "bar"
        assert_equal [r strlen foo] 3
        r append foo "baz"
        assert_equal [r get foo] "barbaz"
    }

    test {Feature-off transparency: large STRING round-trip is unchanged} {
        set buf [string repeat "abcd" 10000]
        r set big $buf
        assert_equal [r get big] $buf
        assert_equal [r strlen big] [string length $buf]
    }

    test {Feature-off transparency: OBJECT ENCODING is not 'compressed'} {
        r set small "x"
        assert_equal [r object encoding small] "embstr"
        set buf [string repeat "y" 1024]
        r set medium $buf
        # Feature-off means NO value is ever encoded as 'compressed'.
        # After Phase 1 this assertion will still hold when the feature
        # is disabled (default), but will fail under the transparency
        # harness (which forces feature-on); at that point the test
        # will be tagged compression:skip or rewritten via
        # assert_string_encoding (plan §7.1).
        assert_equal [r object encoding medium] "raw"
    }

    test {Toggling compression-enabled at runtime has no observable effect in Phase 0} {
        r config set compression-enabled yes
        assert_equal [lindex [r config get compression-enabled] 1] "yes"
        # Feature stays functionally disabled in Phase 0 — STATUS still
        # reports disabled state since the worker pool is a stub.
        set status [r compression status]
        assert_match "*compression_enabled:0*" $status
        r config set compression-enabled no
    }
}
