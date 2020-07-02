/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_MSG_H_
#define _TF_MSG_H_

#include <rte_common.h>
#include <hsi_struct_def_dpdk.h>

#include "tf_tbl.h"
#include "tf_rm.h"
#include "tf_tcam.h"
#include "tf_global_cfg.h"

struct tf;

/* HWRM Direct messages */

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
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_open(struct tf *tfp,
			char *ctrl_chan_name,
			uint8_t *fw_session_id,
			uint8_t *fw_session_client_id);

/**
 * Sends session close request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * [in] ctrl_chan_name
 *   PCI name of the control channel
 *
 * [in] fw_session_id
 *   Pointer to the fw_session_id that is assigned to the session at
 *   time of session open
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_attach(struct tf *tfp,
			  char *ctrl_channel_name,
			  uint8_t tf_fw_session_id);

/**
 * Sends session client register request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * [in] ctrl_chan_name
 *   PCI name of the control channel
 *
 * [in/out] fw_session_client_id
 *   Pointer to the fw_session_client_id that is allocated on firmware
 *   side
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_client_register(struct tf *tfp,
				   char *ctrl_channel_name,
				   uint8_t *fw_session_client_id);

/**
 * Sends session client unregister request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * [in/out] fw_session_client_id
 *   Pointer to the fw_session_client_id that is allocated on firmware
 *   side
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_client_unregister(struct tf *tfp,
				     uint8_t fw_session_client_id);

/**
 * Sends session close request to Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_close(struct tf *tfp);

/**
 * Sends session query config request to TF Firmware
 *
 * [in] session
 *   Pointer to session handle
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_qcfg(struct tf *tfp);

/**
 * Sends session HW resource query capability request to TF Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * [in] size
 *   Number of elements in the query. Should be set to the max
 *   elements for the device type
 *
 * [out] query
 *   Pointer to an array of query elements
 *
 * [out] resv_strategy
 *   Pointer to the reservation strategy
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_qcaps(struct tf *tfp,
			      enum tf_dir dir,
			      uint16_t size,
			      struct tf_rm_resc_req_entry *query,
			      enum tf_rm_resc_resv_strategy *resv_strategy);

/**
 * Sends session HW resource allocation request to TF Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * [in] size
 *   Number of elements in the req and resv arrays
 *
 * [in] req
 *   Pointer to an array of request elements
 *
 * [in] resv
 *   Pointer to an array of reserved elements
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_alloc(struct tf *tfp,
			      enum tf_dir dir,
			      uint16_t size,
			      struct tf_rm_resc_req_entry *request,
			      struct tf_rm_resc_entry *resv);

/**
 * Sends session resource flush request to TF Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * [in] size
 *   Number of elements in the req and resv arrays
 *
 * [in] resv
 *   Pointer to an array of reserved elements that needs to be flushed
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_flush(struct tf *tfp,
			      enum tf_dir dir,
			      uint16_t size,
			      struct tf_rm_resc_entry *resv);
/**
 * Sends EM internal insert request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] params
 *   Pointer to em insert parameter list
 *
 * [in] rptr_index
 *   Record ptr index
 *
 * [in] rptr_entry
 *   Record ptr entry
 *
 * [in] num_of_entries
 *   Number of entries to insert
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_insert_em_internal_entry(struct tf *tfp,
				    struct tf_insert_em_entry_parms *params,
				    uint16_t *rptr_index,
				    uint8_t *rptr_entry,
				    uint8_t *num_of_entries);
/**
 * Sends EM internal delete request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] em_parms
 *   Pointer to em delete parameters
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_delete_em_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *em_parms);

/**
 * Sends EM mem register request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] page_lvl
 *   Page level
 *
 * [in] page_size
 *   Page size
 *
 * [in] dma_addr
 *   DMA Address for the memory page
 *
 * [in] ctx_id
 *   Context id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_em_mem_rgtr(struct tf *tfp,
		       int page_lvl,
		       int page_size,
		       uint64_t dma_addr,
		       uint16_t *ctx_id);

/**
 * Sends EM mem unregister request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] ctx_id
 *   Context id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_em_mem_unrgtr(struct tf *tfp,
			 uint16_t *ctx_id);

/**
 * Sends EM qcaps request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * [in] em_caps
 *   Pointer to EM capabilities
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_em_qcaps(struct tf *tfp,
		    int dir,
		    struct tf_em_caps *em_caps);

/**
 * Sends EM config request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] num_entries
 *   EM Table, key 0, number of entries to configure
 *
 * [in] key0_ctx_id
 *   EM Table, Key 0 context id
 *
 * [in] key1_ctx_id
 *   EM Table, Key 1 context id
 *
 * [in] record_ctx_id
 *   EM Table, Record context id
 *
 * [in] efc_ctx_id
 *   EM Table, EFC Table context id
 *
 * [in] flush_interval
 *   Flush pending HW cached flows every 1/10th of value set in
 *   seconds, both idle and active flows are flushed from the HW
 *   cache. If set to 0, this feature will be disabled.
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_em_cfg(struct tf *tfp,
		  uint32_t num_entries,
		  uint16_t key0_ctx_id,
		  uint16_t key1_ctx_id,
		  uint16_t record_ctx_id,
		  uint16_t efc_ctx_id,
		  uint8_t flush_interval,
		  int dir);

/**
 * Sends EM operation request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dir
 *   Receive or Transmit direction
 *
 * [in] op
 *   CFA Operator
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_em_op(struct tf *tfp,
		 int dir,
		 uint16_t op);

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
			  struct tf_tcam_set_parms *parms);

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
			   struct tf_tcam_free_parms *parms);

/**
 * Sends Set message of a Table Type element to the firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] dir
 *   Direction location of the element to set
 *
 * [in] hcapi_type
 *   Type of the object to set
 *
 * [in] size
 *   Size of the data to set
 *
 * [in] data
 *   Data to set
 *
 * [in] index
 *   Index to set
 *
 * Returns:
 *   0 - Success
 */
int tf_msg_set_tbl_entry(struct tf *tfp,
			 enum tf_dir dir,
			 uint16_t hcapi_type,
			 uint16_t size,
			 uint8_t *data,
			 uint32_t index);

/**
 * Sends get message of a Table Type element to the firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] dir
 *   Direction location of the element to get
 *
 * [in] hcapi_type
 *   Type of the object to get
 *
 * [in] size
 *   Size of the data read
 *
 * [in] data
 *   Data read
 *
 * [in] index
 *   Index to get
 *
 * Returns:
 *   0 - Success
 */
int tf_msg_get_tbl_entry(struct tf *tfp,
			 enum tf_dir dir,
			 uint16_t hcapi_type,
			 uint16_t size,
			 uint8_t *data,
			 uint32_t index);

/* HWRM Tunneled messages */

/**
 * Sends global cfg read request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] params
 *   Pointer to read parameters
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_get_global_cfg(struct tf *tfp,
			  struct tf_dev_global_cfg_parms *params);

/**
 * Sends global cfg update request to Firmware
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] params
 *   Pointer to write parameters
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_set_global_cfg(struct tf *tfp,
			  struct tf_dev_global_cfg_parms *params);

/**
 * Sends bulk get message of a Table Type element to the firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] parms
 *   Pointer to table get bulk parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_bulk_get_tbl_entry(struct tf *tfp,
			      enum tf_dir dir,
			      uint16_t hcapi_type,
			      uint32_t starting_idx,
			      uint16_t num_entries,
			      uint16_t entry_sz_in_bytes,
			      uint64_t physical_mem_addr);

/**
 * Sends Set message of a IF Table Type element to the firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] parms
 *   Pointer to IF table set parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_set_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_set_parms *params);

/**
 * Sends get message of a IF Table Type element to the firmware.
 *
 * [in] tfp
 *   Pointer to session handle
 *
 * [in] parms
 *   Pointer to IF table get parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_get_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_get_parms *params);

#endif  /* _TF_MSG_H_ */
