/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#ifndef RTE_PREFETCH_LOONGARCH_H
#define RTE_PREFETCH_LOONGARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include <rte_common.h>
#include "generic/rte_prefetch.h"

static inline void rte_prefetch0(const volatile void *p)
{
	__builtin_prefetch((const void *)(uintptr_t)p, 0, 3);
}

static inline void rte_prefetch1(const volatile void *p)
{
	__builtin_prefetch((const void *)(uintptr_t)p, 0, 2);
}

static inline void rte_prefetch2(const volatile void *p)
{
	__builtin_prefetch((const void *)(uintptr_t)p, 0, 1);
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

#endif /* RTE_PREFETCH_LOONGARCH_H */
