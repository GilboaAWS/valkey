/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * compression_registry.c — Phase 0 stub.
 *
 * All registry operations are feature-disabled. Active dict is always
 * NULL; Lookup always returns NULL; Add is rejected. Real dictionary
 * lifecycle is Phase 1 work (plan.md §5 — dictionary lifecycle subsystem).
 *
 * Field naming note: the public struct `compressionDictPair` uses the
 * opaque ZSTD_CDict / ZSTD_DDict typedefs forward-declared in
 * compression_registry.h. Since Phase 0 does not allocate dict pairs,
 * nothing here actually instantiates zstd handles.
 */

#include "server.h"
#include "compression_registry.h"

void compressionRegistryInit(void) {
    /* Phase 0: no state. */
}

void compressionRegistryRelease(void) {
    /* Phase 0: nothing to release. */
}

compressionDictPair *compressionRegistryActive(void) {
    /* Phase 0: no active dict. Callers must treat this as "no-dict
     * mode", which is also how the R2.1.5 third-state is represented. */
    return NULL;
}

compressionDictPair *compressionRegistryLookup(uint32_t dict_id) {
    UNUSED(dict_id);
    /* Phase 0: registry is empty. The RDB loader uses this to detect the
     * "missing dict AUX entry" corruption case (R2.6.5); until the
     * registry is populated, any lookup is a miss. */
    return NULL;
}

uint32_t compressionRegistryAdd(compressionDictPair *p) {
    UNUSED(p);
    /* Phase 0: reject any add — feature is disabled. Ownership contract
     * says we own `p` on success; on rejection the caller still owns
     * it, so we must not free here. */
    return COMPRESSION_DICT_ID_NONE;
}

int compressionRegistryRetire(uint32_t dict_id) {
    UNUSED(dict_id);
    return -1;
}

void compressionRegistryIncRef(uint32_t dict_id) {
    UNUSED(dict_id);
    /* Phase 0: no-op. */
}

void compressionRegistryDecRef(uint32_t dict_id) {
    UNUSED(dict_id);
    /* Phase 0: no-op. */
}

void compressionRegistryForEach(void (*cb)(const compressionDictPair *, void *), void *ctx) {
    UNUSED(cb);
    UNUSED(ctx);
    /* Phase 0: empty iteration. */
}
