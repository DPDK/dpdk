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
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include <eal_export.h>
#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_memcfg.h"
#include "eal_options.h"

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
RTE_EXPORT_SYMBOL(rte_mem_virt2phy)
phys_addr_t
rte_mem_virt2phy(const void *virtaddr)
{
	/* not implemented for FreeBSD when not using contigmem memory */
	if (virtaddr == NULL || rte_eal_iova_mode() != RTE_IOVA_PA)
		return RTE_BAD_IOVA;
	/* when IOVA == PA, return the IOVA */
	return rte_mem_virt2iova(virtaddr);
}

RTE_EXPORT_SYMBOL(rte_mem_virt2iova)
rte_iova_t
rte_mem_virt2iova(const void *virtaddr)
{
	if (virtaddr == NULL)
		return RTE_BAD_IOVA;

	if (rte_eal_iova_mode() == RTE_IOVA_VA)
		return (uintptr_t)virtaddr;

	const struct rte_memseg *ms = rte_mem_virt2memseg(virtaddr, NULL);
	if (ms != NULL && ms->iova != RTE_BAD_IOVA)
		return ms->iova + RTE_PTR_DIFF(virtaddr, ms->addr);

	return RTE_BAD_IOVA;
}

int
rte_eal_hugepage_init(void)
{
	struct rte_mem_config *mcfg;
	uint64_t total_mem = 0;
	void *addr;
	unsigned int i, j, seg_idx = 0;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* for debug purposes, hugetlbfs can be disabled */
	if (internal_conf->no_hugetlbfs) {
		struct rte_memseg_list *msl;
		uint64_t mem_sz, page_sz;
		int n_segs;

		/* create a memseg list */
		msl = &mcfg->memsegs[0];

		mem_sz = internal_conf->memory;
		page_sz = RTE_PGSIZE_4K;
		n_segs = mem_sz / page_sz;

		if (eal_memseg_list_init_named(
				msl, "nohugemem", page_sz, n_segs, 0, true)) {
			return -1;
		}

		addr = mmap(NULL, mem_sz, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (addr == MAP_FAILED) {
			EAL_LOG(ERR, "%s: mmap() failed: %s", __func__,
					strerror(errno));
			return -1;
		}

		msl->base_va = addr;
		msl->len = mem_sz;

		eal_memseg_list_populate(msl, addr, n_segs);

		return 0;
	}

	/* map all hugepages and sort them */
	for (i = 0; i < internal_conf->num_hugepage_sizes; i++) {
		struct hugepage_info *hpi;
		rte_iova_t prev_end = 0;
		uint64_t page_sz, mem_needed;
		unsigned int n_pages, max_pages;

		hpi = &internal_conf->hugepage_info[i];
		page_sz = hpi->hugepage_sz;
		max_pages = hpi->num_pages[0];
		mem_needed = RTE_ALIGN_CEIL(internal_conf->memory - total_mem,
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
				EAL_LOG(ERR, "Failed to get physical addr for buffer %u "
						"from %s", j, hpi->hugedir);
				return -1;
			}

			is_adjacent = prev_end != 0 && physaddr == prev_end;
			prev_end = physaddr + hpi->hugepage_sz;

			for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS;
					msl_idx++) {
				int start_idx, num_elems;
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
				if (need_hole) {
					start_idx = 0;
					/* we need 1, plus hole */
					num_elems = 2;
				} else {
					/* begin our search after the last used
					 * element in the list, skipping over
					 * any previously placed holes
					 */
					start_idx =
						rte_fbarray_find_prev_n_used(arr, arr->len - 1, 1)
						+ 1;
					num_elems = 1;
				}

				ms_idx = rte_fbarray_find_next_n_free(arr,
						start_idx, num_elems);

				/* memseg list is full? */
				if (ms_idx < 0)
					continue;

				/* ms_idx should never be 0 if we need a hole,
				 * but include a check for static analysis.
				 */
				if (need_hole && ms_idx > 0 &&
						rte_fbarray_is_used(arr, ms_idx - 1))
					ms_idx++;

				break;
			}
			if (msl_idx == RTE_MAX_MEMSEG_LISTS) {
				EAL_LOG(ERR,
					"Could not find suitable space for memseg in existing memseg lists");
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
					j * rte_mem_page_size());
			if (addr == MAP_FAILED) {
				EAL_LOG(ERR, "Failed to mmap buffer %u from %s",
						j, hpi->hugedir);
				return -1;
			}

			seg->addr = addr;
			if (rte_eal_iova_mode() == RTE_IOVA_VA)
				seg->iova = (uintptr_t)addr;
			else
				seg->iova = physaddr;
			seg->hugepage_sz = page_sz;
			seg->len = page_sz;
			seg->nchannel = mcfg->nchannel;
			seg->nrank = mcfg->nrank;
			seg->socket_id = 0;

			rte_fbarray_set_used(arr, ms_idx);

			EAL_LOG(INFO, "Mapped memory segment %u @ %p: physaddr:0x%"
					PRIx64", len %zu",
					seg_idx++, addr, physaddr, page_sz);

			total_mem += seg->len;
		}
		if (total_mem >= internal_conf->memory)
			break;
	}
	if (total_mem < internal_conf->memory) {
		EAL_LOG(ERR, "Couldn't reserve requested memory, "
				"requested: %" PRIu64 "M "
				"available: %" PRIu64 "M",
				internal_conf->memory >> 20, total_mem >> 20);
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
			wa->seg_idx * rte_mem_page_size());
	if (addr == MAP_FAILED || addr != ms->addr)
		return -1;
	wa->seg_idx++;

	return 0;
}

int
rte_eal_hugepage_attach(void)
{
	struct hugepage_info *hpi;
	int fd_hugepage = -1;
	unsigned int i;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	hpi = &internal_conf->hugepage_info[0];

	for (i = 0; i < internal_conf->num_hugepage_sizes; i++) {
		const struct hugepage_info *cur_hpi = &hpi[i];
		struct attach_walk_args wa;

		memset(&wa, 0, sizeof(wa));

		/* Obtain a file descriptor for contiguous memory */
		fd_hugepage = open(cur_hpi->hugedir, O_RDWR);
		if (fd_hugepage < 0) {
			EAL_LOG(ERR, "Could not open %s",
					cur_hpi->hugedir);
			goto error;
		}
		wa.fd_hugepage = fd_hugepage;
		wa.seg_idx = 0;

		/* Map the contiguous memory into each memory segment */
		if (rte_memseg_walk(attach_segment, &wa) < 0) {
			EAL_LOG(ERR, "Failed to mmap buffer %u from %s",
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

RTE_EXPORT_SYMBOL(rte_eal_using_phys_addrs)
int
rte_eal_using_phys_addrs(void)
{
	return 0;
}

static int
memseg_list_alloc(struct rte_memseg_list *msl)
{
	int flags = 0;

#ifdef RTE_ARCH_PPC_64
	flags |= EAL_RESERVE_HUGEPAGES;
#endif
	return eal_memseg_list_alloc(msl, flags);
}

static int
memseg_primary_init(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int hpi_idx, msl_idx = 0;
	struct rte_memseg_list *msl;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* no-huge does not need this at all */
	if (internal_conf->no_hugetlbfs)
		return 0;

	/* FreeBSD has an issue where core dump will dump the entire memory
	 * contents, including anonymous zero-page memory. To avoid reserving VA
	 * space we are not going to use, size memseg lists according to
	 * contigmem-provided page counts.
	 */

	/* create memseg lists */
	for (hpi_idx = 0; hpi_idx < (int) internal_conf->num_hugepage_sizes;
			hpi_idx++) {
		struct hugepage_info *hpi;
		uint64_t hugepage_sz;
		unsigned int n_segs;

		hpi = &internal_conf->hugepage_info[hpi_idx];
		hugepage_sz = hpi->hugepage_sz;

		/* no NUMA support on FreeBSD */

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
		n_segs = (hpi->num_pages[0] * 2) - 1;
		if (n_segs == 0)
			continue;

		if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
			EAL_LOG(ERR, "No more space in memseg lists, please increase RTE_MAX_MEMSEG_LISTS");
			return -1;
		}

		msl = &mcfg->memsegs[msl_idx];

		if (eal_memseg_list_init(msl, hugepage_sz, n_segs, 0, msl_idx, false))
			return -1;

		if (memseg_list_alloc(msl)) {
			EAL_LOG(ERR, "Cannot allocate VA space for memseg list");
			return -1;
		}
		msl_idx++;
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

		/* skip empty and external memseg lists */
		if (msl->memseg_arr.len == 0 || msl->external)
			continue;

		if (rte_fbarray_attach(&msl->memseg_arr)) {
			EAL_LOG(ERR, "Cannot attach to primary process memseg lists");
			return -1;
		}

		/* preallocate VA space */
		if (memseg_list_alloc(msl)) {
			EAL_LOG(ERR, "Cannot preallocate VA space for hugepage memory");
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
