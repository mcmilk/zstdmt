
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

#include <stdlib.h>

#include "zstdmt.h"

// #define DEBUGME

#ifdef DEBUGME
#include <stdio.h>
#endif

struct ZSTDMT_CCtx_s {
	int threads;
	int level;

	/* complete buffers */
	size_t buffer_in_len;
	size_t buffer_out_len;
	void *buffer_in;
	void *buffer_out;

	/* number of frames done */
	size_t frames;
	size_t insize;
	size_t outsize;

	/**
	 * arrays, fixed to thread count
	 * - get allocated via ZSTDMT_createCCtx()
	 */
	ZBUFF_CCtx **cctx;
	void **inbuf;		/* buffers for decompressed data */
	void **outbuf;		/* buffers for compressed data */
	size_t *inlen;		/* length of decompressed data */
	size_t *outlen;		/* length of compressed data */
};

struct ZSTDMT_DCtx_s {
	/**
	 * number of threads, which can be used for decompressing
	 */
	int threads;

	/**
	 * number of threads used when compressing
	 * - now called streams here
	 */
	int streams;

	/* number of frames done */
	size_t frames;
	size_t insize;
	size_t outsize;

	/* complete buffers */
	size_t buffer_in_len;
	size_t buffer_out_len;
	void *buffer_in;
	void *buffer_out;

	/**
	 * arrays, fixed to thread count
	 * - get allocated via ZSTDMT_createDCtx()
	 */
	ZBUFF_DCtx **dctx;
	void **inbuf;		/* buffers for decompressed data */
	void **outbuf;		/* buffers for compressed data */
	size_t *inlen;		/* length of decompressed data */
	size_t *outlen;		/* length of compressed data */
};

/* **************************************
 *  Compression
 ****************************************/
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level)
{
	ZSTDMT_CCtx *ctx;
	int t;

	ctx = (ZSTDMT_CCtx *) malloc(sizeof(ZSTDMT_CCtx));
	if (!ctx) {
		return 0;
	}

	ctx->threads = threads;
	ctx->level = level;

	/* complete buffers */
	ctx->buffer_in_len = ZBUFF_recommendedCInSize();
	ctx->buffer_out_len = ZBUFF_recommendedCOutSize() + ZSTDMT_HDR_CHUNK;
	ctx->buffer_in = malloc(ctx->buffer_in_len * threads);
	if (!ctx->buffer_in) {
		free(ctx);
		return 0;
	}

	/* 2 bytes format header + 4 bytes chunk header per thread */
	ctx->buffer_out =
	    malloc(ZSTDMT_HDR_FORMAT + ctx->buffer_out_len * threads);
	if (!ctx->buffer_out) {
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* number of frames done */
	ctx->frames = 0;
	ctx->insize = 0;
	ctx->outsize = 0;

	/* arrays */
	ctx->cctx = malloc(sizeof(void *) * threads);
	if (!ctx->cctx) {
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->inbuf = malloc(sizeof(void *) * threads);
	if (!ctx->inbuf) {
		free(ctx->cctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outbuf = malloc(sizeof(void *) * threads);
	if (!ctx->outbuf) {
		free(ctx->inbuf);
		free(ctx->cctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* these are dynamic */
	ctx->inlen = (size_t *) malloc(sizeof(size_t) * threads);
	if (!ctx->inlen) {
		free(ctx->outbuf);
		free(ctx->inbuf);
		free(ctx->cctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outlen = (size_t *) malloc(sizeof(size_t) * threads);
	if (!ctx->outlen) {
		free(ctx->inlen);
		free(ctx->outbuf);
		free(ctx->inbuf);
		free(ctx->cctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	for (t = 0; t < threads; t++) {
		ctx->cctx[t] = ZBUFF_createCCtx();
		if (!ctx->cctx[t]) {
			free(ctx->outlen);
			free(ctx->inlen);
			free(ctx->outbuf);
			free(ctx->inbuf);
			free(ctx->cctx);
			free(ctx->buffer_out);
			free(ctx->buffer_in);
			free(ctx);
			return 0;
		}

		ZBUFF_compressInit(ctx->cctx[t], ctx->level);

		ctx->inbuf[t] = ctx->buffer_in + ctx->buffer_in_len * t;
		ctx->outbuf[t] =
		    ctx->buffer_out + ZSTDMT_HDR_FORMAT +
		    ctx->buffer_out_len * t;
	}

	/**
	 * 2^14 thread id
	 */
	{
		unsigned char *hdr = ctx->buffer_out;
		unsigned int tid = threads;
		hdr[0] = tid;
		tid >>= 8;
		hdr[1] = tid;
	}

	return ctx;
}

/* returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;
	return ctx->buffer_in;
}

/* returns the length of the input buffer */
size_t ZSTDMT_GetInSizeCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->buffer_in_len * ctx->threads;
}

size_t ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, size_t insize)
{
	size_t todo = insize, t, ret;

	if (!ctx)
		return 0;

	ctx->frames++;
	ctx->insize += insize;

	for (t = 0; t < ctx->threads && insize != 0; t++) {
		ctx->inlen[t] = ctx->buffer_in_len;
		ctx->outlen[t] = ctx->buffer_out_len - ZSTDMT_HDR_CHUNK;
		if (insize < ctx->inlen[t])
			ctx->inlen[t] = insize;

#ifdef DEBUGME
		printf("ZBUFF_compressContinueBefore()[%zu] outlen=%zu inlen=%zu\n",
		       t, ctx->outlen[t], ctx->inlen[t]);
#endif
		ret =
		    ZBUFF_compressContinue(ctx->cctx[t],
					   ctx->outbuf[t] + ZSTDMT_HDR_CHUNK,
					   &ctx->outlen[t], ctx->inbuf[t],
					   &ctx->inlen[t]);
		if (ZSTD_isError(ret))
			return ret;

#ifdef DEBUGME
		printf
		    ("ZBUFF_compressContinueReturn()[%zu] ret=%zu outlen=%zu inlen=%zu\n",
		     t, ret, ctx->outlen[t], ctx->inlen[t]);
#endif

		if (ret != ctx->inlen[t]) {
			ctx->outlen[t] = ctx->buffer_out_len;
			ret =
			    ZBUFF_compressFlush(ctx->cctx[t],
						ctx->outbuf[t] +
						ZSTDMT_HDR_CHUNK,
						&ctx->outlen[t]);
			if (ZSTD_isError(ret))
				return ret;

#ifdef DEBUGME
			printf
			    ("ZBUFF_compressFlushReturn()[%zu] ret=%zu outlen=%zu inlen=%zu\n",
			     t, ret, ctx->outlen[t], ctx->inlen[t]);
#endif
		}

		/**
		 * 2^14 thread id
		 * 2^18 compressed length
		 */
		{
			unsigned char *hdr = ctx->outbuf[t];
			unsigned int u = ctx->outlen[t], tid = t;

			/* test if rest of tid is wrong tid > 2^14" */
			if (tid > 16384)
				return -1;

			/* test id rest of compressed length is wrong, max is: 2^18 */
			if (u > 262144)
				return -1;

			hdr[0] = tid;
			tid >>= 8;
			hdr[3] = u;
			u >>= 8;
			hdr[2] = u;
			u >>= 8;
			hdr[1] = tid | u;
		}

		ctx->outsize += ctx->outlen[t];
		insize -= ctx->inlen[t];
	}

	/* no outlen in the next threads */
	while (t < ctx->threads)
		ctx->outlen[t++] = 0;

	return 0;
}

/* returns pointer to compressed output buffer of thread t with length len */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len)
{
	void *ret;

	if (!ctx)
		return 0;

	ret = ctx->outbuf[thread];
	*len = ctx->outlen[thread];

	if (thread == 0 && ctx->frames == 1) {
		/* special case, return the format header */
		*len += 2;
		ret = ctx->buffer_out;
	}

	return ret;
}

/* returns current frame number */
size_t ZSTDMT_GetCurrentFrameCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->frames;
}

/* returns current uncompressed data size */
size_t ZSTDMT_GetCurrentInsizeCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t ZSTDMT_GetCurrentOutsizeCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx)
{
	size_t t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++)
		ZBUFF_freeCCtx(ctx->cctx[t]);

	free(ctx->buffer_in);
	free(ctx->buffer_out);
	free(ctx->inbuf);
	free(ctx->outbuf);
	free(ctx->inlen);
	free(ctx->outlen);
	free(ctx->cctx);
	free(ctx);
	ctx = 0;

	return;
}

/* **************************************
 *  Decompression
 ****************************************/

ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads)
{
	ZSTDMT_DCtx *ctx;
	int t;

	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx) {
		return 0;
	}

	ctx->threads = threads;
	ctx->streams = 0;

	/* complete buffers */
	ctx->buffer_in_len = ZBUFF_recommendedDInSize() + ZSTDMT_HDR_CHUNK;
	ctx->buffer_out_len = ZBUFF_recommendedDOutSize();
	ctx->buffer_in = malloc(ZSTDMT_HDR_FORMAT + ctx->buffer_in_len * threads);
	if (!ctx->buffer_in) {
		free(ctx);
		return 0;
	}

	/* 2 bytes format header + 4 bytes chunk header per thread */
	ctx->buffer_out =
	    malloc(ctx->buffer_out_len * threads);
	if (!ctx->buffer_out) {
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* number of frames done */
	ctx->frames = 0;
	ctx->insize = 0;
	ctx->outsize = 0;

	/* arrays */
	ctx->dctx = malloc(sizeof(void *) * threads);
	if (!ctx->dctx) {
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->inbuf = malloc(sizeof(void *) * threads);
	if (!ctx->inbuf) {
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outbuf = malloc(sizeof(void *) * threads);
	if (!ctx->outbuf) {
		free(ctx->inbuf);
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* these are dynamic */
	ctx->inlen = (size_t *) malloc(sizeof(size_t) * threads);
	if (!ctx->inlen) {
		free(ctx->outbuf);
		free(ctx->inbuf);
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outlen = (size_t *) malloc(sizeof(size_t) * threads);
	if (!ctx->outlen) {
		free(ctx->inlen);
		free(ctx->outbuf);
		free(ctx->inbuf);
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	for (t = 0; t < threads; t++) {
		ctx->dctx[t] = ZBUFF_createDCtx();
		if (!ctx->dctx[t]) {
			free(ctx->outlen);
			free(ctx->inlen);
			free(ctx->outbuf);
			free(ctx->inbuf);
			free(ctx->dctx);
			free(ctx->buffer_out);
			free(ctx->buffer_in);
			free(ctx);
			return 0;
		}

		ZBUFF_decompressInit(ctx->dctx[t]);

		ctx->inbuf[t] = ZSTDMT_HDR_FORMAT + ctx->buffer_in + ctx->buffer_in_len * t;
		ctx->outbuf[t] =
		    ctx->buffer_out + ZSTDMT_HDR_FORMAT +
		    ctx->buffer_out_len * t;
	}

	/**
	 * 2^14 thread id
	 */
	{
		unsigned char *hdr = ctx->buffer_out;
		unsigned int tid = threads;
		hdr[0] = tid;
		tid >>= 8;
		hdr[1] = tid;
	}

	return ctx;
}

/* 2) returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferDCtx(ZSTDMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->buffer_in;
}

/* 3) returns the length of the input buffer */
size_t ZSTDMT_GetInSizeDCtx(ZSTDMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->buffer_in_len * ctx->threads;
}

/* 4) decompressing */
size_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, size_t srcsize)
{
	size_t todo = insize, t, ret;

	if (!ctx)
		return 0;

	ctx->frames++;
	ctx->insize += insize;

	for (t = 0; t < ctx->threads && insize != 0; t++) {
		ctx->inlen[t] = ctx->buffer_in_len;
		ctx->outlen[t] = ctx->buffer_out_len - ZSTDMT_HDR_CHUNK;
		if (insize < ctx->inlen[t])
			ctx->inlen[t] = insize;

#ifdef DEBUGME
		printf("ZBUFF_compressContinueBefore()[%zu] outlen=%zu inlen=%zu\n",
		       t, ctx->outlen[t], ctx->inlen[t]);
#endif
		ret =
		    ZBUFF_compressContinue(ctx->cctx[t],
					   ctx->outbuf[t] + ZSTDMT_HDR_CHUNK,
					   &ctx->outlen[t], ctx->inbuf[t],
					   &ctx->inlen[t]);
		if (ZSTD_isError(ret))
			return ret;

#ifdef DEBUGME
		printf
		    ("ZBUFF_compressContinueReturn()[%zu] ret=%zu outlen=%zu inlen=%zu\n",
		     t, ret, ctx->outlen[t], ctx->inlen[t]);
#endif

		if (ret != ctx->inlen[t]) {
			ctx->outlen[t] = ctx->buffer_out_len;
			ret =
			    ZBUFF_compressFlush(ctx->cctx[t],
						ctx->outbuf[t] +
						ZSTDMT_HDR_CHUNK,
						&ctx->outlen[t]);
			if (ZSTD_isError(ret))
				return ret;

#ifdef DEBUGME
			printf
			    ("ZBUFF_compressFlushReturn()[%zu] ret=%zu outlen=%zu inlen=%zu\n",
			     t, ret, ctx->outlen[t], ctx->inlen[t]);
#endif
		}

		/**
		 * 2^14 thread id
		 * 2^18 compressed length
		 */
		{
			unsigned char *hdr = ctx->outbuf[t];
			unsigned int u = ctx->outlen[t], tid = t;

			/* test if rest of tid is wrong tid > 2^14" */
			if (tid > 16384)
				return -1;

			/* test id rest of compressed length is wrong, max is: 2^18 */
			if (u > 262144)
				return -1;

			hdr[0] = tid;
			tid >>= 8;
			hdr[3] = u;
			u >>= 8;
			hdr[2] = u;
			u >>= 8;
			hdr[1] = tid | u;
		}

		ctx->outsize += ctx->outlen[t];
		insize -= ctx->inlen[t];
	}

	/* no outlen in the next threads */
	while (t < ctx->threads)
		ctx->outlen[t++] = 0;

	return 0;
}

/* 5) returns pointer to decompressed output buffer of thread */
void *ZSTDMT_GetDecompressedDCtx(ZSTDMT_DCtx * ctx, int thread, size_t * len)
{
	void *ret;

	if (!ctx)
		return 0;

	ret = ctx->outbuf[thread];
	*len = ctx->outlen[thread];

	if (thread == 0 && ctx->frames == 1) {
		/* special case, return the format header */
		*len += 2;
		ret = ctx->buffer_out;
	}

	return ret;
}

/* free dctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx)
{
	size_t t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++)
		ZBUFF_freeDCtx(ctx->dctx[t]);

	free(ctx->buffer_in);
	free(ctx->buffer_out);
	free(ctx->inbuf);
	free(ctx->outbuf);
	free(ctx->inlen);
	free(ctx->outlen);
	free(ctx->dctx);
	free(ctx);
	ctx = 0;
}
