/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "tfc.h"
#include "tfo.h"
#include "tfc_priv.h"
#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_mpc.h"
#include "cfa_bld_mpcops.h"
#include "cfa_bld_devops.h"

/*
 * The tfc_open and tfc_close APIs may only be used for setting TFC software
 * state.  They are never used to modify the HW state.  That is, they are not
 * allowed to send HWRM messages.
 */

int tfc_open(struct tfc *tfcp)
{
	int rc = 0;
	bool is_pf;

	/* Initialize the TF object */
	if (tfcp->tfo) {
		PMD_DRV_LOG_LINE(WARNING, "tfc_opened already");
		return rc;
	}
	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;
	tfo_open(&tfcp->tfo, is_pf);

	return rc;
}

int tfc_close(struct tfc *tfcp)
{
	int rc = 0;
	uint16_t sid;
	uint8_t tsid;
	bool valid;

	/* Nullify the TF object */
	if (tfcp->tfo) {
		if (tfo_sid_get(tfcp->tfo, &sid) == 0) {
			/*
			 * If no error, then there is a valid SID which means
			 * that the FID is still associated with the SID.
			 */
			PMD_DRV_LOG_LINE(WARNING, "There is still a session "
					 "associated with this object");
		}

		for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
			rc = tfo_ts_get(tfcp->tfo, tsid, NULL, NULL, &valid, NULL);
			if (rc == 0 && valid) {
				PMD_DRV_LOG_LINE(WARNING, "There is still a "
						 "tsid %d associated",
						 tsid);
			}
		}
		tfo_close(&tfcp->tfo);
	}

	return rc;
}
