/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "tfc.h"
#include "bnxt.h"
#include "tfc_msg.h"
#include "tfc_util.h"

static int
tfc_hot_upgrade_validate(struct tfc *tfcp, uint8_t app_inst_id,
			 uint16_t *sid)
{
	struct bnxt *bp;
	int rc = -EINVAL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return rc;
	}

	if (tfcp->bp == NULL || tfcp->tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp not initialized");
		return rc;
	}
	bp = tfcp->bp;

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		PMD_DRV_LOG_LINE(ERR, "bp not PF or trusted VF");
		return rc;
	}

	if (app_inst_id > CFA_HOT_UPGRADE_APP_INSTANCE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid app instance: %u",
				 app_inst_id);
		return rc;
	}

	*sid = 0;
	rc = tfo_sid_get(tfcp->tfo, sid);
	if (rc)
		PMD_DRV_LOG_LINE(ERR,
				 "Failed to retrieve SID, rc:%d",
				 rc);

	return rc;
}

int
tfc_hot_up_app_inst_count(struct tfc *tfcp, uint16_t fid,
			  uint8_t app_inst_id, uint8_t *app_inst_cnt)
{
	uint16_t sid;
	int rc = 0;

	if (tfc_hot_upgrade_validate(tfcp, app_inst_id, &sid)) {
		PMD_DRV_LOG_LINE(ERR, "failed validate for instance %u",
				 app_inst_id);
		return -EINVAL;
	}

	rc = tfc_msg_hot_upgrade_process(tfcp, fid, sid, app_inst_id,
					 CFA_HOT_UPGRADE_CMD_GET, 0,
					 app_inst_cnt);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "failed for app instance id %d err=%d",
				 app_inst_id, rc);

	return rc;
}

int
tfc_hot_up_app_inst_alloc(struct tfc *tfcp, uint16_t fid,
			  uint8_t app_inst_id, uint8_t app_inst_cnt,
			  uint8_t *session)
{
	uint16_t sid;
	int rc = 0;

	if (tfc_hot_upgrade_validate(tfcp,  app_inst_id, &sid)) {
		PMD_DRV_LOG_LINE(ERR, "failed validate for instance %u",
				 app_inst_id);
		return -EINVAL;
	}

	rc = tfc_msg_hot_upgrade_process(tfcp, fid, sid, app_inst_id,
					 CFA_HOT_UPGRADE_CMD_ALLOC,
					 app_inst_cnt, session);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "failed for app instance id %d err=%d",
				 app_inst_id, rc);

	return rc;
}

int
tfc_hot_up_app_inst_free(struct tfc *tfcp, uint16_t fid,
			 uint8_t app_inst_id)
{
	uint8_t session_cnt = 0;
	uint16_t sid;
	int rc = 0;

	if (tfc_hot_upgrade_validate(tfcp,  app_inst_id, &sid)) {
		PMD_DRV_LOG_LINE(ERR, "failed validate for instance %u",
				 app_inst_id);
		return -EINVAL;
	}

	rc = tfc_msg_hot_upgrade_process(tfcp, fid, sid, app_inst_id,
					 CFA_HOT_UPGRADE_CMD_FREE,
					 0, &session_cnt);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "failed for app instance id %d err=%d",
				 app_inst_id, rc);

	return rc;
}

int
tfc_hot_up_app_inst_set(struct tfc *tfcp, uint16_t fid, uint8_t app_inst_id)
{
	uint8_t session_cnt = 0;
	uint16_t sid;
	int rc = 0;

	if (tfc_hot_upgrade_validate(tfcp,  app_inst_id, &sid)) {
		PMD_DRV_LOG_LINE(ERR, "failed validate for instance %u",
				 app_inst_id);
		return -EINVAL;
	}

	rc = tfc_msg_hot_upgrade_process(tfcp, fid, sid, app_inst_id,
					 CFA_HOT_UPGRADE_CMD_SET,
					 1, &session_cnt);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "failed for app instance id %d err=%d",
				 app_inst_id, rc);

	return rc;
}
