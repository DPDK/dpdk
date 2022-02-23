/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"

#include "ngbe_mbx.h"

/**
 *  ngbe_read_mbx - Reads a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to read
 *
 *  returns 0 if it successfully read message from buffer
 **/
s32 ngbe_read_mbx(struct ngbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = NGBE_ERR_MBX;

	/* limit read to size of mailbox */
	if (size > mbx->size)
		size = mbx->size;

	if (mbx->read)
		ret_val = mbx->read(hw, msg, size, mbx_id);

	return ret_val;
}

/**
 *  ngbe_write_mbx - Write a message to the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns 0 if it successfully copied message into the buffer
 **/
s32 ngbe_write_mbx(struct ngbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = 0;

	if (size > mbx->size) {
		ret_val = NGBE_ERR_MBX;
		DEBUGOUT("Invalid mailbox message size %d", size);
	} else if (mbx->write) {
		ret_val = mbx->write(hw, msg, size, mbx_id);
	}

	return ret_val;
}

/**
 *  ngbe_check_for_msg - checks to see if someone sent us mail
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 ngbe_check_for_msg(struct ngbe_hw *hw, u16 mbx_id)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = NGBE_ERR_MBX;

	if (mbx->check_for_msg)
		ret_val = mbx->check_for_msg(hw, mbx_id);

	return ret_val;
}

/**
 *  ngbe_check_for_ack - checks to see if someone sent us ACK
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 ngbe_check_for_ack(struct ngbe_hw *hw, u16 mbx_id)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = NGBE_ERR_MBX;

	if (mbx->check_for_ack)
		ret_val = mbx->check_for_ack(hw, mbx_id);

	return ret_val;
}

/**
 *  ngbe_check_for_rst - checks to see if other side has reset
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 ngbe_check_for_rst(struct ngbe_hw *hw, u16 mbx_id)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = NGBE_ERR_MBX;

	if (mbx->check_for_rst)
		ret_val = mbx->check_for_rst(hw, mbx_id);

	return ret_val;
}

STATIC s32 ngbe_check_for_bit_pf(struct ngbe_hw *hw, u32 mask)
{
	u32 mbvficr = rd32(hw, NGBE_MBVFICR);
	s32 ret_val = NGBE_ERR_MBX;

	if (mbvficr & mask) {
		ret_val = 0;
		wr32(hw, NGBE_MBVFICR, mask);
	}

	return ret_val;
}

/**
 *  ngbe_check_for_msg_pf - checks to see if the VF has sent mail
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 ngbe_check_for_msg_pf(struct ngbe_hw *hw, u16 vf_number)
{
	s32 ret_val = NGBE_ERR_MBX;
	u32 vf_bit = vf_number;

	if (!ngbe_check_for_bit_pf(hw, NGBE_MBVFICR_VFREQ_VF1 << vf_bit)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

/**
 *  ngbe_check_for_ack_pf - checks to see if the VF has ACKed
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 ngbe_check_for_ack_pf(struct ngbe_hw *hw, u16 vf_number)
{
	s32 ret_val = NGBE_ERR_MBX;
	u32 vf_bit = vf_number;

	if (!ngbe_check_for_bit_pf(hw, NGBE_MBVFICR_VFACK_VF1 << vf_bit)) {
		ret_val = 0;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

/**
 *  ngbe_check_for_rst_pf - checks to see if the VF has reset
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 ngbe_check_for_rst_pf(struct ngbe_hw *hw, u16 vf_number)
{
	u32 vflre = 0;
	s32 ret_val = NGBE_ERR_MBX;

	vflre = rd32(hw, NGBE_FLRVFE);
	if (vflre & (1 << vf_number)) {
		ret_val = 0;
		wr32(hw, NGBE_FLRVFEC, (1 << vf_number));
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 *  ngbe_obtain_mbx_lock_pf - obtain mailbox lock
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  return 0 if we obtained the mailbox lock
 **/
STATIC s32 ngbe_obtain_mbx_lock_pf(struct ngbe_hw *hw, u16 vf_number)
{
	s32 ret_val = NGBE_ERR_MBX;
	u32 p2v_mailbox;

	/* Take ownership of the buffer */
	wr32(hw, NGBE_MBCTL(vf_number), NGBE_MBCTL_PFU);

	/* reserve mailbox for vf use */
	p2v_mailbox = rd32(hw, NGBE_MBCTL(vf_number));
	if (p2v_mailbox & NGBE_MBCTL_PFU)
		ret_val = 0;
	else
		DEBUGOUT("Failed to obtain mailbox lock for VF%d", vf_number);


	return ret_val;
}

/**
 *  ngbe_write_mbx_pf - Places a message in the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  returns 0 if it successfully copied message into the buffer
 **/
s32 ngbe_write_mbx_pf(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number)
{
	s32 ret_val;
	u16 i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ngbe_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	ngbe_check_for_msg_pf(hw, vf_number);
	ngbe_check_for_ack_pf(hw, vf_number);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		wr32a(hw, NGBE_MBMEM(vf_number), i, msg[i]);

	/* Interrupt VF to tell it a message has been sent and release buffer*/
	wr32(hw, NGBE_MBCTL(vf_number), NGBE_MBCTL_STS);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

out_no_write:
	return ret_val;
}

/**
 *  ngbe_read_mbx_pf - Read a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  This function copies a message from the mailbox buffer to the caller's
 *  memory buffer.  The presumption is that the caller knows that there was
 *  a message due to a VF request so no polling for message is needed.
 **/
s32 ngbe_read_mbx_pf(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number)
{
	s32 ret_val;
	u16 i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ngbe_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_read;

	/* copy the message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = rd32a(hw, NGBE_MBMEM(vf_number), i);

	/* Acknowledge the message and release buffer */
	wr32(hw, NGBE_MBCTL(vf_number), NGBE_MBCTL_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

/**
 *  ngbe_init_mbx_params_pf - set initial values for pf mailbox
 *  @hw: pointer to the HW structure
 *
 *  Initializes the hw->mbx struct to correct values for pf mailbox
 */
void ngbe_init_mbx_params_pf(struct ngbe_hw *hw)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;

	mbx->timeout = 0;
	mbx->usec_delay = 0;

	mbx->size = NGBE_P2VMBX_SIZE;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}
