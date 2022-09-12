/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_npa_buf_type_update(uint64_t aura_handle, enum roc_npa_buf_type type, int count)
{
	uint64_t aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	struct npa_lf *lf;

	lf = idev_npa_obj_get();
	if (lf == NULL || aura_id >= lf->nr_pools)
		return NPA_ERR_PARAM;

	if (plt_bitmap_get(lf->npa_bmp, aura_id)) {
		plt_err("Cannot set buf type on unused aura");
		return NPA_ERR_PARAM;
	}

	if (type >= ROC_NPA_BUF_TYPE_END || (lf->aura_attr[aura_id].buf_type[type] + count < 0)) {
		plt_err("Pool buf type invalid");
		return NPA_ERR_PARAM;
	}

	lf->aura_attr[aura_id].buf_type[type] += count;
	plt_wmb();
	return 0;
}

uint64_t
roc_npa_buf_type_mask(uint64_t aura_handle)
{
	uint64_t aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	uint64_t type_mask = 0;
	struct npa_lf *lf;
	int type;

	lf = idev_npa_obj_get();
	if (lf == NULL || aura_id >= lf->nr_pools) {
		plt_err("Invalid aura id or lf");
		return 0;
	}

	if (plt_bitmap_get(lf->npa_bmp, aura_id)) {
		plt_err("Cannot get buf_type on unused aura");
		return 0;
	}

	for (type = 0; type < ROC_NPA_BUF_TYPE_END; type++) {
		if (lf->aura_attr[aura_id].buf_type[type])
			type_mask |= BIT_ULL(type);
	}

	return type_mask;
}

uint64_t
roc_npa_buf_type_limit_get(uint64_t type_mask)
{
	uint64_t wdata, reg;
	uint64_t limit = 0;
	struct npa_lf *lf;
	uint64_t aura_id;
	int64_t *addr;
	uint64_t val;
	int type;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_PARAM;

	for (aura_id = 0; aura_id < lf->nr_pools; aura_id++) {
		if (plt_bitmap_get(lf->npa_bmp, aura_id))
			continue;

		/* Find aura's matching the buf_types requested */
		if (type_mask != 0) {
			val = 0;
			for (type = 0; type < ROC_NPA_BUF_TYPE_END; type++) {
				if (lf->aura_attr[aura_id].buf_type[type] != 0)
					val |= BIT_ULL(type);
			}
			if ((val & type_mask) == 0)
				continue;
		}

		wdata = aura_id << 44;
		addr = (int64_t *)(lf->base + NPA_LF_AURA_OP_LIMIT);
		reg = roc_atomic64_add_nosync(wdata, addr);

		if (!(reg & BIT_ULL(42)))
			limit += (reg & ROC_AURA_OP_LIMIT_MASK);
	}

	return limit;
}
