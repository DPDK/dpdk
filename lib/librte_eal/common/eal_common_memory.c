/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

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

/*
 * Try to mmap *size bytes in /dev/zero. If it is successful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by hugepage_sz until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of hugepage size.
 */

#define MEMSEG_LIST_FMT "memseg-%" PRIu64 "k-%i-%i"

static uint64_t baseaddr_offset;
static uint64_t system_page_sz;

void *
eal_get_virtual_area(void *requested_addr, size_t *size,
		size_t page_sz, int flags, int mmap_flags)
{
	bool addr_is_hint, allow_shrink, unmap, no_align;
	uint64_t map_sz;
	void *mapped_addr, *aligned_addr;

	if (system_page_sz == 0)
		system_page_sz = sysconf(_SC_PAGESIZE);

	mmap_flags |= MAP_PRIVATE | MAP_ANONYMOUS;

	RTE_LOG(DEBUG, EAL, "Ask a virtual area of 0x%zx bytes\n", *size);

	addr_is_hint = (flags & EAL_VIRTUAL_AREA_ADDR_IS_HINT) > 0;
	allow_shrink = (flags & EAL_VIRTUAL_AREA_ALLOW_SHRINK) > 0;
	unmap = (flags & EAL_VIRTUAL_AREA_UNMAP) > 0;

	if (requested_addr == NULL && internal_config.base_virtaddr != 0) {
		requested_addr = (void *) (internal_config.base_virtaddr +
				(size_t)baseaddr_offset);
		requested_addr = RTE_PTR_ALIGN(requested_addr, page_sz);
		addr_is_hint = true;
	}

	/* if requested address is not aligned by page size, or if requested
	 * address is NULL, add page size to requested length as we may get an
	 * address that's aligned by system page size, which can be smaller than
	 * our requested page size. additionally, we shouldn't try to align if
	 * system page size is the same as requested page size.
	 */
	no_align = (requested_addr != NULL &&
		((uintptr_t)requested_addr & (page_sz - 1)) == 0) ||
		page_sz == system_page_sz;

	do {
		map_sz = no_align ? *size : *size + page_sz;
		if (map_sz > SIZE_MAX) {
			RTE_LOG(ERR, EAL, "Map size too big\n");
			rte_errno = E2BIG;
			return NULL;
		}

		mapped_addr = mmap(requested_addr, (size_t)map_sz, PROT_READ,
				mmap_flags, -1, 0);
		if (mapped_addr == MAP_FAILED && allow_shrink)
			*size -= page_sz;
	} while (allow_shrink && mapped_addr == MAP_FAILED && *size > 0);

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

	baseaddr_offset += *size;

	return aligned_addr;
}

static uint64_t
get_mem_amount(uint64_t page_sz, uint64_t max_mem)
{
	uint64_t area_sz, max_pages;

	/* limit to RTE_MAX_MEMSEG_PER_LIST pages or RTE_MAX_MEM_MB_PER_LIST */
	max_pages = RTE_MAX_MEMSEG_PER_LIST;
	max_mem = RTE_MIN((uint64_t)RTE_MAX_MEM_MB_PER_LIST << 20, max_mem);

	area_sz = RTE_MIN(page_sz * max_pages, max_mem);

	/* make sure the list isn't smaller than the page size */
	area_sz = RTE_MAX(area_sz, page_sz);

	return RTE_ALIGN(area_sz, page_sz);
}

static int
free_memseg_list(struct rte_memseg_list *msl)
{
	if (rte_fbarray_destroy(&msl->memseg_arr)) {
		RTE_LOG(ERR, EAL, "Cannot destroy memseg list\n");
		return -1;
	}
	memset(msl, 0, sizeof(*msl));
	return 0;
}

static int
alloc_memseg_list(struct rte_memseg_list *msl, uint64_t page_sz,
		uint64_t max_mem, int socket_id, int type_msl_idx)
{
	char name[RTE_FBARRAY_NAME_LEN];
	uint64_t mem_amount;
	int max_segs;

	mem_amount = get_mem_amount(page_sz, max_mem);
	max_segs = mem_amount / page_sz;

	snprintf(name, sizeof(name), MEMSEG_LIST_FMT, page_sz >> 10, socket_id,
		 type_msl_idx);
	if (rte_fbarray_init(&msl->memseg_arr, name, max_segs,
			sizeof(struct rte_memseg))) {
		RTE_LOG(ERR, EAL, "Cannot allocate memseg list: %s\n",
			rte_strerror(rte_errno));
		return -1;
	}

	msl->page_sz = page_sz;
	msl->socket_id = socket_id;
	msl->base_va = NULL;

	RTE_LOG(DEBUG, EAL, "Memseg list allocated: 0x%zxkB at socket %i\n",
			(size_t)page_sz >> 10, socket_id);

	return 0;
}

static int
alloc_va_space(struct rte_memseg_list *msl)
{
	uint64_t page_sz;
	size_t mem_sz;
	void *addr;
	int flags = 0;

#ifdef RTE_ARCH_PPC_64
	flags |= MAP_HUGETLB;
#endif

	page_sz = msl->page_sz;
	mem_sz = page_sz * msl->memseg_arr.len;

	addr = eal_get_virtual_area(msl->base_va, &mem_sz, page_sz, 0, flags);
	if (addr == NULL) {
		if (rte_errno == EADDRNOTAVAIL)
			RTE_LOG(ERR, EAL, "Could not mmap %llu bytes at [%p] - please use '--base-virtaddr' option\n",
				(unsigned long long)mem_sz, msl->base_va);
		else
			RTE_LOG(ERR, EAL, "Cannot reserve memory\n");
		return -1;
	}
	msl->base_va = addr;

	return 0;
}

static int __rte_unused
memseg_primary_init_32(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int active_sockets, hpi_idx, msl_idx = 0;
	unsigned int socket_id, i;
	struct rte_memseg_list *msl;
	uint64_t extra_mem_per_socket, total_extra_mem, total_requested_mem;
	uint64_t max_mem;

	/* no-huge does not need this at all */
	if (internal_config.no_hugetlbfs)
		return 0;

	/* this is a giant hack, but desperate times call for desperate
	 * measures. in legacy 32-bit mode, we cannot preallocate VA space,
	 * because having upwards of 2 gigabytes of VA space already mapped will
	 * interfere with our ability to map and sort hugepages.
	 *
	 * therefore, in legacy 32-bit mode, we will be initializing memseg
	 * lists much later - in eal_memory.c, right after we unmap all the
	 * unneeded pages. this will not affect secondary processes, as those
	 * should be able to mmap the space without (too many) problems.
	 */
	if (internal_config.legacy_mem)
		return 0;

	/* 32-bit mode is a very special case. we cannot know in advance where
	 * the user will want to allocate their memory, so we have to do some
	 * heuristics.
	 */
	active_sockets = 0;
	total_requested_mem = 0;
	if (internal_config.force_sockets)
		for (i = 0; i < rte_socket_count(); i++) {
			uint64_t mem;

			socket_id = rte_socket_id_by_idx(i);
			mem = internal_config.socket_mem[socket_id];

			if (mem == 0)
				continue;

			active_sockets++;
			total_requested_mem += mem;
		}
	else
		total_requested_mem = internal_config.memory;

	max_mem = (uint64_t)RTE_MAX_MEM_MB << 20;
	if (total_requested_mem > max_mem) {
		RTE_LOG(ERR, EAL, "Invalid parameters: 32-bit process can at most use %uM of memory\n",
				(unsigned int)(max_mem >> 20));
		return -1;
	}
	total_extra_mem = max_mem - total_requested_mem;
	extra_mem_per_socket = active_sockets == 0 ? total_extra_mem :
			total_extra_mem / active_sockets;

	/* the allocation logic is a little bit convoluted, but here's how it
	 * works, in a nutshell:
	 *  - if user hasn't specified on which sockets to allocate memory via
	 *    --socket-mem, we allocate all of our memory on master core socket.
	 *  - if user has specified sockets to allocate memory on, there may be
	 *    some "unused" memory left (e.g. if user has specified --socket-mem
	 *    such that not all memory adds up to 2 gigabytes), so add it to all
	 *    sockets that are in use equally.
	 *
	 * page sizes are sorted by size in descending order, so we can safely
	 * assume that we dispense with bigger page sizes first.
	 */

	/* create memseg lists */
	for (i = 0; i < rte_socket_count(); i++) {
		int hp_sizes = (int) internal_config.num_hugepage_sizes;
		uint64_t max_socket_mem, cur_socket_mem;
		unsigned int master_lcore_socket;
		struct rte_config *cfg = rte_eal_get_configuration();
		bool skip;

		socket_id = rte_socket_id_by_idx(i);

#ifndef RTE_EAL_NUMA_AWARE_HUGEPAGES
		if (socket_id > 0)
			break;
#endif

		/* if we didn't specifically request memory on this socket */
		skip = active_sockets != 0 &&
				internal_config.socket_mem[socket_id] == 0;
		/* ...or if we didn't specifically request memory on *any*
		 * socket, and this is not master lcore
		 */
		master_lcore_socket = rte_lcore_to_socket_id(cfg->master_lcore);
		skip |= active_sockets == 0 && socket_id != master_lcore_socket;

		if (skip) {
			RTE_LOG(DEBUG, EAL, "Will not preallocate memory on socket %u\n",
					socket_id);
			continue;
		}

		/* max amount of memory on this socket */
		max_socket_mem = (active_sockets != 0 ?
					internal_config.socket_mem[socket_id] :
					internal_config.memory) +
					extra_mem_per_socket;
		cur_socket_mem = 0;

		for (hpi_idx = 0; hpi_idx < hp_sizes; hpi_idx++) {
			uint64_t max_pagesz_mem, cur_pagesz_mem = 0;
			uint64_t hugepage_sz;
			struct hugepage_info *hpi;
			int type_msl_idx, max_segs, total_segs = 0;

			hpi = &internal_config.hugepage_info[hpi_idx];
			hugepage_sz = hpi->hugepage_sz;

			/* check if pages are actually available */
			if (hpi->num_pages[socket_id] == 0)
				continue;

			max_segs = RTE_MAX_MEMSEG_PER_TYPE;
			max_pagesz_mem = max_socket_mem - cur_socket_mem;

			/* make it multiple of page size */
			max_pagesz_mem = RTE_ALIGN_FLOOR(max_pagesz_mem,
					hugepage_sz);

			RTE_LOG(DEBUG, EAL, "Attempting to preallocate "
					"%" PRIu64 "M on socket %i\n",
					max_pagesz_mem >> 20, socket_id);

			type_msl_idx = 0;
			while (cur_pagesz_mem < max_pagesz_mem &&
					total_segs < max_segs) {
				if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
					RTE_LOG(ERR, EAL,
						"No more space in memseg lists, please increase %s\n",
						RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
					return -1;
				}

				msl = &mcfg->memsegs[msl_idx];

				if (alloc_memseg_list(msl, hugepage_sz,
						max_pagesz_mem, socket_id,
						type_msl_idx)) {
					/* failing to allocate a memseg list is
					 * a serious error.
					 */
					RTE_LOG(ERR, EAL, "Cannot allocate memseg list\n");
					return -1;
				}

				if (alloc_va_space(msl)) {
					/* if we couldn't allocate VA space, we
					 * can try with smaller page sizes.
					 */
					RTE_LOG(ERR, EAL, "Cannot allocate VA space for memseg list, retrying with different page size\n");
					/* deallocate memseg list */
					if (free_memseg_list(msl))
						return -1;
					break;
				}

				total_segs += msl->memseg_arr.len;
				cur_pagesz_mem = total_segs * hugepage_sz;
				type_msl_idx++;
				msl_idx++;
			}
			cur_socket_mem += cur_pagesz_mem;
		}
		if (cur_socket_mem == 0) {
			RTE_LOG(ERR, EAL, "Cannot allocate VA space on socket %u\n",
				socket_id);
			return -1;
		}
	}

	return 0;
}

static int __rte_unused
memseg_primary_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, socket_id, hpi_idx, msl_idx = 0;
	struct rte_memseg_list *msl;
	uint64_t max_mem, total_mem;

	/* no-huge does not need this at all */
	if (internal_config.no_hugetlbfs)
		return 0;

	max_mem = (uint64_t)RTE_MAX_MEM_MB << 20;
	total_mem = 0;

	/* create memseg lists */
	for (hpi_idx = 0; hpi_idx < (int) internal_config.num_hugepage_sizes;
			hpi_idx++) {
		struct hugepage_info *hpi;
		uint64_t hugepage_sz;

		hpi = &internal_config.hugepage_info[hpi_idx];
		hugepage_sz = hpi->hugepage_sz;

		for (i = 0; i < (int) rte_socket_count(); i++) {
			uint64_t max_type_mem, total_type_mem = 0;
			int type_msl_idx, max_segs, total_segs = 0;

			socket_id = rte_socket_id_by_idx(i);

#ifndef RTE_EAL_NUMA_AWARE_HUGEPAGES
			if (socket_id > 0)
				break;
#endif

			if (total_mem >= max_mem)
				break;

			max_type_mem = RTE_MIN(max_mem - total_mem,
				(uint64_t)RTE_MAX_MEM_MB_PER_TYPE << 20);
			max_segs = RTE_MAX_MEMSEG_PER_TYPE;

			type_msl_idx = 0;
			while (total_type_mem < max_type_mem &&
					total_segs < max_segs) {
				uint64_t cur_max_mem;
				if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
					RTE_LOG(ERR, EAL,
						"No more space in memseg lists, please increase %s\n",
						RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
					return -1;
				}

				msl = &mcfg->memsegs[msl_idx++];

				cur_max_mem = max_type_mem - total_type_mem;
				if (alloc_memseg_list(msl, hugepage_sz,
						cur_max_mem, socket_id,
						type_msl_idx))
					return -1;

				total_segs += msl->memseg_arr.len;
				total_type_mem = total_segs * hugepage_sz;
				type_msl_idx++;

				if (alloc_va_space(msl)) {
					RTE_LOG(ERR, EAL, "Cannot allocate VA space for memseg list\n");
					return -1;
				}
			}
			total_mem += total_type_mem;
		}
	}
	return 0;
}

static int
memseg_secondary_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int msl_idx = 0;
	struct rte_memseg_list *msl;

	for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS; msl_idx++) {

		msl = &mcfg->memsegs[msl_idx];

		/* skip empty memseg lists */
		if (msl->memseg_arr.len == 0)
			continue;

		if (rte_fbarray_attach(&msl->memseg_arr)) {
			RTE_LOG(ERR, EAL, "Cannot attach to primary process memseg lists\n");
			return -1;
		}

		/* preallocate VA space */
		if (alloc_va_space(msl)) {
			RTE_LOG(ERR, EAL, "Cannot preallocate VA space for hugepage memory\n");
			return -1;
		}
	}

	return 0;
}

static struct rte_memseg *
virt2memseg(const void *addr, const struct rte_memseg_list *msl)
{
	const struct rte_fbarray *arr;
	void *start, *end;
	int ms_idx;

	/* a memseg list was specified, check if it's the right one */
	start = msl->base_va;
	end = RTE_PTR_ADD(start, (size_t)msl->page_sz * msl->memseg_arr.len);

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
		end = RTE_PTR_ADD(start,
				(size_t)msl->page_sz * msl->memseg_arr.len);
		if (addr >= start && addr < end)
			break;
	}
	/* if we didn't find our memseg list */
	if (msl_idx == RTE_MAX_MEMSEG_LISTS)
		return NULL;
	return msl;
}

__rte_experimental struct rte_memseg_list *
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

__rte_experimental void *
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

__rte_experimental struct rte_memseg *
rte_mem_virt2memseg(const void *addr, const struct rte_memseg_list *msl)
{
	return virt2memseg(addr, msl != NULL ? msl :
			rte_mem_virt2memseg_list(addr));
}

static int
physmem_size(const struct rte_memseg_list *msl, void *arg)
{
	uint64_t *total_len = arg;

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
	int msl_idx, ms_idx;
	FILE *f = arg;

	msl_idx = msl - mcfg->memsegs;
	if (msl_idx < 0 || msl_idx >= RTE_MAX_MEMSEG_LISTS)
		return -1;

	ms_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
	if (ms_idx < 0)
		return -1;

	fprintf(f, "Segment %i-%i: IOVA:0x%"PRIx64", len:%zu, "
			"virt:%p, socket_id:%"PRId32", "
			"hugepage_sz:%"PRIu64", nchannel:%"PRIx32", "
			"nrank:%"PRIx32"\n",
			msl_idx, ms_idx,
			ms->iova,
			ms->len,
			ms->addr,
			ms->socket_id,
			ms->hugepage_sz,
			ms->nchannel,
			ms->nrank);

	return 0;
}

/*
 * Defining here because declared in rte_memory.h, but the actual implementation
 * is in eal_common_memalloc.c, like all other memalloc internals.
 */
int __rte_experimental
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

int __rte_experimental
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

int __rte_experimental
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

int __rte_experimental
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

int __rte_experimental
rte_memseg_contig_walk(rte_memseg_contig_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ms_idx, ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);

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
			if (ret < 0) {
				ret = -1;
				goto out;
			} else if (ret > 0) {
				ret = 1;
				goto out;
			}
			ms_idx = rte_fbarray_find_next_used(arr,
					ms_idx + n_segs);
		}
	}
out:
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return ret;
}

int __rte_experimental
rte_memseg_walk(rte_memseg_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ms_idx, ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);

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
			if (ret < 0) {
				ret = -1;
				goto out;
			} else if (ret > 0) {
				ret = 1;
				goto out;
			}
			ms_idx = rte_fbarray_find_next_used(arr, ms_idx + 1);
		}
	}
out:
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return ret;
}

int __rte_experimental
rte_memseg_list_walk(rte_memseg_list_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ret = 0;

	/* do not allow allocations/frees/init while we iterate */
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];

		if (msl->base_va == NULL)
			continue;

		ret = func(msl, arg);
		if (ret < 0) {
			ret = -1;
			goto out;
		}
		if (ret > 0) {
			ret = 1;
			goto out;
		}
	}
out:
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return ret;
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
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);

	retval = rte_eal_process_type() == RTE_PROC_PRIMARY ?
#ifndef RTE_ARCH_64
			memseg_primary_init_32() :
#else
			memseg_primary_init() :
#endif
			memseg_secondary_init();

	if (retval < 0)
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
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return -1;
}
