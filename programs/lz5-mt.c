
/**
 * Copyright (c) 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lz5-mt.h"

#define METHOD   "lz5"
#define PROGNAME "lz5-mt"
#define UNZIP    "unlz5-mt"
#define ZCAT     "lz5cat-mt"
#define SUFFIX   ".lz5"

#define LEVEL_DEF          3
#define LEVEL_MIN          LZ5MT_LEVEL_MIN
#define LEVEL_MAX          LZ5MT_LEVEL_MAX
#define THREAD_MAX         LZ5MT_THREAD_MAX

#define MT_isError         LZ5MT_isError
#define MT_getErrorString  LZ5MT_getErrorString
#define MT_Buffer          LZ5MT_Buffer
#define MT_RdWr_t          LZ5MT_RdWr_t

#define MT_CCtx            LZ5MT_CCtx
#define MT_createCCtx      LZ5MT_createCCtx
#define MT_compressCCtx    LZ5MT_compressCCtx
#define MT_GetFramesCCtx   LZ5MT_GetFramesCCtx
#define MT_GetInsizeCCtx   LZ5MT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  LZ5MT_GetOutsizeCCtx
#define MT_freeCCtx        LZ5MT_freeCCtx

#define MT_DCtx            LZ5MT_DCtx
#define MT_createDCtx      LZ5MT_createDCtx
#define MT_decompressDCtx  LZ5MT_decompressDCtx
#define MT_GetFramesDCtx   LZ5MT_GetFramesDCtx
#define MT_GetInsizeDCtx   LZ5MT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  LZ5MT_GetOutsizeDCtx
#define MT_freeDCtx        LZ5MT_freeDCtx

#include "main.c"
