/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include "ixgbe_type.h"
#include "ixgbe_e610.h"
#include "ixgbe_x550.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"
#include "ixgbe_api.h"

/**
 * ixgbe_init_aci - initialization routine for Admin Command Interface
 * @hw: pointer to the hardware structure
 *
 * Initialize the ACI lock.
 */
void ixgbe_init_aci(struct ixgbe_hw *hw)
{
	ixgbe_init_lock(&hw->aci.lock);
}

/**
 * ixgbe_shutdown_aci - shutdown routine for Admin Command Interface
 * @hw: pointer to the hardware structure
 *
 * Destroy the ACI lock.
 */
void ixgbe_shutdown_aci(struct ixgbe_hw *hw)
{
	ixgbe_destroy_lock(&hw->aci.lock);
}

/**
 * ixgbe_should_retry_aci_send_cmd_execute - decide if ACI command should
 * be resent
 * @opcode: ACI opcode
 *
 * Check if ACI command should be sent again depending on the provided opcode.
 *
 * Return: true if the sending command routine should be repeated,
 * otherwise false.
 */
STATIC bool ixgbe_should_retry_aci_send_cmd_execute(u16 opcode)
{

	switch (opcode) {
	case ixgbe_aci_opc_disable_rxen:
	case ixgbe_aci_opc_get_phy_caps:
	case ixgbe_aci_opc_get_link_status:
	case ixgbe_aci_opc_get_link_topo:
		return true;
	}

	return false;
}

/**
 * ixgbe_aci_send_cmd_execute - execute sending FW Admin Command to FW Admin
 * Command Interface
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 *
 * Admin Command is sent using CSR by setting descriptor and buffer in specific
 * registers.
 *
 * Return: the exit code of the operation.
 * * - IXGBE_SUCCESS - success.
 * * - IXGBE_ERR_ACI_DISABLED - CSR mechanism is not enabled.
 * * - IXGBE_ERR_ACI_BUSY - CSR mechanism is busy.
 * * - IXGBE_ERR_PARAM - buf_size is too big or
 * invalid argument buf or buf_size.
 * * - IXGBE_ERR_ACI_TIMEOUT - Admin Command X command timeout.
 * * - IXGBE_ERR_ACI_ERROR - Admin Command X invalid state of HICR register or
 * Admin Command failed because of bad opcode was returned or
 * Admin Command failed with error Y.
 */
STATIC s32
ixgbe_aci_send_cmd_execute(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
			   void *buf, u16 buf_size)
{
	u32 hicr = 0, tmp_buf_size = 0, i = 0;
	u32 *raw_desc = (u32 *)desc;
	s32 status = IXGBE_SUCCESS;
	bool valid_buf = false;
	u32 *tmp_buf = NULL;
	u16 opcode = 0;

	do {
		hw->aci.last_status = IXGBE_ACI_RC_OK;

		/* It's necessary to check if mechanism is enabled */
		hicr = IXGBE_READ_REG(hw, PF_HICR);
		if (!(hicr & PF_HICR_EN)) {
			status = IXGBE_ERR_ACI_DISABLED;
			break;
		}
		if (hicr & PF_HICR_C) {
			hw->aci.last_status = IXGBE_ACI_RC_EBUSY;
			status = IXGBE_ERR_ACI_BUSY;
			break;
		}
		opcode = desc->opcode;

		if (buf_size > IXGBE_ACI_MAX_BUFFER_SIZE) {
			status = IXGBE_ERR_PARAM;
			break;
		}

		if (buf)
			desc->flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF);

		/* Check if buf and buf_size are proper params */
		if (desc->flags & IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF)) {
			if ((buf && buf_size == 0) ||
			    (buf == NULL && buf_size)) {
				status = IXGBE_ERR_PARAM;
				break;
			}
			if (buf && buf_size)
				valid_buf = true;
		}

		if (valid_buf == true) {
			if (buf_size % 4 == 0)
				tmp_buf_size = buf_size;
			else
				tmp_buf_size = (buf_size & (u16)(~0x03)) + 4;

			tmp_buf = (u32*)ixgbe_malloc(hw, tmp_buf_size);
			if (!tmp_buf)
				return IXGBE_ERR_OUT_OF_MEM;

			/* tmp_buf will be firstly filled with 0xFF and after
			 * that the content of buf will be written into it.
			 * This approach lets us use valid buf_size and
			 * prevents us from reading past buf area
			 * when buf_size mod 4 not equal to 0.
			 */
			memset(tmp_buf, 0xFF, tmp_buf_size);
			memcpy(tmp_buf, buf, buf_size);

			if (tmp_buf_size > IXGBE_ACI_LG_BUF)
				desc->flags |=
				IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_LB);

			desc->datalen = IXGBE_CPU_TO_LE16(buf_size);

			if (desc->flags & IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD)) {
				for (i = 0; i < tmp_buf_size / 4; i++) {
					IXGBE_WRITE_REG(hw, PF_HIBA(i),
						IXGBE_LE32_TO_CPU(tmp_buf[i]));
				}
			}
		}

		/* Descriptor is written to specific registers */
		for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++)
			IXGBE_WRITE_REG(hw, PF_HIDA(i),
					IXGBE_LE32_TO_CPU(raw_desc[i]));

		/* SW has to set PF_HICR.C bit and clear PF_HICR.SV and
		 * PF_HICR_EV
		 */
		hicr = IXGBE_READ_REG(hw, PF_HICR);
		hicr = (hicr | PF_HICR_C) & ~(PF_HICR_SV | PF_HICR_EV);
		IXGBE_WRITE_REG(hw, PF_HICR, hicr);

		/* Wait for sync Admin Command response */
		for (i = 0; i < IXGBE_ACI_SYNC_RESPONSE_TIMEOUT; i += 1) {
			hicr = IXGBE_READ_REG(hw, PF_HICR);
			if ((hicr & PF_HICR_SV) || !(hicr & PF_HICR_C))
				break;

			msec_delay(1);
		}

		/* Wait for async Admin Command response */
		if ((hicr & PF_HICR_SV) && (hicr & PF_HICR_C)) {
			for (i = 0; i < IXGBE_ACI_ASYNC_RESPONSE_TIMEOUT;
			     i += 1) {
				hicr = IXGBE_READ_REG(hw, PF_HICR);
				if ((hicr & PF_HICR_EV) || !(hicr & PF_HICR_C))
					break;

				msec_delay(1);
			}
		}

		/* Read sync Admin Command response */
		if ((hicr & PF_HICR_SV)) {
			for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++) {
				raw_desc[i] = IXGBE_READ_REG(hw, PF_HIDA(i));
				raw_desc[i] = IXGBE_CPU_TO_LE32(raw_desc[i]);
			}
		}

		/* Read async Admin Command response */
		if ((hicr & PF_HICR_EV) && !(hicr & PF_HICR_C)) {
			for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++) {
				raw_desc[i] = IXGBE_READ_REG(hw, PF_HIDA_2(i));
				raw_desc[i] = IXGBE_CPU_TO_LE32(raw_desc[i]);
			}
		}

		/* Handle timeout and invalid state of HICR register */
		if (hicr & PF_HICR_C) {
			status = IXGBE_ERR_ACI_TIMEOUT;
			break;
		} else if (!(hicr & PF_HICR_SV) && !(hicr & PF_HICR_EV)) {
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		/* For every command other than 0x0014 treat opcode mismatch
		 * as an error. Response to 0x0014 command read from HIDA_2
		 * is a descriptor of an event which is expected to contain
		 * different opcode than the command.
		 */
		if (desc->opcode != opcode &&
		    opcode != IXGBE_CPU_TO_LE16(ixgbe_aci_opc_get_fw_event)) {
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		if (desc->retval != IXGBE_ACI_RC_OK) {
			hw->aci.last_status = (enum ixgbe_aci_err)desc->retval;
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		/* Write a response values to a buf */
		if (valid_buf && (desc->flags &
				  IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF))) {
			for (i = 0; i < tmp_buf_size / 4; i++) {
				tmp_buf[i] = IXGBE_READ_REG(hw, PF_HIBA(i));
				tmp_buf[i] = IXGBE_CPU_TO_LE32(tmp_buf[i]);
			}
			memcpy(buf, tmp_buf, buf_size);
		}
	} while (0);

	if (tmp_buf)
		ixgbe_free(hw, tmp_buf);

	return status;
}

/**
 * ixgbe_aci_send_cmd - send FW Admin Command to FW Admin Command Interface
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 *
 * Helper function to send FW Admin Commands to the FW Admin Command Interface.
 *
 * Retry sending the FW Admin Command multiple times to the FW ACI
 * if the EBUSY Admin Command error is returned.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_send_cmd(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
		       void *buf, u16 buf_size)
{
	struct ixgbe_aci_desc desc_cpy;
	enum ixgbe_aci_err last_status;
	bool is_cmd_for_retry;
	u8 *buf_cpy = NULL;
	s32 status;
	u16 opcode;
	u8 idx = 0;

	opcode = IXGBE_LE16_TO_CPU(desc->opcode);
	is_cmd_for_retry = ixgbe_should_retry_aci_send_cmd_execute(opcode);
	memset(&desc_cpy, 0, sizeof(desc_cpy));

	if (is_cmd_for_retry) {
		if (buf) {
			buf_cpy = (u8 *)ixgbe_malloc(hw, buf_size);
			if (!buf_cpy)
				return IXGBE_ERR_OUT_OF_MEM;
		}
		memcpy(&desc_cpy, desc, sizeof(desc_cpy));
	}

	do {
		ixgbe_acquire_lock(&hw->aci.lock);
		status = ixgbe_aci_send_cmd_execute(hw, desc, buf, buf_size);
		last_status = hw->aci.last_status;
		ixgbe_release_lock(&hw->aci.lock);

		if (!is_cmd_for_retry || status == IXGBE_SUCCESS ||
		    last_status != IXGBE_ACI_RC_EBUSY)
			break;

		if (buf)
			memcpy(buf, buf_cpy, buf_size);
		memcpy(desc, &desc_cpy, sizeof(desc_cpy));

		msec_delay(IXGBE_ACI_SEND_DELAY_TIME_MS);
	} while (++idx < IXGBE_ACI_SEND_MAX_EXECUTE);

	if (buf_cpy)
		ixgbe_free(hw, buf_cpy);

	return status;
}

/**
 * ixgbe_aci_check_event_pending - check if there are any pending events
 * @hw: pointer to the HW struct
 *
 * Determine if there are any pending events.
 *
 * Return: true if there are any currently pending events
 * otherwise false.
 */
bool ixgbe_aci_check_event_pending(struct ixgbe_hw *hw)
{
	u32 ep_bit_mask;
	u32 fwsts;

	ep_bit_mask = hw->bus.func ? GL_FWSTS_EP_PF1 : GL_FWSTS_EP_PF0;

	/* Check state of Event Pending (EP) bit */
	fwsts = IXGBE_READ_REG(hw, GL_FWSTS);
	return (fwsts & ep_bit_mask) ? true : false;
}

/**
 * ixgbe_aci_get_event - get an event from ACI
 * @hw: pointer to the HW struct
 * @e: event information structure
 * @pending: optional flag signaling that there are more pending events
 *
 * Obtain an event from ACI and return its content
 * through 'e' using ACI command (0x0014).
 * Provide information if there are more events
 * to retrieve through 'pending'.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_event(struct ixgbe_hw *hw, struct ixgbe_aci_event *e,
			bool *pending)
{
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!e || (!e->msg_buf && e->buf_len) || (e->msg_buf && !e->buf_len))
		return IXGBE_ERR_PARAM;

	ixgbe_acquire_lock(&hw->aci.lock);

	/* Check if there are any events pending */
	if (!ixgbe_aci_check_event_pending(hw)) {
		status = IXGBE_ERR_ACI_NO_EVENTS;
		goto aci_get_event_exit;
	}

	/* Obtain pending event */
	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_fw_event);
	status = ixgbe_aci_send_cmd_execute(hw, &desc, e->msg_buf, e->buf_len);
	if (status)
		goto aci_get_event_exit;

	/* Returned 0x0014 opcode indicates that no event was obtained */
	if (desc.opcode == IXGBE_CPU_TO_LE16(ixgbe_aci_opc_get_fw_event)) {
		status = IXGBE_ERR_ACI_NO_EVENTS;
		goto aci_get_event_exit;
	}

	/* Determine size of event data */
	e->msg_len = MIN_T(u16, IXGBE_LE16_TO_CPU(desc.datalen), e->buf_len);
	/* Write event descriptor to event info structure */
	memcpy(&e->desc, &desc, sizeof(e->desc));

	/* Check if there are any further events pending */
	if (pending) {
		*pending = ixgbe_aci_check_event_pending(hw);
	}

aci_get_event_exit:
	ixgbe_release_lock(&hw->aci.lock);

	return status;
}

/**
 * ixgbe_fill_dflt_direct_cmd_desc - fill ACI descriptor with default values.
 * @desc: pointer to the temp descriptor (non DMA mem)
 * @opcode: the opcode can be used to decide which flags to turn off or on
 *
 * Helper function to fill the descriptor desc with default values
 * and the provided opcode.
 */
void ixgbe_fill_dflt_direct_cmd_desc(struct ixgbe_aci_desc *desc, u16 opcode)
{
	/* zero out the desc */
	memset(desc, 0, sizeof(*desc));
	desc->opcode = IXGBE_CPU_TO_LE16(opcode);
	desc->flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_SI);
}

/**
 * ixgbe_aci_req_res - request a common resource
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @access: access type
 * @sdp_number: resource number
 * @timeout: the maximum time in ms that the driver may hold the resource
 *
 * Requests a common resource using the ACI command (0x0008).
 * Specifies the maximum time the driver may hold the resource.
 * If the requested resource is currently occupied by some other driver,
 * a busy return value is returned and the timeout field value indicates the
 * maximum time the current owner has to free it.
 *
 * Return: the exit code of the operation.
 */
static s32
ixgbe_aci_req_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		  enum ixgbe_aci_res_access_type access, u8 sdp_number,
		  u32 *timeout)
{
	struct ixgbe_aci_cmd_req_res *cmd_resp;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd_resp = &desc.params.res_owner;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_req_res);

	cmd_resp->res_id = IXGBE_CPU_TO_LE16(res);
	cmd_resp->access_type = IXGBE_CPU_TO_LE16(access);
	cmd_resp->res_number = IXGBE_CPU_TO_LE32(sdp_number);
	cmd_resp->timeout = IXGBE_CPU_TO_LE32(*timeout);
	*timeout = 0;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	/* The completion specifies the maximum time in ms that the driver
	 * may hold the resource in the Timeout field.
	 */

	/* If the resource is held by some other driver, the command completes
	 * with a busy return value and the timeout field indicates the maximum
	 * time the current owner of the resource has to free it.
	 */
	if (!status || hw->aci.last_status == IXGBE_ACI_RC_EBUSY)
		*timeout = IXGBE_LE32_TO_CPU(cmd_resp->timeout);

	return status;
}

/**
 * ixgbe_aci_release_res - release a common resource using ACI
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @sdp_number: resource number
 *
 * Release a common resource using ACI command (0x0009).
 *
 * Return: the exit code of the operation.
 */
static s32
ixgbe_aci_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      u8 sdp_number)
{
	struct ixgbe_aci_cmd_req_res *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.res_owner;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_release_res);

	cmd->res_id = IXGBE_CPU_TO_LE16(res);
	cmd->res_number = IXGBE_CPU_TO_LE32(sdp_number);

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_acquire_res - acquire the ownership of a resource
 * @hw: pointer to the HW structure
 * @res: resource ID
 * @access: access type (read or write)
 * @timeout: timeout in milliseconds
 *
 * Make an attempt to acquire the ownership of a resource using
 * the ixgbe_aci_req_res to utilize ACI.
 * In case if some other driver has previously acquired the resource and
 * performed any necessary updates, the IXGBE_ERR_ACI_NO_WORK is returned,
 * and the caller does not obtain the resource and has no further work to do.
 * If needed, the function will poll until the current lock owner timeouts.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_acquire_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      enum ixgbe_aci_res_access_type access, u32 timeout)
{
#define IXGBE_RES_POLLING_DELAY_MS	10
	u32 delay = IXGBE_RES_POLLING_DELAY_MS;
	u32 res_timeout = timeout;
	u32 retry_timeout = 0;
	s32 status;

	status = ixgbe_aci_req_res(hw, res, access, 0, &res_timeout);

	/* A return code of IXGBE_ERR_ACI_NO_WORK means that another driver has
	 * previously acquired the resource and performed any necessary updates;
	 * in this case the caller does not obtain the resource and has no
	 * further work to do.
	 */
	if (status == IXGBE_ERR_ACI_NO_WORK)
		goto ixgbe_acquire_res_exit;

	/* If necessary, poll until the current lock owner timeouts.
	 * Set retry_timeout to the timeout value reported by the FW in the
	 * response to the "Request Resource Ownership" (0x0008) Admin Command
	 * as it indicates the maximum time the current owner of the resource
	 * is allowed to hold it.
	 */
	retry_timeout = res_timeout;
	while (status && retry_timeout && res_timeout) {
		msec_delay(delay);
		retry_timeout = (retry_timeout > delay) ?
			retry_timeout - delay : 0;
		status = ixgbe_aci_req_res(hw, res, access, 0, &res_timeout);

		if (status == IXGBE_ERR_ACI_NO_WORK)
			/* lock free, but no work to do */
			break;

		if (!status)
			/* lock acquired */
			break;
	}

ixgbe_acquire_res_exit:
	return status;
}

/**
 * ixgbe_release_res - release a common resource
 * @hw: pointer to the HW structure
 * @res: resource ID
 *
 * Release a common resource using ixgbe_aci_release_res.
 */
void ixgbe_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res)
{
	u32 total_delay = 0;
	s32 status;

	status = ixgbe_aci_release_res(hw, res, 0);

	/* There are some rare cases when trying to release the resource
	 * results in an admin command timeout, so handle them correctly.
	 */
	while ((status == IXGBE_ERR_ACI_TIMEOUT) &&
	       (total_delay < IXGBE_ACI_RELEASE_RES_TIMEOUT)) {
		msec_delay(1);
		status = ixgbe_aci_release_res(hw, res, 0);
		total_delay++;
	}
}

/**
 * ixgbe_parse_common_caps - Parse common device/function capabilities
 * @hw: pointer to the HW struct
 * @caps: pointer to common capabilities structure
 * @elem: the capability element to parse
 * @prefix: message prefix for tracing capabilities
 *
 * Given a capability element, extract relevant details into the common
 * capability structure.
 *
 * Return: true if the capability matches one of the common capability ids,
 * false otherwise.
 */
static bool
ixgbe_parse_common_caps(struct ixgbe_hw *hw, struct ixgbe_hw_common_caps *caps,
			struct ixgbe_aci_cmd_list_caps_elem *elem,
			const char *prefix)
{
	u32 logical_id = IXGBE_LE32_TO_CPU(elem->logical_id);
	u32 phys_id = IXGBE_LE32_TO_CPU(elem->phys_id);
	u32 number = IXGBE_LE32_TO_CPU(elem->number);
	u16 cap = IXGBE_LE16_TO_CPU(elem->cap);
	bool found = true;

	UNREFERENCED_1PARAMETER(hw);

	switch (cap) {
	case IXGBE_ACI_CAPS_VALID_FUNCTIONS:
		caps->valid_functions = number;
		break;
	case IXGBE_ACI_CAPS_VMDQ:
		caps->vmdq = (number == 1);
		break;
	case IXGBE_ACI_CAPS_DCB:
		caps->dcb = (number == 1);
		caps->active_tc_bitmap = logical_id;
		caps->maxtc = phys_id;
		break;
	case IXGBE_ACI_CAPS_RSS:
		caps->rss_table_size = number;
		caps->rss_table_entry_width = logical_id;
		break;
	case IXGBE_ACI_CAPS_RXQS:
		caps->num_rxq = number;
		caps->rxq_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_TXQS:
		caps->num_txq = number;
		caps->txq_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_MSIX:
		caps->num_msix_vectors = number;
		caps->msix_vector_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_NVM_VER:
		break;
	case IXGBE_ACI_CAPS_NVM_MGMT:
		caps->sec_rev_disabled =
			(number & IXGBE_NVM_MGMT_SEC_REV_DISABLED) ?
			true : false;
		caps->update_disabled =
			(number & IXGBE_NVM_MGMT_UPDATE_DISABLED) ?
			true : false;
		caps->nvm_unified_update =
			(number & IXGBE_NVM_MGMT_UNIFIED_UPD_SUPPORT) ?
			true : false;
		caps->netlist_auth =
			(number & IXGBE_NVM_MGMT_NETLIST_AUTH_SUPPORT) ?
			true : false;
		break;
	case IXGBE_ACI_CAPS_MAX_MTU:
		caps->max_mtu = number;
		break;
	case IXGBE_ACI_CAPS_PCIE_RESET_AVOIDANCE:
		caps->pcie_reset_avoidance = (number > 0);
		break;
	case IXGBE_ACI_CAPS_POST_UPDATE_RESET_RESTRICT:
		caps->reset_restrict_support = (number == 1);
		break;
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG0:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG1:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG2:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG3:
	{
		u8 index = cap - IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG0;

		caps->ext_topo_dev_img_ver_high[index] = number;
		caps->ext_topo_dev_img_ver_low[index] = logical_id;
		caps->ext_topo_dev_img_part_num[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_M) >>
			IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_S;
		caps->ext_topo_dev_img_load_en[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_LOAD_EN) != 0;
		caps->ext_topo_dev_img_prog_en[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_PROG_EN) != 0;
		break;
	}

	case IXGBE_ACI_CAPS_NEXT_CLUSTER_ID:
		caps->next_cluster_id_support = (number == 1);
		DEBUGOUT2("%s: next_cluster_id_support = %d\n",
			  prefix, caps->next_cluster_id_support);
		break;
	default:
		/* Not one of the recognized common capabilities */
		found = false;
	}

	return found;
}

/**
 * ixgbe_hweight8 - count set bits among the 8 lowest bits
 * @w: variable storing set bits to count
 *
 * Return: the number of set bits among the 8 lowest bits in the provided value.
 */
static u8 ixgbe_hweight8(u32 w)
{
	u8 hweight = 0, i;

	for (i = 0; i < 8; i++)
		if (w & (1 << i))
			hweight++;

	return hweight;
}

/**
 * ixgbe_hweight32 - count set bits among the 32 lowest bits
 * @w: variable storing set bits to count
 *
 * Return: the number of set bits among the 32 lowest bits in the
 * provided value.
 */
static u8 ixgbe_hweight32(u32 w)
{
	u32 bitMask = 0x1, i;
	u8  bitCnt = 0;

	for (i = 0; i < 32; i++)
	{
		if (w & bitMask)
			bitCnt++;

		bitMask = bitMask << 0x1;
	}

	return bitCnt;
}

/**
 * ixgbe_func_id_to_logical_id - map from function id to logical pf id
 * @active_function_bitmap: active function bitmap
 * @pf_id: function number of device
 *
 * Return: the logical id of a function mapped by the provided pf_id.
 */
static int ixgbe_func_id_to_logical_id(u32 active_function_bitmap, u8 pf_id)
{
	u8 logical_id = 0;
	u8 i;

	for (i = 0; i < pf_id; i++)
		if (active_function_bitmap & BIT(i))
			logical_id++;

	return logical_id;
}

/**
 * ixgbe_parse_valid_functions_cap - Parse IXGBE_ACI_CAPS_VALID_FUNCTIONS caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_VALID_FUNCTIONS for device capabilities.
 */
static void
ixgbe_parse_valid_functions_cap(struct ixgbe_hw *hw,
				struct ixgbe_hw_dev_caps *dev_p,
				struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_funcs = ixgbe_hweight32(number);

	hw->logical_pf_id = ixgbe_func_id_to_logical_id(number, hw->pf_id);
}

/**
 * ixgbe_parse_vsi_dev_caps - Parse IXGBE_ACI_CAPS_VSI device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_VSI for device capabilities.
 */
static void ixgbe_parse_vsi_dev_caps(struct ixgbe_hw *hw,
				     struct ixgbe_hw_dev_caps *dev_p,
				     struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_vsi_allocd_to_host = number;
}

/**
 * ixgbe_parse_1588_dev_caps - Parse IXGBE_ACI_CAPS_1588 device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_1588 for device capabilities.
 */
static void ixgbe_parse_1588_dev_caps(struct ixgbe_hw *hw,
				      struct ixgbe_hw_dev_caps *dev_p,
				      struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	struct ixgbe_ts_dev_info *info = &dev_p->ts_dev_info;
	u32 logical_id = IXGBE_LE32_TO_CPU(cap->logical_id);
	u32 phys_id = IXGBE_LE32_TO_CPU(cap->phys_id);
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	info->ena = ((number & IXGBE_TS_DEV_ENA_M) != 0);
	dev_p->common_cap.ieee_1588 = info->ena;

	info->tmr0_owner = number & IXGBE_TS_TMR0_OWNR_M;
	info->tmr0_owned = ((number & IXGBE_TS_TMR0_OWND_M) != 0);
	info->tmr0_ena = ((number & IXGBE_TS_TMR0_ENA_M) != 0);

	info->tmr1_owner = (number & IXGBE_TS_TMR1_OWNR_M) >>
			   IXGBE_TS_TMR1_OWNR_S;
	info->tmr1_owned = ((number & IXGBE_TS_TMR1_OWND_M) != 0);
	info->tmr1_ena = ((number & IXGBE_TS_TMR1_ENA_M) != 0);

	info->ena_ports = logical_id;
	info->tmr_own_map = phys_id;

}

/**
 * ixgbe_parse_fdir_dev_caps - Parse IXGBE_ACI_CAPS_FD device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_FD for device capabilities.
 */
static void ixgbe_parse_fdir_dev_caps(struct ixgbe_hw *hw,
				      struct ixgbe_hw_dev_caps *dev_p,
				      struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_flow_director_fltr = number;
}

/**
 * ixgbe_parse_dev_caps - Parse device capabilities
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @buf: buffer containing the device capability records
 * @cap_count: the number of capabilities
 *
 * Helper device to parse device (0x000B) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ixgbe_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the device capabilities structured.
 */
static void ixgbe_parse_dev_caps(struct ixgbe_hw *hw,
				 struct ixgbe_hw_dev_caps *dev_p,
				 void *buf, u32 cap_count)
{
	struct ixgbe_aci_cmd_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ixgbe_aci_cmd_list_caps_elem *)buf;

	memset(dev_p, 0, sizeof(*dev_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = IXGBE_LE16_TO_CPU(cap_resp[i].cap);
		bool found;

		found = ixgbe_parse_common_caps(hw, &dev_p->common_cap,
					      &cap_resp[i], "dev caps");

		switch (cap) {
		case IXGBE_ACI_CAPS_VALID_FUNCTIONS:
			ixgbe_parse_valid_functions_cap(hw, dev_p,
							&cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_VSI:
			ixgbe_parse_vsi_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_1588:
			ixgbe_parse_1588_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case  IXGBE_ACI_CAPS_FD:
			ixgbe_parse_fdir_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			if (!found)
			break;
		}
	}

}

/**
 * ixgbe_get_num_per_func - determine number of resources per PF
 * @hw: pointer to the HW structure
 * @max: value to be evenly split between each PF
 *
 * Determine the number of valid functions by going through the bitmap returned
 * from parsing capabilities and use this to calculate the number of resources
 * per PF based on the max value passed in.
 *
 * Return: the number of resources per PF or 0, if no PH are available.
 */
static u32 ixgbe_get_num_per_func(struct ixgbe_hw *hw, u32 max)
{
	u8 funcs;

#define IXGBE_CAPS_VALID_FUNCS_M	0xFF
	funcs = ixgbe_hweight8(hw->dev_caps.common_cap.valid_functions &
			     IXGBE_CAPS_VALID_FUNCS_M);

	if (!funcs)
		return 0;

	return max / funcs;
}

/**
 * ixgbe_parse_vsi_func_caps - Parse IXGBE_ACI_CAPS_VSI function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for IXGBE_ACI_CAPS_VSI.
 */
static void ixgbe_parse_vsi_func_caps(struct ixgbe_hw *hw,
				      struct ixgbe_hw_func_caps *func_p,
				      struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	func_p->guar_num_vsi = ixgbe_get_num_per_func(hw, IXGBE_MAX_VSI);
}

/**
 * ixgbe_parse_1588_func_caps - Parse IXGBE_ACI_CAPS_1588 function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for IXGBE_ACI_CAPS_1588.
 */
static void ixgbe_parse_1588_func_caps(struct ixgbe_hw *hw,
				       struct ixgbe_hw_func_caps *func_p,
				       struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	struct ixgbe_ts_func_info *info = &func_p->ts_func_info;
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	info->ena = ((number & IXGBE_TS_FUNC_ENA_M) != 0);
	func_p->common_cap.ieee_1588 = info->ena;

	info->src_tmr_owned = ((number & IXGBE_TS_SRC_TMR_OWND_M) != 0);
	info->tmr_ena = ((number & IXGBE_TS_TMR_ENA_M) != 0);
	info->tmr_index_owned = ((number & IXGBE_TS_TMR_IDX_OWND_M) != 0);
	info->tmr_index_assoc = ((number & IXGBE_TS_TMR_IDX_ASSOC_M) != 0);

	info->clk_freq = (number & IXGBE_TS_CLK_FREQ_M) >> IXGBE_TS_CLK_FREQ_S;
	info->clk_src = ((number & IXGBE_TS_CLK_SRC_M) != 0);

	if (info->clk_freq < NUM_IXGBE_TIME_REF_FREQ) {
		info->time_ref = (enum ixgbe_time_ref_freq)info->clk_freq;
	} else {
		/* Unknown clock frequency, so assume a (probably incorrect)
		 * default to avoid out-of-bounds look ups of frequency
		 * related information.
		 */
		info->time_ref = IXGBE_TIME_REF_FREQ_25_000;
	}

}
/**
 * ixgbe_parse_func_caps - Parse function capabilities
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @buf: buffer containing the function capability records
 * @cap_count: the number of capabilities
 *
 * Helper function to parse function (0x000A) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ixgbe_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the function capabilities structured.
 */
static void ixgbe_parse_func_caps(struct ixgbe_hw *hw,
				  struct ixgbe_hw_func_caps *func_p,
				  void *buf, u32 cap_count)
{
	struct ixgbe_aci_cmd_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ixgbe_aci_cmd_list_caps_elem *)buf;

	memset(func_p, 0, sizeof(*func_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = IXGBE_LE16_TO_CPU(cap_resp[i].cap);

		ixgbe_parse_common_caps(hw, &func_p->common_cap,
					&cap_resp[i], "func caps");

		switch (cap) {
		case IXGBE_ACI_CAPS_VSI:
			ixgbe_parse_vsi_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_1588:
			ixgbe_parse_1588_func_caps(hw, func_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			break;
		}
	}

}

/**
 * ixgbe_aci_list_caps - query function/device capabilities
 * @hw: pointer to the HW struct
 * @buf: a buffer to hold the capabilities
 * @buf_size: size of the buffer
 * @cap_count: if not NULL, set to the number of capabilities reported
 * @opc: capabilities type to discover, device or function
 *
 * Get the function (0x000A) or device (0x000B) capabilities description from
 * firmware and store it in the buffer.
 *
 * If the cap_count pointer is not NULL, then it is set to the number of
 * capabilities firmware will report. Note that if the buffer size is too
 * small, it is possible the command will return IXGBE_ERR_OUT_OF_MEM. The
 * cap_count will still be updated in this case. It is recommended that the
 * buffer size be set to IXGBE_ACI_MAX_BUFFER_SIZE (the largest possible
 * buffer that firmware could return) to avoid this.
 *
 * Return: the exit code of the operation.
 * Exit code of IXGBE_ERR_OUT_OF_MEM means the buffer size is too small.
 */
s32 ixgbe_aci_list_caps(struct ixgbe_hw *hw, void *buf, u16 buf_size,
			u32 *cap_count, enum ixgbe_aci_opc opc)
{
	struct ixgbe_aci_cmd_list_caps *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.get_cap;

	if (opc != ixgbe_aci_opc_list_func_caps &&
	    opc != ixgbe_aci_opc_list_dev_caps)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, opc);
	status = ixgbe_aci_send_cmd(hw, &desc, buf, buf_size);

	if (cap_count)
		*cap_count = IXGBE_LE32_TO_CPU(cmd->count);

	return status;
}

/**
 * ixgbe_discover_dev_caps - Read and extract device capabilities
 * @hw: pointer to the hardware structure
 * @dev_caps: pointer to device capabilities structure
 *
 * Read the device capabilities and extract them into the dev_caps structure
 * for later use.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_discover_dev_caps(struct ixgbe_hw *hw,
			    struct ixgbe_hw_dev_caps *dev_caps)
{
	u32 status, cap_count = 0;
	u8 *cbuf = NULL;

	cbuf = (u8*)ixgbe_malloc(hw, IXGBE_ACI_MAX_BUFFER_SIZE);
	if (!cbuf)
		return IXGBE_ERR_OUT_OF_MEM;
	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = IXGBE_ACI_MAX_BUFFER_SIZE /
		    sizeof(struct ixgbe_aci_cmd_list_caps_elem);

	status = ixgbe_aci_list_caps(hw, cbuf, IXGBE_ACI_MAX_BUFFER_SIZE,
				     &cap_count,
				     ixgbe_aci_opc_list_dev_caps);
	if (!status)
		ixgbe_parse_dev_caps(hw, dev_caps, cbuf, cap_count);

	if (cbuf)
		ixgbe_free(hw, cbuf);

	return status;
}

/**
 * ixgbe_discover_func_caps - Read and extract function capabilities
 * @hw: pointer to the hardware structure
 * @func_caps: pointer to function capabilities structure
 *
 * Read the function capabilities and extract them into the func_caps structure
 * for later use.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_discover_func_caps(struct ixgbe_hw *hw,
			     struct ixgbe_hw_func_caps *func_caps)
{
	u32 cap_count = 0;
	u8 *cbuf = NULL;
	s32 status;

	cbuf = (u8*)ixgbe_malloc(hw, IXGBE_ACI_MAX_BUFFER_SIZE);
	if(!cbuf)
		return IXGBE_ERR_OUT_OF_MEM;
	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = IXGBE_ACI_MAX_BUFFER_SIZE /
		    sizeof(struct ixgbe_aci_cmd_list_caps_elem);

	status = ixgbe_aci_list_caps(hw, cbuf, IXGBE_ACI_MAX_BUFFER_SIZE,
				     &cap_count,
				     ixgbe_aci_opc_list_func_caps);
	if (!status)
		ixgbe_parse_func_caps(hw, func_caps, cbuf, cap_count);

	if (cbuf)
		ixgbe_free(hw, cbuf);

	return status;
}

/**
 * ixgbe_get_caps - get info about the HW
 * @hw: pointer to the hardware structure
 *
 * Retrieve both device and function capabilities.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_caps(struct ixgbe_hw *hw)
{
	s32 status;

	status = ixgbe_discover_dev_caps(hw, &hw->dev_caps);
	if (status)
		return status;

	return ixgbe_discover_func_caps(hw, &hw->func_caps);
}

/**
 * ixgbe_aci_disable_rxen - disable RX
 * @hw: pointer to the HW struct
 *
 * Request a safe disable of Receive Enable using ACI command (0x000C).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_disable_rxen(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_disable_rxen *cmd;
	struct ixgbe_aci_desc desc;

	UNREFERENCED_1PARAMETER(hw);

	cmd = &desc.params.disable_rxen;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_disable_rxen);

	cmd->lport_num = (u8)hw->bus.func;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_get_phy_caps - returns PHY capabilities
 * @hw: pointer to the HW struct
 * @qual_mods: report qualified modules
 * @report_mode: report mode capabilities
 * @pcaps: structure for PHY capabilities to be filled
 *
 * Returns the various PHY capabilities supported on the Port
 * using ACI command (0x0600).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_phy_caps(struct ixgbe_hw *hw, bool qual_mods, u8 report_mode,
			   struct ixgbe_aci_cmd_get_phy_caps_data *pcaps)
{
	struct ixgbe_aci_cmd_get_phy_caps *cmd;
	u16 pcaps_size = sizeof(*pcaps);
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.get_phy;

	if (!pcaps || (report_mode & ~IXGBE_ACI_REPORT_MODE_M))
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_phy_caps);

	if (qual_mods)
		cmd->param0 |= IXGBE_CPU_TO_LE16(IXGBE_ACI_GET_PHY_RQM);

	cmd->param0 |= IXGBE_CPU_TO_LE16(report_mode);
	status = ixgbe_aci_send_cmd(hw, &desc, pcaps, pcaps_size);

	if (status == IXGBE_SUCCESS &&
	    report_mode == IXGBE_ACI_REPORT_TOPO_CAP_MEDIA) {
		hw->phy.phy_type_low = IXGBE_LE64_TO_CPU(pcaps->phy_type_low);
		hw->phy.phy_type_high = IXGBE_LE64_TO_CPU(pcaps->phy_type_high);
		memcpy(hw->link.link_info.module_type, &pcaps->module_type,
			   sizeof(hw->link.link_info.module_type));
	}

	return status;
}

/**
 * ixgbe_phy_caps_equals_cfg - check if capabilities match the PHY config
 * @phy_caps: PHY capabilities
 * @phy_cfg: PHY configuration
 *
 * Helper function to determine if PHY capabilities match PHY
 * configuration
 *
 * Return: true if PHY capabilities match PHY configuration.
 */
bool
ixgbe_phy_caps_equals_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *phy_caps,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *phy_cfg)
{
	u8 caps_mask, cfg_mask;

	if (!phy_caps || !phy_cfg)
		return false;

	/* These bits are not common between capabilities and configuration.
	 * Do not use them to determine equality.
	 */
	caps_mask = IXGBE_ACI_PHY_CAPS_MASK & ~(IXGBE_ACI_PHY_AN_MODE |
					      IXGBE_ACI_PHY_EN_MOD_QUAL);
	cfg_mask = IXGBE_ACI_PHY_ENA_VALID_MASK &
		   ~IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

	if (phy_caps->phy_type_low != phy_cfg->phy_type_low ||
	    phy_caps->phy_type_high != phy_cfg->phy_type_high ||
	    ((phy_caps->caps & caps_mask) != (phy_cfg->caps & cfg_mask)) ||
	    phy_caps->low_power_ctrl_an != phy_cfg->low_power_ctrl_an ||
	    phy_caps->eee_cap != phy_cfg->eee_cap ||
	    phy_caps->eeer_value != phy_cfg->eeer_value ||
	    phy_caps->link_fec_options != phy_cfg->link_fec_opt)
		return false;

	return true;
}

/**
 * ixgbe_copy_phy_caps_to_cfg - Copy PHY ability data to configuration data
 * @caps: PHY ability structure to copy data from
 * @cfg: PHY configuration structure to copy data to
 *
 * Helper function to copy data from PHY capabilities data structure
 * to PHY configuration data structure
 */
void ixgbe_copy_phy_caps_to_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
				struct ixgbe_aci_cmd_set_phy_cfg_data *cfg)
{
	if (!caps || !cfg)
		return;

	memset(cfg, 0, sizeof(*cfg));
	cfg->phy_type_low = caps->phy_type_low;
	cfg->phy_type_high = caps->phy_type_high;
	cfg->caps = caps->caps;
	cfg->low_power_ctrl_an = caps->low_power_ctrl_an;
	cfg->eee_cap = caps->eee_cap;
	cfg->eeer_value = caps->eeer_value;
	cfg->link_fec_opt = caps->link_fec_options;
	cfg->module_compliance_enforcement =
		caps->module_compliance_enforcement;
}

/**
 * ixgbe_aci_set_phy_cfg - set PHY configuration
 * @hw: pointer to the HW struct
 * @cfg: structure with PHY configuration data to be set
 *
 * Set the various PHY configuration parameters supported on the Port
 * using ACI command (0x0601).
 * One or more of the Set PHY config parameters may be ignored in an MFP
 * mode as the PF may not have the privilege to set some of the PHY Config
 * parameters.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_phy_cfg(struct ixgbe_hw *hw,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *cfg)
{
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!cfg)
		return IXGBE_ERR_PARAM;

	/* Ensure that only valid bits of cfg->caps can be turned on. */
	if (cfg->caps & ~IXGBE_ACI_PHY_ENA_VALID_MASK) {
		cfg->caps &= IXGBE_ACI_PHY_ENA_VALID_MASK;
	}

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_phy_cfg);
	desc.flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);

	status = ixgbe_aci_send_cmd(hw, &desc, cfg, sizeof(*cfg));

	if (!status)
		hw->phy.curr_user_phy_cfg = *cfg;

	return status;
}

/**
 * ixgbe_aci_set_link_restart_an - set up link and restart AN
 * @hw: pointer to the HW struct
 * @ena_link: if true: enable link, if false: disable link
 *
 * Function sets up the link and restarts the Auto-Negotiation over the link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_link_restart_an(struct ixgbe_hw *hw, bool ena_link)
{
	struct ixgbe_aci_cmd_restart_an *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.restart_an;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_restart_an);

	cmd->cmd_flags = IXGBE_ACI_RESTART_AN_LINK_RESTART;
	if (ena_link)
		cmd->cmd_flags |= IXGBE_ACI_RESTART_AN_LINK_ENABLE;
	else
		cmd->cmd_flags &= ~IXGBE_ACI_RESTART_AN_LINK_ENABLE;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_is_media_cage_present - check if media cage is present
 * @hw: pointer to the HW struct
 *
 * Identify presence of media cage using the ACI command (0x06E0).
 *
 * Return: true if media cage is present, else false. If no cage, then
 * media type is backplane or BASE-T.
 */
static bool ixgbe_is_media_cage_present(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_link_topo *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.get_link_topo;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_topo);

	cmd->addr.topo_params.node_type_ctx =
		(IXGBE_ACI_LINK_TOPO_NODE_CTX_PORT <<
		 IXGBE_ACI_LINK_TOPO_NODE_CTX_S);

	/* set node type */
	cmd->addr.topo_params.node_type_ctx |=
		(IXGBE_ACI_LINK_TOPO_NODE_TYPE_M &
		 IXGBE_ACI_LINK_TOPO_NODE_TYPE_CAGE);

	/* Node type cage can be used to determine if cage is present. If AQC
	 * returns error (ENOENT), then no cage present. If no cage present then
	 * connection type is backplane or BASE-T.
	 */
	return ixgbe_aci_get_netlist_node(hw, cmd, NULL, NULL);
}

/**
 * ixgbe_get_media_type_from_phy_type - Gets media type based on phy type
 * @hw: pointer to the HW struct
 *
 * Try to identify the media type based on the phy type.
 * If more than one media type, the ixgbe_media_type_unknown is returned.
 * First, phy_type_low is checked, then phy_type_high.
 * If none are identified, the ixgbe_media_type_unknown is returned
 *
 * Return: type of a media based on phy type in form of enum.
 */
static enum ixgbe_media_type
ixgbe_get_media_type_from_phy_type(struct ixgbe_hw *hw)
{
	struct ixgbe_link_status *hw_link_info;

	if (!hw)
		return ixgbe_media_type_unknown;

	hw_link_info = &hw->link.link_info;
	if (hw_link_info->phy_type_low && hw_link_info->phy_type_high)
		/* If more than one media type is selected, report unknown */
		return ixgbe_media_type_unknown;

	if (hw_link_info->phy_type_low) {
		/* 1G SGMII is a special case where some DA cable PHYs
		 * may show this as an option when it really shouldn't
		 * be since SGMII is meant to be between a MAC and a PHY
		 * in a backplane. Try to detect this case and handle it
		 */
		if (hw_link_info->phy_type_low == IXGBE_PHY_TYPE_LOW_1G_SGMII &&
		    (hw_link_info->module_type[IXGBE_ACI_MOD_TYPE_IDENT] ==
		    IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE ||
		    hw_link_info->module_type[IXGBE_ACI_MOD_TYPE_IDENT] ==
		    IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE))
			return ixgbe_media_type_da;

		switch (hw_link_info->phy_type_low) {
		case IXGBE_PHY_TYPE_LOW_1000BASE_SX:
		case IXGBE_PHY_TYPE_LOW_1000BASE_LX:
		case IXGBE_PHY_TYPE_LOW_10GBASE_SR:
		case IXGBE_PHY_TYPE_LOW_10GBASE_LR:
		case IXGBE_PHY_TYPE_LOW_25GBASE_SR:
		case IXGBE_PHY_TYPE_LOW_25GBASE_LR:
			return ixgbe_media_type_fiber;
		case IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
		case IXGBE_PHY_TYPE_LOW_25G_AUI_AOC_ACC:
			return ixgbe_media_type_fiber;
		case IXGBE_PHY_TYPE_LOW_100BASE_TX:
		case IXGBE_PHY_TYPE_LOW_1000BASE_T:
		case IXGBE_PHY_TYPE_LOW_2500BASE_T:
		case IXGBE_PHY_TYPE_LOW_5GBASE_T:
		case IXGBE_PHY_TYPE_LOW_10GBASE_T:
		case IXGBE_PHY_TYPE_LOW_25GBASE_T:
			return ixgbe_media_type_copper;
		case IXGBE_PHY_TYPE_LOW_10G_SFI_DA:
		case IXGBE_PHY_TYPE_LOW_25GBASE_CR:
		case IXGBE_PHY_TYPE_LOW_25GBASE_CR_S:
		case IXGBE_PHY_TYPE_LOW_25GBASE_CR1:
			return ixgbe_media_type_da;
		case IXGBE_PHY_TYPE_LOW_25G_AUI_C2C:
			if (ixgbe_is_media_cage_present(hw))
				return ixgbe_media_type_aui;
			return ixgbe_media_type_backplane;
		case IXGBE_PHY_TYPE_LOW_1000BASE_KX:
		case IXGBE_PHY_TYPE_LOW_2500BASE_KX:
		case IXGBE_PHY_TYPE_LOW_2500BASE_X:
		case IXGBE_PHY_TYPE_LOW_5GBASE_KR:
		case IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1:
		case IXGBE_PHY_TYPE_LOW_10G_SFI_C2C:
		case IXGBE_PHY_TYPE_LOW_25GBASE_KR:
		case IXGBE_PHY_TYPE_LOW_25GBASE_KR1:
		case IXGBE_PHY_TYPE_LOW_25GBASE_KR_S:
			return ixgbe_media_type_backplane;
		}
	} else {
		switch (hw_link_info->phy_type_high) {
		case IXGBE_PHY_TYPE_HIGH_10BASE_T:
			return ixgbe_media_type_copper;
		}
	}
	return ixgbe_media_type_unknown;
}

/**
 * ixgbe_update_link_info - update status of the HW network link
 * @hw: pointer to the HW struct
 *
 * Update the status of the HW network link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_update_link_info(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data *pcaps;
	struct ixgbe_link_status *li;
	s32 status;

	if (!hw)
		return IXGBE_ERR_PARAM;

	li = &hw->link.link_info;

	status = ixgbe_aci_get_link_info(hw, true, NULL);
	if (status)
		return status;

	if (li->link_info & IXGBE_ACI_MEDIA_AVAILABLE) {
		pcaps = (struct ixgbe_aci_cmd_get_phy_caps_data *)
			ixgbe_malloc(hw, sizeof(*pcaps));
		if (!pcaps)
			return IXGBE_ERR_OUT_OF_MEM;

		status = ixgbe_aci_get_phy_caps(hw, false,
						IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
						pcaps);

		if (status == IXGBE_SUCCESS)
			memcpy(li->module_type, &pcaps->module_type,
			       sizeof(li->module_type));

		ixgbe_free(hw, pcaps);
	}

	return status;
}

/**
 * ixgbe_get_link_status - get status of the HW network link
 * @hw: pointer to the HW struct
 * @link_up: pointer to bool (true/false = linkup/linkdown)
 *
 * Variable link_up is true if link is up, false if link is down.
 * The variable link_up is invalid if status is non zero. As a
 * result of this call, link status reporting becomes enabled
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_link_status(struct ixgbe_hw *hw, bool *link_up)
{
	s32 status = IXGBE_SUCCESS;

	if (!hw || !link_up)
		return IXGBE_ERR_PARAM;

	if (hw->link.get_link_info) {
		status = ixgbe_update_link_info(hw);
		if (status) {
			return status;
		}
	}

	*link_up = hw->link.link_info.link_info & IXGBE_ACI_LINK_UP;

	return status;
}

/**
 * ixgbe_aci_get_link_info - get the link status
 * @hw: pointer to the HW struct
 * @ena_lse: enable/disable LinkStatusEvent reporting
 * @link: pointer to link status structure - optional
 *
 * Get the current Link Status using ACI command (0x607).
 * The current link can be optionally provided to update
 * the status.
 *
 * Return: the link status of the adapter.
 */
s32 ixgbe_aci_get_link_info(struct ixgbe_hw *hw, bool ena_lse,
			    struct ixgbe_link_status *link)
{
	struct ixgbe_aci_cmd_get_link_status_data link_data = { 0 };
	struct ixgbe_aci_cmd_get_link_status *resp;
	struct ixgbe_link_status *li_old, *li;
	struct ixgbe_fc_info *hw_fc_info;
	enum ixgbe_media_type *hw_media_type;
	struct ixgbe_aci_desc desc;
	bool tx_pause, rx_pause;
	u8 cmd_flags;
	s32 status;

	if (!hw)
		return IXGBE_ERR_PARAM;

	li_old = &hw->link.link_info_old;
	hw_media_type = &hw->phy.media_type;
	li = &hw->link.link_info;
	hw_fc_info = &hw->fc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_status);
	cmd_flags = (ena_lse) ? IXGBE_ACI_LSE_ENA : IXGBE_ACI_LSE_DIS;
	resp = &desc.params.get_link_status;
	resp->cmd_flags = cmd_flags;

	status = ixgbe_aci_send_cmd(hw, &desc, &link_data, sizeof(link_data));

	if (status != IXGBE_SUCCESS)
		return status;

	/* save off old link status information */
	*li_old = *li;

	/* update current link status information */
	li->link_speed = IXGBE_LE16_TO_CPU(link_data.link_speed);
	li->phy_type_low = IXGBE_LE64_TO_CPU(link_data.phy_type_low);
	li->phy_type_high = IXGBE_LE64_TO_CPU(link_data.phy_type_high);
	*hw_media_type = ixgbe_get_media_type_from_phy_type(hw);
	li->link_info = link_data.link_info;
	li->link_cfg_err = link_data.link_cfg_err;
	li->an_info = link_data.an_info;
	li->ext_info = link_data.ext_info;
	li->max_frame_size = IXGBE_LE16_TO_CPU(link_data.max_frame_size);
	li->fec_info = link_data.cfg & IXGBE_ACI_FEC_MASK;
	li->topo_media_conflict = link_data.topo_media_conflict;
	li->pacing = link_data.cfg & (IXGBE_ACI_CFG_PACING_M |
				      IXGBE_ACI_CFG_PACING_TYPE_M);

	/* update fc info */
	tx_pause = !!(link_data.an_info & IXGBE_ACI_LINK_PAUSE_TX);
	rx_pause = !!(link_data.an_info & IXGBE_ACI_LINK_PAUSE_RX);
	if (tx_pause && rx_pause)
		hw_fc_info->current_mode = ixgbe_fc_full;
	else if (tx_pause)
		hw_fc_info->current_mode = ixgbe_fc_tx_pause;
	else if (rx_pause)
		hw_fc_info->current_mode = ixgbe_fc_rx_pause;
	else
		hw_fc_info->current_mode = ixgbe_fc_none;

	li->lse_ena = !!(resp->cmd_flags & IXGBE_ACI_LSE_IS_ENABLED);

	/* save link status information */
	if (link)
		*link = *li;

	/* flag cleared so calling functions don't call AQ again */
	hw->link.get_link_info = false;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_set_event_mask - set event mask
 * @hw: pointer to the HW struct
 * @port_num: port number of the physical function
 * @mask: event mask to be set
 *
 * Set the event mask using ACI command (0x0613).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_event_mask(struct ixgbe_hw *hw, u8 port_num, u16 mask)
{
	struct ixgbe_aci_cmd_set_event_mask *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.set_event_mask;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_event_mask);

	cmd->event_mask = IXGBE_CPU_TO_LE16(mask);
	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_configure_lse - enable/disable link status events
 * @hw: pointer to the HW struct
 * @activate: bool value deciding if lse should be enabled nor disabled
 * @mask: event mask to be set; a set bit means deactivation of the
 * corresponding event
 *
 * Set the event mask and then enable or disable link status events
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_configure_lse(struct ixgbe_hw *hw, bool activate, u16 mask)
{
	s32 rc;

	rc = ixgbe_aci_set_event_mask(hw, (u8)hw->bus.func, mask);
	if (rc) {
		return rc;
	}

	/* Enabling link status events generation by fw */
	rc = ixgbe_aci_get_link_info(hw, activate, NULL);
	if (rc) {
		return rc;
	}
	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_get_netlist_node - get a node handle
 * @hw: pointer to the hw struct
 * @cmd: get_link_topo AQ structure
 * @node_part_number: output node part number if node found
 * @node_handle: output node handle parameter if node found
 *
 * Get the netlist node and assigns it to
 * the provided handle using ACI command (0x06E0).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_netlist_node(struct ixgbe_hw *hw,
			       struct ixgbe_aci_cmd_get_link_topo *cmd,
			       u8 *node_part_number, u16 *node_handle)
{
	struct ixgbe_aci_desc desc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_topo);
	desc.params.get_link_topo = *cmd;

	if (ixgbe_aci_send_cmd(hw, &desc, NULL, 0))
		return IXGBE_ERR_NOT_SUPPORTED;

	if (node_handle)
		*node_handle =
			IXGBE_LE16_TO_CPU(desc.params.get_link_topo.addr.handle);
	if (node_part_number)
		*node_part_number = desc.params.get_link_topo.node_part_num;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_get_netlist_node_pin - get a node pin handle
 * @hw: pointer to the hw struct
 * @cmd: get_link_topo_pin AQ structure
 * @node_handle: output node handle parameter if node found
 *
 * Get the netlist node pin and assign it to
 * the provided handle using ACI command (0x06E1).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_netlist_node_pin(struct ixgbe_hw *hw,
				   struct ixgbe_aci_cmd_get_link_topo_pin *cmd,
				   u16 *node_handle)
{
	struct ixgbe_aci_desc desc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_topo_pin);
	desc.params.get_link_topo_pin = *cmd;

	if (ixgbe_aci_send_cmd(hw, &desc, NULL, 0))
		return IXGBE_ERR_NOT_SUPPORTED;

	if (node_handle)
		*node_handle =
			IXGBE_LE16_TO_CPU(desc.params.get_link_topo_pin.addr.handle);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_find_netlist_node - find a node handle
 * @hw: pointer to the hw struct
 * @node_type_ctx: type of netlist node to look for
 * @node_part_number: node part number to look for
 * @node_handle: output parameter if node found - optional
 *
 * Find and return the node handle for a given node type and part number in the
 * netlist. When found IXGBE_SUCCESS is returned, IXGBE_ERR_NOT_SUPPORTED
 * otherwise. If @node_handle provided, it would be set to found node handle.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_find_netlist_node(struct ixgbe_hw *hw, u8 node_type_ctx,
			    u8 node_part_number, u16 *node_handle)
{
	struct ixgbe_aci_cmd_get_link_topo cmd;
	u8 rec_node_part_number;
	u16 rec_node_handle;
	s32 status;
	u8 idx;

	for (idx = 0; idx < IXGBE_MAX_NETLIST_SIZE; idx++) {
		memset(&cmd, 0, sizeof(cmd));

		cmd.addr.topo_params.node_type_ctx =
			(node_type_ctx << IXGBE_ACI_LINK_TOPO_NODE_TYPE_S);
		cmd.addr.topo_params.index = idx;

		status = ixgbe_aci_get_netlist_node(hw, &cmd,
						    &rec_node_part_number,
						    &rec_node_handle);
		if (status)
			return status;

		if (rec_node_part_number == node_part_number) {
			if (node_handle)
				*node_handle = rec_node_handle;
			return IXGBE_SUCCESS;
		}
	}

	return IXGBE_ERR_NOT_SUPPORTED;
}

/**
 * ixgbe_aci_sff_eeprom - read/write SFF EEPROM
 * @hw: pointer to the HW struct
 * @lport: bits [7:0] = logical port, bit [8] = logical port valid
 * @bus_addr: I2C bus address of the eeprom (typically 0xA0, 0=topo default)
 * @mem_addr: I2C offset. lower 8 bits for address, 8 upper bits zero padding.
 * @page: QSFP page
 * @page_bank_ctrl: configuration of SFF/CMIS paging and banking control
 * @data: pointer to data buffer to be read/written to the I2C device.
 * @length: 1-16 for read, 1 for write.
 * @write: 0 read, 1 for write.
 *
 * Read/write SFF EEPROM using ACI command (0x06EE).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_sff_eeprom(struct ixgbe_hw *hw, u16 lport, u8 bus_addr,
			 u16 mem_addr, u8 page, u8 page_bank_ctrl, u8 *data,
			 u8 length, bool write)
{
	struct ixgbe_aci_cmd_sff_eeprom *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!data || (mem_addr & 0xff00))
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_sff_eeprom);
	cmd = &desc.params.read_write_sff_param;
	desc.flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);
	cmd->lport_num = (u8)(lport & 0xff);
	cmd->lport_num_valid = (u8)((lport >> 8) & 0x01);
	cmd->i2c_bus_addr = IXGBE_CPU_TO_LE16(((bus_addr >> 1) &
					 IXGBE_ACI_SFF_I2CBUS_7BIT_M) |
					((page_bank_ctrl <<
					  IXGBE_ACI_SFF_PAGE_BANK_CTRL_S) &
					 IXGBE_ACI_SFF_PAGE_BANK_CTRL_M));
	cmd->i2c_offset = IXGBE_CPU_TO_LE16(mem_addr & 0xff);
	cmd->module_page = page;
	if (write)
		cmd->i2c_bus_addr |= IXGBE_CPU_TO_LE16(IXGBE_ACI_SFF_IS_WRITE);

	status = ixgbe_aci_send_cmd(hw, &desc, data, length);
	return status;
}

/**
 * ixgbe_aci_get_internal_data - get internal FW/HW data
 * @hw: pointer to the hardware structure
 * @cluster_id: specific cluster to dump
 * @table_id: table ID within cluster
 * @start: index of line in the block to read
 * @buf: dump buffer
 * @buf_size: dump buffer size
 * @ret_buf_size: return buffer size (returned by FW)
 * @ret_next_cluster: next cluster to read (returned by FW)
 * @ret_next_table: next block to read (returned by FW)
 * @ret_next_index: next index to read (returned by FW)
 *
 * Get internal FW/HW data using ACI command (0xFF08) for debug purposes.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_internal_data(struct ixgbe_hw *hw, u16 cluster_id,
				u16 table_id, u32 start, void *buf,
				u16 buf_size, u16 *ret_buf_size,
				u16 *ret_next_cluster, u16 *ret_next_table,
				u32 *ret_next_index)
{
	struct ixgbe_aci_cmd_debug_dump_internals *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.debug_dump;

	if (buf_size == 0 || !buf)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc,
					ixgbe_aci_opc_debug_dump_internals);

	cmd->cluster_id = IXGBE_CPU_TO_LE16(cluster_id);
	cmd->table_id = IXGBE_CPU_TO_LE16(table_id);
	cmd->idx = IXGBE_CPU_TO_LE32(start);

	status = ixgbe_aci_send_cmd(hw, &desc, buf, buf_size);

	if (!status) {
		if (ret_buf_size)
			*ret_buf_size = IXGBE_LE16_TO_CPU(desc.datalen);
		if (ret_next_cluster)
			*ret_next_cluster = IXGBE_LE16_TO_CPU(cmd->cluster_id);
		if (ret_next_table)
			*ret_next_table = IXGBE_LE16_TO_CPU(cmd->table_id);
		if (ret_next_index)
			*ret_next_index = IXGBE_LE32_TO_CPU(cmd->idx);
	}

	return status;
}

/**
 * ixgbe_get_media_type_E610 - Gets media type
 * @hw: pointer to the HW struct
 *
 * In order to get the media type, the function gets PHY
 * capabilities and later on use them to identify the PHY type
 * checking phy_type_high and phy_type_low.
 *
 * Return: the type of media in form of ixgbe_media_type enum
 * or ixgbe_media_type_unknown in case of an error.
 */
enum ixgbe_media_type ixgbe_get_media_type_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	u64 phy_mask = 0;
	s32 rc;
	u8 i;

	rc = ixgbe_update_link_info(hw);
	if (rc) {
		return ixgbe_media_type_unknown;
	}

	/* If there is no link but PHY (dongle) is available SW should use
	 * Get PHY Caps admin command instead of Get Link Status, find most
	 * significant bit that is set in PHY types reported by the command
	 * and use it to discover media type.
	 */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_LINK_UP) &&
	    (hw->link.link_info.link_info & IXGBE_ACI_MEDIA_AVAILABLE)) {
		/* Get PHY Capabilities */
		rc = ixgbe_aci_get_phy_caps(hw, false,
					    IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
					    &pcaps);
		if (rc) {
			return ixgbe_media_type_unknown;
		}

		/* Check if there is some bit set in phy_type_high */
		for (i = 64; i > 0; i--) {
			phy_mask = (u64)((u64)1 << (i - 1));
			if ((pcaps.phy_type_high & phy_mask) != 0) {
				/* If any bit is set treat it as PHY type */
				hw->link.link_info.phy_type_high = phy_mask;
				hw->link.link_info.phy_type_low = 0;
				break;
			}
			phy_mask = 0;
		}

		/* If nothing found in phy_type_high search in phy_type_low */
		if (phy_mask == 0) {
			for (i = 64; i > 0; i--) {
				phy_mask = (u64)((u64)1 << (i - 1));
				if ((pcaps.phy_type_low & phy_mask) != 0) {
					/* If any bit is set treat it as PHY type */
					hw->link.link_info.phy_type_high = 0;
					hw->link.link_info.phy_type_low = phy_mask;
					break;
				}
			}
		}

		/* Based on search above try to discover media type */
		hw->phy.media_type = ixgbe_get_media_type_from_phy_type(hw);
	}

	return hw->phy.media_type;
}

/**
 * ixgbe_setup_link_E610 - Set up link
 * @hw: pointer to hardware structure
 * @speed: new link speed
 * @autoneg_wait: true when waiting for completion is needed
 *
 * Set up the link with the specified speed.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			  bool autoneg_wait)
{

	/* Simply request FW to perform proper PHY setup */
	return hw->phy.ops.setup_link_speed(hw, speed, autoneg_wait);
}

/**
 * ixgbe_check_link_E610 - Determine link and speed status
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: true when link is up
 * @link_up_wait_to_complete: bool used to wait for link up or not
 *
 * Determine if the link is up and the current link speed
 * using ACI command (0x0607).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_check_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			  bool *link_up, bool link_up_wait_to_complete)
{
	s32 rc;
	u32 i;

	if (!speed || !link_up)
		return IXGBE_ERR_PARAM;

	/* Set get_link_info flag to ensure that fresh
	 * link information will be obtained from FW
	 * by sending Get Link Status admin command. */
	hw->link.get_link_info = true;

	/* Update link information in adapter context. */
	rc = ixgbe_get_link_status(hw, link_up);
	if (rc)
		return rc;

	/* Wait for link up if it was requested. */
	if (link_up_wait_to_complete && *link_up == false) {
		for (i = 0; i < hw->mac.max_link_up_time; i++) {
			msec_delay(100);
			hw->link.get_link_info = true;
			rc = ixgbe_get_link_status(hw, link_up);
			if (rc)
				return rc;
			if (*link_up)
				break;
		}
	}

	/* Use link information in adapter context updated by the call
	 * to ixgbe_get_link_status() to determine current link speed.
	 * Link speed information is valid only when link up was
	 * reported by FW. */
	if (*link_up) {
		switch (hw->link.link_info.link_speed) {
		case IXGBE_ACI_LINK_SPEED_10MB:
			*speed = IXGBE_LINK_SPEED_10_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_100MB:
			*speed = IXGBE_LINK_SPEED_100_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_1000MB:
			*speed = IXGBE_LINK_SPEED_1GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_2500MB:
			*speed = IXGBE_LINK_SPEED_2_5GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_5GB:
			*speed = IXGBE_LINK_SPEED_5GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_10GB:
			*speed = IXGBE_LINK_SPEED_10GB_FULL;
			break;
		default:
			*speed = IXGBE_LINK_SPEED_UNKNOWN;
			break;
		}
	} else {
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_get_link_capabilities_E610 - Determine link capabilities
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @autoneg: true when autoneg or autotry is enabled
 *
 * Determine speed and AN parameters of a link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_link_capabilities_E610(struct ixgbe_hw *hw,
				     ixgbe_link_speed *speed,
				     bool *autoneg)
{

	if (!speed || !autoneg)
		return IXGBE_ERR_PARAM;

	*autoneg = true;
	*speed = hw->phy.speeds_supported;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_cfg_phy_fc - Configure PHY Flow Control (FC) data based on FC mode
 * @hw: pointer to hardware structure
 * @cfg: PHY configuration data to set FC mode
 * @req_mode: FC mode to configure
 *
 * Configures PHY Flow Control according to the provided configuration.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_cfg_phy_fc(struct ixgbe_hw *hw,
		     struct ixgbe_aci_cmd_set_phy_cfg_data *cfg,
		     enum ixgbe_fc_mode req_mode)
{
	struct ixgbe_aci_cmd_get_phy_caps_data* pcaps = NULL;
	s32 status = IXGBE_SUCCESS;
	u8 pause_mask = 0x0;

	if (!cfg)
		return IXGBE_ERR_PARAM;

	switch (req_mode) {
	case ixgbe_fc_auto:
	{
		pcaps = (struct ixgbe_aci_cmd_get_phy_caps_data *)
			ixgbe_malloc(hw, sizeof(*pcaps));
		if (!pcaps) {
			status = IXGBE_ERR_OUT_OF_MEM;
			goto out;
		}

		/* Query the value of FC that both the NIC and the attached
		 * media can do. */
		status = ixgbe_aci_get_phy_caps(hw, false,
			IXGBE_ACI_REPORT_TOPO_CAP_MEDIA, pcaps);
		if (status)
			goto out;

		pause_mask |= pcaps->caps & IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= pcaps->caps & IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;

		break;
	}
	case ixgbe_fc_full:
		pause_mask |= IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;
		break;
	case ixgbe_fc_rx_pause:
		pause_mask |= IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;
		break;
	case ixgbe_fc_tx_pause:
		pause_mask |= IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		break;
	default:
		break;
	}

	/* clear the old pause settings */
	cfg->caps &= ~(IXGBE_ACI_PHY_EN_TX_LINK_PAUSE |
		IXGBE_ACI_PHY_EN_RX_LINK_PAUSE);

	/* set the new capabilities */
	cfg->caps |= pause_mask;

out:
	if (pcaps)
		ixgbe_free(hw, pcaps);
	return status;
}

/**
 * ixgbe_setup_fc_E610 - Set up flow control
 * @hw: pointer to hardware structure
 *
 * Set up flow control. This has to be done during init time.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_fc_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data cfg = { 0 };
	s32 status;

	/* Get the current PHY config */
	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &pcaps);
	if (status)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&pcaps, &cfg);

	/* Configure the set PHY data */
	status = ixgbe_cfg_phy_fc(hw, &cfg, hw->fc.requested_mode);
	if (status)
		return status;

	/* If the capabilities have changed, then set the new config */
	if (cfg.caps != pcaps.caps) {
		cfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

		status = ixgbe_aci_set_phy_cfg(hw, &cfg);
		if (status)
			return status;
	}

	return status;
}

/**
 * ixgbe_fc_autoneg_E610 - Configure flow control
 * @hw: pointer to hardware structure
 *
 * Configure Flow Control.
 */
void ixgbe_fc_autoneg_E610(struct ixgbe_hw *hw)
{
	s32 status;

	/* Get current link status.
	 * Current FC mode will be stored in the hw context. */
	status = ixgbe_aci_get_link_info(hw, false, NULL);
	if (status) {
		goto out;
	}

	/* Check if the link is up */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_LINK_UP)) {
		status = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

	/* Check if auto-negotiation has completed */
	if (!(hw->link.link_info.an_info & IXGBE_ACI_AN_COMPLETED)) {
		status = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

out:
	if (status == IXGBE_SUCCESS) {
		hw->fc.fc_was_autonegged = true;
	} else {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
	}
}

/**
 * ixgbe_disable_rx_E610 - Disable RX unit
 * @hw: pointer to hardware structure
 *
 * Disable RX DMA unit on E610 with use of ACI command (0x000C).
 *
 * Return: the exit code of the operation.
 */
void ixgbe_disable_rx_E610(struct ixgbe_hw *hw)
{
	u32 rxctrl;

	DEBUGFUNC("ixgbe_disable_rx_E610");

	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (rxctrl & IXGBE_RXCTRL_RXEN) {
		u32 pfdtxgswc;
		s32 status;

		pfdtxgswc = IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC);
		if (pfdtxgswc & IXGBE_PFDTXGSWC_VT_LBEN) {
			pfdtxgswc &= ~IXGBE_PFDTXGSWC_VT_LBEN;
			IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, pfdtxgswc);
			hw->mac.set_lben = true;
		} else {
			hw->mac.set_lben = false;
		}

		status = ixgbe_aci_disable_rxen(hw);

		/* If we fail - disable RX using register write */
		if (status) {
			rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
			if (rxctrl & IXGBE_RXCTRL_RXEN) {
				rxctrl &= ~IXGBE_RXCTRL_RXEN;
				IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl);
			}
		}
	}
}

/**
 * ixgbe_init_phy_ops_E610 - PHY specific init
 * @hw: pointer to hardware structure
 *
 * Initialize any function pointers that were not able to be
 * set during init_shared_code because the PHY type was not known.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_init_phy_ops_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val;

	phy->ops.identify_sfp = ixgbe_identify_module_E610;
	phy->ops.read_reg = NULL; /* PHY reg access is not required */
	phy->ops.write_reg = NULL;
	phy->ops.read_reg_mdi = NULL;
	phy->ops.write_reg_mdi = NULL;
	phy->ops.setup_link = ixgbe_setup_phy_link_E610;
	phy->ops.get_firmware_version = ixgbe_get_phy_firmware_version_E610;
	phy->ops.read_i2c_byte = NULL; /* disabled for E610 */
	phy->ops.write_i2c_byte = NULL; /* disabled for E610 */
	phy->ops.read_i2c_sff8472 = ixgbe_read_i2c_sff8472_E610;
	phy->ops.read_i2c_eeprom = ixgbe_read_i2c_eeprom_E610;
	phy->ops.write_i2c_eeprom = ixgbe_write_i2c_eeprom_E610;
	phy->ops.i2c_bus_clear = NULL; /* do not use generic implementation  */
	phy->ops.check_overtemp = ixgbe_check_overtemp_E610;
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper)
		phy->ops.set_phy_power = ixgbe_set_phy_power_E610;
	else
		phy->ops.set_phy_power = NULL;
	phy->ops.enter_lplu = ixgbe_enter_lplu_E610;
	phy->ops.handle_lasi = NULL; /* no implementation for E610 */
	phy->ops.read_i2c_byte_unlocked = NULL; /* disabled for E610 */
	phy->ops.write_i2c_byte_unlocked = NULL; /* disabled for E610 */

	/* TODO: Set functions pointers based on device ID */

	/* Identify the PHY */
	ret_val = phy->ops.identify(hw);
	if (ret_val != IXGBE_SUCCESS)
		return ret_val;

	/* TODO: Set functions pointers based on PHY type */

	return ret_val;
}

/**
 * ixgbe_identify_phy_E610 - Identify PHY
 * @hw: pointer to hardware structure
 *
 * Determine PHY type, supported speeds and PHY ID.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_identify_phy_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	s32 rc;

	/* Set PHY type */
	hw->phy.type = ixgbe_phy_fw;

	rc = ixgbe_aci_get_phy_caps(hw, false, IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
				    &pcaps);
	if (rc)
		return rc;

	if (!(pcaps.module_compliance_enforcement &
	      IXGBE_ACI_MOD_ENFORCE_STRICT_MODE)) {
		/* Handle lenient mode */
		rc = ixgbe_aci_get_phy_caps(hw, false,
					    IXGBE_ACI_REPORT_TOPO_CAP_NO_MEDIA,
					    &pcaps);
		if (rc)
			return rc;
	}

	/* Determine supported speeds */
	hw->phy.speeds_supported = IXGBE_LINK_SPEED_UNKNOWN;

	if (pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10BASE_T ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10M_SGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_10_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_100BASE_TX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_100M_SGMII ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_100M_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_100_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_T  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_SX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_LX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_KX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1G_SGMII    ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_1G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_1GB_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_T   ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_X   ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_KX  ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_2500M_SGMII ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_2500M_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_2_5GB_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_5GBASE_T  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_5GBASE_KR ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_5G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_5GB_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_T       ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_DA      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_SR      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_LR      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_C2C     ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_10GB_FULL;

	/* Initialize autoneg speeds */
	if (!hw->phy.autoneg_advertised)
		hw->phy.autoneg_advertised = hw->phy.speeds_supported;

	/* Set PHY ID */
	memcpy(&hw->phy.id, pcaps.phy_id_oui, sizeof(u32));

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_identify_module_E610 - Identify SFP module type
 * @hw: pointer to hardware structure
 *
 * Identify the SFP module type.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_identify_module_E610(struct ixgbe_hw *hw)
{
	bool media_available;
	u8 module_type;
	s32 rc;

	rc = ixgbe_update_link_info(hw);
	if (rc)
		goto err;

	media_available =
		(hw->link.link_info.link_info &
		 IXGBE_ACI_MEDIA_AVAILABLE) ? true : false;

	if (media_available) {
		hw->phy.sfp_type = ixgbe_sfp_type_unknown;

		/* Get module type from hw context updated by ixgbe_update_link_info() */
		module_type = hw->link.link_info.module_type[IXGBE_ACI_MOD_TYPE_IDENT];

		if ((module_type & IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE) ||
		    (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE)) {
			hw->phy.sfp_type = ixgbe_sfp_type_da_cu;
		} else if (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_SR) {
			hw->phy.sfp_type = ixgbe_sfp_type_sr;
		} else if ((module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LR) ||
			   (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LRM)) {
			hw->phy.sfp_type = ixgbe_sfp_type_lr;
		}
		rc = IXGBE_SUCCESS;
	} else {
		hw->phy.sfp_type = ixgbe_sfp_type_not_present;
		rc = IXGBE_ERR_SFP_NOT_PRESENT;
	}
err:
	return rc;
}

/**
 * ixgbe_setup_phy_link_E610 - Sets up firmware-controlled PHYs
 * @hw: pointer to hardware structure
 *
 * Set the parameters for the firmware-controlled PHYs.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_phy_link_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	struct ixgbe_aci_cmd_set_phy_cfg_data pcfg;
	u8 rmode = IXGBE_ACI_REPORT_ACTIVE_CFG;
	s32 rc;

	rc = ixgbe_aci_get_link_info(hw, false, NULL);
	if (rc) {
		goto err;
	}

	/* If media is not available get default config */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_MEDIA_AVAILABLE))
		rmode = IXGBE_ACI_REPORT_DFLT_CFG;

	rc = ixgbe_aci_get_phy_caps(hw, false, rmode, &pcaps);
	if (rc) {
		goto err;
	}

	ixgbe_copy_phy_caps_to_cfg(&pcaps, &pcfg);

	/* Set default PHY types for a given speed */
	pcfg.phy_type_low = 0;
	pcfg.phy_type_high = 0;

	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_10_FULL) {
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10BASE_T;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10M_SGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_100_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_100BASE_TX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_100M_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_100M_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_1GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_SX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_LX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_KX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1G_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_1G_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_2_5GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_X;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_KX;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_2500M_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_2500M_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_5GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_5GBASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_5GBASE_KR;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_5G_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_10GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_DA;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_SR;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_LR;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_C2C;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10G_USXGMII;
	}

	/* Mask the set values to avoid requesting unsupported link types */
	pcfg.phy_type_low &= pcaps.phy_type_low;
	pcfg.phy_type_high &= pcaps.phy_type_high;

	if (pcfg.phy_type_high != pcaps.phy_type_high ||
	    pcfg.phy_type_low != pcaps.phy_type_low ||
	    pcfg.caps != pcaps.caps) {
		pcfg.caps |= IXGBE_ACI_PHY_ENA_LINK;
		pcfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

		rc = ixgbe_aci_set_phy_cfg(hw, &pcfg);
	}

err:
	return rc;
}

/**
 * ixgbe_get_phy_firmware_version_E610 - Gets the PHY Firmware Version
 * @hw: pointer to hardware structure
 * @firmware_version: pointer to the PHY Firmware Version
 *
 * Determines PHY FW version based on response to Get PHY Capabilities
 * admin command (0x0600).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_phy_firmware_version_E610(struct ixgbe_hw *hw,
					u16 *firmware_version)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	s32 status;

	if (!firmware_version)
		return IXGBE_ERR_PARAM;

	status = ixgbe_aci_get_phy_caps(hw, false,
					IXGBE_ACI_REPORT_ACTIVE_CFG,
					&pcaps);
	if (status)
		return status;

	/* TODO: determine which bytes of the 8-byte phy_fw_ver
	 * field should be written to the 2-byte firmware_version
	 * output argument. */
	memcpy(firmware_version, pcaps.phy_fw_ver, sizeof(u16));

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_i2c_sff8472_E610 - Reads 8 bit word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: byte offset at address 0xA2
 * @sff8472_data: value read
 *
 * Performs byte read operation from SFP module's SFF-8472 data over I2C.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_read_i2c_sff8472_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 *sff8472_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR2,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    sff8472_data, 1, false);
}

/**
 * ixgbe_read_i2c_eeprom_E610 - Reads 8 bit EEPROM word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: EEPROM byte offset to read
 * @eeprom_data: value read
 *
 * Performs byte read operation from SFP module's EEPROM over I2C interface.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_read_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
			       u8 *eeprom_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    eeprom_data, 1, false);
}

/**
 * ixgbe_write_i2c_eeprom_E610 - Writes 8 bit EEPROM word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: EEPROM byte offset to write
 * @eeprom_data: value to write
 *
 * Performs byte write operation to SFP module's EEPROM over I2C interface.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_write_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 eeprom_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    &eeprom_data, 1, true);
}

/**
 * ixgbe_check_overtemp_E610 - Check firmware-controlled PHYs for overtemp
 * @hw: pointer to hardware structure
 *
 * Get the link status and check if the PHY temperature alarm detected.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_check_overtemp_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_link_status_data link_data = { 0 };
	struct ixgbe_aci_cmd_get_link_status *resp;
	struct ixgbe_aci_desc desc;
	s32 status = IXGBE_SUCCESS;

	if (!hw)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_status);
	resp = &desc.params.get_link_status;
	resp->cmd_flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_LSE_NOP);

	status = ixgbe_aci_send_cmd(hw, &desc, &link_data, sizeof(link_data));
	if (status != IXGBE_SUCCESS)
		return status;

	if (link_data.ext_info & IXGBE_ACI_LINK_PHY_TEMP_ALARM) {
		ERROR_REPORT1(IXGBE_ERROR_CAUTION,
			      "PHY Temperature Alarm detected");
		status = IXGBE_ERR_OVERTEMP;
	}

	return status;
}

/**
 * ixgbe_set_phy_power_E610 - Control power for copper PHY
 * @hw: pointer to hardware structure
 * @on: true for on, false for off
 *
 * Set the power on/off of the PHY
 * by getting its capabilities and setting the appropriate
 * configuration parameters.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_set_phy_power_E610(struct ixgbe_hw *hw, bool on)
{
	struct ixgbe_aci_cmd_get_phy_caps_data phy_caps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data phy_cfg = { 0 };
	s32 status;

	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &phy_caps);
	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&phy_caps, &phy_cfg);

	if (on) {
		phy_cfg.caps &= ~IXGBE_ACI_PHY_ENA_LOW_POWER;
	} else {
		phy_cfg.caps |= IXGBE_ACI_PHY_ENA_LOW_POWER;
	}

	/* PHY is already in requested power mode */
	if (phy_caps.caps == phy_cfg.caps)
		return IXGBE_SUCCESS;

	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_LINK;
	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

	status = ixgbe_aci_set_phy_cfg(hw, &phy_cfg);

	return status;
}

/**
 * ixgbe_enter_lplu_E610 - Transition to low power states
 * @hw: pointer to hardware structure
 *
 * Configures Low Power Link Up on transition to low power states
 * (from D0 to non-D0). Link is required to enter LPLU so avoid resetting the
 * X557 PHY immediately prior to entering LPLU.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_enter_lplu_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data phy_caps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data phy_cfg = { 0 };
	s32 status;

	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &phy_caps);
	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&phy_caps, &phy_cfg);

	phy_cfg.low_power_ctrl_an |= IXGBE_ACI_PHY_EN_D3COLD_LOW_POWER_AUTONEG;

	status = ixgbe_aci_set_phy_cfg(hw, &phy_cfg);

	return status;
}
