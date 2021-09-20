/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_FC_MGR_H_
#define _ULP_FC_MGR_H_

#include "bnxt_ulp.h"
#include "tf_core.h"

#define ULP_FLAG_FC_THREAD			BIT(0)
#define ULP_FC_TIMER	1/* Timer freq in Sec Flow Counters */

/* Macros to extract packet/byte counters from a 64-bit flow counter. */
#define FLOW_CNTR_BYTE_WIDTH 36
#define FLOW_CNTR_BYTE_MASK  (((uint64_t)1 << FLOW_CNTR_BYTE_WIDTH) - 1)

#define FLOW_CNTR_PKTS(v, d) (((v) & (d)->packet_count_mask) >> \
		(d)->packet_count_shift)
#define FLOW_CNTR_BYTES(v, d) (((v) & (d)->byte_count_mask) >> \
		(d)->byte_count_shift)

#define FLOW_CNTR_PC_FLOW_VALID	0x1000000

struct sw_acc_counter {
	uint64_t pkt_count;
	uint64_t byte_count;
	bool	valid;
	uint32_t hw_cntr_id;
	uint32_t pc_flow_idx;
};

struct hw_fc_mem_info {
	/*
	 * [out] mem_va, pointer to the allocated memory.
	 */
	void *mem_va;
	/*
	 * [out] mem_pa, physical address of the allocated memory.
	 */
	void *mem_pa;
	uint32_t start_idx;
	bool start_idx_is_set;
};

struct bnxt_ulp_fc_info {
	struct sw_acc_counter	*sw_acc_tbl[TF_DIR_MAX];
	struct hw_fc_mem_info	shadow_hw_tbl[TF_DIR_MAX];
	uint32_t		flags;
	uint32_t		num_entries;
	pthread_mutex_t		fc_lock;
	uint32_t		num_counters;
};

int32_t
ulp_fc_mgr_init(struct bnxt_ulp_context *ctxt);

/*
 * Release all resources in the flow counter manager for this ulp context
 *
 * ctxt [in] The ulp context for the flow counter manager
 */
int32_t
ulp_fc_mgr_deinit(struct bnxt_ulp_context *ctxt);

/*
 * Setup the Flow counter timer thread that will fetch/accumulate raw counter
 * data from the chip's internal flow counters
 *
 * ctxt [in] The ulp context for the flow counter manager
 */
int32_t
ulp_fc_mgr_thread_start(struct bnxt_ulp_context *ctxt);

/*
 * Alarm handler that will issue the TF-Core API to fetch
 * data from the chip's internal flow counters
 *
 * ctxt [in] The ulp context for the flow counter manager
 */
void
ulp_fc_mgr_alarm_cb(void *arg);

/*
 * Cancel the alarm handler
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
void ulp_fc_mgr_thread_cancel(struct bnxt_ulp_context *ctxt);

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
int ulp_fc_mgr_start_idx_set(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
			     uint32_t start_idx);

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
int ulp_fc_mgr_cntr_set(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
			uint32_t hw_cntr_id);
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
int ulp_fc_mgr_cntr_reset(struct bnxt_ulp_context *ctxt, enum tf_dir dir,
			  uint32_t hw_cntr_id);
/*
 * Check if the starting HW counter ID value is set in the
 * flow counter manager.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * dir [in] The direction of the flow
 *
 */
bool ulp_fc_mgr_start_idx_isset(struct bnxt_ulp_context *ctxt, enum tf_dir dir);

/*
 * Check if the alarm thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
bool ulp_fc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt);

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
int ulp_fc_mgr_query_count_get(struct bnxt_ulp_context *ulp_ctx,
			       uint32_t flow_id,
			       struct rte_flow_query_count *count);

/*
 * Set the parent flow if in the SW accumulator table entry
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
					uint32_t pc_idx);

#endif /* _ULP_FC_MGR_H_ */
