/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * compression_header.c — Phase 0 stub (with working encode/decode).
 *
 * The header format is simple and stable enough to implement in Phase
 * 0: encode writes four uint32s in native byte order; decode validates
 * that alg_magic matches a known algorithm tag. createCompressedObject
 * / freeCompressedObject are stubs because no code path actually
 * produces compressed frames yet.
 *
 * The encode/decode helpers are written to be used by unit tests in
 * Phase 1 (per-value header round-trip tests per plan §7).
 */

#include "server.h"
#include "compression_header.h"

#include <string.h>

/* Returns 1 if `alg_magic` is a known algorithm tag. v1 only accepts
 * the ZSTD magic; additional entries can be added without changing
 * the on-disk/in-memory layout. */
static int compressionAlgMagicRecognized(uint32_t alg_magic) {
    switch (alg_magic) {
    case COMPRESSION_ALG_ZSTD_MAGIC:
        return 1;
    default:
        return 0;
    }
}

void compressionHeaderEncode(unsigned char *dst,
                             uint32_t alg_magic,
                             uint32_t alg_meta,
                             uint32_t uncompressed_len,
                             uint32_t compressed_len) {
    compressedHeader h = {
        .alg_magic = alg_magic,
        .alg_meta = alg_meta,
        .uncompressed_len = uncompressed_len,
        .compressed_len = compressed_len,
    };
    memcpy(dst, &h, sizeof(h));
}

int compressionHeaderDecode(const unsigned char *src, compressedHeader *out) {
    compressedHeader h;
    memcpy(&h, src, sizeof(h));
    if (!compressionAlgMagicRecognized(h.alg_magic)) return -1;
    if (out) *out = h;
    return 0;
}

robj *createCompressedObject(void *buffer, size_t buffer_len) {
    UNUSED(buffer);
    UNUSED(buffer_len);
    /* Phase 0: feature disabled, we never allocate compressed robjs.
     * Returning NULL signals "fall back to uncompressed storage" to
     * the Phase 1 installer. Note the header contract: on NULL
     * return the caller retains ownership of `buffer` and must
     * reclaim it with zfree — Phase 1 will follow the same rule for
     * the validation-failure path. */
    return NULL;
}

void freeCompressedObject(robj *o) {
    UNUSED(o);
    /* Phase 0: no-op. In Phase 1 this will zfree(o->val_ptr) and
     * compressionRegistryDecRef(header.alg_meta). */
}
