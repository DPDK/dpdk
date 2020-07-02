/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>

#include "tf_session.h"
#include "tf_common.h"
#include "tf_msg.h"
#include "tfp.h"

int
tf_session_open_session(struct tf *tfp,
			struct tf_session_open_session_parms *parms)
{
	int rc;
	struct tf_session *session = NULL;
	struct tfp_calloc_parms cparms;
	uint8_t fw_session_id;
	union tf_session_id *session_id;

	TF_CHECK_PARMS2(tfp, parms);

	/* Open FW session and get a new session_id */
	rc = tf_msg_session_open(tfp,
				 parms->open_cfg->ctrl_chan_name,
				 &fw_session_id);
	if (rc) {
		/* Log error */
		if (rc == -EEXIST)
			TFP_DRV_LOG(ERR,
				    "Session is already open, rc:%s\n",
				    strerror(-rc));
		else
			TFP_DRV_LOG(ERR,
				    "Open message send failed, rc:%s\n",
				    strerror(-rc));

		parms->open_cfg->session_id.id = TF_FW_SESSION_ID_INVALID;
		return rc;
	}

	/* Allocate session */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session_info);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session info, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	tfp->session = (struct tf_session_info *)cparms.mem_va;

	/* Allocate core data for the session */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_session);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session data, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}
	tfp->session->core_data = cparms.mem_va;

	/* Initialize Session and Device */
	session = (struct tf_session *)tfp->session->core_data;
	session->ver.major = 0;
	session->ver.minor = 0;
	session->ver.update = 0;

	session_id = &parms->open_cfg->session_id;
	session->session_id.internal.domain = session_id->internal.domain;
	session->session_id.internal.bus = session_id->internal.bus;
	session->session_id.internal.device = session_id->internal.device;
	session->session_id.internal.fw_session_id = fw_session_id;
	/* Return the allocated fw session id */
	session_id->internal.fw_session_id = fw_session_id;

	session->shadow_copy = parms->open_cfg->shadow_copy;

	tfp_memcpy(session->ctrl_chan_name,
		   parms->open_cfg->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	rc = dev_bind(tfp,
		      parms->open_cfg->device_type,
		      session->shadow_copy,
		      &parms->open_cfg->resources,
		      session->dev);
	/* Logging handled by dev_bind */
	if (rc)
		return rc;

	/* Query for Session Config
	 */
	rc = tf_msg_session_qcfg(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Query config message send failed, rc:%s\n",
			    strerror(-rc));
		goto cleanup_close;
	}

	session->ref_count++;

	return 0;

 cleanup:
	tfp_free(tfp->session->core_data);
	tfp_free(tfp->session);
	tfp->session = NULL;
	return rc;

 cleanup_close:
	tf_close_session(tfp);
	return -EINVAL;
}

int
tf_session_attach_session(struct tf *tfp __rte_unused,
			  struct tf_session_attach_session_parms *parms __rte_unused)
{
	int rc = -EOPNOTSUPP;

	TF_CHECK_PARMS2(tfp, parms);

	TFP_DRV_LOG(ERR,
		    "Attach not yet supported, rc:%s\n",
		    strerror(-rc));
	return rc;
}

int
tf_session_close_session(struct tf *tfp,
			 struct tf_session_close_session_parms *parms)
{
	int rc;
	struct tf_session *tfs = NULL;
	struct tf_dev_info *tfd = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	if (tfs->session_id.id == TF_SESSION_ID_INVALID) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Invalid session id, unable to close, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* Record the session we're closing so the caller knows the
	 * details.
	 */
	*parms->session_id = tfs->session_id;

	rc = tf_session_get_device(tfs, &tfd);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Device lookup failed, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	/* In case we're attached only the session client gets closed */
	rc = tf_msg_session_close(tfp);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "FW Session close failed, rc:%s\n",
			    strerror(-rc));
	}

	tfs->ref_count--;

	/* Final cleanup as we're last user of the session */
	if (tfs->ref_count == 0) {
		/* Unbind the device */
		rc = dev_unbind(tfp, tfd);
		if (rc) {
			/* Log error */
			TFP_DRV_LOG(ERR,
				    "Device unbind failed, rc:%s\n",
				    strerror(-rc));
		}

		tfp_free(tfp->session->core_data);
		tfp_free(tfp->session);
		tfp->session = NULL;
	}

	return 0;
}

int
tf_session_get_session(struct tf *tfp,
		       struct tf_session **tfs)
{
	int rc;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Session not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	*tfs = (struct tf_session *)(tfp->session->core_data);

	return 0;
}

int
tf_session_get_device(struct tf_session *tfs,
		      struct tf_dev_info **tfd)
{
	int rc;

	if (tfs->dev == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Device not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	*tfd = tfs->dev;

	return 0;
}

int
tf_session_get_fw_session_id(struct tf *tfp,
			     uint8_t *fw_session_id)
{
	int rc;
	struct tf_session *tfs = NULL;

	if (tfp->session == NULL) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR,
			    "Session not created, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	rc = tf_session_get_session(tfp, &tfs);
	if (rc)
		return rc;

	*fw_session_id = tfs->session_id.internal.fw_session_id;

	return 0;
}
