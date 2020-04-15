/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include "bnxt_ulp.h"
#include "tf_ext_flow_handle.h"
#include "ulp_mark_mgr.h"
#include "bnxt_tf_common.h"
#include "../bnxt.h"
#include "ulp_template_db.h"
#include "ulp_template_struct.h"

/*
 * Allocate and Initialize all Mark Manager resources for this ulp context.
 *
 * ctxt [in] The ulp context for the mark manager.
 *
 */
int32_t
ulp_mark_db_init(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_mark_tbl *mark_tbl = NULL;
	uint32_t dev_id;

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

	mark_tbl = rte_zmalloc("ulp_rx_mark_tbl_ptr",
			       sizeof(struct bnxt_ulp_mark_tbl), 0);
	if (!mark_tbl)
		goto mem_error;

	/* Need to allocate 2 * Num flows to account for hash type bit. */
	mark_tbl->lfid_tbl = rte_zmalloc("ulp_rx_em_flow_mark_table",
					 dparms->lfid_entries *
					    sizeof(struct bnxt_lfid_mark_info),
					 0);

	if (!mark_tbl->lfid_tbl)
		goto mem_error;

	/* Need to allocate 2 * Num flows to account for hash type bit. */
	mark_tbl->gfid_tbl = rte_zmalloc("ulp_rx_eem_flow_mark_table",
					 2 * dparms->num_flows *
					    sizeof(struct bnxt_gfid_mark_info),
					 0);
	if (!mark_tbl->gfid_tbl)
		goto mem_error;

	/*
	 * TBD: This needs to be generalized for better mark handling
	 * These values are used to compress the FID to the allowable index
	 * space.  The FID from hw may be the full hash.
	 */
	mark_tbl->gfid_max	= dparms->gfid_entries - 1;
	mark_tbl->gfid_mask	= (dparms->gfid_entries / 2) - 1;
	mark_tbl->gfid_type_bit = (dparms->gfid_entries / 2);

	BNXT_TF_DBG(DEBUG, "GFID Max = 0x%08x\nGFID MASK = 0x%08x\n",
		    mark_tbl->gfid_max,
		    mark_tbl->gfid_mask);

	/* Add the mart tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_mark_db_set(ctxt, mark_tbl);

	return 0;

mem_error:
	rte_free(mark_tbl->gfid_tbl);
	rte_free(mark_tbl->lfid_tbl);
	rte_free(mark_tbl);
	BNXT_TF_DBG(DEBUG,
		    "Failed to allocate memory for mark mgr\n");

	return -ENOMEM;
}
