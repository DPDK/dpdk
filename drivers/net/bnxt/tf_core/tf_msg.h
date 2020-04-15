/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_MSG_H_
#define _TF_MSG_H_

#include "tf_rm.h"

struct tf;

/**
 * Sends session open request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * [in] ctrl_chan_name
 *   PCI name of the control channel
 *
 * [in/out] fw_session_id
 *   Pointer to the fw_session_id that is allocated on firmware side
 *
 * Returns:
 *
 */
int tf_msg_session_open(struct tf *tfp,
			char *ctrl_chan_name,
			uint8_t *fw_session_id);

/**
 * Sends session query config request to TF Firmware
 */
int tf_msg_session_qcfg(struct tf *tfp);

/**
 * Sends session HW resource query capability request to TF Firmware
 */
int tf_msg_session_hw_resc_qcaps(struct tf *tfp,
				 enum tf_dir dir,
				 struct tf_rm_hw_query *hw_query);

#endif  /* _TF_MSG_H_ */
