/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 RehiveTech. All rights reserved.
 */

#ifndef _RTE_ATOMIC_ARM32_H_
#define _RTE_ATOMIC_ARM32_H_

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with CONFIG_RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_atomic.h"

/**
 * General memory barrier.
 *
 * Guarantees that the LOAD and STORE operations generated before the
 * barrier occur before the LOAD and STORE operations generated after.
 */
#define	rte_mb()  __sync_synchronize()

/**
 * Write memory barrier.
 *
 * Guarantees that the STORE operations generated before the barrier
 * occur before the STORE operations generated after.
 */
#define	rte_wmb() do { asm volatile ("dmb st" : : : "memory"); } while (0)

/**
 * Read memory barrier.
 *
 * Guarantees that the LOAD operations generated before the barrier
 * occur before the LOAD operations generated after.
 */
#define	rte_rmb() __sync_synchronize()

#define rte_smp_mb() rte_mb()

#define rte_smp_wmb() rte_wmb()

#define rte_smp_rmb() rte_rmb()

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_wmb()

#define rte_io_rmb() rte_rmb()

#define rte_cio_wmb() rte_wmb()

#define rte_cio_rmb() rte_rmb()

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_ARM32_H_ */
