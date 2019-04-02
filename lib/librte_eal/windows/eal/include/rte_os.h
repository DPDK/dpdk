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

#include <BaseTsd.h>

#define strerror_r(a, b, c) strerror_s(b, c, a)

/* strdup is deprecated in Microsoft libc and _strdup is preferred */
#define strdup(str) _strdup(str)

typedef SSIZE_T ssize_t;

#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
