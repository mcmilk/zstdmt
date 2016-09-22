
/**
 * Copyright © 2016 Tino Reichardt
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

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

#include "threading.h"
#include "list.h"
#include "zstdmt.h"

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

#define DEBUGME
#ifdef DEBUGME
#include <stdio.h>
#endif

/* could be replaced by MEM_writeLE32() */
static inline void write_le32(unsigned char *dst, unsigned int u)
{
	dst[0] = (unsigned char)u;
	u >>= 8;
	dst[1] = (unsigned char)u;
	u >>= 8;
	dst[2] = (unsigned char)u;
	u >>= 8;
	dst[3] = (unsigned char)u;
}

/* worker for compression */
typedef struct {
	ZSTDMT_CCtx *ctx;
	ZSTD_CStream *zctx;
	pthread_t pthread;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	ZSTDMT_Buffer out;
	struct list_head node;
};

struct ZSTDMT_CCtx_s {

	/* level: 1..22 */
	int level;

	/* threads: 1..ZSTDMT_THREAD_MAX */
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
 *  Compression
 ****************************************/

ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level, int inputsize)
{
	ZSTDMT_CCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (ZSTDMT_CCtx *) malloc(sizeof(ZSTDMT_CCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > ZSTDMT_THREAD_MAX)
		return 0;

	/* check level */
	if (level < 1 || level > 22)
		return 0;

	/* calculate chunksize for one thread */
	if (inputsize)
		ctx->inputsize = inputsize;
	else {
#if 1
		const int windowLog[] = {
			0, 19, 19, 20, 20, 20, 21, 21,
			21, 21, 21, 22, 22, 22, 22, 22,
			23, 23, 23, 23, 23, 25, 26, 27
		};
		ctx->inputsize = 1 << (windowLog[level] + 1);
#else
		const int mb[] = { 0, 1, 1, 1, 2, 2, 2,
			3, 3, 3, 4, 4, 4, 5,
			5, 5, 5, 5, 5, 5, 5
		};
		ctx->inputsize = 1000 * 1000;
#endif
	}

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
		w->zctx = ZSTD_createCStream();
		if (!w->zctx)
			goto err_zctx;
	}

	return ctx;

 err_zctx:
	{
		int i;
		for (i = 0; i < t; i++) {
			cwork_t *w = &ctx->cwork[t];
			free(w->zctx);
		}
	}
	free(ctx->cwork);
 err_cwork:
	free(ctx);

	return 0;
}

#if 0
/**
 * pt_read - read input a bit smarter
 */
static int pt_read(ZSTDMT_CCtx * ctx, ZSTDMT_Buffer * in)
{
	/**
	 * -1 not allocated
	 *  0 no more elemtes currently
	 * >0 N elements left
	 */
	static int array_len = -1;
	static ZSTDMT_Buffer *array = 0;
	size_t total;
	int t;

	/* deinit */
	if (tid == 0) {
		if (need_init == 0)
			free(in);
		return 0;
	}

	/* init */
	if (array_len == -1) {
		array =
		    (ZSTDMT_Buffer *) malloc(sizeof(ZSTDMT_Buffer) *
					     ctx->threads);
		if (!array)
			return -1;
		array_len = 0;
	}

	/* return next pre-read value */
	if (array_len) {
		in = array[ctx->threads - array_len];
		array_len -= 1;
		return in->size;
	}

	/* read new content */
	total = 0;
	for (t = 0; t < ctx->threads; t++) {
		array[t].size = ctx->inputsize;
		array[t].buf = malloc(ctx->inputsize);
		int len = ctx->fn_read(ctx->arg_read, array[t]);
		if (len == 0)
			break;
		if (len == -1)
			return -1;
		total += len;
	}

	/* check, if chunks are full */
	if (total == ctx->inputsize * ctx->threads) {
		in = array[0];
		array_len = ctx->threads - 1;
		return;
	}
#define MINSIZE ZSTD_CStreamInSize()
	/* eof reached, split up the work a bit... */

	/**
	 * case one, all items together do not fullfill the minumum size
	 * -> create the work for N threads, which get all the optimal size
	 */
	if (total < ZSTD_CStreamInSize() * ctx->threads) {
	}

	return 0;
}
#endif

/**
 * pt_write - queue for compressed output
 */
static int pt_write(ZSTDMT_CCtx * ctx, size_t frame, ZSTDMT_Buffer * out)
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
	/* check, what can be written ... O(n) for thread count */
	list_for_each(entry, &ctx->writelist) {
		wl = list_entry(entry, struct writelist, node);
		if (wl->frame == ctx->curframe) {
			int rv;
			rv = ctx->fn_write(ctx->arg_write, &wl->out);
			if (rv < 0)
				return -1;
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
	ZSTDMT_CCtx *ctx = w->ctx;

	ZSTDMT_Buffer in;
	ZSTDMT_Buffer out;

	/* used for error message */
	out.buf = 0;

	/* inbuf is constant */
	in.buf = malloc(ctx->inputsize);
	if (!in.buf)
		goto error;

	for (;;) {
		size_t result, frame;
		int rv;

		result = ZSTD_initCStream(w->zctx, ctx->level);
		if (ZSTD_isError(result)) {
			out.buf = (void *)ZSTD_getErrorName(result);
			goto error;
		}

		/* allocate space for new output */
		out.size = (int)ZSTD_compressBound(ctx->inputsize) + 12;
		out.buf = malloc(out.size);
		if (!out.buf)
			goto error;

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		in.size = ctx->inputsize;
		rv = ctx->fn_read(ctx->arg_read, &in);
		if (rv == -1) {
			out.buf = "Error while reading input!";
			pthread_mutex_unlock(&ctx->read_mutex);
			goto error;
		}
		if (in.size == 0) {
			free(in.buf);
			free(out.buf);
			pthread_mutex_unlock(&ctx->read_mutex);
			return 0;
		}
		ctx->insize += in.size;
		frame = ctx->frames++;
		pthread_mutex_unlock(&ctx->read_mutex);

		/* compress whole frame */
		ZSTD_inBuffer input;
		input.src = in.buf;
		input.size = in.size;
		input.pos = 0;

		ZSTD_outBuffer output;
		output.dst = (char *)out.buf + 12;
		output.size = out.size - 12;
		output.pos = 0;

		result = ZSTD_compressStream(w->zctx, &output, &input);
		if (ZSTD_isError(result)) {
			out.buf = (void *)ZSTD_getErrorName(result);
			goto error;
		}

		if (input.size != input.pos) {
			out.buf = "ZSTD_compressStream didn't consume input!";
			goto error;
		}

		result = ZSTD_endStream(w->zctx, &output);
		if (ZSTD_isError(result)) {
			out.buf = (void *)ZSTD_getErrorName(result);
			goto error;
		}

		/* write skippable frame */
		write_le32((unsigned char *)out.buf + 0, 0x184D2A50);
		write_le32((unsigned char *)out.buf + 4, 4);
		write_le32((unsigned char *)out.buf + 8,
			   (unsigned int)output.pos);
		out.size = (int)output.pos + 12;

		/* write result */
		pthread_mutex_lock(&ctx->read_mutex);
		rv = pt_write(ctx, frame, &out);
		pthread_mutex_unlock(&ctx->read_mutex);
		if (rv == -1) {
			out.buf = "Error while writing output!";
			goto error;
		}
	}

 error:
#if 0
	if (!out.buf)
		out.buf = "Allocation error : not enough memory";
	printf("ERR @ pt_compress() = %s\n", (char *)out.buf);
	fflush(stdout);
#endif
	out.size = 0;
	return (void *)-1;
}

int ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, ZSTDMT_RdWr_t * rdwr)
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
size_t ZSTDMT_GetInsizeCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t ZSTDMT_GetOutsizeCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t ZSTDMT_GetFramesCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx)
{
	int t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		ZSTD_freeCStream(w->zctx);
	}
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
