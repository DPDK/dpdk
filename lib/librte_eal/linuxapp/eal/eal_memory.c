/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2013 6WIND.
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

#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
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
#include <signal.h>
#include <setjmp.h>

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
#include "eal_hugepages.h"

#define PFN_MASK_SIZE	8

#ifdef RTE_LIBRTE_XEN_DOM0
int rte_xen_dom0_supported(void)
{
	return internal_config.xen_dom0_support;
}
#endif

/**
 * @file
 * Huge page mapping under linux
 *
 * To reserve a big contiguous amount of memory, we use the hugepage
 * feature of linux. For that, we need to have hugetlbfs mounted. This
 * code will create many files in this directory (one per page) and
 * map them in virtual memory. For each page, we will retrieve its
 * physical address and remap it in order to have a virtual contiguous
 * zone as well as a physical contiguous zone.
 */

static uint64_t baseaddr_offset;

static bool phys_addrs_available = true;

#define RANDOMIZE_VA_SPACE_FILE "/proc/sys/kernel/randomize_va_space"

static void
test_phys_addrs_available(void)
{
	uint64_t tmp;
	phys_addr_t physaddr;

	/* For dom0, phys addresses can always be available */
	if (rte_xen_dom0_supported())
		return;

	physaddr = rte_mem_virt2phy(&tmp);
	if (physaddr == RTE_BAD_PHYS_ADDR) {
		RTE_LOG(ERR, EAL,
			"Cannot obtain physical addresses: %s. "
			"Only vfio will function.\n",
			strerror(errno));
		phys_addrs_available = false;
	}
}

/*
 * Get physical address of any mapped virtual address in the current process.
 */
phys_addr_t
rte_mem_virt2phy(const void *virtaddr)
{
	int fd, retval;
	uint64_t page, physaddr;
	unsigned long virt_pfn;
	int page_size;
	off_t offset;

	/* when using dom0, /proc/self/pagemap always returns 0, check in
	 * dpdk memory by browsing the memsegs */
	if (rte_xen_dom0_supported()) {
		struct rte_mem_config *mcfg;
		struct rte_memseg *memseg;
		unsigned i;

		mcfg = rte_eal_get_configuration()->mem_config;
		for (i = 0; i < RTE_MAX_MEMSEG; i++) {
			memseg = &mcfg->memseg[i];
			if (memseg->addr == NULL)
				break;
			if (virtaddr > memseg->addr &&
					virtaddr < RTE_PTR_ADD(memseg->addr,
						memseg->len)) {
				return memseg->phys_addr +
					RTE_PTR_DIFF(virtaddr, memseg->addr);
			}
		}

		return RTE_BAD_PHYS_ADDR;
	}

	/* Cannot parse /proc/self/pagemap, no need to log errors everywhere */
	if (!phys_addrs_available)
		return RTE_BAD_PHYS_ADDR;

	/* standard page size */
	page_size = getpagesize();

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot open /proc/self/pagemap: %s\n",
			__func__, strerror(errno));
		return RTE_BAD_PHYS_ADDR;
	}

	virt_pfn = (unsigned long)virtaddr / page_size;
	offset = sizeof(uint64_t) * virt_pfn;
	if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
		RTE_LOG(ERR, EAL, "%s(): seek error in /proc/self/pagemap: %s\n",
				__func__, strerror(errno));
		close(fd);
		return RTE_BAD_PHYS_ADDR;
	}

	retval = read(fd, &page, PFN_MASK_SIZE);
	close(fd);
	if (retval < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot read /proc/self/pagemap: %s\n",
				__func__, strerror(errno));
		return RTE_BAD_PHYS_ADDR;
	} else if (retval != PFN_MASK_SIZE) {
		RTE_LOG(ERR, EAL, "%s(): read %d bytes from /proc/self/pagemap "
				"but expected %d:\n",
				__func__, retval, PFN_MASK_SIZE);
		return RTE_BAD_PHYS_ADDR;
	}

	/*
	 * the pfn (page frame number) are bits 0-54 (see
	 * pagemap.txt in linux Documentation)
	 */
	if ((page & 0x7fffffffffffffULL) == 0)
		return RTE_BAD_PHYS_ADDR;

	physaddr = ((page & 0x7fffffffffffffULL) * page_size)
		+ ((unsigned long)virtaddr % page_size);

	return physaddr;
}

/*
 * For each hugepage in hugepg_tbl, fill the physaddr value. We find
 * it by browsing the /proc/self/pagemap special file.
 */
static int
find_physaddrs(struct hugepage_file *hugepg_tbl, struct hugepage_info *hpi)
{
	unsigned int i;
	phys_addr_t addr;

	for (i = 0; i < hpi->num_pages[0]; i++) {
		addr = rte_mem_virt2phy(hugepg_tbl[i].orig_va);
		if (addr == RTE_BAD_PHYS_ADDR)
			return -1;
		hugepg_tbl[i].physaddr = addr;
	}
	return 0;
}

/*
 * For each hugepage in hugepg_tbl, fill the physaddr value sequentially.
 */
static int
set_physaddrs(struct hugepage_file *hugepg_tbl, struct hugepage_info *hpi)
{
	unsigned int i;
	static phys_addr_t addr;

	for (i = 0; i < hpi->num_pages[0]; i++) {
		hugepg_tbl[i].physaddr = addr;
		addr += hugepg_tbl[i].size;
	}
	return 0;
}

/*
 * Check whether address-space layout randomization is enabled in
 * the kernel. This is important for multi-process as it can prevent
 * two processes mapping data to the same virtual address
 * Returns:
 *    0 - address space randomization disabled
 *    1/2 - address space randomization enabled
 *    negative error code on error
 */
static int
aslr_enabled(void)
{
	char c;
	int retval, fd = open(RANDOMIZE_VA_SPACE_FILE, O_RDONLY);
	if (fd < 0)
		return -errno;
	retval = read(fd, &c, 1);
	close(fd);
	if (retval < 0)
		return -errno;
	if (retval == 0)
		return -EIO;
	switch (c) {
		case '0' : return 0;
		case '1' : return 1;
		case '2' : return 2;
		default: return -EINVAL;
	}
}

/*
 * Try to mmap *size bytes in /dev/zero. If it is successful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by hugepage_sz until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of hugepage size.
 */
static void *
get_virtual_area(size_t *size, size_t hugepage_sz)
{
	void *addr;
	int fd;
	long aligned_addr;

	if (internal_config.base_virtaddr != 0) {
		addr = (void*) (uintptr_t) (internal_config.base_virtaddr +
				baseaddr_offset);
	}
	else addr = NULL;

	RTE_LOG(DEBUG, EAL, "Ask a virtual area of 0x%zx bytes\n", *size);

	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0){
		RTE_LOG(ERR, EAL, "Cannot open /dev/zero\n");
		return NULL;
	}
	do {
		addr = mmap(addr,
				(*size) + hugepage_sz, PROT_READ,
#ifdef RTE_ARCH_PPC_64
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
#else
				MAP_PRIVATE,
#endif
				fd, 0);
		if (addr == MAP_FAILED)
			*size -= hugepage_sz;
	} while (addr == MAP_FAILED && *size > 0);

	if (addr == MAP_FAILED) {
		close(fd);
		RTE_LOG(ERR, EAL, "Cannot get a virtual area: %s\n",
			strerror(errno));
		return NULL;
	}

	munmap(addr, (*size) + hugepage_sz);
	close(fd);

	/* align addr to a huge page size boundary */
	aligned_addr = (long)addr;
	aligned_addr += (hugepage_sz - 1);
	aligned_addr &= (~(hugepage_sz - 1));
	addr = (void *)(aligned_addr);

	RTE_LOG(DEBUG, EAL, "Virtual area found at %p (size = 0x%zx)\n",
		addr, *size);

	/* increment offset */
	baseaddr_offset += *size;

	return addr;
}

static sigjmp_buf huge_jmpenv;

static void huge_sigbus_handler(int signo __rte_unused)
{
	siglongjmp(huge_jmpenv, 1);
}

/* Put setjmp into a wrap method to avoid compiling error. Any non-volatile,
 * non-static local variable in the stack frame calling sigsetjmp might be
 * clobbered by a call to longjmp.
 */
static int huge_wrap_sigsetjmp(void)
{
	return sigsetjmp(huge_jmpenv, 1);
}

/*
 * Mmap all hugepages of hugepage table: it first open a file in
 * hugetlbfs, then mmap() hugepage_sz data in it. If orig is set, the
 * virtual address is stored in hugepg_tbl[i].orig_va, else it is stored
 * in hugepg_tbl[i].final_va. The second mapping (when orig is 0) tries to
 * map continguous physical blocks in contiguous virtual blocks.
 */
static unsigned
map_all_hugepages(struct hugepage_file *hugepg_tbl,
		struct hugepage_info *hpi, int orig)
{
	int fd;
	unsigned i;
	void *virtaddr;
	void *vma_addr = NULL;
	size_t vma_len = 0;

	for (i = 0; i < hpi->num_pages[0]; i++) {
		uint64_t hugepage_sz = hpi->hugepage_sz;

		if (orig) {
			hugepg_tbl[i].file_id = i;
			hugepg_tbl[i].size = hugepage_sz;
			eal_get_hugefile_path(hugepg_tbl[i].filepath,
					sizeof(hugepg_tbl[i].filepath), hpi->hugedir,
					hugepg_tbl[i].file_id);
			hugepg_tbl[i].filepath[sizeof(hugepg_tbl[i].filepath) - 1] = '\0';
		}
#ifndef RTE_ARCH_64
		/* for 32-bit systems, don't remap 1G and 16G pages, just reuse
		 * original map address as final map address.
		 */
		else if ((hugepage_sz == RTE_PGSIZE_1G)
			|| (hugepage_sz == RTE_PGSIZE_16G)) {
			hugepg_tbl[i].final_va = hugepg_tbl[i].orig_va;
			hugepg_tbl[i].orig_va = NULL;
			continue;
		}
#endif
		else if (vma_len == 0) {
			unsigned j, num_pages;

			/* reserve a virtual area for next contiguous
			 * physical block: count the number of
			 * contiguous physical pages. */
			for (j = i+1; j < hpi->num_pages[0] ; j++) {
#ifdef RTE_ARCH_PPC_64
				/* The physical addresses are sorted in
				 * descending order on PPC64 */
				if (hugepg_tbl[j].physaddr !=
				    hugepg_tbl[j-1].physaddr - hugepage_sz)
					break;
#else
				if (hugepg_tbl[j].physaddr !=
				    hugepg_tbl[j-1].physaddr + hugepage_sz)
					break;
#endif
			}
			num_pages = j - i;
			vma_len = num_pages * hugepage_sz;

			/* get the biggest virtual memory area up to
			 * vma_len. If it fails, vma_addr is NULL, so
			 * let the kernel provide the address. */
			vma_addr = get_virtual_area(&vma_len, hpi->hugepage_sz);
			if (vma_addr == NULL)
				vma_len = hugepage_sz;
		}

		/* try to create hugepage file */
		fd = open(hugepg_tbl[i].filepath, O_CREAT | O_RDWR, 0600);
		if (fd < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n", __func__,
					strerror(errno));
			return i;
		}

		/* map the segment, and populate page tables,
		 * the kernel fills this segment with zeros */
		virtaddr = mmap(vma_addr, hugepage_sz, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_POPULATE, fd, 0);
		if (virtaddr == MAP_FAILED) {
			RTE_LOG(DEBUG, EAL, "%s(): mmap failed: %s\n", __func__,
					strerror(errno));
			close(fd);
			return i;
		}

		if (orig) {
			hugepg_tbl[i].orig_va = virtaddr;
		}
		else {
			hugepg_tbl[i].final_va = virtaddr;
		}

		if (orig) {
			/* In linux, hugetlb limitations, like cgroup, are
			 * enforced at fault time instead of mmap(), even
			 * with the option of MAP_POPULATE. Kernel will send
			 * a SIGBUS signal. To avoid to be killed, save stack
			 * environment here, if SIGBUS happens, we can jump
			 * back here.
			 */
			if (huge_wrap_sigsetjmp()) {
				RTE_LOG(DEBUG, EAL, "SIGBUS: Cannot mmap more "
					"hugepages of size %u MB\n",
					(unsigned)(hugepage_sz / 0x100000));
				munmap(virtaddr, hugepage_sz);
				close(fd);
				unlink(hugepg_tbl[i].filepath);
				return i;
			}
			*(int *)virtaddr = 0;
		}


		/* set shared flock on the file. */
		if (flock(fd, LOCK_SH | LOCK_NB) == -1) {
			RTE_LOG(DEBUG, EAL, "%s(): Locking file failed:%s \n",
				__func__, strerror(errno));
			close(fd);
			return i;
		}

		close(fd);

		vma_addr = (char *)vma_addr + hugepage_sz;
		vma_len -= hugepage_sz;
	}

	return i;
}

/* Unmap all hugepages from original mapping */
static int
unmap_all_hugepages_orig(struct hugepage_file *hugepg_tbl, struct hugepage_info *hpi)
{
        unsigned i;
        for (i = 0; i < hpi->num_pages[0]; i++) {
                if (hugepg_tbl[i].orig_va) {
                        munmap(hugepg_tbl[i].orig_va, hpi->hugepage_sz);
                        hugepg_tbl[i].orig_va = NULL;
                }
        }
        return 0;
}

/*
 * Parse /proc/self/numa_maps to get the NUMA socket ID for each huge
 * page.
 */
static int
find_numasocket(struct hugepage_file *hugepg_tbl, struct hugepage_info *hpi)
{
	int socket_id;
	char *end, *nodestr;
	unsigned i, hp_count = 0;
	uint64_t virt_addr;
	char buf[BUFSIZ];
	char hugedir_str[PATH_MAX];
	FILE *f;

	f = fopen("/proc/self/numa_maps", "r");
	if (f == NULL) {
		RTE_LOG(NOTICE, EAL, "cannot open /proc/self/numa_maps,"
				" consider that all memory is in socket_id 0\n");
		return 0;
	}

	snprintf(hugedir_str, sizeof(hugedir_str),
			"%s/%s", hpi->hugedir, internal_config.hugefile_prefix);

	/* parse numa map */
	while (fgets(buf, sizeof(buf), f) != NULL) {

		/* ignore non huge page */
		if (strstr(buf, " huge ") == NULL &&
				strstr(buf, hugedir_str) == NULL)
			continue;

		/* get zone addr */
		virt_addr = strtoull(buf, &end, 16);
		if (virt_addr == 0 || end == buf) {
			RTE_LOG(ERR, EAL, "%s(): error in numa_maps parsing\n", __func__);
			goto error;
		}

		/* get node id (socket id) */
		nodestr = strstr(buf, " N");
		if (nodestr == NULL) {
			RTE_LOG(ERR, EAL, "%s(): error in numa_maps parsing\n", __func__);
			goto error;
		}
		nodestr += 2;
		end = strstr(nodestr, "=");
		if (end == NULL) {
			RTE_LOG(ERR, EAL, "%s(): error in numa_maps parsing\n", __func__);
			goto error;
		}
		end[0] = '\0';
		end = NULL;

		socket_id = strtoul(nodestr, &end, 0);
		if ((nodestr[0] == '\0') || (end == NULL) || (*end != '\0')) {
			RTE_LOG(ERR, EAL, "%s(): error in numa_maps parsing\n", __func__);
			goto error;
		}

		/* if we find this page in our mappings, set socket_id */
		for (i = 0; i < hpi->num_pages[0]; i++) {
			void *va = (void *)(unsigned long)virt_addr;
			if (hugepg_tbl[i].orig_va == va) {
				hugepg_tbl[i].socket_id = socket_id;
				hp_count++;
			}
		}
	}

	if (hp_count < hpi->num_pages[0])
		goto error;

	fclose(f);
	return 0;

error:
	fclose(f);
	return -1;
}

static int
cmp_physaddr(const void *a, const void *b)
{
#ifndef RTE_ARCH_PPC_64
	const struct hugepage_file *p1 = a;
	const struct hugepage_file *p2 = b;
#else
	/* PowerPC needs memory sorted in reverse order from x86 */
	const struct hugepage_file *p1 = b;
	const struct hugepage_file *p2 = a;
#endif
	if (p1->physaddr < p2->physaddr)
		return -1;
	else if (p1->physaddr > p2->physaddr)
		return 1;
	else
		return 0;
}

/*
 * Uses mmap to create a shared memory area for storage of data
 * Used in this file to store the hugepage file map on disk
 */
static void *
create_shared_memory(const char *filename, const size_t mem_size)
{
	void *retval;
	int fd = open(filename, O_CREAT | O_RDWR, 0666);
	if (fd < 0)
		return NULL;
	if (ftruncate(fd, mem_size) < 0) {
		close(fd);
		return NULL;
	}
	retval = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	return retval;
}

/*
 * this copies *active* hugepages from one hugepage table to another.
 * destination is typically the shared memory.
 */
static int
copy_hugepages_to_shared_mem(struct hugepage_file * dst, int dest_size,
		const struct hugepage_file * src, int src_size)
{
	int src_pos, dst_pos = 0;

	for (src_pos = 0; src_pos < src_size; src_pos++) {
		if (src[src_pos].final_va != NULL) {
			/* error on overflow attempt */
			if (dst_pos == dest_size)
				return -1;
			memcpy(&dst[dst_pos], &src[src_pos], sizeof(struct hugepage_file));
			dst_pos++;
		}
	}
	return 0;
}

static int
unlink_hugepage_files(struct hugepage_file *hugepg_tbl,
		unsigned num_hp_info)
{
	unsigned socket, size;
	int page, nrpages = 0;

	/* get total number of hugepages */
	for (size = 0; size < num_hp_info; size++)
		for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++)
			nrpages +=
			internal_config.hugepage_info[size].num_pages[socket];

	for (page = 0; page < nrpages; page++) {
		struct hugepage_file *hp = &hugepg_tbl[page];

		if (hp->final_va != NULL && unlink(hp->filepath)) {
			RTE_LOG(WARNING, EAL, "%s(): Removing %s failed: %s\n",
				__func__, hp->filepath, strerror(errno));
		}
	}
	return 0;
}

/*
 * unmaps hugepages that are not going to be used. since we originally allocate
 * ALL hugepages (not just those we need), additional unmapping needs to be done.
 */
static int
unmap_unneeded_hugepages(struct hugepage_file *hugepg_tbl,
		struct hugepage_info *hpi,
		unsigned num_hp_info)
{
	unsigned socket, size;
	int page, nrpages = 0;

	/* get total number of hugepages */
	for (size = 0; size < num_hp_info; size++)
		for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++)
			nrpages += internal_config.hugepage_info[size].num_pages[socket];

	for (size = 0; size < num_hp_info; size++) {
		for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
			unsigned pages_found = 0;

			/* traverse until we have unmapped all the unused pages */
			for (page = 0; page < nrpages; page++) {
				struct hugepage_file *hp = &hugepg_tbl[page];

				/* find a page that matches the criteria */
				if ((hp->size == hpi[size].hugepage_sz) &&
						(hp->socket_id == (int) socket)) {

					/* if we skipped enough pages, unmap the rest */
					if (pages_found == hpi[size].num_pages[socket]) {
						uint64_t unmap_len;

						unmap_len = hp->size;

						/* get start addr and len of the remaining segment */
						munmap(hp->final_va, (size_t) unmap_len);

						hp->final_va = NULL;
						if (unlink(hp->filepath) == -1) {
							RTE_LOG(ERR, EAL, "%s(): Removing %s failed: %s\n",
									__func__, hp->filepath, strerror(errno));
							return -1;
						}
					} else {
						/* lock the page and skip */
						pages_found++;
					}

				} /* match page */
			} /* foreach page */
		} /* foreach socket */
	} /* foreach pagesize */

	return 0;
}

static inline uint64_t
get_socket_mem_size(int socket)
{
	uint64_t size = 0;
	unsigned i;

	for (i = 0; i < internal_config.num_hugepage_sizes; i++){
		struct hugepage_info *hpi = &internal_config.hugepage_info[i];
		if (hpi->hugedir != NULL)
			size += hpi->hugepage_sz * hpi->num_pages[socket];
	}

	return size;
}

/*
 * This function is a NUMA-aware equivalent of calc_num_pages.
 * It takes in the list of hugepage sizes and the
 * number of pages thereof, and calculates the best number of
 * pages of each size to fulfill the request for <memory> ram
 */
static int
calc_num_pages_per_socket(uint64_t * memory,
		struct hugepage_info *hp_info,
		struct hugepage_info *hp_used,
		unsigned num_hp_info)
{
	unsigned socket, j, i = 0;
	unsigned requested, available;
	int total_num_pages = 0;
	uint64_t remaining_mem, cur_mem;
	uint64_t total_mem = internal_config.memory;

	if (num_hp_info == 0)
		return -1;

	/* if specific memory amounts per socket weren't requested */
	if (internal_config.force_sockets == 0) {
		int cpu_per_socket[RTE_MAX_NUMA_NODES];
		size_t default_size, total_size;
		unsigned lcore_id;

		/* Compute number of cores per socket */
		memset(cpu_per_socket, 0, sizeof(cpu_per_socket));
		RTE_LCORE_FOREACH(lcore_id) {
			cpu_per_socket[rte_lcore_to_socket_id(lcore_id)]++;
		}

		/*
		 * Automatically spread requested memory amongst detected sockets according
		 * to number of cores from cpu mask present on each socket
		 */
		total_size = internal_config.memory;
		for (socket = 0; socket < RTE_MAX_NUMA_NODES && total_size != 0; socket++) {

			/* Set memory amount per socket */
			default_size = (internal_config.memory * cpu_per_socket[socket])
			                / rte_lcore_count();

			/* Limit to maximum available memory on socket */
			default_size = RTE_MIN(default_size, get_socket_mem_size(socket));

			/* Update sizes */
			memory[socket] = default_size;
			total_size -= default_size;
		}

		/*
		 * If some memory is remaining, try to allocate it by getting all
		 * available memory from sockets, one after the other
		 */
		for (socket = 0; socket < RTE_MAX_NUMA_NODES && total_size != 0; socket++) {
			/* take whatever is available */
			default_size = RTE_MIN(get_socket_mem_size(socket) - memory[socket],
			                       total_size);

			/* Update sizes */
			memory[socket] += default_size;
			total_size -= default_size;
		}
	}

	for (socket = 0; socket < RTE_MAX_NUMA_NODES && total_mem != 0; socket++) {
		/* skips if the memory on specific socket wasn't requested */
		for (i = 0; i < num_hp_info && memory[socket] != 0; i++){
			hp_used[i].hugedir = hp_info[i].hugedir;
			hp_used[i].num_pages[socket] = RTE_MIN(
					memory[socket] / hp_info[i].hugepage_sz,
					hp_info[i].num_pages[socket]);

			cur_mem = hp_used[i].num_pages[socket] *
					hp_used[i].hugepage_sz;

			memory[socket] -= cur_mem;
			total_mem -= cur_mem;

			total_num_pages += hp_used[i].num_pages[socket];

			/* check if we have met all memory requests */
			if (memory[socket] == 0)
				break;

			/* check if we have any more pages left at this size, if so
			 * move on to next size */
			if (hp_used[i].num_pages[socket] == hp_info[i].num_pages[socket])
				continue;
			/* At this point we know that there are more pages available that are
			 * bigger than the memory we want, so lets see if we can get enough
			 * from other page sizes.
			 */
			remaining_mem = 0;
			for (j = i+1; j < num_hp_info; j++)
				remaining_mem += hp_info[j].hugepage_sz *
				hp_info[j].num_pages[socket];

			/* is there enough other memory, if not allocate another page and quit */
			if (remaining_mem < memory[socket]){
				cur_mem = RTE_MIN(memory[socket],
						hp_info[i].hugepage_sz);
				memory[socket] -= cur_mem;
				total_mem -= cur_mem;
				hp_used[i].num_pages[socket]++;
				total_num_pages++;
				break; /* we are done with this socket*/
			}
		}
		/* if we didn't satisfy all memory requirements per socket */
		if (memory[socket] > 0) {
			/* to prevent icc errors */
			requested = (unsigned) (internal_config.socket_mem[socket] /
					0x100000);
			available = requested -
					((unsigned) (memory[socket] / 0x100000));
			RTE_LOG(ERR, EAL, "Not enough memory available on socket %u! "
					"Requested: %uMB, available: %uMB\n", socket,
					requested, available);
			return -1;
		}
	}

	/* if we didn't satisfy total memory requirements */
	if (total_mem > 0) {
		requested = (unsigned) (internal_config.memory / 0x100000);
		available = requested - (unsigned) (total_mem / 0x100000);
		RTE_LOG(ERR, EAL, "Not enough memory available! Requested: %uMB,"
				" available: %uMB\n", requested, available);
		return -1;
	}
	return total_num_pages;
}

static inline size_t
eal_get_hugepage_mem_size(void)
{
	uint64_t size = 0;
	unsigned i, j;

	for (i = 0; i < internal_config.num_hugepage_sizes; i++) {
		struct hugepage_info *hpi = &internal_config.hugepage_info[i];
		if (hpi->hugedir != NULL) {
			for (j = 0; j < RTE_MAX_NUMA_NODES; j++) {
				size += hpi->hugepage_sz * hpi->num_pages[j];
			}
		}
	}

	return (size < SIZE_MAX) ? (size_t)(size) : SIZE_MAX;
}

static struct sigaction huge_action_old;
static int huge_need_recover;

static void
huge_register_sigbus(void)
{
	sigset_t mask;
	struct sigaction action;

	sigemptyset(&mask);
	sigaddset(&mask, SIGBUS);
	action.sa_flags = 0;
	action.sa_mask = mask;
	action.sa_handler = huge_sigbus_handler;

	huge_need_recover = !sigaction(SIGBUS, &action, &huge_action_old);
}

static void
huge_recover_sigbus(void)
{
	if (huge_need_recover) {
		sigaction(SIGBUS, &huge_action_old, NULL);
		huge_need_recover = 0;
	}
}

/*
 * Prepare physical memory mapping: fill configuration structure with
 * these infos, return 0 on success.
 *  1. map N huge pages in separate files in hugetlbfs
 *  2. find associated physical addr
 *  3. find associated NUMA socket ID
 *  4. sort all huge pages by physical address
 *  5. remap these N huge pages in the correct order
 *  6. unmap the first mapping
 *  7. fill memsegs in configuration with contiguous zones
 */
int
rte_eal_hugepage_init(void)
{
	struct rte_mem_config *mcfg;
	struct hugepage_file *hugepage = NULL, *tmp_hp = NULL;
	struct hugepage_info used_hp[MAX_HUGEPAGE_SIZES];

	uint64_t memory[RTE_MAX_NUMA_NODES];

	unsigned hp_offset;
	int i, j, new_memseg;
	int nr_hugefiles, nr_hugepages = 0;
	void *addr;

	test_phys_addrs_available();

	memset(used_hp, 0, sizeof(used_hp));

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* hugetlbfs can be disabled */
	if (internal_config.no_hugetlbfs) {
		addr = mmap(NULL, internal_config.memory, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		if (addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "%s: mmap() failed: %s\n", __func__,
					strerror(errno));
			return -1;
		}
		mcfg->memseg[0].phys_addr = (phys_addr_t)(uintptr_t)addr;
		mcfg->memseg[0].addr = addr;
		mcfg->memseg[0].hugepage_sz = RTE_PGSIZE_4K;
		mcfg->memseg[0].len = internal_config.memory;
		mcfg->memseg[0].socket_id = 0;
		return 0;
	}

/* check if app runs on Xen Dom0 */
	if (internal_config.xen_dom0_support) {
#ifdef RTE_LIBRTE_XEN_DOM0
		/* use dom0_mm kernel driver to init memory */
		if (rte_xen_dom0_memory_init() < 0)
			return -1;
		else
			return 0;
#endif
	}

	/* calculate total number of hugepages available. at this point we haven't
	 * yet started sorting them so they all are on socket 0 */
	for (i = 0; i < (int) internal_config.num_hugepage_sizes; i++) {
		/* meanwhile, also initialize used_hp hugepage sizes in used_hp */
		used_hp[i].hugepage_sz = internal_config.hugepage_info[i].hugepage_sz;

		nr_hugepages += internal_config.hugepage_info[i].num_pages[0];
	}

	/*
	 * allocate a memory area for hugepage table.
	 * this isn't shared memory yet. due to the fact that we need some
	 * processing done on these pages, shared memory will be created
	 * at a later stage.
	 */
	tmp_hp = malloc(nr_hugepages * sizeof(struct hugepage_file));
	if (tmp_hp == NULL)
		goto fail;

	memset(tmp_hp, 0, nr_hugepages * sizeof(struct hugepage_file));

	hp_offset = 0; /* where we start the current page size entries */

	huge_register_sigbus();

	/* map all hugepages and sort them */
	for (i = 0; i < (int)internal_config.num_hugepage_sizes; i ++){
		unsigned pages_old, pages_new;
		struct hugepage_info *hpi;

		/*
		 * we don't yet mark hugepages as used at this stage, so
		 * we just map all hugepages available to the system
		 * all hugepages are still located on socket 0
		 */
		hpi = &internal_config.hugepage_info[i];

		if (hpi->num_pages[0] == 0)
			continue;

		/* map all hugepages available */
		pages_old = hpi->num_pages[0];
		pages_new = map_all_hugepages(&tmp_hp[hp_offset], hpi, 1);
		if (pages_new < pages_old) {
			RTE_LOG(DEBUG, EAL,
				"%d not %d hugepages of size %u MB allocated\n",
				pages_new, pages_old,
				(unsigned)(hpi->hugepage_sz / 0x100000));

			int pages = pages_old - pages_new;

			nr_hugepages -= pages;
			hpi->num_pages[0] = pages_new;
			if (pages_new == 0)
				continue;
		}

		if (phys_addrs_available) {
			/* find physical addresses for each hugepage */
			if (find_physaddrs(&tmp_hp[hp_offset], hpi) < 0) {
				RTE_LOG(DEBUG, EAL, "Failed to find phys addr "
					"for %u MB pages\n",
					(unsigned int)(hpi->hugepage_sz / 0x100000));
				goto fail;
			}
		} else {
			/* set physical addresses for each hugepage */
			if (set_physaddrs(&tmp_hp[hp_offset], hpi) < 0) {
				RTE_LOG(DEBUG, EAL, "Failed to set phys addr "
					"for %u MB pages\n",
					(unsigned int)(hpi->hugepage_sz / 0x100000));
				goto fail;
			}
		}

		if (find_numasocket(&tmp_hp[hp_offset], hpi) < 0){
			RTE_LOG(DEBUG, EAL, "Failed to find NUMA socket for %u MB pages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		qsort(&tmp_hp[hp_offset], hpi->num_pages[0],
		      sizeof(struct hugepage_file), cmp_physaddr);

		/* remap all hugepages */
		if (map_all_hugepages(&tmp_hp[hp_offset], hpi, 0) !=
		    hpi->num_pages[0]) {
			RTE_LOG(ERR, EAL, "Failed to remap %u MB pages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		/* unmap original mappings */
		if (unmap_all_hugepages_orig(&tmp_hp[hp_offset], hpi) < 0)
			goto fail;

		/* we have processed a num of hugepages of this size, so inc offset */
		hp_offset += hpi->num_pages[0];
	}

	huge_recover_sigbus();

	if (internal_config.memory == 0 && internal_config.force_sockets == 0)
		internal_config.memory = eal_get_hugepage_mem_size();

	nr_hugefiles = nr_hugepages;


	/* clean out the numbers of pages */
	for (i = 0; i < (int) internal_config.num_hugepage_sizes; i++)
		for (j = 0; j < RTE_MAX_NUMA_NODES; j++)
			internal_config.hugepage_info[i].num_pages[j] = 0;

	/* get hugepages for each socket */
	for (i = 0; i < nr_hugefiles; i++) {
		int socket = tmp_hp[i].socket_id;

		/* find a hugepage info with right size and increment num_pages */
		const int nb_hpsizes = RTE_MIN(MAX_HUGEPAGE_SIZES,
				(int)internal_config.num_hugepage_sizes);
		for (j = 0; j < nb_hpsizes; j++) {
			if (tmp_hp[i].size ==
					internal_config.hugepage_info[j].hugepage_sz) {
				internal_config.hugepage_info[j].num_pages[socket]++;
			}
		}
	}

	/* make a copy of socket_mem, needed for number of pages calculation */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		memory[i] = internal_config.socket_mem[i];

	/* calculate final number of pages */
	nr_hugepages = calc_num_pages_per_socket(memory,
			internal_config.hugepage_info, used_hp,
			internal_config.num_hugepage_sizes);

	/* error if not enough memory available */
	if (nr_hugepages < 0)
		goto fail;

	/* reporting in! */
	for (i = 0; i < (int) internal_config.num_hugepage_sizes; i++) {
		for (j = 0; j < RTE_MAX_NUMA_NODES; j++) {
			if (used_hp[i].num_pages[j] > 0) {
				RTE_LOG(DEBUG, EAL,
					"Requesting %u pages of size %uMB"
					" from socket %i\n",
					used_hp[i].num_pages[j],
					(unsigned)
					(used_hp[i].hugepage_sz / 0x100000),
					j);
			}
		}
	}

	/* create shared memory */
	hugepage = create_shared_memory(eal_hugepage_info_path(),
			nr_hugefiles * sizeof(struct hugepage_file));

	if (hugepage == NULL) {
		RTE_LOG(ERR, EAL, "Failed to create shared memory!\n");
		goto fail;
	}
	memset(hugepage, 0, nr_hugefiles * sizeof(struct hugepage_file));

	/*
	 * unmap pages that we won't need (looks at used_hp).
	 * also, sets final_va to NULL on pages that were unmapped.
	 */
	if (unmap_unneeded_hugepages(tmp_hp, used_hp,
			internal_config.num_hugepage_sizes) < 0) {
		RTE_LOG(ERR, EAL, "Unmapping and locking hugepages failed!\n");
		goto fail;
	}

	/*
	 * copy stuff from malloc'd hugepage* to the actual shared memory.
	 * this procedure only copies those hugepages that have final_va
	 * not NULL. has overflow protection.
	 */
	if (copy_hugepages_to_shared_mem(hugepage, nr_hugefiles,
			tmp_hp, nr_hugefiles) < 0) {
		RTE_LOG(ERR, EAL, "Copying tables to shared memory failed!\n");
		goto fail;
	}

	/* free the hugepage backing files */
	if (internal_config.hugepage_unlink &&
		unlink_hugepage_files(tmp_hp, internal_config.num_hugepage_sizes) < 0) {
		RTE_LOG(ERR, EAL, "Unlinking hugepage files failed!\n");
		goto fail;
	}

	/* free the temporary hugepage table */
	free(tmp_hp);
	tmp_hp = NULL;

	/* first memseg index shall be 0 after incrementing it below */
	j = -1;
	for (i = 0; i < nr_hugefiles; i++) {
		new_memseg = 0;

		/* if this is a new section, create a new memseg */
		if (i == 0)
			new_memseg = 1;
		else if (hugepage[i].socket_id != hugepage[i-1].socket_id)
			new_memseg = 1;
		else if (hugepage[i].size != hugepage[i-1].size)
			new_memseg = 1;

#ifdef RTE_ARCH_PPC_64
		/* On PPC64 architecture, the mmap always start from higher
		 * virtual address to lower address. Here, both the physical
		 * address and virtual address are in descending order */
		else if ((hugepage[i-1].physaddr - hugepage[i].physaddr) !=
		    hugepage[i].size)
			new_memseg = 1;
		else if (((unsigned long)hugepage[i-1].final_va -
		    (unsigned long)hugepage[i].final_va) != hugepage[i].size)
			new_memseg = 1;
#else
		else if ((hugepage[i].physaddr - hugepage[i-1].physaddr) !=
		    hugepage[i].size)
			new_memseg = 1;
		else if (((unsigned long)hugepage[i].final_va -
		    (unsigned long)hugepage[i-1].final_va) != hugepage[i].size)
			new_memseg = 1;
#endif

		if (new_memseg) {
			j += 1;
			if (j == RTE_MAX_MEMSEG)
				break;

			mcfg->memseg[j].phys_addr = hugepage[i].physaddr;
			mcfg->memseg[j].addr = hugepage[i].final_va;
			mcfg->memseg[j].len = hugepage[i].size;
			mcfg->memseg[j].socket_id = hugepage[i].socket_id;
			mcfg->memseg[j].hugepage_sz = hugepage[i].size;
		}
		/* continuation of previous memseg */
		else {
#ifdef RTE_ARCH_PPC_64
		/* Use the phy and virt address of the last page as segment
		 * address for IBM Power architecture */
			mcfg->memseg[j].phys_addr = hugepage[i].physaddr;
			mcfg->memseg[j].addr = hugepage[i].final_va;
#endif
			mcfg->memseg[j].len += mcfg->memseg[j].hugepage_sz;
		}
		hugepage[i].memseg_id = j;
	}

	if (i < nr_hugefiles) {
		RTE_LOG(ERR, EAL, "Can only reserve %d pages "
			"from %d requested\n"
			"Current %s=%d is not enough\n"
			"Please either increase it or request less amount "
			"of memory.\n",
			i, nr_hugefiles, RTE_STR(CONFIG_RTE_MAX_MEMSEG),
			RTE_MAX_MEMSEG);
		goto fail;
	}

	munmap(hugepage, nr_hugefiles * sizeof(struct hugepage_file));

	return 0;

fail:
	huge_recover_sigbus();
	free(tmp_hp);
	if (hugepage != NULL)
		munmap(hugepage, nr_hugefiles * sizeof(struct hugepage_file));

	return -1;
}

/*
 * uses fstat to report the size of a file on disk
 */
static off_t
getFileSize(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return 0;
	return st.st_size;
}

/*
 * This creates the memory mappings in the secondary process to match that of
 * the server process. It goes through each memory segment in the DPDK runtime
 * configuration and finds the hugepages which form that segment, mapping them
 * in order to form a contiguous block in the virtual memory space
 */
int
rte_eal_hugepage_attach(void)
{
	const struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct hugepage_file *hp = NULL;
	unsigned num_hp = 0;
	unsigned i, s = 0; /* s used to track the segment number */
	unsigned max_seg = RTE_MAX_MEMSEG;
	off_t size = 0;
	int fd, fd_zero = -1, fd_hugepage = -1;

	if (aslr_enabled() > 0) {
		RTE_LOG(WARNING, EAL, "WARNING: Address Space Layout Randomization "
				"(ASLR) is enabled in the kernel.\n");
		RTE_LOG(WARNING, EAL, "   This may cause issues with mapping memory "
				"into secondary processes\n");
	}

	test_phys_addrs_available();

	if (internal_config.xen_dom0_support) {
#ifdef RTE_LIBRTE_XEN_DOM0
		if (rte_xen_dom0_memory_attach() < 0) {
			RTE_LOG(ERR, EAL, "Failed to attach memory segments of primary "
					"process\n");
			return -1;
		}
		return 0;
#endif
	}

	fd_zero = open("/dev/zero", O_RDONLY);
	if (fd_zero < 0) {
		RTE_LOG(ERR, EAL, "Could not open /dev/zero\n");
		goto error;
	}
	fd_hugepage = open(eal_hugepage_info_path(), O_RDONLY);
	if (fd_hugepage < 0) {
		RTE_LOG(ERR, EAL, "Could not open %s\n", eal_hugepage_info_path());
		goto error;
	}

	/* map all segments into memory to make sure we get the addrs */
	for (s = 0; s < RTE_MAX_MEMSEG; ++s) {
		void *base_addr;

		/*
		 * the first memory segment with len==0 is the one that
		 * follows the last valid segment.
		 */
		if (mcfg->memseg[s].len == 0)
			break;

		/*
		 * fdzero is mmapped to get a contiguous block of virtual
		 * addresses of the appropriate memseg size.
		 * use mmap to get identical addresses as the primary process.
		 */
		base_addr = mmap(mcfg->memseg[s].addr, mcfg->memseg[s].len,
				 PROT_READ,
#ifdef RTE_ARCH_PPC_64
				 MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
#else
				 MAP_PRIVATE,
#endif
				 fd_zero, 0);
		if (base_addr == MAP_FAILED ||
		    base_addr != mcfg->memseg[s].addr) {
			max_seg = s;
			if (base_addr != MAP_FAILED) {
				/* errno is stale, don't use */
				RTE_LOG(ERR, EAL, "Could not mmap %llu bytes "
					"in /dev/zero at [%p], got [%p] - "
					"please use '--base-virtaddr' option\n",
					(unsigned long long)mcfg->memseg[s].len,
					mcfg->memseg[s].addr, base_addr);
				munmap(base_addr, mcfg->memseg[s].len);
			} else {
				RTE_LOG(ERR, EAL, "Could not mmap %llu bytes "
					"in /dev/zero at [%p]: '%s'\n",
					(unsigned long long)mcfg->memseg[s].len,
					mcfg->memseg[s].addr, strerror(errno));
			}
			if (aslr_enabled() > 0) {
				RTE_LOG(ERR, EAL, "It is recommended to "
					"disable ASLR in the kernel "
					"and retry running both primary "
					"and secondary processes\n");
			}
			goto error;
		}
	}

	size = getFileSize(fd_hugepage);
	hp = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd_hugepage, 0);
	if (hp == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "Could not mmap %s\n", eal_hugepage_info_path());
		goto error;
	}

	num_hp = size / sizeof(struct hugepage_file);
	RTE_LOG(DEBUG, EAL, "Analysing %u files\n", num_hp);

	s = 0;
	while (s < RTE_MAX_MEMSEG && mcfg->memseg[s].len > 0){
		void *addr, *base_addr;
		uintptr_t offset = 0;
		size_t mapping_size;
		/*
		 * free previously mapped memory so we can map the
		 * hugepages into the space
		 */
		base_addr = mcfg->memseg[s].addr;
		munmap(base_addr, mcfg->memseg[s].len);

		/* find the hugepages for this segment and map them
		 * we don't need to worry about order, as the server sorted the
		 * entries before it did the second mmap of them */
		for (i = 0; i < num_hp && offset < mcfg->memseg[s].len; i++){
			if (hp[i].memseg_id == (int)s){
				fd = open(hp[i].filepath, O_RDWR);
				if (fd < 0) {
					RTE_LOG(ERR, EAL, "Could not open %s\n",
						hp[i].filepath);
					goto error;
				}
				mapping_size = hp[i].size;
				addr = mmap(RTE_PTR_ADD(base_addr, offset),
						mapping_size, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0);
				close(fd); /* close file both on success and on failure */
				if (addr == MAP_FAILED ||
						addr != RTE_PTR_ADD(base_addr, offset)) {
					RTE_LOG(ERR, EAL, "Could not mmap %s\n",
						hp[i].filepath);
					goto error;
				}
				offset+=mapping_size;
			}
		}
		RTE_LOG(DEBUG, EAL, "Mapped segment %u of size 0x%llx\n", s,
				(unsigned long long)mcfg->memseg[s].len);
		s++;
	}
	/* unmap the hugepage config file, since we are done using it */
	munmap(hp, size);
	close(fd_zero);
	close(fd_hugepage);
	return 0;

error:
	for (i = 0; i < max_seg && mcfg->memseg[i].len > 0; i++)
		munmap(mcfg->memseg[i].addr, mcfg->memseg[i].len);
	if (hp != NULL && hp != MAP_FAILED)
		munmap(hp, size);
	if (fd_zero >= 0)
		close(fd_zero);
	if (fd_hugepage >= 0)
		close(fd_hugepage);
	return -1;
}

bool
rte_eal_using_phys_addrs(void)
{
	return phys_addrs_available;
}
