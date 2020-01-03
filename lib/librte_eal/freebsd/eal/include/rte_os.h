/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

/**
 * This is header should contain any function/macro definition
 * which are not supported natively or named differently in the
 * freebsd OS. Functions will be added in future releases.
 */

#include <pthread_np.h>

typedef cpuset_t rte_cpuset_t;
#define RTE_CPU_AND(dst, src1, src2) do \
{ \
	cpuset_t tmp; \
	CPU_COPY(src1, &tmp); \
	CPU_AND(&tmp, src2); \
	CPU_COPY(&tmp, dst); \
} while (0)
#define RTE_CPU_OR(dst, src1, src2) do \
{ \
	cpuset_t tmp; \
	CPU_COPY(src1, &tmp); \
	CPU_OR(&tmp, src2); \
	CPU_COPY(&tmp, dst); \
} while (0)
#define RTE_CPU_FILL(set) CPU_FILL(set)

/* In FreeBSD 13 CPU_NAND macro is CPU_ANDNOT */
#ifdef CPU_NAND
#define RTE_CPU_NOT(dst, src) do \
{ \
	cpuset_t tmp; \
	CPU_FILL(&tmp); \
	CPU_NAND(&tmp, src); \
	CPU_COPY(&tmp, dst); \
} while (0)
#else
#define RTE_CPU_NOT(dst, src) do \
{ \
	cpuset_t tmp; \
	CPU_FILL(&tmp); \
	CPU_ANDNOT(&tmp, src); \
	CPU_COPY(&tmp, dst); \
} while (0)
#endif

#endif /* _RTE_OS_H_ */
