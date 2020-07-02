/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include "bitalloc.h"

#define BITALLOC_MAX_LEVELS 6


/* Finds the last bit set plus 1, equivalent to gcc __builtin_fls */
static int
ba_fls(bitalloc_word_t v)
{
	int c = 32;

	if (!v)
		return 0;

	if (!(v & 0xFFFF0000u)) {
		v <<= 16;
		c -= 16;
	}
	if (!(v & 0xFF000000u)) {
		v <<= 8;
		c -= 8;
	}
	if (!(v & 0xF0000000u)) {
		v <<= 4;
		c -= 4;
	}
	if (!(v & 0xC0000000u)) {
		v <<= 2;
		c -= 2;
	}
	if (!(v & 0x80000000u)) {
		v <<= 1;
		c -= 1;
	}

	return c;
}

/* Finds the first bit set plus 1, equivalent to gcc __builtin_ffs */
static int
ba_ffs(bitalloc_word_t v)
{
	int c; /* c will be the number of zero bits on the right plus 1 */

	v &= -v;
	c = v ? 32 : 0;

	if (v & 0x0000FFFF)
		c -= 16;
	if (v & 0x00FF00FF)
		c -= 8;
	if (v & 0x0F0F0F0F)
		c -= 4;
	if (v & 0x33333333)
		c -= 2;
	if (v & 0x55555555)
		c -= 1;

	return c;
}

int
ba_init(struct bitalloc *pool, int size)
{
	bitalloc_word_t *mem = (bitalloc_word_t *)pool;
	int       i;

	/* Initialize */
	pool->size = 0;

	if (size < 1 || size > BITALLOC_MAX_SIZE)
		return -1;

	/* Zero structure */
	for (i = 0;
	     i < (int)(BITALLOC_SIZEOF(size) / sizeof(bitalloc_word_t));
	     i++)
		mem[i] = 0;

	/* Initialize */
	pool->size = size;

	/* Embed number of words of next level, after each level */
	int words[BITALLOC_MAX_LEVELS];
	int lev = 0;
	int offset = 0;

	words[0] = (size + 31) / 32;
	while (words[lev] > 1) {
		lev++;
		words[lev] = (words[lev - 1] + 31) / 32;
	}

	while (lev) {
		offset += words[lev];
		pool->storage[offset++] = words[--lev];
	}

	/* Free the entire pool */
	for (i = 0; i < size; i++)
		ba_free(pool, i);

	return 0;
}

static int
ba_alloc_helper(struct bitalloc *pool,
		int              offset,
		int              words,
		unsigned int     size,
		int              index,
		int             *clear)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int       loc = ba_ffs(storage[index]);
	int       r;

	if (loc == 0)
		return -1;

	loc--;

	if (pool->size > size) {
		r = ba_alloc_helper(pool,
				    offset + words + 1,
				    storage[words],
				    size * 32,
				    index * 32 + loc,
				    clear);
	} else {
		r = index * 32 + loc;
		*clear = 1;
		pool->free_count--;
	}

	if (*clear) {
		storage[index] &= ~(1 << loc);
		*clear = (storage[index] == 0);
	}

	return r;
}

int
ba_alloc(struct bitalloc *pool)
{
	int clear = 0;

	return ba_alloc_helper(pool, 0, 1, 32, 0, &clear);
}

/**
 * Help function to alloc entry from highest available index
 *
 * Searching the pool from highest index for the empty entry.
 *
 * [in] pool
 *   Pointer to the resource pool
 *
 * [in] offset
 *   Offset of the storage in the pool
 *
 * [in] words
 *   Number of words in this level
 *
 * [in] size
 *   Number of entries in this level
 *
 * [in] index
 *   Index of words that has the entry
 *
 * [in] clear
 *   Indicate if a bit needs to be clear due to the entry is allocated
 *
 * Returns:
 *     0 - Success
 *    -1 - Failure
 */
static int
ba_alloc_reverse_helper(struct bitalloc *pool,
			int offset,
			int words,
			unsigned int size,
			int index,
			int *clear)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int loc = ba_fls(storage[index]);
	int r;

	if (loc == 0)
		return -1;

	loc--;

	if (pool->size > size) {
		r = ba_alloc_reverse_helper(pool,
					    offset + words + 1,
					    storage[words],
					    size * 32,
					    index * 32 + loc,
					    clear);
	} else {
		r = index * 32 + loc;
		*clear = 1;
		pool->free_count--;
	}

	if (*clear) {
		storage[index] &= ~(1 << loc);
		*clear = (storage[index] == 0);
	}

	return r;
}

int
ba_alloc_reverse(struct bitalloc *pool)
{
	int clear = 0;

	return ba_alloc_reverse_helper(pool, 0, 1, 32, 0, &clear);
}

static int
ba_alloc_index_helper(struct bitalloc *pool,
		      int              offset,
		      int              words,
		      unsigned int     size,
		      int             *index,
		      int             *clear)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int       loc;
	int       r;

	if (pool->size > size)
		r = ba_alloc_index_helper(pool,
					  offset + words + 1,
					  storage[words],
					  size * 32,
					  index,
					  clear);
	else
		r = 1; /* Check if already allocated */

	loc = (*index % 32);
	*index = *index / 32;

	if (r == 1) {
		r = (storage[*index] & (1 << loc)) ? 0 : -1;
		if (r == 0) {
			*clear = 1;
			pool->free_count--;
		}
	}

	if (*clear) {
		storage[*index] &= ~(1 << loc);
		*clear = (storage[*index] == 0);
	}

	return r;
}

int
ba_alloc_index(struct bitalloc *pool, int index)
{
	int clear = 0;
	int index_copy = index;

	if (index < 0 || index >= (int)pool->size)
		return -1;

	if (ba_alloc_index_helper(pool, 0, 1, 32, &index_copy, &clear) >= 0)
		return index;
	else
		return -1;
}

static int
ba_inuse_helper(struct bitalloc *pool,
		int              offset,
		int              words,
		unsigned int     size,
		int             *index)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int       loc;
	int       r;

	if (pool->size > size)
		r = ba_inuse_helper(pool,
				    offset + words + 1,
				    storage[words],
				    size * 32,
				    index);
	else
		r = 1; /* Check if in use */

	loc = (*index % 32);
	*index = *index / 32;

	if (r == 1)
		r = (storage[*index] & (1 << loc)) ? -1 : 0;

	return r;
}

int
ba_inuse(struct bitalloc *pool, int index)
{
	if (index < 0 || index >= (int)pool->size)
		return -1;

	return ba_inuse_helper(pool, 0, 1, 32, &index) == 0;
}

static int
ba_free_helper(struct bitalloc *pool,
	       int              offset,
	       int              words,
	       unsigned int     size,
	       int             *index)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int       loc;
	int       r;

	if (pool->size > size)
		r = ba_free_helper(pool,
				   offset + words + 1,
				   storage[words],
				   size * 32,
				   index);
	else
		r = 1; /* Check if already free */

	loc = (*index % 32);
	*index = *index / 32;

	if (r == 1) {
		r = (storage[*index] & (1 << loc)) ? -1 : 0;
		if (r == 0)
			pool->free_count++;
	}

	if (r == 0)
		storage[*index] |= (1 << loc);

	return r;
}

int
ba_free(struct bitalloc *pool, int index)
{
	if (index < 0 || index >= (int)pool->size)
		return -1;

	return ba_free_helper(pool, 0, 1, 32, &index);
}

int
ba_inuse_free(struct bitalloc *pool, int index)
{
	if (index < 0 || index >= (int)pool->size)
		return -1;

	return ba_free_helper(pool, 0, 1, 32, &index) + 1;
}

int
ba_free_count(struct bitalloc *pool)
{
	return (int)pool->free_count;
}

int ba_inuse_count(struct bitalloc *pool)
{
	return (int)(pool->size) - (int)(pool->free_count);
}

static int
ba_find_next_helper(struct bitalloc *pool,
		    int              offset,
		    int              words,
		    unsigned int     size,
		    int             *index,
		    int              free)
{
	bitalloc_word_t *storage = &pool->storage[offset];
	int       loc, r, bottom = 0;

	if (pool->size > size)
		r = ba_find_next_helper(pool,
					offset + words + 1,
					storage[words],
					size * 32,
					index,
					free);
	else
		bottom = 1; /* Bottom of tree */

	loc = (*index % 32);
	*index = *index / 32;

	if (bottom) {
		int bit_index = *index * 32;

		loc = ba_ffs(~storage[*index] & ((bitalloc_word_t)-1 << loc));
		if (loc > 0) {
			loc--;
			r = (bit_index + loc);
			if (r >= (int)pool->size)
				r = -1;
		} else {
			/* Loop over array at bottom of tree */
			r = -1;
			bit_index += 32;
			*index = *index + 1;
			while ((int)pool->size > bit_index) {
				loc = ba_ffs(~storage[*index]);

				if (loc > 0) {
					loc--;
					r = (bit_index + loc);
					if (r >= (int)pool->size)
						r = -1;
					break;
				}
				bit_index += 32;
				*index = *index + 1;
			}
		}
	}

	if (r >= 0 && (free)) {
		if (bottom)
			pool->free_count++;
		storage[*index] |= (1 << loc);
	}

	return r;
}

int
ba_find_next_inuse(struct bitalloc *pool, int index)
{
	if (index < 0 ||
	    index >= (int)pool->size ||
	    pool->free_count == pool->size)
		return -1;

	return ba_find_next_helper(pool, 0, 1, 32, &index, 0);
}

int
ba_find_next_inuse_free(struct bitalloc *pool, int index)
{
	if (index < 0 ||
	    index >= (int)pool->size ||
	    pool->free_count == pool->size)
		return -1;

	return ba_find_next_helper(pool, 0, 1, 32, &index, 1);
}
