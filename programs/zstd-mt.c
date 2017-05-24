
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
#define LEVEL_MIN          ZSTDMT_LEVEL_MIN
#define LEVEL_MAX          ZSTDMT_LEVEL_MAX
#define THREAD_MAX         ZSTDMT_THREAD_MAX

#define MT_isError         ZSTDMT_isError
#define MT_getErrorString  ZSTDMT_getErrorString
#define MT_Buffer          ZSTDMT_Buffer
#define MT_RdWr_t          ZSTDMT_RdWr_t

#define MT_CCtx            ZSTDMT_CCtx
#define MT_createCCtx      ZSTDMT_createCCtx
#define MT_compressCCtx    ZSTDMT_compressCCtx
#define MT_GetFramesCCtx   ZSTDMT_GetFramesCCtx
#define MT_GetInsizeCCtx   ZSTDMT_GetInsizeCCtx
#define MT_GetOutsizeCCtx  ZSTDMT_GetOutsizeCCtx
#define MT_freeCCtx        ZSTDMT_freeCCtx

#define MT_DCtx            ZSTDMT_DCtx
#define MT_createDCtx      ZSTDMT_createDCtx
#define MT_decompressDCtx  ZSTDMT_decompressDCtx
#define MT_GetFramesDCtx   ZSTDMT_GetFramesDCtx
#define MT_GetInsizeDCtx   ZSTDMT_GetInsizeDCtx
#define MT_GetOutsizeDCtx  ZSTDMT_GetOutsizeDCtx
#define MT_freeDCtx        ZSTDMT_freeDCtx

#include "main.c"
