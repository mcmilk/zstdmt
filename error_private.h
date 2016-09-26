
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/* Note : this module is expected to remain private, do not expose it */

#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/* ****************************************
*  Dependencies
******************************************/
#include <stddef.h>        /* size_t */
#include "error_public.h"  /* enum list */


/* ****************************************
*  Compiler-specific
******************************************/
#if defined(__GNUC__)
#  define ERR_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define ERR_STATIC static inline
#elif defined(_MSC_VER)
#  define ERR_STATIC static __inline
#else
#  define ERR_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif


/*-****************************************
*  Customization (error_public.h)
******************************************/
typedef ZSTDMT_ErrorCode ERR_enum;
#define PREFIX(name) ZSTDMT_error_##name


/*-****************************************
*  Error codes handling
******************************************/
#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#endif
#define ERROR(name) ((size_t)-PREFIX(name))

ERR_STATIC unsigned ERR_isError(size_t code) { return (code > ERROR(maxCode)); }
ERR_STATIC ERR_enum ERR_getErrorCode(size_t code) { if (!ERR_isError(code)) return (ERR_enum)0; return (ERR_enum) (0-code); }


/*-****************************************
*  Error Strings
******************************************/

ERR_STATIC const char* ERR_getErrorString(ERR_enum code)
{
    static const char* notErrorCode = "Unspecified error code";
    switch( code )
    {
    case PREFIX(no_error): return "No error detected";
    case PREFIX(memory_allocation): return "Allocation error : not enough memory";
    case PREFIX(read_fail): return "Read failure";
    case PREFIX(write_fail): return "Write failure";
    case PREFIX(data_error): return "Malformed input";
    case PREFIX(frame_decompress): return "Could not decompress Frame at once";
    case PREFIX(compressionParameter_unsupported): return "Compression parameter is out of bound";
    case PREFIX(compression_library): return "Call LIB_getErrorString(LIB_errcode)"; /* these should not happen */
    case PREFIX(maxCode):
    default: return notErrorCode;
    }
}

ERR_STATIC const char* ERR_getErrorName(size_t code)
{
    return ERR_getErrorString(ERR_getErrorCode(code));
}

#if defined (__cplusplus)
}
#endif

#endif /* ERROR_H_MODULE */
