/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_log.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

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
