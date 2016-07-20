
/**
 * Copyright © 2016 Tino Reichardt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zbuff.h>

/* **************************************
*  Thread functions
****************************************/

/**
 * Format Definition
 *
 * 2 bytes format header:
 * - 2^14 bits: threads count
 *
 * 4 bytes chunk header:
 * - 2^14 bits: for thread count
 * - 2^18 bits: for compressed length
 */

#define ZSTDMT_THREADMAX 16384

typedef struct
{
	void *src;
	void *dst;
} ZSTDMT_CCtx;

typedef struct
{
	void *src;
	void *dst;
} ZSTDMT_DCtx;

/* returns new cctx or zero on error */
ZSTDMT_CCtx *ZSTDMT_createCCtx(size_t threads, int level);

/* returns new dctx or zero on error */
ZSTDMT_DCtx *ZSTDMT_createDCtx(size_t threads);
