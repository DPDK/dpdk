/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 IBM Corporation
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_PREFETCH_RISCV_H
#define RTE_PREFETCH_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include <rte_common.h>
#include "generic/rte_prefetch.h"

static inline void rte_prefetch0(const volatile void *p)
{
	RTE_SET_USED(p);
}

static inline void rte_prefetch1(const volatile void *p)
{
	RTE_SET_USED(p);
}

static inline void rte_prefetch2(const volatile void *p)
{
	RTE_SET_USED(p);
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

#endif /* RTE_PREFETCH_RISCV_H */
