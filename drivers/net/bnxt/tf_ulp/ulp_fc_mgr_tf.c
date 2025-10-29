/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_alarm.h>
#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_ulp_tf.h"
#include "bnxt_tf_common.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tf_tbl.h"

/*
 * DMA-in the raw counter data from the HW and accumulate in the
 * local accumulator table using the TF-Core API
 *
 * tfp [in] The TF-Core context
 *
 * fc_info [in] The ULP Flow counter info ptr
 *
 * dir [in] The direction of the flow
 *
 * num_counters [in] The number of counters
 *
 */
static int32_t __rte_unused
ulp_bulk_get_flow_stats(struct tf *tfp,
			struct bnxt_ulp_fc_info *fc_info,
			enum tf_dir dir,
			struct bnxt_ulp_device_params *dparms)
/* MARK AS UNUSED FOR NOW TO AVOID COMPILATION ERRORS TILL API is RESOLVED */
{
	int rc = 0;
	struct tf_tbl_get_bulk_parms parms = { 0 };
	enum tf_tbl_type stype = TF_TBL_TYPE_ACT_STATS_64;  /* TBD: Template? */
	struct sw_acc_counter *sw_acc_tbl_entry = NULL;
	uint64_t *stats = NULL;
	uint16_t i = 0;

	parms.dir = dir;
	parms.type = stype;
	parms.starting_idx = fc_info->shadow_hw_tbl[dir].start_idx;
	parms.num_entries = dparms->flow_count_db_entries / 2; /* direction */
	/*
	 * TODO:
	 * Size of an entry needs to obtained from template
	 */
	parms.entry_sz_in_bytes = sizeof(uint64_t);
	stats = (uint64_t *)fc_info->shadow_hw_tbl[dir].mem_va;
	parms.physical_mem_addr = (uint64_t)
		((uintptr_t)(fc_info->shadow_hw_tbl[dir].mem_pa));

	if (!stats) {
		BNXT_DRV_DBG(ERR,
			     "BULK: Memory not initialized id:0x%x dir:%d\n",
			     parms.starting_idx, dir);
		return -EINVAL;
	}

	rc = tf_tbl_bulk_get(tfp, &parms);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "BULK: Get failed for id:0x%x rc:%d\n",
			     parms.starting_idx, rc);
		return rc;
	}

	for (i = 0; i < parms.num_entries; i++) {
		/* TBD - Get PKT/BYTE COUNT SHIFT/MASK from Template */
		sw_acc_tbl_entry = &fc_info->sw_acc_tbl[dir][i];
		if (!sw_acc_tbl_entry->valid)
			continue;
		sw_acc_tbl_entry->pkt_count += FLOW_CNTR_PKTS(stats[i],
							      dparms);
		sw_acc_tbl_entry->byte_count += FLOW_CNTR_BYTES(stats[i],
								dparms);
	}

	return rc;
}

static int ulp_fc_tf_flow_stat_update(struct bnxt_ulp_context *ctxt,
				      struct tf *tfp,
				      struct bnxt_ulp_fc_info *fc_info,
				      enum tf_dir dir,
				      uint32_t hw_cntr_id,
				      struct bnxt_ulp_device_params *dparms)
{
	int rc = 0;
	struct tf_get_tbl_entry_parms parms = { 0 };
	enum tf_tbl_type stype = TF_TBL_TYPE_ACT_STATS_64;  /* TBD:Template? */
	struct sw_acc_counter *sw_acc_tbl_entry = NULL, *t_sw;
	uint64_t stats = 0;
	uint32_t sw_cntr_indx = 0;

	parms.dir = dir;
	parms.type = stype;
	parms.idx = hw_cntr_id;
	/*
	 * TODO:
	 * Size of an entry needs to obtained from template
	 */
	parms.data_sz_in_bytes = sizeof(uint64_t);
	parms.data = (uint8_t *)&stats;
	rc = tf_get_tbl_entry(tfp, &parms);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Get failed for id:0x%x rc:%d\n",
			     parms.idx, rc);
		return rc;
	}

	/* PKT/BYTE COUNT SHIFT/MASK are device specific */
	sw_cntr_indx = hw_cntr_id - fc_info->shadow_hw_tbl[dir].start_idx;
	sw_acc_tbl_entry = &fc_info->sw_acc_tbl[dir][sw_cntr_indx];

	/* Some dpdk applications may accumulate the flow counters while some
	 * may not. In cases where the application is accumulating the counters
	 * the PMD need not do the accumulation itself and viceversa to report
	 * the correct flow counters.
	 */
	sw_acc_tbl_entry->pkt_count += FLOW_CNTR_PKTS(stats, dparms);
	sw_acc_tbl_entry->byte_count += FLOW_CNTR_BYTES(stats, dparms);

	/* Update the parent counters if it is child flow */
	if (sw_acc_tbl_entry->pc_flow_idx & FLOW_CNTR_PC_FLOW_VALID) {
		uint32_t pc_idx;

		/* Update the parent counters */
		t_sw = sw_acc_tbl_entry;
		pc_idx = t_sw->pc_flow_idx & ~FLOW_CNTR_PC_FLOW_VALID;
		if (ulp_flow_db_parent_flow_count_update(ctxt, pc_idx,
							 t_sw->pkt_count,
							 t_sw->byte_count)) {
			BNXT_DRV_DBG(ERR, "Error updating parent counters\n");
		}
	}

	return rc;
}

static int32_t
ulp_fc_tf_update_accum_stats(struct bnxt_ulp_context *ctxt,
			     struct bnxt_ulp_fc_info *fc_info,
			     struct bnxt_ulp_device_params *dparms)
{
	uint32_t hw_cntr_id = 0, num_entries = 0, j;
	int32_t rc = 0;
	enum tf_dir dir;
	struct tf *tfp;

	num_entries = dparms->flow_count_db_entries / 2;
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		for (j = 0; j < num_entries; j++) {
			if (!fc_info->sw_acc_tbl[dir][j].valid)
				continue;
			tfp = bnxt_ulp_cntxt_tfp_get(ctxt,
						     fc_info->sw_acc_tbl[dir][j].session_type);
			if (!tfp) {
				BNXT_DRV_DBG(ERR,
					     "Failed to get the tfp\n");
				return 0;
			}

			hw_cntr_id = fc_info->sw_acc_tbl[dir][j].hw_cntr_id;

			rc = ulp_fc_tf_flow_stat_update(ctxt, tfp, fc_info, dir,
							hw_cntr_id, dparms);
			if (rc)
				break;
		}
	}

	return rc;
}

static int32_t
ulp_fc_tf_flow_stat_get(struct bnxt_ulp_context *ctxt,
			uint8_t direction,
			uint32_t session_type,
			uint64_t handle,
			struct rte_flow_query_count *qcount)
{
	struct tf *tfp;
	struct bnxt_ulp_device_params *dparms;
	struct tf_get_tbl_entry_parms parms = { 0 };
	struct tf_set_tbl_entry_parms	sparms = { 0 };
	enum tf_tbl_type stype = TF_TBL_TYPE_ACT_STATS_64;
	uint64_t stats = 0;
	uint32_t dev_id = 0;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ctxt, session_type);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get the truflow pointer\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device id\n");
		bnxt_ulp_cntxt_entry_release();
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to device parms\n");
		bnxt_ulp_cntxt_entry_release();
		return -EINVAL;
	}
	parms.dir = (enum tf_dir)direction;
	parms.type = stype;
	parms.idx = (uint32_t)handle;
	parms.data_sz_in_bytes = sizeof(uint64_t);
	parms.data = (uint8_t *)&stats;
	rc = tf_get_tbl_entry(tfp, &parms);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Get failed for id:0x%x rc:%d\n",
			     parms.idx, rc);
		return rc;
	}
	qcount->hits = FLOW_CNTR_PKTS(stats, dparms);
	if (qcount->hits)
		qcount->hits_set = 1;
	qcount->bytes = FLOW_CNTR_BYTES(stats, dparms);
	if (qcount->bytes)
		qcount->bytes_set = 1;

	if (qcount->reset) {
		stats = 0;
		sparms.dir = (enum tf_dir)direction;
		sparms.type = stype;
		sparms.idx = (uint32_t)handle;
		sparms.data = (uint8_t *)&stats;
		sparms.data_sz_in_bytes = sizeof(uint64_t);
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Set failed for id:0x%x rc:%d\n",
				     sparms.idx, rc);
			return rc;
		}
	}
	return rc;
}

const struct bnxt_ulp_fc_core_ops ulp_fc_tf_core_ops = {
	.ulp_flow_stat_get = ulp_fc_tf_flow_stat_get,
	.ulp_flow_stats_accum_update = ulp_fc_tf_update_accum_stats,
	.ulp_mtr_stat_get = NULL,
};
