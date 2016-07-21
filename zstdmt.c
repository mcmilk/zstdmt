
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

#include <stdio.h>

#include "zstdmt.h"

static void die_nomem();

static void die_nomem()
{
	exit(1);
}

ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int streams, int level)
{
	ZSTDMT_CCtx *ctx;
	int t;

	ctx = (ZSTDMT_CCtx *) malloc(sizeof(ZSTDMT_CCtx));
	if (!ctx)
		die_nomem();

	/* constant sizes */
	ctx->buffer_in = malloc(ctx->buffer_in_len);
	ctx->streams = streams;
	ctx->threads = threads;
	ctx->level = level;
	ctx->buffer_in_len = ZBUFF_recommendedCInSize() * threads;
	ctx->buffer_out_len = ZBUFF_recommendedCOutSize() * threads;

	/* buffers */
	ctx->buffer_in = malloc(ctx->buffer_in_len);
	if (!ctx->buffer_in)
		die_nomem();
	ctx->buffer_out = malloc(ctx->buffer_out_len);
	if (!ctx->buffer_out)
		die_nomem();

	/* arrays */
	ctx->cctx = malloc(sizeof(void *) * threads);
	if (!ctx->cctx)
		die_nomem();
	ctx->cbuf = malloc(sizeof(void *) * threads);
	if (!ctx->cbuf)
		die_nomem();
	ctx->dbuf = malloc(sizeof(void *) * threads);
	if (!ctx->dbuf)
		die_nomem();

	/* these are dynamic */
	ctx->dlen = (int *)malloc(sizeof(int) * threads);
	if (!ctx->dlen)
		die_nomem();
	ctx->clen = (int *)malloc(sizeof(int) * threads);
	if (!ctx->clen)
		die_nomem();

	for (t = 0; t < threads; t++) {
		ctx->cctx[t] = ZBUFF_createCCtx();
		if (!ctx->cctx[t])
			ZBUFF_compressInit(ctx->cctx[t], ctx->level);

		/* destination data (compressed), start for threads */
		ctx->cbuf[t] = ctx->buffer_in + ctx->clen[t] * t;

		/* source data (uncompressed), start for threads */
		ctx->dbuf[t] = ctx->buffer_out + ctx->dlen[t] * t;
	}

	return ctx;
}

/* returns the length of the input buffer */
int ZSTDMT_GetInSizeCCtx(ZSTDMT_CCtx * ctx)
{
	return ctx->buffer_in_len;
}

/* returns pointer to input buffer, should be used for reading data */
void *ZSTDMT_GetInBufferCCtx(ZSTDMT_CCtx * ctx)
{
	return ctx->buffer_in;
}

void ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, int insize)
{
	/* XXX threading here ... */
	int t;

	printf("ZSTDMT_CompressCCtx() insize=%d\n", insize);
	for (t = 0; t < ctx->threads; t++) {

	}

	return;
}

/* returns pointer to compressed output buffer of thread t with length len */
void *ZSTDMT_GetCompressedCCtx(ZSTDMT_CCtx * ctx, int thread, size_t * len)
{
	*len = ctx->clen[thread];
	return ctx->cbuf[thread];
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

	ctx->clen = (int *)malloc(sizeof(int) * threads);
	if (!ctx->clen)
		die_nomem();

	ctx->cbuf = malloc(sizeof(void *) * threads);
	if (!ctx->cbuf)
		die_nomem();

	ctx->dlen = (int *)malloc(sizeof(int) * threads);
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
