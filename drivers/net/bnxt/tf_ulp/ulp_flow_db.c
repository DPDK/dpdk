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

#define ULP_FLOW_DB_RES_DIR_BIT		31
#define ULP_FLOW_DB_RES_DIR_MASK	0x80000000
#define ULP_FLOW_DB_RES_FUNC_BITS	28
#define ULP_FLOW_DB_RES_FUNC_MASK	0x70000000
#define ULP_FLOW_DB_RES_NXT_MASK	0x0FFFFFFF

/* Macro to copy the nxt_resource_idx */
#define ULP_FLOW_DB_RES_NXT_SET(dst, src)	{(dst) |= ((src) &\
					 ULP_FLOW_DB_RES_NXT_MASK); }
#define ULP_FLOW_DB_RES_NXT_RESET(dst)	((dst) &= ~(ULP_FLOW_DB_RES_NXT_MASK))

/*
 * Helper function to allocate the flow table and initialize
 *  is set.No validation being done in this function.
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

/*
 * Helper function to copy the resource params to resource info
 *  No validation being done in this function.
 *
 * resource_info [out] Ptr to resource information
 * params [in] The input params from the caller
 * returns none
 */
static void
ulp_flow_db_res_params_to_info(struct ulp_fdb_resource_info   *resource_info,
			       struct ulp_flow_db_res_params  *params)
{
	resource_info->nxt_resource_idx |= ((params->direction <<
				      ULP_FLOW_DB_RES_DIR_BIT) &
				     ULP_FLOW_DB_RES_DIR_MASK);
	resource_info->nxt_resource_idx |= ((params->resource_func <<
					     ULP_FLOW_DB_RES_FUNC_BITS) &
					    ULP_FLOW_DB_RES_FUNC_MASK);

	if (params->resource_func != BNXT_ULP_RESOURCE_FUNC_EM_TABLE) {
		resource_info->resource_hndl = (uint32_t)params->resource_hndl;
		resource_info->resource_type = params->resource_type;

	} else {
		resource_info->resource_em_handle = params->resource_hndl;
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
 * Helper function to de allocate the flow table.
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
		goto error_free;
	}

	/* Attach the flow database to the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, flow_db);

	/* Populate the regular flow table limits. */
	flow_tbl = &flow_db->flow_tbl[BNXT_ULP_REGULAR_FLOW_TABLE];
	flow_tbl->num_flows = dparms->num_flows + 1;
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
	rte_free(flow_db);

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
	if ((flow_tbl->num_flows + 1) >= flow_tbl->tail_index) {
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

	/* all good, return success */
	return 0;
}
