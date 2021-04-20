/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

/**
 * This header should contain any definition
 * which is not supported natively or named differently in FreeBSD.
 */

#include <pthread_np.h>

typedef cpuset_t rte_cpuset_t;
#define RTE_HAS_CPUSET
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
