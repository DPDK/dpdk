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
 * which is not supported natively or named differently in Linux.
 */

#include <sched.h>
#include <sys/queue.h>

/* These macros are compatible with system's sys/queue.h. */
#define RTE_TAILQ_HEAD(name, type) TAILQ_HEAD(name, type)
#define RTE_TAILQ_ENTRY(type) TAILQ_ENTRY(type)
#define RTE_TAILQ_FOREACH(var, head, field) TAILQ_FOREACH(var, head, field)
#define RTE_TAILQ_FIRST(head) TAILQ_FIRST(head)
#define RTE_TAILQ_NEXT(elem, field) TAILQ_NEXT(elem, field)
#define RTE_STAILQ_HEAD(name, type) STAILQ_HEAD(name, type)
#define RTE_STAILQ_ENTRY(type) STAILQ_ENTRY(type)

#ifdef CPU_SETSIZE /* may require _GNU_SOURCE */
typedef cpu_set_t rte_cpuset_t;
#define RTE_HAS_CPUSET
#define RTE_CPU_AND(dst, src1, src2) CPU_AND(dst, src1, src2)
#define RTE_CPU_OR(dst, src1, src2) CPU_OR(dst, src1, src2)
#define RTE_CPU_FILL(set) do \
{ \
	unsigned int i; \
	CPU_ZERO(set); \
	for (i = 0; i < CPU_SETSIZE; i++) \
		CPU_SET(i, set); \
} while (0)
#define RTE_CPU_NOT(dst, src) do \
{ \
	cpu_set_t tmp; \
	RTE_CPU_FILL(&tmp); \
	CPU_XOR(dst, &tmp, src); \
} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_OS_H_ */
