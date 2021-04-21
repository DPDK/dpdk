/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RTE_OS_SHIM_
#define _RTE_OS_SHIM_

#include <time.h>

#include <rte_os.h>
#include <rte_windows.h>

/**
 * @file
 * @internal
 * Provides semi-standard OS facilities by convenient names.
 */

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define strdup(str) _strdup(str)
#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#ifndef RTE_TOOLCHAIN_GCC
#define strncasecmp(s1, s2, count) _strnicmp(s1, s2, count)
#endif

#define open(path, flags, ...) _open(path, flags, ##__VA_ARGS__)
#define read(fd, buf, n) _read(fd, buf, n)
#define write(fd, buf, n) _write(fd, buf, n)
#define close(fd) _close(fd)
#define unlink(path) _unlink(path)

#define IPVERSION	4

#define IPPROTO_IPIP	4
#define IPPROTO_GRE	47
#ifdef RTE_TOOLCHAIN_GCC
#define IPPROTO_SCTP	132
#endif

#ifdef RTE_TOOLCHAIN_GCC

#define TIME_UTC 1

static inline int
rte_timespec_get(struct timespec *now, int base)
{
	/* 100ns ticks from 1601-01-01 to 1970-01-01 */
	static const uint64_t EPOCH = 116444736000000000ULL;
	static const uint64_t TICKS_PER_SEC = 10000000;
	static const uint64_t NS_PER_TICK = 100;

	FILETIME ft;
	uint64_t ticks;

	if (base != TIME_UTC)
		return 0;

	GetSystemTimePreciseAsFileTime(&ft);
	ticks = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	ticks -= EPOCH;
	now->tv_sec = ticks / TICKS_PER_SEC;
	now->tv_nsec = (ticks - now->tv_sec * TICKS_PER_SEC) * NS_PER_TICK;
	return base;
}

#define timespec_get(ts, base) rte_timespec_get(ts, base)

#endif /* RTE_TOOLCHAIN_GCC */

#endif /* _RTE_OS_SHIM_ */
