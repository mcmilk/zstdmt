
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

#include <stdlib.h>
#include <string.h>

#define LZ4F_DISABLE_OBSOLETE_ENUMS
#include <lz4frame.h>

#include "threading.h"
#include "list.h"
#include "lz4mt.h"

/**
 * multi threaded lz4 - multiple workers version
 *
 * - each thread works on his own
 * - no main thread which does reading and then starting the work
 * - needs a callback for reading / writing
 * - each worker does his:
 *   1) get read mutex and read some input
 *   2) release read mutex and do compression
 *   3) get write mutex and write result
 *   4) begin with step 1 again, until no input
 */

#define DEBUGME
#ifdef DEBUGME
#include <stdio.h>
#endif

/* could be replaced by MEM_writeLE32() */
static inline void write_le32(unsigned char *dst, unsigned int u)
{
	dst[0] = u;
	u >>= 8;
	dst[1] = u;
	u >>= 8;
	dst[2] = u;
	u >>= 8;
	dst[3] = u;
}

/* worker for compression */
typedef struct {
	LZ4MT_DCtx *ctx;
	pthread_t pthread;
	LZ4MT_Buffer in;
	LZ4MT_Buffer out;
	LZ4F_decompressionContext_t dctx;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	LZ4MT_Buffer out;
	struct list_head node;
};

struct LZ4MT_DCtx_s {

	/* threads: 1..LZ4MT_THREAD_MAX */
	int threads;

	/* should be used for read from input */
	size_t inputsize;
	int read8only;

	/* statistic */
	size_t insize;
	size_t outsize;
	size_t curframe;
	size_t frames;

	/* threading */
	cwork_t *cwork;

	/* reading input */
	pthread_mutex_t read_mutex;
	fn_read *fn_read;
	void *arg_read;

	/* writing output */
	pthread_mutex_t write_mutex;
	fn_write *fn_write;
	void *arg_write;
	struct list_head writelist;
};

/* **************************************
 * Decompression
 ****************************************/

/* could be replaced by MEM_writeLE32() */
static unsigned int read_le32(const unsigned char *x)
{
	return (unsigned int)(x[0]) | (((unsigned int)(x[1])) << 8) |
	    (((unsigned int)(x[2])) << 16) | (((unsigned int)(x[3])) << 24);
}

LZ4MT_DCtx *LZ4MT_createDCtx(int threads, int inputsize)
{
	LZ4MT_DCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZ4MT_DCtx *) malloc(sizeof(LZ4MT_DCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZ4MT_THREAD_MAX)
		return 0;

	/* setup ctx */
	ctx->threads = threads;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	/* will be used for single stream only */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = 1024 * 64;	/* 64K buffer */

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);
	INIT_LIST_HEAD(&ctx->writelist);

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;

		/* setup thread work */
		LZ4F_createDecompressionContext(&w->dctx, LZ4F_VERSION);
	}

	return ctx;

 err_cwork:
	free(ctx);

	return 0;
}

/**
 * pt_write - queue for decompressed output
 */
static int pt_write(LZ4MT_DCtx * ctx, size_t frame, LZ4MT_Buffer * out)
{
	struct writelist *wl =
	    (struct writelist *)malloc(sizeof(struct writelist));
	struct list_head *entry;

	if (!wl)
		return -1;

	wl->frame = frame;
	wl->out.buf = out->buf;
	wl->out.size = out->size;
	list_add(&wl->node, &ctx->writelist);

 again:
	/* check, what can be written ... */
	list_for_each(entry, &ctx->writelist) {
		wl = list_entry(entry, struct writelist, node);
		if (wl->frame == ctx->curframe) {
			ctx->fn_write(ctx->arg_write, &wl->out);
			ctx->outsize += wl->out.size;
			ctx->curframe++;
			list_del(entry);
			free(wl->out.buf);
			free(wl);
			goto again;
		}
	}

	return 0;
}

/**
 * pt_read - read compressed output
 */
static int pt_read(LZ4MT_DCtx * ctx, LZ4MT_Buffer * in, size_t * frame)
{
	unsigned char hdrbuf[12];
	LZ4MT_Buffer hdr;
	int rv;

	/* read skippable frame (8 or 12 bytes) */
	pthread_mutex_lock(&ctx->read_mutex);

	/* special case, first 4 bytes already read */
	if (ctx->read8only) {
		ctx->read8only = 0;
		hdr.buf = hdrbuf + 4;
		hdr.size = 8;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv == -1)
			goto error_read;
		if (hdr.size != 8)
			goto error_read;
		hdr.buf = hdrbuf;
	} else {
		hdr.buf = hdrbuf;
		hdr.size = 12;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv == 0)
			return 0;
		if (rv == -1)
			goto error_read;
		if (hdr.size != 12)
			goto error_read;
		if (read_le32(hdr.buf + 0) != 0x184D2A50)
			goto error_data;
	}

	/* check header data */
	if (read_le32(hdr.buf + 4) != 4)
		goto error_data;

	/* read new inputsize */
	{
		size_t toRead = read_le32(hdr.buf + 8);
		if (in->allocated < toRead) {
			/* need bigger input buffer */
			if (in->buf)
				in->buf = realloc(in->buf, toRead);
			else
				in->buf = malloc(toRead);
			if (!in->buf)
				goto error_nomem;
			in->allocated = toRead;
		}

		in->size = toRead;
		rv = ctx->fn_read(ctx->arg_read, in);
		/* generic read failure! */
		if (rv == -1)
			goto error_read;
		/* needed more bytes! */
		if (in->size != toRead)
			goto error_data;

		ctx->insize += in->size;
	}
	*frame = ctx->frames++;
	pthread_mutex_unlock(&ctx->read_mutex);

	/* done, no error */
	return in->size;

	/* xxx, add errno meaning */
 error_data:
	printf("pt_read err data (size=%zu)\n", in->size);
	fflush(stdout);
	pthread_mutex_unlock(&ctx->read_mutex);
	return -1;
 error_read:
	printf("pt_read error read (size=%zu)\n", in->size);
	fflush(stdout);
	pthread_mutex_unlock(&ctx->read_mutex);
	return -1;
 error_nomem:
	printf("pt_read nomem\n");
	fflush(stdout);
	pthread_mutex_unlock(&ctx->read_mutex);
	return -1;
}

static void *pt_decompress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZ4MT_Buffer *out = &w->out;
	LZ4MT_Buffer *in = &w->in;
	LZ4MT_DCtx *ctx = w->ctx;
	size_t result = 0;
	char *errmsg = 0;
	int rv;

	for (;;) {
		size_t frame = 0;
		size_t done = 0;

		/* zero should not happen here! */
		rv = pt_read(ctx, in, &frame);
		printf("pt_read() in->size=%zu frame=%zu\n", in->size, frame);
		fflush(stdout);
		if (rv == 0)
			goto done;
		if (rv < 0)
			goto error_read;

		{
			unsigned char *src = in->buf;
			unsigned char bd = src[5];
			printf("bd: %d \n", bd >> 4);

			out->size = 0;
			switch ((bd >> 4) & 0x07) {
			case LZ4F_max64KB:
				out->size = 1024 * 64;
				break;
			case LZ4F_max256KB:
				out->size = 1024 * 256;
				break;
			case LZ4F_max1MB:
				out->size = 1024 * 1024;
				break;
			case LZ4F_max4MB:
				out->size = 1024 * 1024 * 4;
				break;
			}
		}

		/* allocate space for output */
		out->buf = malloc(out->size);
		if (!out->buf)
			goto error_nomem;

		while (done != out->size) {

			result =
			    LZ4F_decompress(w->dctx, out->buf, &out->size,
					    in->buf, &in->size, 0);
			printf("in->size=%zu out->size=%zu result=%zu\n",
			       in->size, out->size, result);
			fflush(stdout);
			if (LZ4F_isError(result))
				goto error_lz4f;

			done += out->size;

			/* write result */
			pthread_mutex_lock(&ctx->write_mutex);
			rv = pt_write(ctx, frame, out);
			pthread_mutex_unlock(&ctx->write_mutex);
			if (rv == -1) {
				goto error_write;
			}
		}

	}

 done:
	if (in->allocated)
		free(in->buf);
	if (out->allocated)
		free(out->buf);
	printf("done()\n");
	fflush(stdout);

	return 0;

 error_lz4f:
	errmsg = (char *)LZ4F_getErrorName(result);
 error_read:
	if (!errmsg)
		errmsg = "Error while reading input!";
 error_write:
	if (!errmsg)
		errmsg = "Error while writing output!";
 error_nomem:
	if (!errmsg)
		errmsg = "No memory!";
	out->buf = errmsg;
	out->size = 0;
	printf("error: %s\n", errmsg);
	fflush(stdout);
	return (void *)-1;
}

/* single threaded */
static int st_decompress(void *arg)
{
	LZ4MT_DCtx *ctx = (LZ4MT_DCtx *) arg;
	LZ4F_errorCode_t nextToLoad = 0;
	cwork_t *w = &ctx->cwork[0];
	LZ4MT_Buffer *out = &w->out;
	LZ4MT_Buffer *in = &w->in;
	void *magic = in->buf;
	char *errmsg = 0;
	size_t pos = 0;
	int rv;

	/* allocate space for input buffer */
	in->size = ctx->inputsize;
	in->buf = malloc(in->size);
	if (!in->buf)
		goto error_nomem;

	/* allocate space for output buffer */
	out->size = ctx->inputsize;
	out->buf = malloc(out->size);
	if (!out->buf)
		goto error_nomem;

	/* we have read already 4 bytes */
	in->size = 4;
	memcpy(in->buf, magic, in->size);
	nextToLoad =
	    LZ4F_decompress(w->dctx, out->buf, &pos, in->buf, &in->size, 0);
	if (LZ4F_isError(nextToLoad))
		goto error_lz4f;

	for (; nextToLoad; pos = 0) {
		if (nextToLoad > ctx->inputsize)
			nextToLoad = ctx->inputsize;

		/* read new input */
		in->size = nextToLoad;
		rv = ctx->fn_read(ctx->arg_read, in);
		if (rv == -1)
			goto error_read;

		/* done, eof reached */
		if (in->size == 0)
			break;

		/* still to read, or still to flush */
		while ((pos < in->size) || (out->size == ctx->inputsize)) {
			size_t remaining = in->size - pos;
			out->size = ctx->inputsize;

			/* decompress */
			nextToLoad =
			    LZ4F_decompress(w->dctx, out->buf, &out->size,
					    in->buf + pos, &remaining, NULL);
			if (LZ4F_isError(nextToLoad))
				goto error_lz4f;

			/* have some output */
			if (out->size) {
				rv = ctx->fn_write(ctx->arg_write, out);
				if (rv == -1)
					goto error_write;
			}

			if (nextToLoad == 0)
				break;

			pos += remaining;
		}
	}

	/* no error */
	free(out->buf);
	free(in->buf);
	return 0;

 error_lz4f:
	errmsg = (char *)LZ4F_getErrorName(nextToLoad);
 error_read:
	if (!errmsg)
		errmsg = "Error while reading input!";
 error_write:
	if (!errmsg)
		errmsg = "Error while writing output!";
 error_nomem:
	if (!errmsg)
		errmsg = "No memory!";
	if (in->buf)
		free(in->buf);
	if (out->buf)
		free(out->buf);
	out->buf = errmsg;
	out->size = 0;
#if 0
	printf("err = %s\n", (char *)out->buf);
	fflush(stdout);
#endif
	return -1;
}

int LZ4MT_DecompressDCtx(LZ4MT_DCtx * ctx, LZ4MT_RdWr_t * rdwr)
{
	unsigned char buf[4];
	int t, rv;
	cwork_t *w = &ctx->cwork[0];
	LZ4MT_Buffer *in = &w->in;

	if (!ctx)
		return -1;

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for 0x184D2A50 (multithreading stream) */
	/* 0x184D2204 = lz4 0x184D2205 = lz5 */
	in->buf = buf;
	in->size = 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv == -1)
		return -1;
	if (in->size != 4)
		return -1;

	/* single threaded with unknown sizes */
	if (read_le32(buf) != 0x184D2A50) {

		/* look for correct magic */
		if (read_le32(buf) != 0x184D2204)
			return -1;

		/* decompress single threaded */
		return st_decompress(ctx);
	}

	in->buf = 0;
	in->size = 0;
	ctx->read8only = 1;

	/* single threaded, but with known sizes */
	if (ctx->threads == 1) {
		/* no pthread_create() needed! */
		void *p = pt_decompress(w);
		if (p)
			return -1;
		return 0;
	}

	/* multi threaded */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		pthread_create(&w->pthread, NULL, pt_decompress, w);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		void *p;
		pthread_join(w->pthread, &p);
		printf("pthread_join()\n");
		fflush(stdout);
		if (p)
			return -1;
	}

	return 0;
}

/* returns current uncompressed data size */
size_t LZ4MT_GetInsizeDCtx(LZ4MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZ4MT_GetOutsizeDCtx(LZ4MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZ4MT_GetFramesDCtx(LZ4MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZ4MT_freeDCtx(LZ4MT_DCtx * ctx)
{
	int t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		LZ4F_freeDecompressionContext(w->dctx);
	}
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
