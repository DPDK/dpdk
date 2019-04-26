/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2013 6WIND S.A.
 */

#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#ifdef F_ADD_SEALS /* if file sealing is supported, so is memfd */
#include <linux/memfd.h>
#define MEMFD_SUPPORTED
#endif
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
#include <numa.h>
#include <numaif.h>
#endif

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "eal_private.h"
#include "eal_memalloc.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_hugepages.h"
#include "eal_options.h"

#define PFN_MASK_SIZE	8

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

static bool phys_addrs_available = true;

#define RANDOMIZE_VA_SPACE_FILE "/proc/sys/kernel/randomize_va_space"

static void
test_phys_addrs_available(void)
{
	uint64_t tmp = 0;
	phys_addr_t physaddr;

	if (!rte_eal_has_hugepages()) {
		RTE_LOG(ERR, EAL,
			"Started without hugepages support, physical addresses not available\n");
		phys_addrs_available = false;
		return;
	}

	physaddr = rte_mem_virt2phy(&tmp);
	if (physaddr == RTE_BAD_PHYS_ADDR) {
		if (rte_eal_iova_mode() == RTE_IOVA_PA)
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

	/* Cannot parse /proc/self/pagemap, no need to log errors everywhere */
	if (!phys_addrs_available)
		return RTE_BAD_IOVA;

	/* standard page size */
	page_size = getpagesize();

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0) {
		RTE_LOG(INFO, EAL, "%s(): cannot open /proc/self/pagemap: %s\n",
			__func__, strerror(errno));
		return RTE_BAD_IOVA;
	}

	virt_pfn = (unsigned long)virtaddr / page_size;
	offset = sizeof(uint64_t) * virt_pfn;
	if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
		RTE_LOG(INFO, EAL, "%s(): seek error in /proc/self/pagemap: %s\n",
				__func__, strerror(errno));
		close(fd);
		return RTE_BAD_IOVA;
	}

	retval = read(fd, &page, PFN_MASK_SIZE);
	close(fd);
	if (retval < 0) {
		RTE_LOG(INFO, EAL, "%s(): cannot read /proc/self/pagemap: %s\n",
				__func__, strerror(errno));
		return RTE_BAD_IOVA;
	} else if (retval != PFN_MASK_SIZE) {
		RTE_LOG(INFO, EAL, "%s(): read %d bytes from /proc/self/pagemap "
				"but expected %d:\n",
				__func__, retval, PFN_MASK_SIZE);
		return RTE_BAD_IOVA;
	}

	/*
	 * the pfn (page frame number) are bits 0-54 (see
	 * pagemap.txt in linux Documentation)
	 */
	if ((page & 0x7fffffffffffffULL) == 0)
		return RTE_BAD_IOVA;

	physaddr = ((page & 0x7fffffffffffffULL) * page_size)
		+ ((unsigned long)virtaddr % page_size);

	return physaddr;
}

rte_iova_t
rte_mem_virt2iova(const void *virtaddr)
{
	if (rte_eal_iova_mode() == RTE_IOVA_VA)
		return (uintptr_t)virtaddr;
	return rte_mem_virt2phy(virtaddr);
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

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
/* Callback for numa library. */
void numa_error(char *where)
{
	RTE_LOG(ERR, EAL, "%s failed: %s\n", where, strerror(errno));
}
#endif

/*
 * Mmap all hugepages of hugepage table: it first open a file in
 * hugetlbfs, then mmap() hugepage_sz data in it. If orig is set, the
 * virtual address is stored in hugepg_tbl[i].orig_va, else it is stored
 * in hugepg_tbl[i].final_va. The second mapping (when orig is 0) tries to
 * map contiguous physical blocks in contiguous virtual blocks.
 */
static unsigned
map_all_hugepages(struct hugepage_file *hugepg_tbl, struct hugepage_info *hpi,
		  uint64_t *essential_memory __rte_unused)
{
	int fd;
	unsigned i;
	void *virtaddr;
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	int node_id = -1;
	int essential_prev = 0;
	int oldpolicy;
	struct bitmask *oldmask = NULL;
	bool have_numa = true;
	unsigned long maxnode = 0;

	/* Check if kernel supports NUMA. */
	if (numa_available() != 0) {
		RTE_LOG(DEBUG, EAL, "NUMA is not supported.\n");
		have_numa = false;
	}

	if (have_numa) {
		RTE_LOG(DEBUG, EAL, "Trying to obtain current memory policy.\n");
		oldmask = numa_allocate_nodemask();
		if (get_mempolicy(&oldpolicy, oldmask->maskp,
				  oldmask->size + 1, 0, 0) < 0) {
			RTE_LOG(ERR, EAL,
				"Failed to get current mempolicy: %s. "
				"Assuming MPOL_DEFAULT.\n", strerror(errno));
			oldpolicy = MPOL_DEFAULT;
		}
		for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
			if (internal_config.socket_mem[i])
				maxnode = i + 1;
	}
#endif

	for (i = 0; i < hpi->num_pages[0]; i++) {
		struct hugepage_file *hf = &hugepg_tbl[i];
		uint64_t hugepage_sz = hpi->hugepage_sz;

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
		if (maxnode) {
			unsigned int j;

			for (j = 0; j < maxnode; j++)
				if (essential_memory[j])
					break;

			if (j == maxnode) {
				node_id = (node_id + 1) % maxnode;
				while (!internal_config.socket_mem[node_id]) {
					node_id++;
					node_id %= maxnode;
				}
				essential_prev = 0;
			} else {
				node_id = j;
				essential_prev = essential_memory[j];

				if (essential_memory[j] < hugepage_sz)
					essential_memory[j] = 0;
				else
					essential_memory[j] -= hugepage_sz;
			}

			RTE_LOG(DEBUG, EAL,
				"Setting policy MPOL_PREFERRED for socket %d\n",
				node_id);
			numa_set_preferred(node_id);
		}
#endif

		hf->file_id = i;
		hf->size = hugepage_sz;
		eal_get_hugefile_path(hf->filepath, sizeof(hf->filepath),
				hpi->hugedir, hf->file_id);
		hf->filepath[sizeof(hf->filepath) - 1] = '\0';

		/* try to create hugepage file */
		fd = open(hf->filepath, O_CREAT | O_RDWR, 0600);
		if (fd < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n", __func__,
					strerror(errno));
			goto out;
		}

		/* map the segment, and populate page tables,
		 * the kernel fills this segment with zeros. we don't care where
		 * this gets mapped - we already have contiguous memory areas
		 * ready for us to map into.
		 */
		virtaddr = mmap(NULL, hugepage_sz, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_POPULATE, fd, 0);
		if (virtaddr == MAP_FAILED) {
			RTE_LOG(DEBUG, EAL, "%s(): mmap failed: %s\n", __func__,
					strerror(errno));
			close(fd);
			goto out;
		}

		hf->orig_va = virtaddr;

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
				(unsigned int)(hugepage_sz / 0x100000));
			munmap(virtaddr, hugepage_sz);
			close(fd);
			unlink(hugepg_tbl[i].filepath);
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
			if (maxnode)
				essential_memory[node_id] =
					essential_prev;
#endif
			goto out;
		}
		*(int *)virtaddr = 0;

		/* set shared lock on the file. */
		if (flock(fd, LOCK_SH) < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): Locking file failed:%s \n",
				__func__, strerror(errno));
			close(fd);
			goto out;
		}

		close(fd);
	}

out:
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	if (maxnode) {
		RTE_LOG(DEBUG, EAL,
			"Restoring previous memory policy: %d\n", oldpolicy);
		if (oldpolicy == MPOL_DEFAULT) {
			numa_set_localalloc();
		} else if (set_mempolicy(oldpolicy, oldmask->maskp,
					 oldmask->size + 1) < 0) {
			RTE_LOG(ERR, EAL, "Failed to restore mempolicy: %s\n",
				strerror(errno));
			numa_set_localalloc();
		}
	}
	if (oldmask != NULL)
		numa_free_cpumask(oldmask);
#endif
	return i;
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
		RTE_LOG(NOTICE, EAL, "NUMA support not available"
			" consider that all memory is in socket_id 0\n");
		return 0;
	}

	snprintf(hugedir_str, sizeof(hugedir_str),
			"%s/%s", hpi->hugedir, eal_get_hugefile_prefix());

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
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
				RTE_LOG(DEBUG, EAL,
					"Hugepage %s is on socket %d\n",
					hugepg_tbl[i].filepath, socket_id);
#endif
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
	int fd;

	/* if no shared files mode is used, create anonymous memory instead */
	if (internal_config.no_shconf) {
		retval = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (retval == MAP_FAILED)
			return NULL;
		return retval;
	}

	fd = open(filename, O_CREAT | O_RDWR, 0600);
	if (fd < 0)
		return NULL;
	if (ftruncate(fd, mem_size) < 0) {
		close(fd);
		return NULL;
	}
	retval = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (retval == MAP_FAILED)
		return NULL;
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
		if (src[src_pos].orig_va != NULL) {
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

		if (hp->orig_va != NULL && unlink(hp->filepath)) {
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
						munmap(hp->orig_va,
							(size_t)unmap_len);

						hp->orig_va = NULL;
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

static int
remap_segment(struct hugepage_file *hugepages, int seg_start, int seg_end)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *msl;
	struct rte_fbarray *arr;
	int cur_page, seg_len;
	unsigned int msl_idx;
	int ms_idx;
	uint64_t page_sz;
	size_t memseg_len;
	int socket_id;

	page_sz = hugepages[seg_start].size;
	socket_id = hugepages[seg_start].socket_id;
	seg_len = seg_end - seg_start;

	RTE_LOG(DEBUG, EAL, "Attempting to map %" PRIu64 "M on socket %i\n",
			(seg_len * page_sz) >> 20ULL, socket_id);

	/* find free space in memseg lists */
	for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS; msl_idx++) {
		bool empty;
		msl = &mcfg->memsegs[msl_idx];
		arr = &msl->memseg_arr;

		if (msl->page_sz != page_sz)
			continue;
		if (msl->socket_id != socket_id)
			continue;

		/* leave space for a hole if array is not empty */
		empty = arr->count == 0;
		ms_idx = rte_fbarray_find_next_n_free(arr, 0,
				seg_len + (empty ? 0 : 1));

		/* memseg list is full? */
		if (ms_idx < 0)
			continue;

		/* leave some space between memsegs, they are not IOVA
		 * contiguous, so they shouldn't be VA contiguous either.
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

#ifdef RTE_ARCH_PPC64
	/* for PPC64 we go through the list backwards */
	for (cur_page = seg_end - 1; cur_page >= seg_start;
			cur_page--, ms_idx++) {
#else
	for (cur_page = seg_start; cur_page < seg_end; cur_page++, ms_idx++) {
#endif
		struct hugepage_file *hfile = &hugepages[cur_page];
		struct rte_memseg *ms = rte_fbarray_get(arr, ms_idx);
		void *addr;
		int fd;

		fd = open(hfile->filepath, O_RDWR);
		if (fd < 0) {
			RTE_LOG(ERR, EAL, "Could not open '%s': %s\n",
					hfile->filepath, strerror(errno));
			return -1;
		}
		/* set shared lock on the file. */
		if (flock(fd, LOCK_SH) < 0) {
			RTE_LOG(DEBUG, EAL, "Could not lock '%s': %s\n",
					hfile->filepath, strerror(errno));
			close(fd);
			return -1;
		}
		memseg_len = (size_t)page_sz;
		addr = RTE_PTR_ADD(msl->base_va, ms_idx * memseg_len);

		/* we know this address is already mmapped by memseg list, so
		 * using MAP_FIXED here is safe
		 */
		addr = mmap(addr, page_sz, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, 0);
		if (addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "Couldn't remap '%s': %s\n",
					hfile->filepath, strerror(errno));
			close(fd);
			return -1;
		}

		/* we have a new address, so unmap previous one */
#ifndef RTE_ARCH_64
		/* in 32-bit legacy mode, we have already unmapped the page */
		if (!internal_config.legacy_mem)
			munmap(hfile->orig_va, page_sz);
#else
		munmap(hfile->orig_va, page_sz);
#endif

		hfile->orig_va = NULL;
		hfile->final_va = addr;

		/* rewrite physical addresses in IOVA as VA mode */
		if (rte_eal_iova_mode() == RTE_IOVA_VA)
			hfile->physaddr = (uintptr_t)addr;

		/* set up memseg data */
		ms->addr = addr;
		ms->hugepage_sz = page_sz;
		ms->len = memseg_len;
		ms->iova = hfile->physaddr;
		ms->socket_id = hfile->socket_id;
		ms->nchannel = rte_memory_get_nchannel();
		ms->nrank = rte_memory_get_nrank();

		rte_fbarray_set_used(arr, ms_idx);

		/* store segment fd internally */
		if (eal_memalloc_set_seg_fd(msl_idx, ms_idx, fd) < 0)
			RTE_LOG(ERR, EAL, "Could not store segment fd: %s\n",
				rte_strerror(rte_errno));
	}
	RTE_LOG(DEBUG, EAL, "Allocated %" PRIu64 "M on socket %i\n",
			(seg_len * page_sz) >> 20, socket_id);
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
	msl->len = mem_sz;

	return 0;
}

/*
 * Our VA space is not preallocated yet, so preallocate it here. We need to know
 * how many segments there are in order to map all pages into one address space,
 * and leave appropriate holes between segments so that rte_malloc does not
 * concatenate them into one big segment.
 *
 * we also need to unmap original pages to free up address space.
 */
static int __rte_unused
prealloc_segments(struct hugepage_file *hugepages, int n_pages)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int cur_page, seg_start_page, end_seg, new_memseg;
	unsigned int hpi_idx, socket, i;
	int n_contig_segs, n_segs;
	int msl_idx;

	/* before we preallocate segments, we need to free up our VA space.
	 * we're not removing files, and we already have information about
	 * PA-contiguousness, so it is safe to unmap everything.
	 */
	for (cur_page = 0; cur_page < n_pages; cur_page++) {
		struct hugepage_file *hpi = &hugepages[cur_page];
		munmap(hpi->orig_va, hpi->size);
		hpi->orig_va = NULL;
	}

	/* we cannot know how many page sizes and sockets we have discovered, so
	 * loop over all of them
	 */
	for (hpi_idx = 0; hpi_idx < internal_config.num_hugepage_sizes;
			hpi_idx++) {
		uint64_t page_sz =
			internal_config.hugepage_info[hpi_idx].hugepage_sz;

		for (i = 0; i < rte_socket_count(); i++) {
			struct rte_memseg_list *msl;

			socket = rte_socket_id_by_idx(i);
			n_contig_segs = 0;
			n_segs = 0;
			seg_start_page = -1;

			for (cur_page = 0; cur_page < n_pages; cur_page++) {
				struct hugepage_file *prev, *cur;
				int prev_seg_start_page = -1;

				cur = &hugepages[cur_page];
				prev = cur_page == 0 ? NULL :
						&hugepages[cur_page - 1];

				new_memseg = 0;
				end_seg = 0;

				if (cur->size == 0)
					end_seg = 1;
				else if (cur->socket_id != (int) socket)
					end_seg = 1;
				else if (cur->size != page_sz)
					end_seg = 1;
				else if (cur_page == 0)
					new_memseg = 1;
#ifdef RTE_ARCH_PPC_64
				/* On PPC64 architecture, the mmap always start
				 * from higher address to lower address. Here,
				 * physical addresses are in descending order.
				 */
				else if ((prev->physaddr - cur->physaddr) !=
						cur->size)
					new_memseg = 1;
#else
				else if ((cur->physaddr - prev->physaddr) !=
						cur->size)
					new_memseg = 1;
#endif
				if (new_memseg) {
					/* if we're already inside a segment,
					 * new segment means end of current one
					 */
					if (seg_start_page != -1) {
						end_seg = 1;
						prev_seg_start_page =
								seg_start_page;
					}
					seg_start_page = cur_page;
				}

				if (end_seg) {
					if (prev_seg_start_page != -1) {
						/* we've found a new segment */
						n_contig_segs++;
						n_segs += cur_page -
							prev_seg_start_page;
					} else if (seg_start_page != -1) {
						/* we didn't find new segment,
						 * but did end current one
						 */
						n_contig_segs++;
						n_segs += cur_page -
								seg_start_page;
						seg_start_page = -1;
						continue;
					} else {
						/* we're skipping this page */
						continue;
					}
				}
				/* segment continues */
			}
			/* check if we missed last segment */
			if (seg_start_page != -1) {
				n_contig_segs++;
				n_segs += cur_page - seg_start_page;
			}

			/* if no segments were found, do not preallocate */
			if (n_segs == 0)
				continue;

			/* we now have total number of pages that we will
			 * allocate for this segment list. add separator pages
			 * to the total count, and preallocate VA space.
			 */
			n_segs += n_contig_segs - 1;

			/* now, preallocate VA space for these segments */

			/* first, find suitable memseg list for this */
			for (msl_idx = 0; msl_idx < RTE_MAX_MEMSEG_LISTS;
					msl_idx++) {
				msl = &mcfg->memsegs[msl_idx];

				if (msl->base_va != NULL)
					continue;
				break;
			}
			if (msl_idx == RTE_MAX_MEMSEG_LISTS) {
				RTE_LOG(ERR, EAL, "Not enough space in memseg lists, please increase %s\n",
					RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
				return -1;
			}

			/* now, allocate fbarray itself */
			if (alloc_memseg_list(msl, page_sz, n_segs, socket,
						msl_idx) < 0)
				return -1;

			/* finally, allocate VA space */
			if (alloc_va_space(msl) < 0)
				return -1;
		}
	}
	return 0;
}

/*
 * We cannot reallocate memseg lists on the fly because PPC64 stores pages
 * backwards, therefore we have to process the entire memseg first before
 * remapping it into memseg list VA space.
 */
static int
remap_needed_hugepages(struct hugepage_file *hugepages, int n_pages)
{
	int cur_page, seg_start_page, new_memseg, ret;

	seg_start_page = 0;
	for (cur_page = 0; cur_page < n_pages; cur_page++) {
		struct hugepage_file *prev, *cur;

		new_memseg = 0;

		cur = &hugepages[cur_page];
		prev = cur_page == 0 ? NULL : &hugepages[cur_page - 1];

		/* if size is zero, no more pages left */
		if (cur->size == 0)
			break;

		if (cur_page == 0)
			new_memseg = 1;
		else if (cur->socket_id != prev->socket_id)
			new_memseg = 1;
		else if (cur->size != prev->size)
			new_memseg = 1;
#ifdef RTE_ARCH_PPC_64
		/* On PPC64 architecture, the mmap always start from higher
		 * address to lower address. Here, physical addresses are in
		 * descending order.
		 */
		else if ((prev->physaddr - cur->physaddr) != cur->size)
			new_memseg = 1;
#else
		else if ((cur->physaddr - prev->physaddr) != cur->size)
			new_memseg = 1;
#endif

		if (new_memseg) {
			/* if this isn't the first time, remap segment */
			if (cur_page != 0) {
				ret = remap_segment(hugepages, seg_start_page,
						cur_page);
				if (ret != 0)
					return -1;
			}
			/* remember where we started */
			seg_start_page = cur_page;
		}
		/* continuation of previous memseg */
	}
	/* we were stopped, but we didn't remap the last segment, do it now */
	if (cur_page != 0) {
		ret = remap_segment(hugepages, seg_start_page,
				cur_page);
		if (ret != 0)
			return -1;
	}
	return 0;
}

static inline uint64_t
get_socket_mem_size(int socket)
{
	uint64_t size = 0;
	unsigned i;

	for (i = 0; i < internal_config.num_hugepage_sizes; i++){
		struct hugepage_info *hpi = &internal_config.hugepage_info[i];
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
		size_t total_size;
#ifdef RTE_ARCH_64
		int cpu_per_socket[RTE_MAX_NUMA_NODES];
		size_t default_size;
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
#else
		/* in 32-bit mode, allocate all of the memory only on master
		 * lcore socket
		 */
		total_size = internal_config.memory;
		for (socket = 0; socket < RTE_MAX_NUMA_NODES && total_size != 0;
				socket++) {
			struct rte_config *cfg = rte_eal_get_configuration();
			unsigned int master_lcore_socket;

			master_lcore_socket =
				rte_lcore_to_socket_id(cfg->master_lcore);

			if (master_lcore_socket != socket)
				continue;

			/* Update sizes */
			memory[socket] = total_size;
			break;
		}
#endif
	}

	for (socket = 0; socket < RTE_MAX_NUMA_NODES && total_mem != 0; socket++) {
		/* skips if the memory on specific socket wasn't requested */
		for (i = 0; i < num_hp_info && memory[socket] != 0; i++){
			strlcpy(hp_used[i].hugedir, hp_info[i].hugedir,
				sizeof(hp_used[i].hugedir));
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
		if (memory[socket] > 0 &&
				internal_config.socket_mem[socket] != 0) {
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
		if (strnlen(hpi->hugedir, sizeof(hpi->hugedir)) != 0) {
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
static int
eal_legacy_hugepage_init(void)
{
	struct rte_mem_config *mcfg;
	struct hugepage_file *hugepage = NULL, *tmp_hp = NULL;
	struct hugepage_info used_hp[MAX_HUGEPAGE_SIZES];
	struct rte_fbarray *arr;
	struct rte_memseg *ms;

	uint64_t memory[RTE_MAX_NUMA_NODES];

	unsigned hp_offset;
	int i, j;
	int nr_hugefiles, nr_hugepages = 0;
	void *addr;

	test_phys_addrs_available();

	memset(used_hp, 0, sizeof(used_hp));

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* hugetlbfs can be disabled */
	if (internal_config.no_hugetlbfs) {
		struct rte_memseg_list *msl;
		int n_segs, cur_seg, fd, flags;
#ifdef MEMFD_SUPPORTED
		int memfd;
#endif
		uint64_t page_sz;

		/* nohuge mode is legacy mode */
		internal_config.legacy_mem = 1;

		/* nohuge mode is single-file segments mode */
		internal_config.single_file_segments = 1;

		/* create a memseg list */
		msl = &mcfg->memsegs[0];

		page_sz = RTE_PGSIZE_4K;
		n_segs = internal_config.memory / page_sz;

		if (rte_fbarray_init(&msl->memseg_arr, "nohugemem", n_segs,
					sizeof(struct rte_memseg))) {
			RTE_LOG(ERR, EAL, "Cannot allocate memseg list\n");
			return -1;
		}

		/* set up parameters for anonymous mmap */
		fd = -1;
		flags = MAP_PRIVATE | MAP_ANONYMOUS;

#ifdef MEMFD_SUPPORTED
		/* create a memfd and store it in the segment fd table */
		memfd = memfd_create("nohuge", 0);
		if (memfd < 0) {
			RTE_LOG(DEBUG, EAL, "Cannot create memfd: %s\n",
					strerror(errno));
			RTE_LOG(DEBUG, EAL, "Falling back to anonymous map\n");
		} else {
			/* we got an fd - now resize it */
			if (ftruncate(memfd, internal_config.memory) < 0) {
				RTE_LOG(ERR, EAL, "Cannot resize memfd: %s\n",
						strerror(errno));
				RTE_LOG(ERR, EAL, "Falling back to anonymous map\n");
				close(memfd);
			} else {
				/* creating memfd-backed file was successful.
				 * we want changes to memfd to be visible to
				 * other processes (such as vhost backend), so
				 * map it as shared memory.
				 */
				RTE_LOG(DEBUG, EAL, "Using memfd for anonymous memory\n");
				fd = memfd;
				flags = MAP_SHARED;
			}
		}
#endif
		addr = mmap(NULL, internal_config.memory, PROT_READ | PROT_WRITE,
				flags, fd, 0);
		if (addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "%s: mmap() failed: %s\n", __func__,
					strerror(errno));
			return -1;
		}
		msl->base_va = addr;
		msl->page_sz = page_sz;
		msl->socket_id = 0;
		msl->len = internal_config.memory;

		/* we're in single-file segments mode, so only the segment list
		 * fd needs to be set up.
		 */
		if (fd != -1) {
			if (eal_memalloc_set_seg_list_fd(0, fd) < 0) {
				RTE_LOG(ERR, EAL, "Cannot set up segment list fd\n");
				/* not a serious error, proceed */
			}
		}

		/* populate memsegs. each memseg is one page long */
		for (cur_seg = 0; cur_seg < n_segs; cur_seg++) {
			arr = &msl->memseg_arr;

			ms = rte_fbarray_get(arr, cur_seg);
			if (rte_eal_iova_mode() == RTE_IOVA_VA)
				ms->iova = (uintptr_t)addr;
			else
				ms->iova = RTE_BAD_IOVA;
			ms->addr = addr;
			ms->hugepage_sz = page_sz;
			ms->socket_id = 0;
			ms->len = page_sz;

			rte_fbarray_set_used(arr, cur_seg);

			addr = RTE_PTR_ADD(addr, (size_t)page_sz);
		}
		if (mcfg->dma_maskbits &&
		    rte_mem_check_dma_mask_thread_unsafe(mcfg->dma_maskbits)) {
			RTE_LOG(ERR, EAL,
				"%s(): couldn't allocate memory due to IOVA exceeding limits of current DMA mask.\n",
				__func__);
			if (rte_eal_iova_mode() == RTE_IOVA_VA &&
			    rte_eal_using_phys_addrs())
				RTE_LOG(ERR, EAL,
					"%s(): Please try initializing EAL with --iova-mode=pa parameter.\n",
					__func__);
			goto fail;
		}
		return 0;
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

	/* make a copy of socket_mem, needed for balanced allocation. */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		memory[i] = internal_config.socket_mem[i];

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
		pages_new = map_all_hugepages(&tmp_hp[hp_offset], hpi, memory);
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

		if (phys_addrs_available &&
				rte_eal_iova_mode() != RTE_IOVA_VA) {
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
	hugepage = create_shared_memory(eal_hugepage_data_path(),
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
	 * this procedure only copies those hugepages that have orig_va
	 * not NULL. has overflow protection.
	 */
	if (copy_hugepages_to_shared_mem(hugepage, nr_hugefiles,
			tmp_hp, nr_hugefiles) < 0) {
		RTE_LOG(ERR, EAL, "Copying tables to shared memory failed!\n");
		goto fail;
	}

#ifndef RTE_ARCH_64
	/* for legacy 32-bit mode, we did not preallocate VA space, so do it */
	if (internal_config.legacy_mem &&
			prealloc_segments(hugepage, nr_hugefiles)) {
		RTE_LOG(ERR, EAL, "Could not preallocate VA space for hugepages\n");
		goto fail;
	}
#endif

	/* remap all pages we do need into memseg list VA space, so that those
	 * pages become first-class citizens in DPDK memory subsystem
	 */
	if (remap_needed_hugepages(hugepage, nr_hugefiles)) {
		RTE_LOG(ERR, EAL, "Couldn't remap hugepage files into memseg lists\n");
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

	munmap(hugepage, nr_hugefiles * sizeof(struct hugepage_file));
	hugepage = NULL;

	/* we're not going to allocate more pages, so release VA space for
	 * unused memseg lists
	 */
	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];
		size_t mem_sz;

		/* skip inactive lists */
		if (msl->base_va == NULL)
			continue;
		/* skip lists where there is at least one page allocated */
		if (msl->memseg_arr.count > 0)
			continue;
		/* this is an unused list, deallocate it */
		mem_sz = msl->len;
		munmap(msl->base_va, mem_sz);
		msl->base_va = NULL;

		/* destroy backing fbarray */
		rte_fbarray_destroy(&msl->memseg_arr);
	}

	if (mcfg->dma_maskbits &&
	    rte_mem_check_dma_mask_thread_unsafe(mcfg->dma_maskbits)) {
		RTE_LOG(ERR, EAL,
			"%s(): couldn't allocate memory due to IOVA exceeding limits of current DMA mask.\n",
			__func__);
		goto fail;
	}

	return 0;

fail:
	huge_recover_sigbus();
	free(tmp_hp);
	if (hugepage != NULL)
		munmap(hugepage, nr_hugefiles * sizeof(struct hugepage_file));

	return -1;
}

static int __rte_unused
hugepage_count_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct hugepage_info *hpi = arg;

	if (msl->page_sz != hpi->hugepage_sz)
		return 0;

	hpi->num_pages[msl->socket_id] += msl->memseg_arr.len;
	return 0;
}

static int
limits_callback(int socket_id, size_t cur_limit, size_t new_len)
{
	RTE_SET_USED(socket_id);
	RTE_SET_USED(cur_limit);
	RTE_SET_USED(new_len);
	return -1;
}

static int
eal_hugepage_init(void)
{
	struct hugepage_info used_hp[MAX_HUGEPAGE_SIZES];
	uint64_t memory[RTE_MAX_NUMA_NODES];
	int hp_sz_idx, socket_id;

	test_phys_addrs_available();

	memset(used_hp, 0, sizeof(used_hp));

	for (hp_sz_idx = 0;
			hp_sz_idx < (int) internal_config.num_hugepage_sizes;
			hp_sz_idx++) {
#ifndef RTE_ARCH_64
		struct hugepage_info dummy;
		unsigned int i;
#endif
		/* also initialize used_hp hugepage sizes in used_hp */
		struct hugepage_info *hpi;
		hpi = &internal_config.hugepage_info[hp_sz_idx];
		used_hp[hp_sz_idx].hugepage_sz = hpi->hugepage_sz;

#ifndef RTE_ARCH_64
		/* for 32-bit, limit number of pages on socket to whatever we've
		 * preallocated, as we cannot allocate more.
		 */
		memset(&dummy, 0, sizeof(dummy));
		dummy.hugepage_sz = hpi->hugepage_sz;
		if (rte_memseg_list_walk(hugepage_count_walk, &dummy) < 0)
			return -1;

		for (i = 0; i < RTE_DIM(dummy.num_pages); i++) {
			hpi->num_pages[i] = RTE_MIN(hpi->num_pages[i],
					dummy.num_pages[i]);
		}
#endif
	}

	/* make a copy of socket_mem, needed for balanced allocation. */
	for (hp_sz_idx = 0; hp_sz_idx < RTE_MAX_NUMA_NODES; hp_sz_idx++)
		memory[hp_sz_idx] = internal_config.socket_mem[hp_sz_idx];

	/* calculate final number of pages */
	if (calc_num_pages_per_socket(memory,
			internal_config.hugepage_info, used_hp,
			internal_config.num_hugepage_sizes) < 0)
		return -1;

	for (hp_sz_idx = 0;
			hp_sz_idx < (int)internal_config.num_hugepage_sizes;
			hp_sz_idx++) {
		for (socket_id = 0; socket_id < RTE_MAX_NUMA_NODES;
				socket_id++) {
			struct rte_memseg **pages;
			struct hugepage_info *hpi = &used_hp[hp_sz_idx];
			unsigned int num_pages = hpi->num_pages[socket_id];
			unsigned int num_pages_alloc;

			if (num_pages == 0)
				continue;

			RTE_LOG(DEBUG, EAL, "Allocating %u pages of size %" PRIu64 "M on socket %i\n",
				num_pages, hpi->hugepage_sz >> 20, socket_id);

			/* we may not be able to allocate all pages in one go,
			 * because we break up our memory map into multiple
			 * memseg lists. therefore, try allocating multiple
			 * times and see if we can get the desired number of
			 * pages from multiple allocations.
			 */

			num_pages_alloc = 0;
			do {
				int i, cur_pages, needed;

				needed = num_pages - num_pages_alloc;

				pages = malloc(sizeof(*pages) * needed);

				/* do not request exact number of pages */
				cur_pages = eal_memalloc_alloc_seg_bulk(pages,
						needed, hpi->hugepage_sz,
						socket_id, false);
				if (cur_pages <= 0) {
					free(pages);
					return -1;
				}

				/* mark preallocated pages as unfreeable */
				for (i = 0; i < cur_pages; i++) {
					struct rte_memseg *ms = pages[i];
					ms->flags |= RTE_MEMSEG_FLAG_DO_NOT_FREE;
				}
				free(pages);

				num_pages_alloc += cur_pages;
			} while (num_pages_alloc != num_pages);
		}
	}
	/* if socket limits were specified, set them */
	if (internal_config.force_socket_limits) {
		unsigned int i;
		for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
			uint64_t limit = internal_config.socket_limit[i];
			if (limit == 0)
				continue;
			if (rte_mem_alloc_validator_register("socket-limit",
					limits_callback, i, limit))
				RTE_LOG(ERR, EAL, "Failed to register socket limits validator callback\n");
		}
	}
	return 0;
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
eal_legacy_hugepage_attach(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct hugepage_file *hp = NULL;
	unsigned int num_hp = 0;
	unsigned int i = 0;
	unsigned int cur_seg;
	off_t size = 0;
	int fd, fd_hugepage = -1;

	if (aslr_enabled() > 0) {
		RTE_LOG(WARNING, EAL, "WARNING: Address Space Layout Randomization "
				"(ASLR) is enabled in the kernel.\n");
		RTE_LOG(WARNING, EAL, "   This may cause issues with mapping memory "
				"into secondary processes\n");
	}

	test_phys_addrs_available();

	fd_hugepage = open(eal_hugepage_data_path(), O_RDONLY);
	if (fd_hugepage < 0) {
		RTE_LOG(ERR, EAL, "Could not open %s\n",
				eal_hugepage_data_path());
		goto error;
	}

	size = getFileSize(fd_hugepage);
	hp = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd_hugepage, 0);
	if (hp == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "Could not mmap %s\n",
				eal_hugepage_data_path());
		goto error;
	}

	num_hp = size / sizeof(struct hugepage_file);
	RTE_LOG(DEBUG, EAL, "Analysing %u files\n", num_hp);

	/* map all segments into memory to make sure we get the addrs. the
	 * segments themselves are already in memseg list (which is shared and
	 * has its VA space already preallocated), so we just need to map
	 * everything into correct addresses.
	 */
	for (i = 0; i < num_hp; i++) {
		struct hugepage_file *hf = &hp[i];
		size_t map_sz = hf->size;
		void *map_addr = hf->final_va;
		int msl_idx, ms_idx;
		struct rte_memseg_list *msl;
		struct rte_memseg *ms;

		/* if size is zero, no more pages left */
		if (map_sz == 0)
			break;

		fd = open(hf->filepath, O_RDWR);
		if (fd < 0) {
			RTE_LOG(ERR, EAL, "Could not open %s: %s\n",
				hf->filepath, strerror(errno));
			goto error;
		}

		map_addr = mmap(map_addr, map_sz, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED, fd, 0);
		if (map_addr == MAP_FAILED) {
			RTE_LOG(ERR, EAL, "Could not map %s: %s\n",
				hf->filepath, strerror(errno));
			goto fd_error;
		}

		/* set shared lock on the file. */
		if (flock(fd, LOCK_SH) < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): Locking file failed: %s\n",
				__func__, strerror(errno));
			goto fd_error;
		}

		/* find segment data */
		msl = rte_mem_virt2memseg_list(map_addr);
		if (msl == NULL) {
			RTE_LOG(DEBUG, EAL, "%s(): Cannot find memseg list\n",
				__func__);
			goto fd_error;
		}
		ms = rte_mem_virt2memseg(map_addr, msl);
		if (ms == NULL) {
			RTE_LOG(DEBUG, EAL, "%s(): Cannot find memseg\n",
				__func__);
			goto fd_error;
		}

		msl_idx = msl - mcfg->memsegs;
		ms_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
		if (ms_idx < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): Cannot find memseg idx\n",
				__func__);
			goto fd_error;
		}

		/* store segment fd internally */
		if (eal_memalloc_set_seg_fd(msl_idx, ms_idx, fd) < 0)
			RTE_LOG(ERR, EAL, "Could not store segment fd: %s\n",
				rte_strerror(rte_errno));
	}
	/* unmap the hugepage config file, since we are done using it */
	munmap(hp, size);
	close(fd_hugepage);
	return 0;

fd_error:
	close(fd);
error:
	/* map all segments into memory to make sure we get the addrs */
	cur_seg = 0;
	for (cur_seg = 0; cur_seg < i; cur_seg++) {
		struct hugepage_file *hf = &hp[i];
		size_t map_sz = hf->size;
		void *map_addr = hf->final_va;

		munmap(map_addr, map_sz);
	}
	if (hp != NULL && hp != MAP_FAILED)
		munmap(hp, size);
	if (fd_hugepage >= 0)
		close(fd_hugepage);
	return -1;
}

static int
eal_hugepage_attach(void)
{
	if (eal_memalloc_sync_with_primary()) {
		RTE_LOG(ERR, EAL, "Could not map memory from primary process\n");
		if (aslr_enabled() > 0)
			RTE_LOG(ERR, EAL, "It is recommended to disable ASLR in the kernel and retry running both primary and secondary processes\n");
		return -1;
	}
	return 0;
}

int
rte_eal_hugepage_init(void)
{
	return internal_config.legacy_mem ?
			eal_legacy_hugepage_init() :
			eal_hugepage_init();
}

int
rte_eal_hugepage_attach(void)
{
	return internal_config.legacy_mem ?
			eal_legacy_hugepage_attach() :
			eal_hugepage_attach();
}

int
rte_eal_using_phys_addrs(void)
{
	return phys_addrs_available;
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
		/* we can still sort pages by socket in legacy mode */
		if (!internal_config.legacy_mem && socket_id > 0)
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
				uint64_t cur_mem;
				unsigned int n_segs;

				if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
					RTE_LOG(ERR, EAL,
						"No more space in memseg lists, please increase %s\n",
						RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
					return -1;
				}

				msl = &mcfg->memsegs[msl_idx];

				cur_mem = get_mem_amount(hugepage_sz,
						max_pagesz_mem);
				n_segs = cur_mem / hugepage_sz;

				if (alloc_memseg_list(msl, hugepage_sz, n_segs,
						socket_id, type_msl_idx)) {
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
	struct memtype {
		uint64_t page_sz;
		int socket_id;
	} *memtypes = NULL;
	int i, hpi_idx, msl_idx, ret = -1; /* fail unless told to succeed */
	struct rte_memseg_list *msl;
	uint64_t max_mem, max_mem_per_type;
	unsigned int max_seglists_per_type;
	unsigned int n_memtypes, cur_type;

	/* no-huge does not need this at all */
	if (internal_config.no_hugetlbfs)
		return 0;

	/*
	 * figuring out amount of memory we're going to have is a long and very
	 * involved process. the basic element we're operating with is a memory
	 * type, defined as a combination of NUMA node ID and page size (so that
	 * e.g. 2 sockets with 2 page sizes yield 4 memory types in total).
	 *
	 * deciding amount of memory going towards each memory type is a
	 * balancing act between maximum segments per type, maximum memory per
	 * type, and number of detected NUMA nodes. the goal is to make sure
	 * each memory type gets at least one memseg list.
	 *
	 * the total amount of memory is limited by RTE_MAX_MEM_MB value.
	 *
	 * the total amount of memory per type is limited by either
	 * RTE_MAX_MEM_MB_PER_TYPE, or by RTE_MAX_MEM_MB divided by the number
	 * of detected NUMA nodes. additionally, maximum number of segments per
	 * type is also limited by RTE_MAX_MEMSEG_PER_TYPE. this is because for
	 * smaller page sizes, it can take hundreds of thousands of segments to
	 * reach the above specified per-type memory limits.
	 *
	 * additionally, each type may have multiple memseg lists associated
	 * with it, each limited by either RTE_MAX_MEM_MB_PER_LIST for bigger
	 * page sizes, or RTE_MAX_MEMSEG_PER_LIST segments for smaller ones.
	 *
	 * the number of memseg lists per type is decided based on the above
	 * limits, and also taking number of detected NUMA nodes, to make sure
	 * that we don't run out of memseg lists before we populate all NUMA
	 * nodes with memory.
	 *
	 * we do this in three stages. first, we collect the number of types.
	 * then, we figure out memory constraints and populate the list of
	 * would-be memseg lists. then, we go ahead and allocate the memseg
	 * lists.
	 */

	/* create space for mem types */
	n_memtypes = internal_config.num_hugepage_sizes * rte_socket_count();
	memtypes = calloc(n_memtypes, sizeof(*memtypes));
	if (memtypes == NULL) {
		RTE_LOG(ERR, EAL, "Cannot allocate space for memory types\n");
		return -1;
	}

	/* populate mem types */
	cur_type = 0;
	for (hpi_idx = 0; hpi_idx < (int) internal_config.num_hugepage_sizes;
			hpi_idx++) {
		struct hugepage_info *hpi;
		uint64_t hugepage_sz;

		hpi = &internal_config.hugepage_info[hpi_idx];
		hugepage_sz = hpi->hugepage_sz;

		for (i = 0; i < (int) rte_socket_count(); i++, cur_type++) {
			int socket_id = rte_socket_id_by_idx(i);

#ifndef RTE_EAL_NUMA_AWARE_HUGEPAGES
			/* we can still sort pages by socket in legacy mode */
			if (!internal_config.legacy_mem && socket_id > 0)
				break;
#endif
			memtypes[cur_type].page_sz = hugepage_sz;
			memtypes[cur_type].socket_id = socket_id;

			RTE_LOG(DEBUG, EAL, "Detected memory type: "
				"socket_id:%u hugepage_sz:%" PRIu64 "\n",
				socket_id, hugepage_sz);
		}
	}
	/* number of memtypes could have been lower due to no NUMA support */
	n_memtypes = cur_type;

	/* set up limits for types */
	max_mem = (uint64_t)RTE_MAX_MEM_MB << 20;
	max_mem_per_type = RTE_MIN((uint64_t)RTE_MAX_MEM_MB_PER_TYPE << 20,
			max_mem / n_memtypes);
	/*
	 * limit maximum number of segment lists per type to ensure there's
	 * space for memseg lists for all NUMA nodes with all page sizes
	 */
	max_seglists_per_type = RTE_MAX_MEMSEG_LISTS / n_memtypes;

	if (max_seglists_per_type == 0) {
		RTE_LOG(ERR, EAL, "Cannot accommodate all memory types, please increase %s\n",
			RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
		goto out;
	}

	/* go through all mem types and create segment lists */
	msl_idx = 0;
	for (cur_type = 0; cur_type < n_memtypes; cur_type++) {
		unsigned int cur_seglist, n_seglists, n_segs;
		unsigned int max_segs_per_type, max_segs_per_list;
		struct memtype *type = &memtypes[cur_type];
		uint64_t max_mem_per_list, pagesz;
		int socket_id;

		pagesz = type->page_sz;
		socket_id = type->socket_id;

		/*
		 * we need to create segment lists for this type. we must take
		 * into account the following things:
		 *
		 * 1. total amount of memory we can use for this memory type
		 * 2. total amount of memory per memseg list allowed
		 * 3. number of segments needed to fit the amount of memory
		 * 4. number of segments allowed per type
		 * 5. number of segments allowed per memseg list
		 * 6. number of memseg lists we are allowed to take up
		 */

		/* calculate how much segments we will need in total */
		max_segs_per_type = max_mem_per_type / pagesz;
		/* limit number of segments to maximum allowed per type */
		max_segs_per_type = RTE_MIN(max_segs_per_type,
				(unsigned int)RTE_MAX_MEMSEG_PER_TYPE);
		/* limit number of segments to maximum allowed per list */
		max_segs_per_list = RTE_MIN(max_segs_per_type,
				(unsigned int)RTE_MAX_MEMSEG_PER_LIST);

		/* calculate how much memory we can have per segment list */
		max_mem_per_list = RTE_MIN(max_segs_per_list * pagesz,
				(uint64_t)RTE_MAX_MEM_MB_PER_LIST << 20);

		/* calculate how many segments each segment list will have */
		n_segs = RTE_MIN(max_segs_per_list, max_mem_per_list / pagesz);

		/* calculate how many segment lists we can have */
		n_seglists = RTE_MIN(max_segs_per_type / n_segs,
				max_mem_per_type / max_mem_per_list);

		/* limit number of segment lists according to our maximum */
		n_seglists = RTE_MIN(n_seglists, max_seglists_per_type);

		RTE_LOG(DEBUG, EAL, "Creating %i segment lists: "
				"n_segs:%i socket_id:%i hugepage_sz:%" PRIu64 "\n",
			n_seglists, n_segs, socket_id, pagesz);

		/* create all segment lists */
		for (cur_seglist = 0; cur_seglist < n_seglists; cur_seglist++) {
			if (msl_idx >= RTE_MAX_MEMSEG_LISTS) {
				RTE_LOG(ERR, EAL,
					"No more space in memseg lists, please increase %s\n",
					RTE_STR(CONFIG_RTE_MAX_MEMSEG_LISTS));
				goto out;
			}
			msl = &mcfg->memsegs[msl_idx++];

			if (alloc_memseg_list(msl, pagesz, n_segs,
					socket_id, cur_seglist))
				goto out;

			if (alloc_va_space(msl)) {
				RTE_LOG(ERR, EAL, "Cannot allocate VA space for memseg list\n");
				goto out;
			}
		}
	}
	/* we're successful */
	ret = 0;
out:
	free(memtypes);
	return ret;
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
	/* increase rlimit to maximum */
	struct rlimit lim;

	if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
		/* set limit to maximum */
		lim.rlim_cur = lim.rlim_max;

		if (setrlimit(RLIMIT_NOFILE, &lim) < 0) {
			RTE_LOG(DEBUG, EAL, "Setting maximum number of open files failed: %s\n",
					strerror(errno));
		} else {
			RTE_LOG(DEBUG, EAL, "Setting maximum number of open files to %"
					PRIu64 "\n",
					(uint64_t)lim.rlim_cur);
		}
	} else {
		RTE_LOG(ERR, EAL, "Cannot get current resource limits\n");
	}
#ifndef RTE_EAL_NUMA_AWARE_HUGEPAGES
	if (!internal_config.legacy_mem && rte_socket_count() > 1) {
		RTE_LOG(WARNING, EAL, "DPDK is running on a NUMA system, but is compiled without NUMA support.\n");
		RTE_LOG(WARNING, EAL, "This will have adverse consequences for performance and usability.\n");
		RTE_LOG(WARNING, EAL, "Please use --"OPT_LEGACY_MEM" option, or recompile with NUMA support.\n");
	}
#endif

	return rte_eal_process_type() == RTE_PROC_PRIMARY ?
#ifndef RTE_ARCH_64
			memseg_primary_init_32() :
#else
			memseg_primary_init() :
#endif
			memseg_secondary_init();
}
