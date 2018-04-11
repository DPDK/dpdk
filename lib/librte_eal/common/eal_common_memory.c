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

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

/*
 * Try to mmap *size bytes in /dev/zero. If it is successful, return the
 * pointer to the mmap'd area and keep *size unmodified. Else, retry
 * with a smaller zone: decrease *size by hugepage_sz until it reaches
 * 0. In this case, return NULL. Note: this function returns an address
 * which is a multiple of hugepage size.
 */

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

		mapped_addr = mmap(requested_addr, map_sz, PROT_READ,
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

	if (unmap)
		munmap(mapped_addr, map_sz);

	RTE_LOG(DEBUG, EAL, "Virtual area found at %p (size = 0x%zx)\n",
		aligned_addr, *size);

	baseaddr_offset += *size;

	return aligned_addr;
}

/*
 * Return a pointer to a read-only table of struct rte_physmem_desc
 * elements, containing the layout of all addressable physical
 * memory. The last element of the table contains a NULL address.
 */
const struct rte_memseg *
rte_eal_get_physmem_layout(void)
{
	return rte_eal_get_configuration()->mem_config->memseg;
}


/* get the total size of memory */
uint64_t
rte_eal_get_physmem_size(void)
{
	const struct rte_mem_config *mcfg;
	unsigned i = 0;
	uint64_t total_len = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (mcfg->memseg[i].addr == NULL)
			break;

		total_len += mcfg->memseg[i].len;
	}

	return total_len;
}

/* Dump the physical memory layout on console */
void
rte_dump_physmem_layout(FILE *f)
{
	const struct rte_mem_config *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (mcfg->memseg[i].addr == NULL)
			break;

		fprintf(f, "Segment %u: IOVA:0x%"PRIx64", len:%zu, "
		       "virt:%p, socket_id:%"PRId32", "
		       "hugepage_sz:%"PRIu64", nchannel:%"PRIx32", "
		       "nrank:%"PRIx32"\n", i,
		       mcfg->memseg[i].iova,
		       mcfg->memseg[i].len,
		       mcfg->memseg[i].addr,
		       mcfg->memseg[i].socket_id,
		       mcfg->memseg[i].hugepage_sz,
		       mcfg->memseg[i].nchannel,
		       mcfg->memseg[i].nrank);
	}
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

/* init memory subsystem */
int
rte_eal_memory_init(void)
{
	RTE_LOG(DEBUG, EAL, "Setting up physically contiguous memory...\n");

	const int retval = rte_eal_process_type() == RTE_PROC_PRIMARY ?
			rte_eal_hugepage_init() :
			rte_eal_hugepage_attach();
	if (retval < 0)
		return -1;

	if (internal_config.no_shconf == 0 && rte_eal_memdevice_init() < 0)
		return -1;

	return 0;
}
