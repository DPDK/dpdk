/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ATOMIC_H_
#define _RTE_ATOMIC_H_

/**
 * @file
 * Atomic Operations
 *
 * This file defines a generic API for atomic operations.
 */

#include <stdint.h>

#include <rte_common.h>
#include <rte_stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __DOXYGEN__

/** @name Memory Barrier
 */
///@{
/**
 * General memory barrier.
 *
 * Guarantees that the LOAD and STORE operations generated before the
 * barrier occur before the LOAD and STORE operations generated after.
 */
static inline void rte_mb(void);

/**
 * Write memory barrier.
 *
 * Guarantees that the STORE operations generated before the barrier
 * occur before the STORE operations generated after.
 */
static inline void rte_wmb(void);

/**
 * Read memory barrier.
 *
 * Guarantees that the LOAD operations generated before the barrier
 * occur before the LOAD operations generated after.
 */
static inline void rte_rmb(void);
///@}

/** @name SMP Memory Barrier
 */
///@{
/**
 * General memory barrier between lcores
 *
 * Guarantees that the LOAD and STORE operations that precede the
 * rte_smp_mb() call are globally visible across the lcores
 * before the LOAD and STORE operations that follows it.
 *
 * @note
 *  This function is deprecated.
 *  It provides similar synchronization primitive as atomic fence,
 *  but has different syntax and memory ordering semantic. Hence
 *  deprecated for the simplicity of memory ordering semantics in use.
 *
 *  rte_atomic_thread_fence(rte_memory_order_acq_rel) should be used instead.
 */
static inline void rte_smp_mb(void);

/**
 * Write memory barrier between lcores
 *
 * Guarantees that the STORE operations that precede the
 * rte_smp_wmb() call are globally visible across the lcores
 * before the STORE operations that follows it.
 *
 * @note
 *  This function is deprecated.
 *  It provides similar synchronization primitive as atomic fence,
 *  but has different syntax and memory ordering semantic. Hence
 *  deprecated for the simplicity of memory ordering semantics in use.
 *
 *  rte_atomic_thread_fence(rte_memory_order_release) should be used instead.
 *  The fence also guarantees LOAD operations that precede the call
 *  are globally visible across the lcores before the STORE operations
 *  that follows it.
 */
static inline void rte_smp_wmb(void);

/**
 * Read memory barrier between lcores
 *
 * Guarantees that the LOAD operations that precede the
 * rte_smp_rmb() call are globally visible across the lcores
 * before the LOAD operations that follows it.
 *
 * @note
 *  This function is deprecated.
 *  It provides similar synchronization primitive as atomic fence,
 *  but has different syntax and memory ordering semantic. Hence
 *  deprecated for the simplicity of memory ordering semantics in use.
 *
 *  rte_atomic_thread_fence(rte_memory_order_acquire) should be used instead.
 *  The fence also guarantees LOAD operations that precede the call
 *  are globally visible across the lcores before the STORE operations
 *  that follows it.
 */
static inline void rte_smp_rmb(void);
///@}

/** @name I/O Memory Barrier
 */
///@{
/**
 * General memory barrier for I/O device
 *
 * Guarantees that the LOAD and STORE operations that precede the
 * rte_io_mb() call are visible to I/O device or CPU before the
 * LOAD and STORE operations that follow it.
 */
static inline void rte_io_mb(void);

/**
 * Write memory barrier for I/O device
 *
 * Guarantees that the STORE operations that precede the
 * rte_io_wmb() call are visible to I/O device before the STORE
 * operations that follow it.
 */
static inline void rte_io_wmb(void);

/**
 * Read memory barrier for IO device
 *
 * Guarantees that the LOAD operations on I/O device that precede the
 * rte_io_rmb() call are visible to CPU before the LOAD
 * operations that follow it.
 */
static inline void rte_io_rmb(void);
///@}

#endif /* __DOXYGEN__ */

/**
 * Compiler barrier.
 *
 * Guarantees that operation reordering does not occur at compile time
 * for operations directly before and after the barrier.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define rte_compiler_barrier() _ReadWriteBarrier()
#else
#define	rte_compiler_barrier() do {		\
	asm volatile ("" : : : "memory");	\
} while(0)
#endif

/**
 * Synchronization fence between threads based on the specified memory order.
 */
static inline void rte_atomic_thread_fence(rte_memory_order memorder);


/*------------------------ 128 bit atomic operations -------------------------*/

/**
 * 128-bit integer structure.
 */
typedef struct __rte_aligned(16) {
	union {
		uint64_t val[2];
#ifdef RTE_ARCH_64
#ifndef RTE_TOOLCHAIN_MSVC
		__extension__ __int128 int128;
#endif
#endif
	};
} rte_int128_t;

#ifdef __DOXYGEN__

/**
 * An atomic compare and set function used by the mutex functions.
 * (Atomically) Equivalent to:
 * @code
 *   if (*dst == *exp)
 *     *dst = *src
 *   else
 *     *exp = *dst
 * @endcode
 *
 * @note This function is currently available for the x86-64 and aarch64
 * platforms.
 *
 * @note The success and failure arguments must be one of the __ATOMIC_* values
 * defined in the C++11 standard. For details on their behavior, refer to the
 * standard.
 *
 * @param dst
 *   The destination into which the value will be written.
 * @param exp
 *   Pointer to the expected value. If the operation fails, this memory is
 *   updated with the actual value.
 * @param src
 *   Pointer to the new value.
 * @param weak
 *   A value of true allows the comparison to spuriously fail and allows the
 *   'exp' update to occur non-atomically (i.e. a torn read may occur).
 *   Implementations may ignore this argument and only implement the strong
 *   variant.
 * @param success
 *   If successful, the operation's memory behavior conforms to this (or a
 *   stronger) model.
 * @param failure
 *   If unsuccessful, the operation's memory behavior conforms to this (or a
 *   stronger) model. This argument cannot be rte_memory_order_release,
 *   rte_memory_order_acq_rel, or a stronger model than success.
 * @return
 *   Non-zero on success; 0 on failure.
 */
static inline int
rte_atomic128_cmp_exchange(rte_int128_t *dst,
			   rte_int128_t *exp,
			   const rte_int128_t *src,
			   unsigned int weak,
			   int success,
			   int failure);

#endif /* __DOXYGEN__ */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_H_ */
