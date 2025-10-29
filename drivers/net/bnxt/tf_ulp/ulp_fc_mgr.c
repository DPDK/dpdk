/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
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

static const struct bnxt_ulp_fc_core_ops *
bnxt_ulp_fc_ops_get(struct bnxt_ulp_context *ctxt)
{
	int32_t rc;
	enum bnxt_ulp_device_id  dev_id;
	const struct bnxt_ulp_fc_core_ops *func_ops;

	rc = bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id);
	if (rc)
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &ulp_fc_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_STINGRAY:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		func_ops = &ulp_fc_tf_core_ops;
		break;
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
}

static int
ulp_fc_mgr_shadow_mem_alloc(struct hw_fc_mem_info *parms, int size)
{
	/* Allocate memory*/
	if (!parms)
		return -EINVAL;

	parms->mem_va = rte_zmalloc("ulp_fc_info",
				    RTE_CACHE_LINE_ROUNDUP(size),
				    4096);
	if (!parms->mem_va) {
		BNXT_DRV_DBG(ERR, "Allocate failed mem_va\n");
		return -ENOMEM;
	}

	rte_mem_lock_page(parms->mem_va);

	parms->mem_pa = (void *)(uintptr_t)rte_mem_virt2phy(parms->mem_va);
	if (parms->mem_pa == (void *)RTE_BAD_IOVA) {
		BNXT_DRV_DBG(ERR, "Allocate failed mem_pa\n");
		return -ENOMEM;
	}

	return 0;
}

static void
ulp_fc_mgr_shadow_mem_free(struct hw_fc_mem_info *parms)
{
	rte_free(parms->mem_va);
}

/*
 * Allocate and Initialize all Flow Counter Manager resources for this ulp
 * context.
 *
 * ctxt [in] The ulp context for the Flow Counter manager.
 *
 */
int32_t
ulp_fc_mgr_init(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_device_params *dparms;
	uint32_t dev_id, sw_acc_cntr_tbl_sz, hw_fc_mem_info_sz;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	const struct bnxt_ulp_fc_core_ops *fc_ops;
	uint32_t flags = 0;
	int i, rc;

	if (!ctxt) {
		BNXT_DRV_DBG(DEBUG, "Invalid ULP CTXT\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to device parms\n");
		return -EINVAL;
	}

	/* update the features list */
	if (dparms->dev_features & BNXT_ULP_DEV_FT_STAT_SW_AGG)
		flags = ULP_FLAG_FC_SW_AGG_EN;
	if (dparms->dev_features & BNXT_ULP_DEV_FT_STAT_PARENT_AGG)
		flags |= ULP_FLAG_FC_PARENT_AGG_EN;

	fc_ops = bnxt_ulp_fc_ops_get(ctxt);
	if (fc_ops == NULL) {
		BNXT_DRV_DBG(DEBUG, "Failed to get the counter ops\n");
		return -EINVAL;
	}

	ulp_fc_info = rte_zmalloc("ulp_fc_info", sizeof(*ulp_fc_info), 0);
	if (!ulp_fc_info)
		goto error;

	ulp_fc_info->fc_ops = fc_ops;
	ulp_fc_info->flags = flags;

	pthread_mutex_init(&ulp_fc_info->fc_lock, NULL);

	/* Add the FC info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_fc_info_set(ctxt, ulp_fc_info);

	ulp_fc_info->num_counters = dparms->flow_count_db_entries;
	if (!ulp_fc_info->num_counters) {
		/* No need for software counters, call fw directly */
		BNXT_DRV_DBG(DEBUG, "Sw flow counter support not enabled\n");
		return 0;
	}

	/* no need to allocate sw aggregation memory if agg is disabled */
	if (!(ulp_fc_info->flags & ULP_FLAG_FC_SW_AGG_EN))
		return 0;

	sw_acc_cntr_tbl_sz = sizeof(struct sw_acc_counter) *
				dparms->flow_count_db_entries;

	for (i = 0; i < TF_DIR_MAX; i++) {
		ulp_fc_info->sw_acc_tbl[i] = rte_zmalloc("ulp_sw_acc_cntr_tbl",
							 sw_acc_cntr_tbl_sz, 0);
		if (!ulp_fc_info->sw_acc_tbl[i])
			goto error;
	}

	hw_fc_mem_info_sz = sizeof(uint64_t) * dparms->flow_count_db_entries;

	for (i = 0; i < TF_DIR_MAX; i++) {
		rc = ulp_fc_mgr_shadow_mem_alloc(&ulp_fc_info->shadow_hw_tbl[i],
						 hw_fc_mem_info_sz);
		if (rc)
			goto error;
	}

	return 0;

error:
	ulp_fc_mgr_deinit(ctxt);
	BNXT_DRV_DBG(DEBUG, "Failed to allocate memory for fc mgr\n");

	return -ENOMEM;
}

/*
 * Release all resources in the Flow Counter Manager for this ulp context
 *
 * ctxt [in] The ulp context for the Flow Counter manager
 *
 */
int32_t
ulp_fc_mgr_deinit(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;
	struct hw_fc_mem_info *shd_info;
	int i;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);

	if (!ulp_fc_info)
		return -EINVAL;

	if (ulp_fc_info->flags & ULP_FLAG_FC_SW_AGG_EN)
		ulp_fc_mgr_thread_cancel(ctxt);

	pthread_mutex_destroy(&ulp_fc_info->fc_lock);

	if (ulp_fc_info->flags & ULP_FLAG_FC_SW_AGG_EN) {
		for (i = 0; i < TF_DIR_MAX; i++)
			rte_free(ulp_fc_info->sw_acc_tbl[i]);

		for (i = 0; i < TF_DIR_MAX; i++) {
			shd_info = &ulp_fc_info->shadow_hw_tbl[i];
			ulp_fc_mgr_shadow_mem_free(shd_info);
		}
	}

	rte_free(ulp_fc_info);

	/* Safe to ignore on deinit */
	(void)bnxt_ulp_cntxt_ptr2_fc_info_set(ctxt, NULL);

	return 0;
}

/*
 * Check if the alarm thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
bool ulp_fc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);

	if (ulp_fc_info)
		return !!(ulp_fc_info->flags & ULP_FLAG_FC_THREAD);

	return false;
}

/*
 * Setup the Flow counter timer thread that will fetch/accumulate raw counter
 * data from the chip's internal flow counters
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
int32_t
ulp_fc_mgr_thread_start(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);

	if (ulp_fc_info && !(ulp_fc_info->flags & ULP_FLAG_FC_THREAD)) {
		rte_eal_alarm_set(US_PER_S * ULP_FC_TIMER,
				  ulp_fc_mgr_alarm_cb, (void *)ctxt->cfg_data);
		ulp_fc_info->flags |= ULP_FLAG_FC_THREAD;
	}

	return 0;
}

/*
 * Cancel the alarm handler
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
void ulp_fc_mgr_thread_cancel(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return;

	ulp_fc_info->flags &= ~ULP_FLAG_FC_THREAD;
	rte_eal_alarm_cancel(ulp_fc_mgr_alarm_cb, ctxt->cfg_data);
}

/*
 * Alarm handler that will issue the TF-Core API to fetch
 * data from the chip's internal flow counters
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */

void
ulp_fc_mgr_alarm_cb(void *arg)
{
	const struct bnxt_ulp_fc_core_ops *fc_ops;
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	struct bnxt_ulp_context *ctxt;
	uint32_t dev_id;
	int rc = 0;

	ctxt = bnxt_ulp_cntxt_entry_acquire(arg);
	if (ctxt == NULL) {
		BNXT_DRV_DBG(INFO, "could not get the ulp context lock\n");
		rte_eal_alarm_set(US_PER_S * ULP_FC_TIMER,
				  ulp_fc_mgr_alarm_cb, arg);
		return;
	}

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info) {
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	fc_ops = ulp_fc_info->fc_ops;

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device id\n");
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to device parms\n");
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	/*
	 * Take the fc_lock to ensure no flow is destroyed
	 * during the bulk get
	 */
	if (pthread_mutex_trylock(&ulp_fc_info->fc_lock))
		goto out;

	if (!ulp_fc_info->num_entries) {
		pthread_mutex_unlock(&ulp_fc_info->fc_lock);
		ulp_fc_mgr_thread_cancel(ctxt);
		bnxt_ulp_cntxt_entry_release();
		return;
	}
	/*
	 * Commented for now till GET_BULK is resolved, just get the first flow
	 * stat for now
	 for (i = 0; i < TF_DIR_MAX; i++) {
		rc = ulp_bulk_get_flow_stats(tfp, ulp_fc_info, i,
					     dparms->flow_count_db_entries);
		if (rc)
			break;
	}
	*/

	/* reset the parent accumulation counters before accumulation if any */
	ulp_flow_db_parent_flow_count_reset(ctxt);

	rc = fc_ops->ulp_flow_stats_accum_update(ctxt, ulp_fc_info, dparms);

	pthread_mutex_unlock(&ulp_fc_info->fc_lock);

	/*
	 * If cmd fails once, no need of
	 * invoking again every second
	 */

	if (rc) {
		ulp_fc_mgr_thread_cancel(ctxt);
		bnxt_ulp_cntxt_entry_release();
		return;
	}
out:
	bnxt_ulp_cntxt_entry_release();
	rte_eal_alarm_set(US_PER_S * ULP_FC_TIMER,
			  ulp_fc_mgr_alarm_cb, arg);
}

/*
 * Set the starting index that indicates the first HW flow
 * counter ID
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 * start_idx [in] The HW flow counter ID
 *
 */
bool ulp_fc_mgr_start_idx_isset(struct bnxt_ulp_context *ctxt, uint8_t dir)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);

	if (ulp_fc_info)
		return ulp_fc_info->shadow_hw_tbl[dir].start_idx_is_set;

	return false;
}

/*
 * Set the starting index that indicates the first HW flow
 * counter ID
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 * start_idx [in] The HW flow counter ID
 *
 */
int32_t ulp_fc_mgr_start_idx_set(struct bnxt_ulp_context *ctxt, uint8_t dir,
				 uint32_t start_idx)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);

	if (!ulp_fc_info)
		return -EIO;

	if (!ulp_fc_info->shadow_hw_tbl[dir].start_idx_is_set) {
		ulp_fc_info->shadow_hw_tbl[dir].start_idx = start_idx;
		ulp_fc_info->shadow_hw_tbl[dir].start_idx_is_set = true;
	}

	return 0;
}

/*
 * Set the corresponding SW accumulator table entry based on
 * the difference between this counter ID and the starting
 * counter ID. Also, keep track of num of active counter enabled
 * flows.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 * hw_cntr_id [in] The HW flow counter ID
 *
 */
int32_t ulp_fc_mgr_cntr_set(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
			    uint32_t hw_cntr_id,
			    enum bnxt_ulp_session_type session_type)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;
	uint32_t sw_cntr_idx;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -EIO;

	if (!ulp_fc_info->num_counters)
		return 0;

	pthread_mutex_lock(&ulp_fc_info->fc_lock);
	sw_cntr_idx = hw_cntr_id - ulp_fc_info->shadow_hw_tbl[dir].start_idx;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].valid = true;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].hw_cntr_id = hw_cntr_id;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].session_type = session_type;
	ulp_fc_info->num_entries++;
	pthread_mutex_unlock(&ulp_fc_info->fc_lock);

	return 0;
}

/*
 * Reset the corresponding SW accumulator table entry based on
 * the difference between this counter ID and the starting
 * counter ID.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 * hw_cntr_id [in] The HW flow counter ID
 *
 */
int32_t ulp_fc_mgr_cntr_reset(struct bnxt_ulp_context *ctxt, uint8_t dir,
			      uint32_t hw_cntr_id)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;
	uint32_t sw_cntr_idx;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -EIO;

	if (!ulp_fc_info->num_counters)
		return 0;

	pthread_mutex_lock(&ulp_fc_info->fc_lock);
	sw_cntr_idx = hw_cntr_id - ulp_fc_info->shadow_hw_tbl[dir].start_idx;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].valid = false;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].hw_cntr_id = 0;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].session_type = 0;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].pkt_count = 0;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].byte_count = 0;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].pc_flow_idx = 0;
	ulp_fc_info->num_entries--;
	pthread_mutex_unlock(&ulp_fc_info->fc_lock);

	return 0;
}

/*
 * Fill the rte_flow_query_count 'data' argument passed
 * in the rte_flow_query() with the values obtained and
 * accumulated locally.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * flow_id [in] The HW flow ID
 *
 * count [out] The rte_flow_query_count 'data' that is set
 *
 */
int ulp_fc_mgr_query_count_get(struct bnxt_ulp_context *ctxt,
			       uint32_t flow_id,
			       struct rte_flow_query_count *count)
{
	int rc = 0;
	uint32_t nxt_resource_index = 0;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	const struct bnxt_ulp_fc_core_ops *fc_ops;
	struct ulp_flow_db_res_params params;
	uint32_t hw_cntr_id = 0, sw_cntr_idx = 0;
	struct sw_acc_counter *sw_acc_tbl_entry;
	bool found_cntr_resource = false;
	bool found_parent_flow = false;
	uint32_t pc_idx = 0;
	uint32_t session_type = 0;
	uint8_t dir;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -ENODEV;

	fc_ops = ulp_fc_info->fc_ops;

	if (bnxt_ulp_cntxt_acquire_fdb_lock(ctxt))
		return -EIO;

	do {
		rc = ulp_flow_db_resource_get(ctxt,
					      BNXT_ULP_FDB_TYPE_REGULAR,
					      flow_id,
					      &nxt_resource_index,
					      &params);
		if (params.resource_func ==
		     BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE &&
		     (params.resource_sub_type ==
		      BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT ||
		      params.resource_sub_type ==
		      BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_EXT_COUNT)) {
			found_cntr_resource = true;
			break;
		}
		if (params.resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
			found_cntr_resource = true;
			break;
		}
		if (params.resource_func ==
		    BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW) {
			found_parent_flow = true;
			pc_idx = params.resource_hndl;
		}

	} while (!rc && nxt_resource_index);

	if (rc || !found_cntr_resource) {
		bnxt_ulp_cntxt_release_fdb_lock(ctxt);
		return rc;
	}

	dir = params.direction;
	session_type = ulp_flow_db_shared_session_get(&params);
	if (!(ulp_fc_info->flags & ULP_FLAG_FC_SW_AGG_EN)) {
		rc = fc_ops->ulp_flow_stat_get(ctxt, dir, session_type,
					       params.resource_hndl, count);
		bnxt_ulp_cntxt_release_fdb_lock(ctxt);
		return rc;
	}

	if (!found_parent_flow &&
	    params.resource_sub_type ==
			BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		hw_cntr_id = params.resource_hndl;
		if (!ulp_fc_info->num_counters) {
			rc = fc_ops->ulp_flow_stat_get(ctxt, dir, session_type,
						       hw_cntr_id, count);
			bnxt_ulp_cntxt_release_fdb_lock(ctxt);
			return rc;
		}

		/* TODO:
		 * Think about optimizing with try_lock later
		 */
		pthread_mutex_lock(&ulp_fc_info->fc_lock);
		sw_cntr_idx = hw_cntr_id -
			ulp_fc_info->shadow_hw_tbl[dir].start_idx;
		sw_acc_tbl_entry = &ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx];
		if (sw_acc_tbl_entry->pkt_count) {
			count->hits_set = 1;
			count->bytes_set = 1;
			count->hits = sw_acc_tbl_entry->pkt_count;
			count->bytes = sw_acc_tbl_entry->byte_count;
		}
		if (count->reset) {
			sw_acc_tbl_entry->pkt_count = 0;
			sw_acc_tbl_entry->byte_count = 0;
		}
		pthread_mutex_unlock(&ulp_fc_info->fc_lock);
	} else if (found_parent_flow &&
		   params.resource_sub_type ==
			BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		/* Get stats from the parent child table */
		if (ulp_flow_db_parent_flow_count_get(ctxt, flow_id,
						      pc_idx,
						      &count->hits,
						      &count->bytes,
						      count->reset)) {
			bnxt_ulp_cntxt_release_fdb_lock(ctxt);
			return -EIO;
		}
		if (count->hits)
			count->hits_set = 1;
		if (count->bytes)
			count->bytes_set = 1;
	} else {
		rc = -EINVAL;
	}
	bnxt_ulp_cntxt_release_fdb_lock(ctxt);
	return rc;
}

/*
 * Fill the rte_mtr_stats 'mtr_count' argument passed
 * in the rte_mtr_stats_read() with the values obtained
 * through CFA table get.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * mtr_id [in] The SW meter ID
 *
 * count [out] The rte_mtr_stats 'mtr_count' that is set
 *
 */
int ulp_mtr_query_count_get(struct bnxt_ulp_context *ctxt,
			    uint32_t mtr_id,
			    int clear,
			    struct rte_mtr_stats *mtr_count)
{
	int rc = 0;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	const struct bnxt_ulp_fc_core_ops *fc_ops;
	struct ulp_flow_db_res_params params;
	uint32_t session_type = 0;
	uint8_t dir = 0;
	const struct ulp_mapper_core_ops *oper;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -ENODEV;

	oper = ulp_mapper_data_oper_get(ctxt);
	if (!oper)
		return -ENODEV;
	rc = oper->ulp_mapper_mtr_stats_hndl_get(mtr_id, &params.resource_hndl);
	if (rc)
		return rc;

	fc_ops = ulp_fc_info->fc_ops;
	if (!fc_ops || !fc_ops->ulp_mtr_stat_get)
		return -ENODEV;

	rc = fc_ops->ulp_mtr_stat_get(ctxt, dir, session_type,
				      params.resource_hndl, clear, mtr_count);

	return rc;
}

/*
 * Set the parent flow if it is SW accumulation counter entry.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 * hw_cntr_id [in] The HW flow counter ID
 *
 * pc_idx [in] parent child db index
 *
 */
int32_t ulp_fc_mgr_cntr_parent_flow_set(struct bnxt_ulp_context *ctxt,
					uint8_t dir,
					uint32_t hw_cntr_id,
					uint32_t pc_idx)
{
	struct bnxt_ulp_fc_info *ulp_fc_info;
	uint32_t sw_cntr_idx;
	int32_t rc = 0;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -EIO;

	pthread_mutex_lock(&ulp_fc_info->fc_lock);
	sw_cntr_idx = hw_cntr_id - ulp_fc_info->shadow_hw_tbl[dir].start_idx;
	if (ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].valid) {
		pc_idx |= FLOW_CNTR_PC_FLOW_VALID;
		ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].pc_flow_idx = pc_idx;
	} else {
		BNXT_DRV_DBG(ERR, "Failed to set parent flow id %x:%x\n",
			     hw_cntr_id, pc_idx);
		rc = -ENOENT;
	}
	pthread_mutex_unlock(&ulp_fc_info->fc_lock);

	return rc;
}
