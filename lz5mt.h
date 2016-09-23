
/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

/* ***************************************
 * Defines
 ****************************************/

#ifndef LZ5MT_H
#define LZ5MT_H

#if defined (__cplusplus)
extern "C" {
#endif

/* current maximum the library will accept */
#define LZ5MT_THREAD_MAX 128

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	int size;		/* length of buf */
} LZ5MT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS
 */
typedef int (fn_read) (void *args, LZ5MT_Buffer * in);
typedef int (fn_write) (void *args, LZ5MT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} LZ5MT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct LZ5MT_CCtx_s LZ5MT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. LZ5MT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
LZ5MT_CCtx *LZ5MT_createCCtx(int threads, int level, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
int LZ5MT_CompressCCtx(LZ5MT_CCtx * ctx, LZ5MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ5MT_GetFramesCCtx(LZ5MT_CCtx * ctx);
size_t LZ5MT_GetInsizeCCtx(LZ5MT_CCtx * ctx);
size_t LZ5MT_GetOutsizeCCtx(LZ5MT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ5MT_freeCCtx(LZ5MT_CCtx * ctx);

/* **************************************
 * Decompression - TODO, but it's easy...
 ****************************************/

typedef struct LZ5MT_DCtx_s LZ5MT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 22
 * @threads - 1 .. LZ5MT_THREAD_MAX
 * @srclen  - the max size of src for LZ5MT_CompressCCtx()
 * @dstlen  - the min size of dst
 */
LZ5MT_DCtx *LZ5MT_createDCtx(int threads);

/**
 * 2) threaded compression
 * - return -1 on error
 */
int LZ5MT_DecompressDCtx(LZ5MT_DCtx * ctx, LZ5MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ5MT_GetFramesDCtx(LZ5MT_DCtx * ctx);
size_t LZ5MT_GetInsizeDCtx(LZ5MT_DCtx * ctx);
size_t LZ5MT_GetOutsizeDCtx(LZ5MT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ5MT_freeDCtx(LZ5MT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif
#endif				/* LZ5MT_H */
