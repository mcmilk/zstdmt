/* ***************************************
 * Defines
 ****************************************/

#ifndef LZFSEMT_H
#define LZFSEMT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */


#define LZFSEMT_THREAD_MAX 128
#define LZFSEMT_MAGICNUMBER 0x464C // LF
#define LZFSEMT_MAGIC_SKIPPABLE 0x184D2A50U  // MT magic number

/* **************************************
 * Error Handling
 ****************************************/

typedef enum {
  LZFSEMT_error_no_error,
  LZFSEMT_error_memory_allocation,
  LZFSEMT_error_read_fail,
  LZFSEMT_error_write_fail,
  LZFSEMT_error_data_error,
  LZFSEMT_error_frame_compress,
  LZFSEMT_error_frame_decompress,
  LZFSEMT_error_compressionParameter_unsupported,
  LZFSEMT_error_compression_library,
  LZFSEMT_error_canceled,
  LZFSEMT_error_maxCode
} LZFSEMT_ErrorCode;

#define PREFIX(name) LZFSEMT_error_##name
#define MT_ERROR(name)  ((size_t)-PREFIX(name))
extern unsigned LZFSEMT_isError(size_t code);
extern const char* LZFSEMT_getErrorString(size_t code);

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} LZFSEMT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS or bromt.c
 * - the function should return -1 on error and zero on success
 * - the read or written bytes will go to in->size or out->size
 */
typedef int (fnRead) (void *args, LZFSEMT_Buffer * in);
typedef int (fnWrite) (void *args, LZFSEMT_Buffer * out);

typedef struct {
	fnRead *fn_read;
	void *arg_read;
	fnWrite *fn_write;
	void *arg_write;
} LZFSEMT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct LZFSEMT_CCtx_s LZFSEMT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. LZFSEMT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
LZFSEMT_CCtx *LZFSEMT_createCCtx(int threads, int level,/*Not use*/ 
                                   int inputsize);

/**
 * 2) threaded compression
 * - errorcheck via 
 */
size_t LZFSEMT_compressCCtx(LZFSEMT_CCtx * ctx, LZFSEMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZFSEMT_GetFramesCCtx(LZFSEMT_CCtx * ctx);
size_t LZFSEMT_GetInsizeCCtx(LZFSEMT_CCtx * ctx);
size_t LZFSEMT_GetOutsizeCCtx(LZFSEMT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZFSEMT_freeCCtx(LZFSEMT_CCtx * ctx);

/* **************************************
 * Decompression
 ****************************************/

typedef struct LZFSEMT_DCtx_s LZFSEMT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @threads - 1 .. LZFSEMT_THREAD_MAX
 * @ inputsize - used for single threaded standard bro format without 
 *   skippable frames
 */
LZFSEMT_DCtx *LZFSEMT_createDCtx(int threads, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
size_t LZFSEMT_decompressDCtx(LZFSEMT_DCtx * ctx, LZFSEMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZFSEMT_GetFramesDCtx(LZFSEMT_DCtx * ctx);
size_t LZFSEMT_GetInsizeDCtx(LZFSEMT_DCtx * ctx);
size_t LZFSEMT_GetOutsizeDCtx(LZFSEMT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZFSEMT_freeDCtx(LZFSEMT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif

#endif