/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "ulp_flow_db.h"
#include "ulp_template_struct.h"

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
