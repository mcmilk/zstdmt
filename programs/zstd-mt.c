
/**
 * Copyright (c) 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "zstd-mt.h"

#define METHOD   "zstd"
#define PROGNAME "zstd-mt"
#define UNZIP    "unzstd-mt"
#define ZCAT     "zstdcat-mt"
#define SUFFIX   ".zst"

#define LEVEL_DEF          3
#define LEVEL_MIN          ZSTDCB_LEVEL_MIN
#define LEVEL_MAX          ZSTDCB_LEVEL_MAX
#define THREAD_MAX         ZSTDCB_THREAD_MAX

#define MT_isError         ZSTDCB_isError
#define MT_getErrorString  ZSTDCB_getErrorString
#define MT_Buffer          ZSTDCB_Buffer
#define MT_RdWr_t          ZSTDCB_RdWr_t

#define MT_CCtx            ZSTDCB_CCtx
#define MT_createCCtx      ZSTDCB_createCCtx
#define MT_compressCCtx    ZSTDCB_compressCCtx
#define MT_GetFramesCCtx   ZSTDCB_GetFramesCCtx
#define MT_GetInsizeCCtx   ZSTDCB_GetInsizeCCtx
#define MT_GetOutsizeCCtx  ZSTDCB_GetOutsizeCCtx
#define MT_freeCCtx        ZSTDCB_freeCCtx

#define MT_DCtx            ZSTDCB_DCtx
#define MT_createDCtx      ZSTDCB_createDCtx
#define MT_decompressDCtx  ZSTDCB_decompressDCtx
#define MT_GetFramesDCtx   ZSTDCB_GetFramesDCtx
#define MT_GetInsizeDCtx   ZSTDCB_GetInsizeDCtx
#define MT_GetOutsizeDCtx  ZSTDCB_GetOutsizeDCtx
#define MT_freeDCtx        ZSTDCB_freeDCtx

#include "main.c"
