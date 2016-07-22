
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

/* XXX - temp */
#include <stdio.h>

#include "zstdmt.h"

static void die_nomem();
static void die_msg();

static void die_nomem()
{
	exit(1);
}

static void die_msg(const char *msg)
{
	perror(msg);
	exit(1);
}

/**
 *
 */
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level)
{
	ZSTDMT_CCtx *ctx;
	int t;

	ctx = (ZSTDMT_CCtx *) malloc(sizeof(ZSTDMT_CCtx));
	if (!ctx)
		die_nomem();

	ctx->threads = threads;
	ctx->level = level;
	ctx->state = ZSTDMT_STATE_INIT;

	/* complete buffers */
	ctx->buffer_in_len = ZBUFF_recommendedCInSize();
	ctx->buffer_out_len = ZBUFF_recommendedCOutSize() + ZSTDMT_HDR_CHUNK;
	ctx->buffer_in = malloc(ctx->buffer_in_len * threads);
	if (!ctx->buffer_in)
		die_nomem();

	/* 2 bytes format header + 4 bytes chunk header per thread */
	ctx->buffer_out = malloc(ZSTDMT_HDR_FORMAT + ctx->buffer_out_len * threads);
	if (!ctx->buffer_out)
		die_nomem();

	/* number of frames done */
	ctx->frames = 0;
	ctx->insize = 0;
	ctx->outsize = 0;

	/* arrays */
	ctx->cctx = malloc(sizeof(void *) * threads);
	if (!ctx->cctx)
		die_nomem();
	ctx->inbuf = malloc(sizeof(void *) * threads);
	if (!ctx->inbuf)
		die_nomem();
	ctx->outbuf = malloc(sizeof(void *) * threads);
	if (!ctx->outbuf)
		die_nomem();

	/* these are dynamic */
	ctx->inlen = (size_t *)malloc(sizeof(size_t) * threads);
	if (!ctx->inlen)
		die_nomem();
	ctx->outlen = (size_t *)malloc(sizeof(size_t) * threads);
	if (!ctx->outlen)
		die_nomem();

	for (t = 0; t < threads; t++) {
		ctx->cctx[t] = ZBUFF_createCCtx();
		if (!ctx->cctx[t])
			die_nomem();
		ZBUFF_compressInit(ctx->cctx[t], ctx->level);

		ctx->inbuf[t] = ctx->buffer_in + ctx->buffer_in_len * t;
		ctx->outbuf[t] = ctx->buffer_out + ZSTDMT_HDR_FORMAT + ctx->buffer_out_len * t;
	}

	return ctx;
}

/* returns the length of the input buffer */
size_t ZSTDMT_GetInSizeCCtx(ZSTDMT_CCtx * ctx)
{
	return ctx->buffer_in_len * ctx->threads;
}

/* returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferCCtx(ZSTDMT_CCtx * ctx)
{
	printf("ctx->buffer_in = %p\n", ctx->buffer_in);
	return ctx->buffer_in;
}

void ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, size_t insize)
{
	size_t todo = insize, t, ret;

	ctx->frames++;
	ctx->insize += insize;

	for (t = 0; t < ctx->threads && insize != 0; t++) {
		ctx->inlen[t] = ctx->buffer_in_len;
		ctx->outlen[t] = ctx->buffer_out_len;
		if (insize < ctx->inlen[t])
			ctx->inlen[t] = insize;
		printf ("ZBUFF_compressContinue1()[%zu] outlen=%zu inlen=%zu\n", t, ctx->outlen[t], ctx->inlen[t]);
		ret = ZBUFF_compressContinue(ctx->cctx[t], ctx->outbuf[t], &ctx->outlen[t], ctx->inbuf[t], &ctx->inlen[t]);
		if (ZSTD_isError(ret))
			die_msg(ZSTD_getErrorName(ret));
		printf ("ZBUFF_compressContinue2()[%zu] ret=%zu outlen=%zu inlen=%zu\n", t, ret, ctx->outlen[t], ctx->inlen[t]);

		if (ret != ctx->inlen[t]) {
			ctx->outlen[t] = ctx->buffer_out_len;
			ret = ZBUFF_compressFlush(ctx->cctx[t], ctx->outbuf[t], &ctx->outlen[t]);
			if (ZSTD_isError(ret))
				die_msg(ZSTD_getErrorName(ret));
			printf ("ZBUFF_compressFlush()[%zu] ret=%zu outlen=%zu inlen=%zu\n", t, ret, ctx->outlen[t], ctx->inlen[t]);
		}

		ctx->outsize += ctx->outlen[t];
		insize -= ctx->inlen[t];
	}

	/* no outlen in the next threads */
	while (t < ctx->threads) {
		printf ("reset outlen of thread %zu\n", t);
		ctx->outlen[t++] = 0;
	}

	return;
}

/* returns pointer to compressed output buffer of thread t with length len */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len)
{
	void *ret = ctx->outbuf[thread];
	*len = ctx->outlen[thread];

	if (thread == 0 && ctx->frames == 1) {
		/* special case, return the format header */
		*len += 2;
		ret = ctx->buffer_out;
	}

	printf ("thread=%d, len=%zu, frame=%zu\n", thread, *len, ctx->frames);
	return ret;
}

ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads)
{
	ZSTDMT_DCtx *ctx;
	int t;

	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx)
		die_nomem();

	ctx->buffer_in = malloc(ZBUFF_recommendedDInSize() * threads);
	if (!ctx->buffer_in)
		die_nomem();

	ctx->buffer_out = malloc(ZBUFF_recommendedDOutSize() * threads);
	if (!ctx->buffer_out)
		die_nomem();

	ctx->threads = threads;

	ctx->dctx = malloc(sizeof(void *) * threads);
	if (!ctx->dctx)
		die_nomem();

	ctx->clen = (size_t *)malloc(sizeof(size_t) * threads);
	if (!ctx->clen)
		die_nomem();

	ctx->cbuf = malloc(sizeof(void *) * threads);
	if (!ctx->cbuf)
		die_nomem();

	ctx->dlen = (size_t *)malloc(sizeof(size_t) * threads);
	if (!ctx->dlen)
		die_nomem();

	ctx->dbuf = malloc(sizeof(void *) * threads);
	if (!ctx->dbuf)
		die_nomem();

	for (t = 0; t < threads; t++) {
		ctx->dctx[t] = ZBUFF_createDCtx();
		if (!ctx->dctx[t])
			ZBUFF_decompressInit(ctx->dctx[t]);

		/* source data (compressed) */
		ctx->clen[t] = ZBUFF_recommendedDInSize();
		ctx->cbuf[t] = ctx->buffer_out + ctx->dlen[t] * t;

		/* destination data (decompressed) */
		ctx->dlen[t] = ZBUFF_recommendedDOutSize();
		ctx->dbuf[t] = ctx->buffer_out + ctx->dlen[t] * t;
	}

	return ctx;
}
