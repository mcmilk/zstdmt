
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

#include <sys/types.h>
#include <unistd.h>

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

#define DEBUGME
#ifdef DEBUGME
#include <stdio.h>
#endif

static void die_nomem(void)
{
	exit(123);
}

/* thread work */
typedef struct {
	ZBUFF_CCtx *zctx;
	ZSTDMT_CCtx *ctx;
	int level;

	/* thread id */
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
	size_t block;
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
	size_t blocks;		/* blocks read @ input */
	size_t insize;		/* current read size @ input */
	size_t outsize;		/* current written size to output */
	size_t curblock;	/* current block @ output */

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

typedef struct work_item_tag {
	struct work_item_tag *next;
	void *data;
} work_item_t;

typedef struct {
	work_item_t *head;
	work_item_t *tail;
	int item_count;
} work_queue_t;

static void work_queue_push(work_queue_t * work_q, void *data)
{
	work_item_t *work_item;

	if (!work_q)
		return;

	work_item = malloc(sizeof(work_item_t));
	work_item->next = NULL;
	work_item->data = data;

	if (work_q->head == NULL) {
		work_q->head = work_q->tail = work_item;
		work_q->item_count = 1;
	} else {
		work_q->tail->next = work_item;
		work_q->tail = work_item;
		work_q->item_count++;
	}

	return;
}

static void *work_queue_pop(work_queue_t * work_q)
{
	work_item_t *work_item;
	void *data;

	if (!work_q || !work_q->head)
		return 0;

	work_item = work_q->head;
	data = work_item->data;
	work_q->head = work_item->next;
	if (work_q->head == NULL) {
		work_q->tail = NULL;
	}
	free(work_item);
	work_q->item_count--;

	return data;
}

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
	ctx->blocks = 0;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->curblock = 0;

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

static void WriteCompressedHdr(ZSTDMT_CCtx * ctx, size_t length, int thread)
{
	unsigned char hdr[4];
	unsigned int len = length, tid = thread;

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
	ctx->writefn(ctx->fdout, hdr, 4);
	ctx->outsize += 4;

	return;
}

static void WriteCompressedData(ZSTDMT_CCtx * ctx, int thread, size_t block,
				void *buf, size_t length)
{
	struct worklist *head = &ctx->worklist, *wl, *cur;

	if (block == ctx->curblock) {
		/* new not written buffer found */
		WriteCompressedHdr(ctx, length, thread);
		ctx->writefn(ctx->fdout, buf, length);
		ctx->outsize += length;
		ctx->curblock++;
		free(buf);
	} else {
		cur = (struct worklist *)malloc(sizeof(struct worklist));
		if (!cur)
			die_nomem();
		cur->block = block;
		cur->buf = buf;
		cur->len = length;
		cur->tid = thread;

		/* add new data to queue */
		wl = head;
		cur->next = wl->next;
		wl->next = cur;
	}

	/* check, for elements we have to write */
	for (wl = head; wl->next; wl = wl->next) {
		cur = wl->next;

		if (cur->block != ctx->curblock)
			continue;

		WriteCompressedHdr(ctx, cur->len, cur->tid);

		/* new not written buffer found */
		ctx->writefn(ctx->fdout, cur->buf, cur->len);
		ctx->outsize += cur->len + 4;
		ctx->curblock++;
		free(cur->buf);

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
		size_t block;

		/* allocate space for new output */
		work->outlen = ZBUFF_recommendedCOutSize();
		work->outbuf = (void *)malloc(work->outlen);
		if (!work->inbuf)
			die_nomem();
		if (!work->outbuf)
			die_nomem();

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		len = ctx->readfn(ctx->fdin, work->inbuf, work->inlen);
		if (len == 0) {
			free(work->outbuf);
			free(work->inbuf);
#ifdef DEBUGME
			printf
			    ("ZBUFF_compress()[%2d] ENDE blocks=%zu curblock=%zu\n",
			     work->tid, ctx->blocks, ctx->curblock);
#endif
			pthread_mutex_unlock(&ctx->read_mutex);
			return 0;
		}

		ctx->insize += len;
		block = ctx->blocks;
		ctx->blocks++;
		pthread_mutex_unlock(&ctx->read_mutex);

		/* compressing */
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

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		WriteCompressedData(ctx, work->tid, block, work->outbuf,
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

	/**
	 * write the number of threads into the first 2 bytee
	 * -> 2^14 thread id
	 */
	{
		unsigned char hdr[2];
		unsigned int tid = ctx->threads;
		hdr[0] = tid;
		tid >>= 8;
		hdr[1] = tid;
		ctx->writefn(ctx->fdout, hdr, 2);
		ctx->outsize += 2;
	}

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

/* returns current block number */
size_t ZSTDMT_GetCurrentBlockCCtx(ZSTDMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curblock;
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

/* decompression work structure */
typedef struct {
	ZBUFF_DCtx *zctx;
	ZSTDMT_DCtx *ctx;

	/* thread id */
	int tid;

	/* buffers */
	void *inbuf;
	void *outbuf;
	size_t inlen;
	size_t outlen;
	size_t block;		/* blocks read @ input */

/* decompress thread states */
#define DTH_STATE_WAIT 0
#define DTH_STATE_WORK 1
#define DTH_STATE_EXIT 2
	int state;
	pthread_cond_t work; /* signal from main to worker, work */
	pthread_cond_t done; /* signal from worker to main, done */
	pthread_mutex_t mutex;
} dwork_t;

struct ZSTDMT_DCtx_s {

	/* number of threads, which are used for decompressing */
	int threads;

	/* statistic */
	size_t blocks;		/* blocks read @ input */
	size_t insize;		/* current read size @ input */
	size_t outsize;		/* current written size to output */
	size_t curblock;	/* current block @ output */

	/* fdin and fdout, each with func and mutex */
	int fdin;
	int fdout;
	fn_read *readfn;
	fn_write *writefn;

	/* all decompression threads can write */
	pthread_mutex_t write_mutex;
	// struct worklist worklist;

	work_queue_t *queue;

	/* threading */
	pthread_t *th;
	dwork_t *work;
	pthread_mutex_t mutex;
};

ZSTDMT_DCtx *ZSTDMT_createDCtx(unsigned char hdr[2])
{
	ZSTDMT_DCtx *ctx;
	int t;

	ctx = (ZSTDMT_DCtx *) malloc(sizeof(ZSTDMT_DCtx));
	if (!ctx) {
		return 0;
	}

	ctx->threads = hdr[0] + ((hdr[1] & 0x3f) << 8);
	ctx->blocks = 0;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->curblock = 0;

	pthread_mutex_init(&ctx->mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	ctx->work = (dwork_t *) malloc(sizeof(dwork_t) * ctx->threads);
	if (!ctx->work) {
		free(ctx);
		return 0;
	}

	ctx->th = (pthread_t *) malloc(sizeof(pthread_t) * ctx->threads);
	if (!ctx->th) {
		free(ctx->work);
		free(ctx);
		return 0;
	}

	for (t = 0; t < ctx->threads; t++) {
		dwork_t *work = &ctx->work[t];
		work->ctx = ctx;
		work->tid = t;
		work->zctx = ZBUFF_createDCtx();
		if (!work->zctx) {
			free(ctx->work);
			free(ctx->th);
			free(ctx);
			return 0;

		}
		ZBUFF_decompressInit(work->zctx);
		pthread_mutex_init(&work->mutex, NULL);
		pthread_cond_init(&work->done, NULL);
		pthread_cond_init(&work->work, NULL);
	}

	return ctx;
}

static void WriteDecompressedData(ZSTDMT_DCtx * ctx, int thread, size_t block,
				void *buf, size_t length)
{
	//struct worklist *head = &ctx->worklist, *wl, *cur;

	printf("writing[%d] len=%zu\n", thread, length);

#if 0
	if (block == ctx->curblock) {
		/* new not written buffer found */
		ctx->writefn(ctx->fdout, buf, length);
		ctx->outsize += length;
		ctx->curblock++;
		free(buf);
	} else {
		cur = (struct worklist *)malloc(sizeof(struct worklist));
		if (!cur)
			die_nomem();
		cur->block = block;
		cur->buf = buf;
		cur->len = length;
		cur->tid = thread;

		/* add new data to queue */
		wl = head;
		cur->next = wl->next;
		wl->next = cur;
	}

	/* check, for elements we have to write */
	for (wl = head; wl->next; wl = wl->next) {
		cur = wl->next;

		if (cur->block != ctx->curblock)
			continue;

		/* new not written buffer found */
		ctx->writefn(ctx->fdout, cur->buf, cur->len);
		ctx->outsize += cur->len + 4;
		ctx->curblock++;
		free(cur->buf);

		/* remove then */
		if (wl != head) {
			wl->next = cur->next;
			free(cur);
			/* back to the root */
			wl = head;
		}
	}
#endif

	return;
}

/**
 * decompression worker
 */
static void *pt_decompress(void *arg)
{
	dwork_t *work = (dwork_t *) arg;
	size_t ret;

	for (;;) {
		pthread_mutex_lock(&work->mutex);

		for (;;) {
			printf(" thread[%d] oldlen=%zu WAIT\n", work->tid, work->inlen);
			int r = pthread_cond_wait(&work->work, &work->mutex);
			printf(" thread[%d] len=%zu TODO\n", work->tid, work->inlen);
			if (r)
				die_nomem();

			if (work->state == DTH_STATE_EXIT)
				return 0;

			if (work->state == DTH_STATE_WORK)
				break;
		}

		work->state = DTH_STATE_WAIT;
		work->outlen = ZBUFF_recommendedDInSize();
		work->outbuf = malloc(work->outlen);
		if (!work->outbuf)
			die_nomem();

#ifdef DEBUGME2
		printf
		    ("ZBUFF_decompressContinueBefore[%d] outlen=%zu inlen=%zu\n",
		     work->tid, work->outlen, work->inlen);
#endif

		ret = ZBUFF_decompressContinue(work->zctx,
					       work->outbuf,
					       &work->outlen,
					       work->inbuf, &work->inlen);

#ifdef DEBUGME2
		printf ("ZBUFF_decompressContinue[%d] outlen=%zu inlen=%zu sleep4()\n", work->tid, work->outlen, work->inlen);
#endif
		if (ZSTD_isError(ret)) {
#ifdef DEBUGME
			printf(" thread[%d] len=%zu ZStd Error: %s\n", work->tid, work->inlen, ZSTD_getErrorName(ret));
			fflush(stdout);
#endif
			die_nomem();
			return 0;
		}

		pthread_mutex_lock(&work->ctx->write_mutex);
		//work->ctx->writefn(work->ctx->fdout, work->outbuf, work->outlen);
		WriteDecompressedData(work->ctx, work->tid, 0, work->outbuf, work->outlen);
		pthread_mutex_unlock(&work->ctx->write_mutex);

		printf(" thread[%d] len=%zu DONE\n", work->tid, work->inlen);

		pthread_cond_signal(&work->done);
		pthread_mutex_unlock(&work->mutex);
	}

	return 0;
}

/* 2) decompression
 * - start the decompression threads
 * - one main thread which reads input
 *   - first: read 4 bytes for getting the thread id and the length
 *   - second: read the input, length is known
 *   - give the work to the decompressing thread, id = tid
 * - read the next input, until no input is read
 * - wait for all decompression threads to finish
 */
ssize_t ZSTDMT_DecompressDCtx(ZSTDMT_DCtx * ctx, fn_read readfn,
			      fn_write writefn, int fdin, int fdout)
{
	dwork_t *work;
	size_t t;

	if (!ctx)
		return -1;

	/* init reading and writing functions */
	ctx->fdin = fdin;
	ctx->fdout = fdout;
	ctx->readfn = readfn;
	ctx->writefn = writefn;

	/* start all workers */
	for (t = 0; t < ctx->threads; t++) {
		dwork_t *w = &ctx->work[t];
		w->ctx = ctx;
		pthread_create(&ctx->th[t], NULL, pt_decompress, w);
	}

	/* read input */
	for (;;) {
		int rv;
		unsigned char hdr[4];
		unsigned int tid, len;
		void *inbuf = malloc(ZBUFF_recommendedDInSize());

		len = ctx->readfn(ctx->fdin, hdr, 4);
		if (len == 0) {
			free(inbuf);
			break;
		}

		if (len != 4) {
			return -1;
		}

		/* get tid and length */
		tid = hdr[0] + ((hdr[1] & 0x3f) << 8);
		len = hdr[3] + (hdr[2] << 8) + (((hdr[1] & 0xc0) << 16) >> 6);
#ifdef DEBUGME
		ssize_t off = lseek(ctx->fdin, 0, SEEK_CUR);
		printf("main[%u] len(%u) off=%zd block=%zu\n", tid,
		       len, off, ctx->blocks);
#endif
		if (tid >= ctx->threads) {
			return -1;
		}

		len = ctx->readfn(ctx->fdin, inbuf, len);
		if (len > 0)
			ctx->insize += len;

		if (len == 0) {
			//perror("len == 0 - ende");
			free(inbuf);
			break;
		}

		work = &ctx->work[tid];

		/* add work to the queue of specific thread */
		rv = pthread_mutex_lock(&work->mutex);
		if (rv)
			die_nomem();

		work->block = ctx->blocks;
		work->inbuf = inbuf;
		work->inlen = len;
		ctx->blocks++;

		/* size_t block; void *buf; size_t len; int tid; */
		work->state = DTH_STATE_WORK;
		rv = pthread_cond_signal(&work->work);
		if (rv)
			die_nomem();
		rv = pthread_mutex_unlock(&work->mutex);
		if (rv)
			die_nomem();
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		work = &ctx->work[t];
		work->state = DTH_STATE_EXIT;
		//pthread_cond_signal(&work->work);
		//int r = pthread_cond_wait(&work->work, &work->mutex);
		pthread_join(ctx->th[t], NULL);
	}

	return 0;
}

/* 3) free dctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx)
{
	size_t t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		ZBUFF_freeDCtx(ctx->work[t].zctx);
	}

	free(ctx->work);
	free(ctx->th);
	free(ctx);
	ctx = 0;

	return;
}
