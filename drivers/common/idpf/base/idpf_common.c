/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include "idpf_type.h"
#include "idpf_prototype.h"
#include <virtchnl.h>


/**
 * idpf_set_mac_type - Sets MAC type
 * @hw: pointer to the HW structure
 *
 * This function sets the mac type of the adapter based on the
 * vendor ID and device ID stored in the hw structure.
 */
int idpf_set_mac_type(struct idpf_hw *hw)
{
	int status = 0;

	DEBUGFUNC("Set MAC type\n");

	if (hw->vendor_id == IDPF_INTEL_VENDOR_ID) {
		switch (hw->device_id) {
		case IDPF_DEV_ID_PF:
			hw->mac.type = IDPF_MAC_PF;
			break;
		case IDPF_DEV_ID_VF:
			hw->mac.type = IDPF_MAC_VF;
			break;
		default:
			hw->mac.type = IDPF_MAC_GENERIC;
			break;
		}
	} else {
		status = -ENODEV;
	}

	DEBUGOUT2("Setting MAC type found mac: %d, returns: %d\n",
		  hw->mac.type, status);
	return status;
}

/**
 *  idpf_init_hw - main initialization routine
 *  @hw: pointer to the hardware structure
 *  @ctlq_size: struct to pass ctlq size data
 */
int idpf_init_hw(struct idpf_hw *hw, struct idpf_ctlq_size ctlq_size)
{
	struct idpf_ctlq_create_info *q_info;
	int status = 0;
	struct idpf_ctlq_info *cq = NULL;

	/* Setup initial control queues */
	q_info = (struct idpf_ctlq_create_info *)
		 idpf_calloc(hw, 2, sizeof(struct idpf_ctlq_create_info));
	if (!q_info)
		return -ENOMEM;

	q_info[0].type             = IDPF_CTLQ_TYPE_MAILBOX_TX;
	q_info[0].buf_size         = ctlq_size.asq_buf_size;
	q_info[0].len              = ctlq_size.asq_ring_size;
	q_info[0].id               = -1; /* default queue */

	if (hw->mac.type == IDPF_MAC_PF) {
		q_info[0].reg.head         = PF_FW_ATQH;
		q_info[0].reg.tail         = PF_FW_ATQT;
		q_info[0].reg.len          = PF_FW_ATQLEN;
		q_info[0].reg.bah          = PF_FW_ATQBAH;
		q_info[0].reg.bal          = PF_FW_ATQBAL;
		q_info[0].reg.len_mask     = PF_FW_ATQLEN_ATQLEN_M;
		q_info[0].reg.len_ena_mask = PF_FW_ATQLEN_ATQENABLE_M;
		q_info[0].reg.head_mask    = PF_FW_ATQH_ATQH_M;
	} else {
		q_info[0].reg.head         = VF_ATQH;
		q_info[0].reg.tail         = VF_ATQT;
		q_info[0].reg.len          = VF_ATQLEN;
		q_info[0].reg.bah          = VF_ATQBAH;
		q_info[0].reg.bal          = VF_ATQBAL;
		q_info[0].reg.len_mask     = VF_ATQLEN_ATQLEN_M;
		q_info[0].reg.len_ena_mask = VF_ATQLEN_ATQENABLE_M;
		q_info[0].reg.head_mask    = VF_ATQH_ATQH_M;
	}

	q_info[1].type             = IDPF_CTLQ_TYPE_MAILBOX_RX;
	q_info[1].buf_size         = ctlq_size.arq_buf_size;
	q_info[1].len              = ctlq_size.arq_ring_size;
	q_info[1].id               = -1; /* default queue */

	if (hw->mac.type == IDPF_MAC_PF) {
		q_info[1].reg.head         = PF_FW_ARQH;
		q_info[1].reg.tail         = PF_FW_ARQT;
		q_info[1].reg.len          = PF_FW_ARQLEN;
		q_info[1].reg.bah          = PF_FW_ARQBAH;
		q_info[1].reg.bal          = PF_FW_ARQBAL;
		q_info[1].reg.len_mask     = PF_FW_ARQLEN_ARQLEN_M;
		q_info[1].reg.len_ena_mask = PF_FW_ARQLEN_ARQENABLE_M;
		q_info[1].reg.head_mask    = PF_FW_ARQH_ARQH_M;
	} else {
		q_info[1].reg.head         = VF_ARQH;
		q_info[1].reg.tail         = VF_ARQT;
		q_info[1].reg.len          = VF_ARQLEN;
		q_info[1].reg.bah          = VF_ARQBAH;
		q_info[1].reg.bal          = VF_ARQBAL;
		q_info[1].reg.len_mask     = VF_ARQLEN_ARQLEN_M;
		q_info[1].reg.len_ena_mask = VF_ARQLEN_ARQENABLE_M;
		q_info[1].reg.head_mask    = VF_ARQH_ARQH_M;
	}

	status = idpf_ctlq_init(hw, 2, q_info);
	if (status) {
		/* TODO return error */
		idpf_free(hw, q_info);
		return status;
	}

	LIST_FOR_EACH_ENTRY(cq, &hw->cq_list_head, idpf_ctlq_info, cq_list) {
		if (cq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_TX)
			hw->asq = cq;
		else if (cq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_RX)
			hw->arq = cq;
	}

	/* TODO hardcode a mac addr for now */
	hw->mac.addr[0] = 0x00;
	hw->mac.addr[1] = 0x00;
	hw->mac.addr[2] = 0x00;
	hw->mac.addr[3] = 0x00;
	hw->mac.addr[4] = 0x03;
	hw->mac.addr[5] = 0x14;

	idpf_free(hw, q_info);

	return 0;
}

/**
 * idpf_send_msg_to_cp
 * @hw: pointer to the hardware structure
 * @v_opcode: opcodes for VF-PF communication
 * @v_retval: return error code
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 * @cmd_details: pointer to command details
 *
 * Send message to CP. By default, this message
 * is sent asynchronously, i.e. idpf_asq_send_command() does not wait for
 * completion before returning.
 */
int idpf_send_msg_to_cp(struct idpf_hw *hw, int v_opcode,
			int v_retval, u8 *msg, u16 msglen)
{
	struct idpf_ctlq_msg ctlq_msg = { 0 };
	struct idpf_dma_mem dma_mem = { 0 };
	int status;

	ctlq_msg.opcode = idpf_mbq_opc_send_msg_to_pf;
	ctlq_msg.func_id = 0;
	ctlq_msg.data_len = msglen;
	ctlq_msg.cookie.mbx.chnl_retval = v_retval;
	ctlq_msg.cookie.mbx.chnl_opcode = v_opcode;

	if (msglen > 0) {
		dma_mem.va = (struct idpf_dma_mem *)
			  idpf_alloc_dma_mem(hw, &dma_mem, msglen);
		if (!dma_mem.va)
			return -ENOMEM;

		idpf_memcpy(dma_mem.va, msg, msglen, IDPF_NONDMA_TO_DMA);
		ctlq_msg.ctx.indirect.payload = &dma_mem;
	}
	status = idpf_ctlq_send(hw, hw->asq, 1, &ctlq_msg);

	if (dma_mem.va)
		idpf_free_dma_mem(hw, &dma_mem);

	return status;
}

/**
 *  idpf_asq_done - check if FW has processed the Admin Send Queue
 *  @hw: pointer to the hw struct
 *
 *  Returns true if the firmware has processed all descriptors on the
 *  admin send queue. Returns false if there are still requests pending.
 */
bool idpf_asq_done(struct idpf_hw *hw)
{
	/* AQ designers suggest use of head for better
	 * timing reliability than DD bit
	 */
	return rd32(hw, hw->asq->reg.head) == hw->asq->next_to_use;
}

/**
 * idpf_check_asq_alive
 * @hw: pointer to the hw struct
 *
 * Returns true if Queue is enabled else false.
 */
bool idpf_check_asq_alive(struct idpf_hw *hw)
{
	if (hw->asq->reg.len)
		return !!(rd32(hw, hw->asq->reg.len) &
			  PF_FW_ATQLEN_ATQENABLE_M);

	return false;
}

/**
 *  idpf_clean_arq_element
 *  @hw: pointer to the hw struct
 *  @e: event info from the receive descriptor, includes any buffers
 *  @pending: number of events that could be left to process
 *
 *  This function cleans one Admin Receive Queue element and returns
 *  the contents through e.  It can also return how many events are
 *  left to process through 'pending'
 */
int idpf_clean_arq_element(struct idpf_hw *hw,
			   struct idpf_arq_event_info *e, u16 *pending)
{
	struct idpf_dma_mem *dma_mem = NULL;
	struct idpf_ctlq_msg msg = { 0 };
	int status;
	u16 msg_data_len;

	*pending = 1;

	status = idpf_ctlq_recv(hw->arq, pending, &msg);
	if (status == -ENOMSG)
		goto exit;

	/* ctlq_msg does not align to ctlq_desc, so copy relevant data here */
	e->desc.opcode = msg.opcode;
	e->desc.cookie_high = msg.cookie.mbx.chnl_opcode;
	e->desc.cookie_low = msg.cookie.mbx.chnl_retval;
	e->desc.ret_val = msg.status;
	e->desc.datalen = msg.data_len;
	if (msg.data_len > 0) {
		if (!msg.ctx.indirect.payload)
			return -EINVAL;
		e->buf_len = msg.data_len;
		msg_data_len = msg.data_len;
		idpf_memcpy(e->msg_buf, msg.ctx.indirect.payload->va, msg_data_len,
			    IDPF_DMA_TO_NONDMA);
		dma_mem = msg.ctx.indirect.payload;
	} else {
		*pending = 0;
	}

	status = idpf_ctlq_post_rx_buffs(hw, hw->arq, pending, &dma_mem);

exit:
	return status;
}

/**
 *  idpf_deinit_hw - shutdown routine
 *  @hw: pointer to the hardware structure
 */
int idpf_deinit_hw(struct idpf_hw *hw)
{
	hw->asq = NULL;
	hw->arq = NULL;

	return idpf_ctlq_deinit(hw);
}

/**
 * idpf_reset
 * @hw: pointer to the hardware structure
 *
 * Send a RESET message to the CPF. Does not wait for response from CPF
 * as none will be forthcoming. Immediately after calling this function,
 * the control queue should be shut down and (optionally) reinitialized.
 */
int idpf_reset(struct idpf_hw *hw)
{
	return idpf_send_msg_to_cp(hw, VIRTCHNL_OP_RESET_VF,
				      0, NULL, 0);
}

/**
 * idpf_get_set_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 * @set: set true to set the table, false to get the table
 *
 * Internal function to get or set RSS look up table
 */
STATIC int idpf_get_set_rss_lut(struct idpf_hw *hw, u16 vsi_id,
				bool pf_lut, u8 *lut, u16 lut_size,
				bool set)
{
	/* TODO fill out command */
	return 0;
}

/**
 * idpf_get_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 *
 * get the RSS lookup table, PF or VSI type
 */
int idpf_get_rss_lut(struct idpf_hw *hw, u16 vsi_id, bool pf_lut,
		     u8 *lut, u16 lut_size)
{
	return idpf_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size, false);
}

/**
 * idpf_set_rss_lut
 * @hw: pointer to the hardware structure
 * @vsi_id: vsi fw index
 * @pf_lut: for PF table set true, for VSI table set false
 * @lut: pointer to the lut buffer provided by the caller
 * @lut_size: size of the lut buffer
 *
 * set the RSS lookup table, PF or VSI type
 */
int idpf_set_rss_lut(struct idpf_hw *hw, u16 vsi_id, bool pf_lut,
		     u8 *lut, u16 lut_size)
{
	return idpf_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size, true);
}

/**
 * idpf_get_set_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 * @set: set true to set the key, false to get the key
 *
 * get the RSS key per VSI
 */
STATIC int idpf_get_set_rss_key(struct idpf_hw *hw, u16 vsi_id,
				struct idpf_get_set_rss_key_data *key,
				bool set)
{
	/* TODO fill out command */
	return 0;
}

/**
 * idpf_get_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 *
 */
int idpf_get_rss_key(struct idpf_hw *hw, u16 vsi_id,
		     struct idpf_get_set_rss_key_data *key)
{
	return idpf_get_set_rss_key(hw, vsi_id, key, false);
}

/**
 * idpf_set_rss_key
 * @hw: pointer to the hw struct
 * @vsi_id: vsi fw index
 * @key: pointer to key info struct
 *
 * set the RSS key per VSI
 */
int idpf_set_rss_key(struct idpf_hw *hw, u16 vsi_id,
		     struct idpf_get_set_rss_key_data *key)
{
	return idpf_get_set_rss_key(hw, vsi_id, key, true);
}

RTE_LOG_REGISTER_DEFAULT(idpf_common_logger, NOTICE);
