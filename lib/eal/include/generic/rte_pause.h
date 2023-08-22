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
#include <rte_stdatomic.h>

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
 *  rte_memory_order_acquire and rte_memory_order_relaxed.
 */
static __rte_always_inline void
rte_wait_until_equal_16(volatile uint16_t *addr, uint16_t expected,
		rte_memory_order memorder);

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
 *  rte_memory_order_acquire and rte_memory_order_relaxed.
 */
static __rte_always_inline void
rte_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
		rte_memory_order memorder);

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
 *  rte_memory_order_acquire and rte_memory_order_relaxed.
 */
static __rte_always_inline void
rte_wait_until_equal_64(volatile uint64_t *addr, uint64_t expected,
		rte_memory_order memorder);

#ifndef RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED
static __rte_always_inline void
rte_wait_until_equal_16(volatile uint16_t *addr, uint16_t expected,
		rte_memory_order memorder)
{
	assert(memorder == rte_memory_order_acquire || memorder == rte_memory_order_relaxed);

	while (rte_atomic_load_explicit((volatile __rte_atomic uint16_t *)addr, memorder)
			!= expected)
		rte_pause();
}

static __rte_always_inline void
rte_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
		rte_memory_order memorder)
{
	assert(memorder == rte_memory_order_acquire || memorder == rte_memory_order_relaxed);

	while (rte_atomic_load_explicit((volatile __rte_atomic uint32_t *)addr, memorder)
			!= expected)
		rte_pause();
}

static __rte_always_inline void
rte_wait_until_equal_64(volatile uint64_t *addr, uint64_t expected,
		rte_memory_order memorder)
{
	assert(memorder == rte_memory_order_acquire || memorder == rte_memory_order_relaxed);

	while (rte_atomic_load_explicit((volatile __rte_atomic uint64_t *)addr, memorder)
			!= expected)
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
 *  rte_memory_order_acquire and rte_memory_order_relaxed.
 */
#define RTE_WAIT_UNTIL_MASKED(addr, mask, cond, expected, memorder) do { \
	RTE_BUILD_BUG_ON(!__builtin_constant_p(memorder));               \
	RTE_BUILD_BUG_ON((memorder) != rte_memory_order_acquire &&       \
		(memorder) != rte_memory_order_relaxed);                 \
	typeof(*(addr)) expected_value = (expected);                     \
	while (!((rte_atomic_load_explicit((addr), (memorder)) & (mask)) \
			cond expected_value))                            \
		rte_pause();                                             \
} while (0)
#endif /* ! RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED */

#endif /* _RTE_PAUSE_H_ */
