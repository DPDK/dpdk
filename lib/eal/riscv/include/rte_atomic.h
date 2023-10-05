/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 * All rights reserved.
 */

#ifndef RTE_ATOMIC_RISCV_H
#define RTE_ATOMIC_RISCV_H

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_common.h>
#include <rte_config.h>
#include "generic/rte_atomic.h"

#define rte_mb()	asm volatile("fence rw, rw" : : : "memory")

#define rte_wmb()	asm volatile("fence w, w" : : : "memory")

#define rte_rmb()	asm volatile("fence r, r" : : : "memory")

#define rte_smp_mb()	rte_mb()

#define rte_smp_wmb()	rte_wmb()

#define rte_smp_rmb()	rte_rmb()

#define rte_io_mb()	asm volatile("fence iorw, iorw" : : : "memory")

#define rte_io_wmb()	asm volatile("fence orw, ow" : : : "memory")

#define rte_io_rmb()	asm volatile("fence ir, ir" : : : "memory")

static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	__rte_atomic_thread_fence(memorder);
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_ATOMIC_RISCV_H */
