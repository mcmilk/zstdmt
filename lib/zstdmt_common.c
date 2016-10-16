
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "zstd.h"
#include "zstdmt.h"

/* ****************************************
 * LZ4MT Error Management
 ******************************************/

size_t zstdmt_errcode;

/**
 * ZSTDMT_isError() - tells if a return value is an error code
 */
unsigned ZSTDMT_isError(size_t code)
{
	return (code > ZSTDMT_ERROR(maxCode));
}
