/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _BITALLOC_H_
#define _BITALLOC_H_

#include <stdint.h>

/* Bitalloc works on uint32_t as its word size */
typedef uint32_t bitalloc_word_t;

struct bitalloc {
	bitalloc_word_t size;
	bitalloc_word_t free_count;
	bitalloc_word_t storage[1];
};

#define BA_L0(s) (((s) + 31) / 32)
#define BA_L1(s) ((BA_L0(s) + 31) / 32)
#define BA_L2(s) ((BA_L1(s) + 31) / 32)
#define BA_L3(s) ((BA_L2(s) + 31) / 32)
#define BA_L4(s) ((BA_L3(s) + 31) / 32)

#define BITALLOC_SIZEOF(size)                                    \
	(sizeof(struct bitalloc) *				 \
	 (((sizeof(struct bitalloc) +				 \
	    sizeof(struct bitalloc) - 1 +			 \
	    (sizeof(bitalloc_word_t) *				 \
	     ((BA_L0(size) - 1) +				 \
	      ((BA_L0(size) == 1) ? 0 : (BA_L1(size) + 1)) +	 \
	      ((BA_L1(size) == 1) ? 0 : (BA_L2(size) + 1)) +	 \
	      ((BA_L2(size) == 1) ? 0 : (BA_L3(size) + 1)) +	 \
	      ((BA_L3(size) == 1) ? 0 : (BA_L4(size) + 1)))))) / \
	  sizeof(struct bitalloc)))

#define BITALLOC_MAX_SIZE (32 * 32 * 32 * 32 * 32 * 32)

/* The instantiation of a bitalloc looks a bit odd. Since a
 * bit allocator has variable storage, we need a way to get a
 * a pointer to a bitalloc structure that points to the correct
 * amount of storage. We do this by creating an array of
 * bitalloc where the first element in the array is the
 * actual bitalloc base structure, and the remaining elements
 * in the array provide the storage for it. This approach allows
 * instances to be individual variables or members of larger
 * structures.
 */
#define BITALLOC_INST(name, size)                      \
	struct bitalloc name[(BITALLOC_SIZEOF(size) /  \
			      sizeof(struct bitalloc))]

/* Symbolic return codes */
#define BA_SUCCESS           0
#define BA_FAIL             -1
#define BA_ENTRY_FREE        0
#define BA_ENTRY_IN_USE      1
#define BA_NO_ENTRY_FOUND   -1

/**
 * Initializates the bitallocator
 *
 * Returns 0 on success, -1 on failure.  Size is arbitrary up to
 * BITALLOC_MAX_SIZE
 */
int ba_init(struct bitalloc *pool, int size);

/**
 * Returns -1 on failure, or index of allocated entry
 */
int ba_alloc(struct bitalloc *pool);
int ba_alloc_index(struct bitalloc *pool, int index);

/**
 * Returns -1 on failure, or index of allocated entry
 */
int ba_alloc_reverse(struct bitalloc *pool);

/**
 * Query a particular index in a pool to check if its in use.
 *
 * Returns -1 on invalid index, 1 if the index is allocated, 0 if it
 * is free
 */
int ba_inuse(struct bitalloc *pool, int index);

/**
 * Variant of ba_inuse that frees the index if it is allocated, same
 * return codes as ba_inuse
 */
int ba_inuse_free(struct bitalloc *pool, int index);

/**
 * Find next index that is in use, start checking at index 'idx'
 *
 * Returns next index that is in use on success, or
 * -1 if no in use index is found
 */
int ba_find_next_inuse(struct bitalloc *pool, int idx);

/**
 * Variant of ba_find_next_inuse that also frees the next in use index,
 * same return codes as ba_find_next_inuse
 */
int ba_find_next_inuse_free(struct bitalloc *pool, int idx);

/**
 * Multiple freeing of the same index has no negative side effects,
 * but will return -1.  returns -1 on failure, 0 on success.
 */
int ba_free(struct bitalloc *pool, int index);

/**
 * Returns the pool's free count
 */
int ba_free_count(struct bitalloc *pool);

/**
 * Returns the pool's in use count
 */
int ba_inuse_count(struct bitalloc *pool);

#endif /* _BITALLOC_H_ */
