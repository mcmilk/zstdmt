
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

/* will be used for lib errors */
size_t zstdmt_errcode;

/*-****************************************
*  ZSTDMT Error Management
******************************************/
/*! ZSTDMT_isError() :
*   tells if a return value is an error code */
unsigned ZSTDMT_isError(size_t code)
{
	return (code > ZSTDMT_error_maxCode);
}

/*! ZSTDMT_getErrorString() :
*   provides error code string from function result (useful for debugging) */
const char *ZSTDMT_getErrorString(size_t code)
{
    if (ZSTD_isError(zstdmt_errcode))
	    return ZSTDMT_getErrorString(zstdmt_errcode);

    static const char* notErrorCode = "Unspecified error code";
    switch( code )
    {
    case PREFIX(no_error): return "No error detected";
    case PREFIX(memory_allocation): return "Allocation error : not enough memory";
    case PREFIX(read_fail): return "Read failure";
    case PREFIX(write_fail): return "Write failure";
    case PREFIX(data_error): return "Malformed input";
    case PREFIX(frame_compress): return "Could not compress frame at once";
    case PREFIX(frame_decompress): return "Could not decompress frame at once";
    case PREFIX(compressionParameter_unsupported): return "Compression parameter is out of bound";
    case PREFIX(compression_library): return "Compression library reports failure";
    case PREFIX(maxCode):
    default: return notErrorCode;
    }
}
