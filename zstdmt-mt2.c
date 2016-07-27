
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

#include "zstdmt-mt2.h"

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

//#define DEBUGME2

#ifdef DEBUGME2
#include <stdio.h>
#endif

static void die_nomem(void)
{
	exit(123);
}

/* thread work */
typedef struct {
	/* ZBUFF_CCtx ZBUFF_DCtx */ ;
	void *zctx;
	int level;

	/* ptr to ZSTDMT_CCtx_s */
	void *ctx;

	/* thread id (alias stream id) */
	int tid;

	/* buffers */
	void *inbuf;
	void *outbuf;
	size_t inlen;
	size_t outlen;
} work_t;

/* thread work */
struct worklist;
struct worklist {
	size_t frame;
	void *buf;
	size_t len;
	int tid;
	struct worklist *next;
};

struct ZSTDMT_CCtx_s {

	/* number of threads and compression level */
	int level;
	int threads;

	/* statistic */
	size_t frames;		/* frames read @ input */
	size_t insize;		/* current read size @ input */
	size_t outsize;		/* current written size to output */
	size_t curframe;	/* current frame @ output */

	/* fdin and fdout, each with func and mutex */
	int fdin;
	int fdout;
	fn_read *readfn;
	fn_write *writefn;
	pthread_mutex_t read_mutex;
	pthread_mutex_t write_mutex;
	struct worklist worklist;

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
	ctx->curframe = 0;

	ctx->work = (work_t *) malloc(sizeof(work_t) * threads);
	if (!ctx->work) {
		free(ctx);
		return 0;
	}

	ctx->th = (pthread_t *) malloc(sizeof(pthread_t) * threads);
	if (!ctx->th) {
		free(ctx->work);
		free(ctx);
		return 0;
	}

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	for (t = 0; t < threads; t++) {
		work_t *work = &ctx->work[t];
		work->ctx = ctx;
		work->zctx = ZBUFF_createCCtx();
		if (!work->zctx) {
			free(ctx->work);
			free(ctx->th);
			free(ctx);
			return 0;
		}

		ZBUFF_compressInit(work->zctx, ctx->level);
		work->tid = t;
		work->level = level;
	}

	return ctx;
}

static void WriteCompressedData(ZSTDMT_CCtx * ctx, int thread, size_t frame,
				void *buf, size_t length)
{
	struct worklist *head = &ctx->worklist, *wl, *cur;

	/**
	 * the very first frame gets its header
	 * -> 2^14 thread id
	 */
	if (frame == 0) {
		unsigned char hdr[2];
		unsigned int tid = ctx->threads;
		hdr[0] = tid;
		tid >>= 8;
		hdr[1] = tid;
		ctx->writefn(ctx->fdout, hdr, 2);
		ctx->outsize += 2;
	}

	if (frame == ctx->curframe) {

		/**
		 * 2^14 thread id  (max = 0x3f ff)
		 * 2^18 compressed length (max = 3 ff ff)
		 *
		 * TTTT:TTTT TTTT:TTLL LLLL:LLLL LLLL:LLLL
		 */
		unsigned char hdr[4];
		unsigned int len = length, tid = thread;

		hdr[0] = tid;
		tid >>= 8;
		hdr[3] = len;
		len >>= 8;
		hdr[2] = len;
		len >>= 8;
		hdr[1] = tid | (len << 6);
		ctx->writefn(ctx->fdout, hdr, 4);

		/* new not written buffer found */
		ctx->writefn(ctx->fdout, buf, len);
		ctx->outsize += length + 4;
		ctx->curframe++;
		free(buf);

#ifdef DEBUGME2
		printf("WRITE1(%d), frame=%zu len=%zu\n", thread, frame,
		       length);
#endif

	} else {
		cur = (struct worklist *)malloc(sizeof(struct worklist));
		if (!cur)
			die_nomem();
		cur->frame = frame;
		cur->buf = buf;
		cur->len = length;
		cur->tid = thread;

		/* add new data */
		wl = head;
		cur->next = wl->next;
		wl->next = cur;
	}

	/* check, for elements we have to write */
	for (wl = head; wl->next; wl = wl->next) {
		cur = wl->next;

		if (cur->frame != ctx->curframe)
			continue;

		/**
		 * 2^14 thread id  (max = 0x3f ff)
		 * 2^18 compressed length (max = 3 ff ff)
		 *
		 * TTTT:TTTT TTTT:TTLL LLLL:LLLL LLLL:LLLL
		 */
		{
			unsigned char hdr[4];
			unsigned int len = cur->len, tid = cur->tid;

			hdr[0] = tid;
			tid >>= 8;
			hdr[3] = len;
			len >>= 8;
			hdr[2] = len;
			len >>= 8;
			hdr[1] = tid | (len << 6);
			ctx->writefn(ctx->fdout, hdr, 4);
		}

		/* new not written buffer found */
		ctx->writefn(ctx->fdout, cur->buf, cur->len);
		ctx->outsize += cur->len + 4;
		ctx->curframe++;
		free(cur->buf);

#ifdef DEBUGME2
		printf("WRITE2(%d), frame=%zu len=%zu\n", cur->tid, cur->frame,
		       cur->len);
#endif

		/* remove then */
		if (wl != head) {
			wl->next = cur->next;
			free(cur);
			/* back to the root */
			wl = head;
		}
	}

	return;
}

/**
 * compression worker
 * - each worker reads input itself
 * - after this, it compresses the thing
 * - when done with compression, the result is given to one special writing thread
 * - this writing thread reads all compressed stuff
 * - reading new input then ... by who?
 */
static void *pt_compress(void *arg)
{
	work_t *work = (work_t *) arg;
	ZSTDMT_CCtx *ctx = work->ctx;
	ssize_t ret, len;

	/* inbuf is constant */
	work->inlen = ZBUFF_recommendedCInSize();
	work->inbuf = (void *)malloc(work->inlen);
	for (;;) {
		size_t frame;

		/* allocate space for new input */
		work->outlen = ZBUFF_recommendedCOutSize();
		work->outbuf = (void *)malloc(work->outlen);
		if (!work->inbuf)
			die_nomem();
		if (!work->outbuf)
			die_nomem();

		pthread_mutex_lock(&ctx->read_mutex);
		len = ctx->readfn(ctx->fdin, work->inbuf, work->inlen);
		if (len > 0)
			ctx->insize += len;
		frame = ctx->frames;
		ctx->frames++;
		//printf("read(%d) frame=%zu, len=%zd\n", work->tid, frame, len);
		pthread_mutex_unlock(&ctx->read_mutex);

		if (len == 0) {
			free(work->outbuf);
			free(work->inbuf);
			break;
		}

		ret =
		    ZBUFF_compressContinue(work->zctx,
					   work->outbuf,
					   &work->outlen, work->inbuf,
					   &work->inlen);
		if (ZSTD_isError(ret))
			return 0;

#ifdef DEBUGME
		printf
		    ("ZBUFF_compressContinue()[%2d] ret=%zu inlen=%zu outlen=%zu\n",
		     work->tid, ret, work->inlen, work->outlen);
#endif

		if (ret != work->inlen) {
			work->outlen = ZBUFF_recommendedCOutSize();
			ret =
			    ZBUFF_compressFlush(work->zctx,
						work->outbuf, &work->outlen);

#ifdef DEBUGME
			printf
			    ("ZBUFF_compressFlush()[%2d] ret=%zu inlen=%zu outlen=%zu\n",
			     work->tid, ret, work->inlen, work->outlen);
#endif

			if (ZSTD_isError(ret))
				return 0;

		}

		pthread_mutex_lock(&ctx->write_mutex);
		WriteCompressedData(ctx, work->tid, frame, work->outbuf,
				    work->outlen);
		pthread_mutex_unlock(&ctx->write_mutex);
	}

	return 0;
}

ssize_t ZSTDMT_CompressCCtx(ZSTDMT_CCtx * ctx, fn_read readfn, fn_write writefn,
			    int fdin, int fdout)
{
	size_t t;

	if (!ctx)
		return 0;

	/* init reading and writing functions */
	ctx->fdin = fdin;
	ctx->fdout = fdout;
	ctx->readfn = readfn;
	ctx->writefn = writefn;

	/* start all workers */
	for (t = 0; t < ctx->threads; t++) {
		work_t *w = &ctx->work[t];
		w->ctx = ctx;
		pthread_create(&ctx->th[t], NULL, pt_compress, w);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		pthread_join(ctx->th[t], NULL);
	}

	return 0;
}

void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx)
{
	size_t t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		ZBUFF_freeCCtx(ctx->work[t].zctx);
	}

	free(ctx->work);
	free(ctx->th);
	free(ctx);
	ctx = 0;

	return;
}

/* returns current frame number */
size_t ZSTDMT_GetCurrentFrameCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->frames - 1;
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

/* **************************************
 *  Decompression
 ****************************************/

ZSTDMT_DCtx *ZSTDMT_createDCtx(unsigned char hdr[2])
{
	return 0;
}

/* 2) decompression */
ssize_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, fn_read readfn,
			      fn_write writefn, int fdin, int fdout)
{
	return 0;
}

/* 3) free dctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx)
{
	return;
}
