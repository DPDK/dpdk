/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <inttypes.h>
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
	unsigned i, j, seg_idx = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* for debug purposes, hugetlbfs can be disabled */
	if (internal_config.no_hugetlbfs) {
		addr = malloc(internal_config.memory);
		mcfg->memseg[0].iova = (rte_iova_t)(uintptr_t)addr;
		mcfg->memseg[0].addr = addr;
		mcfg->memseg[0].hugepage_sz = RTE_PGSIZE_4K;
		mcfg->memseg[0].len = internal_config.memory;
		mcfg->memseg[0].socket_id = 0;
		return 0;
	}

	/* map all hugepages and sort them */
	for (i = 0; i < internal_config.num_hugepage_sizes; i ++){
		struct hugepage_info *hpi;

		hpi = &internal_config.hugepage_info[i];
		for (j = 0; j < hpi->num_pages[0]; j++) {
			struct rte_memseg *seg;
			rte_iova_t physaddr;
			int error;
			size_t sysctl_size = sizeof(physaddr);
			char physaddr_str[64];

			addr = mmap(NULL, hpi->hugepage_sz, PROT_READ|PROT_WRITE,
				    MAP_SHARED, hpi->lock_descriptor,
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

			seg = &mcfg->memseg[seg_idx++];
			seg->addr = addr;
			seg->iova = physaddr;
			seg->hugepage_sz = hpi->hugepage_sz;
			seg->len = hpi->hugepage_sz;
			seg->nchannel = mcfg->nchannel;
			seg->nrank = mcfg->nrank;
			seg->socket_id = 0;

			RTE_LOG(INFO, EAL, "Mapped memory segment %u @ %p: physaddr:0x%"
					PRIx64", len %zu\n",
					seg_idx, addr, physaddr, hpi->hugepage_sz);
			if (total_mem >= internal_config.memory ||
					seg_idx >= RTE_MAX_MEMSEG)
				break;
		}
	}
	return 0;
}

int
rte_eal_hugepage_attach(void)
{
	const struct hugepage_info *hpi;
	int fd_hugepage_info, fd_hugepage = -1;
	unsigned i = 0;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	/* Obtain a file descriptor for hugepage_info */
	fd_hugepage_info = open(eal_hugepage_info_path(), O_RDONLY);
	if (fd_hugepage_info < 0) {
		RTE_LOG(ERR, EAL, "Could not open %s\n", eal_hugepage_info_path());
		return -1;
	}

	/* Map the shared hugepage_info into the process address spaces */
	hpi = mmap(NULL, sizeof(struct hugepage_info), PROT_READ, MAP_PRIVATE,
			fd_hugepage_info, 0);
	if (hpi == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "Could not mmap %s\n", eal_hugepage_info_path());
		goto error;
	}

	/* Obtain a file descriptor for contiguous memory */
	fd_hugepage = open(hpi->hugedir, O_RDWR);
	if (fd_hugepage < 0) {
		RTE_LOG(ERR, EAL, "Could not open %s\n", hpi->hugedir);
		goto error;
	}

	/* Map the contiguous memory into each memory segment */
	for (i = 0; i < hpi->num_pages[0]; i++) {

		void *addr;
		struct rte_memseg *seg = &mcfg->memseg[i];

		addr = mmap(seg->addr, hpi->hugepage_sz, PROT_READ|PROT_WRITE,
			    MAP_SHARED|MAP_FIXED, fd_hugepage,
			    i * EAL_PAGE_SIZE);
		if (addr == MAP_FAILED || addr != seg->addr) {
			RTE_LOG(ERR, EAL, "Failed to mmap buffer %u from %s\n",
				i, hpi->hugedir);
			goto error;
		}

	}

	/* hugepage_info is no longer required */
	munmap((void *)(uintptr_t)hpi, sizeof(struct hugepage_info));
	close(fd_hugepage_info);
	close(fd_hugepage);
	return 0;

error:
	if (fd_hugepage_info >= 0)
		close(fd_hugepage_info);
	if (fd_hugepage >= 0)
		close(fd_hugepage);
	return -1;
}

int
rte_eal_using_phys_addrs(void)
{
	return 0;
}
