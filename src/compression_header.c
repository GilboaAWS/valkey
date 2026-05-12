/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * compression_header.c — Phase 0 stub (with working encode/decode).
 *
 * The header format is simple and stable enough to implement in Phase
 * 0: encode writes four uint32s in native byte order and validates the
 * magic on decode. createCompressedObject / freeCompressedObject are
 * stubs because no code path actually produces compressed frames yet.
 *
 * The encode/decode helpers are written to be used by unit tests in
 * Phase 1 (per-value header round-trip tests per plan §7).
 */

#include "server.h"
#include "compression_header.h"

#include <string.h>

void compressionHeaderEncode(unsigned char *dst,
                             uint32_t dict_id,
                             uint32_t uncompressed_len,
                             uint32_t compressed_len) {
    compressedHeader h = {
        .magic = COMPRESSION_HEADER_MAGIC,
        .dict_id = dict_id,
        .uncompressed_len = uncompressed_len,
        .compressed_len = compressed_len,
    };
    memcpy(dst, &h, sizeof(h));
}

int compressionHeaderDecode(const unsigned char *src, compressedHeader *out) {
    compressedHeader h;
    memcpy(&h, src, sizeof(h));
    if (h.magic != COMPRESSION_HEADER_MAGIC) return -1;
    if (out) *out = h;
    return 0;
}

robj *createCompressedObject(uint32_t dict_id,
                             const void *compressed_frame,
                             uint32_t compressed_len,
                             uint32_t uncompressed_len) {
    UNUSED(dict_id);
    UNUSED(compressed_frame);
    UNUSED(compressed_len);
    UNUSED(uncompressed_len);
    /* Phase 0: feature disabled, we never allocate compressed robjs.
     * Returning NULL signals "fall back to uncompressed storage" to the
     * future Phase 1 installer. */
    return NULL;
}

void freeCompressedObject(robj *o) {
    UNUSED(o);
    /* Phase 0: no-op. In Phase 1 this will zfree(o->val_ptr) and
     * compressionRegistryDecRef(header.dict_id). */
}
