/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 */

#ifndef _RTE_BITOPS_H_
#define _RTE_BITOPS_H_

/**
 * @file
 * Bit Operations
 *
 * This file defines a family of APIs for bit operations
 * without enforcing memory ordering.
 */

#include <stdint.h>
#include <rte_debug.h>
#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the uint64_t value for a specified bit set.
 *
 * @param nr
 *   The bit number in range of 0 to 63.
 */
#define RTE_BIT64(nr) (UINT64_C(1) << (nr))

/**
 * Get the uint32_t value for a specified bit set.
 *
 * @param nr
 *   The bit number in range of 0 to 31.
 */
#define RTE_BIT32(nr) (UINT32_C(1) << (nr))

/*------------------------ 32-bit relaxed operations ------------------------*/

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get the target bit from a 32-bit value without memory ordering.
 *
 * @param nr
 *   The target bit to get.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The target bit.
 */
__rte_experimental
static inline uint32_t
rte_bit_relaxed_get32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = UINT32_C(1) << nr;
	return (*addr) & mask;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Set the target bit in a 32-bit value to 1 without memory ordering.
 *
 * @param nr
 *   The target bit to set.
 * @param addr
 *   The address holding the bit.
 */
__rte_experimental
static inline void
rte_bit_relaxed_set32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	*addr = (*addr) | mask;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Clear the target bit in a 32-bit value to 0 without memory ordering.
 *
 * @param nr
 *   The target bit to clear.
 * @param addr
 *   The address holding the bit.
 */
__rte_experimental
static inline void
rte_bit_relaxed_clear32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	*addr = (*addr) & (~mask);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Return the original bit from a 32-bit value, then set it to 1 without
 * memory ordering.
 *
 * @param nr
 *   The target bit to get and set.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The original bit.
 */
__rte_experimental
static inline uint32_t
rte_bit_relaxed_test_and_set32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	uint32_t val = *addr;
	*addr = val | mask;
	return val & mask;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Return the original bit from a 32-bit value, then clear it to 0 without
 * memory ordering.
 *
 * @param nr
 *   The target bit to get and clear.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The original bit.
 */
__rte_experimental
static inline uint32_t
rte_bit_relaxed_test_and_clear32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	uint32_t val = *addr;
	*addr = val & (~mask);
	return val & mask;
}

/*------------------------ 64-bit relaxed operations ------------------------*/

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get the target bit from a 64-bit value without memory ordering.
 *
 * @param nr
 *   The target bit to get.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The target bit.
 */
__rte_experimental
static inline uint64_t
rte_bit_relaxed_get64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	return (*addr) & mask;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Set the target bit in a 64-bit value to 1 without memory ordering.
 *
 * @param nr
 *   The target bit to set.
 * @param addr
 *   The address holding the bit.
 */
__rte_experimental
static inline void
rte_bit_relaxed_set64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	(*addr) = (*addr) | mask;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Clear the target bit in a 64-bit value to 0 without memory ordering.
 *
 * @param nr
 *   The target bit to clear.
 * @param addr
 *   The address holding the bit.
 */
__rte_experimental
static inline void
rte_bit_relaxed_clear64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	*addr = (*addr) & (~mask);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Return the original bit from a 64-bit value, then set it to 1 without
 * memory ordering.
 *
 * @param nr
 *   The target bit to get and set.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The original bit.
 */
__rte_experimental
static inline uint64_t
rte_bit_relaxed_test_and_set64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	uint64_t val = *addr;
	*addr = val | mask;
	return val;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Return the original bit from a 64-bit value, then clear it to 0 without
 * memory ordering.
 *
 * @param nr
 *   The target bit to get and clear.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The original bit.
 */
__rte_experimental
static inline uint64_t
rte_bit_relaxed_test_and_clear64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	uint64_t val = *addr;
	*addr = val & (~mask);
	return val & mask;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BITOPS_H_ */
