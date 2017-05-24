
/**
 * Copyright (c) 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "brotli-mt.h"

#define METHOD   "brotli"
#define PROGNAME "brotli-mt"
#define UNZIP    "unbrotli-mt"
#define ZCAT     "brotlicat-mt"
#define SUFFIX   ".brot"

#define LEVEL_DEF          3
#define LEVEL_MIN          BROTLIMT_LEVEL_MIN
#define LEVEL_MAX          BROTLIMT_LEVEL_MAX
#define THREAD_MAX         BROTLIMT_THREAD_MAX

#define MT_isError         BROTLIMT_isError
#define MT_getErrorString  BROTLIMT_getErrorString
#define MT_Buffer          BROTLIMT_Buffer
#define MT_RdWr_t          BROTLIMT_RdWr_t

#define MT_CCtx            BROTLIMT_CCtx
#define MT_createCCtx      BROTLIMT_createCCtx
#define MT_compressCCtx    BROTLIMT_compressCCtx
#define MT_GetFramesCCtx   BROTLIMT_GetFramesCCtx
#define MT_GetInsizeCCtx   BROTLIMT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  BROTLIMT_GetOutsizeCCtx
#define MT_freeCCtx        BROTLIMT_freeCCtx

#define MT_DCtx            BROTLIMT_DCtx
#define MT_createDCtx      BROTLIMT_createDCtx
#define MT_decompressDCtx  BROTLIMT_decompressDCtx
#define MT_GetFramesDCtx   BROTLIMT_GetFramesDCtx
#define MT_GetInsizeDCtx   BROTLIMT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  BROTLIMT_GetOutsizeDCtx
#define MT_freeDCtx        BROTLIMT_freeDCtx

#include "main.c"
