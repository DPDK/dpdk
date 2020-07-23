/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "ulp_flow_db.h"
#include "ulp_utils.h"
#include "ulp_template_struct.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"

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
 * Helper function to set the bit in the active flow table
 * No validation is done in this function.
 *
 * flow_tbl [in] Ptr to flow table
 * idx [in] The index to bit to be set or reset.
 * flag [in] 1 to set and 0 to reset.
 *
 * returns none
 */
static void
ulp_flow_db_active_flow_set(struct bnxt_ulp_flow_tbl	*flow_tbl,
			    uint32_t			idx,
			    uint32_t			flag)
{
	uint32_t		active_index;

	active_index = idx / ULP_INDEX_BITMAP_SIZE;
	if (flag)
		ULP_INDEX_BITMAP_SET(flow_tbl->active_flow_tbl[active_index],
				     idx);
	else
		ULP_INDEX_BITMAP_RESET(flow_tbl->active_flow_tbl[active_index],
				       idx);
}

/*
 * Helper function to allocate the flow table and initialize
 * is set.No validation being done in this function.
 *
 * flow_tbl [in] Ptr to flow table
 * idx [in] The index to bit to be set or reset.
 *
 * returns 1 on set or 0 if not set.
 */
static int32_t
ulp_flow_db_active_flow_is_set(struct bnxt_ulp_flow_tbl	*flow_tbl,
			       uint32_t			idx)
{
	uint32_t		active_index;

	active_index = idx / ULP_INDEX_BITMAP_SIZE;
	return ULP_INDEX_BITMAP_GET(flow_tbl->active_flow_tbl[active_index],
				    idx);
}

static uint8_t
ulp_flow_db_resource_func_get(struct ulp_fdb_resource_info *res_info)
{
	uint8_t func;

	func = (((res_info->nxt_resource_idx & ULP_FLOW_DB_RES_FUNC_MASK) >>
		 ULP_FLOW_DB_RES_FUNC_BITS) << ULP_FLOW_DB_RES_FUNC_UPPER);
	/* The reource func is split into upper and lower */
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
	if (params->resource_func != BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE &&
	    params->resource_func != BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE) {
		resource_info->resource_hndl = (uint32_t)params->resource_hndl;
		resource_info->resource_type = params->resource_type;
		resource_info->resource_sub_type = params->resource_sub_type;
		resource_info->reserved = params->reserved;
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
	params->direction = ((resource_info->nxt_resource_idx &
				 ULP_FLOW_DB_RES_DIR_MASK) >>
				 ULP_FLOW_DB_RES_DIR_BIT);

	/* use the helper function to get the resource func */
	params->resource_func = ulp_flow_db_resource_func_get(resource_info);

	if (params->resource_func == BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE ||
	    params->resource_func == BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE) {
		params->resource_hndl = resource_info->resource_em_handle;
	} else if (params->resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
		params->resource_hndl = resource_info->resource_hndl;
		params->resource_type = resource_info->resource_type;
		params->resource_sub_type = resource_info->resource_sub_type;
		params->reserved = resource_info->reserved;
	}
}

/*
 * Helper function to allocate the flow table and initialize
 * the stack for allocation operations.
 *
 * flow_db [in] Ptr to flow database structure
 * tbl_idx [in] The index to table creation.
 *
 * Returns 0 on success or negative number on failure.
 */
static int32_t
ulp_flow_db_alloc_resource(struct bnxt_ulp_flow_db *flow_db,
			   enum bnxt_ulp_flow_db_tables tbl_idx)
{
	uint32_t			idx = 0;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	uint32_t			size;

	flow_tbl = &flow_db->flow_tbl[tbl_idx];

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
	flow_tbl->active_flow_tbl = rte_zmalloc("active flow tbl", size, 0);
	if (!flow_tbl->active_flow_tbl) {
		BNXT_TF_DBG(ERR, "Failed to alloc memory active tbl\n");
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
 * tbl_idx [in] The index to table creation.
 *
 * Returns none.
 */
static void
ulp_flow_db_dealloc_resource(struct bnxt_ulp_flow_db *flow_db,
			     enum bnxt_ulp_flow_db_tables tbl_idx)
{
	struct bnxt_ulp_flow_tbl	*flow_tbl;

	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* Free all the allocated tables in the flow table. */
	if (flow_tbl->active_flow_tbl) {
		rte_free(flow_tbl->active_flow_tbl);
		flow_tbl->active_flow_tbl = NULL;
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
 * Initialize the flow database. Memory is allocated in this
 * call and assigned to the flow database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_flow_db_init(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_device_params		*dparms;
	struct bnxt_ulp_flow_tbl		*flow_tbl;
	struct bnxt_ulp_flow_db			*flow_db;
	uint32_t				dev_id;

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

	/* Populate the regular flow table limits. */
	flow_tbl = &flow_db->flow_tbl[BNXT_ULP_REGULAR_FLOW_TABLE];
	flow_tbl->num_flows = dparms->flow_db_num_entries + 1;
	flow_tbl->num_resources = (flow_tbl->num_flows *
				   dparms->num_resources_per_flow);

	/* Populate the default flow table limits. */
	flow_tbl = &flow_db->flow_tbl[BNXT_ULP_DEFAULT_FLOW_TABLE];
	flow_tbl->num_flows = BNXT_FLOW_DB_DEFAULT_NUM_FLOWS + 1;
	flow_tbl->num_resources = (flow_tbl->num_flows *
				   BNXT_FLOW_DB_DEFAULT_NUM_RESOURCES);

	/* Allocate the resource for the regular flow table. */
	if (ulp_flow_db_alloc_resource(flow_db, BNXT_ULP_REGULAR_FLOW_TABLE))
		goto error_free;
	if (ulp_flow_db_alloc_resource(flow_db, BNXT_ULP_DEFAULT_FLOW_TABLE))
		goto error_free;

	/* add 1 since we are not using index 0 for flow id */
	flow_db->func_id_tbl_size = dparms->flow_db_num_entries + 1;
	/* Allocate the function Id table */
	flow_db->func_id_tbl = rte_zmalloc("bnxt_ulp_flow_db_func_id_table",
					   flow_db->func_id_tbl_size *
					   sizeof(uint16_t), 0);
	if (!flow_db->func_id_tbl) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate mem for flow table func id\n");
		goto error_free;
	}
	/* All good so return. */
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
int32_t	ulp_flow_db_deinit(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_flow_db			*flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* Detach the flow database from the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, NULL);

	/* Free up all the memory. */
	ulp_flow_db_dealloc_resource(flow_db, BNXT_ULP_REGULAR_FLOW_TABLE);
	ulp_flow_db_dealloc_resource(flow_db, BNXT_ULP_DEFAULT_FLOW_TABLE);
	rte_free(flow_db->func_id_tbl);
	rte_free(flow_db);

	return 0;
}

/*
 * Allocate the flow database entry
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [out] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t ulp_flow_db_fid_alloc(struct bnxt_ulp_context *ulp_ctxt,
			      enum bnxt_ulp_flow_db_tables tbl_idx,
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

	flow_tbl = &flow_db->flow_tbl[tbl_idx];
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
	ulp_flow_db_active_flow_set(flow_tbl, *fid, 1);

	/* The function id update is only valid for regular flow table */
	if (tbl_idx == BNXT_ULP_REGULAR_FLOW_TABLE)
		ulp_flow_db_func_id_set(flow_db, *fid, func_id);

	/* all good, return success */
	return 0;
}

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
int32_t	ulp_flow_db_resource_add(struct bnxt_ulp_context	*ulp_ctxt,
				 enum bnxt_ulp_flow_db_tables	tbl_idx,
				 uint32_t			fid,
				 struct ulp_flow_db_res_params	*params)
{
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	struct ulp_fdb_resource_info	*resource, *fid_resource;
	uint32_t			idx;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (tbl_idx >= BNXT_ULP_FLOW_TABLE_MAX) {
		BNXT_TF_DBG(ERR, "Invalid table index\n");
		return -EINVAL;
	}
	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flow_is_set(flow_tbl, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}

	/* check for max resource */
	if ((flow_tbl->head_index + 1) >= flow_tbl->tail_index) {
		BNXT_TF_DBG(ERR, "Flow db has reached max resources\n");
		return -ENOMEM;
	}
	fid_resource = &flow_tbl->flow_resources[fid];

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

	if (params->resource_type == TF_TBL_TYPE_ACT_STATS_64 &&
	    params->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT) {
		/* Store the first HW counter ID for this table */
		if (!ulp_fc_mgr_start_idx_isset(ulp_ctxt, params->direction))
			ulp_fc_mgr_start_idx_set(ulp_ctxt, params->direction,
						 params->resource_hndl);

		ulp_fc_mgr_cntr_set(ulp_ctxt, params->direction,
				    params->resource_hndl);

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
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * params [in/out] The contents to be copied into params.
 * Onlythe critical_resource needs to be set by the caller.
 *
 * Returns 0 on success and negative on failure.
 */
int32_t	ulp_flow_db_resource_del(struct bnxt_ulp_context	*ulp_ctxt,
				 enum bnxt_ulp_flow_db_tables	tbl_idx,
				 uint32_t			fid,
				 struct ulp_flow_db_res_params	*params)
{
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	struct ulp_fdb_resource_info	*nxt_resource, *fid_resource;
	uint32_t			nxt_idx = 0;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (tbl_idx >= BNXT_ULP_FLOW_TABLE_MAX) {
		BNXT_TF_DBG(ERR, "Invalid table index\n");
		return -EINVAL;
	}
	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flow_is_set(flow_tbl, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
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
	    BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT) {
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
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int32_t	ulp_flow_db_fid_free(struct bnxt_ulp_context		*ulp_ctxt,
			     enum bnxt_ulp_flow_db_tables	tbl_idx,
			     uint32_t				fid)
{
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (tbl_idx >= BNXT_ULP_FLOW_TABLE_MAX) {
		BNXT_TF_DBG(ERR, "Invalid table index\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flow_is_set(flow_tbl, fid)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}
	flow_tbl->head_index--;
	if (!flow_tbl->head_index) {
		BNXT_TF_DBG(ERR, "FlowDB: Head Ptr is zero\n");
		return -ENOENT;
	}
	flow_tbl->flow_tbl_stack[flow_tbl->head_index] = fid;
	ulp_flow_db_active_flow_set(flow_tbl, fid, 0);
	if (tbl_idx == BNXT_ULP_REGULAR_FLOW_TABLE)
		ulp_flow_db_func_id_set(flow_db, fid, 0);

	/* all good, return success */
	return 0;
}

/*
 * Get the flow database entry details
 *
 * ulp_ctxt [in] Ptr to ulp_context
 * tbl_idx [in] Specify it is regular or default flow
 * fid [in] The index to the flow entry
 * nxt_idx [in/out] the index to the next entry
 * params [out] The contents to be copied into params.
 *
 * returns 0 on success and negative on failure.
 */
int32_t	ulp_flow_db_resource_get(struct bnxt_ulp_context	*ulp_ctxt,
				 enum bnxt_ulp_flow_db_tables	tbl_idx,
				 uint32_t			fid,
				 uint32_t			*nxt_idx,
				 struct ulp_flow_db_res_params	*params)
{
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	struct ulp_fdb_resource_info	*nxt_resource, *fid_resource;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (tbl_idx >= BNXT_ULP_FLOW_TABLE_MAX) {
		BNXT_TF_DBG(ERR, "Invalid table index\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flow_is_set(flow_tbl, fid)) {
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
 * fid [in/out] The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
static int32_t
ulp_flow_db_next_entry_get(struct bnxt_ulp_flow_tbl	*flowtbl,
			   uint32_t			*fid)
{
	uint32_t	lfid = *fid;
	uint32_t	idx, s_idx, mod_fid;
	uint64_t	bs;

	do {
		/* increment the flow id to find the next valid flow id */
		lfid++;
		if (lfid >= flowtbl->num_flows)
			return -ENOENT;
		idx = lfid / ULP_INDEX_BITMAP_SIZE;
		mod_fid = lfid % ULP_INDEX_BITMAP_SIZE;
		s_idx = idx;
		while (!(bs = flowtbl->active_flow_tbl[idx])) {
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
	} while (!ulp_flow_db_active_flow_is_set(flowtbl, lfid));

	/* all good, return success */
	*fid = lfid;
	return 0;
}

/*
 * Flush all flows in the flow database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * tbl_idx [in] The index to table
 *
 * returns 0 on success or negative number on failure
 */
int32_t	ulp_flow_db_flush_flows(struct bnxt_ulp_context *ulp_ctx,
				uint32_t		idx)
{
	uint32_t			fid = 0;
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid Argument\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return -EINVAL;
	}
	flow_tbl = &flow_db->flow_tbl[idx];
	while (!ulp_flow_db_next_entry_get(flow_tbl, &fid))
		ulp_mapper_resources_free(ulp_ctx, fid, idx);

	return 0;
}

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
				uint16_t func_id)
{
	uint32_t flow_id = 0;
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt_ulp_flow_tbl *flow_tbl;

	if (!ulp_ctx || !func_id) {
		BNXT_TF_DBG(ERR, "Invalid Argument\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Flow database not found\n");
		return -EINVAL;
	}
	flow_tbl = &flow_db->flow_tbl[BNXT_ULP_REGULAR_FLOW_TABLE];
	while (!ulp_flow_db_next_entry_get(flow_tbl, &flow_id)) {
		if (flow_db->func_id_tbl[flow_id] == func_id)
			ulp_mapper_resources_free(ulp_ctx, flow_id,
						  BNXT_ULP_REGULAR_FLOW_TABLE);
	}

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
	return ulp_flow_db_flush_flows(ulp_ctx, BNXT_ULP_REGULAR_FLOW_TABLE);
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
ulp_flow_db_resource_hndl_get(struct bnxt_ulp_context *ulp_ctx,
			      enum bnxt_ulp_flow_db_tables tbl_idx,
			      uint32_t flow_id,
			      uint32_t resource_func,
			      uint32_t res_subtype,
			      uint64_t *res_hndl)
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

	flow_tbl = &flow_db->flow_tbl[tbl_idx];

	/* check for limits of fid */
	if (flow_id >= flow_tbl->num_flows || !flow_id) {
		BNXT_TF_DBG(ERR, "Invalid flow index\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flow_is_set(flow_tbl, flow_id)) {
		BNXT_TF_DBG(ERR, "flow does not exist\n");
		return -EINVAL;
	}
	/* Iterate the resource to get the resource handle */
	res_id =  flow_id;
	while (res_id) {
		fid_res = &flow_tbl->flow_resources[res_id];
		if (ulp_flow_db_resource_func_get(fid_res) == resource_func) {
			if (resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
				if (res_subtype == fid_res->resource_sub_type) {
					*res_hndl = fid_res->resource_hndl;
					return 0;
				}

			} else if (resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE ||
				   resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE) {
				*res_hndl = fid_res->resource_em_handle;
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
	uint8_t sub_type = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_VFR_CFA_ACTION;
	uint64_t hndl;
	int32_t rc;

	rc = ulp_flow_db_resource_hndl_get(ulp_ctx,
					   BNXT_ULP_DEFAULT_FLOW_TABLE,
					   flow_id,
					   BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
					   sub_type, &hndl);
	if (rc) {
		BNXT_TF_DBG(ERR, "CFA Action ptr not found for flow id %u\n",
			    flow_id);
		return -ENOENT;
	}
	*cfa_action = hndl;
	return 0;
}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
/*
 * Dump the entry details
 *
 * ulp_ctxt [in] Ptr to ulp_context
 *
 * returns none
 */
static void ulp_flow_db_res_dump(struct ulp_fdb_resource_info	*r,
				 uint32_t	*nxt_res)
{
	uint8_t res_func = ulp_flow_db_resource_func_get(r);

	BNXT_TF_DBG(DEBUG, "Resource func = %x, nxt_resource_idx = %x\n",
		    res_func, (ULP_FLOW_DB_RES_NXT_MASK & r->nxt_resource_idx));
	if (res_func == BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE ||
	    res_func == BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE)
		BNXT_TF_DBG(DEBUG, "EM Handle = 0x%016" PRIX64 "\n",
			    r->resource_em_handle);
	else
		BNXT_TF_DBG(DEBUG, "Handle = 0x%08x\n", r->resource_hndl);

	*nxt_res = 0;
	ULP_FLOW_DB_RES_NXT_SET(*nxt_res,
				r->nxt_resource_idx);
}

/*
 * Dump the flow database entry details
 *
 * ulp_ctxt [in] Ptr to ulp_context
 *
 * returns none
 */
int32_t	ulp_flow_db_debug_dump(struct bnxt_ulp_context	*ulp_ctxt)
{
	struct bnxt_ulp_flow_db		*flow_db;
	struct bnxt_ulp_flow_tbl	*flow_tbl;
	struct ulp_fdb_resource_info	*r;
	uint32_t			nxt_res = 0;
	enum bnxt_ulp_flow_db_tables	tbl_idx;
	uint32_t			fid;

	if (!ulp_ctxt || !ulp_ctxt->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	for (tbl_idx = 0; tbl_idx < BNXT_ULP_FLOW_TABLE_MAX; tbl_idx++) {
		flow_tbl = &flow_db->flow_tbl[tbl_idx];
		BNXT_TF_DBG(DEBUG, "Dump Tbl index = %u, flows = %u:%u\n",
			    tbl_idx, flow_tbl->num_flows,
			    flow_tbl->num_resources);
		BNXT_TF_DBG(DEBUG, "Head_index = %u, Tail_index = %u\n",
			    flow_tbl->head_index, flow_tbl->tail_index);
		for (fid = 0; fid < flow_tbl->num_flows; fid++) {
			if (ulp_flow_db_active_flow_is_set(flow_tbl, fid)) {
				BNXT_TF_DBG(DEBUG, "fid = %u\n", fid);
				/* iterate the resource */
				nxt_res = fid;
				do {
					r = &flow_tbl->flow_resources[nxt_res];
					ulp_flow_db_res_dump(r, &nxt_res);
				} while (nxt_res);
			}
		}
		BNXT_TF_DBG(DEBUG, "Done.\n");
	}
	return 0;
}
#endif
