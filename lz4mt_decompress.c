
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
#include "error_private.h"

#include <stdio.h>

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

/* will be used for lib errors */
size_t lz4mt_errcode;

/* worker for compression */
typedef struct {
	LZ4MT_DCtx *ctx;
	pthread_t pthread;
	LZ4MT_Buffer in;
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

	/* lists for writing queue */
	struct list_head writelist_free;
	struct list_head writelist_busy;
	struct list_head writelist_done;
};

/* **************************************
 * Decompression
 ****************************************/

/* could be replaced by MEM_readLE32() */
static unsigned int read_le32(const unsigned char *x)
{
	return (unsigned int)(x[0]) | (((unsigned int)(x[1])) << 8) |
	    (((unsigned int)(x[2])) << 16) | (((unsigned int)(x[3])) << 24);
}

static unsigned long long int read_le64(const unsigned char *x)
{
	return (unsigned long long int)(x[0])
	    | (((unsigned long long int)(x[1])) << 8)
	    | (((unsigned long long int)(x[2])) << 16)
	    | (((unsigned long long int)(x[3])) << 24)
	    | (((unsigned long long int)(x[4])) << 32)
	    | (((unsigned long long int)(x[5])) << 40)
	    | (((unsigned long long int)(x[6])) << 48)
	    | (((unsigned long long int)(x[7])) << 56);
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

	INIT_LIST_HEAD(&ctx->writelist_free);
	INIT_LIST_HEAD(&ctx->writelist_busy);
	INIT_LIST_HEAD(&ctx->writelist_done);

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
static size_t pt_write(LZ4MT_DCtx * ctx, struct writelist *wl)
{
	struct list_head *entry;

	/* move the entry to the done list */
	list_move(&wl->node, &ctx->writelist_done);
 again:
	/* check, what can be written ... */
	list_for_each(entry, &ctx->writelist_done) {
		wl = list_entry(entry, struct writelist, node);
		if (wl->frame == ctx->curframe) {
			int rv = ctx->fn_write(ctx->arg_write, &wl->out);
			if (rv == -1)
				return -1;
			ctx->outsize += wl->out.size;
			ctx->curframe++;
			list_move(entry, &ctx->writelist_free);
			goto again;
		}
	}

	return 0;
}

/**
 * pt_read - read compressed output
 */
static size_t pt_read(LZ4MT_DCtx * ctx, LZ4MT_Buffer * in, size_t * frame)
{
	unsigned char hdrbuf[12];
	LZ4MT_Buffer hdr;
	int rv;

	/* read skippable frame (8 or 12 bytes) */
	pthread_mutex_lock(&ctx->read_mutex);

	/* special case, first 4 bytes already read */
	if (ctx->frames == 0) {
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
		/* eof reached ? */
		if (rv == 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			in->size = 0;
			return 0;
		}
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

	ctx->insize += 12;
	/* read new inputsize */
	{
		size_t toRead = read_le32(hdr.buf + 8);
		printf("LZ4F_decompress(vor) toRead=%zu\n", toRead);
		if (in->allocated < toRead) {
			/* need bigger input buffer */
			if (in->allocated)
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

 error_data:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(data_error);
 error_read:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(read_fail);
 error_nomem:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(memory_allocation);
}

static void *pt_decompress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZ4MT_Buffer *in = &w->in;
	LZ4MT_DCtx *ctx = w->ctx;
	size_t result = 0;

	for (;;) {
		struct list_head *entry;
		struct writelist *wl;
		LZ4MT_Buffer *out;
		int rv;

		/* allocate space for new output */
		pthread_mutex_lock(&ctx->write_mutex);
		if (!list_empty(&ctx->writelist_free)) {
			/* take unused entry */
			entry = list_first(&ctx->writelist_free);
			wl = list_entry(entry, struct writelist, node);
			list_move(entry, &ctx->writelist_busy);
		} else {
			/* allocate new one */
			wl = (struct writelist *)
			    malloc(sizeof(struct writelist));
			if (!wl) {
				pthread_mutex_unlock(&ctx->write_mutex);
				return (void *)ERROR(memory_allocation);
			}

			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);
		out = &wl->out;

		/* zero should not happen here! */
		result = pt_read(ctx, in, &wl->frame);
		if (in->size == 0) {
			list_move(&wl->node, &ctx->writelist_free);
			goto done;
		}

		if (LZ4MT_isError(result)) {
			list_move(&wl->node, &ctx->writelist_free);
			return (void *)result;
		}

		{
			/* get frame size for output buffer */
			unsigned char *src = in->buf + 6;
			out->size = (size_t) read_le64(src);
		}

		if (out->allocated < out->size) {
			if (out->allocated)
				out->buf = realloc(out->buf, out->size);
			else
				out->buf = malloc(out->size);
			if (!out->buf) {
				list_move(&wl->node, &ctx->writelist_free);
				return (void *)ERROR(memory_allocation);
			}
			out->allocated = out->size;
		}

		printf("LZ4F_decompress(vor) ret=%zu, in=%zu out=%zu\n", result,
		       in->size, out->size);

		result =
		    LZ4F_decompress(w->dctx, out->buf, &out->size,
				    in->buf, &in->size, 0);

		printf("LZ4F_decompress(ret) ret=%zu, in=%zu out=%zu\n", result,
		       in->size, out->size);

		if (LZ4F_isError(result)) {
			lz4mt_errcode = result;
			list_move(&wl->node, &ctx->writelist_free);
			return (void *)ERROR(compression_library);
		}

		if (result != 0) {
			list_move(&wl->node, &ctx->writelist_free);
			return (void *)ERROR(frame_decompress);
		}

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		rv = pt_write(ctx, wl);
		pthread_mutex_unlock(&ctx->write_mutex);
		if (rv == -1) {
			list_move(&wl->node, &ctx->writelist_free);
			return (void *)ERROR(write_fail);
		}
	}

 done:
	return 0;
}

/* single threaded */
static size_t st_decompress(void *arg)
{
	LZ4MT_DCtx *ctx = (LZ4MT_DCtx *) arg;
	LZ4F_errorCode_t nextToLoad = 0;
	cwork_t *w = &ctx->cwork[0];
	LZ4MT_Buffer Out;
	LZ4MT_Buffer *out = &Out;
	LZ4MT_Buffer *in = &w->in;
	void *magic = in->buf;
	size_t pos = 0;
	int rv;

	/* allocate space for input buffer */
	in->size = ctx->inputsize;
	in->buf = malloc(in->size);
	if (!in->buf)
		return ERROR(memory_allocation);

	/* allocate space for output buffer */
	out->size = ctx->inputsize;
	out->buf = malloc(out->size);
	if (!out->buf) {
		free(in->buf);
		return ERROR(memory_allocation);
	}

	/* we have read already 4 bytes */
	in->size = 4;
	memcpy(in->buf, magic, in->size);

	nextToLoad =
	    LZ4F_decompress(w->dctx, out->buf, &pos, in->buf, &in->size, 0);
	if (LZ4F_isError(nextToLoad)) {
		free(in->buf);
		free(out->buf);
		return ERROR(compression_library);
	}

	for (; nextToLoad; pos = 0) {
		if (nextToLoad > ctx->inputsize)
			nextToLoad = ctx->inputsize;

		/* read new input */
		in->size = nextToLoad;
		rv = ctx->fn_read(ctx->arg_read, in);
		if (rv == -1) {
			free(in->buf);
			free(out->buf);
			return ERROR(read_fail);
		}

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
			if (LZ4F_isError(nextToLoad)) {
				free(in->buf);
				free(out->buf);
				return ERROR(compression_library);
			}

			/* have some output */
			if (out->size) {
				rv = ctx->fn_write(ctx->arg_write, out);
				if (rv == -1) {
					free(in->buf);
					free(out->buf);
					return ERROR(write_fail);
				}
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
}

size_t LZ4MT_DecompressDCtx(LZ4MT_DCtx * ctx, LZ4MT_RdWr_t * rdwr)
{
	unsigned char buf[4];
	int t, rv;
	cwork_t *w = &ctx->cwork[0];
	LZ4MT_Buffer *in = &w->in;

	if (!ctx)
		return ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for 0x184D2A50 (multithreading stream) */
	in->buf = buf;
	in->size = 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv == -1)
		return ERROR(read_fail);
	if (in->size != 4)
		return ERROR(data_error);

	/* single threaded with unknown sizes */
	if (read_le32(buf) != 0x184D2A50) {

		/* look for correct magic */
		if (read_le32(buf) != 0x184D2204)
			return ERROR(data_error);

		/* decompress single threaded */
		return st_decompress(ctx);
	}

	in->buf = 0;
	in->size = 0;

	/* single threaded, but with known sizes */
	if (ctx->threads == 1) {
		/* no pthread_create() needed! */
		void *p = pt_decompress(w);
		if (p)
			return (size_t) p;
		goto okay;
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
		if (p)
			return (size_t) p;
	}

 okay:
	/* clean up lists */
	{
		struct list_head *entry;
		struct writelist *wl;
		list_for_each(entry, &ctx->writelist_free) {
			wl = list_entry(entry, struct writelist, node);
			free(wl->out.buf);
			free(wl);
		}
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
