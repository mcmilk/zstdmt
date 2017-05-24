
/**
 * Copyright (c) 2016 - 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "../lib/memmt.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

extern int getcpucount(void);

#define _FILE_OFFSET_BITS 64

#if defined(_MSC_VER) || defined(__MINGW32__)

/* Windows */
#include <windows.h>
#include <processthreadsapi.h> /* GetProcessTimes() */
#include <timezoneapi.h> /* FileTimeToSystemTime() */
#include <psapi.h> /* GetProcessMemoryInfo() */
#include <io.h> /* _isatty */

#define DEVNULL "NUL"
#define PATH_SEPERATOR '\\'
#define SET_BINARY(file) setmode(fileno(file),O_BINARY)
#define IS_CONSOLE(stdStream) (_isatty(_fileno(stdStream)))

# ifndef _TIMEVAL_DEFINED
# define _TIMEVAL_DEFINED
struct timeval {
	long tv_sec;
	long tv_usec;
};
# endif
extern int gettimeofday(struct timeval *tp, void *tzp);

#define timersub(a,b,x) do \
{ (x)->tv_sec=(a)->tv_sec-(b)->tv_sec; \
if (((x)->tv_usec=(a)->tv_usec-(b)->tv_usec)<0) \
{ --(x)->tv_sec; (x)->tv_usec+=1000000; } } while (0)

#define RUSAGE_SELF	0
struct rusage {
	struct timeval ru_utime; /* user CPU time used */
	struct timeval ru_stime; /* system CPU time used */
	U64 ru_maxrss;   /* maximum resident set size */
	U64 ru_ixrss;    /* integral shared memory size */
	U64 ru_idrss;    /* integral unshared data size */
	U64 ru_isrss;    /* integral unshared stack size */
	U64 ru_minflt;   /* page reclaims (soft page faults) */
	U64 ru_majflt;   /* page faults (hard page faults) */
	U64 ru_nswap;    /* swaps */
	U64 ru_inblock;  /* block input operations */
	U64 ru_oublock;  /* block output operations */
	U64 ru_msgsnd;   /* IPC messages sent */
	U64 ru_msgrcv;   /* IPC messages received */
	U64 ru_nsignals; /* signals received */
	U64 ru_nvcsw;    /* voluntary context switches */
	U64 ru_nivcsw;   /* involuntary context switches */
};
extern int getrusage(int who, struct rusage *uv_rusage);
#else

/* POSIX */

#include <sys/resource.h> /* getrusage() */
#define DEVNULL "/dev/null"
#define PATH_SEPERATOR '/'
#define SET_BINARY(file)
#define IS_CONSOLE(stdStream) (isatty(fileno(stdStream)))

#endif /* POSIX */

#if defined (__cplusplus)
}
#endif

#endif /* PLATFORM_H */
