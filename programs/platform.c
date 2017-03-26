
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

#include "platform.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
int gettimeofday(struct timeval *tp, void *tzp)
{
	DWORD t = timeGetTime();

	(void)tzp;
	tp->tv_sec = t / 1000;
	tp->tv_usec = t % 1000;

	return 0;
}

int getcpucount(void)
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}

int getrusage(int who, struct rusage *uv_rusage)
{
	FILETIME createTime, exitTime, kernelTime, userTime;
	SYSTEMTIME kernelSystemTime, userSystemTime;
	PROCESS_MEMORY_COUNTERS memCounters;
	IO_COUNTERS ioCounters;
	int ret;

	(void)who;

	ret =
	    GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime,
			    &kernelTime, &userTime);
	if (ret == 0)
		return -1;

	ret = FileTimeToSystemTime(&kernelTime, &kernelSystemTime);
	if (ret == 0)
		return -1;

	ret = FileTimeToSystemTime(&userTime, &userSystemTime);
	if (ret == 0)
		return -1;

	ret =
	    GetProcessMemoryInfo(GetCurrentProcess(), &memCounters,
				 sizeof(memCounters));
	if (ret == 0)
		return -1;

	ret = GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
	if (ret == 0)
		return -1;

	memset(uv_rusage, 0, sizeof(*uv_rusage));

	uv_rusage->ru_utime.tv_sec =
	    userSystemTime.wHour * 3600 + userSystemTime.wMinute * 60 +
	    userSystemTime.wSecond;
	uv_rusage->ru_utime.tv_usec = userSystemTime.wMilliseconds * 1000;

	uv_rusage->ru_stime.tv_sec =
	    kernelSystemTime.wHour * 3600 + kernelSystemTime.wMinute * 60 +
	    kernelSystemTime.wSecond;
	uv_rusage->ru_stime.tv_usec = kernelSystemTime.wMilliseconds * 1000;

	uv_rusage->ru_majflt = memCounters.PageFaultCount;
	uv_rusage->ru_maxrss = memCounters.PeakWorkingSetSize / 1024;

	uv_rusage->ru_oublock = ioCounters.WriteOperationCount;
	uv_rusage->ru_inblock = ioCounters.ReadOperationCount;

	return 0;
}
#else
/* POSIX */
int getcpucount(void)
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif
