#include "lzfse.h"
#include "lzfse-mt.h"

#include "memmt.h"
#include "threading.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LZFSE_IN_ALLOC_SIZE (1 << 16)

/**
 * multi threaded lzfse - multiple workers version
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

typedef struct {
	LZFSEMT_CCtx *ctx;
	pthread_t pthread;
} cwork_t;

struct writelist {
	size_t frame;
	LZFSEMT_Buffer out;
	struct list_head node;
};


struct LZFSEMT_CCtx_s {

	/* levels: 1..LZFSEMT NOT USE  DELETE level maybe later*/
	int level;

	/* threads: 1..LZFSEMT_THREAD_MAX */
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
	fnRead *fn_read;
	void *arg_read;

	/* writing output */
	pthread_mutex_t write_mutex;
	fnWrite *fn_write;
	void *arg_write;

	/* lists for writing queue */
	struct list_head writelist_free;
	struct list_head writelist_busy;
	struct list_head writelist_done;
};

/* **************************************
 * Compression
 ****************************************/

LZFSEMT_CCtx *LZFSEMT_createCCtx(int threads, __attribute__((unused)) int level,/*Not use*/ 
								   int inputsize)
{
	LZFSEMT_CCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZFSEMT_CCtx *) malloc(sizeof(LZFSEMT_CCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZFSEMT_THREAD_MAX)
		return 0;

	/* check level */
	/* None level */

	/* calculate chunksize for one thread */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = LZFSE_IN_ALLOC_SIZE;  /* 64K frame */

	/* setup ctx */
	ctx->level = 0; 
	ctx->threads = threads;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	/* free -> busy -> out -> free -> ... */
	INIT_LIST_HEAD(&ctx->writelist_free);	/* free, can be used */
	INIT_LIST_HEAD(&ctx->writelist_busy);	/* busy */
	INIT_LIST_HEAD(&ctx->writelist_done);	/* can be written */

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;

	}

	return ctx;

 err_cwork:
	free(ctx);

	return NULL;
}

/**
 * mt_error - return mt lib specific error code read write ERROR
 */
static size_t mt_error(int rv)
{
	switch (rv) {
	case -1:
		return MT_ERROR(read_fail);
	case -2:
		return MT_ERROR(canceled);
	case -3:
		return MT_ERROR(memory_allocation);
	}

	return MT_ERROR(read_fail);
}

/**
 * pt_write - queue for compressed output
 */
static size_t pt_write(LZFSEMT_CCtx *ctx, struct writelist *wl)
{
	struct list_head *entry;

	/* move the entry to the done list */
	list_move(&wl->node, &ctx->writelist_done);

	/* the entry isn't the currently needed, return...  */
	if (wl->frame != ctx->curframe)
		return 0;

 again:
	/* check, what can be written ... */
	list_for_each(entry, &ctx->writelist_done) {
		wl = list_entry(entry, struct writelist, node);
		if (wl->frame == ctx->curframe) {
			int rv = ctx->fn_write(ctx->arg_write, &wl->out);
			if (rv != 0)
				return mt_error(rv);
			ctx->outsize += wl->out.size;
			ctx->curframe++;
			list_move(entry, &ctx->writelist_free);
			goto again;
		}
	}

	return 0;
}

static void *pt_compress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZFSEMT_CCtx *ctx = w->ctx;
	size_t result;
	LZFSEMT_Buffer in;

	/* inbuf is constant */
	in.size = ctx->inputsize;
	in.buf = malloc(in.size);
	if (!in.buf)
		return (void *)MT_ERROR(memory_allocation);

	for (;;) {
		struct list_head *entry;
		struct writelist *wl;
		int rv;

		/* allocate space for new output */
		pthread_mutex_lock(&ctx->write_mutex);
		if (!list_empty(&ctx->writelist_free)) {
			/* take unused entry */
			entry = list_first(&ctx->writelist_free);
			wl = list_entry(entry, struct writelist, node);
			wl->out.size = ctx->inputsize + 16;
			list_move(entry, &ctx->writelist_busy);
		} else {
			/* allocate new one */
			wl = (struct writelist *)
			    malloc(sizeof(struct writelist));
			if (!wl) {
				pthread_mutex_unlock(&ctx->write_mutex);
				return (void *)MT_ERROR(memory_allocation);
			}
			wl->out.size = ctx->inputsize + 16;
			wl->out.buf = malloc(wl->out.size);
			if (!wl->out.buf) {
				pthread_mutex_unlock(&ctx->write_mutex);
				return (void *)MT_ERROR(memory_allocation);
			}
			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		in.size = ctx->inputsize;
		rv = ctx->fn_read(ctx->arg_read, &in);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return (void *)mt_error(rv);
		}

		/* eof */
		if (in.size == 0 && ctx->frames > 0) {
			free(in.buf);
			pthread_mutex_unlock(&ctx->read_mutex);

			pthread_mutex_lock(&ctx->write_mutex);
			list_move(&wl->node, &ctx->writelist_free);
			pthread_mutex_unlock(&ctx->write_mutex);

			goto okay;
		}
		ctx->insize += in.size;
		wl->frame = ctx->frames++;
		pthread_mutex_unlock(&ctx->read_mutex);

		/* compress whole frame */
		while (1) {
			const char *ibuf = (char *)(in.buf);
			char *obuf = (char *)(wl->out.buf) + 16;
			wl->out.size -= 16;

			rv = lzfse_encode_buffer((uint8_t *)obuf, wl->out.size, (uint8_t *)ibuf, in.size, NULL);

			if (rv == 0) {
				wl->out.size <<= 1;
				wl->out.size += 16;
				wl->out.buf = (uint8_t *)realloc(wl->out.buf, wl->out.size);
				if (wl->out.buf == 0) {
					pthread_mutex_lock(&ctx->write_mutex);
					list_move(&wl->node, &ctx->writelist_free);
					pthread_mutex_unlock(&ctx->write_mutex);
					return (void *)MT_ERROR(frame_compress);
				}
				continue;
			}
			wl->out.size = rv;
			break;
		}

		/* write skippable frame */
		MEM_writeLE32((unsigned char *)wl->out.buf + 0,
			      LZFSEMT_MAGIC_SKIPPABLE);
		MEM_writeLE32((unsigned char *)wl->out.buf + 4, 8);
		MEM_writeLE32((unsigned char *)wl->out.buf + 8,
			      (U32) wl->out.size);
		/* BR */
		MEM_writeLE16((unsigned char *)wl->out.buf + 12,
			      (U16) LZFSEMT_MAGICNUMBER);

		/* number of 64KB blocks needed for decompression */
		{
		U16 hintsize;
		if (ctx->inputsize > (int)in.size) { // size of readed data less than block size
			hintsize = (U16)(in.size >> 16);
			hintsize += 1;
		} else
			hintsize = ctx->inputsize >> 16; // size of readed data equal to block size, never greater than
			MEM_writeLE16((unsigned char *)wl->out.buf + 14, hintsize);
		}

		wl->out.size += 16;

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, wl);
		pthread_mutex_unlock(&ctx->write_mutex);
		if (LZFSEMT_isError(result))
			return (void *)result;
	}

 okay:
	return 0;
}

size_t LZFSEMT_compressCCtx(LZFSEMT_CCtx *ctx, LZFSEMT_RdWr_t *rdwr)
{
	int t;
	void *retval_of_thread = 0;

	if (!ctx)
		return MT_ERROR(compressionParameter_unsupported);

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
		void *p = 0;
		pthread_join(w->pthread, &p);
		if (p)
			retval_of_thread = p;
	}

	/* clean up lists */
	while (!list_empty(&ctx->writelist_free)) {
		struct writelist *wl;
		struct list_head *entry;
		entry = list_first(&ctx->writelist_free);
		wl = list_entry(entry, struct writelist, node);
		free(wl->out.buf);
		list_del(&wl->node);
		free(wl);
	}

	return (size_t) retval_of_thread;
}

/* returns current uncompressed data size */
size_t LZFSEMT_GetInsizeCCtx(LZFSEMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZFSEMT_GetOutsizeCCtx(LZFSEMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZFSEMT_GetFramesCCtx(LZFSEMT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZFSEMT_freeCCtx(LZFSEMT_CCtx * ctx)
{
	if (!ctx)
		return;

	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}