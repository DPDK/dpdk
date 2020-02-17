/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

/**
 * This is header should contain any function/macro definition
 * which are not supported natively or named differently in the
 * Windows OS. Functions will be added in future releases.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Windows.h>
#include <BaseTsd.h>
#include <pthread.h>
#include <stdio.h>

/* limits.h replacement */
#include <stdlib.h>
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define strerror_r(a, b, c) strerror_s(b, c, a)

/* strdup is deprecated in Microsoft libc and _strdup is preferred */
#define strdup(str) _strdup(str)

typedef SSIZE_T ssize_t;

#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)

#define index(a, b)     strchr(a, b)
#define rindex(a, b)    strrchr(a, b)

#define strncasecmp(s1, s2, count)        _strnicmp(s1, s2, count)

/**
 * Create a thread.
 * This function is private to EAL.
 *
 * @param thread
 *   The location to store the thread id if successful.
 * @return
 *   0 for success, -1 if the thread is not created.
 */
int eal_thread_create(pthread_t *thread);

/**
 * Create a map of processors and cores on the system.
 * This function is private to EAL.
 */
void eal_create_cpu_map(void);

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

/* cpu_set macros implementation */
#define RTE_CPU_AND(dst, src1, src2) CPU_AND(dst, src1, src2)
#define RTE_CPU_OR(dst, src1, src2) CPU_OR(dst, src1, src2)
#define RTE_CPU_FILL(set) CPU_FILL(set)
#define RTE_CPU_NOT(dst, src) CPU_NOT(dst, src)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
