/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_OS_H_
#define _RTE_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This header should contain any definition
 * which is not supported natively or named differently in FreeBSD.
 */

#include <pthread_np.h>
#include <sys/queue.h>

/* These macros are compatible with system's sys/queue.h. */
#define RTE_TAILQ_HEAD(name, type) TAILQ_HEAD(name, type)
#define RTE_TAILQ_ENTRY(type) TAILQ_ENTRY(type)
#define RTE_TAILQ_FOREACH(var, head, field) TAILQ_FOREACH(var, head, field)
#define RTE_TAILQ_FIRST(head) TAILQ_FIRST(head)
#define RTE_TAILQ_NEXT(elem, field) TAILQ_NEXT(elem, field)
#define RTE_STAILQ_HEAD(name, type) STAILQ_HEAD(name, type)
#define RTE_STAILQ_ENTRY(type) STAILQ_ENTRY(type)

typedef cpuset_t rte_cpuset_t;
#define RTE_HAS_CPUSET

#ifdef RTE_EAL_FREEBSD_CPUSET_LEGACY
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
#endif /* CPU_NAND */

#else /* RTE_EAL_FREEBSD_CPUSET_LEGACY */

#define RTE_CPU_AND CPU_AND
#define RTE_CPU_OR CPU_OR
#define RTE_CPU_FILL CPU_FILL
#define RTE_CPU_NOT(dst, src) do { \
	cpu_set_t tmp; \
	CPU_FILL(&tmp); \
	CPU_XOR(dst, src, &tmp); \
} while (0)

#endif /* RTE_EAL_FREEBSD_CPUSET_LEGACY */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
