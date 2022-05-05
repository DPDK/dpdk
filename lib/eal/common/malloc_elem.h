/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef MALLOC_ELEM_H_
#define MALLOC_ELEM_H_

#include <stdbool.h>

#include <rte_common.h>

#define MIN_DATA_SIZE (RTE_CACHE_LINE_SIZE)

/* dummy definition of struct so we can use pointers to it in malloc_elem struct */
struct malloc_heap;

enum elem_state {
	ELEM_FREE = 0,
	ELEM_BUSY,
	ELEM_PAD  /* element is a padding-only header */
};

struct malloc_elem {
	struct malloc_heap *heap;
	struct malloc_elem *volatile prev;
	/**< points to prev elem in memseg */
	struct malloc_elem *volatile next;
	/**< points to next elem in memseg */
	LIST_ENTRY(malloc_elem) free_list;
	/**< list of free elements in heap */
	struct rte_memseg_list *msl;
	volatile enum elem_state state;
	uint32_t pad;
	size_t size;
	struct malloc_elem *orig_elem;
	size_t orig_size;
#ifdef RTE_MALLOC_DEBUG
	uint64_t header_cookie;         /* Cookie marking start of data */
	                                /* trailer cookie at start + size */
#endif
#ifdef RTE_MALLOC_ASAN
	size_t user_size;
	uint64_t asan_cookie[2]; /* must be next to header_cookie */
#endif
} __rte_cache_aligned;

static const unsigned int MALLOC_ELEM_HEADER_LEN = sizeof(struct malloc_elem);

#ifndef RTE_MALLOC_DEBUG
#ifdef RTE_MALLOC_ASAN
static const unsigned int MALLOC_ELEM_TRAILER_LEN = RTE_CACHE_LINE_SIZE;
#else
static const unsigned int MALLOC_ELEM_TRAILER_LEN;
#endif

/* dummy function - just check if pointer is non-null */
static inline int
malloc_elem_cookies_ok(const struct malloc_elem *elem){ return elem != NULL; }

/* dummy function - no header if malloc_debug is not enabled */
static inline void
set_header(struct malloc_elem *elem __rte_unused){ }

/* dummy function - no trailer if malloc_debug is not enabled */
static inline void
set_trailer(struct malloc_elem *elem __rte_unused){ }


#else
static const unsigned int MALLOC_ELEM_TRAILER_LEN = RTE_CACHE_LINE_SIZE;

#define MALLOC_HEADER_COOKIE   0xbadbadbadadd2e55ULL /**< Header cookie. */
#define MALLOC_TRAILER_COOKIE  0xadd2e55badbadbadULL /**< Trailer cookie.*/

/* define macros to make referencing the header and trailer cookies easier */
#define MALLOC_ELEM_TRAILER(elem) (*((uint64_t*)RTE_PTR_ADD(elem, \
		elem->size - MALLOC_ELEM_TRAILER_LEN)))
#define MALLOC_ELEM_HEADER(elem) (elem->header_cookie)

static inline void
set_header(struct malloc_elem *elem)
{
	if (elem != NULL)
		MALLOC_ELEM_HEADER(elem) = MALLOC_HEADER_COOKIE;
}

static inline void
set_trailer(struct malloc_elem *elem)
{
	if (elem != NULL)
		MALLOC_ELEM_TRAILER(elem) = MALLOC_TRAILER_COOKIE;
}

/* check that the header and trailer cookies are set correctly */
static inline int
malloc_elem_cookies_ok(const struct malloc_elem *elem)
{
	return elem != NULL &&
			MALLOC_ELEM_HEADER(elem) == MALLOC_HEADER_COOKIE &&
			MALLOC_ELEM_TRAILER(elem) == MALLOC_TRAILER_COOKIE;
}

#endif

#define MALLOC_ELEM_OVERHEAD (MALLOC_ELEM_HEADER_LEN + MALLOC_ELEM_TRAILER_LEN)

#ifdef RTE_MALLOC_ASAN

/*
 * ASAN_SHADOW_OFFSET should match to the corresponding
 * value defined in gcc/libsanitizer/asan/asan_mapping.h
 */
#ifdef RTE_ARCH_X86_64
#define ASAN_SHADOW_OFFSET    0x00007fff8000
#elif defined(RTE_ARCH_ARM64)
#define ASAN_SHADOW_OFFSET    0x001000000000
#elif defined(RTE_ARCH_PPC_64)
#define ASAN_SHADOW_OFFSET    0x020000000000
#endif

#define ASAN_SHADOW_GRAIN_SIZE	8
#define ASAN_MEM_FREE_FLAG	0xfd
#define ASAN_MEM_REDZONE_FLAG	0xfa
#define ASAN_SHADOW_SCALE    3

#define ASAN_MEM_SHIFT(mem) ((void *)((uintptr_t)(mem) >> ASAN_SHADOW_SCALE))
#define ASAN_MEM_TO_SHADOW(mem) \
	RTE_PTR_ADD(ASAN_MEM_SHIFT(mem), ASAN_SHADOW_OFFSET)

__rte_no_asan
static inline void
asan_set_shadow(void *addr, char val)
{
	*(char *)addr = val;
}

static inline void
asan_set_zone(void *ptr, size_t len, uint32_t val)
{
	size_t offset, i;
	void *shadow;
	size_t zone_len = len / ASAN_SHADOW_GRAIN_SIZE;
	if (len % ASAN_SHADOW_GRAIN_SIZE != 0)
		zone_len += 1;

	for (i = 0; i < zone_len; i++) {
		offset = i * ASAN_SHADOW_GRAIN_SIZE;
		shadow = ASAN_MEM_TO_SHADOW((uintptr_t)ptr + offset);
		asan_set_shadow(shadow, val);
	}
}

/*
 * When the memory is released, the release mark is
 * set in the corresponding range of the shadow area.
 */
static inline void
asan_set_freezone(void *ptr, size_t size)
{
	asan_set_zone(ptr, size, ASAN_MEM_FREE_FLAG);
}

/*
 * When the memory is allocated, memory state must set as accessible.
 */
static inline void
asan_clear_alloczone(struct malloc_elem *elem)
{
	asan_set_zone((void *)elem, elem->size, 0x0);
}

static inline void
asan_clear_split_alloczone(struct malloc_elem *elem)
{
	void *ptr = RTE_PTR_SUB(elem, MALLOC_ELEM_TRAILER_LEN);
	asan_set_zone(ptr, MALLOC_ELEM_OVERHEAD, 0x0);
}

/*
 * When the memory is allocated, the memory boundary is
 * marked in the corresponding range of the shadow area.
 * Requirement: redzone >= 16, is a power of two.
 */
static inline void
asan_set_redzone(struct malloc_elem *elem, size_t user_size)
{
	uintptr_t head_redzone;
	uintptr_t tail_redzone;
	void *front_shadow;
	void *tail_shadow;
	uint32_t val;

	if (elem != NULL) {
		if (elem->state != ELEM_PAD)
			elem = RTE_PTR_ADD(elem, elem->pad);

		elem->user_size = user_size;

		/* Set mark before the start of the allocated memory */
		head_redzone = (uintptr_t)RTE_PTR_ADD(elem,
			MALLOC_ELEM_HEADER_LEN - ASAN_SHADOW_GRAIN_SIZE);
		front_shadow = ASAN_MEM_TO_SHADOW(head_redzone);
		asan_set_shadow(front_shadow, ASAN_MEM_REDZONE_FLAG);
		front_shadow = ASAN_MEM_TO_SHADOW(head_redzone
			- ASAN_SHADOW_GRAIN_SIZE);
		asan_set_shadow(front_shadow, ASAN_MEM_REDZONE_FLAG);

		/* Set mark after the end of the allocated memory */
		tail_redzone = (uintptr_t)RTE_PTR_ADD(elem,
			MALLOC_ELEM_HEADER_LEN
			+ elem->user_size);
		tail_shadow = ASAN_MEM_TO_SHADOW(tail_redzone);
		val = (tail_redzone % ASAN_SHADOW_GRAIN_SIZE);
		val = (val == 0) ? ASAN_MEM_REDZONE_FLAG : val;
		asan_set_shadow(tail_shadow, val);
		tail_shadow = ASAN_MEM_TO_SHADOW(tail_redzone
			+ ASAN_SHADOW_GRAIN_SIZE);
		asan_set_shadow(tail_shadow, ASAN_MEM_REDZONE_FLAG);
	}
}

/*
 * When the memory is released, the mark of the memory boundary
 * in the corresponding range of the shadow area is cleared.
 * Requirement: redzone >= 16, is a power of two.
 */
static inline void
asan_clear_redzone(struct malloc_elem *elem)
{
	uintptr_t head_redzone;
	uintptr_t tail_redzone;
	void *head_shadow;
	void *tail_shadow;

	if (elem != NULL) {
		elem = RTE_PTR_ADD(elem, elem->pad);

		/* Clear mark before the start of the allocated memory */
		head_redzone = (uintptr_t)RTE_PTR_ADD(elem,
			MALLOC_ELEM_HEADER_LEN - ASAN_SHADOW_GRAIN_SIZE);
		head_shadow = ASAN_MEM_TO_SHADOW(head_redzone);
		asan_set_shadow(head_shadow, 0x00);
		head_shadow = ASAN_MEM_TO_SHADOW(head_redzone
				- ASAN_SHADOW_GRAIN_SIZE);
		asan_set_shadow(head_shadow, 0x00);

		/* Clear mark after the end of the allocated memory */
		tail_redzone = (uintptr_t)RTE_PTR_ADD(elem,
			MALLOC_ELEM_HEADER_LEN + elem->user_size);
		tail_shadow = ASAN_MEM_TO_SHADOW(tail_redzone);
		asan_set_shadow(tail_shadow, 0x00);
		tail_shadow = ASAN_MEM_TO_SHADOW(tail_redzone
				+ ASAN_SHADOW_GRAIN_SIZE);
		asan_set_shadow(tail_shadow, 0x00);
	}
}

static inline size_t
old_malloc_size(struct malloc_elem *elem)
{
	if (elem->state != ELEM_PAD)
		elem = RTE_PTR_ADD(elem, elem->pad);

	return elem->user_size;
}

#else /* !RTE_MALLOC_ASAN */

static inline void
asan_set_zone(void *ptr __rte_unused, size_t len __rte_unused,
		uint32_t val __rte_unused) { }

static inline void
asan_set_freezone(void *ptr __rte_unused, size_t size __rte_unused) { }

static inline void
asan_clear_alloczone(struct malloc_elem *elem __rte_unused) { }

static inline void
asan_clear_split_alloczone(struct malloc_elem *elem __rte_unused) { }

static inline void
asan_set_redzone(struct malloc_elem *elem __rte_unused,
					size_t user_size __rte_unused) { }

static inline void
asan_clear_redzone(struct malloc_elem *elem __rte_unused) { }

static inline size_t
old_malloc_size(struct malloc_elem *elem)
{
	return elem->size - elem->pad - MALLOC_ELEM_OVERHEAD;
}
#endif /* !RTE_MALLOC_ASAN */

/*
 * Given a pointer to the start of a memory block returned by malloc, get
 * the actual malloc_elem header for that block.
 */
static inline struct malloc_elem *
malloc_elem_from_data(const void *data)
{
	if (data == NULL)
		return NULL;

	struct malloc_elem *elem = RTE_PTR_SUB(data, MALLOC_ELEM_HEADER_LEN);
	if (!malloc_elem_cookies_ok(elem))
		return NULL;
	return elem->state != ELEM_PAD ? elem:  RTE_PTR_SUB(elem, elem->pad);
}

/*
 * initialise a malloc_elem header
 */
void
malloc_elem_init(struct malloc_elem *elem,
		struct malloc_heap *heap,
		struct rte_memseg_list *msl,
		size_t size,
		struct malloc_elem *orig_elem,
		size_t orig_size);

void
malloc_elem_insert(struct malloc_elem *elem);

/*
 * return true if the current malloc_elem can hold a block of data
 * of the requested size and with the requested alignment
 */
int
malloc_elem_can_hold(struct malloc_elem *elem, size_t size,
		unsigned int align, size_t bound, bool contig);

/*
 * reserve a block of data in an existing malloc_elem. If the malloc_elem
 * is much larger than the data block requested, we split the element in two.
 */
struct malloc_elem *
malloc_elem_alloc(struct malloc_elem *elem, size_t size,
		unsigned int align, size_t bound, bool contig);

/*
 * free a malloc_elem block by adding it to the free list. If the
 * blocks either immediately before or immediately after newly freed block
 * are also free, the blocks are merged together.
 */
struct malloc_elem *
malloc_elem_free(struct malloc_elem *elem);

struct malloc_elem *
malloc_elem_join_adjacent_free(struct malloc_elem *elem);

/*
 * attempt to resize a malloc_elem by expanding into any free space
 * immediately after it in memory.
 */
int
malloc_elem_resize(struct malloc_elem *elem, size_t size);

void
malloc_elem_hide_region(struct malloc_elem *elem, void *start, size_t len);

void
malloc_elem_free_list_remove(struct malloc_elem *elem);

/*
 * dump contents of malloc elem to a file.
 */
void
malloc_elem_dump(const struct malloc_elem *elem, FILE *f);

/*
 * Given an element size, compute its freelist index.
 */
size_t
malloc_elem_free_list_index(size_t size);

/*
 * Add element to its heap's free list.
 */
void
malloc_elem_free_list_insert(struct malloc_elem *elem);

/*
 * Find biggest IOVA-contiguous zone within an element with specified alignment.
 */
size_t
malloc_elem_find_max_iova_contig(struct malloc_elem *elem, size_t align);

#endif /* MALLOC_ELEM_H_ */
