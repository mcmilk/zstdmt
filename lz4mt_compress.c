
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
	LZ4MT_CCtx *ctx;
	LZ4F_preferences_t zpref;
	pthread_t pthread;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	LZ4MT_Buffer out;
	struct list_head node;
};

struct LZ4MT_CCtx_s {

	/* level: 1..22 */
	int level;

	/* threads: 1..LZ4MT_THREAD_MAX */
	int threads;

	/* should be used for read from input */
	int inputsize;

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
 * Compression
 ****************************************/

LZ4MT_CCtx *LZ4MT_createCCtx(int threads, int level, int inputsize)
{
	LZ4MT_CCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZ4MT_CCtx *) malloc(sizeof(LZ4MT_CCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZ4MT_THREAD_MAX)
		return 0;

	/* check level */
	if (level < 1 || level > 9)
		return 0;

	/* calculate chunksize for one thread */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = 64 * 1024;

	/* setup ctx */
	ctx->level = level;
	ctx->threads = threads;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);
	INIT_LIST_HEAD(&ctx->writelist);

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;

		/* setup preferences for that thread */
		w->zpref.compressionLevel = level;
		w->zpref.frameInfo.blockMode = LZ4F_blockLinked;
		w->zpref.frameInfo.contentChecksumFlag =
		    LZ4F_contentChecksumEnabled;

	}

	return ctx;

 err_cwork:
	free(ctx);

	return 0;
}

/**
 * pt_write - queue for compressed output
 */
static int pt_write(LZ4MT_CCtx * ctx, size_t frame, LZ4MT_Buffer * out)
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

	/* check, what can be written ... */
 again:
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

static void *pt_compress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZ4MT_CCtx *ctx = w->ctx;
	int result, len;

	LZ4MT_Buffer in;
	LZ4MT_Buffer out;

	/* used for error message */
	out.buf = 0;

	/* inbuf is constant */
	in.size = ctx->inputsize;
	in.buf = malloc(in.size);
	if (!in.buf)
		goto error;

	for (;;) {
		size_t frame;

		/* allocate space for new output */
		out.size =
		    LZ4F_compressFrameBound(ctx->inputsize, &w->zpref) + 12;
		out.buf = malloc(out.size);
		if (!out.buf)
			goto error;

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		ctx->fn_read(ctx->arg_read, &in);
		len = in.size;
		in.size = ctx->inputsize;
		if (len == -1) {
			out.buf = "Error while reading input!";
			pthread_mutex_unlock(&ctx->read_mutex);
			goto error;
		}
		if (len == 0) {
			free(in.buf);
			free(out.buf);
			pthread_mutex_unlock(&ctx->read_mutex);
			return 0;
		}
		ctx->insize += len;
		frame = ctx->frames++;
		pthread_mutex_unlock(&ctx->read_mutex);

		/* compress whole frame */
		result =
		    LZ4F_compressFrame(out.buf + 12, out.size - 12, in.buf, len,
				       &w->zpref);
		if (LZ4F_isError(result)) {
			out.buf = (void *)LZ4F_getErrorName(result);
			goto error;
		}

		/* write skippable frame */
		write_le32(out.buf + 0, 0x184D2A50);
		write_le32(out.buf + 4, 4);
		write_le32(out.buf + 8, result);
		out.size = result + 12;

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, frame, &out);
		pthread_mutex_unlock(&ctx->write_mutex);
		if (result == -1) {
			out.buf = "Error while writing output!";
			goto error;
		}
	}

	return 0;
 error:
#if 1
	if (!out.buf)
		out.buf = "Allocation error : not enough memory";
	printf("ERR @ pt_compress() = %s\n", (char *)out.buf);
	fflush(stdout);
#endif
	out.size = 0;
	return (void *)-1;
}

int LZ4MT_CompressCCtx(LZ4MT_CCtx * ctx, LZ4MT_RdWr_t * rdwr)
{
	int t;

	if (!ctx)
		return -1;

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* start all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		pthread_create(&w->pthread, NULL, pt_compress, w);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		void *p;
		pthread_join(w->pthread, &p);
		if (p)
			return -1;
	}

	return 0;
}

/* returns current uncompressed data size */
size_t LZ4MT_GetInsizeCCtx(LZ4MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZ4MT_GetOutsizeCCtx(LZ4MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZ4MT_GetFramesCCtx(LZ4MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZ4MT_freeCCtx(LZ4MT_CCtx * ctx)
{
	if (!ctx)
		return;

	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
