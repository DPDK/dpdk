/*  SPDX-License-Identifier: BSD-3-Clause
 *  Copyright(c) 2018 Marvell International Ltd.
 */

#include <rte_common.h>

#include <env/mv_autogen_comp_flags.h>
#include <env/mv_sys_dma.h>

#include "rte_mvep_common.h"

/* Memory size (in bytes) for MUSDK dma buffers */
#define MRVL_MUSDK_DMA_MEMSIZE (40 * 1024 * 1024)

struct mvep {
	uint32_t ref_count;
};

static struct mvep mvep;

int rte_mvep_init(enum mvep_module_type module __rte_unused,
		  struct rte_kvargs *kvlist __rte_unused)
{
	int ret;

	if (!mvep.ref_count) {
		ret = mv_sys_dma_mem_init(MRVL_MUSDK_DMA_MEMSIZE);
		if (ret)
			return ret;
	}

	mvep.ref_count++;

	return 0;
}

int rte_mvep_deinit(enum mvep_module_type module __rte_unused)
{
	mvep.ref_count--;

	if (!mvep.ref_count)
		mv_sys_dma_mem_destroy();

	return 0;
}
