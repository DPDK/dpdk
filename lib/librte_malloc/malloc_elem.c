/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_common.h>
#include <rte_spinlock.h>

#include "malloc_elem.h"
#include "malloc_heap.h"

#define MIN_DATA_SIZE (CACHE_LINE_SIZE)

/*
 * initialise a general malloc_elem header structure
 */
void
malloc_elem_init(struct malloc_elem *elem,
		struct malloc_heap *heap, const struct rte_memzone *mz, size_t size)
{
	elem->heap = heap;
	elem->mz = mz;
	elem->prev = NULL;
	memset(&elem->free_list, 0, sizeof(elem->free_list));
	elem->state = ELEM_FREE;
	elem->size = size;
	elem->pad = 0;
	set_header(elem);
	set_trailer(elem);
}

/*
 * initialise a dummy malloc_elem header for the end-of-memzone marker
 */
void
malloc_elem_mkend(struct malloc_elem *elem, struct malloc_elem *prev)
{
	malloc_elem_init(elem, prev->heap, prev->mz, 0);
	elem->prev = prev;
	elem->state = ELEM_BUSY; /* mark busy so its never merged */
}

/*
 * calculate the starting point of where data of the requested size
 * and alignment would fit in the current element. If the data doesn't
 * fit, return NULL.
 */
static void *
elem_start_pt(struct malloc_elem *elem, size_t size, unsigned align)
{
	const uintptr_t end_pt = (uintptr_t)elem +
			elem->size - MALLOC_ELEM_TRAILER_LEN;
	const uintptr_t new_data_start = rte_align_floor_int((end_pt - size),align);
	const uintptr_t new_elem_start = new_data_start - MALLOC_ELEM_HEADER_LEN;

	/* if the new start point is before the exist start, it won't fit */
	return (new_elem_start < (uintptr_t)elem) ? NULL : (void *)new_elem_start;
}

/*
 * use elem_start_pt to determine if we get meet the size and
 * alignment request from the current element
 */
int
malloc_elem_can_hold(struct malloc_elem *elem, size_t size, unsigned align)
{
	return elem_start_pt(elem, size, align) != NULL;
}

/*
 * split an existing element into two smaller elements at the given
 * split_pt parameter.
 */
static void
split_elem(struct malloc_elem *elem, struct malloc_elem *split_pt)
{
	struct malloc_elem *next_elem = RTE_PTR_ADD(elem, elem->size);
	const unsigned old_elem_size = (uintptr_t)split_pt - (uintptr_t)elem;
	const unsigned new_elem_size = elem->size - old_elem_size;

	malloc_elem_init(split_pt, elem->heap, elem->mz, new_elem_size);
	split_pt->prev = elem;
	next_elem->prev = split_pt;
	elem->size = old_elem_size;
	set_trailer(elem);
}

/*
 * Given an element size, compute its freelist index.
 * We free an element into the freelist containing similarly-sized elements.
 * We try to allocate elements starting with the freelist containing
 * similarly-sized elements, and if necessary, we search freelists
 * containing larger elements.
 *
 * Example element size ranges for a heap with five free lists:
 *   heap->free_head[0] - (0   , 2^8]
 *   heap->free_head[1] - (2^8 , 2^10]
 *   heap->free_head[2] - (2^10 ,2^12]
 *   heap->free_head[3] - (2^12, 2^14]
 *   heap->free_head[4] - (2^14, MAX_SIZE]
 */
size_t
malloc_elem_free_list_index(size_t size)
{
#define MALLOC_MINSIZE_LOG2   8
#define MALLOC_LOG2_INCREMENT 2

	size_t log2;
	size_t index;

	if (size <= (1UL << MALLOC_MINSIZE_LOG2))
		return 0;

	/* Find next power of 2 >= size. */
	log2 = sizeof(size) * 8 - __builtin_clzl(size-1);

	/* Compute freelist index, based on log2(size). */
	index = (log2 - MALLOC_MINSIZE_LOG2 + MALLOC_LOG2_INCREMENT - 1) /
	        MALLOC_LOG2_INCREMENT;

	return (index <= RTE_HEAP_NUM_FREELISTS-1?
	        index: RTE_HEAP_NUM_FREELISTS-1);
}

/*
 * Add the specified element to its heap's free list.
 */
void
malloc_elem_free_list_insert(struct malloc_elem *elem)
{
	size_t idx = malloc_elem_free_list_index(elem->size - MALLOC_ELEM_HEADER_LEN);

	elem->state = ELEM_FREE;
	LIST_INSERT_HEAD(&elem->heap->free_head[idx], elem, free_list);
}

/*
 * Remove the specified element from its heap's free list.
 */
static void
elem_free_list_remove(struct malloc_elem *elem)
{
	LIST_REMOVE(elem, free_list);
}

/*
 * reserve a block of data in an existing malloc_elem. If the malloc_elem
 * is much larger than the data block requested, we split the element in two.
 * This function is only called from malloc_heap_alloc so parameter checking
 * is not done here, as it's done there previously.
 */
struct malloc_elem *
malloc_elem_alloc(struct malloc_elem *elem, size_t size, unsigned align)
{
	struct malloc_elem *new_elem = elem_start_pt(elem, size, align);
	const unsigned old_elem_size = (uintptr_t)new_elem - (uintptr_t)elem;

	if (old_elem_size < MALLOC_ELEM_OVERHEAD + MIN_DATA_SIZE){
		/* don't split it, pad the element instead */
		elem->state = ELEM_BUSY;
		elem->pad = old_elem_size;

		/* put a dummy header in padding, to point to real element header */
		if (elem->pad > 0){ /* pad will be at least 64-bytes, as everything
		                     * is cache-line aligned */
			new_elem->pad = elem->pad;
			new_elem->state = ELEM_PAD;
			new_elem->size = elem->size - elem->pad;
			set_header(new_elem);
		}
		/* remove element from free list */
		elem_free_list_remove(elem);

		return new_elem;
	}

	/* we are going to split the element in two. The original element
	 * remains free, and the new element is the one allocated.
	 * Re-insert original element, in case its new size makes it
	 * belong on a different list.
	 */
	elem_free_list_remove(elem);
	split_elem(elem, new_elem);
	new_elem->state = ELEM_BUSY;
	malloc_elem_free_list_insert(elem);

	return new_elem;
}

/*
 * joing two struct malloc_elem together. elem1 and elem2 must
 * be contiguous in memory.
 */
static inline void
join_elem(struct malloc_elem *elem1, struct malloc_elem *elem2)
{
	struct malloc_elem *next = RTE_PTR_ADD(elem2, elem2->size);
	elem1->size += elem2->size;
	next->prev = elem1;
}

/*
 * free a malloc_elem block by adding it to the free list. If the
 * blocks either immediately before or immediately after newly freed block
 * are also free, the blocks are merged together.
 */
int
malloc_elem_free(struct malloc_elem *elem)
{
	if (!malloc_elem_cookies_ok(elem) || elem->state != ELEM_BUSY)
		return -1;

	rte_spinlock_lock(&(elem->heap->lock));
	struct malloc_elem *next = RTE_PTR_ADD(elem, elem->size);
	if (next->state == ELEM_FREE){
		/* remove from free list, join to this one */
		elem_free_list_remove(next);
		join_elem(elem, next);
	}

	/* check if previous element is free, if so join with it and return,
	 * need to re-insert in free list, as that element's size is changing
	 */
	if (elem->prev != NULL && elem->prev->state == ELEM_FREE) {
		elem_free_list_remove(elem->prev);
		join_elem(elem->prev, elem);
		malloc_elem_free_list_insert(elem->prev);
	}
	/* otherwise add ourselves to the free list */
	else {
		malloc_elem_free_list_insert(elem);
		elem->pad = 0;
	}
	/* decrease heap's count of allocated elements */
	elem->heap->alloc_count--;
	rte_spinlock_unlock(&(elem->heap->lock));

	return 0;
}

/*
 * attempt to resize a malloc_elem by expanding into any free space
 * immediately after it in memory.
 */
int
malloc_elem_resize(struct malloc_elem *elem, size_t size)
{
	const size_t new_size = size + MALLOC_ELEM_OVERHEAD;
	/* if we request a smaller size, then always return ok */
	const size_t current_size = elem->size - elem->pad;
	if (current_size >= new_size)
		return 0;

	struct malloc_elem *next = RTE_PTR_ADD(elem, elem->size);
	rte_spinlock_lock(&elem->heap->lock);
	if (next ->state != ELEM_FREE)
		goto err_return;
	if (current_size + next->size < new_size)
		goto err_return;

	/* we now know the element fits, so remove from free list,
	 * join the two
	 */
	elem_free_list_remove(next);
	join_elem(elem, next);

	if (elem->size - new_size >= MIN_DATA_SIZE + MALLOC_ELEM_OVERHEAD){
		/* now we have a big block together. Lets cut it down a bit, by splitting */
		struct malloc_elem *split_pt = RTE_PTR_ADD(elem, new_size);
		split_pt = RTE_PTR_ALIGN_CEIL(split_pt, CACHE_LINE_SIZE);
		split_elem(elem, split_pt);
		malloc_elem_free_list_insert(split_pt);
	}
	rte_spinlock_unlock(&elem->heap->lock);
	return 0;

err_return:
	rte_spinlock_unlock(&elem->heap->lock);
	return -1;
}
