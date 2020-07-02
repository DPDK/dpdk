/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

int
tf_session_get_session(struct tf *tfp,
		       struct tf_session *tfs)
{
	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		TFP_DRV_LOG(ERR, "Session not created\n");
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	return 0;
}

int
tf_session_get_device(struct tf_session *tfs,
		      struct tf_device *tfd)
{
	if (tfs->dev == NULL) {
		TFP_DRV_LOG(ERR, "Device not created\n");
		return -EINVAL;
	}
	tfd = tfs->dev;

	return 0;
}
