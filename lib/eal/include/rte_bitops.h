/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 * Copyright(c) 2010-2019 Intel Corporation
 * Copyright(c) 2023 Microsoft Corporation
 * Copyright(c) 2024 Ericsson AB
 */

#ifndef _RTE_BITOPS_H_
#define _RTE_BITOPS_H_

/**
 * @file
 * Bit Operations
 *
 * This file provides functionality for low-level, single-word
 * arithmetic and bit-level operations, such as counting or
 * setting individual bits.
 */

#include <stdint.h>

#include <rte_compat.h>
#include <rte_debug.h>
#include <rte_stdatomic.h>

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

/**
 * Get the uint32_t shifted value.
 *
 * @param val
 *   The value to be shifted.
 * @param nr
 *   The shift number in range of 0 to (32 - width of val).
 */
#define RTE_SHIFT_VAL32(val, nr) ((uint32_t)(val) << (nr))

/**
 * Get the uint64_t shifted value.
 *
 * @param val
 *   The value to be shifted.
 * @param nr
 *   The shift number in range of 0 to (64 - width of val).
 */
#define RTE_SHIFT_VAL64(val, nr) ((uint64_t)(val) << (nr))

/**
 * Generate a contiguous 32-bit mask
 * starting at bit position low and ending at position high.
 *
 * @param high
 *   High bit position.
 * @param low
 *   Low bit position.
 */
#define RTE_GENMASK32(high, low) \
		(((~UINT32_C(0)) << (low)) & (~UINT32_C(0) >> (31u - (high))))

/**
 * Generate a contiguous 64-bit mask
 * starting at bit position low and ending at position high.
 *
 * @param high
 *   High bit position.
 * @param low
 *   Low bit position.
 */
#define RTE_GENMASK64(high, low) \
		(((~UINT64_C(0)) << (low)) & (~UINT64_C(0) >> (63u - (high))))

/**
 * Extract a 32-bit field element.
 *
 * @param mask
 *   Shifted mask.
 * @param reg
 *   Value of entire bitfield.
 */
#define RTE_FIELD_GET32(mask, reg) \
		((typeof(mask))(((reg) & (mask)) >> rte_ctz32(mask)))

/**
 * Extract a 64-bit field element.
 *
 * @param mask
 *   Shifted mask.
 * @param reg
 *   Value of entire bitfield.
 */
#define RTE_FIELD_GET64(mask, reg) \
		((typeof(mask))(((reg) & (mask)) >> rte_ctz64(mask)))

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Test bit in word.
 *
 * Generic selection macro to test the value of a bit in a 32-bit or
 * 64-bit word. The type of operation depends on the type of the @c
 * addr parameter.
 *
 * This macro does not give any guarantees in regards to memory
 * ordering or atomicity.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 */
#define rte_bit_test(addr, nr) \
	_Generic((addr), \
		uint32_t *: __rte_bit_test32, \
		const uint32_t *: __rte_bit_test32, \
		volatile uint32_t *: __rte_bit_v_test32, \
		const volatile uint32_t *: __rte_bit_v_test32, \
		uint64_t *: __rte_bit_test64, \
		const uint64_t *: __rte_bit_test64, \
		volatile uint64_t *: __rte_bit_v_test64, \
		const volatile uint64_t *: __rte_bit_v_test64) \
			(addr, nr)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set bit in word.
 *
 * Generic selection macro to set a bit in a 32-bit or 64-bit
 * word. The type of operation depends on the type of the @c addr
 * parameter.
 *
 * This macro does not give any guarantees in regards to memory
 * ordering or atomicity.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 */
#define rte_bit_set(addr, nr) \
	_Generic((addr), \
		uint32_t *: __rte_bit_set32, \
		volatile uint32_t *: __rte_bit_v_set32, \
		uint64_t *: __rte_bit_set64, \
		volatile uint64_t *: __rte_bit_v_set64) \
			(addr, nr)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Clear bit in word.
 *
 * Generic selection macro to clear a bit in a 32-bit or 64-bit
 * word. The type of operation depends on the type of the @c addr
 * parameter.
 *
 * This macro does not give any guarantees in regards to memory
 * ordering or atomicity.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 */
#define rte_bit_clear(addr, nr) \
	_Generic((addr), \
		uint32_t *: __rte_bit_clear32, \
		volatile uint32_t *: __rte_bit_v_clear32, \
		uint64_t *: __rte_bit_clear64, \
		volatile uint64_t *: __rte_bit_v_clear64) \
			(addr, nr)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Assign a value to a bit in word.
 *
 * Generic selection macro to assign a value to a bit in a 32-bit or 64-bit
 * word. The type of operation depends on the type of the @c addr parameter.
 *
 * This macro does not give any guarantees in regards to memory
 * ordering or atomicity.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param value
 *   The new value of the bit - true for '1', or false for '0'.
 */
#define rte_bit_assign(addr, nr, value) \
	_Generic((addr), \
		uint32_t *: __rte_bit_assign32, \
		volatile uint32_t *: __rte_bit_v_assign32, \
		uint64_t *: __rte_bit_assign64, \
		volatile uint64_t *: __rte_bit_v_assign64) \
			(addr, nr, value)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Flip a bit in word.
 *
 * Generic selection macro to change the value of a bit to '0' if '1'
 * or '1' if '0' in a 32-bit or 64-bit word. The type of operation
 * depends on the type of the @c addr parameter.
 *
 * This macro does not give any guarantees in regards to memory
 * ordering or atomicity.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 */
#define rte_bit_flip(addr, nr) \
	_Generic((addr), \
		uint32_t *: __rte_bit_flip32, \
		volatile uint32_t *: __rte_bit_v_flip32, \
		uint64_t *: __rte_bit_flip64, \
		volatile uint64_t *: __rte_bit_v_flip64) \
			(addr, nr)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Test if a particular bit in a word is set with a particular memory
 * order.
 *
 * Test a bit with the resulting memory load ordered as per the
 * specified memory order.
 *
 * @param addr
 *   A pointer to the word to query.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 * @return
 *   Returns true if the bit is set, and false otherwise.
 */
#define rte_bit_atomic_test(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_test32, \
		const uint32_t *: __rte_bit_atomic_test32, \
		volatile uint32_t *: __rte_bit_atomic_v_test32, \
		const volatile uint32_t *: __rte_bit_atomic_v_test32, \
		uint64_t *: __rte_bit_atomic_test64, \
		const uint64_t *: __rte_bit_atomic_test64, \
		volatile uint64_t *: __rte_bit_atomic_v_test64, \
		const volatile uint64_t *: __rte_bit_atomic_v_test64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically set bit in word.
 *
 * Generic selection macro to atomically set bit specified by @c nr in
 * the word pointed to by @c addr to '1', with the memory ordering as
 * specified by @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 */
#define rte_bit_atomic_set(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_set32, \
		volatile uint32_t *: __rte_bit_atomic_v_set32, \
		uint64_t *: __rte_bit_atomic_set64, \
		volatile uint64_t *: __rte_bit_atomic_v_set64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically clear bit in word.
 *
 * Generic selection macro to atomically set bit specified by @c nr in
 * the word pointed to by @c addr to '0', with the memory ordering as
 * specified by @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 */
#define rte_bit_atomic_clear(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_clear32, \
		volatile uint32_t *: __rte_bit_atomic_v_clear32, \
		uint64_t *: __rte_bit_atomic_clear64, \
		volatile uint64_t *: __rte_bit_atomic_v_clear64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically assign a value to bit in word.
 *
 * Generic selection macro to atomically set bit specified by @c nr in the
 * word pointed to by @c addr to the value indicated by @c value, with
 * the memory ordering as specified with @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param value
 *   The new value of the bit - true for '1', or false for '0'.
 * @param memory_order
 *   The memory order to use.
 */
#define rte_bit_atomic_assign(addr, nr, value, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_assign32, \
		volatile uint32_t *: __rte_bit_atomic_v_assign32, \
		uint64_t *: __rte_bit_atomic_assign64, \
		volatile uint64_t *: __rte_bit_atomic_v_assign64) \
			(addr, nr, value, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically flip bit in word.
 *
 * Generic selection macro to atomically negate the value of the bit
 * specified by @c nr in the word pointed to by @c addr to the value
 * indicated by @c value, with the memory ordering as specified with
 * @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 */
#define rte_bit_atomic_flip(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_flip32, \
		volatile uint32_t *: __rte_bit_atomic_v_flip32, \
		uint64_t *: __rte_bit_atomic_flip64, \
		volatile uint64_t *: __rte_bit_atomic_v_flip64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically test and set a bit in word.
 *
 * Generic selection macro to atomically test and set bit specified by
 * @c nr in the word pointed to by @c addr to '1', with the memory
 * ordering as specified with @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 * @return
 *   Returns true if the bit was set, and false otherwise.
 */
#define rte_bit_atomic_test_and_set(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_test_and_set32, \
		volatile uint32_t *: __rte_bit_atomic_v_test_and_set32, \
		uint64_t *: __rte_bit_atomic_test_and_set64, \
		volatile uint64_t *: __rte_bit_atomic_v_test_and_set64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically test and clear a bit in word.
 *
 * Generic selection macro to atomically test and clear bit specified
 * by @c nr in the word pointed to by @c addr to '0', with the memory
 * ordering as specified with @c memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param memory_order
 *   The memory order to use.
 * @return
 *   Returns true if the bit was set, and false otherwise.
 */
#define rte_bit_atomic_test_and_clear(addr, nr, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_test_and_clear32, \
		volatile uint32_t *: __rte_bit_atomic_v_test_and_clear32, \
		uint64_t *: __rte_bit_atomic_test_and_clear64, \
		volatile uint64_t *: __rte_bit_atomic_v_test_and_clear64) \
			(addr, nr, memory_order)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically test and assign a bit in word.
 *
 * Generic selection macro to atomically test and assign bit specified
 * by @c nr in the word pointed to by @c addr the value specified by
 * @c value, with the memory ordering as specified with @c
 * memory_order.
 *
 * @param addr
 *   A pointer to the word to modify.
 * @param nr
 *   The index of the bit.
 * @param value
 *   The new value of the bit - true for '1', or false for '0'.
 * @param memory_order
 *   The memory order to use.
 * @return
 *   Returns true if the bit was set, and false otherwise.
 */
#define rte_bit_atomic_test_and_assign(addr, nr, value, memory_order) \
	_Generic((addr), \
		uint32_t *: __rte_bit_atomic_test_and_assign32, \
		volatile uint32_t *: __rte_bit_atomic_v_test_and_assign32, \
		uint64_t *: __rte_bit_atomic_test_and_assign64, \
		volatile uint64_t *: __rte_bit_atomic_v_test_and_assign64) \
			(addr, nr, value, memory_order)

#define __RTE_GEN_BIT_TEST(variant, qualifier, size) \
__rte_experimental \
static inline bool \
__rte_bit_ ## variant ## test ## size(const qualifier uint ## size ## _t *addr, unsigned int nr) \
{ \
	RTE_ASSERT(nr < size); \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	return *addr & mask; \
}

#define __RTE_GEN_BIT_SET(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_ ## variant ## set ## size(qualifier uint ## size ## _t *addr, unsigned int nr) \
{ \
	RTE_ASSERT(nr < size); \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	*addr |= mask; \
}

#define __RTE_GEN_BIT_CLEAR(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_ ## variant ## clear ## size(qualifier uint ## size ## _t *addr, unsigned int nr) \
{ \
	RTE_ASSERT(nr < size); \
	uint ## size ## _t mask = ~((uint ## size ## _t)1 << nr); \
	(*addr) &= mask; \
}

#define __RTE_GEN_BIT_ASSIGN(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_ ## variant ## assign ## size(qualifier uint ## size ## _t *addr, unsigned int nr, \
		bool value) \
{ \
	if (value) \
		__rte_bit_ ## variant ## set ## size(addr, nr); \
	else \
		__rte_bit_ ## variant ## clear ## size(addr, nr); \
}

#define __RTE_GEN_BIT_FLIP(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_ ## variant ## flip ## size(qualifier uint ## size ## _t *addr, unsigned int nr) \
{ \
	bool value; \
	value = __rte_bit_ ## variant ## test ## size(addr, nr); \
	__rte_bit_ ## variant ## assign ## size(addr, nr, !value); \
}

#define __RTE_GEN_BIT_OPS(v, qualifier, size) \
	__RTE_GEN_BIT_TEST(v, qualifier, size) \
	__RTE_GEN_BIT_SET(v, qualifier, size) \
	__RTE_GEN_BIT_CLEAR(v, qualifier, size) \
	__RTE_GEN_BIT_ASSIGN(v, qualifier, size) \
	__RTE_GEN_BIT_FLIP(v, qualifier, size)

#define __RTE_GEN_BIT_OPS_SIZE(size) \
	__RTE_GEN_BIT_OPS(,, size) \
	__RTE_GEN_BIT_OPS(v_, volatile, size)

#ifdef ALLOW_EXPERIMENTAL_API
__RTE_GEN_BIT_OPS_SIZE(32)
__RTE_GEN_BIT_OPS_SIZE(64)
#endif

#define __RTE_GEN_BIT_ATOMIC_TEST(variant, qualifier, size) \
__rte_experimental \
static inline bool \
__rte_bit_atomic_ ## variant ## test ## size(const qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	const qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(const qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	return rte_atomic_load_explicit(a_addr, memory_order) & mask; \
}

#define __RTE_GEN_BIT_ATOMIC_SET(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_atomic_ ## variant ## set ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	rte_atomic_fetch_or_explicit(a_addr, mask, memory_order); \
}

#define __RTE_GEN_BIT_ATOMIC_CLEAR(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_atomic_ ## variant ## clear ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	rte_atomic_fetch_and_explicit(a_addr, ~mask, memory_order); \
}

#define __RTE_GEN_BIT_ATOMIC_FLIP(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_atomic_ ## variant ## flip ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	rte_atomic_fetch_xor_explicit(a_addr, mask, memory_order); \
}

#define __RTE_GEN_BIT_ATOMIC_ASSIGN(variant, qualifier, size) \
__rte_experimental \
static inline void \
__rte_bit_atomic_## variant ## assign ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, bool value, int memory_order) \
{ \
	if (value) \
		__rte_bit_atomic_ ## variant ## set ## size(addr, nr, memory_order); \
	else \
		__rte_bit_atomic_ ## variant ## clear ## size(addr, nr, memory_order); \
}

#define __RTE_GEN_BIT_ATOMIC_TEST_AND_SET(variant, qualifier, size) \
__rte_experimental \
static inline bool \
__rte_bit_atomic_ ## variant ## test_and_set ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	uint ## size ## _t prev; \
	prev = rte_atomic_fetch_or_explicit(a_addr, mask, memory_order); \
	return prev & mask; \
}

#define __RTE_GEN_BIT_ATOMIC_TEST_AND_CLEAR(variant, qualifier, size) \
__rte_experimental \
static inline bool \
__rte_bit_atomic_ ## variant ## test_and_clear ## size(qualifier uint ## size ## _t *addr, \
		unsigned int nr, int memory_order) \
{ \
	RTE_ASSERT(nr < size); \
	qualifier RTE_ATOMIC(uint ## size ## _t) *a_addr = \
		(qualifier RTE_ATOMIC(uint ## size ## _t) *)addr; \
	uint ## size ## _t mask = (uint ## size ## _t)1 << nr; \
	uint ## size ## _t prev; \
	prev = rte_atomic_fetch_and_explicit(a_addr, ~mask, memory_order); \
	return prev & mask; \
}

#define __RTE_GEN_BIT_ATOMIC_TEST_AND_ASSIGN(variant, qualifier, size) \
__rte_experimental \
static inline bool \
__rte_bit_atomic_ ## variant ## test_and_assign ## size( \
		qualifier uint ## size ## _t *addr, unsigned int nr, bool value, \
		int memory_order) \
{ \
	if (value) \
		return __rte_bit_atomic_ ## variant ## test_and_set ## size(addr, nr, \
			memory_order); \
	else \
		return __rte_bit_atomic_ ## variant ## test_and_clear ## size(addr, nr, \
			memory_order); \
}

#define __RTE_GEN_BIT_ATOMIC_OPS(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_TEST(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_SET(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_CLEAR(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_ASSIGN(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_TEST_AND_SET(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_TEST_AND_CLEAR(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_TEST_AND_ASSIGN(variant, qualifier, size) \
	__RTE_GEN_BIT_ATOMIC_FLIP(variant, qualifier, size)

#define __RTE_GEN_BIT_ATOMIC_OPS_SIZE(size) \
	__RTE_GEN_BIT_ATOMIC_OPS(,, size) \
	__RTE_GEN_BIT_ATOMIC_OPS(v_, volatile, size)

#ifdef ALLOW_EXPERIMENTAL_API
__RTE_GEN_BIT_ATOMIC_OPS_SIZE(32)
__RTE_GEN_BIT_ATOMIC_OPS_SIZE(64)
#endif

/*------------------------ 32-bit relaxed operations ------------------------*/

/**
 * Get the target bit from a 32-bit value without memory ordering.
 *
 * @param nr
 *   The target bit to get.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The target bit.
 */
static inline uint32_t
rte_bit_relaxed_get32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = UINT32_C(1) << nr;
	return (*addr) & mask;
}

/**
 * Set the target bit in a 32-bit value to 1 without memory ordering.
 *
 * @param nr
 *   The target bit to set.
 * @param addr
 *   The address holding the bit.
 */
static inline void
rte_bit_relaxed_set32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	*addr = (*addr) | mask;
}

/**
 * Clear the target bit in a 32-bit value to 0 without memory ordering.
 *
 * @param nr
 *   The target bit to clear.
 * @param addr
 *   The address holding the bit.
 */
static inline void
rte_bit_relaxed_clear32(unsigned int nr, volatile uint32_t *addr)
{
	RTE_ASSERT(nr < 32);

	uint32_t mask = RTE_BIT32(nr);
	*addr = (*addr) & (~mask);
}

/**
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
 * Get the target bit from a 64-bit value without memory ordering.
 *
 * @param nr
 *   The target bit to get.
 * @param addr
 *   The address holding the bit.
 * @return
 *   The target bit.
 */
static inline uint64_t
rte_bit_relaxed_get64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	return (*addr) & mask;
}

/**
 * Set the target bit in a 64-bit value to 1 without memory ordering.
 *
 * @param nr
 *   The target bit to set.
 * @param addr
 *   The address holding the bit.
 */
static inline void
rte_bit_relaxed_set64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	(*addr) = (*addr) | mask;
}

/**
 * Clear the target bit in a 64-bit value to 0 without memory ordering.
 *
 * @param nr
 *   The target bit to clear.
 * @param addr
 *   The address holding the bit.
 */
static inline void
rte_bit_relaxed_clear64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	*addr = (*addr) & (~mask);
}

/**
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
static inline uint64_t
rte_bit_relaxed_test_and_clear64(unsigned int nr, volatile uint64_t *addr)
{
	RTE_ASSERT(nr < 64);

	uint64_t mask = RTE_BIT64(nr);
	uint64_t val = *addr;
	*addr = val & (~mask);
	return val & mask;
}

#ifdef RTE_TOOLCHAIN_MSVC

/**
 * Get the count of leading 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of leading zero bits.
 */
static inline unsigned int
rte_clz32(uint32_t v)
{
	unsigned long rv;

	(void)_BitScanReverse(&rv, v);

	return (unsigned int)(sizeof(v) * CHAR_BIT - 1 - rv);
}

/**
 * Get the count of leading 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of leading zero bits.
 */
static inline unsigned int
rte_clz64(uint64_t v)
{
	unsigned long rv;

	(void)_BitScanReverse64(&rv, v);

	return (unsigned int)(sizeof(v) * CHAR_BIT - 1 - rv);
}

/**
 * Get the count of trailing 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of trailing zero bits.
 */
static inline unsigned int
rte_ctz32(uint32_t v)
{
	unsigned long rv;

	(void)_BitScanForward(&rv, v);

	return (unsigned int)rv;
}

/**
 * Get the count of trailing 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of trailing zero bits.
 */
static inline unsigned int
rte_ctz64(uint64_t v)
{
	unsigned long rv;

	(void)_BitScanForward64(&rv, v);

	return (unsigned int)rv;
}

/**
 * Get the count of 1-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of 1-bits.
 */
static inline unsigned int
rte_popcount32(uint32_t v)
{
	return (unsigned int)__popcnt(v);
}

/**
 * Get the count of 1-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of 1-bits.
 */
static inline unsigned int
rte_popcount64(uint64_t v)
{
	return (unsigned int)__popcnt64(v);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Search v from least significant bit (LSB) to the most
 * significant bit (MSB) for a set bit (1).
 *
 * @param v
 *   The value.
 * @return
 *   Bit index + 1 if a set bit is found, zero otherwise.
 */
__rte_experimental
static inline unsigned int
rte_ffs32(uint32_t v)
{
	unsigned long rv;

	if (_BitScanForward(&rv, v) == 0)
		return 0;

	return (unsigned int)rv + 1;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Search v from least significant bit (LSB) to the most
 * significant bit (MSB) for a set bit (1).
 *
 * @param v
 *   The value.
 * @return
 *   Bit index + 1 if a set bit is found, zero otherwise.
 */
__rte_experimental
static inline unsigned int
rte_ffs64(uint64_t v)
{
	unsigned long rv;

	if (_BitScanForward64(&rv, v) == 0)
		return 0;

	return (unsigned int)rv + 1;
}

#else

/**
 * Get the count of leading 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of leading zero bits.
 */
static inline unsigned int
rte_clz32(uint32_t v)
{
	return (unsigned int)__builtin_clz(v);
}

/**
 * Get the count of leading 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of leading zero bits.
 */
static inline unsigned int
rte_clz64(uint64_t v)
{
	return (unsigned int)__builtin_clzll(v);
}

/**
 * Get the count of trailing 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of trailing zero bits.
 */
static inline unsigned int
rte_ctz32(uint32_t v)
{
	return (unsigned int)__builtin_ctz(v);
}

/**
 * Get the count of trailing 0-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of trailing zero bits.
 */
static inline unsigned int
rte_ctz64(uint64_t v)
{
	return (unsigned int)__builtin_ctzll(v);
}

/**
 * Get the count of 1-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of 1-bits.
 */
static inline unsigned int
rte_popcount32(uint32_t v)
{
	return (unsigned int)__builtin_popcount(v);
}

/**
 * Get the count of 1-bits in v.
 *
 * @param v
 *   The value.
 * @return
 *   The count of 1-bits.
 */
static inline unsigned int
rte_popcount64(uint64_t v)
{
	return (unsigned int)__builtin_popcountll(v);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Search v from least significant bit (LSB) to the most
 * significant bit (MSB) for a set bit (1).
 *
 * @param v
 *   The value.
 * @return
 *   Bit index + 1 if a set bit is found, zero otherwise.
 */
__rte_experimental
static inline unsigned int
rte_ffs32(uint32_t v)
{
	return (unsigned int)__builtin_ffs(v);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Search v from least significant bit (LSB) to the most
 * significant bit (MSB) for a set bit (1).
 *
 * @param v
 *   The value.
 * @return
 *   Bit index + 1 if a set bit is found, zero otherwise.
 */
__rte_experimental
static inline unsigned int
rte_ffs64(uint64_t v)
{
	return (unsigned int)__builtin_ffsll(v);
}

#endif

/**
 * Combines 32b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param x
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint32_t
rte_combine32ms1b(uint32_t x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return x;
}

/**
 * Combines 64b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param v
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint64_t
rte_combine64ms1b(uint64_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	return v;
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
rte_bsf32(uint32_t v)
{
	return (uint32_t)rte_ctz32(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
rte_bsf32_safe(uint32_t v, uint32_t *pos)
{
	if (v == 0)
		return 0;

	*pos = rte_bsf32(v);
	return 1;
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
rte_bsf64(uint64_t v)
{
	return (uint32_t)rte_ctz64(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
rte_bsf64_safe(uint64_t v, uint32_t *pos)
{
	if (v == 0)
		return 0;

	*pos = rte_bsf64(v);
	return 1;
}

/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 32.
 * @note rte_fls_u32(0) = 0, rte_fls_u32(1) = 1, rte_fls_u32(0x80000000) = 32
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline uint32_t
rte_fls_u32(uint32_t x)
{
	return (x == 0) ? 0 : 32 - rte_clz32(x);
}

/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 64.
 * @note rte_fls_u64(0) = 0, rte_fls_u64(1) = 1,
 *       rte_fls_u64(0x8000000000000000) = 64
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline uint32_t
rte_fls_u64(uint64_t x)
{
	return (x == 0) ? 0 : 64 - rte_clz64(x);
}

/*********** Macros to work with powers of 2 ********/

/**
 * Macro to return 1 if n is a power of 2, 0 otherwise
 */
#define RTE_IS_POWER_OF_2(n) ((n) && !(((n) - 1) & (n)))

/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int
rte_is_power_of_2(uint32_t n)
{
	return n && !(n & (n - 1));
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   The smallest power of 2 which is greater than or equal to @c x.
 */
static inline uint32_t
rte_align32pow2(uint32_t x)
{
	x--;
	x = rte_combine32ms1b(x);

	return x + 1;
}

/**
 * Aligns input parameter to the previous power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   The greatest power of 2 which is smaller than or equal to @c x.
 */
static inline uint32_t
rte_align32prevpow2(uint32_t x)
{
	x = rte_combine32ms1b(x);

	return x - (x >> 1);
}

/**
 * Aligns 64b input parameter to the next power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   The smallest power of 2 which is greater than or equal to @c v.
 */
static inline uint64_t
rte_align64pow2(uint64_t v)
{
	v--;
	v = rte_combine64ms1b(v);

	return v + 1;
}

/**
 * Aligns 64b input parameter to the previous power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   The greatest power of 2 which is smaller than or equal to @c v.
 */
static inline uint64_t
rte_align64prevpow2(uint64_t v)
{
	v = rte_combine64ms1b(v);

	return v - (v >> 1);
}

/**
 * Return the rounded-up log2 of a integer.
 *
 * @note Contrary to the logarithm mathematical operation,
 * rte_log2_u32(0) == 0 and not -inf.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
rte_log2_u32(uint32_t v)
{
	if (v == 0)
		return 0;
	v = rte_align32pow2(v);
	return rte_bsf32(v);
}

/**
 * Return the rounded-up log2 of a 64-bit integer.
 *
 * @note Contrary to the logarithm mathematical operation,
 * rte_log2_u64(0) == 0 and not -inf.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
rte_log2_u64(uint64_t v)
{
	if (v == 0)
		return 0;
	v = rte_align64pow2(v);
	/* we checked for v being 0 already, so no undefined behavior */
	return rte_bsf64(v);
}

#ifdef __cplusplus
}

/*
 * Since C++ doesn't support generic selection (i.e., _Generic),
 * function overloading is used instead. Such functions must be
 * defined outside 'extern "C"' to be accepted by the compiler.
 */

#undef rte_bit_test
#undef rte_bit_set
#undef rte_bit_clear
#undef rte_bit_assign
#undef rte_bit_flip

#undef rte_bit_atomic_test
#undef rte_bit_atomic_set
#undef rte_bit_atomic_clear
#undef rte_bit_atomic_assign
#undef rte_bit_atomic_flip
#undef rte_bit_atomic_test_and_set
#undef rte_bit_atomic_test_and_clear
#undef rte_bit_atomic_test_and_assign

#define __RTE_BIT_OVERLOAD_V_2(family, v, fun, qualifier, size, arg1_type, arg1_name) \
static inline void \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name) \
{ \
	__rte_bit_ ## family ## v ## fun ## size(addr, arg1_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_2(family, fun, qualifier, size, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_V_2(family,, fun, qualifier, size, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_V_2(family, v_, fun, qualifier volatile, size, arg1_type, arg1_name)

#define __RTE_BIT_OVERLOAD_2(family, fun, qualifier, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_SZ_2(family, fun, qualifier, 32, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_SZ_2(family, fun, qualifier, 64, arg1_type, arg1_name)

#define __RTE_BIT_OVERLOAD_V_2R(family, v, fun, qualifier, size, ret_type, arg1_type, arg1_name) \
static inline ret_type \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name) \
{ \
	return __rte_bit_ ## family ## v ## fun ## size(addr, arg1_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_2R(family, fun, qualifier, size, ret_type, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_V_2R(family,, fun, qualifier, size, ret_type, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_V_2R(family, v_, fun, qualifier volatile, size, ret_type, arg1_type, \
		arg1_name)

#define __RTE_BIT_OVERLOAD_2R(family, fun, qualifier, ret_type, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_SZ_2R(family, fun, qualifier, 32, ret_type, arg1_type, arg1_name) \
	__RTE_BIT_OVERLOAD_SZ_2R(family, fun, qualifier, 64, ret_type, arg1_type, arg1_name)

#define __RTE_BIT_OVERLOAD_V_3(family, v, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
static inline void \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name, \
		arg2_type arg2_name) \
{ \
	__rte_bit_ ## family ## v ## fun ## size(addr, arg1_name, arg2_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_3(family, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_V_3(family,, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_V_3(family, v_, fun, qualifier volatile, size, arg1_type, arg1_name, \
		arg2_type, arg2_name)

#define __RTE_BIT_OVERLOAD_3(family, fun, qualifier, arg1_type, arg1_name, arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_SZ_3(family, fun, qualifier, 32, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_SZ_3(family, fun, qualifier, 64, arg1_type, arg1_name, \
		arg2_type, arg2_name)

#define __RTE_BIT_OVERLOAD_V_3R(family, v, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
static inline ret_type \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name, \
		arg2_type arg2_name) \
{ \
	return __rte_bit_ ## family ## v ## fun ## size(addr, arg1_name, arg2_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_3R(family, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_V_3R(family,, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_V_3R(family, v_, fun, qualifier volatile, size, ret_type, \
		arg1_type, arg1_name, arg2_type, arg2_name)

#define __RTE_BIT_OVERLOAD_3R(family, fun, qualifier, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_SZ_3R(family, fun, qualifier, 32, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name) \
	__RTE_BIT_OVERLOAD_SZ_3R(family, fun, qualifier, 64, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name)

#define __RTE_BIT_OVERLOAD_V_4(family, v, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
static inline void \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name, \
		arg2_type arg2_name, arg3_type arg3_name) \
{ \
	__rte_bit_ ## family ## v ## fun ## size(addr, arg1_name, arg2_name, arg3_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_4(family, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_V_4(family,, fun, qualifier, size, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_V_4(family, v_, fun, qualifier volatile, size, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name)

#define __RTE_BIT_OVERLOAD_4(family, fun, qualifier, arg1_type, arg1_name, arg2_type, arg2_name, \
		arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_SZ_4(family, fun, qualifier, 32, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_SZ_4(family, fun, qualifier, 64, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name)

#define __RTE_BIT_OVERLOAD_V_4R(family, v, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
static inline ret_type \
rte_bit_ ## family ## fun(qualifier uint ## size ## _t *addr, arg1_type arg1_name, \
		arg2_type arg2_name, arg3_type arg3_name) \
{ \
	return __rte_bit_ ## family ## v ## fun ## size(addr, arg1_name, arg2_name, \
		arg3_name); \
}

#define __RTE_BIT_OVERLOAD_SZ_4R(family, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_V_4R(family,, fun, qualifier, size, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_V_4R(family, v_, fun, qualifier volatile, size, ret_type, \
		arg1_type, arg1_name, arg2_type, arg2_name, arg3_type, arg3_name)

#define __RTE_BIT_OVERLOAD_4R(family, fun, qualifier, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_SZ_4R(family, fun, qualifier, 32, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name) \
	__RTE_BIT_OVERLOAD_SZ_4R(family, fun, qualifier, 64, ret_type, arg1_type, arg1_name, \
		arg2_type, arg2_name, arg3_type, arg3_name)

#ifdef ALLOW_EXPERIMENTAL_API
__RTE_BIT_OVERLOAD_2R(, test, const, bool, unsigned int, nr)
__RTE_BIT_OVERLOAD_2(, set,, unsigned int, nr)
__RTE_BIT_OVERLOAD_2(, clear,, unsigned int, nr)
__RTE_BIT_OVERLOAD_3(, assign,, unsigned int, nr, bool, value)
__RTE_BIT_OVERLOAD_2(, flip,, unsigned int, nr)

__RTE_BIT_OVERLOAD_3R(atomic_, test, const, bool, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_3(atomic_, set,, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_3(atomic_, clear,, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_4(atomic_, assign,, unsigned int, nr, bool, value, int, memory_order)
__RTE_BIT_OVERLOAD_3(atomic_, flip,, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_3R(atomic_, test_and_set,, bool, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_3R(atomic_, test_and_clear,, bool, unsigned int, nr, int, memory_order)
__RTE_BIT_OVERLOAD_4R(atomic_, test_and_assign,, bool, unsigned int, nr, bool, value,
	int, memory_order)
#endif

#endif

#endif /* _RTE_BITOPS_H_ */
