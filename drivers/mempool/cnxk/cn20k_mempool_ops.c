/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include "cnxk_mempool.h"

static int
cn20k_mempool_alloc(struct rte_mempool *mp)
{
	uint32_t block_size;
	int halo_ena, rc;
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

	/* Get halo status */
	halo_ena = roc_idev_npa_halo_ena_get();

	rc = cnxk_mempool_alloc(mp, halo_ena ? ROC_NPA_HALO_F : 0);
	if (rc)
		return rc;

	rc = batch_op_init(mp);
	if (rc) {
		plt_err("Failed to init batch alloc mem rc=%d", rc);
		goto error;
	}

	return 0;
error:
	cnxk_mempool_free(mp);
	return rc;
}

static struct rte_mempool_ops cn20k_mempool_ops = {
	.name = "cn20k_mempool_ops",
	.alloc = cn20k_mempool_alloc,
	.free = cn10k_mempool_free,
	.enqueue = cn10k_mempool_enq,
	.dequeue = cn10k_mempool_deq,
	.get_count = cn10k_mempool_get_count,
	.calc_mem_size = cnxk_mempool_calc_mem_size,
	.populate = cnxk_mempool_populate,
};

RTE_MEMPOOL_REGISTER_OPS(cn20k_mempool_ops);
