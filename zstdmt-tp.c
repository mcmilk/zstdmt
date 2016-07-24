
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

#include <pthread.h>
#include <stdlib.h>

#include "zstdmt.h"
#include "workq.h"

/**
 * multi threaded zstd - thread pool
 */

//#define DEBUGME

#ifdef DEBUGME
#include <stdio.h>
#endif

/* thread work */
typedef struct {
	void *ctx; /* ZBUFF_CCtx ZBUFF_DCtx */ ;
	void *inbuf;
	void *outbuf;
	size_t tid;
	size_t inlen;
	size_t outlen;
} work_t;

struct ZSTDMT_CCtx_s {

	/* number of threads and compression level */
	int level;
	int threads;

	/* complete buffers */
	size_t optimal_inlen;
	size_t optimal_outlen;
	void *buffer_in;
	void *buffer_out;

	/* statistic */
	size_t frames;
	size_t insize;
	size_t outsize;

	/* threading */
	pthread_t *th;
	work_t *work;
};

struct ZSTDMT_DCtx_s {

	/* number of threads, which are used for decompressing */
	int threads;

	/* how much work is loaded into the input buffers */
	int worker;

	/* complete buffers */
	size_t optimal_inlen;
	size_t optimal_outlen;
	void *buffer_in;
	void *buffer_out;

	/* threading */
	pthread_t *th;
	work_t *work;
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

	/* integer */
	ctx->threads = threads;
	ctx->level = level;
	ctx->frames = 0;
	ctx->insize = 0;
	ctx->outsize = 0;

	/* buffers */
	ctx->optimal_inlen = ZBUFF_recommendedCInSize();
	ctx->optimal_outlen = ZBUFF_recommendedCOutSize() + ZSTDMT_HDR_CHUNK;
	ctx->buffer_in = malloc(ctx->optimal_inlen * threads);
	if (!ctx->buffer_in) {
		free(ctx);
		return 0;
	}

	/* 2 bytes format header + 4 bytes chunk header per thread */
	ctx->buffer_out =
	    malloc(ZSTDMT_HDR_FORMAT + ctx->optimal_outlen * threads);
	if (!ctx->buffer_out) {
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->work = (work_t *) malloc(sizeof(work_t) * threads);
	if (!ctx->work) {
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->th = (pthread_t *) malloc(sizeof(pthread_t) * threads);
	if (!ctx->th) {
		free(ctx->work);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	for (t = 0; t < threads; t++) {
		ctx->work[t].ctx = ZBUFF_createCCtx();
		if (!ctx->work[t].ctx) {
			free(ctx->work);
			free(ctx->buffer_out);
			free(ctx->buffer_in);
			free(ctx);
			return 0;
		}

		ZBUFF_compressInit(ctx->work[t].ctx, ctx->level);
		ctx->work[t].tid = t;
		ctx->work[t].inbuf = ctx->buffer_in + ctx->optimal_inlen * t;
		ctx->work[t].outbuf =
		    ctx->buffer_out + ZSTDMT_HDR_FORMAT +
		    ctx->optimal_outlen * t;
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

	return ctx->optimal_inlen * ctx->threads;
}

static void *pt_compress(void *arg)
{
	work_t *work = (work_t *) arg;
	size_t ret;

	work->outlen = ZBUFF_recommendedCOutSize();
	ret =
	    ZBUFF_compressContinue(work->ctx,
				   work->outbuf + ZSTDMT_HDR_CHUNK,
				   &work->outlen, work->inbuf, &work->inlen);
	if (ZSTD_isError(ret))
		return 0;

#ifdef DEBUGME
	printf("ZBUFF_compressContinue()[%2zu] ret=%zu inlen=%zu outlen=%zu\n",
	       work->tid, ret, work->inlen, work->outlen);
#endif

	if (ret != work->inlen) {
		work->outlen = ZBUFF_recommendedCOutSize();
		ret =
		    ZBUFF_compressFlush(work->ctx,
					work->outbuf +
					ZSTDMT_HDR_CHUNK, &work->outlen);

#ifdef DEBUGME
		printf
		    ("ZBUFF_compressFlush()[%2zu] ret=%zu inlen=%zu outlen=%zu\n",
		     work->tid, ret, work->inlen, work->outlen);
#endif

		if (ZSTD_isError(ret))
			return 0;

	}

	{
		unsigned char *hdr = work->outbuf;
		unsigned int len = work->outlen, tid = work->tid;

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

	return 0;
}

size_t ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, size_t insize)
{
	size_t t, nthreads;

	if (!ctx)
		return 0;

	if (ctx->frames == 1)
		ctx->outsize += 2;

	ctx->frames++;
	ctx->insize += insize;

	for (t = 0; t < ctx->threads && insize != 0; t++) {
		work_t *work = ctx->work;
		work[t].inlen = ctx->optimal_inlen;
		if (insize < work[t].inlen)
			work[t].inlen = insize;
		insize -= ctx->work[t].inlen;
		pthread_create(&ctx->th[t], NULL, pt_compress, &ctx->work[t]);
	}

	nthreads = t;
	for (t = 0; t < nthreads; t++) {
		pthread_join(ctx->th[t], NULL);
		ctx->outsize += ctx->work[t].outlen + 4;
	}

	/* no outlen in the next threads, if there */
	while (t < ctx->threads)
		ctx->work[t++].outlen = 0;

	return 0;
}

/* returns pointer to compressed output buffer of thread t with length len */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len)
{
	void *ret;

	if (!ctx)
		return 0;

	if (ctx->work[thread].outlen == 0)
		return 0;

	ret = ctx->work[thread].outbuf;
	*len = ctx->work[thread].outlen + 4;

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

	for (t = 0; t < ctx->threads; t++) {
		ZBUFF_freeCCtx(ctx->work[t].ctx);
	}

	free(ctx->buffer_in);
	free(ctx->buffer_out);
	free(ctx->work);
	free(ctx->th);
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
	ctx->optimal_inlen = ZBUFF_recommendedDInSize();
	ctx->optimal_outlen = ZBUFF_recommendedDOutSize();
	ctx->buffer_in = malloc(ctx->optimal_inlen * ctx->threads);
	if (!ctx->buffer_in) {
		free(ctx);
		return 0;
	}
	ctx->buffer_out = malloc(ctx->optimal_outlen * ctx->threads);
	if (!ctx->buffer_out) {
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->work = (work_t *) malloc(sizeof(work_t) * ctx->threads);
	if (!ctx->work) {
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	ctx->th = (pthread_t *) malloc(sizeof(pthread_t) * ctx->threads);
	if (!ctx->th) {
		free(ctx->work);
		free(ctx->buffer_out);
		free(ctx->buffer_in);
		free(ctx);
		return 0;
	}

	for (t = 0; t < ctx->threads; t++) {
		ctx->work[t].ctx = ZBUFF_createDCtx();
		if (!ctx->work[t].ctx) {
			free(ctx->th);
			free(ctx->work);
			free(ctx->buffer_out);
			free(ctx->buffer_in);
			free(ctx->work);
			return 0;
		}

		ZBUFF_decompressInit(ctx->work[t].ctx);
		ctx->work[t].tid = t;
		ctx->work[t].inbuf = ctx->buffer_in + ctx->optimal_inlen * t;
		ctx->work[t].outbuf = ctx->buffer_out + ctx->optimal_outlen * t;
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
	ctx->work[thread].inlen = *len;

	ctx->worker = thread + 1;

	/* input failure */
	if (tid > ctx->threads)
		return 0;

#ifdef DEBUGME
	printf
	    ("*ZSTDMT_GetNextBufferDCtx() tid=%u len=%zu ctx->inbuf[thread]=%p worker=%d\n",
	     tid, *len, ctx->work[thread].inbuf, ctx->worker);
#endif

	return ctx->work[thread].inbuf;
}

static void *pt_decompress(void *arg)
{
	work_t *work = (work_t *) arg;
	size_t ret;

#ifdef DEBUGME
	printf("ZBUFF_decompressContinueA[%zu] inlen=%zu outlen=%zu\n",
	       work->tid, work->outlen, work->inlen);
#endif
	ret = ZBUFF_decompressContinue(work->ctx,
				       work->outbuf,
				       &work->outlen,
				       work->inbuf, &work->inlen);

#ifdef DEBUGME
	printf("ZBUFF_decompressContinueB[%zu] inlen=%zu outlen=%zu\n",
	       work->tid, work->outlen, work->inlen);
#endif

	if (ZSTD_isError(ret))
		return 0;

	return 0;
}

/* 4) threaded decompression */
void *ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, size_t * len)
{
	size_t t, ret;

	*len = 0;

#ifdef DEBUGME
	printf("*ZSTDMT_DecompressDCtx(%d) worker=%d\n",
	       ctx->threads, ctx->worker);
	fflush(stdout);
#endif

	for (t = 0; t < ctx->worker; t++) {
		ctx->work[t].outlen = ctx->optimal_outlen;
		ret =
		    pthread_create(&ctx->th[t], NULL, pt_decompress,
				   &ctx->work[t]);
		if (ret != 0)
			return 0;
	}

	for (t = 0; t < ctx->worker; t++) {
		pthread_join(ctx->th[t], NULL);
		*len += ctx->work[t].outlen;
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
		ZBUFF_freeDCtx(ctx->work[t].ctx);

	free(ctx->buffer_in);
	free(ctx->buffer_out);
	free(ctx->work);
	free(ctx->th);
	ctx = 0;
}
