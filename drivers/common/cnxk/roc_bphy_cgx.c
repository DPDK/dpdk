/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"

/*
 * CN10K stores number of lmacs in 4 bit filed
 * in contraty to CN9K which uses only 3 bits.
 *
 * In theory masks should differ yet on CN9K
 * bits beyond specified range contain zeros.
 *
 * Hence common longer mask may be used.
 */
#define CGX_CMRX_RX_LMACS	0x128
#define CGX_CMRX_RX_LMACS_LMACS GENMASK_ULL(3, 0)

static uint64_t
roc_bphy_cgx_read(struct roc_bphy_cgx *roc_cgx, uint64_t lmac, uint64_t offset)
{
	int shift = roc_model_is_cn10k() ? 20 : 18;
	uint64_t base = (uint64_t)roc_cgx->bar0_va;

	return plt_read64(base + (lmac << shift) + offset);
}

static unsigned int
roc_bphy_cgx_dev_id(struct roc_bphy_cgx *roc_cgx)
{
	uint64_t cgx_id = roc_model_is_cn10k() ? GENMASK_ULL(26, 24) :
						 GENMASK_ULL(25, 24);

	return FIELD_GET(cgx_id, roc_cgx->bar0_pa);
}

int
roc_bphy_cgx_dev_init(struct roc_bphy_cgx *roc_cgx)
{
	uint64_t val;

	if (!roc_cgx || !roc_cgx->bar0_va || !roc_cgx->bar0_pa)
		return -EINVAL;

	val = roc_bphy_cgx_read(roc_cgx, 0, CGX_CMRX_RX_LMACS);
	val = FIELD_GET(CGX_CMRX_RX_LMACS_LMACS, val);
	if (roc_model_is_cn9k())
		val = GENMASK_ULL(val - 1, 0);
	roc_cgx->lmac_bmap = val;
	roc_cgx->id = roc_bphy_cgx_dev_id(roc_cgx);

	return 0;
}

int
roc_bphy_cgx_dev_fini(struct roc_bphy_cgx *roc_cgx)
{
	if (!roc_cgx)
		return -EINVAL;

	return 0;
}
