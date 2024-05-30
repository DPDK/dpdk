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
