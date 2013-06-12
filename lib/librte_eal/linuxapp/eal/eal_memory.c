/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   Copyright(c) 2012-2013 6WIND.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#define _FILE_OFFSET_BITS 64
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
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_fs_paths.h"
#include "eal_hugepages.h"

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


#define RANDOMIZE_VA_SPACE_FILE "/proc/sys/kernel/randomize_va_space"

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
 * Try to mmap *size bytes in /dev/zero. If it is succesful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by hugepage_sz until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of hugepage size.
 */
static void *
get_virtual_area(uint64_t *size, uint64_t hugepage_sz)
{
	void *addr;
	int fd;
	long aligned_addr;

	RTE_LOG(INFO, EAL, "Ask a virtual area of 0x%"PRIx64" bytes\n", *size);

	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0){
		RTE_LOG(ERR, EAL, "Cannot open /dev/zero\n");
		return NULL;
	}
	do {
		addr = mmap(NULL, (*size) + hugepage_sz, PROT_READ, MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED)
			*size -= hugepage_sz;
	} while (addr == MAP_FAILED && *size > 0);

	if (addr == MAP_FAILED) {
		close(fd);
		RTE_LOG(INFO, EAL, "Cannot get a virtual area\n");
		return NULL;
	}

	munmap(addr, (*size) + hugepage_sz);
	close(fd);

	/* align addr to a huge page size boundary */
	aligned_addr = (long)addr;
	aligned_addr += (hugepage_sz - 1);
	aligned_addr &= (~(hugepage_sz - 1));
	addr = (void *)(aligned_addr);

	RTE_LOG(INFO, EAL, "Virtual area found at %p (size = 0x%"PRIx64")\n",
		addr, *size);

	return addr;
}

/*
 * Mmap all hugepages of hugepage table: it first open a file in
 * hugetlbfs, then mmap() hugepage_sz data in it. If orig is set, the
 * virtual address is stored in hugepg_tbl[i].orig_va, else it is stored
 * in hugepg_tbl[i].final_va. The second mapping (when orig is 0) tries to
 * map continguous physical blocks in contiguous virtual blocks.
 */
static int
map_all_hugepages(struct hugepage *hugepg_tbl,
		struct hugepage_info *hpi, int orig)
{
	int fd;
	unsigned i;
	void *virtaddr;
	void *vma_addr = NULL;
	uint64_t vma_len = 0;

	for (i = 0; i < hpi->num_pages; i++) {
		uint64_t hugepage_sz = hpi->hugepage_sz;

		if (orig) {
			hugepg_tbl[i].file_id = i;
			hugepg_tbl[i].size = hugepage_sz;
			eal_get_hugefile_path(hugepg_tbl[i].filepath,
					sizeof(hugepg_tbl[i].filepath), hpi->hugedir,
					hugepg_tbl[i].file_id);
			hugepg_tbl[i].filepath[sizeof(hugepg_tbl[i].filepath) - 1] = '\0';
		}
#ifndef RTE_ARCH_X86_64
		/* for 32-bit systems, don't remap 1G pages, just reuse original
		 * map address as final map address.
		 */
		else if (hugepage_sz == RTE_PGSIZE_1G){
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
			for (j = i+1; j < hpi->num_pages ; j++) {
				if (hugepg_tbl[j].physaddr !=
				    hugepg_tbl[j-1].physaddr + hugepage_sz)
					break;
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

		fd = open(hugepg_tbl[i].filepath, O_CREAT | O_RDWR, 0755);
		if (fd < 0) {
			RTE_LOG(ERR, EAL, "%s(): open failed: %s", __func__,
					strerror(errno));
			return -1;
		}

		virtaddr = mmap(vma_addr, hugepage_sz, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0);
		if (virtaddr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "%s(): mmap failed: %s", __func__,
					strerror(errno));
			close(fd);
			return -1;
		}
		if (orig) {
			hugepg_tbl[i].orig_va = virtaddr;
			memset(virtaddr, 0, hugepage_sz);
		}
		else {
			hugepg_tbl[i].final_va = virtaddr;
		}

		vma_addr = (char *)vma_addr + hugepage_sz;
		vma_len -= hugepage_sz;
		close(fd);
	}
	return 0;
}

/* Unmap all hugepages from original mapping. */
static int
unmap_all_hugepages_orig(struct hugepage *hugepg_tbl, struct hugepage_info *hpi)
{
	unsigned i;
	for (i = 0; i < hpi->num_pages; i++) {
		if (hugepg_tbl[i].orig_va) {
			munmap(hugepg_tbl[i].orig_va, hpi->hugepage_sz);
			hugepg_tbl[i].orig_va = NULL;
		}
	}
	return 0;
}

/* Lock page in physical memory and prevent from swapping. */
int
rte_mem_lock_page(const void *virt)
{
	unsigned long virtual = (unsigned long)virt;
	int page_size = getpagesize();
	unsigned long aligned = (virtual & ~ (page_size - 1));
	return mlock((void*)aligned, page_size);
}

/*
 * Get physical address of any mapped virtual address in the current process.
 */
phys_addr_t
rte_mem_virt2phy(const void *virt)
{
	int fdmem;
	uint64_t page;
	off_t offset;
	unsigned long virtual = (unsigned long)virt;
	int page_size = getpagesize();

	fdmem = open("/proc/self/pagemap", O_RDONLY);
	if (fdmem < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot open /proc/self/pagemap: %s\n",
		                  __func__, strerror(errno));
		return RTE_BAD_PHYS_ADDR;
	}
	offset = (off_t) (virtual / page_size) * sizeof(uint64_t);
	if (lseek(fdmem, offset, SEEK_SET) == (off_t) -1) {
		RTE_LOG(ERR, EAL, "%s(): seek error in /proc/self/pagemap: %s\n",
		                  __func__, strerror(errno));
		close(fdmem);
		return RTE_BAD_PHYS_ADDR;
	}
	if (read(fdmem, &page, sizeof(uint64_t)) <= 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot read /proc/self/pagemap: %s\n",
		                  __func__, strerror(errno));
		close(fdmem);
		return RTE_BAD_PHYS_ADDR;
	}
	close (fdmem);

	/* pfn (page frame number) are bits 0-54 (see pagemap.txt in Linux doc) */
	return ((page & 0x7fffffffffffffULL) * page_size) + (virtual % page_size);
}

/*
 * For each hugepage in hugepg_tbl, fill the physaddr value.
 */
static int
find_physaddr(struct hugepage *hugepg_tbl, struct hugepage_info *hpi)
{
	unsigned i;

	for (i = 0; i < hpi->num_pages; i++) {
		hugepg_tbl[i].physaddr = rte_mem_virt2phy(hugepg_tbl[i].orig_va);
		if (hugepg_tbl[i].physaddr == RTE_BAD_PHYS_ADDR)
			return -1;
	}
	return 0;
}

/*
 * Parse /proc/self/numa_maps to get the NUMA socket ID for each huge
 * page.
 */
static int
find_numasocket(struct hugepage *hugepg_tbl, struct hugepage_info *hpi)
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
		RTE_LOG(INFO, EAL, "cannot open /proc/self/numa_maps,"
				"consider that all memory is in socket_id 0");
		return 0;
	}

	rte_snprintf(hugedir_str, sizeof(hugedir_str),
			"%s/", hpi->hugedir);

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
		for (i = 0; i < hpi->num_pages; i++) {
			void *va = (void *)(unsigned long)virt_addr;
			if (hugepg_tbl[i].orig_va == va) {
				hugepg_tbl[i].socket_id = socket_id;
				hp_count++;
			}
		}
	}
	if (hp_count < hpi->num_pages)
		goto error;
	fclose(f);
	return 0;

error:
	fclose(f);
	return -1;
}

/*
 * Sort the hugepg_tbl by physical address (lower addresses first). We
 * use a slow algorithm, but we won't have millions of pages, and this
 * is only done at init time.
 */
static int
sort_by_physaddr(struct hugepage *hugepg_tbl, struct hugepage_info *hpi)
{
	unsigned i, j;
	int smallest_idx;
	uint64_t smallest_addr;
	struct hugepage tmp;

	for (i = 0; i < hpi->num_pages; i++) {
		smallest_addr = 0;
		smallest_idx = -1;

		/*
		 * browse all entries starting at 'i', and find the
		 * entry with the smallest addr
		 */
		for (j=i; j<hpi->num_pages; j++) {

			if (smallest_addr == 0 ||
			    hugepg_tbl[j].physaddr < smallest_addr) {
				smallest_addr = hugepg_tbl[j].physaddr;
				smallest_idx = j;
			}
		}

		/* should not happen */
		if (smallest_idx == -1) {
			RTE_LOG(ERR, EAL, "%s(): error in physaddr sorting\n", __func__);
			return -1;
		}

		/* swap the 2 entries in the table */
		memcpy(&tmp, &hugepg_tbl[smallest_idx], sizeof(struct hugepage));
		memcpy(&hugepg_tbl[smallest_idx], &hugepg_tbl[i],
				sizeof(struct hugepage));
		memcpy(&hugepg_tbl[i], &tmp, sizeof(struct hugepage));
	}
	return 0;
}

/*
 * Uses mmap to create a shared memory area for storage of data
 *Used in this file to store the hugepage file map on disk
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
 * This function takes in the list of hugepage sizes and the
 * number of pages thereof, and calculates the best number of
 * pages of each size to fulfill the request for <memory> ram
 */
static int
calc_num_pages(uint64_t memory,
		struct hugepage_info *hp_info,
		struct hugepage_info *hp_used,
		unsigned num_hp_info)
{
	unsigned i = 0;
	int total_num_pages = 0;
	if (num_hp_info == 0)
		return -1;

	for (i = 0; i < num_hp_info; i++){
		hp_used[i].hugepage_sz = hp_info[i].hugepage_sz;
		hp_used[i].hugedir = hp_info[i].hugedir;
		hp_used[i].num_pages = RTE_MIN(memory / hp_info[i].hugepage_sz,
				hp_info[i].num_pages);

		memory -= hp_used[i].num_pages * hp_used[i].hugepage_sz;
		total_num_pages += hp_used[i].num_pages;

		/* check if we have met all memory requests */
		if (memory == 0)
			break;
		/* check if we have any more pages left at this size, if so
		 * move on to next size */
		if (hp_used[i].num_pages == hp_info[i].num_pages)
			continue;
		/* At this point we know that there are more pages available that are
		 * bigger than the memory we want, so lets see if we can get enough
		 * from other page sizes.
		 */
		unsigned j;
		uint64_t remaining_mem = 0;
		for (j = i+1; j < num_hp_info; j++)
			remaining_mem += hp_info[j].hugepage_sz * hp_info[j].num_pages;

		/* is there enough other memory, if not allocate another page and quit*/
		if (remaining_mem < memory){
			memory -= hp_info[i].hugepage_sz;
			hp_used[i].num_pages++;
			total_num_pages++;
			break; /* we are done */
		}
	}
	return total_num_pages;
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
static int
rte_eal_hugepage_init(void)
{
	struct rte_mem_config *mcfg;
	struct hugepage *hugepage;
	struct hugepage_info used_hp[MAX_HUGEPAGE_SIZES];
	int i, j, new_memseg;
	int nrpages;
	void *addr;

	memset(used_hp, 0, sizeof(used_hp));

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* for debug purposes, hugetlbfs can be disabled */
	if (internal_config.no_hugetlbfs) {
		addr = malloc(internal_config.memory);
		mcfg->memseg[0].phys_addr = (unsigned long)addr;
		mcfg->memseg[0].addr = addr;
		mcfg->memseg[0].len = internal_config.memory;
		mcfg->memseg[0].socket_id = 0;
		return 0;
	}

	nrpages = calc_num_pages(internal_config.memory,
			&internal_config.hugepage_info[0], &used_hp[0],
			internal_config.num_hugepage_sizes);
	for (i = 0; i < (int)internal_config.num_hugepage_sizes; i++)
		RTE_LOG(INFO, EAL, "Requesting %u pages of size %"PRIu64"\n",
				used_hp[i].num_pages, used_hp[i].hugepage_sz);

	hugepage = create_shared_memory(eal_hugepage_info_path(),
			nrpages * sizeof(struct hugepage));
	if (hugepage == NULL)
		return -1;
	memset(hugepage, 0, nrpages * sizeof(struct hugepage));

	unsigned hp_offset = 0; /* where we start the current page size entries */
	for (i = 0; i < (int)internal_config.num_hugepage_sizes; i ++){
		struct hugepage_info *hpi = &used_hp[i];
		if (hpi->num_pages == 0)
			continue;

		if (map_all_hugepages(&hugepage[hp_offset], hpi, 1) < 0){
			RTE_LOG(DEBUG, EAL, "Failed to mmap %u MB hugepages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		if (find_physaddr(&hugepage[hp_offset], hpi) < 0){
			RTE_LOG(DEBUG, EAL, "Failed to find phys addr for %u MB pages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		if (find_numasocket(&hugepage[hp_offset], hpi) < 0){
			RTE_LOG(DEBUG, EAL, "Failed to find NUMA socket for %u MB pages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		if (sort_by_physaddr(&hugepage[hp_offset], hpi) < 0)
			goto fail;

		if (map_all_hugepages(&hugepage[hp_offset], hpi, 0) < 0){
			RTE_LOG(DEBUG, EAL, "Failed to remap %u MB pages\n",
					(unsigned)(hpi->hugepage_sz / 0x100000));
			goto fail;
		}

		if (unmap_all_hugepages_orig(&hugepage[hp_offset], hpi) < 0)
			goto fail;

		/* we have processed a num of hugepages of this size, so inc offset */
		hp_offset += hpi->num_pages;
	}

	memset(mcfg->memseg, 0, sizeof(mcfg->memseg));
	j = -1;
	for (i = 0; i < nrpages; i++) {
		new_memseg = 0;

		/* if this is a new section, create a new memseg */
		if (i == 0)
			new_memseg = 1;
		else if (hugepage[i].socket_id != hugepage[i-1].socket_id)
			new_memseg = 1;
		else if (hugepage[i].size != hugepage[i-1].size)
			new_memseg = 1;
		else if ((hugepage[i].physaddr - hugepage[i-1].physaddr) !=
			 hugepage[i].size)
			new_memseg = 1;
		else if (((unsigned long)hugepage[i].final_va -
		     (unsigned long)hugepage[i-1].final_va) != hugepage[i].size)
			new_memseg = 1;

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
			mcfg->memseg[j].len += mcfg->memseg[j].hugepage_sz;
		}
		hugepage[i].memseg_id = j;
	}

	return 0;


 fail:
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
static int
rte_eal_hugepage_attach(void)
{
	const struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	const struct hugepage *hp = NULL;
	unsigned num_hp = 0;
	unsigned i, s = 0; /* s used to track the segment number */
	off_t size;
	int fd, fd_zero = -1, fd_hugepage = -1;

	if (aslr_enabled() > 0) {
		RTE_LOG(WARNING, EAL, "WARNING: Address Space Layout Randomization "
				"(ASLR) is enabled in the kernel.\n");
		RTE_LOG(WARNING, EAL, "   This may cause issues with mapping memory "
				"into secondary processes\n");
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

	size = getFileSize(fd_hugepage);
	hp = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd_hugepage, 0);
	if (hp == NULL) {
		RTE_LOG(ERR, EAL, "Could not mmap %s\n", eal_hugepage_info_path());
		goto error;
	}

	num_hp = size / sizeof(struct hugepage);
	RTE_LOG(DEBUG, EAL, "Analysing %u hugepages\n", num_hp);

	while (s < RTE_MAX_MEMSEG && mcfg->memseg[s].len > 0){
		void *addr, *base_addr;
		uintptr_t offset = 0;

		/* fdzero is mmapped to get a contiguous block of virtual addresses
		 * get a block of free memory of the appropriate size -
		 * use mmap to attempt to get an identical address as server.
		 */
		base_addr = mmap(mcfg->memseg[s].addr, mcfg->memseg[s].len,
				PROT_READ, MAP_PRIVATE, fd_zero, 0);
		if (base_addr == MAP_FAILED || base_addr != mcfg->memseg[s].addr) {
			RTE_LOG(ERR, EAL, "Could not mmap %llu bytes "
				"in /dev/zero to requested address [%p]\n",
				(unsigned long long)mcfg->memseg[s].len,
				mcfg->memseg[s].addr);
			if (aslr_enabled() > 0)
				RTE_LOG(ERR, EAL, "It is recommended to disable ASLR in the kernel "
						"and retry running both primary and secondary processes\n");
			goto error;
		}
		/* free memory so we can map the hugepages into the space */
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
				addr = mmap(RTE_PTR_ADD(base_addr, offset),
						hp[i].size, PROT_READ | PROT_WRITE,
						MAP_SHARED | MAP_FIXED, fd, 0);
				close(fd); /* close file both on success and on failure */
				if (addr == MAP_FAILED) {
					RTE_LOG(ERR, EAL, "Could not mmap %s\n",
						hp[i].filepath);
					goto error;
				}
				offset+=hp[i].size;
			}
		}
		RTE_LOG(DEBUG, EAL, "Mapped segment %u of size 0x%llx\n", s,
				(unsigned long long)mcfg->memseg[s].len);
		s++;
	}
	close(fd_zero);
	close(fd_hugepage);
	return 0;

error:
	if (fd_zero >= 0)
		close(fd_zero);
	if (fd_hugepage >= 0)
		close(fd_hugepage);
	return -1;
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


/* init memory subsystem */
int
rte_eal_memory_init(void)
{
	const int retval = rte_eal_process_type() == RTE_PROC_PRIMARY ?
			rte_eal_hugepage_init() :
			rte_eal_hugepage_attach();
	if (retval < 0)
		return -1;

	if (internal_config.no_shconf == 0 && rte_eal_memdevice_init() < 0)
		return -1;

	return 0;
}
