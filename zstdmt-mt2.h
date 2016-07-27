
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
 *
 * 4 bytes chunk header:
 * - 2^14 bits: for current thread id
 * - 2^18 bits: for compressed length
 */

#define ZSTDMT_HDR_FORMAT 2
#define ZSTDMT_HDR_CHUNK  4
#define ZSTDMT_THREADMAX  16384 - 1

/**
 * reading and writing functions
 * - so you may use stdio functions or other things
 * - just write some wrapper on your own
 */
typedef ssize_t(fn_read) (int fd, void *buf, size_t count);
typedef ssize_t(fn_write) (int fd, const void *buf, size_t count);

/* **************************************
 *  Compression
 ****************************************/

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 */
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level);

/**
 * 2) threaded compresion
 * - return compressed size or -1 on error
 */
ssize_t ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, fn_read, fn_write, int fdin,
			    int fdout);

/**
 * 3) free cctx
 * - no special return value
 */
void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx);

/* statistic */
size_t ZSTDMT_GetCurrentFrameCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetCurrentInsizeCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetCurrentOutsizeCCtx(ZSTDMT_CCtx * ctx);

/* **************************************
 *  Decompression
 ****************************************/

typedef struct ZSTDMT_DCtx_s ZSTDMT_DCtx;

/**
 * 1) allocate new dctx
 * - return dctx or zero on error
 */
ZSTDMT_DCtx *ZSTDMT_createDCtx(unsigned char hdr[2]);

/**
 * 2) threaded decompresion
 * - returns decompressed size or -1 on error
 */
ssize_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, fn_read, fn_write, int fdin,
			      int fdout);

/**
 * 3) free cctx
 * - no special return value
 */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx);
