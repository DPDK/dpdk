/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "rte_malloc.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "tfc_msg.h"
#include "tfc_util.h"

int tfc_if_tbl_set(struct tfc *tfcp, uint16_t fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   const uint8_t *data, uint8_t data_sz_in_bytes)
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

	if (tbl_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tbl_info is NULL");
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid cfa dir: %d", tbl_info->dir);
		return -EINVAL;
	}

	if (tbl_info->rsubtype >= CFA_RSUBTYPE_IF_TBL_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid if tbl subtype: %d",
				 tbl_info->rsubtype);
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

	rc = tfc_msg_if_tbl_set(tfcp, fid, sid, tbl_info->dir,
				tbl_info->rsubtype, tbl_info->id,
				data_sz_in_bytes, data);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "hwrm failed: %s:%s %d %s",
				 tfc_dir_2_str(tbl_info->dir),
				 tfc_if_tbl_2_str(tbl_info->rsubtype),
				 tbl_info->id, strerror(-rc));

	return rc;
}

int tfc_if_tbl_get(struct tfc *tfcp, uint16_t fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   uint8_t *data, uint8_t *data_sz_in_bytes)
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

	if (tbl_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tbl_info is NULL");
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid cfa dir: %d", tbl_info->dir);
		return -EINVAL;
	}

	if (tbl_info->rsubtype >= CFA_RSUBTYPE_IF_TBL_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid if tbl subtype: %d",
				 tbl_info->rsubtype);
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

	rc = tfc_msg_if_tbl_get(tfcp, fid, sid, tbl_info->dir,
				tbl_info->rsubtype, tbl_info->id,
				data_sz_in_bytes, data);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "hwrm failed: %s:%s %d %s",
				 tfc_dir_2_str(tbl_info->dir),
				 tfc_if_tbl_2_str(tbl_info->rsubtype),
				 tbl_info->id, strerror(-rc));
	return rc;
}
