/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_ATOMIC_ARM64_H_
#define _RTE_ATOMIC_ARM64_H_

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with CONFIG_RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_atomic.h"

#define dsb(opt) asm volatile("dsb " #opt : : : "memory")
#define dmb(opt) asm volatile("dmb " #opt : : : "memory")

#define rte_mb() dsb(sy)

#define rte_wmb() dsb(st)

#define rte_rmb() dsb(ld)

#define rte_smp_mb() dmb(ish)

#define rte_smp_wmb() dmb(ishst)

#define rte_smp_rmb() dmb(ishld)

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_wmb()

#define rte_io_rmb() rte_rmb()

#define rte_cio_wmb() dmb(oshst)

#define rte_cio_rmb() dmb(oshld)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_ARM64_H_ */
