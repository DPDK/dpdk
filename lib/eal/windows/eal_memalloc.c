/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <rte_errno.h>

#include "eal_internal_cfg.h"
#include "eal_memalloc.h"
#include "eal_memcfg.h"
#include "eal_private.h"
#include "eal_windows.h"

int
eal_memalloc_get_seg_fd(int list_idx, int seg_idx)
{
	/* Hugepages have no associated files in Windows. */
	RTE_SET_USED(list_idx);
	RTE_SET_USED(seg_idx);
	EAL_LOG_NOT_IMPLEMENTED();
	return -ENOTSUP;
}

int
eal_memalloc_get_seg_fd_offset(int list_idx, int seg_idx, size_t *offset)
{
	/* Hugepages have no associated files in Windows. */
	RTE_SET_USED(list_idx);
	RTE_SET_USED(seg_idx);
	RTE_SET_USED(offset);
	EAL_LOG_NOT_IMPLEMENTED();
	return -ENOTSUP;
}

static int
alloc_seg(struct rte_memseg *ms, void *requested_addr, int socket_id,
	struct hugepage_info *hi)
{
	HANDLE current_process;
	unsigned int numa_node;
	size_t alloc_sz;
	void *addr;
	rte_iova_t iova = RTE_BAD_IOVA;
	PSAPI_WORKING_SET_EX_INFORMATION info;
	PSAPI_WORKING_SET_EX_BLOCK *page;

	if (ms->len > 0) {
		/* If a segment is already allocated as needed, return it. */
		if ((ms->addr == requested_addr) &&
			(ms->socket_id == socket_id) &&
			(ms->hugepage_sz == hi->hugepage_sz)) {
			return 0;
		}

		/* Bugcheck, should not happen. */
		RTE_LOG(DEBUG, EAL, "Attempted to reallocate segment %p "
			"(size %zu) on socket %d", ms->addr,
			ms->len, ms->socket_id);
		return -1;
	}

	current_process = GetCurrentProcess();
	numa_node = eal_socket_numa_node(socket_id);
	alloc_sz = hi->hugepage_sz;

	if (requested_addr == NULL) {
		/* Request a new chunk of memory from OS. */
		addr = eal_mem_alloc_socket(alloc_sz, socket_id);
		if (addr == NULL) {
			RTE_LOG(DEBUG, EAL, "Cannot allocate %zu bytes "
				"on socket %d\n", alloc_sz, socket_id);
			return -1;
		}
	} else {
		/* Requested address is already reserved, commit memory. */
		addr = eal_mem_commit(requested_addr, alloc_sz, socket_id);

		/* During commitment, memory is temporary freed and might
		 * be allocated by different non-EAL thread. This is a fatal
		 * error, because it breaks MSL assumptions.
		 */
		if ((addr != NULL) && (addr != requested_addr)) {
			RTE_LOG(CRIT, EAL, "Address %p occupied by an alien "
				" allocation - MSL is not VA-contiguous!\n",
				requested_addr);
			return -1;
		}

		if (addr == NULL) {
			RTE_LOG(DEBUG, EAL, "Cannot commit reserved memory %p "
				"(size %zu) on socket %d\n",
				requested_addr, alloc_sz, socket_id);
			return -1;
		}
	}

	/* Force OS to allocate a physical page and select a NUMA node.
	 * Hugepages are not pageable in Windows, so there's no race
	 * for physical address.
	 */
	*(volatile int *)addr = *(volatile int *)addr;

	iova = rte_mem_virt2iova(addr);
	if (iova == RTE_BAD_IOVA) {
		RTE_LOG(DEBUG, EAL,
			"Cannot get IOVA of allocated segment\n");
		goto error;
	}

	/* Only "Ex" function can handle hugepages. */
	info.VirtualAddress = addr;
	if (!QueryWorkingSetEx(current_process, &info, sizeof(info))) {
		RTE_LOG_WIN32_ERR("QueryWorkingSetEx(%p)", addr);
		goto error;
	}

	page = &info.VirtualAttributes;
	if (!page->Valid || !page->LargePage) {
		RTE_LOG(DEBUG, EAL, "Got regular page instead of a hugepage\n");
		goto error;
	}
	if (page->Node != numa_node) {
		RTE_LOG(DEBUG, EAL,
			"NUMA node hint %u (socket %d) not respected, got %u\n",
			numa_node, socket_id, page->Node);
		goto error;
	}

	ms->addr = addr;
	ms->hugepage_sz = hi->hugepage_sz;
	ms->len = alloc_sz;
	ms->nchannel = rte_memory_get_nchannel();
	ms->nrank = rte_memory_get_nrank();
	ms->iova = iova;
	ms->socket_id = socket_id;

	return 0;

error:
	/* Only jump here when `addr` and `alloc_sz` are valid. */
	if (eal_mem_decommit(addr, alloc_sz) && (rte_errno == EADDRNOTAVAIL)) {
		/* During decommitment, memory is temporarily returned
		 * to the system and the address may become unavailable.
		 */
		RTE_LOG(CRIT, EAL, "Address %p occupied by an alien "
			" allocation - MSL is not VA-contiguous!\n", addr);
	}
	return -1;
}

static int
free_seg(struct rte_memseg *ms)
{
	if (eal_mem_decommit(ms->addr, ms->len)) {
		if (rte_errno == EADDRNOTAVAIL) {
			/* See alloc_seg() for explanation. */
			RTE_LOG(CRIT, EAL, "Address %p occupied by an alien "
				" allocation - MSL is not VA-contiguous!\n",
				ms->addr);
		}
		return -1;
	}

	/* Must clear the segment, because alloc_seg() inspects it. */
	memset(ms, 0, sizeof(*ms));
	return 0;
}

struct alloc_walk_param {
	struct hugepage_info *hi;
	struct rte_memseg **ms;
	size_t page_sz;
	unsigned int segs_allocated;
	unsigned int n_segs;
	int socket;
	bool exact;
};

static int
alloc_seg_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct alloc_walk_param *wa = arg;
	struct rte_memseg_list *cur_msl;
	size_t page_sz;
	int cur_idx, start_idx, j;
	unsigned int msl_idx, need, i;

	if (msl->page_sz != wa->page_sz)
		return 0;
	if (msl->socket_id != wa->socket)
		return 0;

	page_sz = (size_t)msl->page_sz;

	msl_idx = msl - mcfg->memsegs;
	cur_msl = &mcfg->memsegs[msl_idx];

	need = wa->n_segs;

	/* try finding space in memseg list */
	if (wa->exact) {
		/* if we require exact number of pages in a list, find them */
		cur_idx = rte_fbarray_find_next_n_free(
			&cur_msl->memseg_arr, 0, need);
		if (cur_idx < 0)
			return 0;
		start_idx = cur_idx;
	} else {
		int cur_len;

		/* we don't require exact number of pages, so we're going to go
		 * for best-effort allocation. that means finding the biggest
		 * unused block, and going with that.
		 */
		cur_idx = rte_fbarray_find_biggest_free(
			&cur_msl->memseg_arr, 0);
		if (cur_idx < 0)
			return 0;
		start_idx = cur_idx;
		/* adjust the size to possibly be smaller than original
		 * request, but do not allow it to be bigger.
		 */
		cur_len = rte_fbarray_find_contig_free(
			&cur_msl->memseg_arr, cur_idx);
		need = RTE_MIN(need, (unsigned int)cur_len);
	}

	for (i = 0; i < need; i++, cur_idx++) {
		struct rte_memseg *cur;
		void *map_addr;

		cur = rte_fbarray_get(&cur_msl->memseg_arr, cur_idx);
		map_addr = RTE_PTR_ADD(cur_msl->base_va, cur_idx * page_sz);

		if (alloc_seg(cur, map_addr, wa->socket, wa->hi)) {
			RTE_LOG(DEBUG, EAL, "attempted to allocate %i segments, "
				"but only %i were allocated\n", need, i);

			/* if exact number wasn't requested, stop */
			if (!wa->exact)
				goto out;

			/* clean up */
			for (j = start_idx; j < cur_idx; j++) {
				struct rte_memseg *tmp;
				struct rte_fbarray *arr = &cur_msl->memseg_arr;

				tmp = rte_fbarray_get(arr, j);
				rte_fbarray_set_free(arr, j);

				if (free_seg(tmp))
					RTE_LOG(DEBUG, EAL, "Cannot free page\n");
			}
			/* clear the list */
			if (wa->ms)
				memset(wa->ms, 0, sizeof(*wa->ms) * wa->n_segs);

			return -1;
		}
		if (wa->ms)
			wa->ms[i] = cur;

		rte_fbarray_set_used(&cur_msl->memseg_arr, cur_idx);
	}

out:
	wa->segs_allocated = i;
	if (i > 0)
		cur_msl->version++;

	/* if we didn't allocate any segments, move on to the next list */
	return i > 0;
}

struct free_walk_param {
	struct hugepage_info *hi;
	struct rte_memseg *ms;
};
static int
free_seg_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *found_msl;
	struct free_walk_param *wa = arg;
	uintptr_t start_addr, end_addr;
	int msl_idx, seg_idx, ret;

	start_addr = (uintptr_t) msl->base_va;
	end_addr = start_addr + msl->len;

	if ((uintptr_t)wa->ms->addr < start_addr ||
		(uintptr_t)wa->ms->addr >= end_addr)
		return 0;

	msl_idx = msl - mcfg->memsegs;
	seg_idx = RTE_PTR_DIFF(wa->ms->addr, start_addr) / msl->page_sz;

	/* msl is const */
	found_msl = &mcfg->memsegs[msl_idx];
	found_msl->version++;

	rte_fbarray_set_free(&found_msl->memseg_arr, seg_idx);

	ret = free_seg(wa->ms);

	return (ret < 0) ? (-1) : 1;
}

int
eal_memalloc_alloc_seg_bulk(struct rte_memseg **ms, int n_segs,
		size_t page_sz, int socket, bool exact)
{
	unsigned int i;
	int ret = -1;
	struct alloc_walk_param wa;
	struct hugepage_info *hi = NULL;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (internal_conf->legacy_mem) {
		RTE_LOG(ERR, EAL, "dynamic allocation not supported in legacy mode\n");
		return -ENOTSUP;
	}

	for (i = 0; i < internal_conf->num_hugepage_sizes; i++) {
		struct hugepage_info *hpi = &internal_conf->hugepage_info[i];
		if (page_sz == hpi->hugepage_sz) {
			hi = hpi;
			break;
		}
	}
	if (!hi) {
		RTE_LOG(ERR, EAL, "cannot find relevant hugepage_info entry\n");
		return -1;
	}

	memset(&wa, 0, sizeof(wa));
	wa.exact = exact;
	wa.hi = hi;
	wa.ms = ms;
	wa.n_segs = n_segs;
	wa.page_sz = page_sz;
	wa.socket = socket;
	wa.segs_allocated = 0;

	/* memalloc is locked, so it's safe to use thread-unsafe version */
	ret = rte_memseg_list_walk_thread_unsafe(alloc_seg_walk, &wa);
	if (ret == 0) {
		RTE_LOG(ERR, EAL, "cannot find suitable memseg_list\n");
		ret = -1;
	} else if (ret > 0) {
		ret = (int)wa.segs_allocated;
	}

	return ret;
}

struct rte_memseg *
eal_memalloc_alloc_seg(size_t page_sz, int socket)
{
	struct rte_memseg *ms = NULL;
	eal_memalloc_alloc_seg_bulk(&ms, 1, page_sz, socket, true);
	return ms;
}

int
eal_memalloc_free_seg_bulk(struct rte_memseg **ms, int n_segs)
{
	int seg, ret = 0;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* dynamic free not supported in legacy mode */
	if (internal_conf->legacy_mem)
		return -1;

	for (seg = 0; seg < n_segs; seg++) {
		struct rte_memseg *cur = ms[seg];
		struct hugepage_info *hi = NULL;
		struct free_walk_param wa;
		size_t i;
		int walk_res;

		/* if this page is marked as unfreeable, fail */
		if (cur->flags & RTE_MEMSEG_FLAG_DO_NOT_FREE) {
			RTE_LOG(DEBUG, EAL, "Page is not allowed to be freed\n");
			ret = -1;
			continue;
		}

		memset(&wa, 0, sizeof(wa));

		for (i = 0; i < RTE_DIM(internal_conf->hugepage_info); i++) {
			hi = &internal_conf->hugepage_info[i];
			if (cur->hugepage_sz == hi->hugepage_sz)
				break;
		}
		if (i == RTE_DIM(internal_conf->hugepage_info)) {
			RTE_LOG(ERR, EAL, "Can't find relevant hugepage_info entry\n");
			ret = -1;
			continue;
		}

		wa.ms = cur;
		wa.hi = hi;

		/* memalloc is locked, so it's safe to use thread-unsafe version
		 */
		walk_res = rte_memseg_list_walk_thread_unsafe(free_seg_walk,
				&wa);
		if (walk_res == 1)
			continue;
		if (walk_res == 0)
			RTE_LOG(ERR, EAL, "Couldn't find memseg list\n");
		ret = -1;
	}
	return ret;
}

int
eal_memalloc_free_seg(struct rte_memseg *ms)
{
	return eal_memalloc_free_seg_bulk(&ms, 1);
}

int
eal_memalloc_sync_with_primary(void)
{
	/* No multi-process support. */
	EAL_LOG_NOT_IMPLEMENTED();
	return -ENOTSUP;
}

int
eal_memalloc_cleanup(void)
{
	/* not implemented */
	return 0;
}

int
eal_memalloc_init(void)
{
	/* No action required. */
	return 0;
}
