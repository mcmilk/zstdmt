
/**
 * Copyright (c) 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lz4-mt.h"

#define METHOD   "lz4"
#define PROGNAME "lz4-mt"
#define UNZIP    "unlz4-mt"
#define ZCAT     "lz4cat-mt"
#define SUFFIX   ".lz4"

#define LEVEL_DEF          3
#define LEVEL_MIN          LZ4MT_LEVEL_MIN
#define LEVEL_MAX          LZ4MT_LEVEL_MAX
#define THREAD_MAX         LZ4MT_THREAD_MAX

#define MT_isError         LZ4MT_isError
#define MT_getErrorString  LZ4MT_getErrorString
#define MT_Buffer          LZ4MT_Buffer
#define MT_RdWr_t          LZ4MT_RdWr_t

#define MT_CCtx            LZ4MT_CCtx
#define MT_createCCtx      LZ4MT_createCCtx
#define MT_compressCCtx    LZ4MT_compressCCtx
#define MT_GetFramesCCtx   LZ4MT_GetFramesCCtx
#define MT_GetInsizeCCtx   LZ4MT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  LZ4MT_GetOutsizeCCtx
#define MT_freeCCtx        LZ4MT_freeCCtx

#define MT_DCtx            LZ4MT_DCtx
#define MT_createDCtx      LZ4MT_createDCtx
#define MT_decompressDCtx  LZ4MT_decompressDCtx
#define MT_GetFramesDCtx   LZ4MT_GetFramesDCtx
#define MT_GetInsizeDCtx   LZ4MT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  LZ4MT_GetOutsizeDCtx
#define MT_freeDCtx        LZ4MT_freeDCtx

#include "main.c"
