
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
#include <stdio.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

#include "mem.h"
#include "threading.h"
#include "list.h"
#include "zstdmt.h"
#include "error_private.h"

/**
 * multi threaded zstd - multiple workers version
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
size_t zstdmt_errcode;

/* worker for compression */
typedef struct {
	ZSTDMT_DCtx *ctx;
	pthread_t pthread;
	ZSTDMT_Buffer in;
	ZSTD_DStream *dctx;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	ZSTDMT_Buffer out;
	struct list_head node;
};

struct ZSTDMT_DCtx_s {

	/* threads: 1..ZSTDMT_THREAD_MAX */
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

ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads, int inputsize)
{
	ZSTDMT_DCtx *ctx;

	/* allocate ctx */
	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > ZSTDMT_THREAD_MAX)
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
		ctx->inputsize = 0;	/* auto detect */

	/* later */
	ctx->cwork = 0;

	return ctx;
}

/**
 * pt_write - queue for decompressed output
 */
static size_t pt_write(ZSTDMT_DCtx * ctx, struct writelist *wl)
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
				return ERROR(write_fail);
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
static size_t pt_read(ZSTDMT_DCtx * ctx, ZSTDMT_Buffer * in, size_t * frame)
{
	unsigned char hdrbuf[12];
	ZSTDMT_Buffer hdr;
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
		if (MEM_readLE32((unsigned char *)hdr.buf + 0) !=
		    ZSTDMT_MAGIC_SKIPPABLE)
			goto error_data;
	}

	/* check header data */
	if (MEM_readLE32((unsigned char *)hdr.buf + 4) != 4)
		goto error_data;

	ctx->insize += 12;
	/* read new inputsize */
	{
		size_t toRead = MEM_readLE32((unsigned char *)hdr.buf + 8);
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
	ZSTDMT_Buffer *in = &w->in;
	ZSTDMT_DCtx *ctx = w->ctx;
	size_t result = 0;
	struct writelist *wl;

	/* init dstream stream */
	result = ZSTD_initDStream(w->dctx);
	if (ZSTD_isError(result)) {
		zstdmt_errcode = result;
		return (void*)ERROR(compression_library);
	}

	for (;;) {
		struct list_head *entry;
		ZSTDMT_Buffer *out;
		ZSTD_inBuffer zIn;
		ZSTD_outBuffer zOut;

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
				result = ERROR(memory_allocation);
				goto error_unlock;
			}
			wl->out.buf = 0;
			wl->out.size = 0;
			wl->out.allocated = 0;
			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);
		out = &wl->out;

		/* zero should not happen here! */
		result = pt_read(ctx, in, &wl->frame);
		if (in->size == 0)
			break;
		if (ZSTDMT_isError(result)) {
			goto error_lock;
		}

		{
		ZSTD_frameParams fp;
		ZSTD_getFrameParams(&fp, in->buf, in->size);
		printf("frameContentSize=%llu ", fp.frameContentSize);
		fflush(stdout);
		out->size = 1024*1024*20;
		if (fp.frameContentSize != 0)
			out->size = fp.frameContentSize;
		}
		if (out->allocated < out->size) {
			if (out->allocated)
				out->buf = realloc(out->buf, out->size);
			else
				out->buf = malloc(out->size);
			if (!out->buf) {
				result = ERROR(memory_allocation);
				goto error_lock;
			}
			out->allocated = out->size;
		}

		zIn.size = in->allocated;
		zIn.src = in->buf;
		zIn.pos = 0;
		for (;;) {
			/* decompress loop */
			zOut.size = out->allocated;
			zOut.dst = out->buf;
			zOut.pos = 0;

			printf("in: in.size=%zu out.size=%zu\n", in->size, out->size);
			result = ZSTD_decompressStream(w->dctx, &zOut, &zIn);
			printf("out: ret=%zu in.size=%zu out.size=%zu\n",
			result, in->size, out->size);
			if (ZSTD_isError(result)) {
				goto error_clib;
			}

			if (zOut.pos) {

				/* write result */
				pthread_mutex_lock(&ctx->write_mutex);
				result = pt_write(ctx, wl);
				if (ERR_isError(result))
					goto error_unlock;
				pthread_mutex_unlock(&ctx->write_mutex);
			}

			/* one more round */
			if ((zIn.pos == zIn.size) && (result == 1)
			    && zOut.pos)
				continue;

			/* end of frame */
			if (result == 0) {
				printf("ZSTD_initDStream()...\n");
				fflush(stdout);
				result = ZSTD_initDStream(w->dctx);
				if (ZSTD_isError(result))
					goto error_clib;
			}

			/* finished */
			if (zIn.pos == zIn.size)
				break;

		}		/* decompress */

	}

	/* everything is okay */
	pthread_mutex_lock(&ctx->write_mutex);
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
	return 0;

 error_clib:
	zstdmt_errcode = result;
	result = ERROR(compression_library);
	/* fall through */
 error_lock:
	pthread_mutex_lock(&ctx->write_mutex);
 error_unlock:
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
	return (void *)result;
}

/* single threaded */
static size_t st_decompress(void *arg)
{
	ZSTDMT_DCtx *ctx = (ZSTDMT_DCtx *) arg;
	cwork_t *w = &ctx->cwork[0];
	ZSTDMT_Buffer Out;
	ZSTDMT_Buffer *in = &w->in;
	ZSTDMT_Buffer *out = &Out;
	void *magic = in->buf;
	size_t result;
	int rv;

	ZSTD_inBuffer zIn;
	ZSTD_outBuffer zOut;

	/* init dstream stream */
	result = ZSTD_initDStream(w->dctx);
	if (ZSTD_isError(result)) {
		zstdmt_errcode = result;
		return ERROR(compression_library);
	}

	/* allocate space for input buffer */
	in->size = ZSTD_DStreamInSize();
	in->buf = malloc(in->size);
	if (!in->buf)
		return ERROR(memory_allocation);
	in->allocated = in->size;

	/* allocate space for output buffer */
	out->size = ZSTD_DStreamOutSize();
	out->buf = malloc(out->size);
	if (!out->buf) {
		free(in->buf);
		return ERROR(memory_allocation);
	}
	out->allocated = out->size;

	/* 4 bytes + some more ... */
	memcpy(in->buf, magic, 4);
	in->buf += 4;
	in->size = in->allocated - 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv == -1) {
		result = ERROR(read_fail);
		goto error;
	}

	if (in->size == 0) {
		result = ERROR(data_error);
		goto error;
	}
	in->buf -= 4;
	in->size += 4;
	ctx->insize += in->size;

	zIn.src = in->buf;
	zIn.size = in->size;
	zIn.pos = 0;

	zOut.dst = out->buf;

	for (;;) {
		for (;;) {
			/* decompress loop */
			zOut.size = out->allocated;
			zOut.pos = 0;

			result = ZSTD_decompressStream(w->dctx, &zOut, &zIn);
			if (ZSTD_isError(result))
				goto error_clib;

			if (zOut.pos) {
				ZSTDMT_Buffer w;
				w.size = zOut.pos;
				w.buf = zOut.dst;
				rv = ctx->fn_write(ctx->arg_write, &w);
				ctx->outsize += zOut.pos;
			}

			/* one more round */
			if ((zIn.pos == zIn.size) && (result == 1)
			    && zOut.pos)
				continue;

			/* finished */
			if (zIn.pos == zIn.size)
				break;

			/* end of frame */
			if (result == 0) {
				result = ZSTD_initDStream(w->dctx);
				if (ZSTD_isError(result))
					goto error_clib;
			}
		}		/* decompress */

		/* read next input */
		in->size = in->allocated;
		rv = ctx->fn_read(ctx->arg_read, in);
		if (rv == -1) {
			result = ERROR(read_fail);
			goto error;
		}

		if (in->size == 0)
			goto okay;
		ctx->insize += in->size;

		zIn.size = in->size;
		zIn.pos = 0;
	}			/* read */

 error_clib:
	zstdmt_errcode = result;
	/* fall through */
 error:
	/* return with error */
	free(out->buf);
	free(in->buf);
	return result;
 okay:
	/* no error */
	free(out->buf);
	free(in->buf);
	return 0;
}

size_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, ZSTDMT_RdWr_t * rdwr)
{
	unsigned char buf[4];
	ZSTDMT_Buffer In;
	ZSTDMT_Buffer *in = &In;
	cwork_t *w;
	int t, rv;

	if (!ctx)
		return ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for ZSTDMT_MAGIC_SKIPPABLE */
	in->buf = buf;
	in->size = 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv == -1)
		return ERROR(read_fail);
	if (in->size != 4)
		return ERROR(data_error);

	/* single threaded with unknown sizes */
	if (MEM_readLE32(buf) != ZSTDMT_MAGIC_SKIPPABLE) {
		U32 magic = MEM_readLE32(buf);

		/* look for usable magic */
		if (magic < ZSTDMT_MAGICNUMBER_MIN)
			return ERROR(data_error);
		else if (magic > ZSTDMT_MAGICNUMBER_MAX)
			return ERROR(data_error);

		ctx->threads = 1;
		ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t));
		if (!ctx->cwork)
			return ERROR(memory_allocation);
		w = &ctx->cwork[0];
		w->in.buf = in->buf;
		w->in.size = in->size;
		w->ctx = ctx;
		w->dctx = ZSTD_createDStream();
		if (!w->dctx)
			return ERROR(memory_allocation);

		/* decompress single threaded */
		return st_decompress(ctx);
	}

	printf("multistream\n");
	fflush(stdout);

	/* setup thread work */
	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * ctx->threads);
	if (!ctx->cwork)
		return ERROR(memory_allocation);

	for (t = 0; t < ctx->threads; t++) {
		w = &ctx->cwork[t];
		w->ctx = ctx;
		w->dctx = ZSTD_createDStream();
		if (!w->dctx)
			return ERROR(memory_allocation);
	}

	/* single threaded, but with known sizes */
	if (ctx->threads == 1) {
		/* no pthread_create() needed! */
		w = &ctx->cwork[0];
		w->in.buf = in->buf;
		w->in.size = in->size;
		/* test, if pt_decompress is better... */
		return st_decompress(ctx);
	}

	/* real multi threaded, init pthread's */
	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	INIT_LIST_HEAD(&ctx->writelist_free);
	INIT_LIST_HEAD(&ctx->writelist_busy);
	INIT_LIST_HEAD(&ctx->writelist_done);

	/* multi threaded */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->in.buf = 0;
		w->in.size = 0;
		w->in.allocated = 0;
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

	/* clean up pthread stuff */
	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);

	/* clean up the buffers */
	while (!list_empty(&ctx->writelist_free)) {
		struct writelist *wl;
		struct list_head *entry;
		entry = list_first(&ctx->writelist_free);
		wl = list_entry(entry, struct writelist, node);
		free(wl->out.buf);
		list_del(&wl->node);
		free(wl);
	}

	return 0;
}

/* returns current uncompressed data size */
size_t ZSTDMT_GetInsizeDCtx(ZSTDMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t ZSTDMT_GetOutsizeDCtx(ZSTDMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t ZSTDMT_GetFramesDCtx(ZSTDMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx)
{
	int t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		ZSTD_freeDStream(w->dctx);
	}

	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
