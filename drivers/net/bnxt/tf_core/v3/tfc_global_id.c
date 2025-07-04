/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "tfc_msg.h"

int tfc_global_id_alloc(struct tfc *tfcp, uint16_t fid,
			enum tfc_domain_id domain_id, uint16_t req_cnt,
			const struct tfc_global_id_req *req,
			struct tfc_global_id *rsp, uint16_t *rsp_cnt,
			bool *first)
{
	int rc = 0;
	struct bnxt *bp;
	uint16_t sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->bp == NULL || tfcp->tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp not initialized");
		return -EINVAL;
	}

	if (req == NULL) {
		PMD_DRV_LOG_LINE(ERR, "global_id req is NULL");
		return -EINVAL;
	}

	if (rsp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "global_id rsp is NULL");
		return -EINVAL;
	}

	bp = tfcp->bp;
	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		PMD_DRV_LOG_LINE(ERR, "bp not PF or trusted VF");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to retrieve SID, rc:%s",
				 strerror(-rc));
		return rc;
	}

	rc = tfc_msg_global_id_alloc(tfcp, fid, sid, domain_id, req_cnt,
				     req, rsp, rsp_cnt, first);
	return rc;
}
