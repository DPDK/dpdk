/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include "bnxt.h"
#include "bnxt_ulp.h"
#include "tf_ext_flow_handle.h"
#include "ulp_mark_mgr.h"
#include "bnxt_tf_common.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"

#define ULP_MARK_DB_ENTRY_SET_VALID(mark_info) ((mark_info)->flags |=\
						BNXT_ULP_MARK_VALID)
#define ULP_MARK_DB_ENTRY_IS_INVALID(mark_info) (!((mark_info)->flags &\
						   BNXT_ULP_MARK_VALID))
#define ULP_MARK_DB_ENTRY_SET_VFR_ID(mark_info) ((mark_info)->flags |=\
						 BNXT_ULP_MARK_VFR_ID)
#define ULP_MARK_DB_ENTRY_IS_VFR_ID(mark_info) ((mark_info)->flags &\
						BNXT_ULP_MARK_VFR_ID)
#define ULP_MARK_DB_ENTRY_IS_GLOBAL_HW_FID(mark_info) ((mark_info)->flags &\
						BNXT_ULP_MARK_GLOBAL_HW_FID)

static inline uint32_t
ulp_mark_db_idx_get(bool is_gfid, uint32_t fid, struct bnxt_ulp_mark_tbl *mtbl)
{
	uint32_t idx = 0, hashtype = 0;

	if (is_gfid) {
		TF_GET_HASH_TYPE_FROM_GFID(fid, hashtype);
		TF_GET_HASH_INDEX_FROM_GFID(fid, idx);

		/* Need to truncate anything beyond supported flows */
		idx &= mtbl->gfid_mask;
		if (hashtype)
			idx |= mtbl->gfid_type_bit;
	} else {
		idx = fid;
	}
	return idx;
}

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

	if (!dparms->mark_db_lfid_entries || !dparms->mark_db_gfid_entries) {
		BNXT_TF_DBG(DEBUG, "mark Table is not allocated\n");
		bnxt_ulp_cntxt_ptr2_mark_db_set(ctxt, NULL);
		return 0;
	}

	mark_tbl = rte_zmalloc("ulp_rx_mark_tbl_ptr",
			       sizeof(struct bnxt_ulp_mark_tbl), 0);
	if (!mark_tbl)
		goto mem_error;

	/* Need to allocate 2 * Num flows to account for hash type bit.*/
	mark_tbl->lfid_num_entries = dparms->mark_db_lfid_entries;
	mark_tbl->lfid_tbl = rte_zmalloc("ulp_rx_em_flow_mark_table",
					 mark_tbl->lfid_num_entries *
					 sizeof(struct bnxt_lfid_mark_info),
					 0);
	if (!mark_tbl->lfid_tbl)
		goto mem_error;

	/* Need to allocate 2 * Num flows to account for hash type bit */
	mark_tbl->gfid_num_entries = dparms->mark_db_gfid_entries;
	if (!mark_tbl->gfid_num_entries)
		goto gfid_not_required;

	mark_tbl->gfid_tbl = rte_zmalloc("ulp_rx_eem_flow_mark_table",
					 mark_tbl->gfid_num_entries *
					 sizeof(struct bnxt_gfid_mark_info),
					 0);
	if (!mark_tbl->gfid_tbl)
		goto mem_error;

	/*
	 * These values are used to compress the FID to the allowable index
	 * space.  The FID from hw may be the full hash which may be a big
	 * value to allocate and so allocate only needed hash values.
	 * gfid mask is the number of flow entries for the each left/right
	 * hash  The gfid type bit is used to get to the higher or lower hash
	 * entries.
	 */
	mark_tbl->gfid_mask	= (mark_tbl->gfid_num_entries / 2) - 1;
	mark_tbl->gfid_type_bit = (mark_tbl->gfid_num_entries / 2);

	BNXT_TF_DBG(DEBUG, "GFID Max = 0x%08x GFID MASK = 0x%08x\n",
		    mark_tbl->gfid_num_entries - 1,
		    mark_tbl->gfid_mask);

gfid_not_required:
	/* Add the mark tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_mark_db_set(ctxt, mark_tbl);
	return 0;

mem_error:
	if (mark_tbl) {
		rte_free(mark_tbl->gfid_tbl);
		rte_free(mark_tbl->lfid_tbl);
		rte_free(mark_tbl);
	}
	BNXT_TF_DBG(DEBUG, "Failed to allocate memory for mark mgr\n");
	return -ENOMEM;
}

/*
 * Release all resources in the Mark Manager for this ulp context
 *
 * ctxt [in] The ulp context for the mark manager
 *
 */
int32_t
ulp_mark_db_deinit(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_mark_tbl *mtbl;

	mtbl = bnxt_ulp_cntxt_ptr2_mark_db_get(ctxt);

	if (mtbl) {
		rte_free(mtbl->gfid_tbl);
		rte_free(mtbl->lfid_tbl);
		rte_free(mtbl);

		/* Safe to ignore on deinit */
		(void)bnxt_ulp_cntxt_ptr2_mark_db_set(ctxt, NULL);
	}

	return 0;
}

/*
 * Get a Mark from the Mark Manager
 *
 * ctxt [in] The ulp context for the mark manager
 *
 * is_gfid [in] The type of fid (GFID or LFID)
 *
 * fid [in] The flow id that is returned by HW in BD
 *
 * vfr_flag [out].it indicatesif mark is vfr_id or mark id
 *
 * mark [out] The mark that is associated with the FID
 *
 */
int32_t
ulp_mark_db_mark_get(struct bnxt_ulp_context *ctxt,
		     bool is_gfid,
		     uint32_t fid,
		     uint32_t *vfr_flag,
		     uint32_t *mark)
{
	struct bnxt_ulp_mark_tbl *mtbl;
	uint32_t idx = 0;

	if (!ctxt || !mark)
		return -EINVAL;

	mtbl = bnxt_ulp_cntxt_ptr2_mark_db_get(ctxt);
	if (!mtbl)
		return -EINVAL;

	idx = ulp_mark_db_idx_get(is_gfid, fid, mtbl);

	if (is_gfid) {
		if (idx >= mtbl->gfid_num_entries ||
		    ULP_MARK_DB_ENTRY_IS_INVALID(&mtbl->gfid_tbl[idx]))
			return -EINVAL;

		*vfr_flag = ULP_MARK_DB_ENTRY_IS_VFR_ID(&mtbl->gfid_tbl[idx]);
		*mark = mtbl->gfid_tbl[idx].mark_id;
	} else {
		if (idx >= mtbl->lfid_num_entries ||
		    ULP_MARK_DB_ENTRY_IS_INVALID(&mtbl->lfid_tbl[idx]))
			return -EINVAL;

		*vfr_flag = ULP_MARK_DB_ENTRY_IS_VFR_ID(&mtbl->lfid_tbl[idx]);
		*mark = mtbl->lfid_tbl[idx].mark_id;
	}

	return 0;
}

/*
 * Adds a Mark to the Mark Manager
 *
 * ctxt [in] The ulp context for the mark manager
 *
 * mark_flag [in] mark flags.
 *
 * fid [in] The flow id that is returned by HW in BD
 *
 * mark [in] The mark to be associated with the FID
 *
 */
int32_t
ulp_mark_db_mark_add(struct bnxt_ulp_context *ctxt,
		     uint32_t mark_flag,
		     uint32_t fid,
		     uint32_t mark)
{
	struct bnxt_ulp_mark_tbl *mtbl;
	uint32_t idx = 0;
	bool is_gfid;

	if (!ctxt) {
		BNXT_TF_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}

	mtbl = bnxt_ulp_cntxt_ptr2_mark_db_get(ctxt);
	if (!mtbl) {
		BNXT_TF_DBG(ERR, "Unable to get Mark DB\n");
		return -EINVAL;
	}

	is_gfid = (mark_flag & BNXT_ULP_MARK_GLOBAL_HW_FID);
	if (is_gfid) {
		idx = ulp_mark_db_idx_get(is_gfid, fid, mtbl);
		if (idx >= mtbl->gfid_num_entries) {
			BNXT_TF_DBG(ERR, "Mark index greater than allocated\n");
			return -EINVAL;
		}
		BNXT_TF_DBG(DEBUG, "Set GFID[0x%0x] = 0x%0x\n", idx, mark);
		mtbl->gfid_tbl[idx].mark_id = mark;
		ULP_MARK_DB_ENTRY_SET_VALID(&mtbl->gfid_tbl[idx]);

	} else {
		/* For the LFID, the FID is used as the index */
		if (fid >= mtbl->lfid_num_entries) {
			BNXT_TF_DBG(ERR, "Mark index greater than allocated\n");
			return -EINVAL;
		}
		BNXT_TF_DBG(DEBUG, "Set LFID[0x%0x] = 0x%0x\n", fid, mark);
		mtbl->lfid_tbl[fid].mark_id = mark;
		ULP_MARK_DB_ENTRY_SET_VALID(&mtbl->lfid_tbl[fid]);

		if (mark_flag & BNXT_ULP_MARK_VFR_ID)
			ULP_MARK_DB_ENTRY_SET_VFR_ID(&mtbl->lfid_tbl[fid]);
	}

	return 0;
}

/*
 * Removes a Mark from the Mark Manager
 *
 * ctxt [in] The ulp context for the mark manager
 *
 * mark_flag [in] mark flags.
 *
 * fid [in] The flow id that is returned by HW in BD
 *
 */
int32_t
ulp_mark_db_mark_del(struct bnxt_ulp_context *ctxt,
		     uint32_t mark_flag,
		     uint32_t fid)
{
	struct bnxt_ulp_mark_tbl *mtbl;
	uint32_t idx = 0;
	bool is_gfid;

	if (!ctxt) {
		BNXT_TF_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}

	mtbl = bnxt_ulp_cntxt_ptr2_mark_db_get(ctxt);
	if (!mtbl) {
		BNXT_TF_DBG(ERR, "Unable to get Mark DB\n");
		return -EINVAL;
	}

	is_gfid = (mark_flag & BNXT_ULP_MARK_GLOBAL_HW_FID);
	if (is_gfid) {
		idx = ulp_mark_db_idx_get(is_gfid, fid, mtbl);
		if (idx >= mtbl->gfid_num_entries) {
			BNXT_TF_DBG(ERR, "Mark index greater than allocated\n");
			return -EINVAL;
		}
		BNXT_TF_DBG(DEBUG, "Reset GFID[0x%0x]\n", idx);
		memset(&mtbl->gfid_tbl[idx], 0,
		       sizeof(struct bnxt_gfid_mark_info));

	} else {
		/* For the LFID, the FID is used as the index */
		if (fid >= mtbl->lfid_num_entries) {
			BNXT_TF_DBG(ERR, "Mark index greater than allocated\n");
			return -EINVAL;
		}
		memset(&mtbl->lfid_tbl[fid], 0,
		       sizeof(struct bnxt_lfid_mark_info));
	}

	return 0;
}
