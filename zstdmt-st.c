
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

/**
 * simple single threaded zstd
 */

//#define DEBUGME

#ifdef DEBUGME
#include <stdio.h>
#endif

struct ZSTDMT_CCtx_s {

	/**
	 * number of threads, which are used for compressing
	 */
	int level;
	int threads;

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
	 * number of threads, which are used for decompressing
	 */
	int threads;

	/**
	 * how much work is loaded into the input buffers
	 */
	int worker;

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
	size_t t, ret;

	if (!ctx)
		return 0;

	if (ctx->frames == 1)
		ctx->outsize += 2;

	ctx->frames++;
	ctx->insize += insize;

	for (t = 0; t < ctx->threads && insize != 0; t++) {
		ctx->inlen[t] = ctx->buffer_in_len;
		ctx->outlen[t] = ctx->buffer_out_len - ZSTDMT_HDR_CHUNK;
		if (insize < ctx->inlen[t])
			ctx->inlen[t] = insize;
		ret =
		    ZBUFF_compressContinue(ctx->cctx[t],
					   ctx->outbuf[t] + ZSTDMT_HDR_CHUNK,
					   &ctx->outlen[t], ctx->inbuf[t],
					   &ctx->inlen[t]);
		if (ZSTD_isError(ret))
			return ret;

#ifdef DEBUGME
		printf
		    ("ZBUFF_compressContinue()[%2zu] ret=%zu inlen=%zu outlen=%zu\n",
		     t, ret, ctx->inlen[t], ctx->outlen[t]);
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
			    ("ZBUFF_compressFlush()[2%zu] ret=%zu inlen=%zu outlen=%zu\n",
			     t, ret, ctx->inlen[t], ctx->outlen[t]);
#endif
		}

		{
			unsigned char *hdr = ctx->outbuf[t];
			unsigned int len = ctx->outlen[t], tid = t;

			/**
			 * 2^14 thread id  (max = 0x3f ff)
			 * 2^18 compressed length (max = 3 ff ff)
			 *
			 * TTTT:TTTT TTTT:TTLL LLLL:LLLL LLLL:LLLL
			 */
			hdr[0] = tid;
			tid >>= 8;
			hdr[3] = len;
			len >>= 8;
			hdr[2] = len;
			len >>= 8;
			hdr[1] = tid | (len << 6);
		}

		ctx->outsize += ctx->outlen[t] + 4;
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
	*len = ctx->outlen[thread] + 4;

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

ZSTDMT_DCtx *ZSTDMT_createDCtx(unsigned char hdr[2])
{
	ZSTDMT_DCtx *ctx;
	int t;

	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx) {
		return 0;
	}

	ctx->threads = hdr[0] + ((hdr[1] & 0x3f) << 8);
	ctx->worker = 0;

	/* complete buffers */
	ctx->buffer_in_len = ZBUFF_recommendedDInSize();
	ctx->buffer_out_len = ZBUFF_recommendedDOutSize();
	ctx->buffer_in = malloc(ctx->buffer_in_len * ctx->threads);
	if (!ctx->buffer_in) {
		free(ctx);
		return 0;
	}

	/* 2 bytes format header + 4 bytes chunk header per thread */
	ctx->buffer_out = malloc(ctx->buffer_out_len * ctx->threads);
	if (!ctx->buffer_out) {
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* arrays */
	ctx->dctx = malloc(sizeof(void *) * ctx->threads);
	if (!ctx->dctx) {
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->inbuf = malloc(sizeof(void *) * ctx->threads);
	if (!ctx->inbuf) {
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outbuf = malloc(sizeof(void *) * ctx->threads);
	if (!ctx->outbuf) {
		free(ctx->inbuf);
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	/* these are dynamic */
	ctx->inlen = (size_t *) malloc(sizeof(size_t) * ctx->threads);
	if (!ctx->inlen) {
		free(ctx->outbuf);
		free(ctx->inbuf);
		free(ctx->dctx);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->outlen = (size_t *) malloc(sizeof(size_t) * ctx->threads);
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

	for (t = 0; t < ctx->threads; t++) {
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

		ctx->inbuf[t] = ctx->buffer_in + ctx->buffer_in_len * t;
		ctx->outbuf[t] = ctx->buffer_out + ctx->buffer_out_len * t;
	}

	/**
	 * 2^14 thread id
	 */
	{
		unsigned char *hdr = ctx->buffer_out;
		unsigned int tid = ctx->threads;
		hdr[0] = tid;
		tid >>= 8;
		hdr[1] = tid;
	}

	return ctx;
}

unsigned int ZSTDMT_GetThreadsDCtx(ZSTDMT_DCtx * ctx)
{
	return ctx->threads;
}

void *ZSTDMT_GetNextBufferDCtx(ZSTDMT_DCtx * ctx, unsigned char hdr[4],
			       int thread, size_t * len)
{
	unsigned int tid = hdr[0] + ((hdr[1] & 0x3f) << 8);

	*len = hdr[3] + (hdr[2] << 8) + (((hdr[1] & 0xc0) << 16) >> 6);
	ctx->inlen[thread] = *len;

	ctx->worker = thread + 1;

#ifdef DEBUGME
	printf
	    ("*ZSTDMT_GetNextBufferDCtx() tid=%u len=%zu ctx->inbuf[thread]=%p\n",
	     tid, *len, ctx->inbuf[thread]);
#endif

	/* input failure */
	if (tid > ctx->threads)
		return 0;

	return ctx->inbuf[thread];
}

/* 4) threaded decompression */
void *ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, size_t * len)
{
	size_t t;

#ifdef DEBUGME
	printf("*ZSTDMT_DecompressDCtx() threads=%u work=%u\n",
	       ctx->threads, ctx->worker);
#endif

	*len = 0;
	for (t = 0; t < ctx->worker; t++) {
		ctx->outlen[t] = ctx->buffer_out_len;
#ifdef DEBUGME
		printf
		    ("ZBUFF_decompressContinueBefore[%zu] outlen=%zu inlen=%zu\n",
		     t, ctx->outlen[t], ctx->inlen[t]);
#endif
		size_t ret =
		    ZBUFF_decompressContinue(ctx->dctx[t], ctx->outbuf[t],
					     &ctx->outlen[t], ctx->inbuf[t],
					     &ctx->inlen[t]);
#ifdef DEBUGME
		printf
		    ("ZBUFF_decompressContinueThen[%zu] ret=%zu outlen=%zu inlen=%zu\n",
		     t, ret, ctx->outlen[t], ctx->inlen[t]);
#endif
		if (ZSTD_isError(ret))
			return 0;

		*len += ctx->outlen[t];
	}

	return ctx->buffer_out;
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
