
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
 * - 2^14 bits: for thread count
 * - 2^18 bits: for compressed length
 */

#define ZSTDMT_THREADMAX 16384

/* XXX - todo */
#define ZSTDMT_STATE_NONE      1
#define ZSTDMT_STATE_INIT      2
#define ZSTDMT_STATE_READING   3
#define ZSTDMT_STATE_COMPRESS  4
#define ZSTDMT_STATE_WRITE     5
#define ZSTDMT_STATE_FINISH    6

typedef struct {
	int threads;
	int streams;
	int level;
	int state;

	/* complete buffers */
	void *buffer_in;
	void *buffer_out;
	int buffer_in_len;
	int buffer_out_len;

	/**
	 * arrays, fixed to thread count
	 * - get allocated via ZSTDMT_createCCtx()
	 */
	ZBUFF_CCtx **cctx;
	void **cbuf;		/* buffers for compressed data */
	void **dbuf;		/* buffers for decompressed data */
	int *clen;		/* length of compressed data */
	int *dlen;		/* length of decompressed data */
} ZSTDMT_CCtx;

typedef struct {
	int threads;
	int streams;

	/* complete buffers */
	void *buffer_in;
	void *buffer_out;
	int buffer_in_len;
	int buffer_out_len;

	/**
	 * arrays, fixed to thread count
	 * - get allocated via ZSTDMT_createDCtx()
	 */
	ZBUFF_DCtx **dctx;
	void **dbuf;		/* buffers for decompressed data */
	void **cbuf;		/* buffers for compressed data */
	int *dlen;		/* length of decompressed data */
	int *clen;		/* length of compressed data */
} ZSTDMT_DCtx;

/* **************************************
*  Compression
****************************************/

/* 1) returns new cctx or zero on error */
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int streams, int level);

/* 2) returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferCCtx(ZSTDMT_CCtx * ctx);

/* 3) returns the length of the input buffer */
int ZSTDMT_GetInSizeCCtx(ZSTDMT_CCtx * ctx);

/* 4) compressing */
void ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, int insize);

/* 5) returns pointer to compressed output buffer */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len);

/* 6) free cctx */
void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx);

/* **************************************
*  Decompression
****************************************/

/* returns new dctx or zero on error */
ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads);

/* XXX - add function definitions for decompression */

/* free dctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx);

