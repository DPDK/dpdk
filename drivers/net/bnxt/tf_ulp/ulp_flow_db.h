/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_FLOW_DB_H_
#define _ULP_FLOW_DB_H_

#include "bnxt_ulp.h"
#include "ulp_template_db_enum.h"
#include "ulp_mapper.h"

#define BNXT_FLOW_DB_DEFAULT_NUM_FLOWS		512
#define BNXT_FLOW_DB_DEFAULT_NUM_RESOURCES	8

/* Defines for the fdb flag */
#define ULP_FDB_FLAG_SHARED_SESSION	0x1
#define ULP_FDB_FLAG_SHARED_WC_SESSION	0x2

/*
 * Structure for the flow database resource information
 * The below structure is based on the below partitions
 * nxt_resource_idx = dir[31],resource_func_upper[30:28],nxt_resource_idx[27:0]
 * If resource_func is EM_TBL then use resource_em_handle.
 * Else the other part of the union is used and
 * resource_func is resource_func_upper[30:28] << 5 | resource_func_lower
 */
struct ulp_fdb_resource_info {
	/* Points to next resource in the chained list. */
	uint32_t			nxt_resource_idx;
	union {
		uint64_t		resource_em_handle;
		struct {
			uint8_t		resource_func_lower;
			uint8_t		resource_type;
			uint8_t		resource_sub_type;
			uint8_t		fdb_flags;
			uint32_t	resource_hndl;
		};
	};
};

/* Structure for the flow database resource information. */
struct bnxt_ulp_flow_tbl {
	/* Flow tbl is the resource object list for each flow id. */
	struct ulp_fdb_resource_info	*flow_resources;

	/* Flow table stack to track free list of resources. */
	uint32_t	*flow_tbl_stack;
	uint32_t	head_index;
	uint32_t	tail_index;

	/* Table to track the active flows. */
	uint64_t	*active_reg_flows;
	uint64_t	*active_dflt_flows;
	uint32_t	num_flows;
	uint32_t	num_resources;
};

/* Structure to maintain parent-child flow relationships */
struct ulp_fdb_parent_info {
	uint32_t	valid;
	uint32_t	parent_fid;
	uint32_t	counter_acc;
	uint64_t	pkt_count;
	uint64_t	byte_count;
	uint64_t	*child_fid_bitset;
	uint32_t	f2_cnt;
	uint8_t		tun_idx;
};

/* Structure to maintain parent-child flow relationships */
struct ulp_fdb_parent_child_db {
	struct ulp_fdb_parent_info	*parent_flow_tbl;
	uint32_t			child_bitset_size;
	uint32_t			entries_count;
	uint8_t				*parent_flow_tbl_mem;
};

/* Structure for the flow database resource information. */
struct bnxt_ulp_flow_db {
	struct bnxt_ulp_flow_tbl	flow_tbl;
	uint16_t			*func_id_tbl;
	uint32_t			func_id_tbl_size;
	struct ulp_fdb_parent_child_db	parent_child_db;
};

/* flow db resource params to add resources */
struct ulp_flow_db_res_params {
	enum tf_dir			direction;
	enum bnxt_ulp_resource_func	resource_func;
	uint8_t				resource_type;
	uint8_t				resource_sub_type;
	uint8_t				fdb_flags;
	uint8_t				critical_resource;
	uint64_t			resource_hndl;
};

/*
 * Initialize the flow database. Memory is allocated in this
 * call and assigned to the flow database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_flow_db_init(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Deinitialize the flow database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success.
 */
int32_t	ulp_flow_db_deinit(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Allocate the flow database entry
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * func_id [in] The function id of the device.Valid only for regular flows.
 * fid [out] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_fid_alloc(struct bnxt_ulp_context *ulp_ctxt,
		      enum bnxt_ulp_fdb_type flow_type,
		      uint16_t func_id,
		      uint32_t *fid);

/*
 * Allocate the flow database entry.
 * The params->critical_resource has to be set to 0 to allocate a new resource.
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * params [in] The contents to be copied into resource
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_resource_add(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 uint32_t fid,
			 struct ulp_flow_db_res_params *params);

/*
 * Free the flow database entry.
 * The params->critical_resource has to be set to 1 to free the first resource.
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * params [in/out] The contents to be copied into params.
 * Only the critical_resource needs to be set by the caller.
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_resource_del(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 uint32_t fid,
			 struct ulp_flow_db_res_params *params);

/*
 * Free the flow database entry
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_fid_free(struct bnxt_ulp_context *ulp_ctxt,
		     enum bnxt_ulp_fdb_type tbl_idx,
		     uint32_t fid);

/*
 *Get the flow database entry details
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * nxt_idx [in/out] the index to the next entry
 * params [out] The contents to be copied into params.
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_resource_get(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 uint32_t fid,
			 uint32_t *nxt_idx,
			 struct ulp_flow_db_res_params *params);

/*
 * Flush all flows in the flow database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * flow_type [in] - specify default or regular
 *
 * returns 0 on success or negative number on failure
 */
int32_t
ulp_flow_db_flush_flows(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type);

/*
 * Flush all flows in the flow database that belong to a device function.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * tbl_idx [in] The index to table
 *
 * returns 0 on success or negative number on failure
 */
int32_t
ulp_flow_db_function_flow_flush(struct bnxt_ulp_context *ulp_ctx,
				uint16_t func_id);

/*
 * Flush all flows in the flow database that are associated with the session.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * returns 0 on success or negative number on failure
 */
int32_t
ulp_flow_db_session_flow_flush(struct bnxt_ulp_context *ulp_ctx);

/*
 * Check that flow id matches the function id or not
 *
 * ulp_ctxt [in] Ptr to ulp context
 * flow_id [in] flow id of the flow.
 * func_id [in] The func_id to be set, for reset pass zero.
 *
 * returns true on success or false on failure
 */
bool
ulp_flow_db_validate_flow_func(struct bnxt_ulp_context *ulp_ctx,
			       uint32_t flow_id,
			       uint32_t func_id);

/*
 * Api to get the cfa action pointer from a flow.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * flow_id [in] flow id
 * cfa_action [out] The resource handle stored in the flow database
 *
 * returns 0 on success
 */
int32_t
ulp_default_flow_db_cfa_action_get(struct bnxt_ulp_context *ulp_ctx,
				   uint32_t flow_id,
				   uint16_t *cfa_action);

/*
 * Set or reset the parent flow in the parent-child database
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * pc_idx [in] The index to parent child db
 * parent_fid [in] The flow id of the parent flow entry
 * set_flag [in] Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int32_t
ulp_flow_db_pc_db_parent_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				  uint32_t pc_idx,
				  uint32_t parent_fid,
				  uint32_t set_flag);

/*
 * Set or reset the child flow in the parent-child database
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * pc_idx [in] The index to parent child db
 * child_fid [in] The flow id of the child flow entry
 * set_flag [in] Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int32_t
ulp_flow_db_pc_db_child_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				 uint32_t pc_idx,
				 uint32_t child_fid,
				 uint32_t set_flag);

/*
 * Get the parent index from the parent-child database
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * parent_fid [in] The flow id of the parent flow entry
 * parent_idx [out] The parent index of parent flow entry
 *
 * returns zero on success and negative on failure.
 */
int32_t
ulp_flow_db_parent_flow_idx_get(struct bnxt_ulp_context *ulp_ctxt,
				uint32_t parent_fid,
				uint32_t *parent_idx);

/*
 * Get the next child flow in the parent-child database
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * parent_fid [in] The flow id of the parent flow entry
 * child_fid [in/out] The flow id of the child flow entry
 *
 * returns zero on success and negative on failure.
 * Pass child_fid as zero for first entry.
 */
int32_t
ulp_flow_db_parent_child_flow_next_entry_get(struct bnxt_ulp_flow_db *flow_db,
					     uint32_t parent_idx,
					     uint32_t *child_fid);

/*
 * Orphan the child flow entry
 * This is called only for child flows that have
 * BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW resource
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_child_flow_reset(struct bnxt_ulp_context *ulp_ctxt,
			     enum bnxt_ulp_fdb_type flow_type,
			     uint32_t fid);

/*
 * Create parent flow in the parent flow tbl
 *
 * parms [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_parent_flow_create(struct bnxt_ulp_mapper_parms *parms);

/*
 * Create child flow in the parent flow tbl
 *
 * parms [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_child_flow_create(struct bnxt_ulp_mapper_parms *parms);

/*
 * Update the parent counters
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * pc_idx [in] The parent flow entry idx
 * packet_count [in] - packet count
 * byte_count [in] - byte count
 *
 * returns 0 on success
 */
int32_t
ulp_flow_db_parent_flow_count_update(struct bnxt_ulp_context *ulp_ctxt,
				     uint32_t pc_idx,
				     uint64_t packet_count,
				     uint64_t byte_count);
/*
 * Get the parent accumulation counters
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * pc_idx [in] The parent flow entry idx
 * packet_count [out] - packet count
 * byte_count [out] - byte count
 *
 * returns 0 on success
 */
int32_t
ulp_flow_db_parent_flow_count_get(struct bnxt_ulp_context *ulp_ctxt,
				  uint32_t pc_idx,
				  uint64_t *packet_count,
				  uint64_t *byte_count,
				  uint8_t count_reset);

/*
 * reset the parent accumulation counters
 *
 * ulp_ctxt [in] Ptr to ulp_context
 *
 * returns none
 */
void
ulp_flow_db_parent_flow_count_reset(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Set the shared bit for the flow db entry
 *
 * res [in] Ptr to fdb entry
 * s_type [in] session flag
 *
 * returns none
 */
void ulp_flow_db_shared_session_set(struct ulp_flow_db_res_params *res,
				    enum bnxt_ulp_session_type s_type);

/*
 * Get the shared bit for the flow db entry
 *
 * res [out] Shared session type
 */
enum bnxt_ulp_session_type
ulp_flow_db_shared_session_get(struct ulp_flow_db_res_params *res);
#endif /* _ULP_FLOW_DB_H_ */
