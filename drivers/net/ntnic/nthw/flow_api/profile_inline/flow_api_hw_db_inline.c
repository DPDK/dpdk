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

	struct hw_db_inline_resource_db_qsl {
		struct hw_db_inline_qsl_data data;
		int qst_idx;
	} *qsl;

	struct hw_db_inline_resource_db_slc_lr {
		struct hw_db_inline_slc_lr_data data;
		int ref;
	} *slc_lr;

	uint32_t nb_cot;
	uint32_t nb_qsl;
	uint32_t nb_slc_lr;

	/* Items */
	struct hw_db_inline_resource_db_cat {
		struct hw_db_inline_cat_data data;
		int ref;
	} *cat;

	struct hw_db_inline_resource_db_km_rcp {
		struct hw_db_inline_km_rcp_data data;
		int ref;

		struct hw_db_inline_resource_db_km_ft {
			struct hw_db_inline_km_ft_data data;
			int ref;
		} *ft;
	} *km;

	uint32_t nb_cat;
	uint32_t nb_km_ft;
	uint32_t nb_km_rcp;

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

	db->nb_qsl = ndev->be.qsl.nb_rcp_categories;
	db->qsl = calloc(db->nb_qsl, sizeof(struct hw_db_inline_resource_db_qsl));

	if (db->qsl == NULL) {
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

	db->nb_km_ft = ndev->be.cat.nb_flow_types;
	db->nb_km_rcp = ndev->be.km.nb_categories;
	db->km = calloc(db->nb_km_rcp, sizeof(struct hw_db_inline_resource_db_km_rcp));

	if (db->km == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	for (uint32_t i = 0; i < db->nb_km_rcp; ++i) {
		db->km[i].ft = calloc(db->nb_km_ft * db->nb_cat,
			sizeof(struct hw_db_inline_resource_db_km_ft));

		if (db->km[i].ft == NULL) {
			hw_db_inline_destroy(db);
			return -1;
		}
	}

	*db_handle = db;
	return 0;
}

void hw_db_inline_destroy(void *db_handle)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	free(db->cot);
	free(db->qsl);
	free(db->slc_lr);
	free(db->cat);

	if (db->km) {
		for (uint32_t i = 0; i < db->nb_km_rcp; ++i)
			free(db->km[i].ft);

		free(db->km);
	}

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

		case HW_DB_IDX_TYPE_QSL:
			hw_db_inline_qsl_deref(ndev, db_handle, *(struct hw_db_qsl_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_SLC_LR:
			hw_db_inline_slc_lr_deref(ndev, db_handle,
				*(struct hw_db_slc_lr_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_KM_RCP:
			hw_db_inline_km_deref(ndev, db_handle, *(struct hw_db_km_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_KM_FT:
			hw_db_inline_km_ft_deref(ndev, db_handle, *(struct hw_db_km_ft *)&idxs[i]);
			break;

		default:
			break;
		}
	}
}


const void *hw_db_inline_find_data(struct flow_nic_dev *ndev, void *db_handle,
	enum hw_db_idx_type type, struct hw_db_idx *idxs, uint32_t size)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	for (uint32_t i = 0; i < size; ++i) {
		if (idxs[i].type != type)
			continue;

		switch (type) {
		case HW_DB_IDX_TYPE_NONE:
			return NULL;

		case HW_DB_IDX_TYPE_CAT:
			return &db->cat[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_QSL:
			return &db->qsl[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_COT:
			return &db->cot[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_SLC_LR:
			return &db->slc_lr[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_KM_RCP:
			return &db->km[idxs[i].id1].data;

		case HW_DB_IDX_TYPE_KM_FT:
			return NULL;	/* FTs can't be easily looked up */

		default:
			return NULL;
		}
	}

	return NULL;
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

	/* QSL for traffic policing */
	if (hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DROP, qsl_hw_id, 0x3) < 0)
		return -1;

	if (hw_mod_qsl_rcp_flush(&ndev->be, qsl_hw_id, 1) < 0)
		return -1;

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
/* QSL                                                                        */
/******************************************************************************/

/* Calculate queue mask for QSL TBL_MSK for given number of queues.
 * NOTE: If number of queues is not power of two, then queue mask will be created
 *       for nearest smaller power of two.
 */
static uint32_t queue_mask(uint32_t nr_queues)
{
	nr_queues |= nr_queues >> 1;
	nr_queues |= nr_queues >> 2;
	nr_queues |= nr_queues >> 4;
	nr_queues |= nr_queues >> 8;
	nr_queues |= nr_queues >> 16;
	return nr_queues >> 1;
}

static int hw_db_inline_qsl_compare(const struct hw_db_inline_qsl_data *data1,
	const struct hw_db_inline_qsl_data *data2)
{
	if (data1->discard != data2->discard || data1->drop != data2->drop ||
		data1->table_size != data2->table_size || data1->retransmit != data2->retransmit) {
		return 0;
	}

	for (int i = 0; i < HW_DB_INLINE_MAX_QST_PER_QSL; ++i) {
		if (data1->table[i].queue != data2->table[i].queue ||
			data1->table[i].queue_en != data2->table[i].queue_en ||
			data1->table[i].tx_port != data2->table[i].tx_port ||
			data1->table[i].tx_port_en != data2->table[i].tx_port_en) {
			return 0;
		}
	}

	return 1;
}

struct hw_db_qsl_idx hw_db_inline_qsl_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_qsl_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_qsl_idx qsl_idx = { .raw = 0 };
	uint32_t qst_idx = 0;
	int res;

	qsl_idx.type = HW_DB_IDX_TYPE_QSL;

	if (data->discard) {
		qsl_idx.ids = 0;
		return qsl_idx;
	}

	for (uint32_t i = 1; i < db->nb_qsl; ++i) {
		if (hw_db_inline_qsl_compare(data, &db->qsl[i].data)) {
			qsl_idx.ids = i;
			hw_db_inline_qsl_ref(ndev, db, qsl_idx);
			return qsl_idx;
		}
	}

	res = flow_nic_alloc_resource(ndev, RES_QSL_RCP, 1);

	if (res < 0) {
		qsl_idx.error = 1;
		return qsl_idx;
	}

	qsl_idx.ids = res & 0xff;

	if (data->table_size > 0) {
		res = flow_nic_alloc_resource_config(ndev, RES_QSL_QST, data->table_size, 1);

		if (res < 0) {
			flow_nic_deref_resource(ndev, RES_QSL_RCP, qsl_idx.ids);
			qsl_idx.error = 1;
			return qsl_idx;
		}

		qst_idx = (uint32_t)res;
	}

	memcpy(&db->qsl[qsl_idx.ids].data, data, sizeof(struct hw_db_inline_qsl_data));
	db->qsl[qsl_idx.ids].qst_idx = qst_idx;

	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_PRESET_ALL, qsl_idx.ids, 0x0);

	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DISCARD, qsl_idx.ids, data->discard);
	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DROP, qsl_idx.ids, data->drop * 0x3);
	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_LR, qsl_idx.ids, data->retransmit * 0x3);

	if (data->table_size == 0) {
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_LO, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_HI, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_IDX, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_MSK, qsl_idx.ids, 0x0);

	} else {
		const uint32_t table_start = qst_idx;
		const uint32_t table_end = table_start + data->table_size - 1;

		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_LO, qsl_idx.ids, table_start);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_HI, qsl_idx.ids, table_end);

		/* Toeplitz hash function uses TBL_IDX and TBL_MSK. */
		uint32_t msk = queue_mask(table_end - table_start + 1);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_IDX, qsl_idx.ids, table_start);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_MSK, qsl_idx.ids, msk);

		for (uint32_t i = 0; i < data->table_size; ++i) {
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_PRESET_ALL, table_start + i, 0x0);

			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_TX_PORT, table_start + i,
				data->table[i].tx_port);
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_LRE, table_start + i,
				data->table[i].tx_port_en);

			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_QUEUE, table_start + i,
				data->table[i].queue);
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_EN, table_start + i,
				data->table[i].queue_en);
		}

		hw_mod_qsl_qst_flush(&ndev->be, table_start, data->table_size);
	}

	hw_mod_qsl_rcp_flush(&ndev->be, qsl_idx.ids, 1);

	return qsl_idx;
}

void hw_db_inline_qsl_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_qsl_idx idx)
{
	(void)db_handle;

	if (!idx.error && idx.ids != 0)
		flow_nic_ref_resource(ndev, RES_QSL_RCP, idx.ids);
}

void hw_db_inline_qsl_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_qsl_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error || idx.ids == 0)
		return;

	if (flow_nic_deref_resource(ndev, RES_QSL_RCP, idx.ids) == 0) {
		const int table_size = (int)db->qsl[idx.ids].data.table_size;

		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_PRESET_ALL, idx.ids, 0x0);
		hw_mod_qsl_rcp_flush(&ndev->be, idx.ids, 1);

		if (table_size > 0) {
			const int table_start = db->qsl[idx.ids].qst_idx;

			for (int i = 0; i < (int)table_size; ++i) {
				hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_PRESET_ALL,
					table_start + i, 0x0);
				flow_nic_free_resource(ndev, RES_QSL_QST, table_start + i);
			}

			hw_mod_qsl_qst_flush(&ndev->be, table_start, table_size);
		}

		memset(&db->qsl[idx.ids].data, 0x0, sizeof(struct hw_db_inline_qsl_data));
		db->qsl[idx.ids].qst_idx = 0;
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

/******************************************************************************/
/* KM RCP                                                                     */
/******************************************************************************/

static int hw_db_inline_km_compare(const struct hw_db_inline_km_rcp_data *data1,
	const struct hw_db_inline_km_rcp_data *data2)
{
	return data1->rcp == data2->rcp;
}

struct hw_db_km_idx hw_db_inline_km_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_rcp_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_km_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_KM_RCP;

	for (uint32_t i = 0; i < db->nb_km_rcp; ++i) {
		if (!found && db->km[i].ref <= 0) {
			found = 1;
			idx.id1 = i;
		}

		if (db->km[i].ref > 0 && hw_db_inline_km_compare(data, &db->km[i].data)) {
			idx.id1 = i;
			hw_db_inline_km_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&db->km[idx.id1].data, data, sizeof(struct hw_db_inline_km_rcp_data));
	db->km[idx.id1].ref = 1;

	return idx;
}

void hw_db_inline_km_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_km_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->km[idx.id1].ref += 1;
}

void hw_db_inline_km_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_km_idx idx)
{
	(void)ndev;
	(void)db_handle;

	if (idx.error)
		return;
}

/******************************************************************************/
/* KM FT                                                                      */
/******************************************************************************/

static int hw_db_inline_km_ft_compare(const struct hw_db_inline_km_ft_data *data1,
	const struct hw_db_inline_km_ft_data *data2)
{
	return data1->cat.raw == data2->cat.raw && data1->km.raw == data2->km.raw &&
		data1->action_set.raw == data2->action_set.raw;
}

struct hw_db_km_ft hw_db_inline_km_ft_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_ft_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_km_rcp *km_rcp = &db->km[data->km.id1];
	struct hw_db_km_ft idx = { .raw = 0 };
	uint32_t cat_offset = data->cat.ids * db->nb_cat;
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_KM_FT;
	idx.id2 = data->km.id1;
	idx.id3 = data->cat.ids;

	if (km_rcp->data.rcp == 0) {
		idx.id1 = 0;
		return idx;
	}

	for (uint32_t i = 1; i < db->nb_km_ft; ++i) {
		const struct hw_db_inline_resource_db_km_ft *km_ft = &km_rcp->ft[cat_offset + i];

		if (!found && km_ft->ref <= 0) {
			found = 1;
			idx.id1 = i;
		}

		if (km_ft->ref > 0 && hw_db_inline_km_ft_compare(data, &km_ft->data)) {
			idx.id1 = i;
			hw_db_inline_km_ft_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&km_rcp->ft[cat_offset + idx.id1].data, data,
		sizeof(struct hw_db_inline_km_ft_data));
	km_rcp->ft[cat_offset + idx.id1].ref = 1;

	return idx;
}

void hw_db_inline_km_ft_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_km_ft idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error) {
		uint32_t cat_offset = idx.id3 * db->nb_cat;
		db->km[idx.id2].ft[cat_offset + idx.id1].ref += 1;
	}
}

void hw_db_inline_km_ft_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_km_ft idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_km_rcp *km_rcp = &db->km[idx.id2];
	uint32_t cat_offset = idx.id3 * db->nb_cat;

	if (idx.error)
		return;

	km_rcp->ft[cat_offset + idx.id1].ref -= 1;

	if (km_rcp->ft[cat_offset + idx.id1].ref <= 0) {
		memset(&km_rcp->ft[cat_offset + idx.id1].data, 0x0,
			sizeof(struct hw_db_inline_km_ft_data));
		km_rcp->ft[cat_offset + idx.id1].ref = 0;
	}
}
