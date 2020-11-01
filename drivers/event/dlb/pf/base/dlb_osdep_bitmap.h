/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_OSDEP_BITMAP_H__
#define __DLB_OSDEP_BITMAP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <rte_bitmap.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include "../dlb_main.h"

/*************************/
/*** Bitmap operations ***/
/*************************/
struct dlb_bitmap {
	struct rte_bitmap *map;
	unsigned int len;
	struct dlb_hw *hw;
};

/**
 * dlb_bitmap_alloc() - alloc a bitmap data structure
 * @bitmap: pointer to dlb_bitmap structure pointer.
 * @len: number of entries in the bitmap.
 *
 * This function allocates a bitmap and initializes it with length @len. All
 * entries are initially zero.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or len is 0.
 * ENOMEM - could not allocate memory for the bitmap data structure.
 */
static inline int dlb_bitmap_alloc(struct dlb_hw *hw,
				   struct dlb_bitmap **bitmap,
				   unsigned int len)
{
	struct dlb_bitmap *bm;
	void *mem;
	uint32_t alloc_size;
	uint32_t nbits = (uint32_t) len;
	RTE_SET_USED(hw);

	if (bitmap == NULL || nbits == 0)
		return -EINVAL;

	/* Allocate DLB bitmap control struct */
	bm = rte_malloc("DLB_PF",
		sizeof(struct dlb_bitmap),
		RTE_CACHE_LINE_SIZE);

	if (bm == NULL)
		return -ENOMEM;

	/* Allocate bitmap memory */
	alloc_size = rte_bitmap_get_memory_footprint(nbits);
	mem = rte_malloc("DLB_PF_BITMAP", alloc_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		rte_free(bm);
		return -ENOMEM;
	}

	bm->map = rte_bitmap_init(len, mem, alloc_size);
	if (bm->map == NULL) {
		rte_free(mem);
		rte_free(bm);
		return -ENOMEM;
	}

	bm->len = len;

	*bitmap = bm;

	return 0;
}

/**
 * dlb_bitmap_free() - free a previously allocated bitmap data structure
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * This function frees a bitmap that was allocated with dlb_bitmap_alloc().
 */
static inline void dlb_bitmap_free(struct dlb_bitmap *bitmap)
{
	if (bitmap == NULL)
		return;

	rte_free(bitmap->map);
	rte_free(bitmap);
}

/**
 * dlb_bitmap_fill() - fill a bitmap with all 1s
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * This function sets all bitmap values to 1.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb_bitmap_fill(struct dlb_bitmap *bitmap)
{
	unsigned int i;

	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	for (i = 0; i != bitmap->len; i++)
		rte_bitmap_set(bitmap->map, i);

	return 0;
}

/**
 * dlb_bitmap_zero() - fill a bitmap with all 0s
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * This function sets all bitmap values to 0.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb_bitmap_zero(struct dlb_bitmap *bitmap)
{
	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	rte_bitmap_reset(bitmap->map);

	return 0;
}

/**
 * dlb_bitmap_set() - set a bitmap entry
 * @bitmap: pointer to dlb_bitmap structure.
 * @bit: bit index.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or bit is larger than the
 *	    bitmap length.
 */
static inline int dlb_bitmap_set(struct dlb_bitmap *bitmap,
				 unsigned int bit)
{
	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	rte_bitmap_set(bitmap->map, bit);

	return 0;
}

/**
 * dlb_bitmap_set_range() - set a range of bitmap entries
 * @bitmap: pointer to dlb_bitmap structure.
 * @bit: starting bit index.
 * @len: length of the range.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or the range exceeds the bitmap
 *	    length.
 */
static inline int dlb_bitmap_set_range(struct dlb_bitmap *bitmap,
				       unsigned int bit,
				       unsigned int len)
{
	unsigned int i;

	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	for (i = 0; i != len; i++)
		rte_bitmap_set(bitmap->map, bit + i);

	return 0;
}

/**
 * dlb_bitmap_clear() - clear a bitmap entry
 * @bitmap: pointer to dlb_bitmap structure.
 * @bit: bit index.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or bit is larger than the
 *	    bitmap length.
 */
static inline int dlb_bitmap_clear(struct dlb_bitmap *bitmap,
				   unsigned int bit)
{
	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	rte_bitmap_clear(bitmap->map, bit);

	return 0;
}

/**
 * dlb_bitmap_clear_range() - clear a range of bitmap entries
 * @bitmap: pointer to dlb_bitmap structure.
 * @bit: starting bit index.
 * @len: length of the range.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or the range exceeds the bitmap
 *	    length.
 */
static inline int dlb_bitmap_clear_range(struct dlb_bitmap *bitmap,
					 unsigned int bit,
					 unsigned int len)
{
	unsigned int i;

	if (bitmap  == NULL || bitmap->map == NULL)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	for (i = 0; i != len; i++)
		rte_bitmap_clear(bitmap->map, bit + i);

	return 0;
}

/**
 * dlb_bitmap_find_set_bit_range() - find a range of set bits
 * @bitmap: pointer to dlb_bitmap structure.
 * @len: length of the range.
 *
 * This function looks for a range of set bits of length @len.
 *
 * Return:
 * Returns the base bit index upon success, < 0 otherwise.
 *
 * Errors:
 * ENOENT - unable to find a length *len* range of set bits.
 * EINVAL - bitmap is NULL or is uninitialized, or len is invalid.
 */
static inline int dlb_bitmap_find_set_bit_range(struct dlb_bitmap *bitmap,
						unsigned int len)
{
	unsigned int i, j = 0;

	if (bitmap  == NULL || bitmap->map  == NULL || len == 0)
		return -EINVAL;

	if (bitmap->len < len)
		return -ENOENT;

	for (i = 0; i != bitmap->len; i++) {
		if  (rte_bitmap_get(bitmap->map, i)) {
			if (++j == len)
				return i - j + 1;
		} else
			j = 0;
	}

	/* No set bit range of length len? */
	return -ENOENT;
}

/**
 * dlb_bitmap_find_set_bit() - find the first set bit
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * This function looks for a single set bit.
 *
 * Return:
 * Returns the base bit index upon success, < 0 otherwise.
 *
 * Errors:
 * ENOENT - the bitmap contains no set bits.
 * EINVAL - bitmap is NULL or is uninitialized, or len is invalid.
 */
static inline int dlb_bitmap_find_set_bit(struct dlb_bitmap *bitmap)
{
	unsigned int i;

	if (bitmap == NULL)
		return -EINVAL;

	if (bitmap->map == NULL)
		return -EINVAL;

	for (i = 0; i != bitmap->len; i++) {
		if  (rte_bitmap_get(bitmap->map, i))
			return i;
	}

	return -ENOENT;
}

/**
 * dlb_bitmap_count() - returns the number of set bits
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * This function looks for a single set bit.
 *
 * Return:
 * Returns the number of set bits upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb_bitmap_count(struct dlb_bitmap *bitmap)
{
	int weight = 0;
	unsigned int i;

	if (bitmap == NULL)
		return -EINVAL;

	if (bitmap->map == NULL)
		return -EINVAL;

	for (i = 0; i != bitmap->len; i++) {
		if  (rte_bitmap_get(bitmap->map, i))
			weight++;
	}
	return weight;
}

/**
 * dlb_bitmap_longest_set_range() - returns longest contiguous range of set bits
 * @bitmap: pointer to dlb_bitmap structure.
 *
 * Return:
 * Returns the bitmap's longest contiguous range of set bits upon success,
 * <0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb_bitmap_longest_set_range(struct dlb_bitmap *bitmap)
{
	int max_len = 0, len = 0;
	unsigned int i;

	if (bitmap == NULL)
		return -EINVAL;

	if (bitmap->map == NULL)
		return -EINVAL;

	for (i = 0; i != bitmap->len; i++) {
		if  (rte_bitmap_get(bitmap->map, i)) {
			len++;
		} else {
			if (len > max_len)
				max_len = len;
			len = 0;
		}
	}

	if (len > max_len)
		max_len = len;

	return max_len;
}

/**
 * dlb_bitmap_or() - store the logical 'or' of two bitmaps into a third
 * @dest: pointer to dlb_bitmap structure, which will contain the results of
 *	  the 'or' of src1 and src2.
 * @src1: pointer to dlb_bitmap structure, will be 'or'ed with src2.
 * @src2: pointer to dlb_bitmap structure, will be 'or'ed with src1.
 *
 * This function 'or's two bitmaps together and stores the result in a third
 * bitmap. The source and destination bitmaps can be the same.
 *
 * Return:
 * Returns the number of set bits upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - One of the bitmaps is NULL or is uninitialized.
 */
static inline int dlb_bitmap_or(struct dlb_bitmap *dest,
				struct dlb_bitmap *src1,
				struct dlb_bitmap *src2)
{
	unsigned int i, min;
	int numset = 0;

	if (dest  == NULL || dest->map == NULL ||
	    src1 == NULL || src1->map == NULL ||
	    src2  == NULL || src2->map == NULL)
		return -EINVAL;

	min = dest->len;
	min = (min > src1->len) ? src1->len : min;
	min = (min > src2->len) ? src2->len : min;

	for (i = 0; i != min; i++) {
		if  (rte_bitmap_get(src1->map, i) ||
				rte_bitmap_get(src2->map, i)) {
			rte_bitmap_set(dest->map, i);
			numset++;
		} else
			rte_bitmap_clear(dest->map, i);
	}

	return numset;
}

#endif /*  __DLB_OSDEP_BITMAP_H__ */
