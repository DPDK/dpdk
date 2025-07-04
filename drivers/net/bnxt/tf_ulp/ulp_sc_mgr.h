/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_SC_MGR_H_
#define _ULP_SC_MGR_H_

#include "pthread.h"
#include "bnxt_ulp.h"
#include "ulp_flow_db.h"

#define ULP_FLAG_SC_THREAD			BIT(0)

#define ULP_SC_ENTRY_FLAG_VALID BIT(0)
#define ULP_SC_ENTRY_FLAG_PARENT BIT(1)
#define ULP_SC_PC_IDX_MASK 0xFFFFF

#define ULP_SC_BATCH_SIZE   64
#define ULP_SC_PAGE_SIZE  4096

struct ulp_sc_tfc_stats_cache_entry {
	struct bnxt_ulp_context *ctxt;
	uint32_t flags : 8;
	uint32_t pc_idx : 24;
	uint64_t timestamp;
	uint64_t handle;
	uint8_t dir;
	uint64_t packet_count;
	uint64_t byte_count;
	uint64_t count_fields1;
	uint64_t count_fields2;
	bool reset;
};

struct bnxt_ulp_sc_info {
	struct ulp_sc_tfc_stats_cache_entry *stats_cache_tbl;
	uint8_t		*read_data;
	uint64_t	read_data_iova[ULP_SC_BATCH_SIZE];
	uint32_t	flags;
	uint32_t	num_entries;
	uint32_t	num_counters;
	uint32_t	cache_tbl_size;
	rte_thread_t	tid;
	const struct bnxt_ulp_sc_core_ops *sc_ops;
};

struct bnxt_ulp_sc_core_ops {
	int32_t
	(*ulp_stats_cache_update)(struct tfc *tfcp,
				  int dir,
				  uint64_t *host_address,
				  uint64_t handle,
				  uint16_t *words,
				  struct tfc_mpc_batch_info_t *batch_info,
				  bool reset);
};

/*
 * Allocate all resources in the stats cache manager for this ulp context
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
int32_t
ulp_sc_mgr_init(struct bnxt_ulp_context *ctxt);

/*
 * Release all resources in the stats cache manager for this ulp context
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
int32_t
ulp_sc_mgr_deinit(struct bnxt_ulp_context *ctxt);

/*
 * Setup the stats cache timer thread that will fetch/accumulate raw counter
 * data from the chip's internal stats caches
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
int32_t
ulp_sc_mgr_thread_start(struct bnxt_ulp_context *ctxt);

/*
 * Alarm handler that will issue the TF-Core API to fetch
 * data from the chip's internal stats caches
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
void
ulp_sc_mgr_alarm_cb(void *arg);

/*
 * Cancel the alarm handler
 *
 * ctxt [in] The ulp context for the stats cache manager
 *
 */
void ulp_sc_mgr_thread_cancel(struct bnxt_ulp_context *ctxt);

/*
 * Check if the thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the stats cache manager
 *
 */
bool ulp_sc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt);

/*
 * Get the current counts for the given flow id
 *
 * ctxt [in] The ulp context for the stats cache manager
 * flow_id [in] The flow identifier
 * count [out] structure in which the updated counts are passed
 * back to the caller.
 *
 */
int ulp_sc_mgr_query_count_get(struct bnxt_ulp_context *ctxt,
			       uint32_t flow_id,
			       struct rte_flow_query_count *count);

/*
 * Allocate a cache entry for flow
 *
 * parms [in] Various fields used to identify the flow
 * counter_handle [in] This is the action table entry identifier.
 * tbl [in] Various fields used to identify the flow
 *
 */
int ulp_sc_mgr_entry_alloc(struct bnxt_ulp_mapper_parms *parms,
			   uint64_t counter_handle,
			   struct bnxt_ulp_mapper_tbl_info *tbl);

/*
 * Free cache entry
 *
 * ulp [in] The ulp context for the stats cache manager
 * fid [in] The flow identifier
 *
 */
void ulp_sc_mgr_entry_free(struct bnxt_ulp_context *ulp,
			   uint32_t fid);


/*
 * Set pc_idx for the flow if stat cache info is valid
 *
 * ctxt [in] The ulp context for the flow counter manager
 * flow_id [in] The HW flow ID
 * pc_idx [in] The parent flow entry idx
 *
 */
void ulp_sc_mgr_set_pc_idx(struct bnxt_ulp_context *ctxt,
			   uint32_t flow_id,
			   uint32_t pc_idx);

extern const struct bnxt_ulp_sc_core_ops ulp_sc_tfc_core_ops;

#endif /* _ULP_SC_MGR_H_ */
