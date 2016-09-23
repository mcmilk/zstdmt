
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef ERROR_PUBLIC_H_MODULE
#define ERROR_PUBLIC_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/*===== dependency =====*/
#include <stddef.h>   /* size_t */

/*-****************************************
*  error codes list
******************************************/
typedef enum {
  ZSTDMT_error_no_error,
  ZSTDMT_error_memory_allocation,
  ZSTDMT_error_read_fail,
  ZSTDMT_error_write_fail,
  ZSTDMT_error_data_error,
  ZSTDMT_error_frame_decompress,
  ZSTDMT_error_maxCode
} ZSTDMT_ErrorCode;

/*-****************************************
*  ZSTDMT Error Management
******************************************/

/*! ZSTDMT_isError() :
*   tells if a return value is an error code */
extern unsigned ZSTDMT_isError(size_t code);

/*! ZSTDMT_getErrorName() :
*   provides error code string from function result (useful for debugging) */
extern const char* ZSTDMT_getErrorName(size_t code);

/*! ZSTDMT_getError() :
*   convert a `size_t` function result into a proper ZSTDMT_errorCode enum */
extern ZSTDMT_ErrorCode ZSTDMT_getErrorCode(size_t code);

/*! ZSTDMT_getErrorString() :
*   provides error code string from enum */
extern const char* ZSTDMT_getErrorString(ZSTDMT_ErrorCode code);

#if defined (__cplusplus)
}
#endif

#endif /* ERROR_PUBLIC_H_MODULE */
