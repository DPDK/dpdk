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
 * Sends session close request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * [in] fw_session_id
 *   Pointer to the fw_session_id that is assigned to the session at
 *   time of session open
 *
 * Returns:
 *
 */
int tf_msg_session_attach(struct tf *tfp,
			  char *ctrl_channel_name,
			  uint8_t tf_fw_session_id);

/**
 * Sends session close request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * Returns:
 *
 */
int tf_msg_session_close(struct tf *tfp);

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

/**
 * Sends session HW resource allocation request to TF Firmware
 */
int tf_msg_session_hw_resc_alloc(struct tf *tfp,
				 enum tf_dir dir,
				 struct tf_rm_hw_alloc *hw_alloc,
				 struct tf_rm_entry *hw_entry);

/**
 * Sends session HW resource free request to TF Firmware
 */
int tf_msg_session_hw_resc_free(struct tf *tfp,
				enum tf_dir dir,
				struct tf_rm_entry *hw_entry);

/**
 * Sends session HW resource flush request to TF Firmware
 */
int tf_msg_session_hw_resc_flush(struct tf *tfp,
				 enum tf_dir dir,
				 struct tf_rm_entry *hw_entry);

/**
 * Sends session SRAM resource query capability request to TF Firmware
 */
int tf_msg_session_sram_resc_qcaps(struct tf *tfp,
				   enum tf_dir dir,
				   struct tf_rm_sram_query *sram_query);

/**
 * Sends session SRAM resource allocation request to TF Firmware
 */
int tf_msg_session_sram_resc_alloc(struct tf *tfp,
				   enum tf_dir dir,
				   struct tf_rm_sram_alloc *sram_alloc,
				   struct tf_rm_entry *sram_entry);

/**
 * Sends session SRAM resource free request to TF Firmware
 */
int tf_msg_session_sram_resc_free(struct tf *tfp,
				  enum tf_dir dir,
				  struct tf_rm_entry *sram_entry);

/**
 * Sends session SRAM resource flush request to TF Firmware
 */
int tf_msg_session_sram_resc_flush(struct tf *tfp,
				   enum tf_dir dir,
				   struct tf_rm_entry *sram_entry);

/**
 * Sends tcam entry 'set' to the Firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] parms
 *   Pointer to set parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_tcam_entry_set(struct tf *tfp,
			  struct tf_set_tcam_entry_parms *parms);

/**
 * Sends tcam entry 'free' to the Firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] parms
 *   Pointer to free parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_tcam_entry_free(struct tf *tfp,
			   struct tf_free_tcam_entry_parms *parms);

#endif  /* _TF_MSG_H_ */
