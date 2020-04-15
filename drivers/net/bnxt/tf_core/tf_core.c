/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <stdio.h>

#include "tf_core.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tfp.h"
#include "bnxt.h"

int
tf_open_session(struct tf                    *tfp,
		struct tf_open_session_parms *parms)
{
	int rc;
	struct tf_session *session;
	struct tfp_calloc_parms alloc_parms;
	unsigned int domain, bus, slot, device;
	uint8_t fw_session_id;

	if (tfp == NULL || parms == NULL)
		return -EINVAL;

	/* Filter out any non-supported device types on the Core
	 * side. It is assumed that the Firmware will be supported if
	 * firmware open session succeeds.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_WH)
		return -ENOTSUP;

	/* Build the beginning of session_id */
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		PMD_DRV_LOG(ERR,
			    "Failed to scan device ctrl_chan_name\n");
		return -EINVAL;
	}

	/* open FW session and get a new session_id */
	rc = tf_msg_session_open(tfp,
				 parms->ctrl_chan_name,
				 &fw_session_id);
	if (rc) {
		/* Log error */
		if (rc == -EEXIST)
			PMD_DRV_LOG(ERR,
				    "Session is already open, rc:%d\n",
				    rc);
		else
			PMD_DRV_LOG(ERR,
				    "Open message send failed, rc:%d\n",
				    rc);

		parms->session_id.id = TF_FW_SESSION_ID_INVALID;
		return rc;
	}

	/* Allocate session */
	alloc_parms.nitems = 1;
	alloc_parms.size = sizeof(struct tf_session_info);
	alloc_parms.alignment = 0;
	rc = tfp_calloc(&alloc_parms);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "Failed to allocate session info, rc:%d\n",
			    rc);
		goto cleanup;
	}

	tfp->session = (struct tf_session_info *)alloc_parms.mem_va;

	/* Allocate core data for the session */
	alloc_parms.nitems = 1;
	alloc_parms.size = sizeof(struct tf_session);
	alloc_parms.alignment = 0;
	rc = tfp_calloc(&alloc_parms);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "Failed to allocate session data, rc:%d\n",
			    rc);
		goto cleanup;
	}

	tfp->session->core_data = alloc_parms.mem_va;

	session = (struct tf_session *)tfp->session->core_data;
	tfp_memcpy(session->ctrl_chan_name,
		   parms->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	/* Initialize Session */
	session->device_type = parms->device_type;

	/* Construct the Session ID */
	session->session_id.internal.domain = domain;
	session->session_id.internal.bus = bus;
	session->session_id.internal.device = device;
	session->session_id.internal.fw_session_id = fw_session_id;

	rc = tf_msg_session_qcfg(tfp);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "Query config message send failed, rc:%d\n",
			    rc);
		goto cleanup_close;
	}

	session->ref_count++;

	/* Return session ID */
	parms->session_id = session->session_id;

	PMD_DRV_LOG(INFO,
		    "Session created, session_id:%d\n",
		    parms->session_id.id);

	PMD_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    parms->session_id.internal.domain,
		    parms->session_id.internal.bus,
		    parms->session_id.internal.device,
		    parms->session_id.internal.fw_session_id);

	return 0;

 cleanup:
	tfp_free(tfp->session->core_data);
	tfp_free(tfp->session);
	tfp->session = NULL;
	return rc;

 cleanup_close:
	return -EINVAL;
}
