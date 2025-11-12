/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef _RTE_BITSET_H_
#define _RTE_BITSET_H_

/**
 * @file
 * RTE Bitset
 *
 * This file provides functions and macros for querying and
 * manipulating sets of bits kept in arrays of @c uint64_t-sized
 * elements.
 *
 * The bits in a bitset are numbered from 0 to @c size - 1, with the
 * lowest index being the least significant bit.
 *
 * The bitset array must be properly aligned.
 *
 * For optimal performance, the @c size parameter, required by
 * many of the API's functions, should be a compile-time constant.
 *
 * For large bitsets, the rte_bitmap.h API may be more appropriate.
 *
 * @warning
 * All functions modifying a bitset may overwrite any unused bits of
 * the last word. Such unused bits are ignored by all functions reading
 * bits.
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rte_bitops.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_debug.h>
#include <rte_memcpy.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The size (in bytes) of each element in the array used to represent
 * a bitset.
 */
#define RTE_BITSET_WORD_SIZE (sizeof(uint64_t))

/**
 * The size (in bits) of each element in the array used to represent
 * a bitset.
 */
#define RTE_BITSET_WORD_BITS (RTE_BITSET_WORD_SIZE * CHAR_BIT)

/**
 * Computes the number of words required to store @c size bits.
 */
#define RTE_BITSET_NUM_WORDS(size) \
	((size + RTE_BITSET_WORD_BITS - 1) / RTE_BITSET_WORD_BITS)

/**
 * Computes the amount of memory (in bytes) required to fit a bitset
 * holding @c size bits.
 */
#define RTE_BITSET_SIZE(size) \
	((size_t)(RTE_BITSET_NUM_WORDS(size) * RTE_BITSET_WORD_SIZE))

#define __RTE_BITSET_WORD_IDX(bit_num) ((bit_num) / RTE_BITSET_WORD_BITS)
#define __RTE_BITSET_BIT_OFFSET(bit_num) ((bit_num) % RTE_BITSET_WORD_BITS)
#define __RTE_BITSET_UNUSED(size) \
	((RTE_BITSET_NUM_WORDS(size) * RTE_BITSET_WORD_BITS) - (size))
#define __RTE_BITSET_USED_MASK(size) \
	(UINT64_MAX >> __RTE_BITSET_UNUSED(size))

#define __RTE_BITSET_DELEGATE_N(fun, bitset, bit_num, ...) \
	fun(&(bitset)[__RTE_BITSET_WORD_IDX(bit_num)], __RTE_BITSET_BIT_OFFSET(bit_num), \
		__VA_ARGS__)

/* MSVC doesn't have ##__VA_ARGS__, so argument-less -> special case */
#define __RTE_BITSET_DELEGATE(fun, bitset, bit_num) \
	fun(&(bitset)[__RTE_BITSET_WORD_IDX(bit_num)], __RTE_BITSET_BIT_OFFSET(bit_num))

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Declare a bitset.
 *
 * Declare (e.g., as a struct field) or define (e.g., as a stack
 * variable) a bitset of the specified size.
 *
 * @param size
 *   The number of bits the bitset must be able to represent. Must be
 *   a compile-time constant.
 * @param name
 *   The field or variable name of the resulting definition.
 */
#define RTE_BITSET_DECLARE(name, size) \
	uint64_t name[RTE_BITSET_NUM_WORDS(size)]

#define __RTE_BITSET_FOREACH_LEFT(var, size, start_bit, len) \
	((len) - 1 - ((var) >= (start_bit) ? (var) - (start_bit) : (size) - (start_bit) + (var)))

#define __RTE_BITSET_FOREACH(var, bitset, size, start_bit, len, flags) \
	for ((var) = __rte_bitset_find(bitset, size, start_bit, len, flags); \
			(var) != -1; \
			(var) = __RTE_BITSET_FOREACH_LEFT(var, size, start_bit, len) > 0 ? \
				__rte_bitset_find(bitset, size, ((var) + 1) % (size), \
				__RTE_BITSET_FOREACH_LEFT(var, size, start_bit, len), flags) : -1)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Iterate over all bits set.
 *
 * This macro iterates over all bits set (i.e., all ones) in the
 * bitset, in the forward direction (i.e., starting with the least
 * significant '1').
 *
 * @param var
 *   An iterator variable of type @c ssize_t. For each successive
 *   iteration, this variable will hold the bit index of a set bit.
 * @param bitset
 *   A <tt>const uint64_t *</tt> pointer to the bitset array.
 * @param size
 *   The size of the bitset (in bits).
 */
#define RTE_BITSET_FOREACH_SET(var, bitset, size) \
	__RTE_BITSET_FOREACH(var, bitset, size, 0, size, 0)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Iterate over all bits cleared.
 *
 * This macro iterates over all bits cleared in the bitset, in the
 * forward direction (i.e., starting with the lowest-indexed set bit).
 *
 * @param var
 *   An iterator variable of type @c ssize_t. For each successive iteration,
 *   this variable will hold the bit index of a cleared bit.
 * @param bitset
 *   A <tt>const uint64_t *</tt> pointer to the bitset array.
 * @param size
 *   The size of the bitset (in bits).
 */
#define RTE_BITSET_FOREACH_CLEAR(var, bitset, size) \
	__RTE_BITSET_FOREACH(var, bitset, size, 0, size, __RTE_BITSET_FIND_FLAG_FIND_CLEAR)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Iterate over all bits set within a range.
 *
 * This macro iterates over all bits set (i.e., all ones) in the
 * specified range, in the forward direction (i.e., starting with the
 * least significant '1').
 *
 * @param var
 *   An iterator variable of type @c ssize_t. For each successive iteration,
 *   this variable will hold the bit index of a set bit.
 * @param bitset
 *   A <tt>const uint64_t *</tt> pointer to the bitset array.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The length (in bits) of the range. @c start_bit + @c len must be less
 *   than or equal to @c size.
 */
#define RTE_BITSET_FOREACH_SET_RANGE(var, bitset, size, start_bit, len) \
	__RTE_BITSET_FOREACH(var, bitset, size, start_bit, len, 0)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Iterate over all cleared bits within a range.
 *
 * This macro iterates over all bits cleared (i.e., all zeroes) in the
 * specified range, in the forward direction (i.e., starting with the
 * least significant '0').
 *
 * @param var
 *   An iterator variable of type @c ssize_t. For each successive iteration,
 *   this variable will hold the bit index of a set bit.
 * @param bitset
 *   A <tt>const uint64_t *</tt> pointer to the bitset array.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The length (in bits) of the range. @c start_bit + @c len must be less
 *   than or equal to @c size.
 */
#define RTE_BITSET_FOREACH_CLEAR_RANGE(var, bitset, size, start_bit, len) \
	__RTE_BITSET_FOREACH(var, bitset, size, start_bit, len, __RTE_BITSET_FIND_FLAG_FIND_CLEAR)

#define RTE_BITSET_FOREACH_SET_WRAP(var, bitset, size, start_bit, len) \
	__RTE_BITSET_FOREACH(var, bitset, size, start_bit, len, __RTE_BITSET_FIND_FLAG_WRAP)

#define RTE_BITSET_FOREACH_CLEAR_WRAP(var, bitset, size, start_bit, len) \
	__RTE_BITSET_FOREACH(var, bitset, size, start_bit, len, \
		__RTE_BITSET_FIND_FLAG_WRAP | __RTE_BITSET_FIND_FLAG_FIND_CLEAR)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initializes a bitset.
 *
 * All bits are cleared.
 *
 * In case all words in the bitset array are already set to zero by
 * other means (e.g., at the time of memory allocation), this function
 * need not be called.
 *
 * @param bitset
 *   A pointer to the array of bitset 64-bit words.
 * @param size
 *   The size of the bitset (in bits).
 */
__rte_experimental
static inline void
rte_bitset_init(uint64_t *bitset, size_t size)
{
	memset(bitset, 0, RTE_BITSET_SIZE(size));
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Test if a bit is set.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   Index of the bit to test. Index 0 is the least significant bit.
 * @return
 *   Returns true if the bit is '1', and false if the bit is '0'.
 */
__rte_experimental
static inline bool
rte_bitset_test(const uint64_t *bitset, size_t bit_num)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __RTE_BITSET_DELEGATE(rte_bit_test, bitset, bit_num);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set a bit in the bitset.
 *
 * Bits are numbered from 0 to (size - 1) (inclusive).
 *
 * The operation is not guaranteed to be atomic.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be set.
 */
__rte_experimental
static inline void
rte_bitset_set(uint64_t *bitset, size_t bit_num)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE(rte_bit_set, bitset, bit_num);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Clear a bit in the bitset.
 *
 * Bits are numbered 0 to (size - 1) (inclusive).
 *
 * The operation is not guaranteed to be atomic.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be cleared.
 */
__rte_experimental
static inline void
rte_bitset_clear(uint64_t *bitset, size_t bit_num)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE(rte_bit_clear, bitset, bit_num);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set or clear a bit in the bitset.
 *
 * Bits are numbered 0 to (size - 1) (inclusive).
 *
 * The operation is not guaranteed to be atomic.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be set or cleared.
 * @param bit_value
 *   Control if the bit should be set or cleared.
 */
__rte_experimental
static inline void
rte_bitset_assign(uint64_t *bitset, size_t bit_num, bool bit_value)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE_N(rte_bit_assign, bitset, bit_num, bit_value);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(bit_value);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Change the value of a bit in the bitset.
 *
 * Bits are numbered 0 to (size - 1) (inclusive).
 *
 * The operation is not guaranteed to be atomic.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be flipped.
 */
__rte_experimental
static inline void
rte_bitset_flip(uint64_t *bitset, size_t bit_num)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE(rte_bit_flip, bitset, bit_num);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically test if a bit is set.
 *
 * Atomically test if a bit in a bitset is set with the specified
 * memory ordering.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   Index of the bit to test. Index 0 is the least significant bit.
 * @param memory_order
 *   The memory order to use.
 * @return
 *   Returns true if the bit is '1', and false if the bit is '0'.
 */
__rte_experimental
static inline bool
rte_bitset_atomic_test(const uint64_t *bitset, size_t bit_num, int memory_order)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __RTE_BITSET_DELEGATE_N(rte_bit_atomic_test, bitset, bit_num, memory_order);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(memory_order);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically set a bit in the bitset.
 *
 * Set a bit in a bitset as an atomic operation, with the specified
 * memory ordering.
 *
 * rte_bitset_atomic_set() is multi-thread safe, provided all threads
 * acting in parallel on the same bitset does so through
 * @c rte_bitset_atomic_*() functions.
 *
 * Bits are numbered from 0 to (size - 1) (inclusive).
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be set.
 * @param memory_order
 *   The memory order to use.
 */
__rte_experimental
static inline void
rte_bitset_atomic_set(uint64_t *bitset, size_t bit_num, int memory_order)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE_N(rte_bit_atomic_set, bitset, bit_num, memory_order);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(memory_order);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically clear a bit in the bitset.
 *
 * Clear a bit in a bitset as an atomic operation, with the specified
 * memory ordering.
 *
 * rte_bitset_atomic_clear() is multi-thread safe, provided all
 * threads acting in parallel on the same bitset does so through @c
 * rte_bitset_atomic_*() functions.
 *
 * Bits are numbered from 0 to (size - 1) (inclusive).
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be cleared.
 * @param memory_order
 *   The memory order to use.
 */
__rte_experimental
static inline void
rte_bitset_atomic_clear(uint64_t *bitset, size_t bit_num, int memory_order)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE_N(rte_bit_atomic_clear, bitset, bit_num, memory_order);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(memory_order);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically set or clear a bit in the bitset.
 *
 * Assign a value to a bit in a bitset as an atomic operation, with
 * the specified memory ordering.
 *
 * rte_bitset_atomic_assign() is multi-thread safe, provided all
 * threads acting in parallel on the same bitset does so through
 * @c rte_bitset_atomic_*() functions.
 *
 * Bits are numbered from 0 to (size - 1) (inclusive).
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be set or cleared.
 * @param bit_value
 *   Control if the bit should be set or cleared.
 * @param memory_order
 *   The memory order to use.
 */
__rte_experimental
static inline void
rte_bitset_atomic_assign(uint64_t *bitset, size_t bit_num, bool bit_value, int memory_order)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE_N(rte_bit_atomic_assign, bitset, bit_num, bit_value, memory_order);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(bit_value);
	RTE_SET_USED(memory_order);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Atomically change the value of a bit in the bitset.
 *
 * Flip a bit in a bitset as an atomic operation, with the specified
 * memory ordering.
 *
 * rte_bitset_atomic_flip() is multi-thread safe, provided all threads
 * acting in parallel on the same bitset does so through
 * @c rte_bitset_atomic_*() functions.
 *
 * Bits are numbered from 0 to (size - 1) (inclusive).
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param bit_num
 *   The index of the bit to be flipped.
 * @param memory_order
 *   The memory order to use.
 */
__rte_experimental
static inline void
rte_bitset_atomic_flip(uint64_t *bitset, size_t bit_num, int memory_order)
{
#ifdef ALLOW_EXPERIMENTAL_API
	__RTE_BITSET_DELEGATE_N(rte_bit_atomic_flip, bitset, bit_num, memory_order);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(bit_num);
	RTE_SET_USED(memory_order);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set all bits in the bitset.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 */
__rte_experimental
static inline void
rte_bitset_set_all(uint64_t *bitset, size_t size)
{
	memset(bitset, 0xFF, RTE_BITSET_SIZE(size));
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Clear all bits in the bitset.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 */
__rte_experimental
static inline void
rte_bitset_clear_all(uint64_t *bitset, size_t size)
{
#ifdef ALLOW_EXPERIMENTAL_API
	rte_bitset_init(bitset, size);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Count all set bits (also known as the @e weight).
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @return
 *   Returns the number of '1' bits in the bitset.
 */
__rte_experimental
static inline size_t
rte_bitset_count_set(const uint64_t *bitset, size_t size)
{
	size_t i;
	size_t total = 0;

	/*
	 * Unused bits in a rte_bitset are always '0', and thus are
	 * not included in this count.
	 */
	for (i = 0; i < RTE_BITSET_NUM_WORDS(size) - 1; i++)
		total += rte_popcount64(bitset[i]);

	total += rte_popcount64(bitset[i] & __RTE_BITSET_USED_MASK(size));

	return total;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Count all cleared bits.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @return
 *   Returns the number of '0' bits in the bitset.
 */
__rte_experimental
static inline size_t
rte_bitset_count_clear(const uint64_t *bitset, size_t size)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return size - rte_bitset_count_set(bitset, size);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_VERIFY(false);
#endif
}

#define __RTE_BITSET_FIND_FLAG_FIND_CLEAR (1U << 0)
#define __RTE_BITSET_FIND_FLAG_WRAP (1U << 1)

__rte_experimental
static inline ssize_t
__rte_bitset_find_nowrap(const uint64_t *bitset, size_t __rte_unused size, size_t start_bit,
		size_t len, bool find_clear)
{
	size_t word_idx;
	size_t offset;
	size_t end_bit = start_bit + len;

	RTE_ASSERT(end_bit <= size);

	if (unlikely(len == 0))
		return -1;

	word_idx = __RTE_BITSET_WORD_IDX(start_bit);
	offset = __RTE_BITSET_BIT_OFFSET(start_bit);

	while (word_idx <= __RTE_BITSET_WORD_IDX(end_bit - 1)) {
		uint64_t word;

		word = bitset[word_idx];
		if (find_clear)
			word = ~word;

		word >>= offset;

		if (word != 0) {
			size_t ffs = start_bit + rte_bsf64(word);

			/*
			 * Check if set bit were among the last,
			 * unused bits, in the last word.
			 */
			if (unlikely(ffs >= end_bit))
				return -1;

			return ffs;
		}

		start_bit += (RTE_BITSET_WORD_BITS - offset);
		word_idx++;
		offset = 0;
	}

	return -1;

}

__rte_experimental
static inline ssize_t
__rte_bitset_find(const uint64_t *bitset, size_t size, size_t start_bit, size_t len,
		unsigned int flags)
{
#ifdef ALLOW_EXPERIMENTAL_API
	bool find_clear = flags & __RTE_BITSET_FIND_FLAG_FIND_CLEAR;
	bool may_wrap = flags & __RTE_BITSET_FIND_FLAG_WRAP;
	bool does_wrap = (start_bit + len) > size;
	ssize_t rc;

	RTE_ASSERT(len <= size);
	if (!may_wrap)
		RTE_ASSERT(!does_wrap);

	if (may_wrap && does_wrap) {
		size_t len0 = size - start_bit;
		size_t len1 = len - len0;

		rc = __rte_bitset_find_nowrap(bitset, size, start_bit, len0, find_clear);
		if (rc < 0)
			rc =  __rte_bitset_find_nowrap(bitset, size, 0, len1, find_clear);
	} else
		rc = __rte_bitset_find_nowrap(bitset, size, start_bit, len, find_clear);

	return rc;
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_SET_USED(start_bit);
	RTE_SET_USED(len);
	RTE_SET_USED(flags);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first bit set.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), and returns the index of the first '1'.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @return
 *   Returns the index of the least significant '1', or -1 if all
 *   bits are '0'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_first_set(const uint64_t *bitset, size_t size)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, 0, size, 0);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first bit set at offset.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), starting at an offset @c start_bit into the
 * bitset, and returns the index of the first '1' encountered.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The number of bits to scan. @c start_bit + @c len must be less
 *   than or equal to @c size.
 * @return
 *   Returns the index of the least significant '1', or -1 if all
 *   bits are '0'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_set(const uint64_t *bitset, size_t size, size_t start_bit, size_t len)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, start_bit, len, 0);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_SET_USED(start_bit);
	RTE_SET_USED(len);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first bit set at offset, with wrap-around.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), starting at an offset @c start_bit into the
 * bitset. If no '1' is encountered before the end of the bitset, the search
 * will continue at index 0.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The number of bits to scan. @c start_bit + @c len must be less
 *   than or equal to @c size.
 * @return
 *   Returns the index of the least significant '1', or -1 if all
 *   bits are '0'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_set_wrap(const uint64_t *bitset, size_t size, size_t start_bit, size_t len)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, start_bit, len, __RTE_BITSET_FIND_FLAG_WRAP);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_SET_USED(start_bit);
	RTE_SET_USED(len);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first cleared bit.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), and returns the index of the first '0'.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @return
 *   Returns the index of the least significant '0', or -1 if all
 *   bits are '1'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_first_clear(const uint64_t *bitset, size_t size)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, 0, size, __RTE_BITSET_FIND_FLAG_FIND_CLEAR);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first cleared bit at offset.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), starting at an offset @c start_bit into the
 * bitset, and returns the index of the first '0' encountered.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The number of bits to scan. @c start_bit + @c len must be less
 *   than or equal to @c size.
 * @return
 *   Returns the index of the least significant '0', or -1 if all
 *   bits are '1'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_clear(const uint64_t *bitset, size_t size, size_t start_bit, size_t len)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, start_bit, len, __RTE_BITSET_FIND_FLAG_FIND_CLEAR);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_SET_USED(start_bit);
	RTE_SET_USED(len);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find first cleared bit at offset, with wrap-around.
 *
 * Scans the bitset in the forward direction (i.e., starting at the
 * least significant bit), starting at an offset @c start_bit into the
 * bitset. If no '0' is encountered before the end of the bitset, the
 * search will continue at index 0.
 *
 * @param bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitset (in bits).
 * @param start_bit
 *   The index of the first bit to check. Must be less than @c size.
 * @param len
 *   The number of bits to scan. @c start_bit + @c len must be less
 *   than or equal to @c size.
 * @return
 *   Returns the index of the least significant '0', or -1 if all
 *   bits are '1'.
 */
__rte_experimental
static inline ssize_t
rte_bitset_find_clear_wrap(const uint64_t *bitset, size_t size, size_t start_bit, size_t len)
{
#ifdef ALLOW_EXPERIMENTAL_API
	return __rte_bitset_find(bitset, size, start_bit, len,
		__RTE_BITSET_FIND_FLAG_FIND_CLEAR | __RTE_BITSET_FIND_FLAG_WRAP);
#else
	RTE_SET_USED(bitset);
	RTE_SET_USED(size);
	RTE_SET_USED(start_bit);
	RTE_SET_USED(len);
	RTE_VERIFY(false);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Copy bitset.
 *
 * Copy the bits of the @c src_bitset to the @c dst_bitset.
 *
 * The bitsets may not overlap and must be of equal size.
 *
 * @param dst_bitset
 *   A pointer to the array of words making up the bitset.
 * @param src_bitset
 *   A pointer to the array of words making up the bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline void
rte_bitset_copy(uint64_t *__rte_restrict dst_bitset, const uint64_t *__rte_restrict src_bitset,
		size_t size)
{
	rte_memcpy(dst_bitset, src_bitset, RTE_BITSET_SIZE(size));
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Bitwise or two bitsets.
 *
 * Perform a bitwise OR operation on all bits in the two equal-size
 * bitsets @c src_bitset0 and @c src_bitset1, and store the results in
 * @c dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset0
 *   A pointer to the first source bitset.
 * @param src_bitset1
 *   A pointer to the second source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline void
rte_bitset_or(uint64_t *dst_bitset, const uint64_t *src_bitset0, const uint64_t *src_bitset1,
		size_t size)
{
	size_t i;

	for (i = 0; i < RTE_BITSET_NUM_WORDS(size); i++)
		dst_bitset[i] = src_bitset0[i] | src_bitset1[i];
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Bitwise and two bitsets.
 *
 * Perform a bitwise AND operation on all bits in the two equal-size
 * bitsets @c src_bitset0 and @c src_bitset1, and store the result in
 * @c dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset0
 *   A pointer to the first source bitset.
 * @param src_bitset1
 *   A pointer to the second source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline void
rte_bitset_and(uint64_t *dst_bitset, const uint64_t *src_bitset0, const uint64_t *src_bitset1,
		size_t size)
{
	size_t i;

	for (i = 0; i < RTE_BITSET_NUM_WORDS(size); i++)
		dst_bitset[i] = src_bitset0[i] & src_bitset1[i];
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Bitwise xor two bitsets.
 *
 * Perform a bitwise XOR operation on all bits in the two equal-size
 * bitsets @c src_bitset0 and @c src_bitset1, and store the result in
 * @c dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset0
 *   A pointer to the first source bitset.
 * @param src_bitset1
 *   A pointer to the second source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline void
rte_bitset_xor(uint64_t *dst_bitset, const uint64_t *src_bitset0, const uint64_t *src_bitset1,
		size_t size)
{
	size_t i;

	for (i = 0; i < RTE_BITSET_NUM_WORDS(size); i++)
		dst_bitset[i] = src_bitset0[i] ^ src_bitset1[i];
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Compute the bitwise complement of a bitset.
 *
 * Flip every bit in the @c src_bitset, and store the result in @c
 * dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset
 *   A pointer to the source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline void
rte_bitset_complement(uint64_t *dst_bitset, const uint64_t *src_bitset, size_t size)
{
	size_t i;

	for (i = 0; i < RTE_BITSET_NUM_WORDS(size); i++)
		dst_bitset[i] = ~src_bitset[i];
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Shift bitset left.
 *
 * Perform a logical shift left of (multiply) @c src_bitset, and store
 * the result in @c dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset
 *   A pointer to the source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 * @param shift_bits
 *   The number of bits to shift the bitset.
 */
__rte_experimental
static inline void
rte_bitset_shift_left(uint64_t *dst_bitset, const uint64_t *src_bitset, size_t size,
		size_t shift_bits)
{
	const int src_word_offset = shift_bits / RTE_BITSET_WORD_BITS;
	const int src_bit_offset = shift_bits % RTE_BITSET_WORD_BITS;
	unsigned int dst_idx;

	for (dst_idx = 0; dst_idx < RTE_BITSET_NUM_WORDS(size); dst_idx++) {
		int src_high_idx = dst_idx - src_word_offset;
		uint64_t low_bits = 0;
		uint64_t high_bits = 0;

		if (src_high_idx >= 0) {
			int src_low_idx = src_high_idx - 1;

			high_bits = src_bitset[src_high_idx] << src_bit_offset;

			if (src_bit_offset > 0 && src_low_idx >= 0)
				low_bits = src_bitset[src_low_idx] >>
					(RTE_BITSET_WORD_BITS - src_bit_offset);
		}
		dst_bitset[dst_idx] = low_bits | high_bits;
	}
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Shift bitset right.
 *
 * Perform a logical shift right of (divide) @c src_bitset, and store
 * the result in @c dst_bitset.
 *
 * @param dst_bitset
 *   A pointer to the destination bitset.
 * @param src_bitset
 *   A pointer to the source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 * @param shift_bits
 *   The number of bits to shift the bitset.
 */
__rte_experimental
static inline void
rte_bitset_shift_right(uint64_t *dst_bitset, const uint64_t *src_bitset, size_t size,
		size_t shift_bits)
{
	const int num_words = RTE_BITSET_NUM_WORDS(size);
	const uint64_t used_mask = __RTE_BITSET_USED_MASK(size);
	const int src_word_offset = shift_bits / RTE_BITSET_WORD_BITS;
	const int src_bit_offset = shift_bits % RTE_BITSET_WORD_BITS;
	int dst_idx;

	for (dst_idx = 0; dst_idx < num_words; dst_idx++) {
		int src_low_idx = src_word_offset + dst_idx;
		int src_high_idx = src_low_idx + 1;
		uint64_t src_low_word_bits = 0;
		uint64_t src_high_word_bits = 0;

		if (src_low_idx < num_words) {
			src_low_word_bits = src_bitset[src_low_idx];

			if (src_low_idx == (num_words - 1))
				src_low_word_bits &= used_mask;

			src_low_word_bits >>= src_bit_offset;

			if (src_bit_offset > 0 && src_high_idx < num_words) {
				src_high_word_bits = src_bitset[src_high_idx];

				if (src_high_idx == (num_words - 1))
					src_high_word_bits &= used_mask;

				src_high_word_bits <<= (RTE_BITSET_WORD_BITS - src_bit_offset);
			}
		}
		dst_bitset[dst_idx] = src_low_word_bits | src_high_word_bits;
	}
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Compare two bitsets.
 *
 * Compare two bitsets for equality.
 *
 * @param bitset_a
 *   A pointer to the destination bitset.
 * @param bitset_b
 *   A pointer to the source bitset.
 * @param size
 *   The size of the bitsets (in bits).
 */
__rte_experimental
static inline bool
rte_bitset_equal(const uint64_t *bitset_a, const uint64_t *bitset_b, size_t size)
{
	size_t i;
	uint64_t last_a, last_b;

	for (i = 0; i < RTE_BITSET_NUM_WORDS(size) - 1; i++)
		if (bitset_a[i] != bitset_b[i])
			return false;

	last_a = bitset_a[i] << __RTE_BITSET_UNUSED(size);
	last_b = bitset_b[i] << __RTE_BITSET_UNUSED(size);

	return last_a == last_b;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Converts a bitset to a string.
 *
 * This function prints a string representation of the bitstring to
 * the supplied buffer.
 *
 * Each bit is represented either by '0' or '1' in the output, with
 * the first (left-most) character in the output being the most
 * significant bit. The resulting string is NUL terminated.
 *
 * @param bitset
 *   A pointer to the array of bitset 64-bit words.
 * @param size
 *   The number of bits the bitset represent.
 * @param buf
 *   A buffer to hold the output.
 * @param capacity
 *   The size of the buffer. Must be @c size + 1 or larger.
 * @return
 *   Returns the number of bytes written (i.e., @c size + 1), or -EINVAL
 *   in case the buffer capacity was too small.
 */
__rte_experimental
ssize_t
rte_bitset_to_str(const uint64_t *bitset, size_t size, char *buf, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BITSET_H_ */
