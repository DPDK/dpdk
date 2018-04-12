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
#include <rte_eal_memconfig.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"

#define EAL_PAGE_SIZE (sysconf(_SC_PAGESIZE))

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
		msl->socket_id = 0;

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

			for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS;
					msl_idx++) {
				bool empty;
				msl = &mcfg->memsegs[msl_idx];
				arr = &msl->memseg_arr;

				if (msl->page_sz != page_sz)
					continue;

				empty = arr->count == 0;

				/* we need 1, plus hole if not empty */
				ms_idx = rte_fbarray_find_next_n_free(arr,
						0, 1 + (empty ? 1 : 0));

				/* memseg list is full? */
				if (ms_idx < 0)
					continue;

				/* leave some space between memsegs, they are
				 * not IOVA contiguous, so they shouldn't be VA
				 * contiguous either.
				 */
				if (!empty)
					ms_idx++;

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

			snprintf(physaddr_str, sizeof(physaddr_str), "hw.contigmem"
					".physaddr.%d", j);
			error = sysctlbyname(physaddr_str, &physaddr, &sysctl_size,
					NULL, 0);
			if (error < 0) {
				RTE_LOG(ERR, EAL, "Failed to get physical addr for buffer %u "
						"from %s\n", j, hpi->hugedir);
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
					seg_idx, addr, physaddr, page_sz);

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
attach_segment(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	struct attach_walk_args *wa = arg;
	void *addr;

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
