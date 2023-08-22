/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#ifndef RTE_ATOMIC_LOONGARCH_H
#define RTE_ATOMIC_LOONGARCH_H

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include "generic/rte_atomic.h"

#define rte_mb()	do { asm volatile("dbar 0":::"memory"); } while (0)

#define rte_wmb()	rte_mb()

#define rte_rmb()	rte_mb()

#define rte_smp_mb()	rte_mb()

#define rte_smp_wmb()	rte_mb()

#define rte_smp_rmb()	rte_mb()

#define rte_io_mb()	rte_mb()

#define rte_io_wmb()	rte_mb()

#define rte_io_rmb()	rte_mb()

static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	__rte_atomic_thread_fence(memorder);
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_ATOMIC_LOONGARCH_H */
