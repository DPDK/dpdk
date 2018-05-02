/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>
#include <rte_memcpy.h>
#include <rte_atomic.h>
#include <rte_fbarray.h>

#include "eal_internal_cfg.h"
#include "eal_memalloc.h"
#include "malloc_elem.h"
#include "malloc_heap.h"
#include "malloc_mp.h"

static unsigned
check_hugepage_sz(unsigned flags, uint64_t hugepage_sz)
{
	unsigned check_flag = 0;

	if (!(flags & ~RTE_MEMZONE_SIZE_HINT_ONLY))
		return 1;

	switch (hugepage_sz) {
	case RTE_PGSIZE_256K:
		check_flag = RTE_MEMZONE_256KB;
		break;
	case RTE_PGSIZE_2M:
		check_flag = RTE_MEMZONE_2MB;
		break;
	case RTE_PGSIZE_16M:
		check_flag = RTE_MEMZONE_16MB;
		break;
	case RTE_PGSIZE_256M:
		check_flag = RTE_MEMZONE_256MB;
		break;
	case RTE_PGSIZE_512M:
		check_flag = RTE_MEMZONE_512MB;
		break;
	case RTE_PGSIZE_1G:
		check_flag = RTE_MEMZONE_1GB;
		break;
	case RTE_PGSIZE_4G:
		check_flag = RTE_MEMZONE_4GB;
		break;
	case RTE_PGSIZE_16G:
		check_flag = RTE_MEMZONE_16GB;
	}

	return check_flag & flags;
}

/*
 * Expand the heap with a memory area.
 */
static struct malloc_elem *
malloc_heap_add_memory(struct malloc_heap *heap, struct rte_memseg_list *msl,
		void *start, size_t len)
{
	struct malloc_elem *elem = start;

	malloc_elem_init(elem, heap, msl, len);

	malloc_elem_insert(elem);

	elem = malloc_elem_join_adjacent_free(elem);

	malloc_elem_free_list_insert(elem);

	return elem;
}

static int
malloc_add_seg(const struct rte_memseg_list *msl,
		const struct rte_memseg *ms, size_t len, void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *found_msl;
	struct malloc_heap *heap;
	int msl_idx;

	heap = &mcfg->malloc_heaps[msl->socket_id];

	/* msl is const, so find it */
	msl_idx = msl - mcfg->memsegs;

	if (msl_idx < 0 || msl_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	found_msl = &mcfg->memsegs[msl_idx];

	malloc_heap_add_memory(heap, found_msl, ms->addr, len);

	heap->total_size += len;

	RTE_LOG(DEBUG, EAL, "Added %zuM to heap on socket %i\n", len >> 20,
			msl->socket_id);
	return 0;
}

/*
 * Iterates through the freelist for a heap to find a free element
 * which can store data of the required size and with the requested alignment.
 * If size is 0, find the biggest available elem.
 * Returns null on failure, or pointer to element on success.
 */
static struct malloc_elem *
find_suitable_element(struct malloc_heap *heap, size_t size,
		unsigned int flags, size_t align, size_t bound, bool contig)
{
	size_t idx;
	struct malloc_elem *elem, *alt_elem = NULL;

	for (idx = malloc_elem_free_list_index(size);
			idx < RTE_HEAP_NUM_FREELISTS; idx++) {
		for (elem = LIST_FIRST(&heap->free_head[idx]);
				!!elem; elem = LIST_NEXT(elem, free_list)) {
			if (malloc_elem_can_hold(elem, size, align, bound,
					contig)) {
				if (check_hugepage_sz(flags,
						elem->msl->page_sz))
					return elem;
				if (alt_elem == NULL)
					alt_elem = elem;
			}
		}
	}

	if ((alt_elem != NULL) && (flags & RTE_MEMZONE_SIZE_HINT_ONLY))
		return alt_elem;

	return NULL;
}

/*
 * Main function to allocate a block of memory from the heap.
 * It locks the free list, scans it, and adds a new memseg if the
 * scan fails. Once the new memseg is added, it re-scans and should return
 * the new element after releasing the lock.
 */
static void *
heap_alloc(struct malloc_heap *heap, const char *type __rte_unused, size_t size,
		unsigned int flags, size_t align, size_t bound, bool contig)
{
	struct malloc_elem *elem;

	size = RTE_CACHE_LINE_ROUNDUP(size);
	align = RTE_CACHE_LINE_ROUNDUP(align);

	elem = find_suitable_element(heap, size, flags, align, bound, contig);
	if (elem != NULL) {
		elem = malloc_elem_alloc(elem, size, align, bound, contig);

		/* increase heap's count of allocated elements */
		heap->alloc_count++;
	}

	return elem == NULL ? NULL : (void *)(&elem[1]);
}

/* this function is exposed in malloc_mp.h */
void
rollback_expand_heap(struct rte_memseg **ms, int n_segs,
		struct malloc_elem *elem, void *map_addr, size_t map_len)
{
	if (elem != NULL) {
		malloc_elem_free_list_remove(elem);
		malloc_elem_hide_region(elem, map_addr, map_len);
	}

	eal_memalloc_free_seg_bulk(ms, n_segs);
}

/* this function is exposed in malloc_mp.h */
struct malloc_elem *
alloc_pages_on_heap(struct malloc_heap *heap, uint64_t pg_sz, size_t elt_size,
		int socket, unsigned int flags, size_t align, size_t bound,
		bool contig, struct rte_memseg **ms, int n_segs)
{
	struct rte_memseg_list *msl;
	struct malloc_elem *elem = NULL;
	size_t alloc_sz;
	int allocd_pages;
	void *ret, *map_addr;

	alloc_sz = (size_t)pg_sz * n_segs;

	/* first, check if we're allowed to allocate this memory */
	if (eal_memalloc_mem_alloc_validate(socket,
			heap->total_size + alloc_sz) < 0) {
		RTE_LOG(DEBUG, EAL, "User has disallowed allocation\n");
		return NULL;
	}

	allocd_pages = eal_memalloc_alloc_seg_bulk(ms, n_segs, pg_sz,
			socket, true);

	/* make sure we've allocated our pages... */
	if (allocd_pages < 0)
		return NULL;

	map_addr = ms[0]->addr;
	msl = rte_mem_virt2memseg_list(map_addr);

	/* check if we wanted contiguous memory but didn't get it */
	if (contig && !eal_memalloc_is_contig(msl, map_addr, alloc_sz)) {
		RTE_LOG(DEBUG, EAL, "%s(): couldn't allocate physically contiguous space\n",
				__func__);
		goto fail;
	}

	/* add newly minted memsegs to malloc heap */
	elem = malloc_heap_add_memory(heap, msl, map_addr, alloc_sz);

	/* try once more, as now we have allocated new memory */
	ret = find_suitable_element(heap, elt_size, flags, align, bound,
			contig);

	if (ret == NULL)
		goto fail;

	return elem;

fail:
	rollback_expand_heap(ms, n_segs, elem, map_addr, alloc_sz);
	return NULL;
}

static int
try_expand_heap_primary(struct malloc_heap *heap, uint64_t pg_sz,
		size_t elt_size, int socket, unsigned int flags, size_t align,
		size_t bound, bool contig)
{
	struct malloc_elem *elem;
	struct rte_memseg **ms;
	void *map_addr;
	size_t alloc_sz;
	int n_segs;
	bool callback_triggered = false;

	alloc_sz = RTE_ALIGN_CEIL(align + elt_size +
			MALLOC_ELEM_TRAILER_LEN, pg_sz);
	n_segs = alloc_sz / pg_sz;

	/* we can't know in advance how many pages we'll need, so we malloc */
	ms = malloc(sizeof(*ms) * n_segs);

	memset(ms, 0, sizeof(*ms) * n_segs);

	if (ms == NULL)
		return -1;

	elem = alloc_pages_on_heap(heap, pg_sz, elt_size, socket, flags, align,
			bound, contig, ms, n_segs);

	if (elem == NULL)
		goto free_ms;

	map_addr = ms[0]->addr;

	/* notify user about changes in memory map */
	eal_memalloc_mem_event_notify(RTE_MEM_EVENT_ALLOC, map_addr, alloc_sz);

	/* notify other processes that this has happened */
	if (request_sync()) {
		/* we couldn't ensure all processes have mapped memory,
		 * so free it back and notify everyone that it's been
		 * freed back.
		 *
		 * technically, we could've avoided adding memory addresses to
		 * the map, but that would've led to inconsistent behavior
		 * between primary and secondary processes, as those get
		 * callbacks during sync. therefore, force primary process to
		 * do alloc-and-rollback syncs as well.
		 */
		callback_triggered = true;
		goto free_elem;
	}
	heap->total_size += alloc_sz;

	RTE_LOG(DEBUG, EAL, "Heap on socket %d was expanded by %zdMB\n",
		socket, alloc_sz >> 20ULL);

	free(ms);

	return 0;

free_elem:
	if (callback_triggered)
		eal_memalloc_mem_event_notify(RTE_MEM_EVENT_FREE,
				map_addr, alloc_sz);

	rollback_expand_heap(ms, n_segs, elem, map_addr, alloc_sz);

	request_sync();
free_ms:
	free(ms);

	return -1;
}

static int
try_expand_heap_secondary(struct malloc_heap *heap, uint64_t pg_sz,
		size_t elt_size, int socket, unsigned int flags, size_t align,
		size_t bound, bool contig)
{
	struct malloc_mp_req req;
	int req_result;

	memset(&req, 0, sizeof(req));

	req.t = REQ_TYPE_ALLOC;
	req.alloc_req.align = align;
	req.alloc_req.bound = bound;
	req.alloc_req.contig = contig;
	req.alloc_req.flags = flags;
	req.alloc_req.elt_size = elt_size;
	req.alloc_req.page_sz = pg_sz;
	req.alloc_req.socket = socket;
	req.alloc_req.heap = heap; /* it's in shared memory */

	req_result = request_to_primary(&req);

	if (req_result != 0)
		return -1;

	if (req.result != REQ_RESULT_SUCCESS)
		return -1;

	return 0;
}

static int
try_expand_heap(struct malloc_heap *heap, uint64_t pg_sz, size_t elt_size,
		int socket, unsigned int flags, size_t align, size_t bound,
		bool contig)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int ret;

	rte_rwlock_write_lock(&mcfg->memory_hotplug_lock);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = try_expand_heap_primary(heap, pg_sz, elt_size, socket,
				flags, align, bound, contig);
	} else {
		ret = try_expand_heap_secondary(heap, pg_sz, elt_size, socket,
				flags, align, bound, contig);
	}

	rte_rwlock_write_unlock(&mcfg->memory_hotplug_lock);
	return ret;
}

static int
compare_pagesz(const void *a, const void *b)
{
	const struct rte_memseg_list * const*mpa = a;
	const struct rte_memseg_list * const*mpb = b;
	const struct rte_memseg_list *msla = *mpa;
	const struct rte_memseg_list *mslb = *mpb;
	uint64_t pg_sz_a = msla->page_sz;
	uint64_t pg_sz_b = mslb->page_sz;

	if (pg_sz_a < pg_sz_b)
		return -1;
	if (pg_sz_a > pg_sz_b)
		return 1;
	return 0;
}

static int
alloc_more_mem_on_socket(struct malloc_heap *heap, size_t size, int socket,
		unsigned int flags, size_t align, size_t bound, bool contig)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *requested_msls[RTE_MAX_MEMSEG_LISTS];
	struct rte_memseg_list *other_msls[RTE_MAX_MEMSEG_LISTS];
	uint64_t requested_pg_sz[RTE_MAX_MEMSEG_LISTS];
	uint64_t other_pg_sz[RTE_MAX_MEMSEG_LISTS];
	uint64_t prev_pg_sz;
	int i, n_other_msls, n_other_pg_sz, n_requested_msls, n_requested_pg_sz;
	bool size_hint = (flags & RTE_MEMZONE_SIZE_HINT_ONLY) > 0;
	unsigned int size_flags = flags & ~RTE_MEMZONE_SIZE_HINT_ONLY;
	void *ret;

	memset(requested_msls, 0, sizeof(requested_msls));
	memset(other_msls, 0, sizeof(other_msls));
	memset(requested_pg_sz, 0, sizeof(requested_pg_sz));
	memset(other_pg_sz, 0, sizeof(other_pg_sz));

	/*
	 * go through memseg list and take note of all the page sizes available,
	 * and if any of them were specifically requested by the user.
	 */
	n_requested_msls = 0;
	n_other_msls = 0;
	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];

		if (msl->socket_id != socket)
			continue;

		if (msl->base_va == NULL)
			continue;

		/* if pages of specific size were requested */
		if (size_flags != 0 && check_hugepage_sz(size_flags,
				msl->page_sz))
			requested_msls[n_requested_msls++] = msl;
		else if (size_flags == 0 || size_hint)
			other_msls[n_other_msls++] = msl;
	}

	/* sort the lists, smallest first */
	qsort(requested_msls, n_requested_msls, sizeof(requested_msls[0]),
			compare_pagesz);
	qsort(other_msls, n_other_msls, sizeof(other_msls[0]),
			compare_pagesz);

	/* now, extract page sizes we are supposed to try */
	prev_pg_sz = 0;
	n_requested_pg_sz = 0;
	for (i = 0; i < n_requested_msls; i++) {
		uint64_t pg_sz = requested_msls[i]->page_sz;

		if (prev_pg_sz != pg_sz) {
			requested_pg_sz[n_requested_pg_sz++] = pg_sz;
			prev_pg_sz = pg_sz;
		}
	}
	prev_pg_sz = 0;
	n_other_pg_sz = 0;
	for (i = 0; i < n_other_msls; i++) {
		uint64_t pg_sz = other_msls[i]->page_sz;

		if (prev_pg_sz != pg_sz) {
			other_pg_sz[n_other_pg_sz++] = pg_sz;
			prev_pg_sz = pg_sz;
		}
	}

	/* finally, try allocating memory of specified page sizes, starting from
	 * the smallest sizes
	 */
	for (i = 0; i < n_requested_pg_sz; i++) {
		uint64_t pg_sz = requested_pg_sz[i];

		/*
		 * do not pass the size hint here, as user expects other page
		 * sizes first, before resorting to best effort allocation.
		 */
		if (!try_expand_heap(heap, pg_sz, size, socket, size_flags,
				align, bound, contig))
			return 0;
	}
	if (n_other_pg_sz == 0)
		return -1;

	/* now, check if we can reserve anything with size hint */
	ret = find_suitable_element(heap, size, flags, align, bound, contig);
	if (ret != NULL)
		return 0;

	/*
	 * we still couldn't reserve memory, so try expanding heap with other
	 * page sizes, if there are any
	 */
	for (i = 0; i < n_other_pg_sz; i++) {
		uint64_t pg_sz = other_pg_sz[i];

		if (!try_expand_heap(heap, pg_sz, size, socket, flags,
				align, bound, contig))
			return 0;
	}
	return -1;
}

/* this will try lower page sizes first */
static void *
heap_alloc_on_socket(const char *type, size_t size, int socket,
		unsigned int flags, size_t align, size_t bound, bool contig)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct malloc_heap *heap = &mcfg->malloc_heaps[socket];
	unsigned int size_flags = flags & ~RTE_MEMZONE_SIZE_HINT_ONLY;
	void *ret;

	rte_spinlock_lock(&(heap->lock));

	align = align == 0 ? 1 : align;

	/* for legacy mode, try once and with all flags */
	if (internal_config.legacy_mem) {
		ret = heap_alloc(heap, type, size, flags, align, bound, contig);
		goto alloc_unlock;
	}

	/*
	 * we do not pass the size hint here, because even if allocation fails,
	 * we may still be able to allocate memory from appropriate page sizes,
	 * we just need to request more memory first.
	 */
	ret = heap_alloc(heap, type, size, size_flags, align, bound, contig);
	if (ret != NULL)
		goto alloc_unlock;

	if (!alloc_more_mem_on_socket(heap, size, socket, flags, align, bound,
			contig)) {
		ret = heap_alloc(heap, type, size, flags, align, bound, contig);

		/* this should have succeeded */
		if (ret == NULL)
			RTE_LOG(ERR, EAL, "Error allocating from heap\n");
	}
alloc_unlock:
	rte_spinlock_unlock(&(heap->lock));
	return ret;
}

void *
malloc_heap_alloc(const char *type, size_t size, int socket_arg,
		unsigned int flags, size_t align, size_t bound, bool contig)
{
	int socket, i, cur_socket;
	void *ret;

	/* return NULL if size is 0 or alignment is not power-of-2 */
	if (size == 0 || (align && !rte_is_power_of_2(align)))
		return NULL;

	if (!rte_eal_has_hugepages())
		socket_arg = SOCKET_ID_ANY;

	if (socket_arg == SOCKET_ID_ANY)
		socket = malloc_get_numa_socket();
	else
		socket = socket_arg;

	/* Check socket parameter */
	if (socket >= RTE_MAX_NUMA_NODES)
		return NULL;

	ret = heap_alloc_on_socket(type, size, socket, flags, align, bound,
			contig);
	if (ret != NULL || socket_arg != SOCKET_ID_ANY)
		return ret;

	/* try other heaps */
	for (i = 0; i < (int) rte_socket_count(); i++) {
		cur_socket = rte_socket_id_by_idx(i);
		if (cur_socket == socket)
			continue;
		ret = heap_alloc_on_socket(type, size, cur_socket, flags,
				align, bound, contig);
		if (ret != NULL)
			return ret;
	}
	return NULL;
}

/* this function is exposed in malloc_mp.h */
int
malloc_heap_free_pages(void *aligned_start, size_t aligned_len)
{
	int n_segs, seg_idx, max_seg_idx;
	struct rte_memseg_list *msl;
	size_t page_sz;

	msl = rte_mem_virt2memseg_list(aligned_start);
	if (msl == NULL)
		return -1;

	page_sz = (size_t)msl->page_sz;
	n_segs = aligned_len / page_sz;
	seg_idx = RTE_PTR_DIFF(aligned_start, msl->base_va) / page_sz;
	max_seg_idx = seg_idx + n_segs;

	for (; seg_idx < max_seg_idx; seg_idx++) {
		struct rte_memseg *ms;

		ms = rte_fbarray_get(&msl->memseg_arr, seg_idx);
		eal_memalloc_free_seg(ms);
	}
	return 0;
}

int
malloc_heap_free(struct malloc_elem *elem)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct malloc_heap *heap;
	void *start, *aligned_start, *end, *aligned_end;
	size_t len, aligned_len, page_sz;
	struct rte_memseg_list *msl;
	unsigned int i, n_segs, before_space, after_space;
	int ret;

	if (!malloc_elem_cookies_ok(elem) || elem->state != ELEM_BUSY)
		return -1;

	/* elem may be merged with previous element, so keep heap address */
	heap = elem->heap;
	msl = elem->msl;
	page_sz = (size_t)msl->page_sz;

	rte_spinlock_lock(&(heap->lock));

	/* mark element as free */
	elem->state = ELEM_FREE;

	elem = malloc_elem_free(elem);

	/* anything after this is a bonus */
	ret = 0;

	/* ...of which we can't avail if we are in legacy mode */
	if (internal_config.legacy_mem)
		goto free_unlock;

	/* check if we can free any memory back to the system */
	if (elem->size < page_sz)
		goto free_unlock;

	/* probably, but let's make sure, as we may not be using up full page */
	start = elem;
	len = elem->size;
	aligned_start = RTE_PTR_ALIGN_CEIL(start, page_sz);
	end = RTE_PTR_ADD(elem, len);
	aligned_end = RTE_PTR_ALIGN_FLOOR(end, page_sz);

	aligned_len = RTE_PTR_DIFF(aligned_end, aligned_start);

	/* can't free anything */
	if (aligned_len < page_sz)
		goto free_unlock;

	/* we can free something. however, some of these pages may be marked as
	 * unfreeable, so also check that as well
	 */
	n_segs = aligned_len / page_sz;
	for (i = 0; i < n_segs; i++) {
		const struct rte_memseg *tmp =
				rte_mem_virt2memseg(aligned_start, msl);

		if (tmp->flags & RTE_MEMSEG_FLAG_DO_NOT_FREE) {
			/* this is an unfreeable segment, so move start */
			aligned_start = RTE_PTR_ADD(tmp->addr, tmp->len);
		}
	}

	/* recalculate length and number of segments */
	aligned_len = RTE_PTR_DIFF(aligned_end, aligned_start);
	n_segs = aligned_len / page_sz;

	/* check if we can still free some pages */
	if (n_segs == 0)
		goto free_unlock;

	/* We're not done yet. We also have to check if by freeing space we will
	 * be leaving free elements that are too small to store new elements.
	 * Check if we have enough space in the beginning and at the end, or if
	 * start/end are exactly page aligned.
	 */
	before_space = RTE_PTR_DIFF(aligned_start, elem);
	after_space = RTE_PTR_DIFF(end, aligned_end);
	if (before_space != 0 &&
			before_space < MALLOC_ELEM_OVERHEAD + MIN_DATA_SIZE) {
		/* There is not enough space before start, but we may be able to
		 * move the start forward by one page.
		 */
		if (n_segs == 1)
			goto free_unlock;

		/* move start */
		aligned_start = RTE_PTR_ADD(aligned_start, page_sz);
		aligned_len -= page_sz;
		n_segs--;
	}
	if (after_space != 0 && after_space <
			MALLOC_ELEM_OVERHEAD + MIN_DATA_SIZE) {
		/* There is not enough space after end, but we may be able to
		 * move the end backwards by one page.
		 */
		if (n_segs == 1)
			goto free_unlock;

		/* move end */
		aligned_end = RTE_PTR_SUB(aligned_end, page_sz);
		aligned_len -= page_sz;
		n_segs--;
	}

	/* now we can finally free us some pages */

	rte_rwlock_write_lock(&mcfg->memory_hotplug_lock);

	/*
	 * we allow secondary processes to clear the heap of this allocated
	 * memory because it is safe to do so, as even if notifications about
	 * unmapped pages don't make it to other processes, heap is shared
	 * across all processes, and will become empty of this memory anyway,
	 * and nothing can allocate it back unless primary process will be able
	 * to deliver allocation message to every single running process.
	 */

	malloc_elem_free_list_remove(elem);

	malloc_elem_hide_region(elem, (void *) aligned_start, aligned_len);

	heap->total_size -= aligned_len;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* notify user about changes in memory map */
		eal_memalloc_mem_event_notify(RTE_MEM_EVENT_FREE,
				aligned_start, aligned_len);

		/* don't care if any of this fails */
		malloc_heap_free_pages(aligned_start, aligned_len);

		request_sync();
	} else {
		struct malloc_mp_req req;

		memset(&req, 0, sizeof(req));

		req.t = REQ_TYPE_FREE;
		req.free_req.addr = aligned_start;
		req.free_req.len = aligned_len;

		/*
		 * we request primary to deallocate pages, but we don't do it
		 * in this thread. instead, we notify primary that we would like
		 * to deallocate pages, and this process will receive another
		 * request (in parallel) that will do it for us on another
		 * thread.
		 *
		 * we also don't really care if this succeeds - the data is
		 * already removed from the heap, so it is, for all intents and
		 * purposes, hidden from the rest of DPDK even if some other
		 * process (including this one) may have these pages mapped.
		 *
		 * notifications about deallocated memory happen during sync.
		 */
		request_to_primary(&req);
	}

	RTE_LOG(DEBUG, EAL, "Heap on socket %d was shrunk by %zdMB\n",
		msl->socket_id, aligned_len >> 20ULL);

	rte_rwlock_write_unlock(&mcfg->memory_hotplug_lock);
free_unlock:
	rte_spinlock_unlock(&(heap->lock));
	return ret;
}

int
malloc_heap_resize(struct malloc_elem *elem, size_t size)
{
	int ret;

	if (!malloc_elem_cookies_ok(elem) || elem->state != ELEM_BUSY)
		return -1;

	rte_spinlock_lock(&(elem->heap->lock));

	ret = malloc_elem_resize(elem, size);

	rte_spinlock_unlock(&(elem->heap->lock));

	return ret;
}

/*
 * Function to retrieve data for heap on given socket
 */
int
malloc_heap_get_stats(struct malloc_heap *heap,
		struct rte_malloc_socket_stats *socket_stats)
{
	size_t idx;
	struct malloc_elem *elem;

	rte_spinlock_lock(&heap->lock);

	/* Initialise variables for heap */
	socket_stats->free_count = 0;
	socket_stats->heap_freesz_bytes = 0;
	socket_stats->greatest_free_size = 0;

	/* Iterate through free list */
	for (idx = 0; idx < RTE_HEAP_NUM_FREELISTS; idx++) {
		for (elem = LIST_FIRST(&heap->free_head[idx]);
			!!elem; elem = LIST_NEXT(elem, free_list))
		{
			socket_stats->free_count++;
			socket_stats->heap_freesz_bytes += elem->size;
			if (elem->size > socket_stats->greatest_free_size)
				socket_stats->greatest_free_size = elem->size;
		}
	}
	/* Get stats on overall heap and allocated memory on this heap */
	socket_stats->heap_totalsz_bytes = heap->total_size;
	socket_stats->heap_allocsz_bytes = (socket_stats->heap_totalsz_bytes -
			socket_stats->heap_freesz_bytes);
	socket_stats->alloc_count = heap->alloc_count;

	rte_spinlock_unlock(&heap->lock);
	return 0;
}

/*
 * Function to retrieve data for heap on given socket
 */
void
malloc_heap_dump(struct malloc_heap *heap, FILE *f)
{
	struct malloc_elem *elem;

	rte_spinlock_lock(&heap->lock);

	fprintf(f, "Heap size: 0x%zx\n", heap->total_size);
	fprintf(f, "Heap alloc count: %u\n", heap->alloc_count);

	elem = heap->first;
	while (elem) {
		malloc_elem_dump(elem, f);
		elem = elem->next;
	}

	rte_spinlock_unlock(&heap->lock);
}

int
rte_eal_malloc_heap_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (register_mp_requests()) {
		RTE_LOG(ERR, EAL, "Couldn't register malloc multiprocess actions\n");
		rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
		return -1;
	}

	/* unlock mem hotplug here. it's safe for primary as no requests can
	 * even come before primary itself is fully initialized, and secondaries
	 * do not need to initialize the heap.
	 */
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);

	/* secondary process does not need to initialize anything */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* add all IOVA-contiguous areas to the heap */
	return rte_memseg_contig_walk(malloc_add_seg, NULL);
}
