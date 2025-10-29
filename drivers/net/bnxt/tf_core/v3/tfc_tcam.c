/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "tfc.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc_msg.h"
#include "tfc_util.h"

int tfc_tcam_alloc(struct tfc *tfcp, uint16_t fid, enum cfa_track_type tt,
		   uint16_t priority, uint8_t key_sz_in_bytes,
		   struct tfc_tcam_info *tcam_info)
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
	bp = tfcp->bp;

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_info is NULL");
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tcam subtype: %d",
				 tcam_info->rsubtype);
		return -EINVAL;
	}

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

	rc = tfc_msg_tcam_alloc(tfcp, fid, sid, tcam_info->dir,
				tcam_info->rsubtype, tt, priority,
				key_sz_in_bytes, &tcam_info->id);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "alloc failed %s:%s %s",
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype),
				 strerror(-rc));

	return rc;
}

int tfc_tcam_alloc_set(struct tfc *tfcp, uint16_t fid, enum cfa_track_type tt,
		       uint16_t priority, struct tfc_tcam_info *tcam_info,
		       const struct tfc_tcam_data *tcam_data)
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
	bp = tfcp->bp;

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_info is NULL");
		return -EINVAL;
	}

	if (tcam_data == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_data is NULL");
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tcam subtype: %d",
				 tcam_info->rsubtype);
		return -EINVAL;
	}

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

	rc = tfc_msg_tcam_alloc_set(tfcp, fid, sid, tcam_info->dir,
				    tcam_info->rsubtype, tt, &tcam_info->id,
				    priority, tcam_data->key,
				    tcam_data->key_sz_in_bytes,
				    tcam_data->mask,
				    tcam_data->remap,
				    tcam_data->remap_sz_in_bytes);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "alloc_set failed: %s:%s %s",
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype), strerror(-rc));

	return rc;
}

int tfc_tcam_set(struct tfc *tfcp, uint16_t fid,
		 const struct tfc_tcam_info *tcam_info,
		 const struct tfc_tcam_data *tcam_data)
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
	bp = tfcp->bp;

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_info is NULL");
		return -EINVAL;
	}

	if (tcam_data == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_data is NULL");
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tcam subtype: %d",
				 tcam_info->rsubtype);
		return -EINVAL;
	}

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

	rc = tfc_msg_tcam_set(tfcp, fid, sid, tcam_info->dir,
			      tcam_info->rsubtype, tcam_info->id,
			      tcam_data->key,
			      tcam_data->key_sz_in_bytes,
			      tcam_data->mask, tcam_data->remap,
			      tcam_data->remap_sz_in_bytes);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "set failed: %s:%s %d %s",
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id,
				 strerror(-rc));

	return rc;
}

int tfc_tcam_get(struct tfc *tfcp, uint16_t fid,
		 const struct tfc_tcam_info *tcam_info,
		 struct tfc_tcam_data *tcam_data)
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
	bp = tfcp->bp;

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_info is NULL");
		return -EINVAL;
	}

	if (tcam_data == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_data is NULL");
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tcam subtype: %d",
				 tcam_info->rsubtype);
		return -EINVAL;
	}

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

	rc = tfc_msg_tcam_get(tfcp, fid, sid, tcam_info->dir,
			      tcam_info->rsubtype, tcam_info->id,
			      tcam_data->key, &tcam_data->key_sz_in_bytes,
			      tcam_data->mask, tcam_data->remap,
			      &tcam_data->remap_sz_in_bytes);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "get failed: %s:%s %d %s",
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id,
				 strerror(-rc));

	return rc;
}

int tfc_tcam_free(struct tfc *tfcp, uint16_t fid, const struct tfc_tcam_info *tcam_info)
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

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tcam_info is NULL");
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tcam subtype: %d",
				 tcam_info->rsubtype);
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

	rc = tfc_msg_tcam_free(tfcp, fid, sid, tcam_info->dir,
			       tcam_info->rsubtype, tcam_info->id);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "free failed: %s:%s:%d %s",
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id,
				 strerror(-rc));
	return rc;
}

int tfc_tcam_priority_update(struct tfc *tfcp, uint16_t fid,
			     enum cfa_track_type tt,
			     const struct tfc_tcam_info *tcam_info,
			     uint16_t priority)
{
	int rc = 0;
	struct bnxt *bp;
	uint16_t sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tfcp pointer", __func__);
		return -EINVAL;
	}

	if (tfcp->bp == NULL || tfcp->tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: tfcp not initialized", __func__);
		return -EINVAL;
	}
	bp = tfcp->bp;

	if (tcam_info == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: tcam_info is NULL", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tcam subtype: %d", __func__,
				 tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		PMD_DRV_LOG_LINE(ERR, "%s: bp not PF or trusted VF", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "%s: Failed to retrieve SID, rc:%s",
				 __func__, strerror(-rc));
		return rc;
	}

	rc = tfc_msg_tcam_prioriry_update(tfcp, fid, sid, tcam_info->dir, tt,
					  tcam_info->rsubtype, tcam_info->id,
					  priority);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "%s: update failed: %s:%s %d %s", __func__,
				 tfc_dir_2_str(tcam_info->dir),
				 tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id,
				 strerror(-rc));

	return rc;
}
