/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_memcfg.h"
#include "eal_options.h"

#define EAL_PAGE_SIZE (sysconf(_SC_PAGESIZE))

uint64_t eal_get_baseaddr(void)
{
	/*
	 * FreeBSD may allocate something in the space we will be mapping things
	 * before we get a chance to do that, so use a base address that's far
	 * away from where malloc() et al usually map things.
	 */
	return 0x1000000000ULL;
}

/*
 * Get physical address of any mapped virtual address in the current process.
 */
phys_addr_t
rte_mem_virt2phy(const void *virtaddr)
{
	/* XXX not implemented. This function is only used by
	 * rte_mempool_virt2iova() when hugepages are disabled. */
	(void)virtaddr;
	return RTE_BAD_IOVA;
}
rte_iova_t
rte_mem_virt2iova(const void *virtaddr)
{
	return rte_mem_virt2phy(virtaddr);
}

int
rte_eal_hugepage_init(void)
{
	struct rte_mem_config *mcfg;
	uint64_t total_mem = 0;
	void *addr;
	unsigned int i, j, seg_idx = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* for debug purposes, hugetlbfs can be disabled */
	if (internal_config.no_hugetlbfs) {
		struct rte_memseg_list *msl;
		struct rte_fbarray *arr;
		struct rte_memseg *ms;
		uint64_t page_sz;
		int n_segs, cur_seg;

		/* create a memseg list */
		msl = &mcfg->memsegs[0];

		page_sz = RTE_PGSIZE_4K;
		n_segs = internal_config.memory / page_sz;

		if (rte_fbarray_init(&msl->memseg_arr, "nohugemem", n_segs,
				sizeof(struct rte_memseg))) {
			RTE_LOG(ERR, EAL, "Cannot allocate memseg list\n");
			return -1;
		}

		addr = mmap(NULL, internal_config.memory,
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "%s: mmap() failed: %s\n", __func__,
					strerror(errno));
			return -1;
		}
		msl->base_va = addr;
		msl->page_sz = page_sz;
		msl->len = internal_config.memory;
		msl->socket_id = 0;
		msl->heap = 1;

		/* populate memsegs. each memseg is 1 page long */
		for (cur_seg = 0; cur_seg < n_segs; cur_seg++) {
			arr = &msl->memseg_arr;

			ms = rte_fbarray_get(arr, cur_seg);
			if (rte_eal_iova_mode() == RTE_IOVA_VA)
				ms->iova = (uintptr_t)addr;
			else
				ms->iova = RTE_BAD_IOVA;
			ms->addr = addr;
			ms->hugepage_sz = page_sz;
			ms->len = page_sz;
			ms->socket_id = 0;

			rte_fbarray_set_used(arr, cur_seg);

			addr = RTE_PTR_ADD(addr, page_sz);
		}
		return 0;
	}

	/* map all hugepages and sort them */
	for (i = 0; i < internal_config.num_hugepage_sizes; i ++){
		struct hugepage_info *hpi;
		rte_iova_t prev_end = 0;
		int prev_ms_idx = -1;
		uint64_t page_sz, mem_needed;
		unsigned int n_pages, max_pages;

		hpi = &internal_config.hugepage_info[i];
		page_sz = hpi->hugepage_sz;
		max_pages = hpi->num_pages[0];
		mem_needed = RTE_ALIGN_CEIL(internal_config.memory - total_mem,
				page_sz);

		n_pages = RTE_MIN(mem_needed / page_sz, max_pages);

		for (j = 0; j < n_pages; j++) {
			struct rte_memseg_list *msl;
			struct rte_fbarray *arr;
			struct rte_memseg *seg;
			int msl_idx, ms_idx;
			rte_iova_t physaddr;
			int error;
			size_t sysctl_size = sizeof(physaddr);
			char physaddr_str[64];
			bool is_adjacent;

			/* first, check if this segment is IOVA-adjacent to
			 * the previous one.
			 */
			snprintf(physaddr_str, sizeof(physaddr_str),
					"hw.contigmem.physaddr.%d", j);
			error = sysctlbyname(physaddr_str, &physaddr,
					&sysctl_size, NULL, 0);
			if (error < 0) {
				RTE_LOG(ERR, EAL, "Failed to get physical addr for buffer %u "
						"from %s\n", j, hpi->hugedir);
				return -1;
			}

			is_adjacent = prev_end != 0 && physaddr == prev_end;
			prev_end = physaddr + hpi->hugepage_sz;

			for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS;
					msl_idx++) {
				bool empty, need_hole;
				msl = &mcfg->memsegs[msl_idx];
				arr = &msl->memseg_arr;

				if (msl->page_sz != page_sz)
					continue;

				empty = arr->count == 0;

				/* we need a hole if this isn't an empty memseg
				 * list, and if previous segment was not
				 * adjacent to current one.
				 */
				need_hole = !empty && !is_adjacent;

				/* we need 1, plus hole if not adjacent */
				ms_idx = rte_fbarray_find_next_n_free(arr,
						0, 1 + (need_hole ? 1 : 0));

				/* memseg list is full? */
				if (ms_idx < 0)
					continue;

				if (need_hole && prev_ms_idx == ms_idx - 1)
					ms_idx++;
				prev_ms_idx = ms_idx;

				break;
			}
			if (msl_idx == RTE_MAX_MEMSEG_LISTS) {
				RTE_LOG(ERR, EAL, "Could not find space for memseg. Please increase %s and/or %s in configuration.\n",
					RTE_STR(CONFIG_RTE_MAX_MEMSEG_PER_TYPE),
					RTE_STR(CONFIG_RTE_MAX_MEM_PER_TYPE));
				return -1;
			}
			arr = &msl->memseg_arr;
			seg = rte_fbarray_get(arr, ms_idx);

			addr = RTE_PTR_ADD(msl->base_va,
					(size_t)msl->page_sz * ms_idx);

			/* address is already mapped in memseg list, so using
			 * MAP_FIXED here is safe.
			 */
			addr = mmap(addr, page_sz, PROT_READ|PROT_WRITE,
					MAP_SHARED | MAP_FIXED,
					hpi->lock_descriptor,
					j * EAL_PAGE_SIZE);
			if (addr == MAP_FAILED) {
				RTE_LOG(ERR, EAL, "Failed to mmap buffer %u from %s\n",
						j, hpi->hugedir);
				return -1;
			}

			seg->addr = addr;
			seg->iova = physaddr;
			seg->hugepage_sz = page_sz;
			seg->len = page_sz;
			seg->nchannel = mcfg->nchannel;
			seg->nrank = mcfg->nrank;
			seg->socket_id = 0;

			rte_fbarray_set_used(arr, ms_idx);

			RTE_LOG(INFO, EAL, "Mapped memory segment %u @ %p: physaddr:0x%"
					PRIx64", len %zu\n",
					seg_idx++, addr, physaddr, page_sz);

			total_mem += seg->len;
		}
		if (total_mem >= internal_config.memory)
			break;
	}
	if (total_mem < internal_config.memory) {
		RTE_LOG(ERR, EAL, "Couldn't reserve requested memory, "
				"requested: %" PRIu64 "M "
				"available: %" PRIu64 "M\n",
				internal_config.memory >> 20, total_mem >> 20);
		return -1;
	}
	return 0;
}

struct attach_walk_args {
	int fd_hugepage;
	int seg_idx;
};
static int
attach_segment(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg)
{
	struct attach_walk_args *wa = arg;
	void *addr;

	if (msl->external)
		return 0;

	addr = mmap(ms->addr, ms->len, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, wa->fd_hugepage,
			wa->seg_idx * EAL_PAGE_SIZE);
	if (addr == MAP_FAILED || addr != ms->addr)
		return -1;
	wa->seg_idx++;

	return 0;
}

int
rte_eal_hugepage_attach(void)
{
	const struct hugepage_info *hpi;
	int fd_hugepage = -1;
	unsigned int i;

	hpi = &internal_config.hugepage_info[0];

	for (i = 0; i < internal_config.num_hugepage_sizes; i++) {
		const struct hugepage_info *cur_hpi = &hpi[i];
		struct attach_walk_args wa;

		memset(&wa, 0, sizeof(wa));

		/* Obtain a file descriptor for contiguous memory */
		fd_hugepage = open(cur_hpi->hugedir, O_RDWR);
		if (fd_hugepage < 0) {
			RTE_LOG(ERR, EAL, "Could not open %s\n",
					cur_hpi->hugedir);
			goto error;
		}
		wa.fd_hugepage = fd_hugepage;
		wa.seg_idx = 0;

		/* Map the contiguous memory into each memory segment */
		if (rte_memseg_walk(attach_segment, &wa) < 0) {
			RTE_LOG(ERR, EAL, "Failed to mmap buffer %u from %s\n",
				wa.seg_idx, cur_hpi->hugedir);
			goto error;
		}

		close(fd_hugepage);
		fd_hugepage = -1;
	}

	/* hugepage_info is no longer required */
	return 0;

error:
	if (fd_hugepage >= 0)
		close(fd_hugepage);
	return -1;
}

int
rte_eal_using_phys_addrs(void)
{
	return 0;
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

#define MEMSEG_LIST_FMT "memseg-%" PRIu64 "k-%i-%i"
static int
alloc_memseg_list(struct rte_memseg_list *msl, uint64_t page_sz,
		int n_segs, int socket_id, int type_msl_idx)
{
	char name[RTE_FBARRAY_NAME_LEN];

	snprintf(name, sizeof(name), MEMSEG_LIST_FMT, page_sz >> 10, socket_id,
		 type_msl_idx);
	if (rte_fbarray_init(&msl->memseg_arr, name, n_segs,
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
			RTE_LOG(ERR, EAL, "Could not mmap %llu bytes at [%p] - "
				"please use '--" OPT_BASE_VIRTADDR "' option\n",
				(unsigned long long)mem_sz, msl->base_va);
		else
			RTE_LOG(ERR, EAL, "Cannot reserve memory\n");
		return -1;
	}
	msl->base_va = addr;
	msl->len = mem_sz;

	return 0;
}


static int
memseg_primary_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int hpi_idx, msl_idx = 0;
	struct rte_memseg_list *msl;
	uint64_t max_mem, total_mem;

	/* no-huge does not need this at all */
	if (internal_config.no_hugetlbfs)
		return 0;

	/* FreeBSD has an issue where core dump will dump the entire memory
	 * contents, including anonymous zero-page memory. Therefore, while we
	 * will be limiting total amount of memory to RTE_MAX_MEM_MB, we will
	 * also be further limiting total memory amount to whatever memory is
	 * available to us through contigmem driver (plus spacing blocks).
	 *
	 * so, at each stage, we will be checking how much memory we are
	 * preallocating, and adjust all the values accordingly.
	 */

	max_mem = (uint64_t)RTE_MAX_MEM_MB << 20;
	total_mem = 0;

	/* create memseg lists */
	for (hpi_idx = 0; hpi_idx < (int) internal_config.num_hugepage_sizes;
			hpi_idx++) {
		uint64_t max_type_mem, total_type_mem = 0;
		uint64_t avail_mem;
		int type_msl_idx, max_segs, avail_segs, total_segs = 0;
		struct hugepage_info *hpi;
		uint64_t hugepage_sz;

		hpi = &internal_config.hugepage_info[hpi_idx];
		hugepage_sz = hpi->hugepage_sz;

		/* no NUMA support on FreeBSD */

		/* check if we've already exceeded total memory amount */
		if (total_mem >= max_mem)
			break;

		/* first, calculate theoretical limits according to config */
		max_type_mem = RTE_MIN(max_mem - total_mem,
			(uint64_t)RTE_MAX_MEM_MB_PER_TYPE << 20);
		max_segs = RTE_MAX_MEMSEG_PER_TYPE;

		/* now, limit all of that to whatever will actually be
		 * available to us, because without dynamic allocation support,
		 * all of that extra memory will be sitting there being useless
		 * and slowing down core dumps in case of a crash.
		 *
		 * we need (N*2)-1 segments because we cannot guarantee that
		 * each segment will be IOVA-contiguous with the previous one,
		 * so we will allocate more and put spaces between segments
		 * that are non-contiguous.
		 */
		avail_segs = (hpi->num_pages[0] * 2) - 1;
		avail_mem = avail_segs * hugepage_sz;

		max_type_mem = RTE_MIN(avail_mem, max_type_mem);
		max_segs = RTE_MIN(avail_segs, max_segs);

		type_msl_idx = 0;
		while (total_type_mem < max_type_mem &&
				total_segs < max_segs) {
			uint64_t cur_max_mem, cur_mem;
			unsigned int n_segs;

			if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
				RTE_LOG(ERR, EAL,
					"No more space in memseg lists, please increase %s\n",
					RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
				return -1;
			}

			msl = &mcfg->memsegs[msl_idx++];

			cur_max_mem = max_type_mem - total_type_mem;

			cur_mem = get_mem_amount(hugepage_sz,
					cur_max_mem);
			n_segs = cur_mem / hugepage_sz;

			if (alloc_memseg_list(msl, hugepage_sz, n_segs,
					0, type_msl_idx))
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

int
rte_eal_memseg_init(void)
{
	return rte_eal_process_type() == RTE_PROC_PRIMARY ?
			memseg_primary_init() :
			memseg_secondary_init();
}
