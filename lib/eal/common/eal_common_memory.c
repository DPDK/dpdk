/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <rte_fbarray.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <rte_telemetry.h>
#endif

#include "eal_memalloc.h"
#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_memcfg.h"
#include "eal_options.h"
#include "malloc_elem.h"
#include "malloc_heap.h"

/*
 * Try to mmap *size bytes in /dev/zero. If it is successful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by hugepage_sz until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of hugepage size.
 */

#define MEMSEG_LIST_FMT "memseg-%" PRIu64 "k-%i-%i"

static void *next_baseaddr;
static uint64_t system_page_sz;

#define MAX_MMAP_WITH_DEFINED_ADDR_TRIES 5
void *
eal_get_virtual_area(void *requested_addr, size_t *size,
	size_t page_sz, int flags, int reserve_flags)
{
	bool addr_is_hint, allow_shrink, unmap, no_align;
	uint64_t map_sz;
	void *mapped_addr, *aligned_addr;
	uint8_t try = 0;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (system_page_sz == 0)
		system_page_sz = rte_mem_page_size();

	RTE_LOG(DEBUG, EAL, "Ask a virtual area of 0x%zx bytes\n", *size);

	addr_is_hint = (flags & EAL_VIRTUAL_AREA_ADDR_IS_HINT) > 0;
	allow_shrink = (flags & EAL_VIRTUAL_AREA_ALLOW_SHRINK) > 0;
	unmap = (flags & EAL_VIRTUAL_AREA_UNMAP) > 0;

	if (next_baseaddr == NULL && internal_conf->base_virtaddr != 0 &&
			rte_eal_process_type() == RTE_PROC_PRIMARY)
		next_baseaddr = (void *) internal_conf->base_virtaddr;

#ifdef RTE_ARCH_64
	if (next_baseaddr == NULL && internal_conf->base_virtaddr == 0 &&
			rte_eal_process_type() == RTE_PROC_PRIMARY)
		next_baseaddr = (void *) eal_get_baseaddr();
#endif
	if (requested_addr == NULL && next_baseaddr != NULL) {
		requested_addr = next_baseaddr;
		requested_addr = RTE_PTR_ALIGN(requested_addr, page_sz);
		addr_is_hint = true;
	}

	/* we don't need alignment of resulting pointer in the following cases:
	 *
	 * 1. page size is equal to system size
	 * 2. we have a requested address, and it is page-aligned, and we will
	 *    be discarding the address if we get a different one.
	 *
	 * for all other cases, alignment is potentially necessary.
	 */
	no_align = (requested_addr != NULL &&
		requested_addr == RTE_PTR_ALIGN(requested_addr, page_sz) &&
		!addr_is_hint) ||
		page_sz == system_page_sz;

	do {
		map_sz = no_align ? *size : *size + page_sz;
		if (map_sz > SIZE_MAX) {
			RTE_LOG(ERR, EAL, "Map size too big\n");
			rte_errno = E2BIG;
			return NULL;
		}

		mapped_addr = eal_mem_reserve(
			requested_addr, (size_t)map_sz, reserve_flags);
		if ((mapped_addr == NULL) && allow_shrink)
			*size -= page_sz;

		if ((mapped_addr != NULL) && addr_is_hint &&
				(mapped_addr != requested_addr)) {
			try++;
			next_baseaddr = RTE_PTR_ADD(next_baseaddr, page_sz);
			if (try <= MAX_MMAP_WITH_DEFINED_ADDR_TRIES) {
				/* hint was not used. Try with another offset */
				eal_mem_free(mapped_addr, map_sz);
				mapped_addr = NULL;
				requested_addr = next_baseaddr;
			}
		}
	} while ((allow_shrink || addr_is_hint) &&
		(mapped_addr == NULL) && (*size > 0));

	/* align resulting address - if map failed, we will ignore the value
	 * anyway, so no need to add additional checks.
	 */
	aligned_addr = no_align ? mapped_addr :
			RTE_PTR_ALIGN(mapped_addr, page_sz);

	if (*size == 0) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area of any size: %s\n",
			rte_strerror(rte_errno));
		return NULL;
	} else if (mapped_addr == NULL) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area: %s\n",
			rte_strerror(rte_errno));
		return NULL;
	} else if (requested_addr != NULL && !addr_is_hint &&
			aligned_addr != requested_addr) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area at requested address: %p (got %p)\n",
			requested_addr, aligned_addr);
		eal_mem_free(mapped_addr, map_sz);
		rte_errno = EADDRNOTAVAIL;
		return NULL;
	} else if (requested_addr != NULL && addr_is_hint &&
			aligned_addr != requested_addr) {
		/*
		 * demote this warning to debug if we did not explicitly request
		 * a base virtual address.
		 */
		if (internal_conf->base_virtaddr != 0) {
			RTE_LOG(WARNING, EAL, "WARNING! Base virtual address hint (%p != %p) not respected!\n",
				requested_addr, aligned_addr);
			RTE_LOG(WARNING, EAL, "   This may cause issues with mapping memory into secondary processes\n");
		} else {
			RTE_LOG(DEBUG, EAL, "WARNING! Base virtual address hint (%p != %p) not respected!\n",
				requested_addr, aligned_addr);
			RTE_LOG(DEBUG, EAL, "   This may cause issues with mapping memory into secondary processes\n");
		}
	} else if (next_baseaddr != NULL) {
		next_baseaddr = RTE_PTR_ADD(aligned_addr, *size);
	}

	RTE_LOG(DEBUG, EAL, "Virtual area found at %p (size = 0x%zx)\n",
		aligned_addr, *size);

	if (unmap) {
		eal_mem_free(mapped_addr, map_sz);
	} else if (!no_align) {
		void *map_end, *aligned_end;
		size_t before_len, after_len;

		/* when we reserve space with alignment, we add alignment to
		 * mapping size. On 32-bit, if 1GB alignment was requested, this
		 * would waste 1GB of address space, which is a luxury we cannot
		 * afford. so, if alignment was performed, check if any unneeded
		 * address space can be unmapped back.
		 */

		map_end = RTE_PTR_ADD(mapped_addr, (size_t)map_sz);
		aligned_end = RTE_PTR_ADD(aligned_addr, *size);

		/* unmap space before aligned mmap address */
		before_len = RTE_PTR_DIFF(aligned_addr, mapped_addr);
		if (before_len > 0)
			eal_mem_free(mapped_addr, before_len);

		/* unmap space after aligned end mmap address */
		after_len = RTE_PTR_DIFF(map_end, aligned_end);
		if (after_len > 0)
			eal_mem_free(aligned_end, after_len);
	}

	if (!unmap) {
		/* Exclude these pages from a core dump. */
		eal_mem_set_dump(aligned_addr, *size, false);
	}

	return aligned_addr;
}

int
eal_memseg_list_init_named(struct rte_memseg_list *msl, const char *name,
		uint64_t page_sz, int n_segs, int socket_id, bool heap)
{
	if (rte_fbarray_init(&msl->memseg_arr, name, n_segs,
			sizeof(struct rte_memseg))) {
		RTE_LOG(ERR, EAL, "Cannot allocate memseg list: %s\n",
			rte_strerror(rte_errno));
		return -1;
	}

	msl->page_sz = page_sz;
	msl->socket_id = socket_id;
	msl->base_va = NULL;
	msl->heap = heap;

	RTE_LOG(DEBUG, EAL,
		"Memseg list allocated at socket %i, page size 0x%"PRIx64"kB\n",
		socket_id, page_sz >> 10);

	return 0;
}

int
eal_memseg_list_init(struct rte_memseg_list *msl, uint64_t page_sz,
		int n_segs, int socket_id, int type_msl_idx, bool heap)
{
	char name[RTE_FBARRAY_NAME_LEN];

	snprintf(name, sizeof(name), MEMSEG_LIST_FMT, page_sz >> 10, socket_id,
		 type_msl_idx);

	return eal_memseg_list_init_named(
		msl, name, page_sz, n_segs, socket_id, heap);
}

int
eal_memseg_list_alloc(struct rte_memseg_list *msl, int reserve_flags)
{
	size_t page_sz, mem_sz;
	void *addr;

	page_sz = msl->page_sz;
	mem_sz = page_sz * msl->memseg_arr.len;

	addr = eal_get_virtual_area(
		msl->base_va, &mem_sz, page_sz, 0, reserve_flags);
	if (addr == NULL) {
#ifndef RTE_EXEC_ENV_WINDOWS
		/* The hint would be misleading on Windows, because address
		 * is by default system-selected (base VA = 0).
		 * However, this function is called from many places,
		 * including common code, so don't duplicate the message.
		 */
		if (rte_errno == EADDRNOTAVAIL)
			RTE_LOG(ERR, EAL, "Cannot reserve %llu bytes at [%p] - "
				"please use '--" OPT_BASE_VIRTADDR "' option\n",
				(unsigned long long)mem_sz, msl->base_va);
#endif
		return -1;
	}
	msl->base_va = addr;
	msl->len = mem_sz;

	RTE_LOG(DEBUG, EAL, "VA reserved for memseg list at %p, size %zx\n",
			addr, mem_sz);

	return 0;
}

void
eal_memseg_list_populate(struct rte_memseg_list *msl, void *addr, int n_segs)
{
	size_t page_sz = msl->page_sz;
	int i;

	for (i = 0; i < n_segs; i++) {
		struct rte_fbarray *arr = &msl->memseg_arr;
		struct rte_memseg *ms = rte_fbarray_get(arr, i);

		if (rte_eal_iova_mode() == RTE_IOVA_VA)
			ms->iova = (uintptr_t)addr;
		else
			ms->iova = RTE_BAD_IOVA;
		ms->addr = addr;
		ms->hugepage_sz = page_sz;
		ms->socket_id = 0;
		ms->len = page_sz;

		rte_fbarray_set_used(arr, i);

		addr = RTE_PTR_ADD(addr, page_sz);
	}
}

static struct rte_memseg *
virt2memseg(const void *addr, const struct rte_memseg_list *msl)
{
	const struct rte_fbarray *arr;
	void *start, *end;
	int ms_idx;

	if (msl == NULL)
		return NULL;

	/* a memseg list was specified, check if it's the right one */
	start = msl->base_va;
	end = RTE_PTR_ADD(start, msl->len);

	if (addr < start || addr >= end)
		return NULL;

	/* now, calculate index */
	arr = &msl->memseg_arr;
	ms_idx = RTE_PTR_DIFF(addr, msl->base_va) / msl->page_sz;
	return rte_fbarray_get(arr, ms_idx);
}

static struct rte_memseg_list *
virt2memseg_list(const void *addr)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *msl;
	int msl_idx;

	for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS; msl_idx++) {
		void *start, *end;
		msl = &mcfg->memsegs[msl_idx];

		start = msl->base_va;
		end = RTE_PTR_ADD(start, msl->len);
		if (addr >= start && addr < end)
			break;
	}
	/* if we didn't find our memseg list */
	if (msl_idx == RTE_MAX_MEMSEG_LISTS)
		return NULL;
	return msl;
}

struct rte_memseg_list *
rte_mem_virt2memseg_list(const void *addr)
{
	return virt2memseg_list(addr);
}

struct virtiova {
	rte_iova_t iova;
	void *virt;
};
static int
find_virt(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	struct virtiova *vi = arg;
	if (vi->iova >= ms->iova && vi->iova < (ms->iova + ms->len)) {
		size_t offset = vi->iova - ms->iova;
		vi->virt = RTE_PTR_ADD(ms->addr, offset);
		/* stop the walk */
		return 1;
	}
	return 0;
}
static int
find_virt_legacy(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, size_t len, void *arg)
{
	struct virtiova *vi = arg;
	if (vi->iova >= ms->iova && vi->iova < (ms->iova + len)) {
		size_t offset = vi->iova - ms->iova;
		vi->virt = RTE_PTR_ADD(ms->addr, offset);
		/* stop the walk */
		return 1;
	}
	return 0;
}

void *
rte_mem_iova2virt(rte_iova_t iova)
{
	struct virtiova vi;
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	memset(&vi, 0, sizeof(vi));

	vi.iova = iova;
	/* for legacy mem, we can get away with scanning VA-contiguous segments,
	 * as we know they are PA-contiguous as well
	 */
	if (internal_conf->legacy_mem)
		rte_memseg_contig_walk(find_virt_legacy, &vi);
	else
		rte_memseg_walk(find_virt, &vi);

	return vi.virt;
}

struct rte_memseg *
rte_mem_virt2memseg(const void *addr, const struct rte_memseg_list *msl)
{
	return virt2memseg(addr, msl != NULL ? msl :
			rte_mem_virt2memseg_list(addr));
}

static int
physmem_size(const struct rte_memseg_list *msl, void *arg)
{
	uint64_t *total_len = arg;

	if (msl->external)
		return 0;

	*total_len += msl->memseg_arr.count * msl->page_sz;

	return 0;
}

/* get the total size of memory */
uint64_t
rte_eal_get_physmem_size(void)
{
	uint64_t total_len = 0;

	rte_memseg_list_walk(physmem_size, &total_len);

	return total_len;
}

static int
dump_memseg(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int msl_idx, ms_idx, fd;
	FILE *f = arg;

	msl_idx = msl - mcfg->memsegs;
	if (msl_idx < 0 || msl_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	ms_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
	if (ms_idx < 0)
		return -1;

	fd = eal_memalloc_get_seg_fd(msl_idx, ms_idx);
	fprintf(f, "Segment %i-%i: IOVA:0x%"PRIx64", len:%zu, "
			"virt:%p, socket_id:%"PRId32", "
			"hugepage_sz:%"PRIu64", nchannel:%"PRIx32", "
			"nrank:%"PRIx32" fd:%i\n",
			msl_idx, ms_idx,
			ms->iova,
			ms->len,
			ms->addr,
			ms->socket_id,
			ms->hugepage_sz,
			ms->nchannel,
			ms->nrank,
			fd);

	return 0;
}

/*
 * Defining here because declared in rte_memory.h, but the actual implementation
 * is in eal_common_memalloc.c, like all other memalloc internals.
 */
int
rte_mem_event_callback_register(const char *name, rte_mem_event_callback_t clb,
		void *arg)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_conf->legacy_mem) {
		RTE_LOG(DEBUG, EAL, "Registering mem event callbacks not supported\n");
		rte_errno = ENOTSUP;
		return -1;
	}
	return eal_memalloc_mem_event_callback_register(name, clb, arg);
}

int
rte_mem_event_callback_unregister(const char *name, void *arg)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_conf->legacy_mem) {
		RTE_LOG(DEBUG, EAL, "Registering mem event callbacks not supported\n");
		rte_errno = ENOTSUP;
		return -1;
	}
	return eal_memalloc_mem_event_callback_unregister(name, arg);
}

int
rte_mem_alloc_validator_register(const char *name,
		rte_mem_alloc_validator_t clb, int socket_id, size_t limit)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_conf->legacy_mem) {
		RTE_LOG(DEBUG, EAL, "Registering mem alloc validators not supported\n");
		rte_errno = ENOTSUP;
		return -1;
	}
	return eal_memalloc_mem_alloc_validator_register(name, clb, socket_id,
			limit);
}

int
rte_mem_alloc_validator_unregister(const char *name, int socket_id)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_conf->legacy_mem) {
		RTE_LOG(DEBUG, EAL, "Registering mem alloc validators not supported\n");
		rte_errno = ENOTSUP;
		return -1;
	}
	return eal_memalloc_mem_alloc_validator_unregister(name, socket_id);
}

/* Dump the physical memory layout on console */
void
rte_dump_physmem_layout(FILE *f)
{
	rte_memseg_walk(dump_memseg, f);
}

static int
check_iova(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	uint64_t *mask = arg;
	rte_iova_t iova;

	/* higher address within segment */
	iova = (ms->iova + ms->len) - 1;
	if (!(iova & *mask))
		return 0;

	RTE_LOG(DEBUG, EAL, "memseg iova %"PRIx64", len %zx, out of range\n",
			    ms->iova, ms->len);

	RTE_LOG(DEBUG, EAL, "\tusing dma mask %"PRIx64"\n", *mask);
	return 1;
}

#define MAX_DMA_MASK_BITS 63

/* check memseg iovas are within the required range based on dma mask */
static int
check_dma_mask(uint8_t maskbits, bool thread_unsafe)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	uint64_t mask;
	int ret;

	/* Sanity check. We only check width can be managed with 64 bits
	 * variables. Indeed any higher value is likely wrong. */
	if (maskbits > MAX_DMA_MASK_BITS) {
		RTE_LOG(ERR, EAL, "wrong dma mask size %u (Max: %u)\n",
				   maskbits, MAX_DMA_MASK_BITS);
		return -1;
	}

	/* create dma mask */
	mask = ~((1ULL << maskbits) - 1);

	if (thread_unsafe)
		ret = rte_memseg_walk_thread_unsafe(check_iova, &mask);
	else
		ret = rte_memseg_walk(check_iova, &mask);

	if (ret)
		/*
		 * Dma mask precludes hugepage usage.
		 * This device can not be used and we do not need to keep
		 * the dma mask.
		 */
		return 1;

	/*
	 * we need to keep the more restricted maskbit for checking
	 * potential dynamic memory allocation in the future.
	 */
	mcfg->dma_maskbits = mcfg->dma_maskbits == 0 ? maskbits :
			     RTE_MIN(mcfg->dma_maskbits, maskbits);

	return 0;
}

int
rte_mem_check_dma_mask(uint8_t maskbits)
{
	return check_dma_mask(maskbits, false);
}

int
rte_mem_check_dma_mask_thread_unsafe(uint8_t maskbits)
{
	return check_dma_mask(maskbits, true);
}

/*
 * Set dma mask to use when memory initialization is done.
 *
 * This function should ONLY be used by code executed before the memory
 * initialization. PMDs should use rte_mem_check_dma_mask if addressing
 * limitations by the device.
 */
void
rte_mem_set_dma_mask(uint8_t maskbits)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	mcfg->dma_maskbits = mcfg->dma_maskbits == 0 ? maskbits :
			     RTE_MIN(mcfg->dma_maskbits, maskbits);
}

/* return the number of memory channels */
unsigned rte_memory_get_nchannel(void)
{
	return rte_eal_get_configuration()->mem_config->nchannel;
}

/* return the number of memory rank */
unsigned rte_memory_get_nrank(void)
{
	return rte_eal_get_configuration()->mem_config->nrank;
}

static int
rte_eal_memdevice_init(void)
{
	struct rte_config *config;
	const struct internal_config *internal_conf;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	internal_conf = eal_get_internal_configuration();
	config = rte_eal_get_configuration();
	config->mem_config->nchannel = internal_conf->force_nchannel;
	config->mem_config->nrank = internal_conf->force_nrank;

	return 0;
}

/* Lock page in physical memory and prevent from swapping. */
int
rte_mem_lock_page(const void *virt)
{
	uintptr_t virtual = (uintptr_t)virt;
	size_t page_size = rte_mem_page_size();
	uintptr_t aligned = RTE_PTR_ALIGN_FLOOR(virtual, page_size);
	return rte_mem_lock((void *)aligned, page_size);
}

int
rte_memseg_contig_walk_thread_unsafe(rte_memseg_contig_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ms_idx, ret = 0;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];
		const struct rte_memseg *ms;
		struct rte_fbarray *arr;

		if (msl->memseg_arr.count == 0)
			continue;

		arr = &msl->memseg_arr;

		ms_idx = rte_fbarray_find_next_used(arr, 0);
		while (ms_idx >= 0) {
			int n_segs;
			size_t len;

			ms = rte_fbarray_get(arr, ms_idx);

			/* find how many more segments there are, starting with
			 * this one.
			 */
			n_segs = rte_fbarray_find_contig_used(arr, ms_idx);
			len = n_segs * msl->page_sz;

			ret = func(msl, ms, len, arg);
			if (ret)
				return ret;
			ms_idx = rte_fbarray_find_next_used(arr,
					ms_idx + n_segs);
		}
	}
	return 0;
}

int
rte_memseg_contig_walk(rte_memseg_contig_walk_t func, void *arg)
{
	int ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_mcfg_mem_read_lock();
	ret = rte_memseg_contig_walk_thread_unsafe(func, arg);
	rte_mcfg_mem_read_unlock();

	return ret;
}

int
rte_memseg_walk_thread_unsafe(rte_memseg_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ms_idx, ret = 0;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];
		const struct rte_memseg *ms;
		struct rte_fbarray *arr;

		if (msl->memseg_arr.count == 0)
			continue;

		arr = &msl->memseg_arr;

		ms_idx = rte_fbarray_find_next_used(arr, 0);
		while (ms_idx >= 0) {
			ms = rte_fbarray_get(arr, ms_idx);
			ret = func(msl, ms, arg);
			if (ret)
				return ret;
			ms_idx = rte_fbarray_find_next_used(arr, ms_idx + 1);
		}
	}
	return 0;
}

int
rte_memseg_walk(rte_memseg_walk_t func, void *arg)
{
	int ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_mcfg_mem_read_lock();
	ret = rte_memseg_walk_thread_unsafe(func, arg);
	rte_mcfg_mem_read_unlock();

	return ret;
}

int
rte_memseg_list_walk_thread_unsafe(rte_memseg_list_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ret = 0;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];

		if (msl->base_va == NULL)
			continue;

		ret = func(msl, arg);
		if (ret)
			return ret;
	}
	return 0;
}

int
rte_memseg_list_walk(rte_memseg_list_walk_t func, void *arg)
{
	int ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_mcfg_mem_read_lock();
	ret = rte_memseg_list_walk_thread_unsafe(func, arg);
	rte_mcfg_mem_read_unlock();

	return ret;
}

int
rte_memseg_get_fd_thread_unsafe(const struct rte_memseg *ms)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *msl;
	struct rte_fbarray *arr;
	int msl_idx, seg_idx, ret;

	if (ms == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	msl = rte_mem_virt2memseg_list(ms->addr);
	if (msl == NULL) {
		rte_errno = EINVAL;
		return -1;
	}
	arr = &msl->memseg_arr;

	msl_idx = msl - mcfg->memsegs;
	seg_idx = rte_fbarray_find_idx(arr, ms);

	if (!rte_fbarray_is_used(arr, seg_idx)) {
		rte_errno = ENOENT;
		return -1;
	}

	/* segment fd API is not supported for external segments */
	if (msl->external) {
		rte_errno = ENOTSUP;
		return -1;
	}

	ret = eal_memalloc_get_seg_fd(msl_idx, seg_idx);
	if (ret < 0) {
		rte_errno = -ret;
		ret = -1;
	}
	return ret;
}

int
rte_memseg_get_fd(const struct rte_memseg *ms)
{
	int ret;

	rte_mcfg_mem_read_lock();
	ret = rte_memseg_get_fd_thread_unsafe(ms);
	rte_mcfg_mem_read_unlock();

	return ret;
}

int
rte_memseg_get_fd_offset_thread_unsafe(const struct rte_memseg *ms,
		size_t *offset)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *msl;
	struct rte_fbarray *arr;
	int msl_idx, seg_idx, ret;

	if (ms == NULL || offset == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	msl = rte_mem_virt2memseg_list(ms->addr);
	if (msl == NULL) {
		rte_errno = EINVAL;
		return -1;
	}
	arr = &msl->memseg_arr;

	msl_idx = msl - mcfg->memsegs;
	seg_idx = rte_fbarray_find_idx(arr, ms);

	if (!rte_fbarray_is_used(arr, seg_idx)) {
		rte_errno = ENOENT;
		return -1;
	}

	/* segment fd API is not supported for external segments */
	if (msl->external) {
		rte_errno = ENOTSUP;
		return -1;
	}

	ret = eal_memalloc_get_seg_fd_offset(msl_idx, seg_idx, offset);
	if (ret < 0) {
		rte_errno = -ret;
		ret = -1;
	}
	return ret;
}

int
rte_memseg_get_fd_offset(const struct rte_memseg *ms, size_t *offset)
{
	int ret;

	rte_mcfg_mem_read_lock();
	ret = rte_memseg_get_fd_offset_thread_unsafe(ms, offset);
	rte_mcfg_mem_read_unlock();

	return ret;
}

int
rte_extmem_register(void *va_addr, size_t len, rte_iova_t iova_addrs[],
		unsigned int n_pages, size_t page_sz)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	unsigned int socket_id, n;
	int ret = 0;

	if (va_addr == NULL || page_sz == 0 || len == 0 ||
			!rte_is_power_of_2(page_sz) ||
			RTE_ALIGN(len, page_sz) != len ||
			((len / page_sz) != n_pages && iova_addrs != NULL) ||
			!rte_is_aligned(va_addr, page_sz)) {
		rte_errno = EINVAL;
		return -1;
	}
	rte_mcfg_mem_write_lock();

	/* make sure the segment doesn't already exist */
	if (malloc_heap_find_external_seg(va_addr, len) != NULL) {
		rte_errno = EEXIST;
		ret = -1;
		goto unlock;
	}

	/* get next available socket ID */
	socket_id = mcfg->next_socket_id;
	if (socket_id > INT32_MAX) {
		RTE_LOG(ERR, EAL, "Cannot assign new socket ID's\n");
		rte_errno = ENOSPC;
		ret = -1;
		goto unlock;
	}

	/* we can create a new memseg */
	n = len / page_sz;
	if (malloc_heap_create_external_seg(va_addr, iova_addrs, n,
			page_sz, "extmem", socket_id) == NULL) {
		ret = -1;
		goto unlock;
	}

	/* memseg list successfully created - increment next socket ID */
	mcfg->next_socket_id++;
unlock:
	rte_mcfg_mem_write_unlock();
	return ret;
}

int
rte_extmem_unregister(void *va_addr, size_t len)
{
	struct rte_memseg_list *msl;
	int ret = 0;

	if (va_addr == NULL || len == 0) {
		rte_errno = EINVAL;
		return -1;
	}
	rte_mcfg_mem_write_lock();

	/* find our segment */
	msl = malloc_heap_find_external_seg(va_addr, len);
	if (msl == NULL) {
		rte_errno = ENOENT;
		ret = -1;
		goto unlock;
	}

	ret = malloc_heap_destroy_external_seg(msl);
unlock:
	rte_mcfg_mem_write_unlock();
	return ret;
}

static int
sync_memory(void *va_addr, size_t len, bool attach)
{
	struct rte_memseg_list *msl;
	int ret = 0;

	if (va_addr == NULL || len == 0) {
		rte_errno = EINVAL;
		return -1;
	}
	rte_mcfg_mem_write_lock();

	/* find our segment */
	msl = malloc_heap_find_external_seg(va_addr, len);
	if (msl == NULL) {
		rte_errno = ENOENT;
		ret = -1;
		goto unlock;
	}
	if (attach)
		ret = rte_fbarray_attach(&msl->memseg_arr);
	else
		ret = rte_fbarray_detach(&msl->memseg_arr);

unlock:
	rte_mcfg_mem_write_unlock();
	return ret;
}

int
rte_extmem_attach(void *va_addr, size_t len)
{
	return sync_memory(va_addr, len, true);
}

int
rte_extmem_detach(void *va_addr, size_t len)
{
	return sync_memory(va_addr, len, false);
}

/* detach all EAL memory */
int
rte_eal_memory_detach(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	size_t page_sz = rte_mem_page_size();
	unsigned int i;

	if (internal_conf->in_memory == 1)
		return 0;

	rte_rwlock_write_lock(&mcfg->memory_hotplug_lock);

	/* detach internal memory subsystem data first */
	if (eal_memalloc_cleanup())
		RTE_LOG(ERR, EAL, "Could not release memory subsystem data\n");

	for (i = 0; i < RTE_DIM(mcfg->memsegs); i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];

		/* skip uninitialized segments */
		if (msl->base_va == NULL)
			continue;
		/*
		 * external segments are supposed to be detached at this point,
		 * but if they aren't, we can't really do anything about it,
		 * because if we skip them here, they'll become invalid after
		 * we unmap the memconfig anyway. however, if this is externally
		 * referenced memory, we have no business unmapping it.
		 */
		if (!msl->external)
			if (rte_mem_unmap(msl->base_va, msl->len) != 0)
				RTE_LOG(ERR, EAL, "Could not unmap memory: %s\n",
						rte_strerror(rte_errno));

		/*
		 * we are detaching the fbarray rather than destroying because
		 * other processes might still reference this fbarray, and we
		 * have no way of knowing if they still do.
		 */
		if (rte_fbarray_detach(&msl->memseg_arr))
			RTE_LOG(ERR, EAL, "Could not detach fbarray: %s\n",
					rte_strerror(rte_errno));
	}
	rte_rwlock_write_unlock(&mcfg->memory_hotplug_lock);

	/*
	 * we've detached the memseg lists, so we can unmap the shared mem
	 * config - we can't zero it out because it might still be referenced
	 * by other processes.
	 */
	if (internal_conf->no_shconf == 0 && mcfg->mem_cfg_addr != 0) {
		if (rte_mem_unmap(mcfg, RTE_ALIGN(sizeof(*mcfg), page_sz)) != 0)
			RTE_LOG(ERR, EAL, "Could not unmap shared memory config: %s\n",
					rte_strerror(rte_errno));
	}
	rte_eal_get_configuration()->mem_config = NULL;

	return 0;
}

/* init memory subsystem */
int
rte_eal_memory_init(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();
	int retval;

	RTE_LOG(DEBUG, EAL, "Setting up physically contiguous memory...\n");

	if (rte_eal_memseg_init() < 0)
		goto fail;

	if (eal_memalloc_init() < 0)
		goto fail;

	retval = rte_eal_process_type() == RTE_PROC_PRIMARY ?
			rte_eal_hugepage_init() :
			rte_eal_hugepage_attach();
	if (retval < 0)
		goto fail;

	if (internal_conf->no_shconf == 0 && rte_eal_memdevice_init() < 0)
		goto fail;

	return 0;
fail:
	return -1;
}

#ifndef RTE_EXEC_ENV_WINDOWS
#define EAL_MEMZONE_LIST_REQ		"/eal/memzone_list"
#define EAL_MEMZONE_INFO_REQ		"/eal/memzone_info"
#define EAL_HEAP_LIST_REQ		"/eal/heap_list"
#define EAL_HEAP_INFO_REQ		"/eal/heap_info"
#define EAL_MEMSEG_LISTS_REQ		"/eal/memseg_lists"
#define EAL_MEMSEG_LIST_INFO_REQ	"/eal/memseg_list_info"
#define EAL_MEMSEG_INFO_REQ		"/eal/memseg_info"
#define EAL_ELEMENT_LIST_REQ		"/eal/mem_element_list"
#define EAL_ELEMENT_INFO_REQ		"/eal/mem_element_info"
#define ADDR_STR			15


/* Telemetry callback handler to return heap stats for requested heap id. */
static int
handle_eal_heap_info_request(const char *cmd __rte_unused, const char *params,
			     struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_malloc_socket_stats sock_stats;
	struct malloc_heap *heap;
	unsigned int heap_id;

	if (params == NULL || strlen(params) == 0)
		return -1;

	heap_id = (unsigned int)strtoul(params, NULL, 10);

	/* Get the heap stats of user provided heap id */
	heap = &mcfg->malloc_heaps[heap_id];
	malloc_heap_get_stats(heap, &sock_stats);

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "Heap_id", heap_id);
	rte_tel_data_add_dict_string(d, "Name", heap->name);
	rte_tel_data_add_dict_uint(d, "Heap_size",
				   sock_stats.heap_totalsz_bytes);
	rte_tel_data_add_dict_uint(d, "Free_size",
				   sock_stats.heap_freesz_bytes);
	rte_tel_data_add_dict_uint(d, "Alloc_size",
				   sock_stats.heap_allocsz_bytes);
	rte_tel_data_add_dict_uint(d, "Greatest_free_size",
				   sock_stats.greatest_free_size);
	rte_tel_data_add_dict_uint(d, "Alloc_count", sock_stats.alloc_count);
	rte_tel_data_add_dict_uint(d, "Free_count", sock_stats.free_count);

	return 0;
}

/* Telemetry callback handler to list the heap ids setup. */
static int
handle_eal_heap_list_request(const char *cmd __rte_unused,
				const char *params __rte_unused,
				struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_malloc_socket_stats sock_stats;
	unsigned int heap_id;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	/* Iterate through all initialised heaps */
	for (heap_id = 0; heap_id < RTE_MAX_HEAPS; heap_id++) {
		struct malloc_heap *heap = &mcfg->malloc_heaps[heap_id];

		malloc_heap_get_stats(heap, &sock_stats);
		if (sock_stats.heap_totalsz_bytes != 0)
			rte_tel_data_add_array_int(d, heap_id);
	}

	return 0;
}

/* Telemetry callback handler to return memzone info for requested index. */
static int
handle_eal_memzone_info_request(const char *cmd __rte_unused,
				const char *params, struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *msl = NULL;
	int ms_idx, ms_count = 0;
	void *cur_addr, *mz_end;
	struct rte_memzone *mz;
	struct rte_memseg *ms;
	char addr[ADDR_STR];
	unsigned int mz_idx;
	size_t page_sz;

	if (params == NULL || strlen(params) == 0)
		return -1;

	mz_idx = strtoul(params, NULL, 10);

	/* Get the memzone handle using index */
	mz = rte_fbarray_get(&mcfg->memzones, mz_idx);

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "Zone", mz_idx);
	rte_tel_data_add_dict_string(d, "Name", mz->name);
	rte_tel_data_add_dict_uint(d, "Length", mz->len);
	snprintf(addr, ADDR_STR, "%p", mz->addr);
	rte_tel_data_add_dict_string(d, "Address", addr);
	rte_tel_data_add_dict_int(d, "Socket", mz->socket_id);
	rte_tel_data_add_dict_uint(d, "Flags", mz->flags);

	/* go through each page occupied by this memzone */
	msl = rte_mem_virt2memseg_list(mz->addr);
	if (!msl) {
		RTE_LOG(DEBUG, EAL, "Skipping bad memzone\n");
		return -1;
	}
	page_sz = (size_t)mz->hugepage_sz;
	cur_addr = RTE_PTR_ALIGN_FLOOR(mz->addr, page_sz);
	mz_end = RTE_PTR_ADD(cur_addr, mz->len);

	ms_idx = RTE_PTR_DIFF(mz->addr, msl->base_va) / page_sz;
	ms = rte_fbarray_get(&msl->memseg_arr, ms_idx);

	rte_tel_data_add_dict_uint(d, "Hugepage_size", page_sz);
	snprintf(addr, ADDR_STR, "%p", ms->addr);
	rte_tel_data_add_dict_string(d, "Hugepage_base", addr);

	do {
		/* advance VA to next page */
		cur_addr = RTE_PTR_ADD(cur_addr, page_sz);

		/* memzones occupy contiguous segments */
		++ms;
		ms_count++;
	} while (cur_addr < mz_end);

	rte_tel_data_add_dict_int(d, "Hugepage_used", ms_count);

	return 0;
}

static void
memzone_list_cb(const struct rte_memzone *mz __rte_unused,
		 void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_tel_data *d = arg;
	int mz_idx;

	mz_idx = rte_fbarray_find_idx(&mcfg->memzones, mz);
	rte_tel_data_add_array_int(d, mz_idx);
}


/* Telemetry callback handler to list the memzones reserved. */
static int
handle_eal_memzone_list_request(const char *cmd __rte_unused,
				const char *params __rte_unused,
				struct rte_tel_data *d)
{
	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	rte_memzone_walk(memzone_list_cb, d);

	return 0;
}

/* n_vals is the number of params to be parsed. */
static int
parse_params(const char *params, uint32_t *vals, size_t n_vals)
{
	char dlim[2] = ",";
	char *params_args;
	size_t count = 0;
	char *token;

	if (vals == NULL || params == NULL || strlen(params) == 0)
		return -1;

	/* strtok expects char * and param is const char *. Hence on using
	 * params as "const char *" compiler throws warning.
	 */
	params_args = strdup(params);
	if (params_args == NULL)
		return -1;

	token = strtok(params_args, dlim);
	while (token && isdigit(*token) && count < n_vals) {
		vals[count++] = strtoul(token, NULL, 10);
		token = strtok(NULL, dlim);
	}

	free(params_args);

	if (count < n_vals)
		return -1;

	return 0;
}

static int
handle_eal_memseg_lists_request(const char *cmd __rte_unused,
				const char *params __rte_unused,
				struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg;
	int i;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);

	rte_mcfg_mem_read_lock();
	mcfg = rte_eal_get_configuration()->mem_config;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];
		if (msl->memseg_arr.count == 0)
			continue;

		rte_tel_data_add_array_int(d, i);
	}
	rte_mcfg_mem_read_unlock();

	return 0;
}

static int
handle_eal_memseg_list_info_request(const char *cmd __rte_unused,
				    const char *params, struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg;
	struct rte_memseg_list *msl;
	struct rte_fbarray *arr;
	uint32_t ms_list_idx;
	int ms_idx;
	/* size of an array == num params to be parsed. */
	uint32_t vals[1] = {0};

	if (parse_params(params, vals, RTE_DIM(vals)) < 0)
		return -1;

	ms_list_idx = vals[0];
	if (ms_list_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);

	rte_mcfg_mem_read_lock();
	mcfg = rte_eal_get_configuration()->mem_config;
	msl = &mcfg->memsegs[ms_list_idx];
	if (msl->memseg_arr.count == 0)
		goto done;

	arr = &msl->memseg_arr;

	ms_idx = rte_fbarray_find_next_used(arr, 0);
	while (ms_idx >= 0) {
		rte_tel_data_add_array_int(d, ms_idx);
		ms_idx = rte_fbarray_find_next_used(arr, ms_idx + 1);
	}

done:
	rte_mcfg_mem_read_unlock();

	return 0;
}

static int
handle_eal_memseg_info_request(const char *cmd __rte_unused,
			       const char *params, struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg;
	uint64_t ms_start_addr, ms_end_addr, ms_size, hugepage_size, ms_iova;
	struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct rte_fbarray *arr;
	char addr[ADDR_STR];
	uint32_t ms_list_idx = 0;
	uint32_t ms_idx = 0;
	int32_t ms_socket_id;
	uint32_t ms_flags;
	/* size of an array == num params to be parsed. */
	uint32_t vals[2] = {0};

	if (parse_params(params, vals, RTE_DIM(vals)) < 0)
		return -1;

	ms_list_idx = vals[0];
	if (ms_list_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	ms_idx = vals[1];

	rte_mcfg_mem_read_lock();

	mcfg = rte_eal_get_configuration()->mem_config;
	msl = &mcfg->memsegs[ms_list_idx];
	if (msl->memseg_arr.count == 0) {
		rte_mcfg_mem_read_unlock();
		return -1;
	}

	arr = &msl->memseg_arr;
	ms = rte_fbarray_get(arr, ms_idx);
	if (ms == NULL) {
		rte_mcfg_mem_read_unlock();
		RTE_LOG(DEBUG, EAL, "Error fetching requested memseg.\n");
		return -1;
	}

	ms_iova = ms->iova;
	ms_start_addr = ms->addr_64;
	ms_end_addr = (uint64_t)RTE_PTR_ADD(ms_start_addr, ms->len);
	ms_size = ms->len;
	hugepage_size = ms->hugepage_sz;
	ms_socket_id = ms->socket_id;
	ms_flags = ms->flags;

	rte_mcfg_mem_read_unlock();

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_int(d, "Memseg_list_index", ms_list_idx);
	rte_tel_data_add_dict_int(d, "Memseg_index", ms_idx);
	if (ms_iova == RTE_BAD_IOVA)
		snprintf(addr, ADDR_STR, "Bad IOVA");
	else
		snprintf(addr, ADDR_STR, "0x%"PRIx64, ms_iova);

	rte_tel_data_add_dict_string(d, "IOVA_addr", addr);
	snprintf(addr, ADDR_STR, "0x%"PRIx64, ms_start_addr);
	rte_tel_data_add_dict_string(d, "Start_addr", addr);
	snprintf(addr, ADDR_STR, "0x%"PRIx64, ms_end_addr);
	rte_tel_data_add_dict_string(d, "End_addr", addr);
	rte_tel_data_add_dict_uint(d, "Size", ms_size);
	rte_tel_data_add_dict_uint(d, "Hugepage_size", hugepage_size);
	rte_tel_data_add_dict_int(d, "Socket_id", ms_socket_id);
	rte_tel_data_add_dict_int(d, "flags", ms_flags);

	return 0;
}

static int
handle_eal_element_list_request(const char *cmd __rte_unused,
				const char *params, struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg;
	struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct malloc_elem *elem;
	struct malloc_heap *heap;
	uint64_t ms_start_addr, ms_end_addr;
	uint64_t elem_start_addr, elem_end_addr;
	uint32_t ms_list_idx = 0;
	uint32_t heap_id = 0;
	uint32_t ms_idx = 0;
	int elem_count = 0;
	/* size of an array == num params to be parsed. */
	uint32_t vals[3] = {0};

	if (parse_params(params, vals, RTE_DIM(vals)) < 0)
		return -1;

	heap_id = vals[0];
	if (heap_id >= RTE_MAX_HEAPS)
		return -1;

	ms_list_idx = vals[1];
	if (ms_list_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	ms_idx = vals[2];

	rte_mcfg_mem_read_lock();

	mcfg = rte_eal_get_configuration()->mem_config;
	msl = &mcfg->memsegs[ms_list_idx];
	ms = rte_fbarray_get(&msl->memseg_arr, ms_idx);
	if (ms == NULL) {
		rte_mcfg_mem_read_unlock();
		RTE_LOG(DEBUG, EAL, "Error fetching requested memseg.\n");
		return -1;
	}

	ms_start_addr = ms->addr_64;
	ms_end_addr = (uint64_t)RTE_PTR_ADD(ms_start_addr, ms->len);
	rte_mcfg_mem_read_unlock();

	rte_tel_data_start_dict(d);

	heap = &mcfg->malloc_heaps[heap_id];
	rte_spinlock_lock(&heap->lock);

	elem = heap->first;
	while (elem) {
		elem_start_addr = (uint64_t)elem;
		elem_end_addr =
			(uint64_t)RTE_PTR_ADD(elem_start_addr, elem->size);

		if ((uint64_t)elem_start_addr >= ms_start_addr &&
		    (uint64_t)elem_end_addr <= ms_end_addr)
			elem_count++;
		elem = elem->next;
	}

	rte_spinlock_unlock(&heap->lock);

	rte_tel_data_add_dict_int(d, "Element_count", elem_count);

	return 0;
}

static int
handle_eal_element_info_request(const char *cmd __rte_unused,
				const char *params, struct rte_tel_data *d)
{
	struct rte_mem_config *mcfg;
	struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct malloc_elem *elem;
	struct malloc_heap *heap;
	struct rte_tel_data *c;
	uint64_t ms_start_addr, ms_end_addr;
	uint64_t elem_start_addr, elem_end_addr;
	uint32_t ms_list_idx = 0;
	uint32_t heap_id = 0;
	uint32_t ms_idx = 0;
	uint32_t start_elem = 0, end_elem = 0;
	uint32_t count = 0, elem_count = 0;
	char str[ADDR_STR];
	/* size of an array == num params to be parsed. */
	uint32_t vals[5] = {0};

	if (parse_params(params, vals, RTE_DIM(vals)) < 0)
		return -1;

	heap_id = vals[0];
	if (heap_id >= RTE_MAX_HEAPS)
		return -1;

	ms_list_idx = vals[1];
	if (ms_list_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	ms_idx = vals[2];
	start_elem = vals[3];
	end_elem = vals[4];

	if (end_elem < start_elem)
		return -1;

	rte_mcfg_mem_read_lock();

	mcfg = rte_eal_get_configuration()->mem_config;
	msl = &mcfg->memsegs[ms_list_idx];
	ms = rte_fbarray_get(&msl->memseg_arr, ms_idx);
	if (ms == NULL) {
		rte_mcfg_mem_read_unlock();
		RTE_LOG(DEBUG, EAL, "Error fetching requested memseg.\n");
		return -1;
	}

	ms_start_addr = ms->addr_64;
	ms_end_addr = (uint64_t)RTE_PTR_ADD(ms_start_addr, ms->len);

	rte_mcfg_mem_read_unlock();

	rte_tel_data_start_dict(d);

	heap = &mcfg->malloc_heaps[heap_id];
	rte_spinlock_lock(&heap->lock);

	elem = heap->first;
	while (elem) {
		elem_start_addr = (uint64_t)elem;
		elem_end_addr =
			(uint64_t)RTE_PTR_ADD(elem_start_addr, elem->size);

		if (elem_start_addr < ms_start_addr ||
				elem_end_addr > ms_end_addr) {
			elem = elem->next;
			continue;
		}

		if (count < start_elem) {
			elem = elem->next;
			count++;
			continue;
		}

		c = rte_tel_data_alloc();
		if (c == NULL)
			break;

		rte_tel_data_start_dict(c);
		rte_tel_data_add_dict_int(c, "msl_id", ms_list_idx);
		rte_tel_data_add_dict_int(c, "ms_id", ms_idx);
		snprintf(str, ADDR_STR, "0x%"PRIx64, ms_start_addr);
		rte_tel_data_add_dict_string(c, "memseg_start_addr", str);
		snprintf(str, ADDR_STR, "0x%"PRIx64, ms_end_addr);
		rte_tel_data_add_dict_string(c, "memseg_end_addr", str);
		snprintf(str, ADDR_STR, "0x%"PRIx64, elem_start_addr);
		rte_tel_data_add_dict_string(c, "element_start_addr", str);
		snprintf(str, ADDR_STR, "0x%"PRIx64, elem_end_addr);
		rte_tel_data_add_dict_string(c, "element_end_addr", str);
		rte_tel_data_add_dict_int(c, "element_size", elem->size);
		snprintf(str, ADDR_STR, "%s", elem->state == 0 ? "Free" :
			 elem->state == 1 ? "Busy" : elem->state == 2 ?
			 "Pad" : "Error");
		rte_tel_data_add_dict_string(c, "element_state", str);

		snprintf(str, ADDR_STR, "%s_%u", "element", count);
		if (rte_tel_data_add_dict_container(d, str, c, 0) != 0) {
			rte_tel_data_free(c);
			break;
		}

		elem_count++;
		count++;
		if (count > end_elem)
			break;

		elem = elem->next;
	}

	rte_spinlock_unlock(&heap->lock);

	rte_tel_data_add_dict_int(d, "Element_count", elem_count);

	return 0;
}

RTE_INIT(memory_telemetry)
{
	rte_telemetry_register_cmd(
			EAL_MEMZONE_LIST_REQ, handle_eal_memzone_list_request,
			"List of memzone index reserved. Takes no parameters");
	rte_telemetry_register_cmd(
			EAL_MEMZONE_INFO_REQ, handle_eal_memzone_info_request,
			"Returns memzone info. Parameters: int mz_id");
	rte_telemetry_register_cmd(
			EAL_HEAP_LIST_REQ, handle_eal_heap_list_request,
			"List of heap index setup. Takes no parameters");
	rte_telemetry_register_cmd(
			EAL_HEAP_INFO_REQ, handle_eal_heap_info_request,
			"Returns malloc heap stats. Parameters: int heap_id");
	rte_telemetry_register_cmd(
			EAL_MEMSEG_LISTS_REQ,
			handle_eal_memseg_lists_request,
			"Returns array of memseg list IDs. Takes no parameters");
	rte_telemetry_register_cmd(
			EAL_MEMSEG_LIST_INFO_REQ,
			handle_eal_memseg_list_info_request,
			"Returns memseg list info. Parameters: int memseg_list_id");
	rte_telemetry_register_cmd(
			EAL_MEMSEG_INFO_REQ, handle_eal_memseg_info_request,
			"Returns memseg info. Parameter: int memseg_list_id,int memseg_id");
	rte_telemetry_register_cmd(EAL_ELEMENT_LIST_REQ,
			handle_eal_element_list_request,
			"Returns array of heap element IDs. Parameters: int heap_id, int memseg_list_id, int memseg_id");
	rte_telemetry_register_cmd(EAL_ELEMENT_INFO_REQ,
			handle_eal_element_info_request,
			"Returns element info. Parameters: int heap_id, int memseg_list_id, int memseg_id, int start_elem_id, int end_elem_id");
}
#endif
