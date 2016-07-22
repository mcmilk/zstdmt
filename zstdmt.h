
/**
 * Copyright © 2016 Tino Reichardt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zbuff.h>

/* **************************************
 *  Thread functions
 ****************************************/

/**
 * Format Definition
 *
 * 2 bytes format header:
 * - 2^14 bits: threads count
 * - 2^2 bits: reserved
 *
 * 4 bytes chunk header:
 * - 2^14 bits: for current thread id
 * - 2^18 bits: for compressed length
 */

#define ZSTDMT_HDR_FORMAT 2
#define ZSTDMT_HDR_CHUNK  4
#define ZSTDMT_THREADMAX  16384

/* **************************************
 *  Compression
 ****************************************/

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx on success
 * - return zero on error
 */
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level);

/* 2) returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferCCtx(ZSTDMT_CCtx * ctx);

/* 3) returns the length of the input buffer */
size_t ZSTDMT_GetInSizeCCtx(ZSTDMT_CCtx * ctx);

/* 4) compressing */
size_t ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, size_t srcsize);

/* 5) returns pointer to compressed output buffer of thread */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len);

/* statistic */
size_t ZSTDMT_GetCurrentFrameCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetCurrentInsizeCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetCurrentOutsizeCCtx(ZSTDMT_CCtx * ctx);

/* 6) free cctx */
void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx);

/* **************************************
 *  Decompression
 ****************************************/

typedef struct ZSTDMT_DCtx_s ZSTDMT_DCtx;

/* 1) returns new dctx or zero on error */
ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads);

/* 2) returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferDCtx(ZSTDMT_DCtx * ctx);

/* 3) returns the length of the input buffer */
size_t ZSTDMT_GetInSizeDCtx(ZSTDMT_DCtx * ctx);

/* 4) decompressing */
size_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, size_t srcsize);

/* 5) returns pointer to decompressed output buffer of thread */
void *ZSTDMT_GetDecompressedDCtx(ZSTDMT_DCtx * ctx, int thread, size_t * len);

/* free dctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx);
