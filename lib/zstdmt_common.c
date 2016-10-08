
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

/**
 * ZSTDMT_isError() - tells if a return value is an error code
 */
unsigned ZSTDMT_isError(size_t code)
{
	return (code > ERROR(maxCode));
}

/**
 * ZSTDMT_getErrorString() - give error code string from function result
 */
const char *ZSTDMT_getErrorString(size_t code, size_t zcode)
{
	if (ZSTD_isError(zstdmt_errcode))
		return ZSTD_getErrorName(zstdmt_errcode);

	static const char *notErrorCode = "Unspecified error lz4mt code";
	switch ((ZSTDMT_ErrorCode)(0-code)) {
	case PREFIX(no_error):
		return "No error detected";
	case PREFIX(memory_allocation):
		return "Allocation error : not enough memory";
	case PREFIX(init_missing):
		return "Context should be init first";
	case PREFIX(read_fail):
		return "Read failure";
	case PREFIX(write_fail):
		return "Write failure";
	case PREFIX(data_error):
		return "Malformed input";
	case PREFIX(frame_compress):
		return "Could not compress frame at once";
	case PREFIX(frame_decompress):
		return "Could not decompress frame at once";
	case PREFIX(compressionParameter_unsupported):
		return "Compression parameter is out of bound";
	case PREFIX(compression_library):
		return "Compression library reports failure";
	case PREFIX(maxCode):
	default:
		return notErrorCode;
	}
}
