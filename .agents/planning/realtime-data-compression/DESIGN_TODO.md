# DESIGN_TODO — PR review-task log

_Generated from [PR #1](https://github.com/ikolomi/valkey/pull/1) ("Inline compression design (initial draft)") at 2026-05-11T11:22:31+00:00._

_Workflow: each task has a stable ID (`T-<short>`). Agents read `status`/`decision` as the source of truth for what's still open. After updating the design, set `status: addressed` and fill `decision:`; re-running the normalizer preserves these fields._

_Legend: `status` ∈ {`open`, `needs-discussion`, `addressed`, `wont-fix`, `duplicate`}._


## .agents/planning/realtime-data-compression/design/detailed-design.md

### #1 · `T-3194682164` · line 121 · @ikolomi  {#T-3194682164}

- **status:** `addressed`
- **decision:** Option A — lowered `compression-max-value-size` default from 1 MiB → 128 KiB (bounds worst-case sync decompression on main thread); rewrote R2.5.4–R2.5.5 to drop the "low latency" framing and explicitly document that multi-key commands pay the sum of per-value costs; v1 is tuned for 256 B – 8 KB sweet spot; no per-command cap (would break transparency or defeat itself). Updated both detailed-design.md §2.5 + §2.12 config table and idea-honing.md Q7.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T10:22:15Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3194682164

**context (what was commented on):**

>   - Does **not** call `signalModifiedKey`. Background compression is a storage change, not a logical value change. (Q11)
> 
> ### 2.5 Decompression path

**comment:**

I have concern that protection for sync decompression focuses on per-value latency. The real risk is command-level amplification:

MGET with 1,000 compressed 8KB values.
Lua script touching many compressed keys.
SORT, STRALGO, or module code reading many strings.
Pipeline workload where one connection issues many large reads.

The design has compression-max-value-size = 1 MiB, but that only bounds per-value cost, not per-command cost.

Some options to consider:
- Lower default compression-max-value-size, maybe 64KB or 128KB, not 1MiB.
- Add compression-max-sync-decompress-bytes-per-command.
- Document that v1 is intended for small/moderate values and benchmark MGET explicitly.

For an OSS proposal, I would avoid claiming “low latency” unless we have P99/P999 data for multi-key reads.


**↳ reply by @ikolomi (2026-05-11T09:56:13Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — lowered `compression-max-value-size` default from 1 MiB → 128 KiB (bounds worst-case sync decompression on main thread); rewrote R2.5.4–R2.5.5 to drop the "low latency" framing and explicitly document that multi-key commands pay the sum of per-value costs; v1 is tuned for 256 B – 8 KB sweet spot; no per-command cap (would break transparency or defeat itself). Updated both detailed-design.md §2.5 + §2.12 config table and idea-honing.md Q7.

_Tracking: [`DESIGN_TODO.md` · T-3194682164](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3194682164)_


### #2 · `T-3194623867` · line 131 · @ikolomi  {#T-3194623867}

- **status:** `addressed`
- **decision:** Option A — added **R2.6.8** to detailed-design.md §2.6: full-sync replication RDB (primary → replica during SYNC/PSYNC) is always emitted **uncompressed** in v1, regardless of `compression-enabled` on the primary; disk RDB still uses the `RDB_ENC_ZSTDDICT` path (R2.6.1). This preserves cross-version replication without replica-side awareness. Updated Appendix D: compressed full-sync moved from "Conditional" to "Planned" for v2 via `REPLCONF compression yes` negotiation. Also resolves Thread #31 (`T-3168318044`).
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T10:11:15Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3194623867

**context (what was commented on):**

> - **R2.5.5** Large-value workloads that need non-blocking reads are directed to either (a) raise `compression-min-value-size` or lower `compression-max-value-size` to opt them out, or (b) wait for v2 async decompression. (Q7)
> 
> ### 2.6 Persistence

**comment:**

The design says AOF and the steady-state replication feed stay uncompressed RESP, which preserves cross-version replication. It also says RDB on disk uses a new RDB_ENC_ZSTDDICT marker and bumps RDB version.

The ambiguity: Valkey full sync uses an RDB image. So is the RDB sent during full sync compressed or uncompressed?

Is the following true?

- Disk RDB may be compressed.
- Replication full-sync RDB should be emitted uncompressed by default, unless both sides negotiate compression support.
- Add an explicit config later for compressed full sync.


**↳ reply by @ikolomi (2026-05-11T09:56:16Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — added **R2.6.8** to detailed-design.md §2.6: full-sync replication RDB (primary → replica during SYNC/PSYNC) is always emitted **uncompressed** in v1, regardless of `compression-enabled` on the primary; disk RDB still uses the `RDB_ENC_ZSTDDICT` path (R2.6.1). This preserves cross-version replication without replica-side awareness. Updated Appendix D: compressed full-sync moved from "Conditional" to "Planned" for v2 via `REPLCONF compression yes` negotiation. Also resolves Thread #31 (`T-3168318044`).

_Tracking: [`DESIGN_TODO.md` · T-3194623867](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3194623867)_


### #3 · `T-3194703979` · line 179 · @ikolomi  {#T-3194703979}

- **status:** `addressed`
- **decision:** Option A — split §2.12 config table into two tiers (Primary: 5 knobs, Advanced: 11 knobs) with a documentation-only distinction. All configs remain in code, `CONFIG GET *`, and modifiable at runtime. `valkey.conf` will mirror the split via a `# Compression — advanced tuning` divider. No new hiding mechanism introduced.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T10:26:16Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3194703979

**context (what was commented on):**

> - **R2.11.4** Compression workers never touch `robj`. They consume and produce flat byte buffers. The main thread owns all `robj` mutation. (Q8)
> 
> ### 2.12 Configuration summary

**comment:**

The configuration surface is large: min/max size, min savings, retry interval, LFU threshold, LRU idle seconds, settle seconds, dict size, first training count, drift ratio, refresh interval, dict max versions, worker count, sweep CPU, cpulist, master switch.

For a production system this is useful. For upstream Valkey, it may look too complex.

Consider spliting into:

Primary knobs:
compression-enabled
compression-min-value-size
compression-max-value-size
compression-threads
compression-dict-size

Advanced knobs:
Everything else, documented as expert-only.

This makes the proposal easier to accept.


**↳ reply by @ikolomi (2026-05-11T09:56:19Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — split §2.12 config table into two tiers (Primary: 5 knobs, Advanced: 11 knobs) with a documentation-only distinction. All configs remain in code, `CONFIG GET *`, and modifiable at runtime. `valkey.conf` will mirror the split via a `# Compression — advanced tuning` divider. No new hiding mechanism introduced.

_Tracking: [`DESIGN_TODO.md` · T-3194703979](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3194703979)_


### #4 · `T-3194330520` · line 429 · @ikolomi  {#T-3194330520}

- **status:** `addressed`
- **decision:** Option B — rely on existing Valkey `dbUnshareStringValue` COW discipline, no memcpy at enqueue. The `incrRefCount(val)` held by the compression job forces `refcount >= 2`, which triggers COW on any subsequent mutation. Added four concrete design artifacts: (1) R2.4.4 states the immutable-snapshot invariant precisely; (2) R2.4.5 enumerates every mutating code path that must go through `dbUnshareStringValue` (t_string.c, bitops.c, module.c DMA-write, debug.c) as a merge-blocker audit checklist; (3) R2.4.6 adds a `#ifdef DEBUG_COMPRESSION_SNAPSHOT` belt-and-suspenders memcmp guard (zero prod cost); (4) new §7.2 test `compression-cow-invariant.tcl` exercises every mutating command against a live compression job to catch regressions. §4.6 concurrency notes rewritten to make the refcount-2→COW chain explicit. Appendix-D-style note: if the audit reveals code paths that can't be made to honor the invariant (e.g. a module with direct sds mutation), we fall back to Option A (memcpy at enqueue) for affected paths.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T09:23:37Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3194330520

**context (what was commented on):**

> 
> **Concurrency notes**:
> - Enqueue holds `incrRefCount(val)` so the sds pointer stays valid for the worker.

**comment:**

That is probably not sufficient unless you guarantee that the SDS bytes cannot be mutated while the worker is reading them.

In Valkey string operations like APPEND, SETRANGE, bit operations, or module write-DMA may mutate or replace string storage. A refcount protects the object from being freed; it does not automatically make the SDS contents immutable if the same object is modified in place before the worker completes.

A background compression job may only read an immutable byte snapshot. Either the server copies the value bytes into the job, or any command that would mutate the SDS must first detach/replace the object when a compression job holds a reference.

Copying hurts memory, but it is much simpler and safer for v1. A middle ground is: copy only when the value is below some size, and for larger values use a “pinned immutable robj” flag that forces copy-on-write on mutation. But without one of these, this design risks rare data corruption.


**↳ reply by @ikolomi (2026-05-11T09:56:22Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option B — rely on existing Valkey `dbUnshareStringValue` COW discipline, no memcpy at enqueue. The `incrRefCount(val)` held by the compression job forces `refcount >= 2`, which triggers COW on any subsequent mutation. Added four concrete design artifacts: (1) R2.4.4 states the immutable-snapshot invariant precisely; (2) R2.4.5 enumerates every mutating code path that must go through `dbUnshareStringValue` (t_string.c, bitops.c, module.c DMA-write, debug.c) as a merge-blocker audit checklist; (3) R2.4.6 adds a `#ifdef DEBUG_COMPRESSION_SNAPSHOT` belt-and-suspenders memcmp guard (zero prod cost); (4) new §7.2 test `compression-cow-invariant.tcl` exercises every mutating command against a live compression job to catch regressions. §4.6 concurrency notes rewritten to make the refcount-2→COW chain explicit. Appendix-D-style note: if the audit reveals code paths that can't be made to honor the invariant (e.g. a module with direct sds mutation), we fall back to Option A (memcpy at enqueue) for affected paths.

_Tracking: [`DESIGN_TODO.md` · T-3194330520](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3194330520)_



## .agents/planning/realtime-data-compression/idea-honing.md

### #5 · `T-3194800295` · line 1 · @ikolomi  {#T-3194800295}

- **status:** `addressed`
- **decision:** Option A — added **§7.5 Compression-aware benchmark suite** to detailed-design.md committing v1 to: (1) extend `valkey-benchmark` with three general-purpose flags — `--key-distribution uniform|zipf`, `--value-size-distribution constant|uniform|lognormal`, `--value-data random|zero|corpus:FILE`; (2) ship a canonical scenario set under `tests/compression/benchmarks/` — baseline-uniform-1k, realistic-hotset, wide-mget, sort-heavy, mixed-pipeline; (3) a `run.sh` driver that produces comparison reports (P50/P99/P999, used_memory, compression_ratio) and commits reference JSON to the repo. CI runs baseline nightly/on-release, full suite is informational per §7.3 policy. Rejected a separate tool; the three flags are useful beyond compression, which strengthens the upstream case.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T10:42:25Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3194800295

**comment:**

This feature should include a new tool (or extend the exicting like valkey-benchmark) which will allow to measure the impact of the compression in terms of memory and latency.

One natural approach is to extend the valkey-benchmark tool to accomodate configurations which are relevant for the real time compression. valkey-benchmark is already able to generate load and measure the latency, so maybe its the best candidate as a benchmarking tool for this feature.

We need to ensure that the tool is able to generate workloads that are relevant to this feature. e.g. we aim to minimize the latency impact by not compressing "hot" items, This requires keys distribution not to be uniform. Currently the tool not nessesarily support that.

We might also consider to implement a new tool which will be more specific for this feature.


**↳ reply by @ikolomi (2026-05-11T09:56:24Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — added **§7.5 Compression-aware benchmark suite** to detailed-design.md committing v1 to: (1) extend `valkey-benchmark` with three general-purpose flags — `--key-distribution uniform|zipf`, `--value-size-distribution constant|uniform|lognormal`, `--value-data random|zero|corpus:FILE`; (2) ship a canonical scenario set under `tests/compression/benchmarks/` — baseline-uniform-1k, realistic-hotset, wide-mget, sort-heavy, mixed-pipeline; (3) a `run.sh` driver that produces comparison reports (P50/P99/P999, used_memory, compression_ratio) and commits reference JSON to the repo. CI runs baseline nightly/on-release, full suite is informational per §7.3 policy. Rejected a separate tool; the three flags are useful beyond compression, which strengthens the upstream case.

_Tracking: [`DESIGN_TODO.md` · T-3194800295](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3194800295)_


### #6 · `T-3187192908` · line 9 · @GilboaAWS  {#T-3187192908}

- **status:** `addressed`
- **decision:** Option B — no doc edit. Q1 covers the version cap; training triggers live in Q9. GitHub reply to post during round-trip: _"Good question — training triggers are covered in Q9 (first-training after `compression-dict-first-training-keys-count` eligible keys, drift-based retraining when live ratio regresses, optional time-based cadence via `compression-dict-refresh-interval`, manual `COMPRESSION TRAIN`). Q1 is scoped to the version cap that bounds how many trained versions can coexist."_
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T09:01:31Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3187192908

**context (what was commented on):**

> ## Q1. Dictionary-version cap
> 
> How many zstd dictionary versions should the server keep live at the same time?

**comment:**

What does trigger a new dict?


**↳ reply by @ikolomi (2026-05-11T09:56:27Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option B — no doc edit. Q1 covers the version cap; training triggers live in Q9. GitHub reply to post during round-trip: _"Good question — training triggers are covered in Q9 (first-training after `compression-dict-first-training-keys-count` eligible keys, drift-based retraining when live ratio regresses, optional time-based cadence via `compression-dict-refresh-interval`, manual `COMPRESSION TRAIN`). Q1 is scoped to the version cap that bounds how many trained versions can coexist."_

_Tracking: [`DESIGN_TODO.md` · T-3187192908](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3187192908)_


### #7 · `T-3187720406` · line 66 · @GilboaAWS  {#T-3187720406}

- **status:** `addressed`
- **decision:** Option A — updated §1.1 Overview in detailed-design.md: changed "decompressed on any read path" → "decompressed on every **client-facing read path**" (lists the boundaries explicitly), and added a follow-on sentence making the disk-RDB exception explicit: *"the on-disk RDB file may contain compressed values … see §2.6 for the full persistence-boundary specification; full-sync replication RDB streams are always uncompressed per R2.6.8."* Also resolves the "transparency" framing for future readers. No idea-honing.md edit — the reviewer quoted from the design doc.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T10:28:31Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3187720406

**context (what was commented on):**

> 
> If a Valkey server has `compression-enabled no` and is asked to load an RDB that contains compressed values (emitted by a prior run with the feature enabled), what should happen?
> 

**comment:**

From the detailed-design:
```
This feature adds opt-in, transparent, server-side, in-memory compression for OBJ_STRING values. Compressed values are automatically decompressed on any read path (client commands, scripts, transactions, replication, AOF, modules),
```
So as I understand, in RDB file there should be no compressed values.


**↳ reply by @ikolomi (2026-05-11T09:56:29Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — updated §1.1 Overview in detailed-design.md: changed "decompressed on any read path" → "decompressed on every **client-facing read path**" (lists the boundaries explicitly), and added a follow-on sentence making the disk-RDB exception explicit: *"the on-disk RDB file may contain compressed values … see §2.6 for the full persistence-boundary specification; full-sync replication RDB streams are always uncompressed per R2.6.8."* Also resolves the "transparency" framing for future readers. No idea-honing.md edit — the reviewer quoted from the design doc.

_Tracking: [`DESIGN_TODO.md` · T-3187720406](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3187720406)_


### #8 · `T-3187833479` · line 87 · @GilboaAWS  {#T-3187833479}

- **status:** `addressed`
- **decision:** Option A — replaced the ambiguous word "rebuilds" in 4 locations (idea-honing.md lines 74 & 87 in Q3; detailed-design.md R2.6.2 & R2.6.3). New wording: *"initializes the needed `ZSTD_DDict` handle from the dictionary bytes stored in the preceding AUX entries (via `ZSTD_createDDict()` — the dictionary itself is already on disk, not retrained)"*. Makes it unambiguous that the raw dictionary bytes are persisted in the RDB's AUX section and the load path just creates a usable handle from them; no retraining happens. Also resolves Thread #9 (`T-3195083121`) which raised the same concern from @ikolomi's self-review.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T10:49:36Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3187833479

**context (what was commented on):**

> **Answer:** Option 2 — **transparently decompress on load**.
> 
> - When the loader encounters `RDB_ENC_ZSTDDICT` while `compression-enabled no`, it rebuilds the needed `ZSTD_DDict` from the preceding AUX entries, decompresses each value inline, and stores the result uncompressed.

**comment:**

Does it mean a new dictionary is built here? I'm not sure it's possible to use this newly built dict to decomp those value.


**↳ reply by @ikolomi (2026-05-06T11:05:22Z):**

no, it does not rebuilt, the dict is stored within the rdb


**↳ reply by @ikolomi (2026-05-11T09:56:31Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — replaced the ambiguous word "rebuilds" in 4 locations (idea-honing.md lines 74 & 87 in Q3; detailed-design.md R2.6.2 & R2.6.3). New wording: *"initializes the needed `ZSTD_DDict` handle from the dictionary bytes stored in the preceding AUX entries (via `ZSTD_createDDict()` — the dictionary itself is already on disk, not retrained)"*. Makes it unambiguous that the raw dictionary bytes are persisted in the RDB's AUX section and the load path just creates a usable handle from them; no retraining happens. Also resolves Thread #9 (`T-3195083121`) which raised the same concern from @ikolomi's self-review.

_Tracking: [`DESIGN_TODO.md` · T-3187833479](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3187833479)_


### #9 · `T-3195083121` · line 87 · @ikolomi  {#T-3195083121}

- **status:** `addressed`
- **decision:** Same fix as Thread #8 (`T-3187833479`). The word "rebuilds" was replaced with "initializes the needed `ZSTD_DDict` handle from the dictionary bytes stored in the preceding AUX entries" in all 4 places. Your self-review and @GilboaAWS's comment identified the same ambiguous wording — resolved together.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T11:33:29Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3195083121

**context (what was commented on):**

> **Answer:** Option 2 — **transparently decompress on load**.
> 
> - When the loader encounters `RDB_ENC_ZSTDDICT` while `compression-enabled no`, it rebuilds the needed `ZSTD_DDict` from the preceding AUX entries, decompresses each value inline, and stores the result uncompressed.

**comment:**

why rebuilds? - the dicts should be stored within the rdb


**↳ reply by @ikolomi (2026-05-11T09:56:32Z):**

**Resolved via 2026-05-10 design walkthrough.**

Same fix as Thread #8 (`T-3187833479`). The word "rebuilds" was replaced with "initializes the needed `ZSTD_DDict` handle from the dictionary bytes stored in the preceding AUX entries" in all 4 places. Your self-review and @GilboaAWS's comment identified the same ambiguous wording — resolved together.

_Tracking: [`DESIGN_TODO.md` · T-3195083121](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3195083121)_


### #10 · `T-3195128013` · line 143 · @ikolomi  {#T-3195128013}

- **status:** `addressed`
- **decision:** Option B + clarification. Dropped the handwave "doubles peak memory" phrase from Q5's option (b) description and replaced with *"causes a large transient memory spike to the full uncompressed dataset size while the scan runs."* Also clarified the **fundamental guarantee difference** the user highlighted: option (a) only decompresses read-touched keys (cold/untouched keys stay compressed indefinitely), while options (b) and (c) **guarantee eventual full-keyspace decompression** regardless of read activity — (b) synchronously with large transient spike, (c) in background under pacing with growth bounded by `compression-sweep-max-cpu-pct`. Also rewrote R2.1.4 in detailed-design.md to drop "never doubled" language and replace with accurate pacing-bounded memory-growth statement (factor `1/compression_ratio`, typically 2–3×). Also resolves Thread #13 (`T-3188268414`) which raised the same concern.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T11:42:03Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3195128013

**context (what was commented on):**

>    - **Disable `yes → no`**: the interesting case. Options:
>      - **(a) Decompression-only mode**: stop compressing new values, but keep the dictionary registry alive and keep decompressing existing compressed values on read. Already-compressed values stay compressed in memory. Cheapest, transparent to clients.
>      - **(b) Immediate eager decompress**: scan the keyspace and decompress everything synchronously. Safe final state (no compressed frames remain) but can take a long time and doubles peak memory.

**comment:**

why doubles? we need to have some numbers behind that statement ... like expected compression rate is X, freeing delay is Y, so it can peak to Z


**↳ reply by @ikolomi (2026-05-11T09:56:35Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option B + clarification. Dropped the handwave "doubles peak memory" phrase from Q5's option (b) description and replaced with *"causes a large transient memory spike to the full uncompressed dataset size while the scan runs."* Also clarified the **fundamental guarantee difference** the user highlighted: option (a) only decompresses read-touched keys (cold/untouched keys stay compressed indefinitely), while options (b) and (c) **guarantee eventual full-keyspace decompression** regardless of read activity — (b) synchronously with large transient spike, (c) in background under pacing with growth bounded by `compression-sweep-max-cpu-pct`. Also rewrote R2.1.4 in detailed-design.md to drop "never doubled" language and replace with accurate pacing-bounded memory-growth statement (factor `1/compression_ratio`, typically 2–3×). Also resolves Thread #13 (`T-3188268414`) which raised the same concern.

_Tracking: [`DESIGN_TODO.md` · T-3195128013](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3195128013)_


### #11 · `T-3188210305` · line 144 · @GilboaAWS  {#T-3188210305}

- **status:** `addressed`
- **decision:** Option B — kept the asymmetric default (auto-sweep on enable, explicit-sweep on disable) but documented the trivial symmetric pattern for operators who want manual control. Updated Q5 Answer in idea-honing.md and R2.1.3 in detailed-design.md with the same language: before flipping `compression-enabled yes`, set `compression-threads 0` (disables the worker pool; candidates queue but no work happens); raise `compression-threads` to 1+ when ready to drain. This gives the symmetric behavior the reviewer asked for via existing configs, no new knob introduced. Cross-references R2.1.4 so the two directions are visibly paired.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T11:58:14Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3188210305

**context (what was commented on):**

>      - **(a) Decompression-only mode**: stop compressing new values, but keep the dictionary registry alive and keep decompressing existing compressed values on read. Already-compressed values stay compressed in memory. Cheapest, transparent to clients.
>      - **(b) Immediate eager decompress**: scan the keyspace and decompress everything synchronously. Safe final state (no compressed frames remain) but can take a long time and doubles peak memory.
>      - **(c) Background sweep decompress**: like (a) but also kicks off a background job to decompress-in-place over time, so eventually the dictionary registry is unreferenced and can be dropped. Middle ground.

**comment:**

I think the transitions to both sides should be symmetric.
If we allow in on to off to keep part of the data compressed, we should allow the opposite in the other, and off to on to keep part of the data decompressed and only start to compress the arriving data from now on.


**↳ reply by @ikolomi (2026-05-11T09:56:38Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option B — kept the asymmetric default (auto-sweep on enable, explicit-sweep on disable) but documented the trivial symmetric pattern for operators who want manual control. Updated Q5 Answer in idea-honing.md and R2.1.3 in detailed-design.md with the same language: before flipping `compression-enabled yes`, set `compression-threads 0` (disables the worker pool; candidates queue but no work happens); raise `compression-threads` to 1+ when ready to drain. This gives the symmetric behavior the reviewer asked for via existing configs, no new knob introduced. Cross-references R2.1.4 so the two directions are visibly paired.

_Tracking: [`DESIGN_TODO.md` · T-3188210305](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3188210305)_


### #12 · `T-3167454097` · line 148 · @ikolomi  {#T-3167454097}

- **status:** `addressed`
- **decision:** Option A — removed the stale "emits a keyspace notification" phrase from Q5's "What I recommend" preamble. The draft was internally inconsistent: the preamble said notifications would be emitted, but Q10c's final decision (later in the same doc) explicitly rejected them. Q5 now matches Q10c. `detailed-design.md` R2.10.3 was already correct ("No keyspace notifications for compression events") — no design-doc edit needed. One-line fix, removes a contradiction a future reader would have hit.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T11:05:59Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167454097

**context (what was commented on):**

> **What I recommend:**
> 
> - **Surface**: both. `CONFIG SET compression-enabled yes|no` is the primary, Valkey-idiomatic interface (persists via `CONFIG REWRITE`, covered by all existing tooling). `COMPRESSION ENABLE`/`DISABLE` is a convenience alias that additionally logs the event and emits a keyspace notification — nicer for runbooks and audit trails.

**comment:**

why keyspace notification - the feature is about storage format not the data set


**↳ reply by @ikolomi (2026-05-11T09:56:41Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — removed the stale "emits a keyspace notification" phrase from Q5's "What I recommend" preamble. The draft was internally inconsistent: the preamble said notifications would be emitted, but Q10c's final decision (later in the same doc) explicitly rejected them. Q5 now matches Q10c. `detailed-design.md` R2.10.3 was already correct ("No keyspace notifications for compression events") — no design-doc edit needed. One-line fix, removes a contradiction a future reader would have hit.

_Tracking: [`DESIGN_TODO.md` · T-3167454097](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167454097)_


### #13 · `T-3188268414` · line 153 · @GilboaAWS  {#T-3188268414}

- **status:** `addressed`
- **decision:** Resolved together with Thread #10 (`T-3195128013`). The "doubled memory peak" phrasing was handwaved — @GilboaAWS correctly pointed out that in-place key-by-key decompression doesn't literally "double" peak, and @ikolomi's reply pointed to the numerical clarification in thread 10. The Q5 option (b) description was rewritten to say *"causes a large transient memory spike to the full uncompressed dataset size while the scan runs"* (qualitative), and R2.1.4 in detailed-design.md was updated with accurate peak memory math (factor `1/ratio`, typically 2–3× for realistic ratios). Also clarifies that v1 default behavior is option (a) (on-read only; cold keys stay compressed), and the explicit sweep path follows option (c) (background, pacing-bounded). None of these is option (b).
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T12:08:48Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3188268414

**context (what was commented on):**

>   - Immediately on `DISABLE`: new writes stop being compressed. Existing compressed values keep working (decompress on read). The dictionary registry stays alive until explicitly swept.
>   - Operator decides when to force-decompress everything: either never (option a is fine for "I just don't want new things compressed") or via `COMPRESSION SWEEP direction=decompress` (eventually drops all compressed frames and dictionaries).
>   - Never auto-doubles peak memory — operator owns the timing.

**comment:**

I can't see any flow reaches doubled memory peak.
Every de/comp will be in-place. 
If not in-place, how will it work?


**↳ reply by @ikolomi (2026-05-06T11:43:38Z):**

Ive put a comment to clarify this with some numbers


**↳ reply by @ikolomi (2026-05-11T09:56:43Z):**

**Resolved via 2026-05-10 design walkthrough.**

Resolved together with Thread #10 (`T-3195128013`). The "doubled memory peak" phrasing was handwaved — @GilboaAWS correctly pointed out that in-place key-by-key decompression doesn't literally "double" peak, and @ikolomi's reply pointed to the numerical clarification in thread 10. The Q5 option (b) description was rewritten to say *"causes a large transient memory spike to the full uncompressed dataset size while the scan runs"* (qualitative), and R2.1.4 in detailed-design.md was updated with accurate peak memory math (factor `1/ratio`, typically 2–3× for realistic ratios). Also clarifies that v1 default behavior is option (a) (on-read only; cold keys stay compressed), and the explicit sweep path follows option (c) (background, pacing-bounded). None of these is option (b).

_Tracking: [`DESIGN_TODO.md` · T-3188268414](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3188268414)_


### #14 · `T-3167472414` · line 155 · @ikolomi  {#T-3167472414}

- **status:** `addressed`
- **decision:** Option A — clarified the Q5 "third-state" text in idea-honing.md. The self-reply was slightly imprecise: `COMPRESSION SWEEP direction=decompress` drains compressed frames and lets dicts retire naturally when refcount hits 0 (per R2.3.4), while **`COMPRESSION DICT DROP <dictID>`** is the explicit per-dict retire command. Both paths are now named in the Q5 text: *"all dicts retired — either force-dropped via `COMPRESSION DICT DROP <dictID>` or drained via `COMPRESSION SWEEP direction=decompress` which lets refcounts reach zero."* No design-doc edit needed — §4.5 already specifies both commands authoritatively.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T11:09:40Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167472414

**context (what was commented on):**

>   - Never auto-doubles peak memory — operator owns the timing.
> 
> - **Consequence**: there's a small third state, "compression-enabled yes but no active dictionary yet" (fresh server, not enough samples to train, or admin has dropped all dicts). In that state, decompression still works for anything that's compressed; new writes stay uncompressed until the first dictionary is published. This is the same code path as "compression-enabled no", just with a different reason.

**comment:**

"admin has dropped all dicts" - is there such command that allows dropping dicts?


**↳ reply by @ikolomi (2026-04-30T11:29:36Z):**

it seems that the idea is "COMPRESSION SWEEP direction=decompress" drops the dicts


**↳ reply by @ikolomi (2026-05-11T09:56:46Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — clarified the Q5 "third-state" text in idea-honing.md. The self-reply was slightly imprecise: `COMPRESSION SWEEP direction=decompress` drains compressed frames and lets dicts retire naturally when refcount hits 0 (per R2.3.4), while **`COMPRESSION DICT DROP <dictID>`** is the explicit per-dict retire command. Both paths are now named in the Q5 text: *"all dicts retired — either force-dropped via `COMPRESSION DICT DROP <dictID>` or drained via `COMPRESSION SWEEP direction=decompress` which lets refcounts reach zero."* No design-doc edit needed — §4.5 already specifies both commands authoritatively.

_Tracking: [`DESIGN_TODO.md` · T-3167472414](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167472414)_


### #15 · `T-3188472136` · line 155 · @GilboaAWS  {#T-3188472136}

- **status:** `addressed`
- **decision:** Option A — pushed back on the "v2 optimization" self-reply. The question reveals a real invariant that was under-documented, not a missing feature. Updated R2.1.5 in detailed-design.md to spell out *why* decompression works in the third state: refcount-based retirement (R2.3.4) + the `COMPRESSION DICT DROP` refcount safety check (§4.5) together guarantee a dict cannot be freed while any frame references it; the state "no dicts AND compressed frames exist" is **by construction unreachable**. Matching clarification added to Q5's "Consequence" paragraph in idea-honing.md. No code change needed — this was a documentation gap.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T12:42:43Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3188472136

**context (what was commented on):**

>   - Never auto-doubles peak memory — operator owns the timing.
> 
> - **Consequence**: there's a small third state, "compression-enabled yes but no active dictionary yet" (fresh server, not enough samples to train, or admin has dropped all dicts). In that state, decompression still works for anything that's compressed; new writes stay uncompressed until the first dictionary is published. This is the same code path as "compression-enabled no", just with a different reason.

**comment:**

`compression-enabled yes but no active dictionary yet` and `decompression still works for anything that's compressed`.
How decompression works w/o the dictionary?


**↳ reply by @ikolomi (2026-05-06T11:58:31Z):**

This might be a good optimization for v2. This will reuquire additional complexity and thus, better to postpone


**↳ reply by @ikolomi (2026-05-11T09:56:48Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — pushed back on the "v2 optimization" self-reply. The question reveals a real invariant that was under-documented, not a missing feature. Updated R2.1.5 in detailed-design.md to spell out *why* decompression works in the third state: refcount-based retirement (R2.3.4) + the `COMPRESSION DICT DROP` refcount safety check (§4.5) together guarantee a dict cannot be freed while any frame references it; the state "no dicts AND compressed frames exist" is **by construction unreachable**. Matching clarification added to Q5's "Consequence" paragraph in idea-honing.md. No code change needed — this was a documentation gap.

_Tracking: [`DESIGN_TODO.md` · T-3188472136](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3188472136)_


### #16 · `T-3188630409` · line 174 · @GilboaAWS  {#T-3188630409}

- **status:** `addressed`
- **decision:** Option D — keep the current design unchanged. The v1 disable-side model provides two operator-controlled mechanisms that together cover the reviewer's three-mode request: (1) **passive decompress-on-read** (default after `compression-enabled no`, maps to reviewer's `Never` — existing compressed data stays compressed until something reads it); (2) **operator-triggered background sweep** via `COMPRESSION SWEEP direction=decompress` (maps to reviewer's `decompress_background` — paced by `compression-sweep-max-cpu-pct`; operator can saturate worker CPUs by temporarily raising pacing to 100, per the Thread-11 pattern). Main-thread-blocking synchronous decompression (`decompress_immediately`) is **not added** — a multi-GB keyspace would block the event loop for minutes, exceeding cluster failover timeouts and monitoring thresholds. The saturate-pacing pattern already gives operators the same "as fast as possible" semantics without the blast radius. If a future use-case genuinely requires atomic-point decompression it would be introduced as a separate command with explicit risk acknowledgment, not as a mode on `SWEEP`. GitHub reply for round-trip: *"Kept the current design — passive on-read decompression + operator-triggered background sweep cover the intended operator-control surface. Main-thread-blocking decompress rejected due to event-loop stall / cluster failover risk; the saturate-pacing pattern (temporarily raise `compression-sweep-max-cpu-pct` and `compression-threads`) gives the same 'fast as possible' semantics safely."*
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T13:07:36Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3188630409

**context (what was commented on):**

>   - Existing compressed values continue to work — decompress on read, dictionary registry stays alive.
>   - No automatic sweep. Operator explicitly runs `COMPRESSION SWEEP direction=decompress` if they want all frames decompressed and the dictionary registry dropped.
>   - Peak memory is never doubled automatically.

**comment:**

I think we should follow the **Valkey's operator culture** and let them decide how it should run.

yes -> no flow options:

1. Immediately: new writes stop being compressed.
2. Existing compressed values continue to work — decompress on read, dictionary registry stays alive.

Add 3 sweep options:
1. `Never` -  keep the data compressed data as it is.
2. `decompress_background` - decomp all the data in the backgorund
3. `decompress_immediately` immidiately start to decompress all the data on the main thread, blocking the data path.


**↳ reply by @ikolomi (2026-05-11T09:56:50Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option D — keep the current design unchanged. The v1 disable-side model provides two operator-controlled mechanisms that together cover the reviewer's three-mode request: (1) **passive decompress-on-read** (default after `compression-enabled no`, maps to reviewer's `Never` — existing compressed data stays compressed until something reads it); (2) **operator-triggered background sweep** via `COMPRESSION SWEEP direction=decompress` (maps to reviewer's `decompress_background` — paced by `compression-sweep-max-cpu-pct`; operator can saturate worker CPUs by temporarily raising pacing to 100, per the Thread-11 pattern). Main-thread-blocking synchronous decompression (`decompress_immediately`) is **not added** — a multi-GB keyspace would block the event loop for minutes, exceeding cluster failover timeouts and monitoring thresholds. The saturate-pacing pattern already gives operators the same "as fast as possible" semantics without the blast radius. If a future use-case genuinely requires atomic-point decompression it would be introduced as a separate command with explicit risk acknowledgment, not as a mode on `SWEEP`. GitHub reply for round-trip: *"Kept the current design — passive on-read decompression + operator-triggered background sweep cover the intended operator-control surface. Main-thread-blocking decompress rejected due to event-loop stall / cluster failover risk; the saturate-pacing pattern (temporarily raise `compression-sweep-max-cpu-pct` and `compression-threads`) gives the same 'fast as possible' semantics safely."*

_Tracking: [`DESIGN_TODO.md` · T-3188630409](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3188630409)_


### #17 · `T-3167636023` · line 186 · @ikolomi  {#T-3167636023}

- **status:** `addressed`
- **decision:** Option A — dropped EMBSTR from the compression-eligibility list in 6 locations (idea-honing.md Q6 baseline filter, Q6c scope statement, consolidated predicate; detailed-design.md R2.2 predicate, R2.7.6 module DMA post-decompress target, and idea-honing.md matching text). Rationale: EMBSTR values are ≤44 B and already memory-optimal; they would never pass `compression-min-value-size` (default 256 B) in production but *would* leak into the §7.1 transparency test harness (which sets min-value-size to 0) and waste CPU on guaranteed-negative-savings cases. Retained 3 EMBSTR mentions that refer to it as an existing Valkey encoding name (OBJECT ENCODING table, test helper encoding list, test-author guidance) — those are correct and unrelated to eligibility. Also resolves Thread #21 (`T-3167872496`) which raised the same point.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T11:42:04Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167636023

**context (what was commented on):**

> 
> - `type == OBJ_STRING`.
> - `encoding ∈ {OBJ_ENCODING_RAW, OBJ_ENCODING_EMBSTR}`. Skip `OBJ_ENCODING_INT` (already memory-optimal — a packed long).

**comment:**

OBJ_ENCODING_EMBSTR is should probably be skipped - it has bad compression profile and is already kind-of optimized storage


**↳ reply by @ikolomi (2026-05-11T09:56:53Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — dropped EMBSTR from the compression-eligibility list in 6 locations (idea-honing.md Q6 baseline filter, Q6c scope statement, consolidated predicate; detailed-design.md R2.2 predicate, R2.7.6 module DMA post-decompress target, and idea-honing.md matching text). Rationale: EMBSTR values are ≤44 B and already memory-optimal; they would never pass `compression-min-value-size` (default 256 B) in production but *would* leak into the §7.1 transparency test harness (which sets min-value-size to 0) and waste CPU on guaranteed-negative-savings cases. Retained 3 EMBSTR mentions that refer to it as an existing Valkey encoding name (OBJECT ENCODING table, test helper encoding list, test-author guidance) — those are correct and unrelated to eligibility. Also resolves Thread #21 (`T-3167872496`) which raised the same point.

_Tracking: [`DESIGN_TODO.md` · T-3167636023](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167636023)_


### #18 · `T-3167775681` · line 192 · @ikolomi  {#T-3167775681}

- **status:** `addressed`
- **decision:** Option A — decoupled compression hotness from eviction policy. The eligibility filter now applies **both** a settle-window (write-age) and an idle-time (read-age) check **in every mode, including `noeviction`**, using Valkey's existing 24-bit `robj->lru` field via `estimateObjectIdleTime()`. The LFU-counter check remains as an additional guard that activates only when LFU is the eviction policy. Renamed `compression-lru-idle-seconds` → `compression-min-idle-seconds` (now mode-agnostic). Updated: Q6 baseline filter, Q6 consolidated predicate, Q6 config table, Q6a answer text (marked "superseded by review"), and all corresponding places in detailed-design.md (R2.2 predicate, §2.12 config table, §7.1 transparency harness). The Thread-19-proposed `recent_access_count` compression-local counter was **not added** — the existing LRU field provides the signal for free without extra robj bits. Also resolves Thread #19 (`T-3167859080`).
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:08:43Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167775681

**context (what was commented on):**

>   - LFU mode: skip if `LFU_freq >= compression-lfu-threshold` (default `5`, on the 0–255 log scale).
>   - LRU mode: skip if `idle_seconds < compression-lru-idle-seconds` (default `60 s`).
>   - `noeviction` / other policies with no useful counter: fall back to a **time-based "settle" window** — a key is eligible N seconds after its last write. Cheapest implementation: stamp the write time in a small per-candidate queue entry; we already need that queue.

**comment:**

many production Redis/Valkey deployments use noeviction; in those cases, a heavily-read but rarely-written key may still be compressed


**↳ reply by @ikolomi (2026-05-11T09:56:56Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — decoupled compression hotness from eviction policy. The eligibility filter now applies **both** a settle-window (write-age) and an idle-time (read-age) check **in every mode, including `noeviction`**, using Valkey's existing 24-bit `robj->lru` field via `estimateObjectIdleTime()`. The LFU-counter check remains as an additional guard that activates only when LFU is the eviction policy. Renamed `compression-lru-idle-seconds` → `compression-min-idle-seconds` (now mode-agnostic). Updated: Q6 baseline filter, Q6 consolidated predicate, Q6 config table, Q6a answer text (marked "superseded by review"), and all corresponding places in detailed-design.md (R2.2 predicate, §2.12 config table, §7.1 transparency harness). The Thread-19-proposed `recent_access_count` compression-local counter was **not added** — the existing LRU field provides the signal for free without extra robj bits. Also resolves Thread #19 (`T-3167859080`).

_Tracking: [`DESIGN_TODO.md` · T-3167775681](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167775681)_


### #19 · `T-3167859080` · line 192 · @ikolomi  {#T-3167859080}

- **status:** `addressed`
- **decision:** Option A — accept the Thread-18 resolution. Two of the three proposed checks adopted universally: `compression-settle-seconds` (recent-write skip) and `compression-min-idle-seconds` (recent-access skip via `estimateObjectIdleTime()`). The third proposal — a new compression-local `recent_access_count` field — **rejected for v1**: it would require new per-robj state (the 24-bit LRU field is packed tight; either steal bits or grow the struct), while the existing LRU field already provides the signal for every eviction policy. The accuracy gain would apply only under LFU, where LFU's own counter already supplies the frequency signal. Cost/benefit favors keeping v1 to the two universal checks.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:22:01Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167859080

**context (what was commented on):**

>   - LFU mode: skip if `LFU_freq >= compression-lfu-threshold` (default `5`, on the 0–255 log scale).
>   - LRU mode: skip if `idle_seconds < compression-lru-idle-seconds` (default `60 s`).
>   - `noeviction` / other policies with no useful counter: fall back to a **time-based "settle" window** — a key is eligible N seconds after its last write. Cheapest implementation: stamp the write time in a small per-candidate queue entry; we already need that queue.

**comment:**

I’d counter it by decoupling compression hotness tracking from eviction policy.

Relying on maxmemory-policy is convenient, but it is the wrong abstraction. Compression needs to know: “will this value probably be read soon?” Eviction policy may be noeviction, but the server still has access metadata in the object header, and we can also maintain compression-specific signals.

consider practical v1 fix
Use a compression-local hotness policy that works even under noeviction:
```
if (now - meta.last_write_time < compression_settle_seconds)
    skip;          // recently written

if (estimateObjectIdleTime(obj) < compression_min_idle_seconds)
    skip;          // recently read/written

if (meta.recent_access_count >= compression_access_threshold)
    skip;          // frequently accessed

compress;
```


**↳ reply by @ikolomi (2026-05-11T09:56:59Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — accept the Thread-18 resolution. Two of the three proposed checks adopted universally: `compression-settle-seconds` (recent-write skip) and `compression-min-idle-seconds` (recent-access skip via `estimateObjectIdleTime()`). The third proposal — a new compression-local `recent_access_count` field — **rejected for v1**: it would require new per-robj state (the 24-bit LRU field is packed tight; either steal bits or grow the struct), while the existing LRU field already provides the signal for every eviction policy. The accuracy gain would apply only under LFU, where LFU's own counter already supplies the frequency signal. Cost/benefit favors keeping v1 to the two universal checks.

_Tracking: [`DESIGN_TODO.md` · T-3167859080](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167859080)_


### #20 · `T-3188785185` · line 193 · @GilboaAWS  {#T-3188785185}

- **status:** `addressed`
- **decision:** Option C — dict-ID primary signal + time-based fallback. The retry mechanism is now stored in a side hashtable `incompressibleKeys{key_hash → (failed_dict_id, timestamp)}` (no robj modification needed). A key is retry-eligible iff (a) not in the table, OR (b) `entry.failed_dict_id != active_dict_id` (primary: new dict promoted → incompressibility may have changed), OR (c) `age(entry.timestamp) >= compression-retry-interval` (fallback: catches content changes that alter compressibility while the dict stays stable, default 1 h). Entries are evicted on successful compression or on `DEL`. Updated: Q6 baseline filter and Q6b Answer in idea-honing.md; consolidated predicate introduces a `retry_eligible(obj)` helper with the full semantics; R2.2 predicate in detailed-design.md; §2.12 config-table description of `compression-retry-interval`; §6 error-handling bullet describing the post-compression guard path. No new config knob introduced; `compression-retry-interval` remains but its role is documented as fallback, not primary. Semantically correct (incompressibility is dict-scoped) and handles the content-change edge case via the time fallback.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-05T13:30:26Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3188785185

**context (what was commented on):**

>   - LRU mode: skip if `idle_seconds < compression-lru-idle-seconds` (default `60 s`).
>   - `noeviction` / other policies with no useful counter: fall back to a **time-based "settle" window** — a key is eligible N seconds after its last write. Cheapest implementation: stamp the write time in a small per-candidate queue entry; we already need that queue.
> - **Skip post-compression if we don't actually save.** After the worker compresses, on the main thread we compare `compressed_size + header >= uncompressed_size * (1 - compression-min-savings-ratio)` (default `10%`). If true: discard the compressed form, leave the value uncompressed, and **mark the key as "don't re-try"** for a cooldown period (e.g., `compression-retry-interval` = 1 h) so we don't waste CPU on incompressible keys.

**comment:**

I don't get why we refer to `cooldown period`, time isn't a factor in such case, but only the dictionary.
Maybe we need to keep the don't re-try flag with the dict-ID so upon a new dict it should retry.


**↳ reply by @ikolomi (2026-05-11T09:57:02Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option C — dict-ID primary signal + time-based fallback. The retry mechanism is now stored in a side hashtable `incompressibleKeys{key_hash → (failed_dict_id, timestamp)}` (no robj modification needed). A key is retry-eligible iff (a) not in the table, OR (b) `entry.failed_dict_id != active_dict_id` (primary: new dict promoted → incompressibility may have changed), OR (c) `age(entry.timestamp) >= compression-retry-interval` (fallback: catches content changes that alter compressibility while the dict stays stable, default 1 h). Entries are evicted on successful compression or on `DEL`. Updated: Q6 baseline filter and Q6b Answer in idea-honing.md; consolidated predicate introduces a `retry_eligible(obj)` helper with the full semantics; R2.2 predicate in detailed-design.md; §2.12 config-table description of `compression-retry-interval`; §6 error-handling bullet describing the post-compression guard path. No new config knob introduced; `compression-retry-interval` remains but its role is documented as fallback, not primary. Semantically correct (incompressibility is dict-scoped) and handles the content-change edge case via the time fallback.

_Tracking: [`DESIGN_TODO.md` · T-3188785185](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3188785185)_


### #21 · `T-3167872496` · line 211 · @ikolomi  {#T-3167872496}

- **status:** `addressed`
- **decision:** Same fix as Thread #17 (`T-3167636023`). EMBSTR removed from all eligibility references — the encoding predicate, Q6c scope text, consolidated predicate (idea-honing.md and detailed-design.md R2.2), and the module DMA write-intent post-decompress target (R2.7.6). Both self-review comments flagged the same redundancy; resolved together.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:24:27Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3167872496

**context (what was commented on):**

> - **(Q6a)** In `noeviction` / non-LRU / non-LFU policies, use a **time-based settle window**. A key is eligible for compression `compression-settle-seconds` (default `60 s`) after its last write. Implemented by stamping write time on the candidate-queue entry.
> - **(Q6b)** Include the **"don't re-try incompressible keys"** cooldown in v1. When the post-compression savings check fails, stamp the key with `compression-retry-interval` (default `1 h`) so sweepers skip it. Counter tracked in `INFO compression` (`compression_skipped_incompressible`).
> - **(Q6c)** **v1 scope is STRING only** (`type == OBJ_STRING`, encodings `RAW`/`EMBSTR`). HASH/SET/ZSET/LIST/STREAM are explicit v2+ work. The encoding-tag approach remains structurally extensible so future types can piggyback on the same infrastructure.

**comment:**

EMBSTR should probably not be compressed


**↳ reply by @ikolomi (2026-05-11T09:57:04Z):**

**Resolved via 2026-05-10 design walkthrough.**

Same fix as Thread #17 (`T-3167636023`). EMBSTR removed from all eligibility references — the encoding predicate, Q6c scope text, consolidated predicate (idea-honing.md and detailed-design.md R2.2), and the module DMA write-intent post-decompress target (R2.7.6). Both self-review comments flagged the same redundancy; resolved together.

_Tracking: [`DESIGN_TODO.md` · T-3167872496](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3167872496)_


### #22 · `T-3168003341` · line 216 · @ikolomi  {#T-3168003341}

- **status:** `addressed`
- **decision:** Option A — housekeeping marker satisfied by revisions already applied in Threads 17/21 (EMBSTR dropped from eligible encodings), Threads 18/19 (universal `write_age` + `idle_seconds` checks replacing the policy-branching filter), and Thread 20 (dict-ID scoped retry guard replacing time-based cooldown). The Q6 consolidated predicate now reflects all of these changes. Nothing further required; closing as addressed with cross-references to the threads that accomplished the revision.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:46:57Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168003341

**context (what was commented on):**

> 
> ```
> eligible(obj) ⇔

**comment:**

revise with aforementioned comments


**↳ reply by @ikolomi (2026-05-11T09:57:07Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — housekeeping marker satisfied by revisions already applied in Threads 17/21 (EMBSTR dropped from eligible encodings), Threads 18/19 (universal `write_age` + `idle_seconds` checks replacing the policy-branching filter), and Thread 20 (dict-ID scoped retry guard replacing time-based cooldown). The Q6 consolidated predicate now reflects all of these changes. Nothing further required; closing as addressed with cross-references to the threads that accomplished the revision.

_Tracking: [`DESIGN_TODO.md` · T-3168003341](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168003341)_


### #23 · `T-3168045037` · line 220 · @ikolomi  {#T-3168045037}

- **status:** `addressed`
- **decision:** Option A — self-resolved housekeeping confirmed by inspection. `compression-max-value-size` is present in the eligibility predicate (idea-honing.md Q6 consolidated predicate + detailed-design.md R2.2), in the primary config table (§2.12) with the Thread-1 default of 131072 (128 KiB). Self-reply "seems to be addressed in Q7" is correct.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:54:33Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168045037

**context (what was commented on):**

>  && obj->encoding ∈ {RAW, EMBSTR}
>  && obj->refcount != SHARED
>  && sdslen(val) >= compression-min-value-size

**comment:**

compression-max-value-size should also be used/designed


**↳ reply by @ikolomi (2026-04-30T12:59:01Z):**

seems to be addressed in Q7


**↳ reply by @ikolomi (2026-05-11T09:57:10Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — self-resolved housekeeping confirmed by inspection. `compression-max-value-size` is present in the eligibility predicate (idea-honing.md Q6 consolidated predicate + detailed-design.md R2.2), in the primary config table (§2.12) with the Thread-1 default of 131072 (128 KiB). Self-reply "seems to be addressed in Q7" is correct.

_Tracking: [`DESIGN_TODO.md` · T-3168045037](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168045037)_


### #24 · `T-3168048488` · line 240 · @ikolomi  {#T-3168048488}

- **status:** `addressed`
- **decision:** Option A — mirror of Thread #23. `compression-min-value-size` is present in the Q6 consolidated predicate, R2.2 predicate, and §2.12 Primary config table (default 256 bytes). Self-reply "actually added in Q7" is correct.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:55:10Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168048488

**context (what was commented on):**

> | Name | Default |
> |---|---|
> | `compression-min-value-size` | `256` bytes |

**comment:**

what about compression-min-value-size ?


**↳ reply by @ikolomi (2026-04-30T12:58:41Z):**

actually added in Q7


**↳ reply by @ikolomi (2026-05-11T09:57:13Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — mirror of Thread #23. `compression-min-value-size` is present in the Q6 consolidated predicate, R2.2 predicate, and §2.12 Primary config table (default 256 bytes). Self-reply "actually added in Q7" is correct.

_Tracking: [`DESIGN_TODO.md` · T-3168048488](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168048488)_


### #25 · `T-3168004892` · line 253 · @ikolomi  {#T-3168004892}

- **status:** `addressed`
- **decision:** Option A — added §C.7 to detailed-design.md Appendix C (Alternative approaches considered) with a three-point rationale: (1) boundary crossing (IO threads shouldn't do keyspace execution), (2) resource coupling (decompression CPU would scale with `io-threads` knob, not compression workload), (3) simpler alternative exists (dedicated compression worker pool). Documented v2 deferral: speculative pre-decompression for simple read-only commands with object-version validation. Added a one-line cross-reference in Q7 Answer (idea-honing.md) pointing to §C.7. The design now makes it explicit why io-threads aren't used for compression/decompression; previously this was only implied by §2.11 R2.11.4 ("Worker pool is separate from `io-threads`") and research notes.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:47:15Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168004892

**context (what was commented on):**

> When a client reads a compressed key, should decompression run **synchronously on the main thread** or be **offloaded to a compression worker thread**?
> 
> **Context.** There are two legitimate patterns, and the POC used both:

**comment:**

Rejected option: use IO threads for decompression / read preparation.
IO threads already parse protocol input and could, in theory, speculatively prepare decompressed values for read commands. However, this crosses the boundary between network IO and keyspace execution. Correct decompression requires key lookup, object lifetime handling, command ordering, script/transaction semantics, and revalidation against earlier commands in the same event-loop batch. It also couples decompression CPU to network IO capacity. For v1, decompression remains synchronous on the main thread; future versions may explore speculative IO-thread pre-decompression for simple read-only commands with object-version validation.


**↳ reply by @ikolomi (2026-05-11T09:57:16Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — added §C.7 to detailed-design.md Appendix C (Alternative approaches considered) with a three-point rationale: (1) boundary crossing (IO threads shouldn't do keyspace execution), (2) resource coupling (decompression CPU would scale with `io-threads` knob, not compression workload), (3) simpler alternative exists (dedicated compression worker pool). Documented v2 deferral: speculative pre-decompression for simple read-only commands with object-version validation. Added a one-line cross-reference in Q7 Answer (idea-honing.md) pointing to §C.7. The design now makes it explicit why io-threads aren't used for compression/decompression; previously this was only implied by §2.11 R2.11.4 ("Worker pool is separate from `io-threads`") and research notes.

_Tracking: [`DESIGN_TODO.md` · T-3168004892](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168004892)_


### #26 · `T-3193851793` · line 256 · @GilboaAWS  {#T-3193851793}

- **status:** `addressed`
- **decision:** Option A — added in-line clarification to Q7's async-path bullet in idea-honing.md. The POC's phrase "block the client" was using "block" in the blocking-command sense (like `BLPOP`) — the specific client waits while the main thread is free and other clients keep executing. The reviewer reasonably read "block" as "stall the event loop" which would describe the sync path, not the async. The new wording makes the distinction explicit and contrasts with the sync path's ~1 µs/KB main-thread stall. Design doc Appendix C.2 also uses the phrase but in context that makes the meaning clear (no change needed there).
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-05-06T07:54:33Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3193851793

**context (what was commented on):**

> 
> - **Sync main-thread decompress**: simple. One extra branch + `ZSTD_decompress_usingDDict` on the main thread, on the command hot path. Cost scales with value size (zstd decompression is ~1 GB/s per core for level 3 with a dict, so ~1 µs/KB).
> - **Async worker decompress**: main thread enqueues a job on the compression-worker SPMC inbox, the command / client yields back to the event loop, worker decompresses, result comes back via the MPSC outbox, main thread resumes the command. The POC called this "block the client."

**comment:**

The **A**sync path called "block the client."? I'd thought the opposite on the sync path where it actually blocks the data-path.


**↳ reply by @ikolomi (2026-05-11T09:57:19Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — added in-line clarification to Q7's async-path bullet in idea-honing.md. The POC's phrase "block the client" was using "block" in the blocking-command sense (like `BLPOP`) — the specific client waits while the main thread is free and other clients keep executing. The reviewer reasonably read "block" as "stall the event loop" which would describe the sync path, not the async. The new wording makes the distinction explicit and contrasts with the sync path's ~1 µs/KB main-thread stall. Design doc Appendix C.2 also uses the phrase but in context that makes the meaning clear (no change needed there).

_Tracking: [`DESIGN_TODO.md` · T-3193851793](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3193851793)_


### #27 · `T-3168037247` · line 282 · @ikolomi  {#T-3168037247}

- **status:** `addressed`
- **decision:** Option A — self-caught wording error. Replaced *"if we tune `compression-min-value-size` the other way"* with *"if we set `compression-max-value-size` to exclude them"*. `compression-min-value-size` is the lower bound (excludes small values), so "tuning it the other way" is nonsense. `compression-max-value-size` is the correct knob for excluding large values. One-word fix in Q7's "Possible objection" paragraph.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T12:53:09Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168037247

**context (what was commented on):**

> - Option 2 can be added in v2 non-breakingly (just introduce the threshold config, default `0` = disabled).
> 
> **Possible objection:** if workloads routinely store 100 KB+ values and read them often, main-thread decompression at ~100 µs per read could matter. Fair point. But: (a) such values compress well without a dictionary anyway (the dictionary is most valuable for small values — so large values may not even be in scope if we tune `compression-min-value-size` the other way), and (b) anyone with that workload can be directed to v2's opt-in async path when it lands.

**comment:**

> the dictionary is most valuable for small values — so large values may not even be in scope if we tune `compression-min-value-size` the other way

This should be probably governed by compression-max-value-size


**↳ reply by @ikolomi (2026-05-11T09:57:21Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — self-caught wording error. Replaced *"if we tune `compression-min-value-size` the other way"* with *"if we set `compression-max-value-size` to exclude them"*. `compression-min-value-size` is the lower bound (excludes small values), so "tuning it the other way" is nonsense. `compression-max-value-size` is the correct knob for excluding large values. One-word fix in Q7's "Possible objection" paragraph.

_Tracking: [`DESIGN_TODO.md` · T-3168037247](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168037247)_


### #28 · `T-3168085345` · line 310 · @ikolomi  {#T-3168085345}

- **status:** `addressed`
- **decision:** Option B — dropped the weak "(a) compress well without a dictionary" argument from Q7's rationale for `compression-max-value-size` and kept only the strong stall argument: *"the largest values incur the largest main-thread decompression stall (per-read cost is proportional to uncompressed size at ~1 µs/KB). Capping them via `compression-max-value-size` is the cheapest way to bound worst-case sync decompression cost."* The dropped point was tangential (it referred to what *could* be done with plain ZSTD outside the feature) and confusing in a doc where v1 always compresses with dictionaries. The self-caught clarity issue is resolved.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T13:01:07Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168085345

**context (what was commented on):**

>                         || sdslen(val) <= compression-max-value-size)
>   ```
>   Rationale: the largest values both (a) compress well *without* a dictionary so the feature's unique benefit is smaller, and (b) incur the largest main-thread decompression stall. Capping them is the cheapest way to bound worst-case sync decompression cost.

**comment:**

> (a) compress well *without* a dictionary so the feature's unique benefit is smaller

why is it relevant - arent we always compress with dictionaries ?


**↳ reply by @ikolomi (2026-05-11T09:57:24Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option B — dropped the weak "(a) compress well without a dictionary" argument from Q7's rationale for `compression-max-value-size` and kept only the strong stall argument: *"the largest values incur the largest main-thread decompression stall (per-read cost is proportional to uncompressed size at ~1 µs/KB). Capping them via `compression-max-value-size` is the cheapest way to bound worst-case sync decompression cost."* The dropped point was tangential (it referred to what *could* be done with plain ZSTD outside the feature) and confusing in a doc where v1 always compresses with dictionaries. The self-caught clarity issue is resolved.

_Tracking: [`DESIGN_TODO.md` · T-3168085345](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168085345)_


### #29 · `T-3168235257` · line 355 · @ikolomi  {#T-3168235257}

- **status:** `addressed`
- **decision:** Option A — rewrote R2.3.6 + Q9 "Training sampling" to move kvstore iteration and refcount manipulation to the **main thread** (spliced across `serverCron` ticks using the active-expiry / defrag iteration pattern); main thread also **copies sample bytes** into a pre-allocated contiguous buffer + parallel `sizes[]` array. Bio receives the immutable buffer and runs `ZDICT_trainFromBuffer` on it. Main thread promotes the resulting dict via the registry. Bio never touches `robj`, `kvstore`, or refcounts — consistent with the R2.11.4 invariant. Dropped the "zero copies, zero sustained overhead" claim (which was incorrect per Thread 30) and replaced with "no sustained reservoir; transient ~10–16 MiB training buffer during infrequent training runs". Added explicit note that spread-in-time iteration (~1 s window at default hz=10) is safe because each collected sample is an immutable snapshot, iteration window ≪ dict's active lifetime, and drift-retraining handles post-training workload shifts. Also resolves Thread #30 (`T-3168215842`) which flagged that `ZDICT_trainFromBuffer` requires a contiguous buffer.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T13:24:56Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168235257

**context (what was commented on):**

> ---
> 
> ## Q9. Dictionary training — trigger, sampling, and algorithm

**comment:**

detailed-design.md says the training job runs on bio, performs keyspace scan + ZDICT_trainFromBuffer, while also saying the main thread owns robj mutation and workers never touch robj. A bio thread walking kvstore and calling incrRefCount on live objects is still touching main-thread-owned state. Unless Valkey’s kvstore iteration and object refcounts are made thread-safe for this path, this is underspecified and likely unsafe.

Consider this as an alternative:

Main thread:
  incrementally scans kvstore
  selects eligible samples
  copies sample bytes into a temporary training buffer
  or pins objects safely
  submits immutable sample buffer to BIO

BIO thread:
  runs ZDICT_trainFromBuffer on immutable bytes only

Main thread:
  installs/promotes resulting dictionary


**↳ reply by @ikolomi (2026-05-11T09:57:27Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — rewrote R2.3.6 + Q9 "Training sampling" to move kvstore iteration and refcount manipulation to the **main thread** (spliced across `serverCron` ticks using the active-expiry / defrag iteration pattern); main thread also **copies sample bytes** into a pre-allocated contiguous buffer + parallel `sizes[]` array. Bio receives the immutable buffer and runs `ZDICT_trainFromBuffer` on it. Main thread promotes the resulting dict via the registry. Bio never touches `robj`, `kvstore`, or refcounts — consistent with the R2.11.4 invariant. Dropped the "zero copies, zero sustained overhead" claim (which was incorrect per Thread 30) and replaced with "no sustained reservoir; transient ~10–16 MiB training buffer during infrequent training runs". Added explicit note that spread-in-time iteration (~1 s window at default hz=10) is safe because each collected sample is an immutable snapshot, iteration window ≪ dict's active lifetime, and drift-retraining handles post-training workload shifts. Also resolves Thread #30 (`T-3168215842`) which flagged that `ZDICT_trainFromBuffer` requires a contiguous buffer.

_Tracking: [`DESIGN_TODO.md` · T-3168235257](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168235257)_


### #30 · `T-3168215842` · line 365 · @ikolomi  {#T-3168215842}

- **status:** `addressed`
- **decision:** Option A — closed by the Thread-29 fix. The corrected training flow (main-thread iteration + copy into contiguous buffer, bio calls `ZDICT_trainFromBuffer(dict_out, dict_capacity, samplesBuffer, sizes, nbSamples)`) satisfies the ZSTD API requirement that samples be concatenated in a single flat `samplesBuffer` with parallel sizes. The incorrect "zero copies" claim is dropped from both idea-honing.md Q9 Sampling section and detailed-design.md Appendix C.3, replaced with accurate language: "no sustained reservoir; transient ~10–16 MiB training buffer allocated only during the infrequent training window". No custom scatter-gather training path needed.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T13:21:45Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168215842

**context (what was commented on):**

> **Sampling: keyspace scan, not reservoir.**
> - Rationale: a reservoir of ~10000 samples at ~1 KB each costs 10–16 MiB of *extra* memory holding copies of values that already exist in the keyspace. For a memory-saving feature, that overhead is unacceptable (1–2% of a 1 GB instance before any compression win).
> - The training job (on `bio`) walks kvstore shards in random shard order, reuses the active-expiry/defrag iteration pattern, and collects sample pointers via `incrRefCount`. Samples are passed directly to `ZDICT_trainFromBuffer` as pointers into sds bodies — zero copies, zero sustained overhead.

**comment:**

There is an important problem in the design wording, though: ZDICT_trainFromBuffer does not take an array of arbitrary scattered pointers. The zstd API expects samples to be stored concatenated in a single flat samplesBuffer, plus an array of sample sizes...

That means the design must either:

Copy sampled SDS bodies into a temporary contiguous training buffer, then call ZDICT_trainFromBuffer; or
Use a different/custom training path that can consume scatter-gather samples, if one exists; or
Rewrite the claim from “zero copies” to “no sustained reservoir copies, but temporary training buffer allocation exists.”


**↳ reply by @ikolomi (2026-05-11T09:57:30Z):**

**Resolved via 2026-05-10 design walkthrough.**

Option A — closed by the Thread-29 fix. The corrected training flow (main-thread iteration + copy into contiguous buffer, bio calls `ZDICT_trainFromBuffer(dict_out, dict_capacity, samplesBuffer, sizes, nbSamples)`) satisfies the ZSTD API requirement that samples be concatenated in a single flat `samplesBuffer` with parallel sizes. The incorrect "zero copies" claim is dropped from both idea-honing.md Q9 Sampling section and detailed-design.md Appendix C.3, replaced with accurate language: "no sustained reservoir; transient ~10–16 MiB training buffer allocated only during the infrequent training window". No custom scatter-gather training path needed.

_Tracking: [`DESIGN_TODO.md` · T-3168215842](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168215842)_


### #31 · `T-3168318044` · line 486 · @ikolomi  {#T-3168318044}

- **status:** `addressed`
- **decision:** Same answer as Thread #2 (`T-3194623867`). Ambiguity resolved by adding **R2.6.8** to detailed-design.md §2.6: full-sync replication RDB is always emitted **uncompressed** in v1. The claim "v1 full sync uses normal RDB writer, therefore compressed RDB is sent to same-version replicas" was **incorrect** — now explicitly wrong, replaced by the R2.6.8 rule. Disk RDB is compressed; wire RDB (over full sync) is not.
- **resolved_by:** 2026-05-10 walkthrough
- **created:** 2026-04-30T13:38:13Z
- **permalink:** https://github.com/ikolomi/valkey/pull/1#discussion_r3168318044

**context (what was commented on):**

> ---
> 
> ## Q12. Replication and AOF wire format

**comment:**

"v1 full sync uses normal RDB writer, therefore compressed RDB is sent to same-version replicas. " - is it true?


**↳ reply by @ikolomi (2026-05-11T09:57:33Z):**

**Resolved via 2026-05-10 design walkthrough.**

Same answer as Thread #2 (`T-3194623867`). Ambiguity resolved by adding **R2.6.8** to detailed-design.md §2.6: full-sync replication RDB is always emitted **uncompressed** in v1. The claim "v1 full sync uses normal RDB writer, therefore compressed RDB is sent to same-version replicas" was **incorrect** — now explicitly wrong, replaced by the R2.6.8 rule. Disk RDB is compressed; wire RDB (over full sync) is not.

_Tracking: [`DESIGN_TODO.md` · T-3168318044](https://github.com/ikolomi/valkey/blob/inline-comp-design/.agents/planning/realtime-data-compression/DESIGN_TODO.md#t-3168318044)_


