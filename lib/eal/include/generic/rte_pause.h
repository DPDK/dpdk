/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_PAUSE_H_
#define _RTE_PAUSE_H_

/**
 * @file
 *
 * CPU pause operation.
 */

#include <stdint.h>
#include <assert.h>
#include <rte_common.h>
#include <rte_atomic.h>

/**
 * Pause CPU execution for a short while
 *
 * This call is intended for tight loops which poll a shared resource or wait
 * for an event. A short pause within the loop may reduce the power consumption.
 */
static inline void rte_pause(void);

/**
 * Wait for *addr to be updated with a 16-bit expected value, with a relaxed
 * memory ordering model meaning the loads around this API can be reordered.
 *
 * @param addr
 *  A pointer to the memory location.
 * @param expected
 *  A 16-bit expected value to be in the memory location.
 * @param memorder
 *  Two different memory orders that can be specified:
 *  __ATOMIC_ACQUIRE and __ATOMIC_RELAXED. These map to
 *  C++11 memory orders with the same names, see the C++11 standard or
 *  the GCC wiki on atomic synchronization for detailed definition.
 */
static __rte_always_inline void
rte_wait_until_equal_16(volatile uint16_t *addr, uint16_t expected,
		int memorder);

/**
 * Wait for *addr to be updated with a 32-bit expected value, with a relaxed
 * memory ordering model meaning the loads around this API can be reordered.
 *
 * @param addr
 *  A pointer to the memory location.
 * @param expected
 *  A 32-bit expected value to be in the memory location.
 * @param memorder
 *  Two different memory orders that can be specified:
 *  __ATOMIC_ACQUIRE and __ATOMIC_RELAXED. These map to
 *  C++11 memory orders with the same names, see the C++11 standard or
 *  the GCC wiki on atomic synchronization for detailed definition.
 */
static __rte_always_inline void
rte_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
		int memorder);

/**
 * Wait for *addr to be updated with a 64-bit expected value, with a relaxed
 * memory ordering model meaning the loads around this API can be reordered.
 *
 * @param addr
 *  A pointer to the memory location.
 * @param expected
 *  A 64-bit expected value to be in the memory location.
 * @param memorder
 *  Two different memory orders that can be specified:
 *  __ATOMIC_ACQUIRE and __ATOMIC_RELAXED. These map to
 *  C++11 memory orders with the same names, see the C++11 standard or
 *  the GCC wiki on atomic synchronization for detailed definition.
 */
static __rte_always_inline void
rte_wait_until_equal_64(volatile uint64_t *addr, uint64_t expected,
		int memorder);

#ifndef RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED
static __rte_always_inline void
rte_wait_until_equal_16(volatile uint16_t *addr, uint16_t expected,
		int memorder)
{
	assert(memorder == __ATOMIC_ACQUIRE || memorder == __ATOMIC_RELAXED);

	while (__atomic_load_n(addr, memorder) != expected)
		rte_pause();
}

static __rte_always_inline void
rte_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
		int memorder)
{
	assert(memorder == __ATOMIC_ACQUIRE || memorder == __ATOMIC_RELAXED);

	while (__atomic_load_n(addr, memorder) != expected)
		rte_pause();
}

static __rte_always_inline void
rte_wait_until_equal_64(volatile uint64_t *addr, uint64_t expected,
		int memorder)
{
	assert(memorder == __ATOMIC_ACQUIRE || memorder == __ATOMIC_RELAXED);

	while (__atomic_load_n(addr, memorder) != expected)
		rte_pause();
}

/*
 * Wait until *addr & mask makes the condition true. With a relaxed memory
 * ordering model, the loads around this helper can be reordered.
 *
 * @param addr
 *  A pointer to the memory location.
 * @param mask
 *  A mask of value bits in interest.
 * @param cond
 *  A symbol representing the condition.
 * @param expected
 *  An expected value to be in the memory location.
 * @param memorder
 *  Two different memory orders that can be specified:
 *  __ATOMIC_ACQUIRE and __ATOMIC_RELAXED. These map to
 *  C++11 memory orders with the same names, see the C++11 standard or
 *  the GCC wiki on atomic synchronization for detailed definition.
 */
#define RTE_WAIT_UNTIL_MASKED(addr, mask, cond, expected, memorder) do { \
	RTE_BUILD_BUG_ON(!__builtin_constant_p(memorder));               \
	RTE_BUILD_BUG_ON(memorder != __ATOMIC_ACQUIRE &&                 \
		memorder != __ATOMIC_RELAXED);                           \
	typeof(*(addr)) expected_value = (expected);                     \
	while (!((__atomic_load_n((addr), (memorder)) & (mask)) cond     \
			expected_value))                                 \
		rte_pause();                                             \
} while (0)
#endif /* ! RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED */

#endif /* _RTE_PAUSE_H_ */
