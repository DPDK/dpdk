/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "ulp_utils.h"
#include "ulp_template_struct.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "ulp_fc_mgr.h"
#include "ulp_tun.h"

#define ULP_FLOW_DB_RES_DIR_BIT		31
#define ULP_FLOW_DB_RES_DIR_MASK	0x80000000
#define ULP_FLOW_DB_RES_FUNC_BITS	28
#define ULP_FLOW_DB_RES_FUNC_MASK	0x70000000
#define ULP_FLOW_DB_RES_NXT_MASK	0x0FFFFFFF
#define ULP_FLOW_DB_RES_FUNC_UPPER	5
#define ULP_FLOW_DB_RES_FUNC_NEED_LOWER	0x80
#define ULP_FLOW_DB_RES_FUNC_LOWER_MASK	0x1F

/* Macro to copy the nxt_resource_idx */
#define ULP_FLOW_DB_RES_NXT_SET(dst, src)	{(dst) |= ((src) &\
					 ULP_FLOW_DB_RES_NXT_MASK); }
#define ULP_FLOW_DB_RES_NXT_RESET(dst)	((dst) &= ~(ULP_FLOW_DB_RES_NXT_MASK))

/*
 * Helper function to set the bit in the active flows
 * No validation is done in this function.
 *
 * flow_db [in] Ptr to flow database
 * flow_type [in] - specify default or regular
 * idx [in] The index to bit to be set or reset.
 * flag [in] 1 to set and 0 to reset.
 *
 * returns none
 */
static void
ulp_flow_db_active_flows_bit_set(struct bnxt_ulp_flow_db *flow_db,
				 enum bnxt_ulp_fdb_type flow_type,
				 uint32_t idx,
				 uint32_t flag)
{
	struct bnxt_ulp_flow_tbl *f_tbl = &flow_db->flow_tbl;
	uint32_t a_idx = idx / ULP_INDEX_BITMAP_SIZE;

	if (flag) {
		if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_SET(f_tbl->active_reg_flows[a_idx],
					     idx);
		if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_SET(f_tbl->active_dflt_flows[a_idx],
					     idx);
	} else {
		if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_RESET(f_tbl->active_reg_flows[a_idx],
					       idx);
		if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_RESET(f_tbl->active_dflt_flows[a_idx],
					       idx);
	}
}

/*
 * Helper function to check if given fid is active flow.
 * No validation being done in this function.
 *
 * flow_db [in] Ptr to flow database
 * flow_type [in] - specify default or regular
 * idx [in] The index to bit to be set or reset.
 *
 * returns 1 on set or 0 if not set.
 */
static int32_t
ulp_flow_db_active_flows_bit_is_set(struct bnxt_ulp_flow_db *flow_db,
				    enum bnxt_ulp_fdb_type flow_type,
				    uint32_t idx)
{
	struct bnxt_ulp_flow_tbl *f_tbl = &flow_db->flow_tbl;
	uint32_t a_idx = idx / ULP_INDEX_BITMAP_SIZE;
	uint32_t reg, dflt;

	reg = ULP_INDEX_BITMAP_GET(f_tbl->active_reg_flows[a_idx], idx);
	dflt = ULP_INDEX_BITMAP_GET(f_tbl->active_dflt_flows[a_idx], idx);

	switch (flow_type) {
	case BNXT_ULP_FDB_TYPE_REGULAR:
		return (reg && !dflt);
	case BNXT_ULP_FDB_TYPE_DEFAULT:
		return (!reg && dflt);
	case BNXT_ULP_FDB_TYPE_RID:
		return (reg && dflt);
	default:
		return 0;
	}
}

static inline enum tf_dir
ulp_flow_db_resource_dir_get(struct ulp_fdb_resource_info *res_info)
{
	return ((res_info->nxt_resource_idx & ULP_FLOW_DB_RES_DIR_MASK) >>
		ULP_FLOW_DB_RES_DIR_BIT);
}

static uint8_t
ulp_flow_db_resource_func_get(struct ulp_fdb_resource_info *res_info)
{
	uint8_t func;

	func = (((res_info->nxt_resource_idx & ULP_FLOW_DB_RES_FUNC_MASK) >>
		 ULP_FLOW_DB_RES_FUNC_BITS) << ULP_FLOW_DB_RES_FUNC_UPPER);
	/* The resource func is split into upper and lower */
	if (func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER)
		return (func | res_info->resource_func_lower);
	return func;
}

/*
 * Helper function to copy the resource params to resource info
 *  No validation being done in this function.
 *
 * resource_info [out] Ptr to resource information
 * params [in] The input params from the caller
 * returns none
 */
static void
ulp_flow_db_res_params_to_info(struct ulp_fdb_resource_info *resource_info,
			       struct ulp_flow_db_res_params *params)
{
	uint32_t resource_func;

	resource_info->nxt_resource_idx |= ((params->direction <<
				      ULP_FLOW_DB_RES_DIR_BIT) &
				     ULP_FLOW_DB_RES_DIR_MASK);
	resource_func = (params->resource_func >> ULP_FLOW_DB_RES_FUNC_UPPER);
	resource_info->nxt_resource_idx |= ((resource_func <<
					     ULP_FLOW_DB_RES_FUNC_BITS) &
					    ULP_FLOW_DB_RES_FUNC_MASK);

	if (params->resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
		/* Break the resource func into two parts */
		resource_func = (params->resource_func &
				 ULP_FLOW_DB_RES_FUNC_LOWER_MASK);
		resource_info->resource_func_lower = resource_func;
	}

	/* Store the handle as 64bit only for EM table entries */
	if (params->resource_func != BNXT_ULP_RESOURCE_FUNC_EM_TABLE) {
		resource_info->resource_hndl = (uint32_t)params->resource_hndl;
		resource_info->resource_type = params->resource_type;
		resource_info->resource_sub_type = params->resource_sub_type;
		resource_info->fdb_flags = params->fdb_flags;
	} else {
		resource_info->resource_em_handle = params->resource_hndl;
	}
}

/*
 * Helper function to copy the resource params to resource info
 *  No validation being done in this function.
 *
 * resource_info [in] Ptr to resource information
 * params [out] The output params to the caller
 *
 * returns none
 */
static void
ulp_flow_db_res_info_to_params(struct ulp_fdb_resource_info *resource_info,
			       struct ulp_flow_db_res_params *params)
{
	memset(params, 0, sizeof(struct ulp_flow_db_res_params));

	/* use the helper function to get the resource func */
	params->direction = ulp_flow_db_resource_dir_get(resource_info);
	params->resource_func = ulp_flow_db_resource_func_get(resource_info);

	if (params->resource_func == BNXT_ULP_RESOURCE_FUNC_EM_TABLE) {
		params->resource_hndl = resource_info->resource_em_handle;
	} else if (params->resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
		params->resource_hndl = resource_info->resource_hndl;
		params->resource_type = resource_info->resource_type;
		params->resource_sub_type = resource_info->resource_sub_type;
		params->fdb_flags = resource_info->fdb_flags;
	}
}

/*
 * Helper function to allocate the flow table and initialize
 * the stack for allocation operations.
 *
 * flow_db [in] Ptr to flow database structure
 *
 * Returns 0 on success or negative number on failure.
 */
static int32_t
ulp_flow_db_alloc_resource(struct bnxt_ulp_flow_db *flow_db)
{
	uint32_t			idx = 0;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	uint32_t			size;

	flow_tbl = &flow_db->flow_tbl;

	size = sizeof(struct ulp_fdb_resource_info) * flow_tbl->num_resources;
	flow_tbl->flow_resources =
			rte_zmalloc("ulp_fdb_resource_info", size, 0);

	if (!flow_tbl->flow_resources) {
		BNXT_TF_DBG(ERR, "Failed to alloc memory for flow table\n");
		return -ENOMEM;
	}
	size = sizeof(uint32_t) * flow_tbl->num_resources;
	flow_tbl->flow_tbl_stack = rte_zmalloc("flow_tbl_stack", size, 0);
	if (!flow_tbl->flow_tbl_stack) {
		BNXT_TF_DBG(ERR, "Failed to alloc memory flow tbl stack\n");
		return -ENOMEM;
	}
	size = (flow_tbl->num_flows / sizeof(uint64_t)) + 1;
	size = ULP_BYTE_ROUND_OFF_8(size);
	flow_tbl->active_reg_flows = rte_zmalloc("active reg flows", size,
						 ULP_BUFFER_ALIGN_64_BYTE);
	if (!flow_tbl->active_reg_flows) {
		BNXT_TF_DBG(ERR, "Failed to alloc memory active reg flows\n");
		return -ENOMEM;
	}

	flow_tbl->active_dflt_flows = rte_zmalloc("active dflt flows", size,
						  ULP_BUFFER_ALIGN_64_BYTE);
	if (!flow_tbl->active_dflt_flows) {
		BNXT_TF_DBG(ERR, "Failed to alloc memory active dflt flows\n");
		return -ENOMEM;
	}

	/* Initialize the stack table. */
	for (idx = 0; idx < flow_tbl->num_resources; idx++)
		flow_tbl->flow_tbl_stack[idx] = idx;

	/* Ignore the first element in the list. */
	flow_tbl->head_index = 1;
	/* Tail points to the last entry in the list. */
	flow_tbl->tail_index = flow_tbl->num_resources - 1;
	return 0;
}

/*
 * Helper function to deallocate the flow table.
 *
 * flow_db [in] Ptr to flow database structure
 *
 * Returns none.
 */
static void
ulp_flow_db_dealloc_resource(struct bnxt_ulp_flow_db *flow_db)
{
	struct bnxt_ulp_flow_tbl *flow_tbl = &flow_db->flow_tbl;

	/* Free all the allocated tables in the flow table. */
	if (flow_tbl->active_reg_flows) {
		rte_free(flow_tbl->active_reg_flows);
		flow_tbl->active_reg_flows = NULL;
	}
	if (flow_tbl->active_dflt_flows) {
		rte_free(flow_tbl->active_dflt_flows);
		flow_tbl->active_dflt_flows = NULL;
	}

	if (flow_tbl->flow_tbl_stack) {
		rte_free(flow_tbl->flow_tbl_stack);
		flow_tbl->flow_tbl_stack = NULL;
	}

	if (flow_tbl->flow_resources) {
		rte_free(flow_tbl->flow_resources);
		flow_tbl->flow_resources = NULL;
	}
}

/*
 * Helper function to add function id to the flow table
 *
 * flow_db [in] Ptr to flow table
 * flow_id [in] The flow id of the flow
 * func_id [in] The func_id to be set, for reset pass zero
 *
 * returns none
 */
static void
ulp_flow_db_func_id_set(struct bnxt_ulp_flow_db *flow_db,
			uint32_t flow_id,
			uint32_t func_id)
{
	/* set the function id in the function table */
	if (flow_id < flow_db->func_id_tbl_size)
		flow_db->func_id_tbl[flow_id] = func_id;
	else /* This should never happen */
		BNXT_TF_DBG(ERR, "Invalid flow id, flowdb corrupt\n");
}

/*
 * Initialize the parent-child database. Memory is allocated in this
 * call and assigned to the database
 *
 * flow_db [in] Ptr to flow table
 * num_entries[in] - number of entries to allocate
 *
 * Returns 0 on success or negative number on failure.
 */
static int32_t
ulp_flow_db_parent_tbl_init(struct bnxt_ulp_flow_db *flow_db,
			    uint32_t num_entries)
{
	struct ulp_fdb_parent_child_db *p_db;
	uint32_t size, idx;

	if (!num_entries)
		return 0;

	/* update the sizes for the allocation */
	p_db = &flow_db->parent_child_db;
	p_db->child_bitset_size = (flow_db->flow_tbl.num_flows /
				   sizeof(uint64_t)) + 1; /* size in bytes */
	p_db->child_bitset_size = ULP_BYTE_ROUND_OFF_8(p_db->child_bitset_size);
	p_db->entries_count = num_entries;

	/* allocate the memory */
	p_db->parent_flow_tbl = rte_zmalloc("fdb parent flow tbl",
					    sizeof(struct ulp_fdb_parent_info) *
					    p_db->entries_count, 0);
	if (!p_db->parent_flow_tbl) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate memory fdb parent flow tbl\n");
		return -ENOMEM;
	}
	size = p_db->child_bitset_size * p_db->entries_count;

	/*
	 * allocate the big chunk of memory to be statically carved into
	 * child_fid_bitset pointer.
	 */
	p_db->parent_flow_tbl_mem = rte_zmalloc("fdb parent flow tbl mem",
						size,
						ULP_BUFFER_ALIGN_64_BYTE);
	if (!p_db->parent_flow_tbl_mem) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate memory fdb parent flow mem\n");
		return -ENOMEM;
	}

	/* set the pointers in parent table to their offsets */
	for (idx = 0 ; idx < p_db->entries_count; idx++) {
		p_db->parent_flow_tbl[idx].child_fid_bitset =
			(uint64_t *)&p_db->parent_flow_tbl_mem[idx *
			p_db->child_bitset_size];
	}
	/* success */
	return 0;
}

/*
 * Deinitialize the parent-child database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * flow_db [in] Ptr to flow table
 *
 * Returns none
 */
static void
ulp_flow_db_parent_tbl_deinit(struct bnxt_ulp_flow_db *flow_db)
{
	/* free the memory related to parent child database */
	if (flow_db->parent_child_db.parent_flow_tbl_mem) {
		rte_free(flow_db->parent_child_db.parent_flow_tbl_mem);
		flow_db->parent_child_db.parent_flow_tbl_mem = NULL;
	}
	if (flow_db->parent_child_db.parent_flow_tbl) {
		rte_free(flow_db->parent_child_db.parent_flow_tbl);
		flow_db->parent_child_db.parent_flow_tbl = NULL;
	}
}

/*
 * Initialize the flow database. Memory is allocated in this
 * call and assigned to the flow database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_flow_db_init(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	uint32_t dev_id, num_flows;
	enum bnxt_ulp_flow_mem_type mtype;

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctxt, &dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(ERR, "could not fetch the device params\n");
		return -ENODEV;
	}

	flow_db = rte_zmalloc("bnxt_ulp_flow_db",
			      sizeof(struct bnxt_ulp_flow_db), 0);
	if (!flow_db) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate memory for flow table ptr\n");
		return -ENOMEM;
	}

	/* Attach the flow database to the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, flow_db);

	/* Determine the number of flows based on EM type */
	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctxt, &mtype))
		goto error_free;

	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT)
		num_flows = dparms->int_flow_db_num_entries;
	else
		num_flows = dparms->ext_flow_db_num_entries;

	/* Populate the regular flow table limits. */
	flow_tbl = &flow_db->flow_tbl;
	flow_tbl->num_flows = num_flows + 1;
	flow_tbl->num_resources = ((num_flows + 1) *
				   dparms->num_resources_per_flow);

	/* Include the default flow table limits. */
	flow_tbl->num_flows += (BNXT_FLOW_DB_DEFAULT_NUM_FLOWS + 1);
	flow_tbl->num_resources += ((BNXT_FLOW_DB_DEFAULT_NUM_FLOWS + 1) *
				    BNXT_FLOW_DB_DEFAULT_NUM_RESOURCES);

	/* Allocate the resource for the flow table. */
	if (ulp_flow_db_alloc_resource(flow_db))
		goto error_free;

	/* add 1 since we are not using index 0 for flow id */
	flow_db->func_id_tbl_size = flow_tbl->num_flows + 1;
	/* Allocate the function Id table */
	flow_db->func_id_tbl = rte_zmalloc("bnxt_ulp_flow_db_func_id_table",
					   flow_db->func_id_tbl_size *
					   sizeof(uint16_t), 0);
	if (!flow_db->func_id_tbl) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate mem for flow table func id\n");
		goto error_free;
	}
	/* initialize the parent child database */
	if (ulp_flow_db_parent_tbl_init(flow_db,
					dparms->fdb_parent_flow_entries)) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate mem for parent child db\n");
		goto error_free;
	}

	/* All good so return. */
	BNXT_TF_DBG(DEBUG, "FlowDB initialized with %d flows.\n",
		    flow_tbl->num_flows);
	return 0;
error_free:
	ulp_flow_db_deinit(ulp_ctxt);
	return -ENOMEM;
}

/*
 * Deinitialize the flow database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success.
 */
int32_t
ulp_flow_db_deinit(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db)
		return -EINVAL;

	/* Detach the flow database from the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, NULL);

	/* Free up all the memory. */
	ulp_flow_db_parent_tbl_deinit(flow_db);
	ulp_flow_db_dealloc_resource(flow_db);
	rte_free(flow_db->func_id_tbl);
	rte_free(flow_db);

	return 0;
}

/*
 * Allocate the flow database entry
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] - specify default or regular
 * func_id [in].function id of the ingress port
 * fid [out] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_fid_alloc(struct bnxt_ulp_context *ulp_ctxt,
		      enum bnxt_ulp_fdb_type flow_type,
		      uint16_t func_id,
		      uint32_t *fid)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;

	*fid = 0; /* Initialize fid to invalid value */
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (flow_tbl->num_flows <= flow_tbl->head_index) {
		BNXT_TF_DBG(ERR, "Flow database has reached max flows\n");
		return -ENOMEM;
	}
	if (flow_tbl->tail_index <= (flow_tbl->head_index + 1)) {
		BNXT_TF_DBG(ERR, "Flow database has reached max resources\n");
		return -ENOMEM;
	}
	*fid = flow_tbl->flow_tbl_stack[flow_tbl->head_index];
	flow_tbl->head_index++;

	/* Set the flow type */
	ulp_flow_db_active_flows_bit_set(flow_db, flow_type, *fid, 1);

	/* function id update is only valid for regular flow table */
	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		ulp_flow_db_func_id_set(flow_db, *fid, func_id);

	/* return success */
	return 0;
}

/*
 * Allocate the flow database entry.
 * The params->critical_resource has to be set to 0 to allocate a new resource.
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * params [in] The contents to be copied into resource
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_resource_add(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 uint32_t fid,
			 struct ulp_flow_db_res_params *params)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *resource, *fid_resource;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	uint32_t idx;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}

	/* check for max resource */
	if ((flow_tbl->head_index + 1) >= flow_tbl->tail_index) {
		BNXT_TF_DBG(ERR, "Flow db has reached max resources\n");
		return -ENOMEM;
	}
	fid_resource = &flow_tbl->flow_resources[fid];

	if (params->critical_resource && fid_resource->resource_em_handle) {
		BNXT_TF_DBG(DEBUG, "Ignore multiple critical resources\n");
		/* Ignore the multiple critical resources */
		params->critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	}

	if (!params->critical_resource) {
		/* Not the critical_resource so allocate a resource */
		idx = flow_tbl->flow_tbl_stack[flow_tbl->tail_index];
		resource = &flow_tbl->flow_resources[idx];
		flow_tbl->tail_index--;

		/* Update the chain list of resource*/
		ULP_FLOW_DB_RES_NXT_SET(resource->nxt_resource_idx,
					fid_resource->nxt_resource_idx);
		/* update the contents */
		ulp_flow_db_res_params_to_info(resource, params);
		ULP_FLOW_DB_RES_NXT_RESET(fid_resource->nxt_resource_idx);
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					idx);
	} else {
		/* critical resource. Just update the fid resource */
		ulp_flow_db_res_params_to_info(fid_resource, params);
	}

	ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ulp_ctxt);
	if (params->resource_type == TF_TBL_TYPE_ACT_STATS_64 &&
	    params->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT &&
	    ulp_fc_info && ulp_fc_info->num_counters) {
		/* Store the first HW counter ID for this table */
		if (!ulp_fc_mgr_start_idx_isset(ulp_ctxt, params->direction))
			ulp_fc_mgr_start_idx_set(ulp_ctxt, params->direction,
						 params->resource_hndl);

		ulp_fc_mgr_cntr_set(ulp_ctxt, params->direction,
				    params->resource_hndl,
				    ulp_flow_db_shared_session_get(params));

		if (!ulp_fc_mgr_thread_isstarted(ulp_ctxt))
			ulp_fc_mgr_thread_start(ulp_ctxt);
	}

	/* all good, return success */
	return 0;
}

/*
 * Free the flow database entry.
 * The params->critical_resource has to be set to 1 to free the first resource.
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] Specify it is regular or default flow
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
			 struct ulp_flow_db_res_params *params)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *nxt_resource, *fid_resource;
	uint32_t nxt_idx = 0;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index %x\n", fid);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}

	fid_resource = &flow_tbl->flow_resources[fid];
	if (!params->critical_resource) {
		/* Not the critical resource so free the resource */
		ULP_FLOW_DB_RES_NXT_SET(nxt_idx,
					fid_resource->nxt_resource_idx);
		if (!nxt_idx) {
			/* reached end of resources */
			return -ENOENT;
		}
		nxt_resource = &flow_tbl->flow_resources[nxt_idx];

		/* connect the fid resource to the next resource */
		ULP_FLOW_DB_RES_NXT_RESET(fid_resource->nxt_resource_idx);
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					nxt_resource->nxt_resource_idx);

		/* update the contents to be given to caller */
		ulp_flow_db_res_info_to_params(nxt_resource, params);

		/* Delete the nxt_resource */
		memset(nxt_resource, 0, sizeof(struct ulp_fdb_resource_info));

		/* add it to the free list */
		flow_tbl->tail_index++;
		if (flow_tbl->tail_index >= flow_tbl->num_resources) {
			BNXT_TF_DBG(ERR, "FlowDB:Tail reached max\n");
			return -ENOENT;
		}
		flow_tbl->flow_tbl_stack[flow_tbl->tail_index] = nxt_idx;

	} else {
		/* Critical resource. copy the contents and exit */
		ulp_flow_db_res_info_to_params(fid_resource, params);
		ULP_FLOW_DB_RES_NXT_SET(nxt_idx,
					fid_resource->nxt_resource_idx);
		memset(fid_resource, 0, sizeof(struct ulp_fdb_resource_info));
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					nxt_idx);
	}

	/* Now that the HW Flow counter resource is deleted, reset it's
	 * corresponding slot in the SW accumulation table in the Flow Counter
	 * manager
	 */
	if (params->resource_type == TF_TBL_TYPE_ACT_STATS_64 &&
	    params->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		ulp_fc_mgr_cntr_reset(ulp_ctxt, params->direction,
				      params->resource_hndl);
	}

	/* all good, return success */
	return 0;
}

/*
 * Free the flow database entry
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] - specify default or regular
 * fid [in] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_fid_free(struct bnxt_ulp_context *ulp_ctxt,
		     enum bnxt_ulp_fdb_type flow_type,
		     uint32_t fid)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}
	flow_tbl->head_index--;
	if (!flow_tbl->head_index) {
		BNXT_TF_DBG(ERR, "FlowDB: Head Ptr is zero\n");
		return -ENOENT;
	}

	flow_tbl->flow_tbl_stack[flow_tbl->head_index] = fid;

	/* Clear the flows bitmap */
	ulp_flow_db_active_flows_bit_set(flow_db, flow_type, fid, 0);

	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		ulp_flow_db_func_id_set(flow_db, fid, 0);

	/* all good, return success */
	return 0;
}

/*
 *Get the flow database entry details
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * flow_type [in] - specify default or regular
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
			 struct ulp_flow_db_res_params *params)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *nxt_resource, *fid_resource;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}

	if (!*nxt_idx) {
		fid_resource = &flow_tbl->flow_resources[fid];
		ulp_flow_db_res_info_to_params(fid_resource, params);
		ULP_FLOW_DB_RES_NXT_SET(*nxt_idx,
					fid_resource->nxt_resource_idx);
	} else {
		nxt_resource = &flow_tbl->flow_resources[*nxt_idx];
		ulp_flow_db_res_info_to_params(nxt_resource, params);
		*nxt_idx = 0;
		ULP_FLOW_DB_RES_NXT_SET(*nxt_idx,
					nxt_resource->nxt_resource_idx);
	}

	/* all good, return success */
	return 0;
}

/*
 * Get the flow database entry iteratively
 *
 * flow_tbl [in] Ptr to flow table
 * flow_type [in] - specify default or regular
 * fid [in/out] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
static int32_t
ulp_flow_db_next_entry_get(struct bnxt_ulp_flow_db *flow_db,
			   enum bnxt_ulp_fdb_type flow_type,
			   uint32_t *fid)
{
	uint32_t lfid = *fid;
	uint32_t idx, s_idx, mod_fid;
	uint64_t bs;
	uint64_t *active_flows;
	struct bnxt_ulp_flow_tbl *flowtbl = &flow_db->flow_tbl;

	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR) {
		active_flows = flowtbl->active_reg_flows;
	} else if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT) {
		active_flows = flowtbl->active_dflt_flows;
	} else {
		BNXT_TF_DBG(ERR, "Invalid flow type %x\n", flow_type);
			return -EINVAL;
	}

	do {
		/* increment the flow id to find the next valid flow id */
		lfid++;
		if (lfid >= flowtbl->num_flows)
			return -ENOENT;
		idx = lfid / ULP_INDEX_BITMAP_SIZE;
		mod_fid = lfid % ULP_INDEX_BITMAP_SIZE;
		s_idx = idx;
		while (!(bs = active_flows[idx])) {
			idx++;
			if ((idx * ULP_INDEX_BITMAP_SIZE) >= flowtbl->num_flows)
				return -ENOENT;
		}
		/*
		 * remove the previous bits in the bitset bs to find the
		 * next non zero bit in the bitset. This needs to be done
		 * only if the idx is same as he one you started.
		 */
		if (s_idx == idx)
			bs &= (-1UL >> mod_fid);
		lfid = (idx * ULP_INDEX_BITMAP_SIZE) + __builtin_clzl(bs);
		if (*fid >= lfid) {
			BNXT_TF_DBG(ERR, "Flow Database is corrupt\n");
			return -ENOENT;
		}
	} while (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type,
						      lfid));

	/* all good, return success */
	*fid = lfid;
	return 0;
}

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
			enum bnxt_ulp_fdb_type flow_type)
{
	uint32_t fid = 0;
	struct bnxt_ulp_flow_db *flow_db;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid Argument\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return -EINVAL;
	}
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_TF_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}

	while (!ulp_flow_db_next_entry_get(flow_db, flow_type, &fid))
		ulp_mapper_resources_free(ulp_ctx, flow_type, fid);

	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	return 0;
}

/*
 * Flush all flows in the flow database that belong to a device function.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * func_id [in] - The port function id
 *
 * returns 0 on success or negative number on failure
 */
int32_t
ulp_flow_db_function_flow_flush(struct bnxt_ulp_context *ulp_ctx,
				uint16_t func_id)
{
	uint32_t flow_id = 0;
	struct bnxt_ulp_flow_db *flow_db;

	if (!ulp_ctx || !func_id) {
		BNXT_TF_DBG(ERR, "Invalid Argument\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return -EINVAL;
	}
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_TF_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}

	while (!ulp_flow_db_next_entry_get(flow_db, BNXT_ULP_FDB_TYPE_REGULAR,
					   &flow_id)) {
		if (flow_db->func_id_tbl[flow_id] == func_id)
			ulp_mapper_resources_free(ulp_ctx,
						  BNXT_ULP_FDB_TYPE_REGULAR,
						  flow_id);
	}
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
	return 0;
}

/*
 * Flush all flows in the flow database that are associated with the session.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * returns 0 on success or negative number on failure
 */
int32_t
ulp_flow_db_session_flow_flush(struct bnxt_ulp_context *ulp_ctx)
{
	/*
	 * TBD: Tf core implementation of FW session flush shall change this
	 * implementation.
	 */
	return ulp_flow_db_flush_flows(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR);
}

/*
 * Check that flow id matches the function id or not
 *
 * ulp_ctxt [in] Ptr to ulp context
 * flow_db [in] Ptr to flow table
 * func_id [in] The func_id to be set, for reset pass zero.
 *
 * returns true on success or false on failure
 */
bool
ulp_flow_db_validate_flow_func(struct bnxt_ulp_context *ulp_ctx,
			       uint32_t flow_id,
			       uint32_t func_id)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return false;
	}

	/* set the function id in the function table */
	if (flow_id < flow_db->func_id_tbl_size && func_id &&
	    flow_db->func_id_tbl[flow_id] == func_id)
		return true;

	return false;
}

/*
 * Internal api to traverse the resource list within a flow
 * and match a resource based on resource func and resource
 * sub type. This api should be used only for resources that
 * are unique and do not have multiple instances of resource
 * func and sub type combination since it will return only
 * the first match.
 */
static int32_t
ulp_flow_db_resource_params_get(struct bnxt_ulp_context *ulp_ctx,
				enum bnxt_ulp_fdb_type flow_type,
				uint32_t flow_id,
				uint32_t resource_func,
				uint32_t res_subtype,
				struct ulp_flow_db_res_params *params)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *fid_res;
	uint32_t res_id;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return -EINVAL;
	}

	if (!params) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (flow_id >= flow_tbl->num_flows || !flow_id) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, flow_id)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}
	/* Iterate the resource to get the resource handle */
	res_id =  flow_id;
	memset(params, 0, sizeof(struct ulp_flow_db_res_params));
	while (res_id) {
		fid_res = &flow_tbl->flow_resources[res_id];
		if (ulp_flow_db_resource_func_get(fid_res) == resource_func) {
			if (resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
				if (res_subtype == fid_res->resource_sub_type) {
					ulp_flow_db_res_info_to_params(fid_res,
								       params);
					return 0;
				}

			} else if (resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_EM_TABLE) {
				ulp_flow_db_res_info_to_params(fid_res,
							       params);
				return 0;
			}
		}
		res_id = 0;
		ULP_FLOW_DB_RES_NXT_SET(res_id, fid_res->nxt_resource_idx);
	}
	return -ENOENT;
}

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
				   uint16_t *cfa_action)
{
	uint8_t sub_typ = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION;
	struct ulp_flow_db_res_params params;
	int32_t rc;

	rc = ulp_flow_db_resource_params_get(ulp_ctx,
					     BNXT_ULP_FDB_TYPE_DEFAULT,
					     flow_id,
					     BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
					     sub_typ, &params);
	if (rc) {
		BNXT_TF_DBG(INFO, "CFA Action ptr not found for flow id %u\n",
			    flow_id);
		return -ENOENT;
	}
	*cfa_action = params.resource_hndl;
	return 0;
}

/* internal validation function for parent flow tbl */
static struct ulp_fdb_parent_info *
ulp_flow_db_pc_db_entry_get(struct bnxt_ulp_context *ulp_ctxt,
			    uint32_t pc_idx)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return NULL;
	}

	/* check for max flows */
	if (pc_idx >= BNXT_ULP_MAX_TUN_CACHE_ENTRIES) {
		BNXT_TF_DBG(ERR, "Invalid tunnel index\n");
		return NULL;
	}

	/* No support for parent child db then just exit */
	if (!flow_db->parent_child_db.entries_count) {
		BNXT_TF_DBG(ERR, "parent child db not supported\n");
		return NULL;
	}
	if (!flow_db->parent_child_db.parent_flow_tbl[pc_idx].valid) {
		BNXT_TF_DBG(ERR, "Not a valid tunnel index\n");
		return NULL;
	}

	return &flow_db->parent_child_db.parent_flow_tbl[pc_idx];
}

/* internal validation function for parent flow tbl */
static struct bnxt_ulp_flow_db *
ulp_flow_db_parent_arg_validation(struct bnxt_ulp_context *ulp_ctxt,
				  uint32_t tun_idx)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return NULL;
	}

	/* check for max flows */
	if (tun_idx >= BNXT_ULP_MAX_TUN_CACHE_ENTRIES) {
		BNXT_TF_DBG(ERR, "Invalid tunnel index\n");
		return NULL;
	}

	/* No support for parent child db then just exit */
	if (!flow_db->parent_child_db.entries_count) {
		BNXT_TF_DBG(ERR, "parent child db not supported\n");
		return NULL;
	}

	return flow_db;
}

/*
 * Allocate the entry in the parent-child database
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tun_idx [in] The tunnel index of the flow entry
 *
 * returns index on success and negative on failure.
 */
static int32_t
ulp_flow_db_pc_db_idx_alloc(struct bnxt_ulp_context *ulp_ctxt,
			    uint32_t tun_idx)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct ulp_fdb_parent_child_db *p_pdb;
	uint32_t idx, free_idx = 0;

	/* validate the arguments */
	flow_db = ulp_flow_db_parent_arg_validation(ulp_ctxt, tun_idx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "parent child db validation failed\n");
		return -EINVAL;
	}

	p_pdb = &flow_db->parent_child_db;
	for (idx = 0; idx < p_pdb->entries_count; idx++) {
		if (p_pdb->parent_flow_tbl[idx].valid &&
		    p_pdb->parent_flow_tbl[idx].tun_idx == tun_idx) {
			return idx;
		}
		if (!p_pdb->parent_flow_tbl[idx].valid && !free_idx)
			free_idx = idx + 1;
	}
	/* no free slots */
	if (!free_idx) {
		BNXT_TF_DBG(ERR, "parent child db is full\n");
		return -ENOMEM;
	}

	free_idx -= 1;
	/* set the Fid in the parent child */
	p_pdb->parent_flow_tbl[free_idx].tun_idx = tun_idx;
	p_pdb->parent_flow_tbl[free_idx].valid = 1;
	return free_idx;
}

/*
 * Free the entry in the parent-child database
 *
 * pc_entry [in] Ptr to parent child db entry
 *
 * returns none.
 */
static void
ulp_flow_db_pc_db_entry_free(struct bnxt_ulp_context *ulp_ctxt,
			     struct ulp_fdb_parent_info *pc_entry)
{
	struct bnxt_tun_cache_entry *tun_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	uint64_t *tmp_bitset;

	/* free the tunnel entry */
	tun_tbl = bnxt_ulp_cntxt_ptr2_tun_tbl_get(ulp_ctxt);
	if (tun_tbl)
		ulp_tunnel_offload_entry_clear(tun_tbl, pc_entry->tun_idx);

	/* free the child bitset*/
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (flow_db)
		memset(pc_entry->child_fid_bitset, 0,
		       flow_db->parent_child_db.child_bitset_size);

	/* free the contents */
	tmp_bitset = pc_entry->child_fid_bitset;
	memset(pc_entry, 0, sizeof(struct ulp_fdb_parent_info));
	pc_entry->child_fid_bitset = tmp_bitset;
}

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
				  uint32_t set_flag)
{
	struct ulp_fdb_parent_info *pc_entry;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "parent child db validation failed\n");
		return -EINVAL;
	}

	/* check for fid validity */
	if (parent_fid >= flow_db->flow_tbl.num_flows || !parent_fid) {
		BNXT_TF_DBG(ERR, "Invalid parent flow index %x\n", parent_fid);
		return -EINVAL;
	}

	/* validate the arguments and parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		BNXT_TF_DBG(ERR, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (set_flag) {
		pc_entry->parent_fid = parent_fid;
	} else {
		if (pc_entry->parent_fid != parent_fid)
			BNXT_TF_DBG(ERR, "Panic: invalid parent id\n");
		pc_entry->parent_fid = 0;

		/* Free the parent child db entry if no user present */
		if (!pc_entry->f2_cnt)
			ulp_flow_db_pc_db_entry_free(ulp_ctxt, pc_entry);
	}
	return 0;
}

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
				 uint32_t set_flag)
{
	struct ulp_fdb_parent_info *pc_entry;
	struct bnxt_ulp_flow_db *flow_db;
	uint32_t a_idx;
	uint64_t *t;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "parent child db validation failed\n");
		return -EINVAL;
	}

	/* check for fid validity */
	if (child_fid >= flow_db->flow_tbl.num_flows || !child_fid) {
		BNXT_TF_DBG(ERR, "Invalid child flow index %x\n", child_fid);
		return -EINVAL;
	}

	/* validate the arguments and parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		BNXT_TF_DBG(ERR, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	a_idx = child_fid / ULP_INDEX_BITMAP_SIZE;
	t = pc_entry->child_fid_bitset;
	if (set_flag) {
		ULP_INDEX_BITMAP_SET(t[a_idx], child_fid);
		pc_entry->f2_cnt++;
	} else {
		ULP_INDEX_BITMAP_RESET(t[a_idx], child_fid);
		if (pc_entry->f2_cnt)
			pc_entry->f2_cnt--;
		if (!pc_entry->f2_cnt && !pc_entry->parent_fid)
			ulp_flow_db_pc_db_entry_free(ulp_ctxt, pc_entry);
	}
	return 0;
}

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
					     uint32_t *child_fid)
{
	struct ulp_fdb_parent_child_db *p_pdb;
	uint32_t idx, s_idx, mod_fid;
	uint32_t next_fid = *child_fid;
	uint64_t *child_bitset;
	uint64_t bs;

	/* check for fid validity */
	p_pdb = &flow_db->parent_child_db;
	if (parent_idx >= p_pdb->entries_count ||
	    !p_pdb->parent_flow_tbl[parent_idx].parent_fid) {
		BNXT_TF_DBG(ERR, "Invalid parent flow index %x\n", parent_idx);
		return -EINVAL;
	}

	child_bitset = p_pdb->parent_flow_tbl[parent_idx].child_fid_bitset;
	do {
		/* increment the flow id to find the next valid flow id */
		next_fid++;
		if (next_fid >= flow_db->flow_tbl.num_flows)
			return -ENOENT;
		idx = next_fid / ULP_INDEX_BITMAP_SIZE;
		mod_fid = next_fid % ULP_INDEX_BITMAP_SIZE;
		s_idx = idx;
		while (!(bs = child_bitset[idx])) {
			idx++;
			if ((idx * ULP_INDEX_BITMAP_SIZE) >=
			    flow_db->flow_tbl.num_flows)
				return -ENOENT;
		}
		/*
		 * remove the previous bits in the bitset bs to find the
		 * next non zero bit in the bitset. This needs to be done
		 * only if the idx is same as he one you started.
		 */
		if (s_idx == idx)
			bs &= (-1UL >> mod_fid);
		next_fid = (idx * ULP_INDEX_BITMAP_SIZE) + __builtin_clzl(bs);
		if (*child_fid >= next_fid) {
			BNXT_TF_DBG(ERR, "Parent Child Database is corrupt\n");
			return -ENOENT;
		}
		idx = next_fid / ULP_INDEX_BITMAP_SIZE;
	} while (!ULP_INDEX_BITMAP_GET(child_bitset[idx], next_fid));
	*child_fid = next_fid;
	return 0;
}

/*
 * Set the counter accumulation in the parent flow
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * pc_idx [in] The parent child index of the parent flow entry
 *
 * returns index on success and negative on failure.
 */
static int32_t
ulp_flow_db_parent_flow_count_accum_set(struct bnxt_ulp_context *ulp_ctxt,
					uint32_t pc_idx)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct ulp_fdb_parent_child_db *p_pdb;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* check for parent idx validity */
	p_pdb = &flow_db->parent_child_db;
	if (pc_idx >= p_pdb->entries_count ||
	    !p_pdb->parent_flow_tbl[pc_idx].parent_fid) {
		BNXT_TF_DBG(ERR, "Invalid parent child index %x\n", pc_idx);
		return -EINVAL;
	}

	p_pdb->parent_flow_tbl[pc_idx].counter_acc = 1;
	return 0;
}

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
			     uint32_t fid)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *fid_res;
	uint32_t res_id = 0;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		BNXT_TF_DBG(ERR, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index %x\n", fid);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}

	/* Iterate the resource to get the resource handle */
	res_id =  fid;
	while (res_id) {
		fid_res = &flow_tbl->flow_resources[res_id];
		if (ulp_flow_db_resource_func_get(fid_res) ==
		    BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW) {
			/* invalidate the resource details */
			fid_res->resource_hndl = 0;
			return 0;
		}
		res_id = 0;
		ULP_FLOW_DB_RES_NXT_SET(res_id, fid_res->nxt_resource_idx);
	}
	/* failed */
	return -1;
}

/*
 * Create parent flow in the parent flow tbl
 *
 * parms [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_parent_flow_create(struct bnxt_ulp_mapper_parms *parms)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t sub_typ = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT;
	struct ulp_flow_db_res_params res_params;
	int32_t pc_idx;

	/* create or get the parent child database */
	pc_idx = ulp_flow_db_pc_db_idx_alloc(parms->ulp_ctx, parms->tun_idx);
	if (pc_idx < 0) {
		BNXT_TF_DBG(ERR, "Error in getting parent child db %x\n",
			    parms->tun_idx);
		return -EINVAL;
	}

	/* Update the parent fid */
	if (ulp_flow_db_pc_db_parent_flow_set(parms->ulp_ctx, pc_idx,
					      parms->fid, 1)) {
		BNXT_TF_DBG(ERR, "Error in setting parent fid %x\n",
			    parms->tun_idx);
		return -EINVAL;
	}

	/* Add the parent details in the resource list of the flow */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.resource_func	= BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW;
	fid_parms.resource_hndl	= pc_idx;
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	if (ulp_flow_db_resource_add(parms->ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				     parms->fid, &fid_parms)) {
		BNXT_TF_DBG(ERR, "Error in adding flow res for fid %x\n",
			    parms->fid);
		return -1;
	}

	/* check of the flow has internal counter accumulation enabled */
	if (!ulp_flow_db_resource_params_get(parms->ulp_ctx,
					     BNXT_ULP_FDB_TYPE_REGULAR,
					     parms->fid,
					     BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
					     sub_typ,
					     &res_params)) {
		/* Enable the counter accumulation in parent entry */
		if (ulp_flow_db_parent_flow_count_accum_set(parms->ulp_ctx,
							    pc_idx)) {
			BNXT_TF_DBG(ERR, "Error in setting counter acc %x\n",
				    parms->fid);
			return -1;
		}
	}

	return 0;
}

/*
 * Create child flow in the parent flow tbl
 *
 * parms [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int32_t
ulp_flow_db_child_flow_create(struct bnxt_ulp_mapper_parms *parms)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t sub_type = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT;
	enum bnxt_ulp_resource_func res_fun;
	struct ulp_flow_db_res_params res_p;
	int32_t rc, pc_idx;

	/* create or get the parent child database */
	pc_idx = ulp_flow_db_pc_db_idx_alloc(parms->ulp_ctx, parms->tun_idx);
	if (pc_idx < 0) {
		BNXT_TF_DBG(ERR, "Error in getting parent child db %x\n",
			    parms->tun_idx);
		return -1;
	}

	/* create the parent flow entry in parent flow table */
	rc = ulp_flow_db_pc_db_child_flow_set(parms->ulp_ctx, pc_idx,
					      parms->fid, 1);
	if (rc) {
		BNXT_TF_DBG(ERR, "Error in setting child fid %x\n", parms->fid);
		return rc;
	}

	/* Add the parent details in the resource list of the flow */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.resource_func	= BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW;
	fid_parms.resource_hndl	= pc_idx;
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	rc  = ulp_flow_db_resource_add(parms->ulp_ctx,
				       BNXT_ULP_FDB_TYPE_REGULAR,
				       parms->fid, &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Error in adding flow res for fid %x\n",
			    parms->fid);
		return rc;
	}

	/* check if internal count action included for this flow.*/
	res_fun = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE;
	rc = ulp_flow_db_resource_params_get(parms->ulp_ctx,
					     BNXT_ULP_FDB_TYPE_REGULAR,
					     parms->fid,
					     res_fun,
					     sub_type,
					     &res_p);
	if (!rc) {
		/* update the counter manager to include parent fid */
		if (ulp_fc_mgr_cntr_parent_flow_set(parms->ulp_ctx,
						    res_p.direction,
						    res_p.resource_hndl,
						    pc_idx)) {
			BNXT_TF_DBG(ERR, "Error in setting child %x\n",
				    parms->fid);
			return -1;
		}
	}

	/* return success */
	return 0;
}

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
				     uint64_t byte_count)
{
	struct ulp_fdb_parent_info *pc_entry;

	/* validate the arguments and get parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		BNXT_TF_DBG(ERR, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (pc_entry->counter_acc) {
		pc_entry->pkt_count += packet_count;
		pc_entry->byte_count += byte_count;
	}
	return 0;
}

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
				  uint32_t pc_idx, uint64_t *packet_count,
				  uint64_t *byte_count, uint8_t count_reset)
{
	struct ulp_fdb_parent_info *pc_entry;

	/* validate the arguments and get parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		BNXT_TF_DBG(ERR, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (pc_entry->counter_acc) {
		*packet_count = pc_entry->pkt_count;
		*byte_count = pc_entry->byte_count;
		if (count_reset) {
			pc_entry->pkt_count = 0;
			pc_entry->byte_count = 0;
		}
	}
	return 0;
}

/*
 * reset the parent accumulation counters
 *
 * ulp_ctxt [in] Ptr to ulp_context
 *
 * returns none
 */
void
ulp_flow_db_parent_flow_count_reset(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct ulp_fdb_parent_child_db *p_pdb;
	uint32_t idx;

	/* validate the arguments */
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "parent child db validation failed\n");
		return;
	}

	p_pdb = &flow_db->parent_child_db;
	for (idx = 0; idx < p_pdb->entries_count; idx++) {
		if (p_pdb->parent_flow_tbl[idx].valid &&
		    p_pdb->parent_flow_tbl[idx].counter_acc) {
			p_pdb->parent_flow_tbl[idx].pkt_count = 0;
			p_pdb->parent_flow_tbl[idx].byte_count = 0;
		}
	}
}

/*
 * Set the shared bit for the flow db entry
 *
 * res [in] Ptr to fdb entry
 * shared [in] shared flag
 *
 * returns none
 */
void ulp_flow_db_shared_session_set(struct ulp_flow_db_res_params *res,
				    enum bnxt_ulp_session_type s_type)
{
	if (res && (s_type & BNXT_ULP_SESSION_TYPE_SHARED))
		res->fdb_flags |= ULP_FDB_FLAG_SHARED_SESSION;
	else if (res && (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
		res->fdb_flags |= ULP_FDB_FLAG_SHARED_WC_SESSION;
}

/*
 * Get the shared bit for the flow db entry
 *
 * res [out] shared session type
 */
enum bnxt_ulp_session_type
ulp_flow_db_shared_session_get(struct ulp_flow_db_res_params *res)
{
	enum bnxt_ulp_session_type stype = BNXT_ULP_SESSION_TYPE_DEFAULT;

	if (res && (res->fdb_flags & ULP_FDB_FLAG_SHARED_SESSION))
		stype = BNXT_ULP_SESSION_TYPE_SHARED;
	else if (res && (res->fdb_flags & ULP_FDB_FLAG_SHARED_WC_SESSION))
		stype = BNXT_ULP_SESSION_TYPE_SHARED_WC;

	return stype;
}
