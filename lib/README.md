
# Usage

## Compression

```
typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} ZSTDMT_Buffer;

/**
 * defining wrapper functions for read/write
 * - you can use stdio functions
 * - you can also use read(2) / write(2) on unix
 * - a sample is given in 7-Zip ZS MT
 *
 * valid return values for the wrapper function
 *  0 - no error, success (all was written or read)
 * -1 - generic read or write error
 * -2 - cancel request by user
 * -3 - out of memory
 */

/**
 * fn_read() - read some input
 */
typedef int (fn_read) (void *args, ZSTDMT_Buffer * in);

/**
 * fn_write() - write some output
 */
typedef int (fn_write) (void *args, ZSTDMT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} ZSTDMT_RdWr_t;

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;

/* 1) allocate new cctx */
ZSTDMT_CCtx *ZSTDMT_createCCtx(int threads, int level, int inputsize);

/* 2) threaded compression */
size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx * ctx, ZSTDMT_RdWr_t * rdwr);

/* 3) get some statistic */
size_t ZSTDMT_GetFramesCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetInsizeCCtx(ZSTDMT_CCtx * ctx);
size_t ZSTDMT_GetOutsizeCCtx(ZSTDMT_CCtx * ctx);

/* 4) free cctx */
void ZSTDMT_freeCCtx(ZSTDMT_CCtx * ctx);
```

## Decompression
```
typedef struct ZSTDMT_DCtx_s ZSTDMT_DCtx;

/* 1) allocate new cctx */
ZSTDMT_DCtx *ZSTDMT_createDCtx(int threads, int inputsize);

/* 2) threaded decompression */
size_t ZSTDMT_decompressDCtx(ZSTDMT_DCtx * ctx, ZSTDMT_RdWr_t * rdwr);

/* 3) get some statistic */
size_t ZSTDMT_GetFramesDCtx(ZSTDMT_DCtx * ctx);
size_t ZSTDMT_GetInsizeDCtx(ZSTDMT_DCtx * ctx);
size_t ZSTDMT_GetOutsizeDCtx(ZSTDMT_DCtx * ctx);

/* 4) free cctx */
void ZSTDMT_freeDCtx(ZSTDMT_DCtx * ctx);
```
