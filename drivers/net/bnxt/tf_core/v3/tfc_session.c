/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "tfc.h"

#include "tfc_msg.h"
#include "cfa_types.h"
#include "tfo.h"
#include "bnxt.h"

int tfc_session_id_alloc(struct tfc *tfcp, uint16_t fid, uint16_t *sid)
{
	int rc = 0;
	uint16_t current_sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (sid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid sid pointer");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &current_sid);
	if (rc == 0) {
		PMD_DRV_LOG_LINE(ERR,
				 "Cannot allocate SID, current session is %u",
				 current_sid);
		return -EBUSY;
	} else if (rc != -ENODEV) {
		PMD_DRV_LOG_LINE(ERR, "Getting current sid failed, rc:%s",
				 strerror(-rc));
		return rc;
	}
	/* -ENODEV ==> current SID is invalid */

	rc = tfc_msg_session_id_alloc(tfcp, fid, sid);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
				 "session id alloc message failed, rc:%s",
				 strerror(-rc));
		return rc;
	}

	rc = tfo_sid_set(tfcp->tfo, *sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to store session id, rc:%s",
				 strerror(-rc));
		return rc;
	}

	return rc;
}

int tfc_session_fid_add(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			uint16_t *fid_cnt)
{
	int rc = 0;
	uint16_t current_sid = INVALID_SID;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (fid_cnt == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid fid_cnt pointer");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &current_sid);
	if (rc == 0) {
		/* SID is valid if rc == 0 */
		if (current_sid != sid) {
			PMD_DRV_LOG_LINE(ERR,
					 "Cannot add FID to SID %u,"
					 " current session is %u",
					 sid, current_sid);
			return -EBUSY;
		}
	} else if (rc != -ENODEV) {
		PMD_DRV_LOG_LINE(ERR,
				 "Getting current sid failed, rc:%s",
				 strerror(-rc));
		return rc;
	}
	/* -ENODEV ==> current SID is invalid */

	rc = tfc_msg_session_fid_add(tfcp, fid, sid, fid_cnt);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
				 "session fid add message failed, rc:%s",
				 strerror(-rc));
		return rc;
	}

	if (current_sid != sid) {
		rc = tfo_sid_set(tfcp->tfo, sid);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR,
					 "Failed to store session id, rc:%s",
					 strerror(-rc));
			return rc;
		}
	}

	return rc;
}
int tfc_session_fid_rem(struct tfc *tfcp, uint16_t fid, uint16_t *fid_cnt)
{
	int rc = 0;
	uint16_t sid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (fid_cnt == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid fid_cnt pointer");
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "no sid allocated, rc:%s", strerror(-rc));
		return rc;
	}

	rc = tfc_msg_session_fid_rem(tfcp, fid, sid, fid_cnt);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
				 "session fid rem message failed, rc:%s",
				 strerror(-rc));
		return rc;
	}

	if (((struct bnxt *)tfcp->bp)->fw_fid == fid) {
		rc = tfo_sid_set(tfcp->tfo, INVALID_SID);
		if (rc)
			PMD_DRV_LOG_LINE(ERR,
					 "Failed to reset session id, rc:%s", strerror(-rc));
	}

	return rc;
}
