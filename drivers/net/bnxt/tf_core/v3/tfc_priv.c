/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include <errno.h>
#include "bnxt.h"
#include "tfc.h"
#include "tfc_priv.h"

int
tfc_get_fid(struct tfc *tfcp, uint16_t *fw_fid)
{
	struct bnxt *bp = NULL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (fw_fid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid fw_fid pointer");
		return -EINVAL;
	}

	bp = (struct bnxt *)tfcp->bp;
	if (bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid bp pointer");
		return -EINVAL;
	}

	*fw_fid = bp->fw_fid;

	return 0;
}

int
tfc_get_pfid(struct tfc *tfcp, uint16_t *pfid)
{
	struct bnxt *bp = NULL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (pfid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid pfid pointer");
		return -EINVAL;
	}

	bp = (struct bnxt *)tfcp->bp;
	if (bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid bp pointer");
		return -EINVAL;
	}

	if (BNXT_VF(bp) && bp->parent) {
		*pfid = bp->parent->fid;
		return 0;
	} else if (BNXT_PF(bp)) {
		*pfid = bp->fw_fid;
		return 0;
	}

	PMD_DRV_LOG_LINE(ERR, "Invalid FID in bp");
	return -EINVAL;
}

int
tfc_bp_is_pf(struct tfc *tfcp, bool *is_pf)
{
	struct bnxt *bp = NULL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (is_pf == NULL) {
		PMD_DRV_LOG_LINE(ERR, "invalid is_pf pointer");
		return -EINVAL;
	}

	bp = (struct bnxt *)tfcp->bp;
	if (bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid bp pointer");
		return -EINVAL;
	}

	if (BNXT_PF(bp)) {
		*is_pf = true;
		return 0;
	}
	*is_pf = false;
	return 0;
}

int tfc_bp_vf_max(struct tfc *tfcp, uint16_t *max_vf)
{
	struct bnxt *bp = NULL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (max_vf == NULL) {
		PMD_DRV_LOG_LINE(ERR, "invalid max_vf pointer");
		return -EINVAL;
	}

	bp = (struct bnxt *)tfcp->bp;
	if (bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid bp pointer");
		return -EINVAL;
	}

	if (!BNXT_PF(bp)) {
		PMD_DRV_LOG_LINE(ERR, "not a PF");
		return -EINVAL;
	}

	*max_vf = bp->pf->first_vf_id + BNXT_MAX_VFS(bp);
	return 0;
}
