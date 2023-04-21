/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MARK_MGR_H_
#define _ULP_MARK_MGR_H_

#include "bnxt_ulp.h"

#define BNXT_ULP_MARK_VALID   0x1
#define BNXT_ULP_MARK_VFR_ID  0x2
#define BNXT_ULP_MARK_GLOBAL_HW_FID 0x4
#define BNXT_ULP_MARK_LOCAL_HW_FID 0x8

struct bnxt_lfid_mark_info {
	uint16_t	mark_id;
	uint16_t	flags;
};

struct bnxt_gfid_mark_info {
	uint32_t	mark_id;
	uint16_t	flags;
};

struct bnxt_ulp_mark_tbl {
	struct bnxt_lfid_mark_info	*lfid_tbl;
	struct bnxt_gfid_mark_info	*gfid_tbl;
	uint32_t			lfid_num_entries;
	uint32_t			gfid_num_entries;
	uint32_t			gfid_mask;
	uint32_t			gfid_type_bit;
};

/*
 * Allocate and Initialize all Mark Manager resources for this ulp context.
 *
 * Initialize MARK database for GFID & LFID tables
 * GFID: Global flow id which is based on EEM hash id.
 * LFID: Local flow id which is the CFA action pointer.
 * GFID is used for EEM flows, LFID is used for EM flows.
 *
 * Flow mapper modules adds mark_id in the MARK database.
 *
 * BNXT PMD receive handler extracts the hardware flow id from the
 * received completion record. Fetches mark_id from the MARK
 * database using the flow id. Injects mark_id into the packet's mbuf.
 *
 * ctxt [in] The ulp context for the mark manager.
 */
int32_t
ulp_mark_db_init(struct bnxt_ulp_context *ctxt);

/*
 * Release all resources in the Mark Manager for this ulp context
 *
 * ctxt [in] The ulp context for the mark manager
 */
int32_t
ulp_mark_db_deinit(struct bnxt_ulp_context *ctxt);

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
		     uint32_t *mark);

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
		     uint32_t gfid,
		     uint32_t mark);

/*
 * Removes a Mark from the Mark Manager
 *
 * ctxt [in] The ulp context for the mark manager
 *
 * mark_flag [in] mark flags
 *
 * fid [in] The flow id that is returned by HW in BD
 *
 */
int32_t
ulp_mark_db_mark_del(struct bnxt_ulp_context *ctxt,
		     uint32_t mark_flag,
		     uint32_t gfid);
#endif /* _ULP_MARK_MGR_H_ */
