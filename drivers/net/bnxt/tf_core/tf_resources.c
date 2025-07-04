/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2024 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <rte_common.h>

#include "tf_tbl.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_session.h"
#include "tf_device.h"
#include "cfa_tcam_mgr_device.h"

#ifdef TF_FLOW_SCALE_QUERY

/* Logging defines */
#define TF_FLOW_SCALE_QUERY_DEBUG 0

/* global data stored in firmware memory and TruFlow driver*/
struct cfa_tf_resc_usage tf_resc_usage[TF_DIR_MAX];

struct tf_resc_usage_buffer_control {
	enum tf_device_type device_type;
	bool fw_sync_paused;
	uint32_t buffer_dirty[TF_DIR_MAX];
};

static struct tf_resc_usage_buffer_control resc_usage_control;

/* Check if supporting resource usage */
static bool tf_resc_usage_support(struct tf *tfp)
{
	struct tf_session *tfs;
	bool support = true;
	int rc;

	/* Not valid session id */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return false;

	/* Support Thor */
	if (resc_usage_control.device_type != tfs->dev.type)
		support = false;

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
	TFP_DRV_LOG(INFO, "Resc usage update on device type: %d, allow: %s\n",
		    resc_usage_control.device_type,
		    support ? "True" : "False");
#endif /* TF_FLOW_SCALE_QUERY_DEBUG == 1 */
	return support;
}

/* Reset the resource usage buffer */
void tf_resc_usage_reset(struct tf *tfp __rte_unused, enum tf_device_type type)
{
	/* Support Thor only*/
	if (type != TF_DEVICE_TYPE_P5)
		return;

	resc_usage_control.fw_sync_paused = false;
	resc_usage_control.device_type = type;
	resc_usage_control.buffer_dirty[TF_DIR_RX] = 1;
	resc_usage_control.buffer_dirty[TF_DIR_TX] = 1;
	memset(tf_resc_usage, 0, sizeof(tf_resc_usage));
}

/* Check the bumber of the used slices in a row */
static int
tf_tcam_mgr_row_entry_used(struct cfa_tcam_mgr_table_rows_0 *row,
			    int max_slices)
{
	int used = 0, j;

	for (j = 0; j < (max_slices / row->entry_size); j++) {
		if (ROW_ENTRY_INUSE(row, j))
			used++;
	}
	return used;
}

/* Initialize the resource usage buffer for WC-TCAM tables */
void tf_tcam_usage_init(struct tf *tfp)
{
	enum cfa_tcam_mgr_tbl_type type = CFA_TCAM_MGR_TBL_TYPE_WC_TCAM;
	struct cfa_tcam_mgr_table_data *table_data = NULL;
	struct tf_resc_wc_tcam_usage *usage_data = NULL;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	enum tf_dir dir;
	int rc;

	/* Check if supported on this device */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM data created for session\n",
			    __func__);
		return;
	}

	/* Iterate over all directions */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
		usage_data = &tf_resc_usage[dir].wc_tcam_usage;

		/* cfa_tcam_mgr_table_dump(tfs->session_id.id, dir, type); */
		memset(usage_data, 0, sizeof(*usage_data));
		if (table_data->start_row != table_data->end_row)
			usage_data->max_row_number = table_data->end_row -
						     table_data->start_row + 1;
		usage_data->unused_row_number = usage_data->max_row_number;

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
		/* dump usage data */
		TFP_DRV_LOG(INFO, "WC-TCAM:  1-p  1-f  2-p  2-f  4-f  free-rows\n");
		TFP_DRV_LOG(INFO, "%s	 %-4d %-4d %-4d %-4d %-4d %-4d\n",
			    (dir == TF_DIR_RX) ? "RX" : "TX",
			    usage_data->slice_row_1_p_used,
			    usage_data->slice_row_1_f_used,
			    usage_data->slice_row_2_p_used,
			    usage_data->slice_row_2_f_used,
			    usage_data->slice_row_4_used,
			    usage_data->unused_row_number);
#endif
	}
}

/* Update wc-tcam table resoure usage */
int tf_tcam_usage_update(struct tf *tfp,
			 enum tf_dir dir,
			 int tcam_tbl_type,
			 void *data,
			 enum tf_resc_opt resc_opt)
{
	struct cfa_tcam_mgr_table_rows_0 *key_row = (struct cfa_tcam_mgr_table_rows_0 *)data;
	struct tf_resc_wc_tcam_usage *usage_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	int key_slices = key_row->entry_size;
	struct tf_session *tfs;
	int used_entries;
	int rc;

	/* Check if supported on this device */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM data created for session\n",
			    __func__);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return -1;

	/* Support WC-TCAM APPs only */
	if (tcam_tbl_type != CFA_TCAM_MGR_TBL_TYPE_WC_TCAM)
		return 0;

	resc_usage_control.buffer_dirty[dir] = 1;
	usage_data = &tf_resc_usage[dir].wc_tcam_usage;
	if (resc_opt == TF_RESC_ALLOC) {
		switch (key_slices) {
		case 4:
			usage_data->unused_row_number -= 1;
			usage_data->slice_row_4_used += 1;
			break;
		case 2:
			used_entries = tf_tcam_mgr_row_entry_used(key_row, 4);
			if (used_entries == 2) {
				usage_data->slice_row_2_p_used -= 1;
				usage_data->slice_row_2_f_used += 1;
			} else {
				usage_data->unused_row_number -= 1;
				usage_data->slice_row_2_p_used += 1;
			}
			break;
		case 1:
			used_entries = tf_tcam_mgr_row_entry_used(key_row, 4);
			if (used_entries == 4) {
				usage_data->slice_row_1_p_used -= 1;
				usage_data->slice_row_1_f_used += 1;
			} else if (used_entries == 1) {
				usage_data->slice_row_1_p_used += 1;
				usage_data->unused_row_number -= 1;
			}
			break;
		default:
			TFP_DRV_LOG(ERR, "CFA invalid size of key slices: %d\n",
				    key_slices);
			break;
		}
	} else { /* free one entry */
		switch (key_slices) {
		case 4:
			usage_data->unused_row_number += 1;
			usage_data->slice_row_4_used -= 1;
			break;
		case 2:
			if (!ROW_INUSE(key_row)) {  /* empty */
				usage_data->unused_row_number += 1;
				usage_data->slice_row_2_p_used -= 1;
			} else {
				usage_data->slice_row_2_p_used += 1;
				usage_data->slice_row_2_f_used -= 1;
			}
			break;
		case 1:
			used_entries = tf_tcam_mgr_row_entry_used(key_row, 4);
			if (!ROW_INUSE(key_row)) {  /* empty */
				usage_data->unused_row_number += 1;
				usage_data->slice_row_1_p_used -= 1;
			} else if (used_entries == 3) {
				usage_data->slice_row_1_f_used -= 1;
				usage_data->slice_row_1_p_used += 1;
			}
			break;
		default:
			TFP_DRV_LOG(ERR, "CFA invalid size of key slices: %d\n",
				    key_slices);
			break;
		}
	}

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
	/* dump usage data*/
	TFP_DRV_LOG(INFO, "WC-TCAM:  1-p  1-f  2-p  2-f  4-f  free-rows\n");
	TFP_DRV_LOG(INFO, "	  %-4d %-4d %-4d %-4d %-4d %-4d\n",
		    usage_data->slice_row_1_p_used,
		    usage_data->slice_row_1_f_used,
		    usage_data->slice_row_2_p_used,
		    usage_data->slice_row_2_f_used,
		    usage_data->slice_row_4_used,
		    usage_data->unused_row_number);
#endif
	return 0;
}

/* Initialize the EM usage table */
void tf_em_usage_init(struct tf *tfp, enum tf_dir dir, uint16_t max_entries)
{
	struct tf_resc_em_usage *em;

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return;

	em = &tf_resc_usage[dir].em_int_usage;
	em->max_entries = max_entries;
	em->used_entries = 0;
}

/* Update the EM usage table */
int tf_em_usage_update(struct tf *tfp,
		       enum tf_dir dir,
		       uint16_t size,
		       enum tf_resc_opt resc_opt)
{
	struct tf_resc_em_usage *em;

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
	TFP_DRV_LOG(INFO, "%s: %s: EM record size: %d, %s\n",
		    __func__,
		    dir ? "TX" : "RX",
		    size,
		    resc_opt == TF_RESC_ALLOC ? "Alloc" : "Free");
#endif /* TF_FLOW_SCALE_QUERY_DEBUG == 1 */

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return -1;

	/* not valid size */
	if (!size)
		return 0;

	resc_usage_control.buffer_dirty[dir] = 1;
	em = &tf_resc_usage[dir].em_int_usage;
	if (resc_opt == TF_RESC_ALLOC) {
		em->used_entries += size;
		assert(em->used_entries <= em->max_entries);
	} else {
		assert(em->used_entries >= size);
		em->used_entries -= size;
	}
	return 0;
}

/* Initialize the usage buffer for all kinds of sram tables */
void tf_tbl_usage_init(struct tf *tfp,
		       enum tf_dir dir,
		       uint32_t tbl_type,
		       uint16_t max_entries)
{
	struct tf_rm_element_cfg *tbl_cfg = tf_tbl_p58[dir];

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
	TFP_DRV_LOG(INFO, "%s: %s: tbl_type: %d[%s], max entries: [%d]:[0x%x]\n",
		    __func__,
		    dir ? "TX" : "RX",
		    tbl_type,
		    tf_tbl_type_2_str(tbl_type),
		    max_entries,
		    max_entries);
#endif /* TF_FLOW_SCALE_QUERY_DEBUG == 1 */

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return;

	/* Convert to entries */
	if (tbl_cfg[tbl_type].slices)
		max_entries *= (16 / tbl_cfg[tbl_type].slices);

	switch (tbl_type) {
	/* Counter Action */
	case TF_TBL_TYPE_ACT_STATS_64:
	{
		struct tf_resc_cnt_usage *cnt;
		cnt = &tf_resc_usage[dir].cnt_usage;
		cnt->max_entries = max_entries;
		cnt->used_entries = 0;
		break;
	}
	/* Action Recrod */
	case TF_TBL_TYPE_COMPACT_ACT_RECORD:
	case TF_TBL_TYPE_FULL_ACT_RECORD:
	{
		struct tf_resc_act_usage *act;
		act = &tf_resc_usage[dir].act_usage;
		act->max_entries += max_entries;
		act->free_entries += max_entries;
		act->num_compact_act_records = 0;
		act->num_full_act_records = 0;
		break;
	}
	/* ACT_ENCAP adn ACT_MODIFY Records */
	case TF_TBL_TYPE_ACT_ENCAP_8B:
	case TF_TBL_TYPE_ACT_ENCAP_16B:
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_ENCAP_64B:
	case TF_TBL_TYPE_ACT_ENCAP_128B:
	case TF_TBL_TYPE_ACT_MODIFY_8B:
	case TF_TBL_TYPE_ACT_MODIFY_16B:
	case TF_TBL_TYPE_ACT_MODIFY_32B:
	case TF_TBL_TYPE_ACT_MODIFY_64B:
	{
		struct tf_resc_act_mod_enc_usage *mod_encap;
		mod_encap = &tf_resc_usage[dir].mod_encap_usage;
		mod_encap->max_entries += max_entries;
		mod_encap->free_entries += max_entries;
		break;
	}
	/* SP_SMAC Record */
	case TF_TBL_TYPE_ACT_SP_SMAC:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
	{
		struct tf_resc_act_sp_smac_usage *sp_smac;
		sp_smac = &tf_resc_usage[dir].sp_smac_usage;
		sp_smac->max_entries += max_entries;
		sp_smac->free_entries += max_entries;
		break;
	}
	/** Meter Profiles */
	case TF_TBL_TYPE_METER_PROF:
		tf_resc_usage[dir].meter_usage.max_meter_profile = max_entries;
		break;
	/** Meter Instance */
	case TF_TBL_TYPE_METER_INST:
		tf_resc_usage[dir].meter_usage.max_meter_instance = max_entries;
		break;
	default:
		break;
	}
}

/* Update the usage buffer for sram tables: add or free one entry */
int tf_tbl_usage_update(struct tf *tfp,
			 enum tf_dir dir,
			 uint32_t tbl_type,
			 enum tf_resc_opt resc_opt)
{
	struct tf_rm_element_cfg *tbl_cfg = tf_tbl_p58[dir];
	struct tf_resc_cnt_usage *cnt;
	int inc = (resc_opt == TF_RESC_ALLOC) ? 1 : -1;
	int slices = tbl_cfg[tbl_type].slices;
	int entries = 0;

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return -1;

	/* Convert to entries */
	if (slices)
		entries = inc * (16 / slices);

#if (TF_FLOW_SCALE_QUERY_DEBUG == 1)
	TFP_DRV_LOG(INFO, "%s: %s: tbl_type: %d[%s] %s, Entries: %d\n", __func__,
		    dir ? "TX" : "RX",
		    tbl_type,
		    tf_tbl_type_2_str(tbl_type),
		    resc_opt ? "Alloc" : "Free",
		    entries);
#endif /* TF_FLOW_SCALE_QUERY_DEBUG == 1 */

	resc_usage_control.buffer_dirty[dir] = 1;
	switch (tbl_type) {
	/* Counter Action */
	case TF_TBL_TYPE_ACT_STATS_64:
		cnt = &tf_resc_usage[dir].cnt_usage;
		cnt->used_entries += inc;
		break;
	/* ACTION Record */
	case TF_TBL_TYPE_FULL_ACT_RECORD:
	case TF_TBL_TYPE_COMPACT_ACT_RECORD:
	{
		struct tf_resc_act_usage *act;
		act = &tf_resc_usage[dir].act_usage;
		if (tbl_type == TF_TBL_TYPE_COMPACT_ACT_RECORD)
			act->num_compact_act_records += inc;
		else
			act->num_full_act_records += inc;
		act->free_entries -= entries;
		break;
	}
	/* ACT_ENCAP and ACT_MODIFY Records */
	case TF_TBL_TYPE_ACT_ENCAP_8B:
	case TF_TBL_TYPE_ACT_ENCAP_16B:
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_ENCAP_64B:
	case TF_TBL_TYPE_ACT_ENCAP_128B:
	case TF_TBL_TYPE_ACT_MODIFY_8B:
	case TF_TBL_TYPE_ACT_MODIFY_16B:
	case TF_TBL_TYPE_ACT_MODIFY_32B:
	case TF_TBL_TYPE_ACT_MODIFY_64B:
	{
		struct tf_resc_act_mod_enc_usage *mod_encap;
		mod_encap = &tf_resc_usage[dir].mod_encap_usage;
		switch (slices) {
		case 1:
			mod_encap->data.num_128b_records += inc;
			break;
		case 2:
			mod_encap->data.num_64b_records += inc;
			break;
		case 4:
			mod_encap->data.num_32b_records += inc;
			break;
		case 8:
			mod_encap->data.num_16b_records += inc;
			break;
		case 16:
			mod_encap->data.num_8b_records += inc;
			break;
		default:
			break;
		}
		mod_encap->free_entries -= entries;
		break;
	}
	/* SP SMAC table */
	case TF_TBL_TYPE_ACT_SP_SMAC:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
	{
		struct tf_resc_act_sp_smac_usage *sp_smac;
		sp_smac = &tf_resc_usage[dir].sp_smac_usage;
		if (tbl_type == TF_TBL_TYPE_ACT_SP_SMAC)
			sp_smac->num_sp_smac_records += inc;
		else if (tbl_type == TF_TBL_TYPE_ACT_SP_SMAC_IPV4)
			sp_smac->num_sp_smac_ipv4_records += inc;
		else if (tbl_type == TF_TBL_TYPE_ACT_SP_SMAC_IPV6)
			sp_smac->num_sp_smac_ipv6_records += inc;
		sp_smac->free_entries -= entries;
		break;
	}
	/* Meter Profiles */
	case TF_TBL_TYPE_METER_PROF:
		tf_resc_usage[dir].meter_usage.used_meter_profile += inc;
		break;
	/* Meter Instance */
	case TF_TBL_TYPE_METER_INST:
		tf_resc_usage[dir].meter_usage.used_meter_instance += inc;
		break;
	default:
	/* not support types */
		break;
	}
	return 0;
}

/* pause usage state update with firmware */
void tf_resc_pause_usage_update(void)
{
	resc_usage_control.fw_sync_paused = true;
}

/* resume usage state update with firmware */
void tf_resc_resume_usage_update(void)
{
	resc_usage_control.fw_sync_paused = false;
}

/* check if paused the resource usage update with firmware */
static bool tf_resc_usage_update_paused(void)
{
	return resc_usage_control.fw_sync_paused;
}

/* resync all resource usage state with firmware for both direction */
void tf_resc_usage_update_all(struct bnxt *bp)
{
	struct tf *tfp;
	enum tf_dir dir;

	/* When paused state update with firmware, do nothing */
	if (tf_resc_usage_update_paused())
		return;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfp || !tfp->session) {
		BNXT_DRV_DBG(ERR, "Failed to get truflow or session pointer\n");
		return;
	}

	/* Check if supported on this device */
	if (!tf_resc_usage_support(tfp))
		return;

	/* update usage state with firmware for each direction */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		if (resc_usage_control.buffer_dirty[dir]) {
			tf_update_resc_usage(tfp, dir, TF_FLOW_RESC_TYPE_ALL);
			resc_usage_control.buffer_dirty[dir] = 0;
		}
	}
}

void dump_tf_resc_usage(__rte_unused enum tf_dir dir,
			__rte_unused void *data,
			__rte_unused uint32_t size)
{
	/* empty routine */
}
#endif /* TF_FLOW_SCALE_QUERY */
