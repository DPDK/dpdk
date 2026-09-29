/* SPDX-License-Identifier: BSD-3-Clause
 * Inspired from FreeBSD src/sys/powerpc/include/atomic.h
 * Copyright (c) 2021 IBM Corporation
 * Copyright (c) 2008 Marcel Moolenaar
 * Copyright (c) 2001 Benno Rice
 * Copyright (c) 2001 David E. O'Brien
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 */

#ifndef _RTE_ATOMIC_PPC_64_H_
#define _RTE_ATOMIC_PPC_64_H_

#include <stdint.h>
#include "generic/rte_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	rte_mb()  asm volatile("sync" : : : "memory")

#define	rte_wmb() asm volatile("sync" : : : "memory")

#define	rte_rmb() asm volatile("sync" : : : "memory")

#define rte_smp_mb() rte_mb()

#define rte_smp_wmb() rte_wmb()

#define rte_smp_rmb() rte_rmb()

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_wmb()

#define rte_io_rmb() rte_rmb()

static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	__rte_atomic_thread_fence(memorder);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_PPC_64_H_ */
