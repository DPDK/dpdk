/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "bnxt.h"
#include "tf_core.h"
#include "tf_session.h"
#include "tfp.h"

#include "tf_msg_common.h"
#include "tf_msg.h"
#include "hsi_struct_def_dpdk.h"
#include "hwrm_tf.h"

/**
 * Sends session open request to TF Firmware
 */
int
tf_msg_session_open(struct tf *tfp,
		    char *ctrl_chan_name,
		    uint8_t *fw_session_id)
{
	int rc;
	struct hwrm_tf_session_open_input req = { 0 };
	struct hwrm_tf_session_open_output resp = { 0 };
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	memcpy(&req.session_name, ctrl_chan_name, TF_SESSION_NAME_MAX);

	parms.tf_type = HWRM_TF_SESSION_OPEN;
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = TF_KONG_MB;

	rc = tfp_send_msg_direct(tfp,
				 &parms);
	if (rc)
		return rc;

	*fw_session_id = resp.fw_session_id;

	return rc;
}

/**
 * Sends session query config request to TF Firmware
 */
int
tf_msg_session_qcfg(struct tf *tfp)
{
	int rc;
	struct hwrm_tf_session_qcfg_input  req = { 0 };
	struct hwrm_tf_session_qcfg_output resp = { 0 };
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);
	struct tfp_send_msg_parms parms = { 0 };

	/* Populate the request */
	req.fw_session_id =
		tfp_cpu_to_le_32(tfs->session_id.internal.fw_session_id);

	parms.tf_type = HWRM_TF_SESSION_QCFG,
	parms.req_data = (uint32_t *)&req;
	parms.req_size = sizeof(req);
	parms.resp_data = (uint32_t *)&resp;
	parms.resp_size = sizeof(resp);
	parms.mailbox = TF_KONG_MB;

	rc = tfp_send_msg_direct(tfp,
				 &parms);
	return rc;
}
