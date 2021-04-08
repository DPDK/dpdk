/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_mempool.h>

#include "roc_api.h"
#include "cnxk_mempool.h"

static int
cn9k_mempool_alloc(struct rte_mempool *mp)
{
	size_t block_size, padding;

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

	/*
	 * Marvell CN9k has 8 sets, 41 ways L1D cache, VA<9:7> bits dictate the
	 * set selection. Add additional padding to ensure that the element size
	 * always occupies odd number of cachelines to ensure even distribution
	 * of elements among L1D cache sets.
	 */
	padding = ((block_size / ROC_ALIGN) % 2) ? 0 : ROC_ALIGN;
	mp->trailer_size += padding;

	return cnxk_mempool_alloc(mp);
}

static struct rte_mempool_ops cn9k_mempool_ops = {
	.name = "cn9k_mempool_ops",
	.alloc = cn9k_mempool_alloc,
	.free = cnxk_mempool_free,
	.enqueue = cnxk_mempool_enq,
	.dequeue = cnxk_mempool_deq,
	.get_count = cnxk_mempool_get_count,
	.calc_mem_size = cnxk_mempool_calc_mem_size,
	.populate = cnxk_mempool_populate,
};

MEMPOOL_REGISTER_OPS(cn9k_mempool_ops);
