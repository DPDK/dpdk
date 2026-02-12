/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "tfc.h"

#include "tfc_msg.h"
#include "cfa_types.h"
#include "tfo.h"
#include "tfc_util.h"
#include "bnxt.h"

int tfc_identifier_alloc(struct tfc *tfcp, uint16_t fid,
			 enum cfa_track_type tt,
			 struct tfc_identifier_info *ident_info)
{
	int rc = 0;
	uint16_t sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (ident_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid ident_info pointer");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to retrieve SID, rc:%s",
			    strerror(-rc));
		return rc;
	}

	rc = tfc_msg_identifier_alloc(tfcp, ident_info->dir,
				      ident_info->rsubtype,
				      tt, fid, sid, &ident_info->id);

	if (rc)
		PMD_DRV_LOG_LINE(ERR, "hwrm failed %s:%s, rc:%s",
				 tfc_dir_2_str(ident_info->dir),
				 tfc_ident_2_str(ident_info->rsubtype),
				 strerror(-rc));

	return rc;
}

int tfc_identifier_free(struct tfc *tfcp, uint16_t fid,
			const struct tfc_identifier_info *ident_info)
{
	int rc = 0;
	uint16_t sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (ident_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid ident_info pointer");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to retrieve SID, rc:%s",
			    strerror(-rc));
		return rc;
	}

	rc = tfc_msg_identifier_free(tfcp, ident_info->dir,
				     ident_info->rsubtype,
				     fid, sid, ident_info->id);

	if (rc)
		PMD_DRV_LOG_LINE(ERR, "hwrm failed  %s:%s:%d, rc:%s",
				 tfc_dir_2_str(ident_info->dir),
				 tfc_ident_2_str(ident_info->rsubtype),
				 ident_info->id, strerror(-rc));

	return rc;
}
