/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * compression_workers.c — Phase 0 stub.
 *
 * No pool is started. Enqueue always succeeds but drops the job on
 * the floor; drain is a no-op. This is consistent with the "feature
 * disabled, quiet sink" Phase 0 contract.
 *
 * Phase 1 (plan.md §5) introduces:
 *   - pthread-based worker pool (one per compression-threads)
 *   - SPMC inbox from queues.h
 *   - MPSC outbox drained on afterSleep
 *   - ZSTD_compress_usingCDict on worker threads (flat buffers only)
 */

#include "server.h"
#include "compression_workers.h"

int compressionWorkersStart(int n_threads) {
    UNUSED(n_threads);
    return 0;
}

void compressionWorkersStop(void) {
    /* Phase 0: no-op. */
}

int compressionWorkersResize(int n_threads) {
    UNUSED(n_threads);
    return 0;
}

int compressionWorkersEnqueue(const sds key,
                              int dbid,
                              uint64_t version,
                              const unsigned char *src,
                              size_t src_len,
                              uint32_t active_dict_id) {
    UNUSED(key);
    UNUSED(dbid);
    UNUSED(version);
    UNUSED(src);
    UNUSED(src_len);
    UNUSED(active_dict_id);
    /* Phase 0: accept-and-drop. Callers are expected to have held an
     * incrRefCount on the robj; they must decrRefCount independently
     * once their own call path completes. We do not take ownership. */
    return 0;
}

int compressionWorkersDrainOutbox(int budget) {
    UNUSED(budget);
    /* Phase 0: nothing to drain. */
    return 0;
}
