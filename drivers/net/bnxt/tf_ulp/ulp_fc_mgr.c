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
#include "bnxt_tf_common.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tf_tbl.h"

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
		BNXT_TF_DBG(ERR, "Allocate failed mem_va\n");
		return -ENOMEM;
	}

	rte_mem_lock_page(parms->mem_va);

	parms->mem_pa = (void *)(uintptr_t)rte_mem_virt2phy(parms->mem_va);
	if (parms->mem_pa == (void *)RTE_BAD_IOVA) {
		BNXT_TF_DBG(ERR, "Allocate failed mem_pa\n");
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
	int i, rc;

	if (!ctxt) {
		BNXT_TF_DBG(DEBUG, "Invalid ULP CTXT\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_TF_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(DEBUG, "Failed to device parms\n");
		return -EINVAL;
	}

	ulp_fc_info = rte_zmalloc("ulp_fc_info", sizeof(*ulp_fc_info), 0);
	if (!ulp_fc_info)
		goto error;

	rc = pthread_mutex_init(&ulp_fc_info->fc_lock, NULL);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to initialize fc mutex\n");
		goto error;
	}

	/* Add the FC info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_fc_info_set(ctxt, ulp_fc_info);

	ulp_fc_info->num_counters = dparms->flow_count_db_entries;
	if (!ulp_fc_info->num_counters) {
		/* No need for software counters, call fw directly */
		BNXT_TF_DBG(DEBUG, "Sw flow counter support not enabled\n");
		return 0;
	}

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
	BNXT_TF_DBG(DEBUG,
		    "Failed to allocate memory for fc mgr\n");

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

	ulp_fc_mgr_thread_cancel(ctxt);

	pthread_mutex_destroy(&ulp_fc_info->fc_lock);

	if (ulp_fc_info->num_counters) {
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
__rte_unused static int32_t
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
		PMD_DRV_LOG(ERR,
			    "BULK: Memory not initialized id:0x%x dir:%d\n",
			    parms.starting_idx, dir);
		return -EINVAL;
	}

	rc = tf_tbl_bulk_get(tfp, &parms);
	if (rc) {
		PMD_DRV_LOG(ERR,
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

static int32_t
ulp_fc_tf_flow_stat_get(struct bnxt_ulp_context *ctxt,
			struct ulp_flow_db_res_params *res,
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

	tfp = bnxt_ulp_cntxt_tfp_get(ctxt, BNXT_ULP_SHARED_SESSION_NO);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get the truflow pointer\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_TF_DBG(DEBUG, "Failed to get device id\n");
		bnxt_ulp_cntxt_entry_release();
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(DEBUG, "Failed to device parms\n");
		bnxt_ulp_cntxt_entry_release();
		return -EINVAL;
	}
	parms.dir = res->direction;
	parms.type = stype;
	parms.idx = res->resource_hndl;
	parms.data_sz_in_bytes = sizeof(uint64_t);
	parms.data = (uint8_t *)&stats;
	rc = tf_get_tbl_entry(tfp, &parms);
	if (rc) {
		PMD_DRV_LOG(ERR,
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
		sparms.dir = res->direction;
		sparms.type = stype;
		sparms.idx = res->resource_hndl;
		sparms.data = (uint8_t *)&stats;
		sparms.data_sz_in_bytes = sizeof(uint64_t);
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			PMD_DRV_LOG(ERR, "Set failed for id:0x%x rc:%d\n",
				    sparms.idx, rc);
			return rc;
		}
	}
	return rc;
}

static int ulp_get_single_flow_stat(struct bnxt_ulp_context *ctxt,
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
		PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR, "Error updating parent counters\n");
		}
	}

	return rc;
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
	int rc = 0;
	unsigned int j;
	enum tf_dir i;
	struct bnxt_ulp_context *ctxt;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	struct bnxt_ulp_device_params *dparms;
	struct tf *tfp;
	uint32_t dev_id, hw_cntr_id = 0, num_entries = 0;

	ctxt = bnxt_ulp_cntxt_entry_acquire(arg);
	if (ctxt == NULL) {
		BNXT_TF_DBG(INFO, "could not get the ulp context lock\n");
		rte_eal_alarm_set(US_PER_S * ULP_FC_TIMER,
				  ulp_fc_mgr_alarm_cb, arg);
		return;
	}

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info) {
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_TF_DBG(DEBUG, "Failed to get device id\n");
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(DEBUG, "Failed to device parms\n");
		bnxt_ulp_cntxt_entry_release();
		return;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(ctxt, BNXT_ULP_SHARED_SESSION_NO);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get the truflow pointer\n");
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

	num_entries = dparms->flow_count_db_entries / 2;
	for (i = 0; i < TF_DIR_MAX; i++) {
		for (j = 0; j < num_entries; j++) {
			if (!ulp_fc_info->sw_acc_tbl[i][j].valid)
				continue;
			hw_cntr_id = ulp_fc_info->sw_acc_tbl[i][j].hw_cntr_id;
			rc = ulp_get_single_flow_stat(ctxt, tfp, ulp_fc_info, i,
						      hw_cntr_id, dparms);
			if (rc)
				break;
		}
	}

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
bool ulp_fc_mgr_start_idx_isset(struct bnxt_ulp_context *ctxt, enum tf_dir dir)
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
int32_t ulp_fc_mgr_start_idx_set(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
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
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].valid = true;
	ulp_fc_info->sw_acc_tbl[dir][sw_cntr_idx].hw_cntr_id = hw_cntr_id;
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
int32_t ulp_fc_mgr_cntr_reset(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
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
	struct ulp_flow_db_res_params params;
	enum tf_dir dir;
	uint32_t hw_cntr_id = 0, sw_cntr_idx = 0;
	struct sw_acc_counter *sw_acc_tbl_entry;
	bool found_cntr_resource = false;
	bool found_parent_flow = false;
	uint32_t pc_idx = 0;

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ctxt);
	if (!ulp_fc_info)
		return -ENODEV;

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
		if (params.resource_func ==
		    BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW) {
			found_parent_flow = true;
			pc_idx = params.resource_hndl;
		}

	} while (!rc && nxt_resource_index);

	bnxt_ulp_cntxt_release_fdb_lock(ctxt);

	if (rc || !found_cntr_resource)
		return rc;

	dir = params.direction;
	hw_cntr_id = params.resource_hndl;
	if (!found_parent_flow &&
	    params.resource_sub_type ==
			BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		if (!ulp_fc_info->num_counters)
			return ulp_fc_tf_flow_stat_get(ctxt, &params, count);

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
		ulp_flow_db_parent_flow_count_get(ctxt, pc_idx,
						  &count->hits, &count->bytes,
						  count->reset);
		if (count->hits)
			count->hits_set = 1;
		if (count->bytes)
			count->bytes_set = 1;
	} else {
		/* TBD: Handle External counters */
		rc = -EINVAL;
	}

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
					enum tf_dir dir,
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
		BNXT_TF_DBG(ERR, "Failed to set parent flow id %x:%x\n",
			    hw_cntr_id, pc_idx);
		rc = -ENOENT;
	}
	pthread_mutex_unlock(&ulp_fc_info->fc_lock);

	return rc;
}
