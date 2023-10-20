/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 * Copyright(c) 2010-2019 Intel Corporation
 * Copyright(c) 2023 Microsoft Corporation
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
 *   Input parameter aligned to the next power of 2
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
 *   Input parameter aligned to the previous power of 2
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
 *   Input parameter aligned to the next power of 2
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
 *   Input parameter aligned to the previous power of 2
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
#endif

#endif /* _RTE_BITOPS_H_ */
