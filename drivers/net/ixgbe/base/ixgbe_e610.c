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
