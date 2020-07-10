/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

/**
 * This is header should contain any function/macro definition
 * which are not supported natively or named differently in the
 * Windows OS. It must not include Windows-specific headers.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* limits.h replacement, value as in <windows.h> */
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

/* sys/mman.h
 * The syscall mmap does not exist on Windows,
 * but this error code is used in a badly defined DPDK API for PCI mapping.
 */
#define MAP_FAILED ((void *) -1)

#define sleep(x) Sleep(1000 * (x))

#define strerror_r(a, b, c) strerror_s(b, c, a)

/* strdup is deprecated in Microsoft libc and _strdup is preferred */
#define strdup(str) _strdup(str)

#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)

#define index(a, b)     strchr(a, b)
#define rindex(a, b)    strrchr(a, b)

#define strncasecmp(s1, s2, count)        _strnicmp(s1, s2, count)

#define close _close
#define unlink _unlink

/* cpu_set macros implementation */
#define RTE_CPU_AND(dst, src1, src2) CPU_AND(dst, src1, src2)
#define RTE_CPU_OR(dst, src1, src2) CPU_OR(dst, src1, src2)
#define RTE_CPU_FILL(set) CPU_FILL(set)
#define RTE_CPU_NOT(dst, src) CPU_NOT(dst, src)

/* as in <windows.h> */
typedef long long ssize_t;

#ifndef RTE_TOOLCHAIN_GCC

static inline int
asprintf(char **buffer, const char *format, ...)
{
	int size, ret;
	va_list arg;

	va_start(arg, format);
	size = vsnprintf(NULL, 0, format, arg);
	va_end(arg);
	if (size < 0)
		return -1;
	size++;

	*buffer = malloc(size);
	if (*buffer == NULL)
		return -1;

	va_start(arg, format);
	ret = vsnprintf(*buffer, size, format, arg);
	va_end(arg);
	if (ret != size - 1) {
		free(*buffer);
		return -1;
	}
	return ret;
}

static inline const char *
eal_strerror(int code)
{
	static char buffer[128];

	strerror_s(buffer, sizeof(buffer), code);
	return buffer;
}

#define strerror eal_strerror

#endif /* RTE_TOOLCHAIN_GCC */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
