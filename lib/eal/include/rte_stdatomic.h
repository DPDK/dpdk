/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Microsoft Corporation
 */

#ifndef RTE_STDATOMIC_H
#define RTE_STDATOMIC_H

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTE_ENABLE_STDATOMIC
#ifdef __STDC_NO_ATOMICS__
#error enable_stdatomic=true but atomics not supported by toolchain
#endif

#include <stdatomic.h>

/* RTE_ATOMIC(type) is provided for use as a type specifier
 * permitting designation of an rte atomic type.
 */
#define RTE_ATOMIC(type) _Atomic(type)

/* __rte_atomic is provided for type qualification permitting
 * designation of an rte atomic qualified type-name.
 */
#define __rte_atomic _Atomic

/* The memory order is an enumerated type in C11. */
typedef memory_order rte_memory_order;

#define rte_memory_order_relaxed memory_order_relaxed
#ifdef __ATOMIC_RELAXED
static_assert(rte_memory_order_relaxed == __ATOMIC_RELAXED,
	"rte_memory_order_relaxed == __ATOMIC_RELAXED");
#endif

#define rte_memory_order_consume memory_order_consume
#ifdef __ATOMIC_CONSUME
static_assert(rte_memory_order_consume == __ATOMIC_CONSUME,
	"rte_memory_order_consume == __ATOMIC_CONSUME");
#endif

#define rte_memory_order_acquire memory_order_acquire
#ifdef __ATOMIC_ACQUIRE
static_assert(rte_memory_order_acquire == __ATOMIC_ACQUIRE,
	"rte_memory_order_acquire == __ATOMIC_ACQUIRE");
#endif

#define rte_memory_order_release memory_order_release
#ifdef __ATOMIC_RELEASE
static_assert(rte_memory_order_release == __ATOMIC_RELEASE,
	"rte_memory_order_release == __ATOMIC_RELEASE");
#endif

#define rte_memory_order_acq_rel memory_order_acq_rel
#ifdef __ATOMIC_ACQ_REL
static_assert(rte_memory_order_acq_rel == __ATOMIC_ACQ_REL,
	"rte_memory_order_acq_rel == __ATOMIC_ACQ_REL");
#endif

#define rte_memory_order_seq_cst memory_order_seq_cst
#ifdef __ATOMIC_SEQ_CST
static_assert(rte_memory_order_seq_cst == __ATOMIC_SEQ_CST,
	"rte_memory_order_seq_cst == __ATOMIC_SEQ_CST");
#endif

#define rte_atomic_load_explicit(ptr, memorder) \
	atomic_load_explicit(ptr, memorder)

#define rte_atomic_store_explicit(ptr, val, memorder) \
	atomic_store_explicit(ptr, val, memorder)

#define rte_atomic_exchange_explicit(ptr, val, memorder) \
	atomic_exchange_explicit(ptr, val, memorder)

#define rte_atomic_compare_exchange_strong_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder) \
	atomic_compare_exchange_strong_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder)

#define rte_atomic_compare_exchange_weak_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder) \
	atomic_compare_exchange_weak_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder)

#define rte_atomic_fetch_add_explicit(ptr, val, memorder) \
	atomic_fetch_add_explicit(ptr, val, memorder)

#define rte_atomic_fetch_sub_explicit(ptr, val, memorder) \
	atomic_fetch_sub_explicit(ptr, val, memorder)

#define rte_atomic_fetch_and_explicit(ptr, val, memorder) \
	atomic_fetch_and_explicit(ptr, val, memorder)

#define rte_atomic_fetch_xor_explicit(ptr, val, memorder) \
	atomic_fetch_xor_explicit(ptr, val, memorder)

#define rte_atomic_fetch_or_explicit(ptr, val, memorder) \
	atomic_fetch_or_explicit(ptr, val, memorder)

#define rte_atomic_fetch_nand_explicit(ptr, val, memorder) \
	atomic_fetch_nand_explicit(ptr, val, memorder)

#define rte_atomic_flag_test_and_set_explicit(ptr, memorder) \
	atomic_flag_test_and_set_explicit(ptr, memorder)

#define rte_atomic_flag_clear_explicit(ptr, memorder) \
	atomic_flag_clear_explicit(ptr, memorder)

/* We provide internal macro here to allow conditional expansion
 * in the body of the per-arch rte_atomic_thread_fence inline functions.
 */
#define __rte_atomic_thread_fence(memorder) \
	atomic_thread_fence(memorder)

#else /* !RTE_ENABLE_STDATOMIC */

#define RTE_ATOMIC(type) type

#define __rte_atomic

/* The memory order is an integer type in GCC built-ins,
 * not an enumerated type like in C11.
 */
typedef int rte_memory_order;

#define rte_memory_order_relaxed __ATOMIC_RELAXED
#define rte_memory_order_consume __ATOMIC_CONSUME
#define rte_memory_order_acquire __ATOMIC_ACQUIRE
#define rte_memory_order_release __ATOMIC_RELEASE
#define rte_memory_order_acq_rel __ATOMIC_ACQ_REL
#define rte_memory_order_seq_cst __ATOMIC_SEQ_CST

#define rte_atomic_load_explicit(ptr, memorder) \
	__atomic_load_n(ptr, memorder)

#define rte_atomic_store_explicit(ptr, val, memorder) \
	__atomic_store_n(ptr, val, memorder)

#define rte_atomic_exchange_explicit(ptr, val, memorder) \
	__atomic_exchange_n(ptr, val, memorder)

#define rte_atomic_compare_exchange_strong_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder) \
	__atomic_compare_exchange_n(ptr, expected, desired, 0, \
		succ_memorder, fail_memorder)

#define rte_atomic_compare_exchange_weak_explicit(ptr, expected, desired, \
		succ_memorder, fail_memorder) \
	__atomic_compare_exchange_n(ptr, expected, desired, 1, \
		succ_memorder, fail_memorder)

#define rte_atomic_fetch_add_explicit(ptr, val, memorder) \
	__atomic_fetch_add(ptr, val, memorder)

#define rte_atomic_fetch_sub_explicit(ptr, val, memorder) \
	__atomic_fetch_sub(ptr, val, memorder)

#define rte_atomic_fetch_and_explicit(ptr, val, memorder) \
	__atomic_fetch_and(ptr, val, memorder)

#define rte_atomic_fetch_xor_explicit(ptr, val, memorder) \
	__atomic_fetch_xor(ptr, val, memorder)

#define rte_atomic_fetch_or_explicit(ptr, val, memorder) \
	__atomic_fetch_or(ptr, val, memorder)

#define rte_atomic_fetch_nand_explicit(ptr, val, memorder) \
	__atomic_fetch_nand(ptr, val, memorder)

#define rte_atomic_flag_test_and_set_explicit(ptr, memorder) \
	__atomic_test_and_set(ptr, memorder)

#define rte_atomic_flag_clear_explicit(ptr, memorder) \
	__atomic_clear(ptr, memorder)

/* We provide internal macro here to allow conditional expansion
 * in the body of the per-arch rte_atomic_thread_fence inline functions.
 */
#define __rte_atomic_thread_fence(memorder) \
	__atomic_thread_fence(memorder)

#endif

#ifdef __cplusplus
}
#endif

#endif /* RTE_STDATOMIC_H */
