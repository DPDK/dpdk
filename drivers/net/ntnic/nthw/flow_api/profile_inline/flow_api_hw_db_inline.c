/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */


#include "hw_mod_backend.h"
#include "flow_api_engine.h"

#include "flow_api_hw_db_inline.h"
#include "rte_common.h"

/******************************************************************************/
/* Handle                                                                     */
/******************************************************************************/

struct hw_db_inline_resource_db {
	/* Actions */
	struct hw_db_inline_resource_db_cot {
		struct hw_db_inline_cot_data data;
		int ref;
	} *cot;

	struct hw_db_inline_resource_db_slc_lr {
		struct hw_db_inline_slc_lr_data data;
		int ref;
	} *slc_lr;

	uint32_t nb_cot;
	uint32_t nb_slc_lr;

	/* Items */
	struct hw_db_inline_resource_db_cat {
		struct hw_db_inline_cat_data data;
		int ref;
	} *cat;

	uint32_t nb_cat;

	/* Hardware */

	struct hw_db_inline_resource_db_cfn {
		uint64_t priority;
		int cfn_hw;
		int ref;
	} *cfn;
};

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle)
{
	/* Note: calloc is required for functionality in the hw_db_inline_destroy() */
	struct hw_db_inline_resource_db *db = calloc(1, sizeof(struct hw_db_inline_resource_db));

	if (db == NULL)
		return -1;

	db->nb_cot = ndev->be.cat.nb_cat_funcs;
	db->cot = calloc(db->nb_cot, sizeof(struct hw_db_inline_resource_db_cot));

	if (db->cot == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_slc_lr = ndev->be.max_categories;
	db->slc_lr = calloc(db->nb_slc_lr, sizeof(struct hw_db_inline_resource_db_slc_lr));

	if (db->slc_lr == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_cat = ndev->be.cat.nb_cat_funcs;
	db->cat = calloc(db->nb_cat, sizeof(struct hw_db_inline_resource_db_cat));

	if (db->cat == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	*db_handle = db;
	return 0;
}

void hw_db_inline_destroy(void *db_handle)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	free(db->cot);
	free(db->slc_lr);
	free(db->cat);

	free(db->cfn);

	free(db);
}

void hw_db_inline_deref_idxs(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_idx *idxs,
	uint32_t size)
{
	for (uint32_t i = 0; i < size; ++i) {
		switch (idxs[i].type) {
		case HW_DB_IDX_TYPE_NONE:
			break;

		case HW_DB_IDX_TYPE_CAT:
			hw_db_inline_cat_deref(ndev, db_handle, *(struct hw_db_cat_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_COT:
			hw_db_inline_cot_deref(ndev, db_handle, *(struct hw_db_cot_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_SLC_LR:
			hw_db_inline_slc_lr_deref(ndev, db_handle,
				*(struct hw_db_slc_lr_idx *)&idxs[i]);
			break;

		default:
			break;
		}
	}
}

/******************************************************************************/
/* Filter                                                                     */
/******************************************************************************/

/*
 * Setup a filter to match:
 *    All packets in CFN checks
 *    All packets in KM
 *    All packets in FLM with look-up C FT equal to specified argument
 *
 * Setup a QSL recipe to DROP all matching packets
 *
 * Note: QSL recipe 0 uses DISCARD in order to allow for exception paths (UNMQ)
 *       Consequently another QSL recipe with hard DROP is needed
 */
int hw_db_inline_setup_mbr_filter(struct flow_nic_dev *ndev, uint32_t cat_hw_id, uint32_t ft,
	uint32_t qsl_hw_id)
{
	(void)ft;
	(void)qsl_hw_id;

	const int offset = ((int)ndev->be.cat.cts_num + 1) / 2;
	(void)offset;

	/* Select and enable QSL recipe */
	if (hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 1, qsl_hw_id))
		return -1;

	if (hw_mod_cat_cts_flush(&ndev->be, offset * cat_hw_id, 6))
		return -1;

	if (hw_mod_cat_cte_set(&ndev->be, HW_CAT_CTE_ENABLE_BM, cat_hw_id, 0x8))
		return -1;

	if (hw_mod_cat_cte_flush(&ndev->be, cat_hw_id, 1))
		return -1;

	/* Make all CFN checks TRUE */
	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_SET_ALL_DEFAULTS, cat_hw_id, 0, 0))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cat_hw_id, 0, 0x1))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_L3, cat_hw_id, 0, 0x0))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_INV, cat_hw_id, 0, 0x1))
		return -1;

	/* Final match: look-up_A == TRUE && look-up_C == TRUE */
	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM0_OR, cat_hw_id, 0, 0x1))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM1_OR, cat_hw_id, 0, 0x3))
		return -1;

	if (hw_mod_cat_cfn_flush(&ndev->be, cat_hw_id, 1))
		return -1;

	return 0;
}

/******************************************************************************/
/* COT                                                                        */
/******************************************************************************/

static int hw_db_inline_cot_compare(const struct hw_db_inline_cot_data *data1,
	const struct hw_db_inline_cot_data *data2)
{
	return data1->matcher_color_contrib == data2->matcher_color_contrib &&
		data1->frag_rcp == data2->frag_rcp;
}

struct hw_db_cot_idx hw_db_inline_cot_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cot_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_cot_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_COT;

	for (uint32_t i = 1; i < db->nb_cot; ++i) {
		int ref = db->cot[i].ref;

		if (ref > 0 && hw_db_inline_cot_compare(data, &db->cot[i].data)) {
			idx.ids = i;
			hw_db_inline_cot_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->cot[idx.ids].ref = 1;
	memcpy(&db->cot[idx.ids].data, data, sizeof(struct hw_db_inline_cot_data));

	return idx;
}

void hw_db_inline_cot_ref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->cot[idx.ids].ref += 1;
}

void hw_db_inline_cot_deref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->cot[idx.ids].ref -= 1;

	if (db->cot[idx.ids].ref <= 0) {
		memset(&db->cot[idx.ids].data, 0x0, sizeof(struct hw_db_inline_cot_data));
		db->cot[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* SLC_LR                                                                     */
/******************************************************************************/

static int hw_db_inline_slc_lr_compare(const struct hw_db_inline_slc_lr_data *data1,
	const struct hw_db_inline_slc_lr_data *data2)
{
	if (!data1->head_slice_en)
		return data1->head_slice_en == data2->head_slice_en;

	return data1->head_slice_en == data2->head_slice_en &&
		data1->head_slice_dyn == data2->head_slice_dyn &&
		data1->head_slice_ofs == data2->head_slice_ofs;
}

struct hw_db_slc_lr_idx hw_db_inline_slc_lr_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_slc_lr_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_slc_lr_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_SLC_LR;

	for (uint32_t i = 1; i < db->nb_slc_lr; ++i) {
		int ref = db->slc_lr[i].ref;

		if (ref > 0 && hw_db_inline_slc_lr_compare(data, &db->slc_lr[i].data)) {
			idx.ids = i;
			hw_db_inline_slc_lr_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->slc_lr[idx.ids].ref = 1;
	memcpy(&db->slc_lr[idx.ids].data, data, sizeof(struct hw_db_inline_slc_lr_data));

	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_SLC_EN, idx.ids, data->head_slice_en);
	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_DYN, idx.ids, data->head_slice_dyn);
	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_OFS, idx.ids, data->head_slice_ofs);
	hw_mod_slc_lr_rcp_flush(&ndev->be, idx.ids, 1);

	return idx;
}

void hw_db_inline_slc_lr_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->slc_lr[idx.ids].ref += 1;
}

void hw_db_inline_slc_lr_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->slc_lr[idx.ids].ref -= 1;

	if (db->slc_lr[idx.ids].ref <= 0) {
		hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_PRESET_ALL, idx.ids, 0x0);
		hw_mod_slc_lr_rcp_flush(&ndev->be, idx.ids, 1);

		memset(&db->slc_lr[idx.ids].data, 0x0, sizeof(struct hw_db_inline_slc_lr_data));
		db->slc_lr[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* CAT                                                                        */
/******************************************************************************/

static int hw_db_inline_cat_compare(const struct hw_db_inline_cat_data *data1,
	const struct hw_db_inline_cat_data *data2)
{
	return data1->vlan_mask == data2->vlan_mask &&
		data1->mac_port_mask == data2->mac_port_mask &&
		data1->ptc_mask_frag == data2->ptc_mask_frag &&
		data1->ptc_mask_l2 == data2->ptc_mask_l2 &&
		data1->ptc_mask_l3 == data2->ptc_mask_l3 &&
		data1->ptc_mask_l4 == data2->ptc_mask_l4 &&
		data1->ptc_mask_tunnel == data2->ptc_mask_tunnel &&
		data1->ptc_mask_l3_tunnel == data2->ptc_mask_l3_tunnel &&
		data1->ptc_mask_l4_tunnel == data2->ptc_mask_l4_tunnel &&
		data1->err_mask_ttl_tunnel == data2->err_mask_ttl_tunnel &&
		data1->err_mask_ttl == data2->err_mask_ttl && data1->ip_prot == data2->ip_prot &&
		data1->ip_prot_tunnel == data2->ip_prot_tunnel;
}

struct hw_db_cat_idx hw_db_inline_cat_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cat_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_cat_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_CAT;

	for (uint32_t i = 0; i < db->nb_cat; ++i) {
		int ref = db->cat[i].ref;

		if (ref > 0 && hw_db_inline_cat_compare(data, &db->cat[i].data)) {
			idx.ids = i;
			hw_db_inline_cat_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->cat[idx.ids].ref = 1;
	memcpy(&db->cat[idx.ids].data, data, sizeof(struct hw_db_inline_cat_data));

	return idx;
}

void hw_db_inline_cat_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cat_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->cat[idx.ids].ref += 1;
}

void hw_db_inline_cat_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cat_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->cat[idx.ids].ref -= 1;

	if (db->cat[idx.ids].ref <= 0) {
		memset(&db->cat[idx.ids].data, 0x0, sizeof(struct hw_db_inline_cat_data));
		db->cat[idx.ids].ref = 0;
	}
}
