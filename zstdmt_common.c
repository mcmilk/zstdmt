/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/*-*************************************
*  Dependencies
***************************************/
#include <stdlib.h>         /* malloc */
#include "error_private.h"


/*-****************************************
*  ZSTDMT Error Management
******************************************/
/*! ZSTDMT_isError() :
*   tells if a return value is an error code */
unsigned ZSTDMT_isError(size_t code) { return ERR_isError(code); }

/*! ZSTDMT_getErrorName() :
*   provides error code string from function result (useful for debugging) */
const char* ZSTDMT_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTDMT_getError() :
*   convert a `size_t` function result into a proper ZSTDMT_errorCode enum */
ZSTDMT_ErrorCode ZSTDMT_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTDMT_getErrorString() :
*   provides error code string from enum */
const char* ZSTDMT_getErrorString(ZSTDMT_ErrorCode code) { return ERR_getErrorName(code); }
