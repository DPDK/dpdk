/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2014.
 */

#ifndef _RTE_PREFETCH_PPC_64_H_
#define _RTE_PREFETCH_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include <rte_common.h>
#include "generic/rte_prefetch.h"

static inline void rte_prefetch0(const volatile void *p)
{
	asm volatile ("dcbt 0,%[p],0" : : [p] "r" (p));
}

static inline void rte_prefetch1(const volatile void *p)
{
	asm volatile ("dcbt 0,%[p],0" : : [p] "r" (p));
}

static inline void rte_prefetch2(const volatile void *p)
{
	asm volatile ("dcbt 0,%[p],0" : : [p] "r" (p));
}

static inline void rte_prefetch_non_temporal(const volatile void *p)
{
	/* non-temporal version not available, fallback to rte_prefetch0 */
	rte_prefetch0(p);
}

__rte_experimental
static inline void
rte_cldemote(const volatile void *p)
{
	RTE_SET_USED(p);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PREFETCH_PPC_64_H_ */
