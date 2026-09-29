/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ATOMIC_X86_H_
#define _RTE_ATOMIC_X86_H_

#include <stdint.h>
#include <rte_common.h>
#include <rte_config.h>
#include <emmintrin.h>
#include "generic/rte_atomic.h"

#if RTE_MAX_LCORE == 1
#define MPLOCKED                        /**< No need to insert MP lock prefix. */
#else
#define MPLOCKED        "lock ; "       /**< Insert MP lock prefix. */
#endif

#define	rte_mb() _mm_mfence()

#define	rte_wmb() _mm_sfence()

#define	rte_rmb() _mm_lfence()

#define rte_smp_wmb() rte_compiler_barrier()

#define rte_smp_rmb() rte_compiler_barrier()

#ifdef __cplusplus
extern "C" {
#endif

/*
 * From Intel Software Development Manual; Vol 3;
 * 8.2.2 Memory Ordering in P6 and More Recent Processor Families:
 * ...
 * . Reads are not reordered with other reads.
 * . Writes are not reordered with older reads.
 * . Writes to memory are not reordered with other writes,
 *   with the following exceptions:
 *   . streaming stores (writes) executed with the non-temporal move
 *     instructions (MOVNTI, MOVNTQ, MOVNTDQ, MOVNTPS, and MOVNTPD); and
 *   . string operations (see Section 8.2.4.1).
 *  ...
 * . Reads may be reordered with older writes to different locations but not
 * with older writes to the same location.
 * . Reads or writes cannot be reordered with I/O instructions,
 * locked instructions, or serializing instructions.
 * . Reads cannot pass earlier LFENCE and MFENCE instructions.
 * . Writes ... cannot pass earlier LFENCE, SFENCE, and MFENCE instructions.
 * . LFENCE instructions cannot pass earlier reads.
 * . SFENCE instructions cannot pass earlier writes ...
 * . MFENCE instructions cannot pass earlier reads, writes ...
 *
 * As pointed by Java guys, that makes possible to use lock-prefixed
 * instructions to get the same effect as mfence and on most modern HW
 * that gives a better performance then using mfence:
 * https://shipilev.net/blog/2014/on-the-fence-with-dependencies/
 * Basic idea is to use lock prefixed add with some dummy memory location
 * as the destination. From their experiments 128B(2 cache lines) below
 * current stack pointer looks like a good candidate.
 * So below we use that technique for rte_smp_mb() implementation.
 */

static __rte_always_inline void
rte_smp_mb(void)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_mfence();
#else
#ifdef RTE_ARCH_I686
	asm volatile("lock addl $0, -128(%%esp); " ::: "memory");
#else
	asm volatile("lock addl $0, -128(%%rsp); " ::: "memory");
#endif
#endif
}

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_compiler_barrier()

#define rte_io_rmb() rte_compiler_barrier()

/**
 * Synchronization fence between threads based on the specified memory order.
 *
 * On x86 the __rte_atomic_thread_fence(rte_memory_order_seq_cst) generates full 'mfence'
 * which is quite expensive. The optimized implementation of rte_smp_mb is
 * used instead.
 */
static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	if (memorder == rte_memory_order_seq_cst)
		rte_smp_mb();
	else
		__rte_atomic_thread_fence(memorder);
}

#ifdef __cplusplus
}
#endif

#ifndef RTE_TOOLCHAIN_MSVC

#ifndef RTE_ARCH_I686
#include "rte_atomic_64.h"
#endif

#endif /* !RTE_TOOLCHAIN_MSVC */

#endif /* _RTE_ATOMIC_X86_H_ */
