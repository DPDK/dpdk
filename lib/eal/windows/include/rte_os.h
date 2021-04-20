/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

/**
 * This header should contain any definition
 * which is not supported natively or named differently in Windows.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* cpu_set macros implementation */
#define RTE_CPU_AND(dst, src1, src2) CPU_AND(dst, src1, src2)
#define RTE_CPU_OR(dst, src1, src2) CPU_OR(dst, src1, src2)
#define RTE_CPU_FILL(set) CPU_FILL(set)
#define RTE_CPU_NOT(dst, src) CPU_NOT(dst, src)

/* This is an exception without "rte_" prefix, because Windows does have
 * ssize_t, but it's defined in <windows.h> which we avoid to expose.
 * If ssize_t is defined in user code, it necessarily has the same type.
 */
typedef long long ssize_t;

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
