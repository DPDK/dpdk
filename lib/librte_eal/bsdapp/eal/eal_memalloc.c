/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <inttypes.h>

#include <rte_log.h>
#include <rte_memory.h>

#include "eal_memalloc.h"

int
eal_memalloc_alloc_seg_bulk(struct rte_memseg **ms __rte_unused,
		int __rte_unused n_segs, size_t __rte_unused page_sz,
		int __rte_unused socket, bool __rte_unused exact)
{
	RTE_LOG(ERR, EAL, "Memory hotplug not supported on FreeBSD\n");
	return -1;
}

struct rte_memseg *
eal_memalloc_alloc_seg(size_t __rte_unused page_sz, int __rte_unused socket)
{
	RTE_LOG(ERR, EAL, "Memory hotplug not supported on FreeBSD\n");
	return NULL;
}

int
eal_memalloc_free_seg(struct rte_memseg *ms __rte_unused)
{
	RTE_LOG(ERR, EAL, "Memory hotplug not supported on FreeBSD\n");
	return -1;
}

int
eal_memalloc_free_seg_bulk(struct rte_memseg **ms __rte_unused,
		int n_segs __rte_unused)
{
	RTE_LOG(ERR, EAL, "Memory hotplug not supported on FreeBSD\n");
	return -1;
}

int
eal_memalloc_sync_with_primary(void)
{
	RTE_LOG(ERR, EAL, "Memory hotplug not supported on FreeBSD\n");
	return -1;
}

int
eal_memalloc_init(void)
{
	return 0;
}
