/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_mempool.h>

#include "roc_api.h"
#include "cnxk_mempool.h"

static int
cn10k_mempool_alloc(struct rte_mempool *mp)
{
	uint32_t block_size;
	size_t padding;

	block_size = mp->elt_size + mp->header_size + mp->trailer_size;
	/* Align header size to ROC_ALIGN */
	if (mp->header_size % ROC_ALIGN != 0) {
		padding = RTE_ALIGN_CEIL(mp->header_size, ROC_ALIGN) -
			  mp->header_size;
		mp->header_size += padding;
		block_size += padding;
	}

	/* Align block size to ROC_ALIGN */
	if (block_size % ROC_ALIGN != 0) {
		padding = RTE_ALIGN_CEIL(block_size, ROC_ALIGN) - block_size;
		mp->trailer_size += padding;
		block_size += padding;
	}

	return cnxk_mempool_alloc(mp);
}

static void
cn10k_mempool_free(struct rte_mempool *mp)
{
	cnxk_mempool_free(mp);
}

static struct rte_mempool_ops cn10k_mempool_ops = {
	.name = "cn10k_mempool_ops",
	.alloc = cn10k_mempool_alloc,
	.free = cn10k_mempool_free,
	.enqueue = cnxk_mempool_enq,
	.dequeue = cnxk_mempool_deq,
	.get_count = cnxk_mempool_get_count,
	.calc_mem_size = cnxk_mempool_calc_mem_size,
	.populate = cnxk_mempool_populate,
};

MEMPOOL_REGISTER_OPS(cn10k_mempool_ops);
