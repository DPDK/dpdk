/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include "avf_type.h"
#include "avf_adminq.h"
#include "avf_prototype.h"
#include "virtchnl.h"


/**
 * avf_set_mac_type - Sets MAC type
 * @hw: pointer to the HW structure
 *
 * This function sets the mac type of the adapter based on the
 * vendor ID and device ID stored in the hw structure.
 **/
enum avf_status_code avf_set_mac_type(struct avf_hw *hw)
{
	enum avf_status_code status = AVF_SUCCESS;

	DEBUGFUNC("avf_set_mac_type\n");

	if (hw->vendor_id == AVF_INTEL_VENDOR_ID) {
		switch (hw->device_id) {
	/* TODO: remove undefined device ID now, need to think how to
	 * remove them in share code
	 */
		case AVF_DEV_ID_ADAPTIVE_VF:
			hw->mac.type = AVF_MAC_VF;
			break;
		default:
			hw->mac.type = AVF_MAC_GENERIC;
			break;
		}
	} else {
		status = AVF_ERR_DEVICE_NOT_SUPPORTED;
	}

	DEBUGOUT2("avf_set_mac_type found mac: %d, returns: %d\n",
		  hw->mac.type, status);
	return status;
}

/**
 * avf_aq_str - convert AQ err code to a string
 * @hw: pointer to the HW structure
 * @aq_err: the AQ error code to convert
 **/
const char *avf_aq_str(struct avf_hw *hw, enum avf_admin_queue_err aq_err)
{
	switch (aq_err) {
	case AVF_AQ_RC_OK:
		return "OK";
	case AVF_AQ_RC_EPERM:
		return "AVF_AQ_RC_EPERM";
	case AVF_AQ_RC_ENOENT:
		return "AVF_AQ_RC_ENOENT";
	case AVF_AQ_RC_ESRCH:
		return "AVF_AQ_RC_ESRCH";
	case AVF_AQ_RC_EINTR:
		return "AVF_AQ_RC_EINTR";
	case AVF_AQ_RC_EIO:
		return "AVF_AQ_RC_EIO";
	case AVF_AQ_RC_ENXIO:
		return "AVF_AQ_RC_ENXIO";
	case AVF_AQ_RC_E2BIG:
		return "AVF_AQ_RC_E2BIG";
	case AVF_AQ_RC_EAGAIN:
		return "AVF_AQ_RC_EAGAIN";
	case AVF_AQ_RC_ENOMEM:
		return "AVF_AQ_RC_ENOMEM";
	case AVF_AQ_RC_EACCES:
		return "AVF_AQ_RC_EACCES";
	case AVF_AQ_RC_EFAULT:
		return "AVF_AQ_RC_EFAULT";
	case AVF_AQ_RC_EBUSY:
		return "AVF_AQ_RC_EBUSY";
	case AVF_AQ_RC_EEXIST:
		return "AVF_AQ_RC_EEXIST";
	case AVF_AQ_RC_EINVAL:
		return "AVF_AQ_RC_EINVAL";
	case AVF_AQ_RC_ENOTTY:
		return "AVF_AQ_RC_ENOTTY";
	case AVF_AQ_RC_ENOSPC:
		return "AVF_AQ_RC_ENOSPC";
	case AVF_AQ_RC_ENOSYS:
		return "AVF_AQ_RC_ENOSYS";
	case AVF_AQ_RC_ERANGE:
		return "AVF_AQ_RC_ERANGE";
	case AVF_AQ_RC_EFLUSHED:
		return "AVF_AQ_RC_EFLUSHED";
	case AVF_AQ_RC_BAD_ADDR:
		return "AVF_AQ_RC_BAD_ADDR";
	case AVF_AQ_RC_EMODE:
		return "AVF_AQ_RC_EMODE";
	case AVF_AQ_RC_EFBIG:
		return "AVF_AQ_RC_EFBIG";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", aq_err);
	return hw->err_str;
}

/**
 * avf_stat_str - convert status err code to a string
 * @hw: pointer to the HW structure
 * @stat_err: the status error code to convert
 **/
const char *avf_stat_str(struct avf_hw *hw, enum avf_status_code stat_err)
{
	switch (stat_err) {
	case AVF_SUCCESS:
		return "OK";
	case AVF_ERR_NVM:
		return "AVF_ERR_NVM";
	case AVF_ERR_NVM_CHECKSUM:
		return "AVF_ERR_NVM_CHECKSUM";
	case AVF_ERR_PHY:
		return "AVF_ERR_PHY";
	case AVF_ERR_CONFIG:
		return "AVF_ERR_CONFIG";
	case AVF_ERR_PARAM:
		return "AVF_ERR_PARAM";
	case AVF_ERR_MAC_TYPE:
		return "AVF_ERR_MAC_TYPE";
	case AVF_ERR_UNKNOWN_PHY:
		return "AVF_ERR_UNKNOWN_PHY";
	case AVF_ERR_LINK_SETUP:
		return "AVF_ERR_LINK_SETUP";
	case AVF_ERR_ADAPTER_STOPPED:
		return "AVF_ERR_ADAPTER_STOPPED";
	case AVF_ERR_INVALID_MAC_ADDR:
		return "AVF_ERR_INVALID_MAC_ADDR";
	case AVF_ERR_DEVICE_NOT_SUPPORTED:
		return "AVF_ERR_DEVICE_NOT_SUPPORTED";
	case AVF_ERR_MASTER_REQUESTS_PENDING:
		return "AVF_ERR_MASTER_REQUESTS_PENDING";
	case AVF_ERR_INVALID_LINK_SETTINGS:
		return "AVF_ERR_INVALID_LINK_SETTINGS";
	case AVF_ERR_AUTONEG_NOT_COMPLETE:
		return "AVF_ERR_AUTONEG_NOT_COMPLETE";
	case AVF_ERR_RESET_FAILED:
		return "AVF_ERR_RESET_FAILED";
	case AVF_ERR_SWFW_SYNC:
		return "AVF_ERR_SWFW_SYNC";
	case AVF_ERR_NO_AVAILABLE_VSI:
		return "AVF_ERR_NO_AVAILABLE_VSI";
	case AVF_ERR_NO_MEMORY:
		return "AVF_ERR_NO_MEMORY";
	case AVF_ERR_BAD_PTR:
		return "AVF_ERR_BAD_PTR";
	case AVF_ERR_RING_FULL:
		return "AVF_ERR_RING_FULL";
	case AVF_ERR_INVALID_PD_ID:
		return "AVF_ERR_INVALID_PD_ID";
	case AVF_ERR_INVALID_QP_ID:
		return "AVF_ERR_INVALID_QP_ID";
	case AVF_ERR_INVALID_CQ_ID:
		return "AVF_ERR_INVALID_CQ_ID";
	case AVF_ERR_INVALID_CEQ_ID:
		return "AVF_ERR_INVALID_CEQ_ID";
	case AVF_ERR_INVALID_AEQ_ID:
		return "AVF_ERR_INVALID_AEQ_ID";
	case AVF_ERR_INVALID_SIZE:
		return "AVF_ERR_INVALID_SIZE";
	case AVF_ERR_INVALID_ARP_INDEX:
		return "AVF_ERR_INVALID_ARP_INDEX";
	case AVF_ERR_INVALID_FPM_FUNC_ID:
		return "AVF_ERR_INVALID_FPM_FUNC_ID";
	case AVF_ERR_QP_INVALID_MSG_SIZE:
		return "AVF_ERR_QP_INVALID_MSG_SIZE";
	case AVF_ERR_QP_TOOMANY_WRS_POSTED:
		return "AVF_ERR_QP_TOOMANY_WRS_POSTED";
	case AVF_ERR_INVALID_FRAG_COUNT:
		return "AVF_ERR_INVALID_FRAG_COUNT";
	case AVF_ERR_QUEUE_EMPTY:
		return "AVF_ERR_QUEUE_EMPTY";
	case AVF_ERR_INVALID_ALIGNMENT:
		return "AVF_ERR_INVALID_ALIGNMENT";
	case AVF_ERR_FLUSHED_QUEUE:
		return "AVF_ERR_FLUSHED_QUEUE";
	case AVF_ERR_INVALID_PUSH_PAGE_INDEX:
		return "AVF_ERR_INVALID_PUSH_PAGE_INDEX";
	case AVF_ERR_INVALID_IMM_DATA_SIZE:
		return "AVF_ERR_INVALID_IMM_DATA_SIZE";
	case AVF_ERR_TIMEOUT:
		return "AVF_ERR_TIMEOUT";
	case AVF_ERR_OPCODE_MISMATCH:
		return "AVF_ERR_OPCODE_MISMATCH";
	case AVF_ERR_CQP_COMPL_ERROR:
		return "AVF_ERR_CQP_COMPL_ERROR";
	case AVF_ERR_INVALID_VF_ID:
		return "AVF_ERR_INVALID_VF_ID";
	case AVF_ERR_INVALID_HMCFN_ID:
		return "AVF_ERR_INVALID_HMCFN_ID";
	case AVF_ERR_BACKING_PAGE_ERROR:
		return "AVF_ERR_BACKING_PAGE_ERROR";
	case AVF_ERR_NO_PBLCHUNKS_AVAILABLE:
		return "AVF_ERR_NO_PBLCHUNKS_AVAILABLE";
	case AVF_ERR_INVALID_PBLE_INDEX:
		return "AVF_ERR_INVALID_PBLE_INDEX";
	case AVF_ERR_INVALID_SD_INDEX:
		return "AVF_ERR_INVALID_SD_INDEX";
	case AVF_ERR_INVALID_PAGE_DESC_INDEX:
		return "AVF_ERR_INVALID_PAGE_DESC_INDEX";
	case AVF_ERR_INVALID_SD_TYPE:
		return "AVF_ERR_INVALID_SD_TYPE";
	case AVF_ERR_MEMCPY_FAILED:
		return "AVF_ERR_MEMCPY_FAILED";
	case AVF_ERR_INVALID_HMC_OBJ_INDEX:
		return "AVF_ERR_INVALID_HMC_OBJ_INDEX";
	case AVF_ERR_INVALID_HMC_OBJ_COUNT:
		return "AVF_ERR_INVALID_HMC_OBJ_COUNT";
	case AVF_ERR_INVALID_SRQ_ARM_LIMIT:
		return "AVF_ERR_INVALID_SRQ_ARM_LIMIT";
	case AVF_ERR_SRQ_ENABLED:
		return "AVF_ERR_SRQ_ENABLED";
	case AVF_ERR_ADMIN_QUEUE_ERROR:
		return "AVF_ERR_ADMIN_QUEUE_ERROR";
	case AVF_ERR_ADMIN_QUEUE_TIMEOUT:
		return "AVF_ERR_ADMIN_QUEUE_TIMEOUT";
	case AVF_ERR_BUF_TOO_SHORT:
		return "AVF_ERR_BUF_TOO_SHORT";
	case AVF_ERR_ADMIN_QUEUE_FULL:
		return "AVF_ERR_ADMIN_QUEUE_FULL";
	case AVF_ERR_ADMIN_QUEUE_NO_WORK:
		return "AVF_ERR_ADMIN_QUEUE_NO_WORK";
	case AVF_ERR_BAD_IWARP_CQE:
		return "AVF_ERR_BAD_IWARP_CQE";
	case AVF_ERR_NVM_BLANK_MODE:
		return "AVF_ERR_NVM_BLANK_MODE";
	case AVF_ERR_NOT_IMPLEMENTED:
		return "AVF_ERR_NOT_IMPLEMENTED";
	case AVF_ERR_PE_DOORBELL_NOT_ENABLED:
		return "AVF_ERR_PE_DOORBELL_NOT_ENABLED";
	case AVF_ERR_DIAG_TEST_FAILED:
		return "AVF_ERR_DIAG_TEST_FAILED";
	case AVF_ERR_NOT_READY:
		return "AVF_ERR_NOT_READY";
	case AVF_NOT_SUPPORTED:
		return "AVF_NOT_SUPPORTED";
	case AVF_ERR_FIRMWARE_API_VERSION:
		return "AVF_ERR_FIRMWARE_API_VERSION";
	case AVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR:
		return "AVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", stat_err);
	return hw->err_str;
}

/**
 * avf_debug_aq
 * @hw: debug mask related to admin queue
 * @mask: debug mask
 * @desc: pointer to admin queue descriptor
 * @buffer: pointer to command buffer
 * @buf_len: max length of buffer
 *
 * Dumps debug log about adminq command with descriptor contents.
 **/
void avf_debug_aq(struct avf_hw *hw, enum avf_debug_mask mask, void *desc,
		   void *buffer, u16 buf_len)
{
	struct avf_aq_desc *aq_desc = (struct avf_aq_desc *)desc;
	u8 *buf = (u8 *)buffer;
	u16 len;
	u16 i = 0;

	if ((!(mask & hw->debug_mask)) || (desc == NULL))
		return;

	len = LE16_TO_CPU(aq_desc->datalen);

	avf_debug(hw, mask,
		   "AQ CMD: opcode 0x%04X, flags 0x%04X, datalen 0x%04X, retval 0x%04X\n",
		   LE16_TO_CPU(aq_desc->opcode),
		   LE16_TO_CPU(aq_desc->flags),
		   LE16_TO_CPU(aq_desc->datalen),
		   LE16_TO_CPU(aq_desc->retval));
	avf_debug(hw, mask, "\tcookie (h,l) 0x%08X 0x%08X\n",
		   LE32_TO_CPU(aq_desc->cookie_high),
		   LE32_TO_CPU(aq_desc->cookie_low));
	avf_debug(hw, mask, "\tparam (0,1)  0x%08X 0x%08X\n",
		   LE32_TO_CPU(aq_desc->params.internal.param0),
		   LE32_TO_CPU(aq_desc->params.internal.param1));
	avf_debug(hw, mask, "\taddr (h,l)   0x%08X 0x%08X\n",
		   LE32_TO_CPU(aq_desc->params.external.addr_high),
		   LE32_TO_CPU(aq_desc->params.external.addr_low));

	if ((buffer != NULL) && (aq_desc->datalen != 0)) {
		avf_debug(hw, mask, "AQ CMD Buffer:\n");
		if (buf_len < len)
			len = buf_len;
		/* write the full 16-byte chunks */
		for (i = 0; i < (len - 16); i += 16)
			avf_debug(hw, mask,
				   "\t0x%04X  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				   i, buf[i], buf[i+1], buf[i+2], buf[i+3],
				   buf[i+4], buf[i+5], buf[i+6], buf[i+7],
				   buf[i+8], buf[i+9], buf[i+10], buf[i+11],
				   buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
		/* the most we could have left is 16 bytes, pad with zeros */
		if (i < len) {
			char d_buf[16];
			int j, i_sav;

			i_sav = i;
			memset(d_buf, 0, sizeof(d_buf));
			for (j = 0; i < len; j++, i++)
				d_buf[j] = buf[i];
			avf_debug(hw, mask,
				   "\t0x%04X  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				   i_sav, d_buf[0], d_buf[1], d_buf[2], d_buf[3],
				   d_buf[4], d_buf[5], d_buf[6], d_buf[7],
				   d_buf[8], d_buf[9], d_buf[10], d_buf[11],
				   d_buf[12], d_buf[13], d_buf[14], d_buf[15]);
		}
	}
}

/**
 * avf_check_asq_alive
 * @hw: pointer to the hw struct
 *
 * Returns true if Queue is enabled else false.
 **/
bool avf_check_asq_alive(struct avf_hw *hw)
{
	if (hw->aq.asq.len)
#ifdef INTEGRATED_VF
		if (avf_is_vf(hw))
			return !!(rd32(hw, hw->aq.asq.len) &
				AVF_ATQLEN1_ATQENABLE_MASK);
#else
		return !!(rd32(hw, hw->aq.asq.len) &
			AVF_ATQLEN1_ATQENABLE_MASK);
#endif /* INTEGRATED_VF */
	return false;
}

/**
 * avf_aq_queue_shutdown
 * @hw: pointer to the hw struct
 * @unloading: is the driver unloading itself
 *
 * Tell the Firmware that we're shutting down the AdminQ and whether
 * or not the driver is unloading as well.
 **/
enum avf_status_code avf_aq_queue_shutdown(struct avf_hw *hw,
					     bool unloading)
{
	struct avf_aq_desc desc;
	struct avf_aqc_queue_shutdown *cmd =
		(struct avf_aqc_queue_shutdown *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
					  avf_aqc_opc_queue_shutdown);

	if (unloading)
		cmd->driver_unloading = CPU_TO_LE32(AVF_AQ_DRIVER_UNLOADING);
	status = avf_asq_send_command(hw, &desc, NULL, 0, NULL);

	return status;
}

/**
 * avf_aq_get_set_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 * @set: set true to set the table, false to get the table
 *
 * Internal function to get or set RSS look up table
 **/
STATIC enum avf_status_code avf_aq_get_set_rss_lut(struct avf_hw *hw,
						     u16 vsi_id, bool pf_lut,
						     u8 *lut, u16 lut_size,
						     bool set)
{
	enum avf_status_code status;
	struct avf_aq_desc desc;
	struct avf_aqc_get_set_rss_lut *cmd_resp =
		   (struct avf_aqc_get_set_rss_lut *)&desc.params.raw;

	if (set)
		avf_fill_default_direct_cmd_desc(&desc,
						  avf_aqc_opc_set_rss_lut);
	else
		avf_fill_default_direct_cmd_desc(&desc,
						  avf_aqc_opc_get_rss_lut);

	/* Indirect command */
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			CPU_TO_LE16((u16)((vsi_id <<
					  AVF_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
					  AVF_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd_resp->vsi_id |= CPU_TO_LE16((u16)AVF_AQC_SET_RSS_LUT_VSI_VALID);

	if (pf_lut)
		cmd_resp->flags |= CPU_TO_LE16((u16)
					((AVF_AQC_SET_RSS_LUT_TABLE_TYPE_PF <<
					AVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					AVF_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	else
		cmd_resp->flags |= CPU_TO_LE16((u16)
					((AVF_AQC_SET_RSS_LUT_TABLE_TYPE_VSI <<
					AVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					AVF_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));

	status = avf_asq_send_command(hw, &desc, lut, lut_size, NULL);

	return status;
}

/**
 * avf_aq_get_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 *
 * get the RSS lookup table, PF or VSI type
 **/
enum avf_status_code avf_aq_get_rss_lut(struct avf_hw *hw, u16 vsi_id,
					  bool pf_lut, u8 *lut, u16 lut_size)
{
	return avf_aq_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size,
				       false);
}

/**
 * avf_aq_set_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 *
 * set the RSS lookup table, PF or VSI type
 **/
enum avf_status_code avf_aq_set_rss_lut(struct avf_hw *hw, u16 vsi_id,
					  bool pf_lut, u8 *lut, u16 lut_size)
{
	return avf_aq_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size, true);
}

/**
 * avf_aq_get_set_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 * @set: set true to set the key, false to get the key
 *
 * get the RSS key per VSI
 **/
STATIC enum avf_status_code avf_aq_get_set_rss_key(struct avf_hw *hw,
				      u16 vsi_id,
				      struct avf_aqc_get_set_rss_key_data *key,
				      bool set)
{
	enum avf_status_code status;
	struct avf_aq_desc desc;
	struct avf_aqc_get_set_rss_key *cmd_resp =
			(struct avf_aqc_get_set_rss_key *)&desc.params.raw;
	u16 key_size = sizeof(struct avf_aqc_get_set_rss_key_data);

	if (set)
		avf_fill_default_direct_cmd_desc(&desc,
						  avf_aqc_opc_set_rss_key);
	else
		avf_fill_default_direct_cmd_desc(&desc,
						  avf_aqc_opc_get_rss_key);

	/* Indirect command */
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			CPU_TO_LE16((u16)((vsi_id <<
					  AVF_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
					  AVF_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd_resp->vsi_id |= CPU_TO_LE16((u16)AVF_AQC_SET_RSS_KEY_VSI_VALID);

	status = avf_asq_send_command(hw, &desc, key, key_size, NULL);

	return status;
}

/**
 * avf_aq_get_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 *
 **/
enum avf_status_code avf_aq_get_rss_key(struct avf_hw *hw,
				      u16 vsi_id,
				      struct avf_aqc_get_set_rss_key_data *key)
{
	return avf_aq_get_set_rss_key(hw, vsi_id, key, false);
}

/**
 * avf_aq_set_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 *
 * set the RSS key per VSI
 **/
enum avf_status_code avf_aq_set_rss_key(struct avf_hw *hw,
				      u16 vsi_id,
				      struct avf_aqc_get_set_rss_key_data *key)
{
	return avf_aq_get_set_rss_key(hw, vsi_id, key, true);
}

/* The avf_ptype_lookup table is used to convert from the 8-bit ptype in the
 * hardware to a bit-field that can be used by SW to more easily determine the
 * packet type.
 *
 * Macros are used to shorten the table lines and make this table human
 * readable.
 *
 * We store the PTYPE in the top byte of the bit field - this is just so that
 * we can check that the table doesn't have a row missing, as the index into
 * the table should be the PTYPE.
 *
 * Typical work flow:
 *
 * IF NOT avf_ptype_lookup[ptype].known
 * THEN
 *      Packet is unknown
 * ELSE IF avf_ptype_lookup[ptype].outer_ip == AVF_RX_PTYPE_OUTER_IP
 *      Use the rest of the fields to look at the tunnels, inner protocols, etc
 * ELSE
 *      Use the enum avf_rx_l2_ptype to decode the packet type
 * ENDIF
 */

/* macro to make the table lines short */
#define AVF_PTT(PTYPE, OUTER_IP, OUTER_IP_VER, OUTER_FRAG, T, TE, TEF, I, PL)\
	{	PTYPE, \
		1, \
		AVF_RX_PTYPE_OUTER_##OUTER_IP, \
		AVF_RX_PTYPE_OUTER_##OUTER_IP_VER, \
		AVF_RX_PTYPE_##OUTER_FRAG, \
		AVF_RX_PTYPE_TUNNEL_##T, \
		AVF_RX_PTYPE_TUNNEL_END_##TE, \
		AVF_RX_PTYPE_##TEF, \
		AVF_RX_PTYPE_INNER_PROT_##I, \
		AVF_RX_PTYPE_PAYLOAD_LAYER_##PL }

#define AVF_PTT_UNUSED_ENTRY(PTYPE) \
		{ PTYPE, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

/* shorter macros makes the table fit but are terse */
#define AVF_RX_PTYPE_NOF		AVF_RX_PTYPE_NOT_FRAG
#define AVF_RX_PTYPE_FRG		AVF_RX_PTYPE_FRAG
#define AVF_RX_PTYPE_INNER_PROT_TS	AVF_RX_PTYPE_INNER_PROT_TIMESYNC

/* Lookup table mapping the HW PTYPE to the bit field for decoding */
struct avf_rx_ptype_decoded avf_ptype_lookup[] = {
	/* L2 Packet types */
	AVF_PTT_UNUSED_ENTRY(0),
	AVF_PTT(1,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	AVF_PTT(2,  L2, NONE, NOF, NONE, NONE, NOF, TS,   PAY2),
	AVF_PTT(3,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	AVF_PTT_UNUSED_ENTRY(4),
	AVF_PTT_UNUSED_ENTRY(5),
	AVF_PTT(6,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	AVF_PTT(7,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	AVF_PTT_UNUSED_ENTRY(8),
	AVF_PTT_UNUSED_ENTRY(9),
	AVF_PTT(10, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	AVF_PTT(11, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	AVF_PTT(12, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(13, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(14, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(15, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(16, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(17, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(18, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(19, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(20, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(21, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),

	/* Non Tunneled IPv4 */
	AVF_PTT(22, IP, IPV4, FRG, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(23, IP, IPV4, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(24, IP, IPV4, NOF, NONE, NONE, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(25),
	AVF_PTT(26, IP, IPV4, NOF, NONE, NONE, NOF, TCP,  PAY4),
	AVF_PTT(27, IP, IPV4, NOF, NONE, NONE, NOF, SCTP, PAY4),
	AVF_PTT(28, IP, IPV4, NOF, NONE, NONE, NOF, ICMP, PAY4),

	/* IPv4 --> IPv4 */
	AVF_PTT(29, IP, IPV4, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	AVF_PTT(30, IP, IPV4, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	AVF_PTT(31, IP, IPV4, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(32),
	AVF_PTT(33, IP, IPV4, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(34, IP, IPV4, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(35, IP, IPV4, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> IPv6 */
	AVF_PTT(36, IP, IPV4, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	AVF_PTT(37, IP, IPV4, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	AVF_PTT(38, IP, IPV4, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(39),
	AVF_PTT(40, IP, IPV4, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(41, IP, IPV4, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(42, IP, IPV4, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT */
	AVF_PTT(43, IP, IPV4, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	/* IPv4 --> GRE/NAT --> IPv4 */
	AVF_PTT(44, IP, IPV4, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	AVF_PTT(45, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	AVF_PTT(46, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(47),
	AVF_PTT(48, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(49, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(50, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> IPv6 */
	AVF_PTT(51, IP, IPV4, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	AVF_PTT(52, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	AVF_PTT(53, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(54),
	AVF_PTT(55, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(56, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(57, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> MAC */
	AVF_PTT(58, IP, IPV4, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	/* IPv4 --> GRE/NAT --> MAC --> IPv4 */
	AVF_PTT(59, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	AVF_PTT(60, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	AVF_PTT(61, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(62),
	AVF_PTT(63, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(64, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(65, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT -> MAC --> IPv6 */
	AVF_PTT(66, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	AVF_PTT(67, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	AVF_PTT(68, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(69),
	AVF_PTT(70, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(71, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(72, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> MAC/VLAN */
	AVF_PTT(73, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	/* IPv4 ---> GRE/NAT -> MAC/VLAN --> IPv4 */
	AVF_PTT(74, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	AVF_PTT(75, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	AVF_PTT(76, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(77),
	AVF_PTT(78, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(79, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(80, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	/* IPv4 -> GRE/NAT -> MAC/VLAN --> IPv6 */
	AVF_PTT(81, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	AVF_PTT(82, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	AVF_PTT(83, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(84),
	AVF_PTT(85, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(86, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(87, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	/* Non Tunneled IPv6 */
	AVF_PTT(88, IP, IPV6, FRG, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(89, IP, IPV6, NOF, NONE, NONE, NOF, NONE, PAY3),
	AVF_PTT(90, IP, IPV6, NOF, NONE, NONE, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(91),
	AVF_PTT(92, IP, IPV6, NOF, NONE, NONE, NOF, TCP,  PAY4),
	AVF_PTT(93, IP, IPV6, NOF, NONE, NONE, NOF, SCTP, PAY4),
	AVF_PTT(94, IP, IPV6, NOF, NONE, NONE, NOF, ICMP, PAY4),

	/* IPv6 --> IPv4 */
	AVF_PTT(95,  IP, IPV6, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	AVF_PTT(96,  IP, IPV6, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	AVF_PTT(97,  IP, IPV6, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(98),
	AVF_PTT(99,  IP, IPV6, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(100, IP, IPV6, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(101, IP, IPV6, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> IPv6 */
	AVF_PTT(102, IP, IPV6, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	AVF_PTT(103, IP, IPV6, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	AVF_PTT(104, IP, IPV6, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(105),
	AVF_PTT(106, IP, IPV6, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(107, IP, IPV6, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(108, IP, IPV6, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT */
	AVF_PTT(109, IP, IPV6, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> IPv4 */
	AVF_PTT(110, IP, IPV6, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	AVF_PTT(111, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	AVF_PTT(112, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(113),
	AVF_PTT(114, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(115, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(116, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> IPv6 */
	AVF_PTT(117, IP, IPV6, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	AVF_PTT(118, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	AVF_PTT(119, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(120),
	AVF_PTT(121, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(122, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(123, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC */
	AVF_PTT(124, IP, IPV6, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> MAC -> IPv4 */
	AVF_PTT(125, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	AVF_PTT(126, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	AVF_PTT(127, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(128),
	AVF_PTT(129, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(130, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(131, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC -> IPv6 */
	AVF_PTT(132, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	AVF_PTT(133, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	AVF_PTT(134, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(135),
	AVF_PTT(136, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(137, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(138, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC/VLAN */
	AVF_PTT(139, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> MAC/VLAN --> IPv4 */
	AVF_PTT(140, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	AVF_PTT(141, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	AVF_PTT(142, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(143),
	AVF_PTT(144, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	AVF_PTT(145, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	AVF_PTT(146, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC/VLAN --> IPv6 */
	AVF_PTT(147, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	AVF_PTT(148, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	AVF_PTT(149, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	AVF_PTT_UNUSED_ENTRY(150),
	AVF_PTT(151, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	AVF_PTT(152, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	AVF_PTT(153, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	/* unused entries */
	AVF_PTT_UNUSED_ENTRY(154),
	AVF_PTT_UNUSED_ENTRY(155),
	AVF_PTT_UNUSED_ENTRY(156),
	AVF_PTT_UNUSED_ENTRY(157),
	AVF_PTT_UNUSED_ENTRY(158),
	AVF_PTT_UNUSED_ENTRY(159),

	AVF_PTT_UNUSED_ENTRY(160),
	AVF_PTT_UNUSED_ENTRY(161),
	AVF_PTT_UNUSED_ENTRY(162),
	AVF_PTT_UNUSED_ENTRY(163),
	AVF_PTT_UNUSED_ENTRY(164),
	AVF_PTT_UNUSED_ENTRY(165),
	AVF_PTT_UNUSED_ENTRY(166),
	AVF_PTT_UNUSED_ENTRY(167),
	AVF_PTT_UNUSED_ENTRY(168),
	AVF_PTT_UNUSED_ENTRY(169),

	AVF_PTT_UNUSED_ENTRY(170),
	AVF_PTT_UNUSED_ENTRY(171),
	AVF_PTT_UNUSED_ENTRY(172),
	AVF_PTT_UNUSED_ENTRY(173),
	AVF_PTT_UNUSED_ENTRY(174),
	AVF_PTT_UNUSED_ENTRY(175),
	AVF_PTT_UNUSED_ENTRY(176),
	AVF_PTT_UNUSED_ENTRY(177),
	AVF_PTT_UNUSED_ENTRY(178),
	AVF_PTT_UNUSED_ENTRY(179),

	AVF_PTT_UNUSED_ENTRY(180),
	AVF_PTT_UNUSED_ENTRY(181),
	AVF_PTT_UNUSED_ENTRY(182),
	AVF_PTT_UNUSED_ENTRY(183),
	AVF_PTT_UNUSED_ENTRY(184),
	AVF_PTT_UNUSED_ENTRY(185),
	AVF_PTT_UNUSED_ENTRY(186),
	AVF_PTT_UNUSED_ENTRY(187),
	AVF_PTT_UNUSED_ENTRY(188),
	AVF_PTT_UNUSED_ENTRY(189),

	AVF_PTT_UNUSED_ENTRY(190),
	AVF_PTT_UNUSED_ENTRY(191),
	AVF_PTT_UNUSED_ENTRY(192),
	AVF_PTT_UNUSED_ENTRY(193),
	AVF_PTT_UNUSED_ENTRY(194),
	AVF_PTT_UNUSED_ENTRY(195),
	AVF_PTT_UNUSED_ENTRY(196),
	AVF_PTT_UNUSED_ENTRY(197),
	AVF_PTT_UNUSED_ENTRY(198),
	AVF_PTT_UNUSED_ENTRY(199),

	AVF_PTT_UNUSED_ENTRY(200),
	AVF_PTT_UNUSED_ENTRY(201),
	AVF_PTT_UNUSED_ENTRY(202),
	AVF_PTT_UNUSED_ENTRY(203),
	AVF_PTT_UNUSED_ENTRY(204),
	AVF_PTT_UNUSED_ENTRY(205),
	AVF_PTT_UNUSED_ENTRY(206),
	AVF_PTT_UNUSED_ENTRY(207),
	AVF_PTT_UNUSED_ENTRY(208),
	AVF_PTT_UNUSED_ENTRY(209),

	AVF_PTT_UNUSED_ENTRY(210),
	AVF_PTT_UNUSED_ENTRY(211),
	AVF_PTT_UNUSED_ENTRY(212),
	AVF_PTT_UNUSED_ENTRY(213),
	AVF_PTT_UNUSED_ENTRY(214),
	AVF_PTT_UNUSED_ENTRY(215),
	AVF_PTT_UNUSED_ENTRY(216),
	AVF_PTT_UNUSED_ENTRY(217),
	AVF_PTT_UNUSED_ENTRY(218),
	AVF_PTT_UNUSED_ENTRY(219),

	AVF_PTT_UNUSED_ENTRY(220),
	AVF_PTT_UNUSED_ENTRY(221),
	AVF_PTT_UNUSED_ENTRY(222),
	AVF_PTT_UNUSED_ENTRY(223),
	AVF_PTT_UNUSED_ENTRY(224),
	AVF_PTT_UNUSED_ENTRY(225),
	AVF_PTT_UNUSED_ENTRY(226),
	AVF_PTT_UNUSED_ENTRY(227),
	AVF_PTT_UNUSED_ENTRY(228),
	AVF_PTT_UNUSED_ENTRY(229),

	AVF_PTT_UNUSED_ENTRY(230),
	AVF_PTT_UNUSED_ENTRY(231),
	AVF_PTT_UNUSED_ENTRY(232),
	AVF_PTT_UNUSED_ENTRY(233),
	AVF_PTT_UNUSED_ENTRY(234),
	AVF_PTT_UNUSED_ENTRY(235),
	AVF_PTT_UNUSED_ENTRY(236),
	AVF_PTT_UNUSED_ENTRY(237),
	AVF_PTT_UNUSED_ENTRY(238),
	AVF_PTT_UNUSED_ENTRY(239),

	AVF_PTT_UNUSED_ENTRY(240),
	AVF_PTT_UNUSED_ENTRY(241),
	AVF_PTT_UNUSED_ENTRY(242),
	AVF_PTT_UNUSED_ENTRY(243),
	AVF_PTT_UNUSED_ENTRY(244),
	AVF_PTT_UNUSED_ENTRY(245),
	AVF_PTT_UNUSED_ENTRY(246),
	AVF_PTT_UNUSED_ENTRY(247),
	AVF_PTT_UNUSED_ENTRY(248),
	AVF_PTT_UNUSED_ENTRY(249),

	AVF_PTT_UNUSED_ENTRY(250),
	AVF_PTT_UNUSED_ENTRY(251),
	AVF_PTT_UNUSED_ENTRY(252),
	AVF_PTT_UNUSED_ENTRY(253),
	AVF_PTT_UNUSED_ENTRY(254),
	AVF_PTT_UNUSED_ENTRY(255)
};


/**
 * avf_validate_mac_addr - Validate unicast MAC address
 * @mac_addr: pointer to MAC address
 *
 * Tests a MAC address to ensure it is a valid Individual Address
 **/
enum avf_status_code avf_validate_mac_addr(u8 *mac_addr)
{
	enum avf_status_code status = AVF_SUCCESS;

	DEBUGFUNC("avf_validate_mac_addr");

	/* Broadcast addresses ARE multicast addresses
	 * Make sure it is not a multicast address
	 * Reject the zero address
	 */
	if (AVF_IS_MULTICAST(mac_addr) ||
	    (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
	      mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0))
		status = AVF_ERR_INVALID_MAC_ADDR;

	return status;
}

/**
 * avf_aq_rx_ctl_read_register - use FW to read from an Rx control register
 * @hw: pointer to the hw struct
 * @reg_addr: register address
 * @reg_val: ptr to register value
 * @cmd_details: pointer to command details structure or NULL
 *
 * Use the firmware to read the Rx control register,
 * especially useful if the Rx unit is under heavy pressure
 **/
enum avf_status_code avf_aq_rx_ctl_read_register(struct avf_hw *hw,
				u32 reg_addr, u32 *reg_val,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_rx_ctl_reg_read_write *cmd_resp =
		(struct avf_aqc_rx_ctl_reg_read_write *)&desc.params.raw;
	enum avf_status_code status;

	if (reg_val == NULL)
		return AVF_ERR_PARAM;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_rx_ctl_reg_read);

	cmd_resp->address = CPU_TO_LE32(reg_addr);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (status == AVF_SUCCESS)
		*reg_val = LE32_TO_CPU(cmd_resp->value);

	return status;
}

/**
 * avf_read_rx_ctl - read from an Rx control register
 * @hw: pointer to the hw struct
 * @reg_addr: register address
 **/
u32 avf_read_rx_ctl(struct avf_hw *hw, u32 reg_addr)
{
	enum avf_status_code status = AVF_SUCCESS;
	bool use_register;
	int retry = 5;
	u32 val = 0;

	use_register = (((hw->aq.api_maj_ver == 1) &&
			(hw->aq.api_min_ver < 5)) ||
			(hw->mac.type == AVF_MAC_X722));
	if (!use_register) {
do_retry:
		status = avf_aq_rx_ctl_read_register(hw, reg_addr, &val, NULL);
		if (hw->aq.asq_last_status == AVF_AQ_RC_EAGAIN && retry) {
			avf_msec_delay(1);
			retry--;
			goto do_retry;
		}
	}

	/* if the AQ access failed, try the old-fashioned way */
	if (status || use_register)
		val = rd32(hw, reg_addr);

	return val;
}

/**
 * avf_aq_rx_ctl_write_register
 * @hw: pointer to the hw struct
 * @reg_addr: register address
 * @reg_val: register value
 * @cmd_details: pointer to command details structure or NULL
 *
 * Use the firmware to write to an Rx control register,
 * especially useful if the Rx unit is under heavy pressure
 **/
enum avf_status_code avf_aq_rx_ctl_write_register(struct avf_hw *hw,
				u32 reg_addr, u32 reg_val,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_rx_ctl_reg_read_write *cmd =
		(struct avf_aqc_rx_ctl_reg_read_write *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_rx_ctl_reg_write);

	cmd->address = CPU_TO_LE32(reg_addr);
	cmd->value = CPU_TO_LE32(reg_val);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

/**
 * avf_write_rx_ctl - write to an Rx control register
 * @hw: pointer to the hw struct
 * @reg_addr: register address
 * @reg_val: register value
 **/
void avf_write_rx_ctl(struct avf_hw *hw, u32 reg_addr, u32 reg_val)
{
	enum avf_status_code status = AVF_SUCCESS;
	bool use_register;
	int retry = 5;

	use_register = (((hw->aq.api_maj_ver == 1) &&
			(hw->aq.api_min_ver < 5)) ||
			(hw->mac.type == AVF_MAC_X722));
	if (!use_register) {
do_retry:
		status = avf_aq_rx_ctl_write_register(hw, reg_addr,
						       reg_val, NULL);
		if (hw->aq.asq_last_status == AVF_AQ_RC_EAGAIN && retry) {
			avf_msec_delay(1);
			retry--;
			goto do_retry;
		}
	}

	/* if the AQ access failed, try the old-fashioned way */
	if (status || use_register)
		wr32(hw, reg_addr, reg_val);
}

/**
 * avf_aq_set_phy_register
 * @hw: pointer to the hw struct
 * @phy_select: select which phy should be accessed
 * @dev_addr: PHY device address
 * @reg_addr: PHY register address
 * @reg_val: new register value
 * @cmd_details: pointer to command details structure or NULL
 *
 * Write the external PHY register.
 **/
enum avf_status_code avf_aq_set_phy_register(struct avf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 reg_val,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_phy_register_access *cmd =
		(struct avf_aqc_phy_register_access *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
					  avf_aqc_opc_set_phy_register);

	cmd->phy_interface = phy_select;
	cmd->dev_addres = dev_addr;
	cmd->reg_address = CPU_TO_LE32(reg_addr);
	cmd->reg_value = CPU_TO_LE32(reg_val);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

/**
 * avf_aq_get_phy_register
 * @hw: pointer to the hw struct
 * @phy_select: select which phy should be accessed
 * @dev_addr: PHY device address
 * @reg_addr: PHY register address
 * @reg_val: read register value
 * @cmd_details: pointer to command details structure or NULL
 *
 * Read the external PHY register.
 **/
enum avf_status_code avf_aq_get_phy_register(struct avf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 *reg_val,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_phy_register_access *cmd =
		(struct avf_aqc_phy_register_access *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
					  avf_aqc_opc_get_phy_register);

	cmd->phy_interface = phy_select;
	cmd->dev_addres = dev_addr;
	cmd->reg_address = CPU_TO_LE32(reg_addr);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);
	if (!status)
		*reg_val = LE32_TO_CPU(cmd->reg_value);

	return status;
}


/**
 * avf_aq_send_msg_to_pf
 * @hw: pointer to the hardware structure
 * @v_opcode: opcodes for VF-PF communication
 * @v_retval: return error code
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 * @cmd_details: pointer to command details
 *
 * Send message to PF driver using admin queue. By default, this message
 * is sent asynchronously, i.e. avf_asq_send_command() does not wait for
 * completion before returning.
 **/
enum avf_status_code avf_aq_send_msg_to_pf(struct avf_hw *hw,
				enum virtchnl_ops v_opcode,
				enum avf_status_code v_retval,
				u8 *msg, u16 msglen,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_asq_cmd_details details;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_send_msg_to_pf);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_SI);
	desc.cookie_high = CPU_TO_LE32(v_opcode);
	desc.cookie_low = CPU_TO_LE32(v_retval);
	if (msglen) {
		desc.flags |= CPU_TO_LE16((u16)(AVF_AQ_FLAG_BUF
						| AVF_AQ_FLAG_RD));
		if (msglen > AVF_AQ_LARGE_BUF)
			desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_LB);
		desc.datalen = CPU_TO_LE16(msglen);
	}
	if (!cmd_details) {
		avf_memset(&details, 0, sizeof(details), AVF_NONDMA_MEM);
		details.async = true;
		cmd_details = &details;
	}
	status = avf_asq_send_command(hw, (struct avf_aq_desc *)&desc, msg,
				       msglen, cmd_details);
	return status;
}

/**
 * avf_parse_hw_config
 * @hw: pointer to the hardware structure
 * @msg: pointer to the virtual channel VF resource structure
 *
 * Given a VF resource message from the PF, populate the hw struct
 * with appropriate information.
 **/
void avf_parse_hw_config(struct avf_hw *hw,
			     struct virtchnl_vf_resource *msg)
{
	struct virtchnl_vsi_resource *vsi_res;
	int i;

	vsi_res = &msg->vsi_res[0];

	hw->dev_caps.num_vsis = msg->num_vsis;
	hw->dev_caps.num_rx_qp = msg->num_queue_pairs;
	hw->dev_caps.num_tx_qp = msg->num_queue_pairs;
	hw->dev_caps.num_msix_vectors_vf = msg->max_vectors;
	hw->dev_caps.dcb = msg->vf_cap_flags &
			   VIRTCHNL_VF_OFFLOAD_L2;
	hw->dev_caps.iwarp = (msg->vf_cap_flags &
			      VIRTCHNL_VF_OFFLOAD_IWARP) ? 1 : 0;
	for (i = 0; i < msg->num_vsis; i++) {
		if (vsi_res->vsi_type == VIRTCHNL_VSI_SRIOV) {
			avf_memcpy(hw->mac.perm_addr,
				    vsi_res->default_mac_addr,
				    ETH_ALEN,
				    AVF_NONDMA_TO_NONDMA);
			avf_memcpy(hw->mac.addr, vsi_res->default_mac_addr,
				    ETH_ALEN,
				    AVF_NONDMA_TO_NONDMA);
		}
		vsi_res++;
	}
}

/**
 * avf_reset
 * @hw: pointer to the hardware structure
 *
 * Send a VF_RESET message to the PF. Does not wait for response from PF
 * as none will be forthcoming. Immediately after calling this function,
 * the admin queue should be shut down and (optionally) reinitialized.
 **/
enum avf_status_code avf_reset(struct avf_hw *hw)
{
	return avf_aq_send_msg_to_pf(hw, VIRTCHNL_OP_RESET_VF,
				      AVF_SUCCESS, NULL, 0, NULL);
}

/**
 * avf_aq_set_arp_proxy_config
 * @hw: pointer to the HW structure
 * @proxy_config: pointer to proxy config command table struct
 * @cmd_details: pointer to command details
 *
 * Set ARP offload parameters from pre-populated
 * avf_aqc_arp_proxy_data struct
 **/
enum avf_status_code avf_aq_set_arp_proxy_config(struct avf_hw *hw,
				struct avf_aqc_arp_proxy_data *proxy_config,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	enum avf_status_code status;

	if (!proxy_config)
		return AVF_ERR_PARAM;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_set_proxy_config);

	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_RD);
	desc.params.external.addr_high =
				  CPU_TO_LE32(AVF_HI_DWORD((u64)proxy_config));
	desc.params.external.addr_low =
				  CPU_TO_LE32(AVF_LO_DWORD((u64)proxy_config));
	desc.datalen = CPU_TO_LE16(sizeof(struct avf_aqc_arp_proxy_data));

	status = avf_asq_send_command(hw, &desc, proxy_config,
				       sizeof(struct avf_aqc_arp_proxy_data),
				       cmd_details);

	return status;
}

/**
 * avf_aq_opc_set_ns_proxy_table_entry
 * @hw: pointer to the HW structure
 * @ns_proxy_table_entry: pointer to NS table entry command struct
 * @cmd_details: pointer to command details
 *
 * Set IPv6 Neighbor Solicitation (NS) protocol offload parameters
 * from pre-populated avf_aqc_ns_proxy_data struct
 **/
enum avf_status_code avf_aq_set_ns_proxy_table_entry(struct avf_hw *hw,
			struct avf_aqc_ns_proxy_data *ns_proxy_table_entry,
			struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	enum avf_status_code status;

	if (!ns_proxy_table_entry)
		return AVF_ERR_PARAM;

	avf_fill_default_direct_cmd_desc(&desc,
				avf_aqc_opc_set_ns_proxy_table_entry);

	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_RD);
	desc.params.external.addr_high =
		CPU_TO_LE32(AVF_HI_DWORD((u64)ns_proxy_table_entry));
	desc.params.external.addr_low =
		CPU_TO_LE32(AVF_LO_DWORD((u64)ns_proxy_table_entry));
	desc.datalen = CPU_TO_LE16(sizeof(struct avf_aqc_ns_proxy_data));

	status = avf_asq_send_command(hw, &desc, ns_proxy_table_entry,
				       sizeof(struct avf_aqc_ns_proxy_data),
				       cmd_details);

	return status;
}

/**
 * avf_aq_set_clear_wol_filter
 * @hw: pointer to the hw struct
 * @filter_index: index of filter to modify (0-7)
 * @filter: buffer containing filter to be set
 * @set_filter: true to set filter, false to clear filter
 * @no_wol_tco: if true, pass through packets cannot cause wake-up
 *		if false, pass through packets may cause wake-up
 * @filter_valid: true if filter action is valid
 * @no_wol_tco_valid: true if no WoL in TCO traffic action valid
 * @cmd_details: pointer to command details structure or NULL
 *
 * Set or clear WoL filter for port attached to the PF
 **/
enum avf_status_code avf_aq_set_clear_wol_filter(struct avf_hw *hw,
				u8 filter_index,
				struct avf_aqc_set_wol_filter_data *filter,
				bool set_filter, bool no_wol_tco,
				bool filter_valid, bool no_wol_tco_valid,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_set_wol_filter *cmd =
		(struct avf_aqc_set_wol_filter *)&desc.params.raw;
	enum avf_status_code status;
	u16 cmd_flags = 0;
	u16 valid_flags = 0;
	u16 buff_len = 0;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_set_wol_filter);

	if (filter_index >= AVF_AQC_MAX_NUM_WOL_FILTERS)
		return  AVF_ERR_PARAM;
	cmd->filter_index = CPU_TO_LE16(filter_index);

	if (set_filter) {
		if (!filter)
			return  AVF_ERR_PARAM;

		cmd_flags |= AVF_AQC_SET_WOL_FILTER;
		cmd_flags |= AVF_AQC_SET_WOL_FILTER_WOL_PRESERVE_ON_PFR;
	}

	if (no_wol_tco)
		cmd_flags |= AVF_AQC_SET_WOL_FILTER_NO_TCO_WOL;
	cmd->cmd_flags = CPU_TO_LE16(cmd_flags);

	if (filter_valid)
		valid_flags |= AVF_AQC_SET_WOL_FILTER_ACTION_VALID;
	if (no_wol_tco_valid)
		valid_flags |= AVF_AQC_SET_WOL_FILTER_NO_TCO_ACTION_VALID;
	cmd->valid_flags = CPU_TO_LE16(valid_flags);

	buff_len = sizeof(*filter);
	desc.datalen = CPU_TO_LE16(buff_len);

	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_RD);

	cmd->address_high = CPU_TO_LE32(AVF_HI_DWORD((u64)filter));
	cmd->address_low = CPU_TO_LE32(AVF_LO_DWORD((u64)filter));

	status = avf_asq_send_command(hw, &desc, filter,
				       buff_len, cmd_details);

	return status;
}

/**
 * avf_aq_get_wake_event_reason
 * @hw: pointer to the hw struct
 * @wake_reason: return value, index of matching filter
 * @cmd_details: pointer to command details structure or NULL
 *
 * Get information for the reason of a Wake Up event
 **/
enum avf_status_code avf_aq_get_wake_event_reason(struct avf_hw *hw,
				u16 *wake_reason,
				struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_get_wake_reason_completion *resp =
		(struct avf_aqc_get_wake_reason_completion *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc, avf_aqc_opc_get_wake_reason);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (status == AVF_SUCCESS)
		*wake_reason = LE16_TO_CPU(resp->wake_reason);

	return status;
}

/**
* avf_aq_clear_all_wol_filters
* @hw: pointer to the hw struct
* @cmd_details: pointer to command details structure or NULL
*
* Get information for the reason of a Wake Up event
**/
enum avf_status_code avf_aq_clear_all_wol_filters(struct avf_hw *hw,
	struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
					  avf_aqc_opc_clear_all_wol_filters);

	status = avf_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

/**
 * avf_aq_write_ddp - Write dynamic device personalization (ddp)
 * @hw: pointer to the hw struct
 * @buff: command buffer (size in bytes = buff_size)
 * @buff_size: buffer size in bytes
 * @track_id: package tracking id
 * @error_offset: returns error offset
 * @error_info: returns error information
 * @cmd_details: pointer to command details structure or NULL
 **/
enum
avf_status_code avf_aq_write_ddp(struct avf_hw *hw, void *buff,
				   u16 buff_size, u32 track_id,
				   u32 *error_offset, u32 *error_info,
				   struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_write_personalization_profile *cmd =
		(struct avf_aqc_write_personalization_profile *)
		&desc.params.raw;
	struct avf_aqc_write_ddp_resp *resp;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
				  avf_aqc_opc_write_personalization_profile);

	desc.flags |= CPU_TO_LE16(AVF_AQ_FLAG_BUF | AVF_AQ_FLAG_RD);
	if (buff_size > AVF_AQ_LARGE_BUF)
		desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_LB);

	desc.datalen = CPU_TO_LE16(buff_size);

	cmd->profile_track_id = CPU_TO_LE32(track_id);

	status = avf_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	if (!status) {
		resp = (struct avf_aqc_write_ddp_resp *)&desc.params.raw;
		if (error_offset)
			*error_offset = LE32_TO_CPU(resp->error_offset);
		if (error_info)
			*error_info = LE32_TO_CPU(resp->error_info);
	}

	return status;
}

/**
 * avf_aq_get_ddp_list - Read dynamic device personalization (ddp)
 * @hw: pointer to the hw struct
 * @buff: command buffer (size in bytes = buff_size)
 * @buff_size: buffer size in bytes
 * @flags: AdminQ command flags
 * @cmd_details: pointer to command details structure or NULL
 **/
enum
avf_status_code avf_aq_get_ddp_list(struct avf_hw *hw, void *buff,
				      u16 buff_size, u8 flags,
				      struct avf_asq_cmd_details *cmd_details)
{
	struct avf_aq_desc desc;
	struct avf_aqc_get_applied_profiles *cmd =
		(struct avf_aqc_get_applied_profiles *)&desc.params.raw;
	enum avf_status_code status;

	avf_fill_default_direct_cmd_desc(&desc,
			  avf_aqc_opc_get_personalization_profile_list);

	desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_BUF);
	if (buff_size > AVF_AQ_LARGE_BUF)
		desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_LB);
	desc.datalen = CPU_TO_LE16(buff_size);

	cmd->flags = flags;

	status = avf_asq_send_command(hw, &desc, buff, buff_size, cmd_details);

	return status;
}

/**
 * avf_find_segment_in_package
 * @segment_type: the segment type to search for (i.e., SEGMENT_TYPE_AVF)
 * @pkg_hdr: pointer to the package header to be searched
 *
 * This function searches a package file for a particular segment type. On
 * success it returns a pointer to the segment header, otherwise it will
 * return NULL.
 **/
struct avf_generic_seg_header *
avf_find_segment_in_package(u32 segment_type,
			     struct avf_package_header *pkg_hdr)
{
	struct avf_generic_seg_header *segment;
	u32 i;

	/* Search all package segments for the requested segment type */
	for (i = 0; i < pkg_hdr->segment_count; i++) {
		segment =
			(struct avf_generic_seg_header *)((u8 *)pkg_hdr +
			 pkg_hdr->segment_offset[i]);

		if (segment->type == segment_type)
			return segment;
	}

	return NULL;
}

/* Get section table in profile */
#define AVF_SECTION_TABLE(profile, sec_tbl)				\
	do {								\
		struct avf_profile_segment *p = (profile);		\
		u32 count;						\
		u32 *nvm;						\
		count = p->device_table_count;				\
		nvm = (u32 *)&p->device_table[count];			\
		sec_tbl = (struct avf_section_table *)&nvm[nvm[0] + 1]; \
	} while (0)

/* Get section header in profile */
#define AVF_SECTION_HEADER(profile, offset)				\
	(struct avf_profile_section_header *)((u8 *)(profile) + (offset))

/**
 * avf_find_section_in_profile
 * @section_type: the section type to search for (i.e., SECTION_TYPE_NOTE)
 * @profile: pointer to the avf segment header to be searched
 *
 * This function searches avf segment for a particular section type. On
 * success it returns a pointer to the section header, otherwise it will
 * return NULL.
 **/
struct avf_profile_section_header *
avf_find_section_in_profile(u32 section_type,
			     struct avf_profile_segment *profile)
{
	struct avf_profile_section_header *sec;
	struct avf_section_table *sec_tbl;
	u32 sec_off;
	u32 i;

	if (profile->header.type != SEGMENT_TYPE_AVF)
		return NULL;

	AVF_SECTION_TABLE(profile, sec_tbl);

	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = AVF_SECTION_HEADER(profile, sec_off);
		if (sec->section.type == section_type)
			return sec;
	}

	return NULL;
}

/**
 * avf_ddp_exec_aq_section - Execute generic AQ for DDP
 * @hw: pointer to the hw struct
 * @aq: command buffer containing all data to execute AQ
 **/
STATIC enum
avf_status_code avf_ddp_exec_aq_section(struct avf_hw *hw,
					  struct avf_profile_aq_section *aq)
{
	enum avf_status_code status;
	struct avf_aq_desc desc;
	u8 *msg = NULL;
	u16 msglen;

	avf_fill_default_direct_cmd_desc(&desc, aq->opcode);
	desc.flags |= CPU_TO_LE16(aq->flags);
	avf_memcpy(desc.params.raw, aq->param, sizeof(desc.params.raw),
		    AVF_NONDMA_TO_NONDMA);

	msglen = aq->datalen;
	if (msglen) {
		desc.flags |= CPU_TO_LE16((u16)(AVF_AQ_FLAG_BUF |
						AVF_AQ_FLAG_RD));
		if (msglen > AVF_AQ_LARGE_BUF)
			desc.flags |= CPU_TO_LE16((u16)AVF_AQ_FLAG_LB);
		desc.datalen = CPU_TO_LE16(msglen);
		msg = &aq->data[0];
	}

	status = avf_asq_send_command(hw, &desc, msg, msglen, NULL);

	if (status != AVF_SUCCESS) {
		avf_debug(hw, AVF_DEBUG_PACKAGE,
			   "unable to exec DDP AQ opcode %u, error %d\n",
			   aq->opcode, status);
		return status;
	}

	/* copy returned desc to aq_buf */
	avf_memcpy(aq->param, desc.params.raw, sizeof(desc.params.raw),
		    AVF_NONDMA_TO_NONDMA);

	return AVF_SUCCESS;
}

/**
 * avf_validate_profile
 * @hw: pointer to the hardware structure
 * @profile: pointer to the profile segment of the package to be validated
 * @track_id: package tracking id
 * @rollback: flag if the profile is for rollback.
 *
 * Validates supported devices and profile's sections.
 */
STATIC enum avf_status_code
avf_validate_profile(struct avf_hw *hw, struct avf_profile_segment *profile,
		      u32 track_id, bool rollback)
{
	struct avf_profile_section_header *sec = NULL;
	enum avf_status_code status = AVF_SUCCESS;
	struct avf_section_table *sec_tbl;
	u32 vendor_dev_id;
	u32 dev_cnt;
	u32 sec_off;
	u32 i;

	if (track_id == AVF_DDP_TRACKID_INVALID) {
		avf_debug(hw, AVF_DEBUG_PACKAGE, "Invalid track_id\n");
		return AVF_NOT_SUPPORTED;
	}

	dev_cnt = profile->device_table_count;
	for (i = 0; i < dev_cnt; i++) {
		vendor_dev_id = profile->device_table[i].vendor_dev_id;
		if ((vendor_dev_id >> 16) == AVF_INTEL_VENDOR_ID &&
		    hw->device_id == (vendor_dev_id & 0xFFFF))
			break;
	}
	if (dev_cnt && (i == dev_cnt)) {
		avf_debug(hw, AVF_DEBUG_PACKAGE,
			   "Device doesn't support DDP\n");
		return AVF_ERR_DEVICE_NOT_SUPPORTED;
	}

	AVF_SECTION_TABLE(profile, sec_tbl);

	/* Validate sections types */
	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = AVF_SECTION_HEADER(profile, sec_off);
		if (rollback) {
			if (sec->section.type == SECTION_TYPE_MMIO ||
			    sec->section.type == SECTION_TYPE_AQ ||
			    sec->section.type == SECTION_TYPE_RB_AQ) {
				avf_debug(hw, AVF_DEBUG_PACKAGE,
					   "Not a roll-back package\n");
				return AVF_NOT_SUPPORTED;
			}
		} else {
			if (sec->section.type == SECTION_TYPE_RB_AQ ||
			    sec->section.type == SECTION_TYPE_RB_MMIO) {
				avf_debug(hw, AVF_DEBUG_PACKAGE,
					   "Not an original package\n");
				return AVF_NOT_SUPPORTED;
			}
		}
	}

	return status;
}

/**
 * avf_write_profile
 * @hw: pointer to the hardware structure
 * @profile: pointer to the profile segment of the package to be downloaded
 * @track_id: package tracking id
 *
 * Handles the download of a complete package.
 */
enum avf_status_code
avf_write_profile(struct avf_hw *hw, struct avf_profile_segment *profile,
		   u32 track_id)
{
	enum avf_status_code status = AVF_SUCCESS;
	struct avf_section_table *sec_tbl;
	struct avf_profile_section_header *sec = NULL;
	struct avf_profile_aq_section *ddp_aq;
	u32 section_size = 0;
	u32 offset = 0, info = 0;
	u32 sec_off;
	u32 i;

	status = avf_validate_profile(hw, profile, track_id, false);
	if (status)
		return status;

	AVF_SECTION_TABLE(profile, sec_tbl);

	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = AVF_SECTION_HEADER(profile, sec_off);
		/* Process generic admin command */
		if (sec->section.type == SECTION_TYPE_AQ) {
			ddp_aq = (struct avf_profile_aq_section *)&sec[1];
			status = avf_ddp_exec_aq_section(hw, ddp_aq);
			if (status) {
				avf_debug(hw, AVF_DEBUG_PACKAGE,
					   "Failed to execute aq: section %d, opcode %u\n",
					   i, ddp_aq->opcode);
				break;
			}
			sec->section.type = SECTION_TYPE_RB_AQ;
		}

		/* Skip any non-mmio sections */
		if (sec->section.type != SECTION_TYPE_MMIO)
			continue;

		section_size = sec->section.size +
			sizeof(struct avf_profile_section_header);

		/* Write MMIO section */
		status = avf_aq_write_ddp(hw, (void *)sec, (u16)section_size,
					   track_id, &offset, &info, NULL);
		if (status) {
			avf_debug(hw, AVF_DEBUG_PACKAGE,
				   "Failed to write profile: section %d, offset %d, info %d\n",
				   i, offset, info);
			break;
		}
	}
	return status;
}

/**
 * avf_rollback_profile
 * @hw: pointer to the hardware structure
 * @profile: pointer to the profile segment of the package to be removed
 * @track_id: package tracking id
 *
 * Rolls back previously loaded package.
 */
enum avf_status_code
avf_rollback_profile(struct avf_hw *hw, struct avf_profile_segment *profile,
		      u32 track_id)
{
	struct avf_profile_section_header *sec = NULL;
	enum avf_status_code status = AVF_SUCCESS;
	struct avf_section_table *sec_tbl;
	u32 offset = 0, info = 0;
	u32 section_size = 0;
	u32 sec_off;
	int i;

	status = avf_validate_profile(hw, profile, track_id, true);
	if (status)
		return status;

	AVF_SECTION_TABLE(profile, sec_tbl);

	/* For rollback write sections in reverse */
	for (i = sec_tbl->section_count - 1; i >= 0; i--) {
		sec_off = sec_tbl->section_offset[i];
		sec = AVF_SECTION_HEADER(profile, sec_off);

		/* Skip any non-rollback sections */
		if (sec->section.type != SECTION_TYPE_RB_MMIO)
			continue;

		section_size = sec->section.size +
			sizeof(struct avf_profile_section_header);

		/* Write roll-back MMIO section */
		status = avf_aq_write_ddp(hw, (void *)sec, (u16)section_size,
					   track_id, &offset, &info, NULL);
		if (status) {
			avf_debug(hw, AVF_DEBUG_PACKAGE,
				   "Failed to write profile: section %d, offset %d, info %d\n",
				   i, offset, info);
			break;
		}
	}
	return status;
}

/**
 * avf_add_pinfo_to_list
 * @hw: pointer to the hardware structure
 * @profile: pointer to the profile segment of the package
 * @profile_info_sec: buffer for information section
 * @track_id: package tracking id
 *
 * Register a profile to the list of loaded profiles.
 */
enum avf_status_code
avf_add_pinfo_to_list(struct avf_hw *hw,
		       struct avf_profile_segment *profile,
		       u8 *profile_info_sec, u32 track_id)
{
	enum avf_status_code status = AVF_SUCCESS;
	struct avf_profile_section_header *sec = NULL;
	struct avf_profile_info *pinfo;
	u32 offset = 0, info = 0;

	sec = (struct avf_profile_section_header *)profile_info_sec;
	sec->tbl_size = 1;
	sec->data_end = sizeof(struct avf_profile_section_header) +
			sizeof(struct avf_profile_info);
	sec->section.type = SECTION_TYPE_INFO;
	sec->section.offset = sizeof(struct avf_profile_section_header);
	sec->section.size = sizeof(struct avf_profile_info);
	pinfo = (struct avf_profile_info *)(profile_info_sec +
					     sec->section.offset);
	pinfo->track_id = track_id;
	pinfo->version = profile->version;
	pinfo->op = AVF_DDP_ADD_TRACKID;
	avf_memcpy(pinfo->name, profile->name, AVF_DDP_NAME_SIZE,
		    AVF_NONDMA_TO_NONDMA);

	status = avf_aq_write_ddp(hw, (void *)sec, sec->data_end,
				   track_id, &offset, &info, NULL);
	return status;
}
