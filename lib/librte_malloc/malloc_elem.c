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

#define MIN_DATA_SIZE (CACHE_LINE_SIZE * 2)

/*
 * initialise a general malloc_elem header structure
 */
void
malloc_elem_init(struct malloc_elem *elem,
		struct malloc_heap *heap, size_t size)
{
	elem->heap = heap;
	elem->prev = elem->next_free = NULL;
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
	malloc_elem_init(elem, prev->heap, 0);
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

	malloc_elem_init(split_pt, elem->heap, new_elem_size);
	split_pt->prev = elem;
	next_elem->prev = split_pt;
	elem->size = old_elem_size;
	set_trailer(elem);
}

/*
 * reserve a block of data in an existing malloc_elem. If the malloc_elem
 * is much larger than the data block requested, we split the element in two.
 * This function is only called from malloc_heap_alloc so parameter checking
 * is not done here, as it's done there previously.
 */
struct malloc_elem *
malloc_elem_alloc(struct malloc_elem *elem, size_t size,
		unsigned align, struct malloc_elem *prev_free)
{
	struct malloc_elem *new_elem = elem_start_pt(elem, size, align);
	const unsigned old_elem_size = (uintptr_t)new_elem - (uintptr_t)elem;

	if (old_elem_size <= MALLOC_ELEM_OVERHEAD + MIN_DATA_SIZE){
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
		if (prev_free == NULL)
			elem->heap->free_head = elem->next_free;
		else
			prev_free->next_free = elem->next_free;

		return new_elem;
	}

	/* we are going to split the element in two. The original element
	 * remains free, and the new element is the one allocated, so no free list
	 * changes need to be made.
	 */
	split_elem(elem, new_elem);
	new_elem->state = ELEM_BUSY;

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
 * scan the free list, and remove the request element from that
 * free list. (Free list to scan is got from heap pointer in element)
 */
static inline void
remove_from_free_list(struct malloc_elem *elem)
{
	if (elem == elem->heap->free_head)
		elem->heap->free_head = elem->next_free;
	else{
		struct malloc_elem *prev_free = elem->heap->free_head;
		while (prev_free && prev_free->next_free != elem)
			prev_free = prev_free->next_free;
		if (!prev_free)
			rte_panic("Corrupted free list\n");
		prev_free->next_free = elem->next_free;
	}
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
		/* join to this one, and remove from free list */
		join_elem(elem, next);
		remove_from_free_list(next);
	}

	/* check if previous element is free, if so join with it and return,
	 * no need to update free list, as that element is already there
	 */
	if (elem->prev != NULL && elem->prev->state == ELEM_FREE)
		join_elem(elem->prev, elem);
	/* otherwise add ourselves to the free list */
	else {
		elem->next_free = elem->heap->free_head;
		elem->heap->free_head = elem;
		elem->state = ELEM_FREE;
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

	/* we now know the element fits, so join the two, then remove from free
	 * list
	 */
	join_elem(elem, next);
	remove_from_free_list(next);

	if (elem->size - new_size > MIN_DATA_SIZE + MALLOC_ELEM_OVERHEAD){
		/* now we have a big block together. Lets cut it down a bit, by splitting */
		struct malloc_elem *split_pt = RTE_PTR_ADD(elem, new_size);
		split_pt = RTE_PTR_ALIGN_CEIL(split_pt, CACHE_LINE_SIZE);
		split_elem(elem, split_pt);
		split_pt->state = ELEM_FREE;
		split_pt->next_free = elem->heap->free_head;
		elem->heap->free_head = split_pt;
	}
	rte_spinlock_unlock(&elem->heap->lock);
	return 0;

err_return:
	rte_spinlock_unlock(&elem->heap->lock);
	return -1;
}
