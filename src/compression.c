/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * compression.c — Phase 0 stub.
 *
 * All public entry points currently return feature-disabled defaults.
 * Real implementations land in Phase 1 (see plan.md §5).
 *
 * Order of operations for future work:
 *   - compressionInit wires worker pool + registry + training hooks
 *   - objectGetUncompressedView is the hot-path decompress seam
 *   - compressionEnqueueCandidate wires into dbAdd/dbSetValue
 *   - compressionCron runs the sweep tick and drift-retrain trigger
 *   - compressionAfterSleep drains the worker outbox
 *
 * DO NOT call any ZSTD API from this file directly until BUILD_ZSTD
 * linkage lands (see plan §6 milestone M0 exit criteria).
 */

#include "server.h"
#include "compression.h"
#include "compression_registry.h"
#include "compression_workers.h"
#include "compression_train.h"

/* ========================================================================
 * Lifecycle stubs
 * ======================================================================== */

void compressionInit(void) {
    /* Phase 0: no-op. */
    /* TODO(Phase 1):
     *   compressionRegistryInit();
     *   compressionWorkersStart(server.compression_threads);
     *   compressionTrainInit();
     */
}

void compressionCron(void) {
    /* Phase 0: no-op. */
    /* TODO(Phase 1): sweep tick + drift-retrain + pacing. */
}

void compressionAfterSleep(void) {
    /* Phase 0: no-op. */
    /* TODO(Phase 1):
     *   compressionWorkersDrainOutbox(budget);
     */
}

/* ========================================================================
 * Toggle stub
 * ======================================================================== */

int compressionToggle(int enabled, sds *err) {
    UNUSED(enabled);
    /* Phase 0: toggling has no observable effect (feature is hard-off).
     * We accept the toggle silently so the config layer does not error. */
    if (err) *err = NULL;
    return 1;
}

/* ========================================================================
 * Hot path stubs
 * ========================================================================
 *
 * In Phase 0 there are no compressed robjs — so objectGetUncompressedView
 * always returns the caller's robj unchanged, and compressionIsEligible
 * always returns 0. Both are designed to be inlined by the compiler into
 * a single predictable branch at every call site.
 */

robj *objectGetUncompressedView(robj *o, sds *scratch) {
    UNUSED(scratch);
    /* Phase 0: feature disabled, always fall through. */
    return o;
}

int compressionIsEligible(const robj *o, const sds key) {
    UNUSED(o);
    UNUSED(key);
    /* Phase 0: no candidate is ever eligible. */
    return 0;
}

void compressionEnqueueCandidate(const sds key, robj *o) {
    UNUSED(key);
    UNUSED(o);
    /* Phase 0: candidate queue is a no-op sink. */
}

/* ========================================================================
 * COMPRESSION command surface
 * ======================================================================== */

static const char *kDisabledReply =
    "compression is not enabled in this build (BUILD_ZSTD=no or feature disabled)";

int compressionStatus(client *c) {
    /* Phase 0: return a static INFO-style bulk string.
     * The field set matches §2.10 R2.10.1 so callers wiring dashboards
     * against Phase 0 servers can do so without waiting for the
     * feature-on observability implementation. */
    sds s = sdsempty();
    s = sdscatprintf(s,
        "compression_enabled:0\r\n"
        "compression_state:disabled\r\n"
        "compression_active_dict_id:0\r\n"
        "compression_known_dicts:0\r\n"
        "compression_dict_cap_reached:0\r\n"
        "compression_compressed_objects:0\r\n"
        "compression_total_uncompressed_bytes:0\r\n"
        "compression_total_compressed_bytes:0\r\n"
        "compression_ratio:0\r\n"
        "compression_live_ratio_10m:0\r\n"
        "compression_net_saved_bytes:0\r\n"
        "compression_candidates_pending:0\r\n"
        "compression_compressions_per_sec:0\r\n"
        "compression_decompressions_per_sec:0\r\n"
        "compression_skipped_incompressible:0\r\n"
        "compression_training_last_duration_ms:0\r\n"
        "compression_training_last_sample_count:0\r\n"
        "compression_errors_total:0\r\n");
    addReplyVerbatim(c, s, sdslen(s), "txt");
    sdsfree(s);
    return C_OK;
}

int compressionForceTrain(client *c) {
    addReplyError(c, kDisabledReply);
    return C_ERR;
}

int compressionSweep(client *c, int direction) {
    UNUSED(direction);
    addReplyError(c, kDisabledReply);
    return C_ERR;
}

int compressionDictList(client *c) {
    /* Empty dict list is a legitimate disabled-state reply. */
    addReplyArrayLen(c, 0);
    return C_OK;
}

int compressionDictExport(client *c, uint32_t dict_id) {
    UNUSED(dict_id);
    addReplyError(c, kDisabledReply);
    return C_ERR;
}

int compressionDictImport(client *c, const unsigned char *bytes, size_t len) {
    UNUSED(bytes);
    UNUSED(len);
    addReplyError(c, kDisabledReply);
    return C_ERR;
}

int compressionDictDrop(client *c, uint32_t dict_id) {
    UNUSED(dict_id);
    addReplyError(c, kDisabledReply);
    return C_ERR;
}

/* Dispatch for the top-level COMPRESSION command. Subcommand JSON lives
 * under src/commands/compression-*.json. We handle the common shape
 * (c->argv[1] = subcommand name) here. */
void compressionCommand(client *c) {
    const char *sub = (c->argc >= 2) ? (const char *)objectGetVal(c->argv[1]) : "";

    if (!strcasecmp(sub, "status")) {
        compressionStatus(c);
    } else if (!strcasecmp(sub, "enable") || !strcasecmp(sub, "disable")) {
        /* Phase 0: these are accepted but inert. */
        addReply(c, shared.ok);
    } else if (!strcasecmp(sub, "help")) {
        const char *help[] = {
            "STATUS",
            "    Return the current compression state.",
            "HELP",
            "    Print this help.",
            "",
            "Note: compression is in Phase 0 (skeleton). Additional",
            "subcommands (DICT LIST/DROP/EXPORT/IMPORT, SWEEP, TRAIN,",
            "ENABLE, DISABLE) land in Phase 1.",
            NULL
        };
        addReplyHelp(c, help);
    } else {
        addReplySubcommandSyntaxError(c);
    }
}

/* ========================================================================
 * INFO
 * ======================================================================== */

void infoCompression(sds *info) {
    if (!info || !*info) return;
    *info = sdscatprintf(*info,
        "# Compression\r\n"
        "compression_enabled:0\r\n"
        "compression_state:disabled\r\n"
        "compression_compressed_objects:0\r\n"
        "compression_total_uncompressed_bytes:0\r\n"
        "compression_total_compressed_bytes:0\r\n"
        "compression_net_saved_bytes:0\r\n");
}
