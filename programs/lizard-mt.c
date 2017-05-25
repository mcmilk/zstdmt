
/**
 * Copyright (c) 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lizard-mt.h"

#define METHOD   "lizard"
#define PROGNAME "lizard-mt"
#define UNZIP    "unlizard-mt"
#define ZCAT     "lizardcat-mt"
#define SUFFIX   ".liz"

#define LEVEL_DEF          17
#define LEVEL_MIN          LIZARDMT_LEVEL_MIN
#define LEVEL_MAX          LIZARDMT_LEVEL_MAX
#define THREAD_MAX         LIZARDMT_THREAD_MAX

#define MT_isError         LIZARDMT_isError
#define MT_getErrorString  LIZARDMT_getErrorString
#define MT_Buffer          LIZARDMT_Buffer
#define MT_RdWr_t          LIZARDMT_RdWr_t

#define MT_CCtx            LIZARDMT_CCtx
#define MT_createCCtx      LIZARDMT_createCCtx
#define MT_compressCCtx    LIZARDMT_compressCCtx
#define MT_GetFramesCCtx   LIZARDMT_GetFramesCCtx
#define MT_GetInsizeCCtx   LIZARDMT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  LIZARDMT_GetOutsizeCCtx
#define MT_freeCCtx        LIZARDMT_freeCCtx

#define MT_DCtx            LIZARDMT_DCtx
#define MT_createDCtx      LIZARDMT_createDCtx
#define MT_decompressDCtx  LIZARDMT_decompressDCtx
#define MT_GetFramesDCtx   LIZARDMT_GetFramesDCtx
#define MT_GetInsizeDCtx   LIZARDMT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  LIZARDMT_GetOutsizeDCtx
#define MT_freeDCtx        LIZARDMT_freeDCtx

#include "main.c"
