
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

#include <stdlib.h>

#include "zstdmt.h"

ZSTDMT_CCtx *ZSTDMT_createCCtx(size_t threads, int level)
{
	ZSTDMT_CCtx *ctx;

	ctx = (ZSTDMT_CCtx *)malloc(sizeof(ZSTDMT_CCtx) * threads);

	return ctx;
}


ZSTDMT_DCtx *ZSTDMT_createDCtx(size_t threads)
{
	ZSTDMT_DCtx *ctx;

	ctx = (ZSTDMT_DCtx *)malloc(sizeof(ZSTDMT_CCtx) * threads);

	return ctx;
}

