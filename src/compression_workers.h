/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __COMPRESSION_WORKERS_H
#define __COMPRESSION_WORKERS_H

/*
 * Compression worker pool — background compression only.
 *
 * Design of record:
 *   .agents/planning/realtime-data-compression/design/detailed-design.md §4.6
 *   .agents/planning/realtime-data-compression/design/detailed-design.md §2.11
 *
 * Hard invariants (§2.11 R2.11.4):
 *   - Workers NEVER touch `robj`. They consume and produce flat byte
 *     buffers via the SPMC inbox / MPSC outbox.
 *   - Main thread owns all `robj` mutation: enqueue a candidate from
 *     dbAdd/dbOverwrite, poll the outbox on afterSleep, install the
 *     compressed buffer into the robj, update the registry refcount.
 *   - Pool is sized by `compression-threads` (independent of
 *     `io-threads` — §2.11 R2.11.1).
 *   - Sweep pacing is governed by `compression-sweep-max-cpu-pct`.
 *     On-demand jobs (training promotion, multi-key compress) are not
 *     paced; they are arrival-bounded.
 *
 * This header exposes only the operations the rest of the codebase
 * needs: pool start/stop, enqueue, drain. The SPMC/MPSC queues
 * themselves are internal to compression_workers.c (based on the
 * primitives in src/queues.h).
 */

#include "server.h"

#include <stddef.h>
#include <stdint.h>

/* ========================================================================
 * Pool lifecycle
 * ========================================================================
 *
 * Called from compressionInit (startup) and from the
 * `compression-threads` config apply path (runtime resize). `n_threads`
 * == 0 disables the pool entirely — eligible candidates still enqueue
 * but no work happens (this is the "enable without auto-sweep" pattern
 * from §2.1 R2.1.3).
 */

int  compressionWorkersStart(int n_threads);
void compressionWorkersStop(void);
int  compressionWorkersResize(int n_threads);

/* ========================================================================
 * Candidate inbox (main thread → workers)
 * ========================================================================
 *
 * Enqueue a compression job for an already-eligible candidate. Caller
 * MUST have held `incrRefCount(val)` so that the `src` pointer remains
 * valid for the worker AND the COW invariant (§2.4 R2.4.4) is enforced
 * on any subsequent mutating command.
 *
 * Returns 0 on success, -1 if the inbox is full (caller drops the
 * candidate — it will be retried by the next sweep tick).
 */
int compressionWorkersEnqueue(const sds key,
                              int dbid,
                              uint64_t version,
                              const unsigned char *src,
                              size_t src_len,
                              uint32_t active_dict_id);

/* ========================================================================
 * Result outbox (workers → main thread)
 * ========================================================================
 *
 * Polled from compressionAfterSleep. Processes up to `budget` results,
 * installing compressed buffers into the owning robjs (via
 * createCompressedObject) and running the net-savings guard
 * (§2.4 R2.4.3). Returns the number of results processed.
 */
int compressionWorkersDrainOutbox(int budget);

#endif /* __COMPRESSION_WORKERS_H */
