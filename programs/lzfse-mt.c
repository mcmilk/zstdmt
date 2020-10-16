
/**
 * Copyright (c) 2020 @jinfeihan57
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lzfse-mt.h"

#define METHOD   "lzfse"
#define PROGNAME "lzfse-mt"
#define UNZIP    "unlzfse-mt"
#define ZCAT     "lzfsecat-mt"
#define SUFFIX   ".lzfse"

#define LEVEL_DEF          0
#define LEVEL_MIN          0
#define LEVEL_MAX          1
#define THREAD_MAX         LZFSEMT_THREAD_MAX

#define MT_isError         LZFSEMT_isError
#define MT_getErrorString  LZFSEMT_getErrorString
#define MT_Buffer          LZFSEMT_Buffer
#define MT_RdWr_t          LZFSEMT_RdWr_t

#define MT_CCtx            LZFSEMT_CCtx
#define MT_createCCtx      LZFSEMT_createCCtx
#define MT_compressCCtx    LZFSEMT_compressCCtx
#define MT_GetFramesCCtx   LZFSEMT_GetFramesCCtx
#define MT_GetInsizeCCtx   LZFSEMT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  LZFSEMT_GetOutsizeCCtx
#define MT_freeCCtx        LZFSEMT_freeCCtx

#define MT_DCtx            LZFSEMT_DCtx
#define MT_createDCtx      LZFSEMT_createDCtx
#define MT_decompressDCtx  LZFSEMT_decompressDCtx
#define MT_GetFramesDCtx   LZFSEMT_GetFramesDCtx
#define MT_GetInsizeDCtx   LZFSEMT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  LZFSEMT_GetOutsizeDCtx
#define MT_freeDCtx        LZFSEMT_freeDCtx

#include "main.c"
