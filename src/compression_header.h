/*
 * Copyright (c) Valkey Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __COMPRESSION_HEADER_H
#define __COMPRESSION_HEADER_H

/*
 * Per-value compressed-buffer header + allocation helpers.
 *
 * Design of record:
 *   .agents/planning/realtime-data-compression/design/detailed-design.md §5.2
 *
 * Layout (16 B header + ZSTD frame):
 *
 *   +-----------------------+-----------------------+
 *   | compressedHeader (16) | ZSTD frame bytes      |
 *   +-----------------------+-----------------------+
 *
 *   compressedHeader {
 *       uint32_t magic;               // 0x5A444943 = "ZDIC" little-endian sanity
 *       uint32_t dict_id;             // references a registry entrylet
 *       uint32_t uncompressed_len;    // original payload length
 *       uint32_t compressed_len;      // frame bytes, excluding header
 *   }
 *
 * The struct is packed and little-endian on disk-and-wire; we do not
 * persist it directly — RDB (§2.6 R2.6.1) re-encodes these fields via
 * rdb length-encoding. This layout is only the in-memory representation
 * pointed to by a robj with encoding=OBJ_ENCODING_COMPRESSED.
 *
 * Boundary: this header is owned by the compression hot path. The
 * dictionary registry does not touch it. The RDB load/save path reads
 * the four fields but writes the on-disk variant separately.
 */

#include "server.h"

#include <stddef.h>
#include <stdint.h>

#define COMPRESSION_HEADER_MAGIC 0x5A444943u    /* "ZDIC" */
#define COMPRESSION_HEADER_SIZE  16u            /* sizeof(compressedHeader) */

typedef struct compressedHeader {
    uint32_t magic;
    uint32_t dict_id;
    uint32_t uncompressed_len;
    uint32_t compressed_len;
} compressedHeader;

_Static_assert(sizeof(compressedHeader) == COMPRESSION_HEADER_SIZE,
               "compressedHeader must be exactly 16 bytes");

/* ========================================================================
 * Encoding / decoding
 * ======================================================================== */

/* Writes a header into `dst` in native byte order. `dst` must point at
 * COMPRESSION_HEADER_SIZE bytes of writable storage. */
void compressionHeaderEncode(unsigned char *dst,
                             uint32_t dict_id,
                             uint32_t uncompressed_len,
                             uint32_t compressed_len);

/* Reads a header from `src` and validates the magic. Returns 0 on
 * success, -1 on magic mismatch (caller treats as corrupt value). */
int  compressionHeaderDecode(const unsigned char *src,
                             compressedHeader *out);

/* ========================================================================
 * robj allocation / free helpers
 * ========================================================================
 *
 * Called by the compression main-thread install path (§2.4 R2.4.3) and
 * the complementary free path (driven by `freeStringObject`/`decrRefCount`
 * in object.c once the hot path is wired in Phase 1).
 *
 * `createCompressedObject` allocates a compressed-frame buffer sized for
 * the header plus `compressed_len`, copies the frame bytes, writes the
 * header, and returns a robj with encoding=OBJ_ENCODING_COMPRESSED.
 * Takes ownership of nothing; callers still own their inputs.
 */
robj *createCompressedObject(uint32_t dict_id,
                             const void *compressed_frame,
                             uint32_t compressed_len,
                             uint32_t uncompressed_len);

/* Frees the compressed buffer owned by a robj with
 * encoding=OBJ_ENCODING_COMPRESSED. Called from freeStringObject. */
void freeCompressedObject(robj *o);

#endif /* __COMPRESSION_HEADER_H */
