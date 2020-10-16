#include "lzfse.h"
#include "lzfse-mt.h"

#include "memmt.h"
#include "threading.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IN_ALLOC_SIZE (1024*1024)

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

/* worker for compression */
typedef struct {
	LZFSEMT_DCtx *ctx;
	pthread_t pthread;
	LZFSEMT_Buffer in;
} cwork_t;

struct writelist {
	size_t frame;
	LZFSEMT_Buffer out;
	struct list_head node;
};

struct LZFSEMT_DCtx_s {

	/* threads: 1..LZFSEMT_THREAD_MAX */
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
 * Decompression
 ****************************************/

LZFSEMT_DCtx *LZFSEMT_createDCtx(int threads, int inputsize)
{
	LZFSEMT_DCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZFSEMT_DCtx *) malloc(sizeof(LZFSEMT_DCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZFSEMT_THREAD_MAX)
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
        w->in.allocated = 0;
        w->in.buf = NULL;
        w->in.size = 0;
		w->ctx = ctx;
	}

	return ctx;

 err_cwork:
	free(ctx);
    ctx = NULL;

	return 0;
}

/**
 * mt_error - return mt lib specific error code
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

	/* XXX, some catch all other errors */
	return MT_ERROR(read_fail);
}

/**
 * pt_write - queue for decompressed output
 */
static size_t pt_write(LZFSEMT_DCtx * ctx, struct writelist *wl)
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

/**
 * pt_read - read compressed output Verify header information
 */
static size_t pt_read(LZFSEMT_DCtx *ctx, LZFSEMT_Buffer *in, size_t *frame, 
                      size_t *uncompressed)
{
	unsigned char hdrbuf[16];
	LZFSEMT_Buffer hdr;
	int rv;

	/* read skippable frame (12 or 16 bytes) */
	pthread_mutex_lock(&ctx->read_mutex);

	/* special case, first 4 bytes already read */
	if (ctx->frames == 0) {
		hdr.buf = hdrbuf + 4;
		hdr.size = 12;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}
		if (hdr.size != 12)
			goto error_read;
		hdr.buf = hdrbuf;
	} else {
		hdr.buf = hdrbuf;
		hdr.size = 16;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}
		/* eof reached ? */
		if (hdr.size == 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			in->size = 0;
			return 0;
		}
		if (hdr.size != 16)
			goto error_read;
		if (MEM_readLE32((unsigned char *)hdr.buf + 0) !=
		    LZFSEMT_MAGIC_SKIPPABLE)
			goto error_data;
	}

	/* check header data */
	if (MEM_readLE32((unsigned char *)hdr.buf + 4) != 8)
		goto error_data;
	if (MEM_readLE16((unsigned char *)hdr.buf + 12) != LZFSEMT_MAGICNUMBER)
		goto error_data;

	ctx->insize += 16;
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
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}

		/* get uncompressed size for output buffer. */
        {
			U16 hintsize = MEM_readLE16((unsigned char *)hdr.buf + 14);
			*uncompressed = hintsize << 16;
		}

		if (in->size != toRead)
			goto error_data;

		ctx->insize += in->size;
	}
	*frame = ctx->frames++;
	pthread_mutex_unlock(&ctx->read_mutex);

	/* done, no error */
	return 0;

 error_data:
	pthread_mutex_unlock(&ctx->read_mutex);
	return MT_ERROR(data_error);
 error_read:
	pthread_mutex_unlock(&ctx->read_mutex);
	return MT_ERROR(read_fail);
 error_nomem:
	pthread_mutex_unlock(&ctx->read_mutex);
	return MT_ERROR(memory_allocation);
}

static void *pt_decompress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZFSEMT_Buffer *in = &w->in;
	LZFSEMT_DCtx *ctx = w->ctx;
	size_t result = 0;
	struct writelist *wl;

	for (;;) {
		struct list_head *entry;
		LZFSEMT_Buffer *out;

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
				result = MT_ERROR(memory_allocation);
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
		result = pt_read(ctx, in, &wl->frame, &(wl->out.size));
		if (LZFSEMT_isError(result)) {
			list_move(&wl->node, &ctx->writelist_free);
			goto error_lock;
		}

		if (in->size == 0)
			break;

		if (out->allocated < out->size) {
			if (out->allocated)
				out->buf = realloc(out->buf, out->size);
			else
				out->buf = malloc(out->size);
			if (!out->buf) {
				result = MT_ERROR(memory_allocation);
				goto error_lock;
			}
			out->allocated = out->size;
		}

		size_t realsize = lzfse_decode_buffer(out->buf, out->size, in->buf, in->size, NULL);

		/* write result */
		out->size = realsize;
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, wl);
		if (LZFSEMT_isError(result))
			goto error_unlock;
		pthread_mutex_unlock(&ctx->write_mutex);
	}

	/* everything is okay */
	pthread_mutex_lock(&ctx->write_mutex);
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
        in->buf = NULL;
        in->allocated = 0;
        in->size = 0;
	return 0;

 error_lock:
	pthread_mutex_lock(&ctx->write_mutex);
 error_unlock:
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
        in->buf = NULL;
        in->allocated = 0;
        in->size = 0;
	return (void *)result;
}

size_t LZFSEMT_decompressDCtx(LZFSEMT_DCtx * ctx, LZFSEMT_RdWr_t * rdwr)
{
	unsigned char buf[4]; // first frame LZFSEMT_MAGIC_SKIPPABLE
	int t, rv;
	cwork_t *w = &ctx->cwork[0];
	LZFSEMT_Buffer *in = &w->in;
	void *retval_of_thread = 0;

	if (!ctx)
		return MT_ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for LZFSEMT_MAGIC_SKIPPABLE  read the first frame
										   LZFSEMT_MAGIC_SKIPPABLE*/
	in->buf = buf;
	in->size = 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv != 0)
		return mt_error(rv);
	if (in->size != 4)
		return MT_ERROR(data_error);

	/* single threaded with unknown sizes */
	if (MEM_readLE32(buf) != LZFSEMT_MAGIC_SKIPPABLE)
		return MT_ERROR(data_error);

	/* mark unused */
	in->buf = 0;
	in->size = 0;
	in->allocated = 0;

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
		cwork_t *wt = &ctx->cwork[t];
		wt->in.buf = 0;
		wt->in.size = 0;
		wt->in.allocated = 0;
		pthread_create(&wt->pthread, NULL, pt_decompress, wt);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *wt = &ctx->cwork[t];
		void *p = 0;
		pthread_join(wt->pthread, &p);
		if (p)
			retval_of_thread = p;
	}

 okay:
	/* clean up the buffers */
	while (!list_empty(&ctx->writelist_free)) {
		struct writelist *wl;
		struct list_head *entry;
		entry = list_first(&ctx->writelist_free);
		wl = list_entry(entry, struct writelist, node);
		free(wl->out.buf);
        wl->out.buf = NULL;
        wl->out.allocated = 0;
        wl->out.size = 0;
		list_del(&wl->node);
		free(wl);
        wl = NULL;
	}

	return (size_t) retval_of_thread;
}

/* returns current uncompressed data size */
size_t LZFSEMT_GetInsizeDCtx(LZFSEMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZFSEMT_GetOutsizeDCtx(LZFSEMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZFSEMT_GetFramesDCtx(LZFSEMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZFSEMT_freeDCtx(LZFSEMT_DCtx * ctx)
{
	if (!ctx)
		return;

	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);
	free(ctx->cwork);
    ctx->cwork = NULL;
	free(ctx);
    ctx = NULL;
	ctx = 0;

	return;
}