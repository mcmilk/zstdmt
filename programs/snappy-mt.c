
/**
 * Copyright (c) 2020 @jinfeihan57
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "snappy-mt.h"

#define METHOD   "snappy"
#define PROGNAME "snappy-mt"
#define UNZIP    "unsnappy-mt"
#define ZCAT     "snappycat-mt"
#define SUFFIX   ".snp"

#define LEVEL_DEF          0
#define LEVEL_MIN          0
#define LEVEL_MAX          1
#define THREAD_MAX         SNAPPYMT_THREAD_MAX

#define MT_isError         SNAPPYMT_isError
#define MT_getErrorString  SNAPPYMT_getErrorString
#define MT_Buffer          SNAPPYMT_Buffer
#define MT_RdWr_t          SNAPPYMT_RdWr_t

#define MT_CCtx            SNAPPYMT_CCtx
#define MT_createCCtx      SNAPPYMT_createCCtx
#define MT_compressCCtx    SNAPPYMT_compressCCtx
#define MT_GetFramesCCtx   SNAPPYMT_GetFramesCCtx
#define MT_GetInsizeCCtx   SNAPPYMT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  SNAPPYMT_GetOutsizeCCtx
#define MT_freeCCtx        SNAPPYMT_freeCCtx

#define MT_DCtx            SNAPPYMT_DCtx
#define MT_createDCtx      SNAPPYMT_createDCtx
#define MT_decompressDCtx  SNAPPYMT_decompressDCtx
#define MT_GetFramesDCtx   SNAPPYMT_GetFramesDCtx
#define MT_GetInsizeDCtx   SNAPPYMT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  SNAPPYMT_GetOutsizeDCtx
#define MT_freeDCtx        SNAPPYMT_freeDCtx

#include "main.c"
