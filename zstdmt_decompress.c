
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

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "threading.h"
#include "list.h"
#include "zstdmt.h"

/**

TODO.......

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
static unsigned int read_le32(const unsigned char *x)
{
	return (unsigned int)(x[0]) | (((unsigned int)(x[1])) << 8) |
	    (((unsigned int)(x[2])) << 16) | (((unsigned int)(x[3])) << 24);
}

/* worker for compression */
typedef struct {
	ZSTDMT_DCtx *ctx;
	ZSTD_CStream *zctx;
	pthread_t pthread;
	ZSTDMT_Buffer in;
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

/**
 * pt_write - queue for compressed output
 */
static int pt_write(ZSTDMT_DCtx * ctx, size_t frame, ZSTDMT_Buffer * out)
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
	ZSTDMT_DCtx *ctx = w->ctx;
	int result, len;

	ZSTDMT_Buffer in;
	ZSTDMT_Buffer out;

	/* used for error message */
	out.buf = 0;

	/* inbuf is constant */
	in.size = ctx->inputsize;
	in.buf = malloc(in.size);
	if (!in.buf)
		goto error;

	for (;;) {
		size_t frame;

		result = ZSTD_initCStream(w->zctx, ctx->level);
		if (ZSTD_isError(result)) {
			out.buf = (void *)ZSTD_getErrorName(result);
			goto error;
		}

		/* allocate space for new output */
		out.size = ZSTD_compressBound(ctx->inputsize) + 12;
		out.buf = malloc(out.size);
		if (!out.buf)
			goto error;

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		len = ctx->fn_read(ctx->arg_read, &in);
		/* later: len = pt_read(ctx, w->tid, &in); */
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
		ZSTD_inBuffer input = { in.buf, len, 0 };
		ZSTD_outBuffer output = { out.buf + 12, out.size - 12, 0 };

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
		write_le32(out.buf + 0, 0x184D2A50);
		write_le32(out.buf + 4, 4);
		write_le32(out.buf + 8, output.pos);
		out.size = output.pos + 12;

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

ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads)
{
	ZSTDMT_DCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > ZSTDMT_THREAD_MAX)
		return 0;

	/* check level */
	if (level < 1 || level > 22)
		return 0;

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

st_decompress()
{
	size_t result;
	UInt32 const toRead = static_cast < const UInt32 > (_buffInSize);
	for (;;) {
		UInt32 read;

		/* read input */
		RINOK(inStream->Read(_buffIn, toRead, &read));
		size_t InSize = static_cast < size_t > (read);
		_processedIn += InSize;

		if (InSize == 0)
			return S_OK;

		/* decompress input */
		ZSTD_inBuffer input = { _buffIn, InSize, 0 };
		for (;;) {
			ZSTD_outBuffer output = { _buffOut, _buffOutSize, 0 };
			result =
			    ZSTD_decompressStream(_dstream, &output, &input);
#if 0
			printf
			    ("%s in=%d out=%d result=%d in.pos=%d in.size=%d\n",
			     __FUNCTION__, InSize, output.pos, result,
			     input.pos, input.size);
			fflush(stdout);
#endif
			if (ZSTD_isError(result))
				return ErrorOut(result);

			/* write decompressed stream and update progress */
			RINOK(WriteStream(outStream, _buffOut, output.pos));
			_processedOut += output.pos;
			RINOK(progress->SetRatioInfo
			      (&_processedIn, &_processedOut));

			/* one more round */
			if ((input.pos == input.size) && (result == 1)
			    && output.pos)
				continue;

			/* finished */
			if (input.pos == input.size)
				break;

			/* end of frame */
			if (result == 0) {
				result = ZSTD_initDStream(_dstream);
				if (ZSTD_isError(result))
					return ErrorOut(result);
			}
		}
	}
}

int ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, ZSTDMT_RdWr_t * rdwr)
{
	ZSTDMT_Buffer in;
	unsigned char buf[4];
	int t, rv;

	if (!ctx)
		return -1;

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for 0x184D2A50 (multithreading stream) */
	in.size = 4;
	in.buf = buf;
	rv = ctx->fn_read(ctx->arg_read, &in);
	if (rv == -1)
		return -1;
	if (read_le32(buf) != 0x184D2A50) {
		/* single threading fallback */
		st_decompress(&in);
		return 0;
	}

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
		ZSTD_freeCStream(w->zctx);
	}
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
