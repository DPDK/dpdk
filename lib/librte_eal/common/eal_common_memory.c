/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/queue.h>

#include <rte_fbarray.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "eal_memalloc.h"
#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_memcfg.h"
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
		size_t page_sz, int flags, int mmap_flags)
{
	bool addr_is_hint, allow_shrink, unmap, no_align;
	uint64_t map_sz;
	void *mapped_addr, *aligned_addr;
	uint8_t try = 0;

	if (system_page_sz == 0)
		system_page_sz = sysconf(_SC_PAGESIZE);

	mmap_flags |= MAP_PRIVATE | MAP_ANONYMOUS;

	RTE_LOG(DEBUG, EAL, "Ask a virtual area of 0x%zx bytes\n", *size);

	addr_is_hint = (flags & EAL_VIRTUAL_AREA_ADDR_IS_HINT) > 0;
	allow_shrink = (flags & EAL_VIRTUAL_AREA_ALLOW_SHRINK) > 0;
	unmap = (flags & EAL_VIRTUAL_AREA_UNMAP) > 0;

	if (next_baseaddr == NULL && internal_config.base_virtaddr != 0 &&
			rte_eal_process_type() == RTE_PROC_PRIMARY)
		next_baseaddr = (void *) internal_config.base_virtaddr;

#ifdef RTE_ARCH_64
	if (next_baseaddr == NULL && internal_config.base_virtaddr == 0 &&
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

		mapped_addr = mmap(requested_addr, (size_t)map_sz, PROT_NONE,
				mmap_flags, -1, 0);
		if (mapped_addr == MAP_FAILED && allow_shrink)
			*size -= page_sz;

		if (mapped_addr != MAP_FAILED && addr_is_hint &&
		    mapped_addr != requested_addr) {
			try++;
			next_baseaddr = RTE_PTR_ADD(next_baseaddr, page_sz);
			if (try <= MAX_MMAP_WITH_DEFINED_ADDR_TRIES) {
				/* hint was not used. Try with another offset */
				munmap(mapped_addr, map_sz);
				mapped_addr = MAP_FAILED;
				requested_addr = next_baseaddr;
			}
		}
	} while ((allow_shrink || addr_is_hint) &&
		 mapped_addr == MAP_FAILED && *size > 0);

	/* align resulting address - if map failed, we will ignore the value
	 * anyway, so no need to add additional checks.
	 */
	aligned_addr = no_align ? mapped_addr :
			RTE_PTR_ALIGN(mapped_addr, page_sz);

	if (*size == 0) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area of any size: %s\n",
			strerror(errno));
		rte_errno = errno;
		return NULL;
	} else if (mapped_addr == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area: %s\n",
			strerror(errno));
		/* pass errno up the call chain */
		rte_errno = errno;
		return NULL;
	} else if (requested_addr != NULL && !addr_is_hint &&
			aligned_addr != requested_addr) {
		RTE_LOG(ERR, EAL, "Cannot get a virtual area at requested address: %p (got %p)\n",
			requested_addr, aligned_addr);
		munmap(mapped_addr, map_sz);
		rte_errno = EADDRNOTAVAIL;
		return NULL;
	} else if (requested_addr != NULL && addr_is_hint &&
			aligned_addr != requested_addr) {
		RTE_LOG(WARNING, EAL, "WARNING! Base virtual address hint (%p != %p) not respected!\n",
			requested_addr, aligned_addr);
		RTE_LOG(WARNING, EAL, "   This may cause issues with mapping memory into secondary processes\n");
	} else if (next_baseaddr != NULL) {
		next_baseaddr = RTE_PTR_ADD(aligned_addr, *size);
	}

	RTE_LOG(DEBUG, EAL, "Virtual area found at %p (size = 0x%zx)\n",
		aligned_addr, *size);

	if (unmap) {
		munmap(mapped_addr, map_sz);
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
			munmap(mapped_addr, before_len);

		/* unmap space after aligned end mmap address */
		after_len = RTE_PTR_DIFF(map_end, aligned_end);
		if (after_len > 0)
			munmap(aligned_end, after_len);
	}

	return aligned_addr;
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

	memset(&vi, 0, sizeof(vi));

	vi.iova = iova;
	/* for legacy mem, we can get away with scanning VA-contiguous segments,
	 * as we know they are PA-contiguous as well
	 */
	if (internal_config.legacy_mem)
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
	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_config.legacy_mem) {
		RTE_LOG(DEBUG, EAL, "Registering mem event callbacks not supported\n");
		rte_errno = ENOTSUP;
		return -1;
	}
	return eal_memalloc_mem_event_callback_register(name, clb, arg);
}

int
rte_mem_event_callback_unregister(const char *name, void *arg)
{
	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_config.legacy_mem) {
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
	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_config.legacy_mem) {
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
	/* FreeBSD boots with legacy mem enabled by default */
	if (internal_config.legacy_mem) {
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

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	config = rte_eal_get_configuration();
	config->mem_config->nchannel = internal_config.force_nchannel;
	config->mem_config->nrank = internal_config.force_nrank;

	return 0;
}

/* Lock page in physical memory and prevent from swapping. */
int
rte_mem_lock_page(const void *virt)
{
	unsigned long virtual = (unsigned long)virt;
	int page_size = getpagesize();
	unsigned long aligned = (virtual & ~(page_size - 1));
	return mlock((void *)aligned, page_size);
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

/* init memory subsystem */
int
rte_eal_memory_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int retval;
	RTE_LOG(DEBUG, EAL, "Setting up physically contiguous memory...\n");

	if (!mcfg)
		return -1;

	/* lock mem hotplug here, to prevent races while we init */
	rte_mcfg_mem_read_lock();

	if (rte_eal_memseg_init() < 0)
		goto fail;

	if (eal_memalloc_init() < 0)
		goto fail;

	retval = rte_eal_process_type() == RTE_PROC_PRIMARY ?
			rte_eal_hugepage_init() :
			rte_eal_hugepage_attach();
	if (retval < 0)
		goto fail;

	if (internal_config.no_shconf == 0 && rte_eal_memdevice_init() < 0)
		goto fail;

	return 0;
fail:
	rte_mcfg_mem_read_unlock();
	return -1;
}
