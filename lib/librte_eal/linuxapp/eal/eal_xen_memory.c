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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include <exec-env/rte_dom0_common.h>

#define PAGE_SIZE RTE_PGSIZE_4K
#define DEFAUL_DOM0_NAME "dom0-mem"

static int xen_fd = -1;
static const char sys_dir_path[] = "/sys/kernel/mm/dom0-mm/memsize-mB";

/*
 * Try to mmap *size bytes in /dev/zero. If it is successful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by mem_size until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of mem_size size.
 */
static void *
xen_get_virtual_area(size_t *size, size_t mem_size)
{
	void *addr;
	int fd;
	long aligned_addr;

	RTE_LOG(INFO, EAL, "Ask a virtual area of 0x%zu bytes\n", *size);

	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0){
		RTE_LOG(ERR, EAL, "Cannot open /dev/zero\n");
		return NULL;
	}
	do {
		addr = mmap(NULL, (*size) + mem_size, PROT_READ,
			MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED)
			*size -= mem_size;
	} while (addr == MAP_FAILED && *size > 0);

	if (addr == MAP_FAILED) {
		close(fd);
		RTE_LOG(INFO, EAL, "Cannot get a virtual area\n");
		return NULL;
	}

	munmap(addr, (*size) + mem_size);
	close(fd);

	/* align addr to a mem_size boundary */
	aligned_addr = (uintptr_t)addr;
	aligned_addr = RTE_ALIGN_CEIL(aligned_addr, mem_size);
        addr = (void *)(aligned_addr);

	RTE_LOG(INFO, EAL, "Virtual area found at %p (size = 0x%zx)\n",
		addr, *size);

	return addr;
}

/**
 * Get memory size configuration from /sys/devices/virtual/misc/dom0_mm
 * /memsize-mB/memsize file, and the size unit is mB.
 */
static int
get_xen_memory_size(void)
{
	char path[PATH_MAX];
	unsigned long mem_size = 0;
	static const char *file_name;

	file_name = "memsize";
	snprintf(path, sizeof(path), "%s/%s",
			sys_dir_path, file_name);

	if (eal_parse_sysfs_value(path, &mem_size) < 0)
		return -1;

	if (mem_size == 0)
		rte_exit(EXIT_FAILURE,"XEN-DOM0:the %s/%s was not"
			" configured.\n",sys_dir_path, file_name);
	if (mem_size % 2)
		rte_exit(EXIT_FAILURE,"XEN-DOM0:the %s/%s must be"
			" even number.\n",sys_dir_path, file_name);

	if (mem_size > DOM0_CONFIG_MEMSIZE)
		rte_exit(EXIT_FAILURE,"XEN-DOM0:the %s/%s should not be larger"
			" than %d mB\n",sys_dir_path, file_name, DOM0_CONFIG_MEMSIZE);

	return mem_size;
}

/**
 * Based on physical address to caculate MFN in Xen Dom0.
 */
phys_addr_t
rte_mem_phy2mch(uint32_t memseg_id, const phys_addr_t phy_addr)
{
	int mfn_id;
	uint64_t mfn, mfn_offset;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg *memseg = mcfg->memseg;

	mfn_id = (phy_addr - memseg[memseg_id].phys_addr) / RTE_PGSIZE_2M;

	/*the MFN is contiguous in 2M */
	mfn_offset = (phy_addr - memseg[memseg_id].phys_addr) %
					RTE_PGSIZE_2M / PAGE_SIZE;
	mfn = mfn_offset + memseg[memseg_id].mfn[mfn_id];

	/** return mechine address */
	return (mfn * PAGE_SIZE + phy_addr % PAGE_SIZE);
}

int
rte_xen_dom0_memory_init(void)
{
	void *vir_addr, *vma_addr = NULL;
	int err, ret = 0;
	uint32_t i, requested, mem_size, memseg_idx, num_memseg = 0;
	size_t vma_len = 0;
	struct memory_info meminfo;
	struct memseg_info seginfo[RTE_MAX_MEMSEG];
	int flags, page_size = getpagesize();
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg *memseg = mcfg->memseg;
	uint64_t total_mem = internal_config.memory;

	memset(seginfo, 0, sizeof(seginfo));
	memset(&meminfo, 0, sizeof(struct memory_info));

	mem_size = get_xen_memory_size();
	requested = (unsigned) (total_mem / 0x100000);
	if (requested > mem_size)
		/* if we didn't satisfy total memory requirements */
		rte_exit(EXIT_FAILURE,"Not enough memory available! Requested: %uMB,"
				" available: %uMB\n", requested, mem_size);
	else if (total_mem != 0)
		mem_size = requested;

	/* Check FD and open once */
	if (xen_fd < 0) {
		xen_fd = open(DOM0_MM_DEV, O_RDWR);
		if (xen_fd < 0) {
			RTE_LOG(ERR, EAL, "Can not open %s\n",DOM0_MM_DEV);
			return -1;
		}
	}

	meminfo.size = mem_size;

	/* construct memory mangement name for Dom0 */
	snprintf(meminfo.name, DOM0_NAME_MAX, "%s-%s",
		internal_config.hugefile_prefix, DEFAUL_DOM0_NAME);

	/* Notify kernel driver to allocate memory */
	ret = ioctl(xen_fd, RTE_DOM0_IOCTL_PREPARE_MEMSEG, &meminfo);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "XEN DOM0:failed to get memory\n");
		err = -EIO;
		goto fail;
	}

	/* Get number of memory segment from driver */
	ret = ioctl(xen_fd, RTE_DOM0_IOCTL_GET_NUM_MEMSEG, &num_memseg);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "XEN DOM0:failed to get memseg count.\n");
		err = -EIO;
		goto fail;
	}

	if(num_memseg > RTE_MAX_MEMSEG){
		RTE_LOG(ERR, EAL, "XEN DOM0: the memseg count %d is greater"
			" than max memseg %d.\n",num_memseg, RTE_MAX_MEMSEG);
		err = -EIO;
		goto fail;
	}

	/* get all memory segements information */
	ret = ioctl(xen_fd, RTE_DOM0_IOCTL_GET_MEMSEG_INFO, seginfo);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "XEN DOM0:failed to get memseg info.\n");
		err = -EIO;
		goto fail;
	}

	/* map all memory segments to contiguous user space */
	for (memseg_idx = 0; memseg_idx < num_memseg; memseg_idx++)
	{
		vma_len = seginfo[memseg_idx].size;

		/**
		 * get the biggest virtual memory area up to vma_len. If it fails,
		 * vma_addr is NULL, so let the kernel provide the address.
		 */
		vma_addr = xen_get_virtual_area(&vma_len, RTE_PGSIZE_2M);
		if (vma_addr == NULL) {
			flags = MAP_SHARED;
			vma_len = RTE_PGSIZE_2M;
		} else
			flags = MAP_SHARED | MAP_FIXED;

		seginfo[memseg_idx].size = vma_len;
		vir_addr = mmap(vma_addr, seginfo[memseg_idx].size,
			PROT_READ|PROT_WRITE, flags, xen_fd,
			memseg_idx * page_size);
		if (vir_addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "XEN DOM0:Could not mmap %s\n",
				DOM0_MM_DEV);
			err = -EIO;
			goto fail;
		}

		memseg[memseg_idx].addr = vir_addr;
		memseg[memseg_idx].phys_addr = page_size *
			seginfo[memseg_idx].pfn ;
		memseg[memseg_idx].len = seginfo[memseg_idx].size;
		for ( i = 0; i < seginfo[memseg_idx].size / RTE_PGSIZE_2M; i++)
			memseg[memseg_idx].mfn[i] = seginfo[memseg_idx].mfn[i];

		/* MFNs are continuous in 2M, so assume that page size is 2M */
		memseg[memseg_idx].hugepage_sz = RTE_PGSIZE_2M;

		memseg[memseg_idx].nchannel = mcfg->nchannel;
		memseg[memseg_idx].nrank = mcfg->nrank;

		/* NUMA is not suppoted in Xen Dom0, so only set socket 0*/
		memseg[memseg_idx].socket_id = 0;
	}

	return 0;
fail:
	if (xen_fd > 0) {
		close(xen_fd);
		xen_fd = -1;
	}
	return err;
}

/*
 * This creates the memory mappings in the secondary process to match that of
 * the server process. It goes through each memory segment in the DPDK runtime
 * configuration, mapping them in order to form a contiguous block in the
 * virtual memory space
 */
int
rte_xen_dom0_memory_attach(void)
{
	const struct rte_mem_config *mcfg;
	unsigned s = 0; /* s used to track the segment number */
	int xen_fd = -1;
	int ret = -1;
	void *vir_addr;
	char name[DOM0_NAME_MAX] = {0};
	int page_size = getpagesize();

	mcfg = rte_eal_get_configuration()->mem_config;

	/* Check FD and open once */
	if (xen_fd < 0) {
		xen_fd = open(DOM0_MM_DEV, O_RDWR);
		if (xen_fd < 0) {
			RTE_LOG(ERR, EAL, "Can not open %s\n",DOM0_MM_DEV);
			goto error;
		}
	}

	/* construct memory mangement name for Dom0 */
	snprintf(name, DOM0_NAME_MAX, "%s-%s",
		internal_config.hugefile_prefix, DEFAUL_DOM0_NAME);
	/* attach to memory segments of primary process */
	ret = ioctl(xen_fd, RTE_DOM0_IOCTL_ATTACH_TO_MEMSEG, name);
	if (ret) {
		RTE_LOG(ERR, EAL,"attach memory segments fail.\n");
		goto error;
	}

	/* map all segments into memory to make sure we get the addrs */
	for (s = 0; s < RTE_MAX_MEMSEG; ++s) {

		/*
		 * the first memory segment with len==0 is the one that
		 * follows the last valid segment.
		 */
		if (mcfg->memseg[s].len == 0)
			break;

		vir_addr = mmap(mcfg->memseg[s].addr, mcfg->memseg[s].len,
				PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, xen_fd,
				s * page_size);
		if (vir_addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "Could not mmap %llu bytes "
				"in %s to requested address [%p]\n",
				(unsigned long long)mcfg->memseg[s].len, DOM0_MM_DEV,
				mcfg->memseg[s].addr);
			goto error;
		}
	}
	return 0;

error:
	if (xen_fd >= 0) {
		close(xen_fd);
		xen_fd = -1;
	}
	return -1;
}
