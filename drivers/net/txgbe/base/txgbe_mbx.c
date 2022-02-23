/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_type.h"

#include "txgbe_mbx.h"

/**
 *  txgbe_read_mbx - Reads a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to read
 *
 *  returns 0 if it successfully read message from buffer
 **/
s32 txgbe_read_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	/* limit read to size of mailbox */
	if (size > mbx->size)
		size = mbx->size;

	if (mbx->read)
		ret_val = mbx->read(hw, msg, size, mbx_id);

	return ret_val;
}

/**
 *  txgbe_write_mbx - Write a message to the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns 0 if it successfully copied message into the buffer
 **/
s32 txgbe_write_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = 0;

	if (size > mbx->size) {
		ret_val = TXGBE_ERR_MBX;
		DEBUGOUT("Invalid mailbox message size %d", size);
	} else if (mbx->write) {
		ret_val = mbx->write(hw, msg, size, mbx_id);
	}

	return ret_val;
}

/**
 *  txgbe_check_for_msg - checks to see if someone sent us mail
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 txgbe_check_for_msg(struct txgbe_hw *hw, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	if (mbx->check_for_msg)
		ret_val = mbx->check_for_msg(hw, mbx_id);

	return ret_val;
}

/**
 *  txgbe_check_for_ack - checks to see if someone sent us ACK
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 txgbe_check_for_ack(struct txgbe_hw *hw, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	if (mbx->check_for_ack)
		ret_val = mbx->check_for_ack(hw, mbx_id);

	return ret_val;
}

/**
 *  txgbe_check_for_rst - checks to see if other side has reset
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns 0 if the Status bit was found or else ERR_MBX
 **/
s32 txgbe_check_for_rst(struct txgbe_hw *hw, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	if (mbx->check_for_rst)
		ret_val = mbx->check_for_rst(hw, mbx_id);

	return ret_val;
}

/**
 *  txgbe_poll_for_msg - Wait for message notification
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification
 **/
STATIC s32 txgbe_poll_for_msg(struct txgbe_hw *hw, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->check_for_msg)
		goto out;

	while (countdown && mbx->check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	if (countdown == 0)
		DEBUGOUT("Polling for VF%d mailbox message timedout", mbx_id);

out:
	return countdown ? 0 : TXGBE_ERR_MBX;
}

/**
 *  txgbe_poll_for_ack - Wait for message acknowledgment
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message acknowledgment
 **/
STATIC s32 txgbe_poll_for_ack(struct txgbe_hw *hw, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->check_for_ack)
		goto out;

	while (countdown && mbx->check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	if (countdown == 0)
		DEBUGOUT("Polling for VF%d mailbox ack timedout", mbx_id);

out:
	return countdown ? 0 : TXGBE_ERR_MBX;
}

/**
 *  txgbe_read_posted_mbx - Wait for message notification and receive message
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification and
 *  copied it into the receive buffer.
 **/
s32 txgbe_read_posted_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	if (!mbx->read)
		goto out;

	ret_val = txgbe_poll_for_msg(hw, mbx_id);

	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		ret_val = mbx->read(hw, msg, size, mbx_id);
out:
	return ret_val;
}

/**
 *  txgbe_write_posted_mbx - Write a message to the mailbox, wait for ack
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully copied message into the buffer and
 *  received an ack to that message within delay * timeout period
 **/
s32 txgbe_write_posted_mbx(struct txgbe_hw *hw, u32 *msg, u16 size,
			   u16 mbx_id)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = TXGBE_ERR_MBX;

	/* exit if either we can't write or there isn't a defined timeout */
	if (!mbx->write || !mbx->timeout)
		goto out;

	/* send msg */
	ret_val = mbx->write(hw, msg, size, mbx_id);

	/* if msg sent wait until we receive an ack */
	if (!ret_val)
		ret_val = txgbe_poll_for_ack(hw, mbx_id);
out:
	return ret_val;
}

/**
 *  txgbe_read_v2p_mailbox - read v2p mailbox
 *  @hw: pointer to the HW structure
 *
 *  This function is used to read the v2p mailbox without losing the read to
 *  clear status bits.
 **/
STATIC u32 txgbe_read_v2p_mailbox(struct txgbe_hw *hw)
{
	u32 v2p_mailbox = rd32(hw, TXGBE_VFMBCTL);

	v2p_mailbox |= hw->mbx.v2p_mailbox;
	hw->mbx.v2p_mailbox |= v2p_mailbox & TXGBE_VFMBCTL_R2C_BITS;

	return v2p_mailbox;
}

/**
 *  txgbe_check_for_bit_vf - Determine if a status bit was set
 *  @hw: pointer to the HW structure
 *  @mask: bitmask for bits to be tested and cleared
 *
 *  This function is used to check for the read to clear bits within
 *  the V2P mailbox.
 **/
STATIC s32 txgbe_check_for_bit_vf(struct txgbe_hw *hw, u32 mask)
{
	u32 v2p_mailbox = txgbe_read_v2p_mailbox(hw);
	s32 ret_val = TXGBE_ERR_MBX;

	if (v2p_mailbox & mask)
		ret_val = 0;

	hw->mbx.v2p_mailbox &= ~mask;

	return ret_val;
}

/**
 *  txgbe_check_for_msg_vf - checks to see if the PF has sent mail
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns SUCCESS if the PF has set the Status bit or else ERR_MBX
 **/
s32 txgbe_check_for_msg_vf(struct txgbe_hw *hw, u16 mbx_id)
{
	s32 ret_val = TXGBE_ERR_MBX;

	UNREFERENCED_PARAMETER(mbx_id);

	if (!txgbe_check_for_bit_vf(hw, TXGBE_VFMBCTL_PFSTS)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

/**
 *  txgbe_check_for_ack_vf - checks to see if the PF has ACK'd
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns SUCCESS if the PF has set the ACK bit or else ERR_MBX
 **/
s32 txgbe_check_for_ack_vf(struct txgbe_hw *hw, u16 mbx_id)
{
	s32 ret_val = TXGBE_ERR_MBX;

	UNREFERENCED_PARAMETER(mbx_id);

	if (!txgbe_check_for_bit_vf(hw, TXGBE_VFMBCTL_PFACK)) {
		ret_val = 0;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

/**
 *  txgbe_check_for_rst_vf - checks to see if the PF has reset
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns true if the PF has set the reset done bit or else false
 **/
s32 txgbe_check_for_rst_vf(struct txgbe_hw *hw, u16 mbx_id)
{
	s32 ret_val = TXGBE_ERR_MBX;

	UNREFERENCED_PARAMETER(mbx_id);

	if (!txgbe_check_for_bit_vf(hw, (TXGBE_VFMBCTL_RSTD |
	    TXGBE_VFMBCTL_RSTI))) {
		ret_val = 0;
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 *  txgbe_obtain_mbx_lock_vf - obtain mailbox lock
 *  @hw: pointer to the HW structure
 *
 *  return SUCCESS if we obtained the mailbox lock
 **/
STATIC s32 txgbe_obtain_mbx_lock_vf(struct txgbe_hw *hw)
{
	s32 ret_val = TXGBE_ERR_MBX;

	/* Take ownership of the buffer */
	wr32(hw, TXGBE_VFMBCTL, TXGBE_VFMBCTL_VFU);

	/* reserve mailbox for vf use */
	if (txgbe_read_v2p_mailbox(hw) & TXGBE_VFMBCTL_VFU)
		ret_val = 0;

	return ret_val;
}

/**
 *  txgbe_write_mbx_vf - Write a message to the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully copied message into the buffer
 **/
s32 txgbe_write_mbx_vf(struct txgbe_hw *hw, u32 *msg, u16 size,
			      u16 mbx_id)
{
	s32 ret_val;
	u16 i;

	UNREFERENCED_PARAMETER(mbx_id);

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = txgbe_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	txgbe_check_for_msg_vf(hw, 0);
	txgbe_check_for_ack_vf(hw, 0);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		wr32a(hw, TXGBE_VFMBX, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* Drop VFU and interrupt the PF to tell it a message has been sent */
	wr32(hw, TXGBE_VFMBCTL, TXGBE_VFMBCTL_REQ);

out_no_write:
	return ret_val;
}

/**
 *  txgbe_read_mbx_vf - Reads a message from the inbox intended for vf
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to read
 *
 *  returns SUCCESS if it successfully read message from buffer
 **/
s32 txgbe_read_mbx_vf(struct txgbe_hw *hw, u32 *msg, u16 size,
			     u16 mbx_id)
{
	s32 ret_val = 0;
	u16 i;

	UNREFERENCED_PARAMETER(mbx_id);

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = txgbe_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out_no_read;

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = rd32a(hw, TXGBE_VFMBX, i);

	/* Acknowledge receipt and release mailbox, then we're done */
	wr32(hw, TXGBE_VFMBCTL, TXGBE_VFMBCTL_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

/**
 *  txgbe_init_mbx_params_vf - set initial values for vf mailbox
 *  @hw: pointer to the HW structure
 *
 *  Initializes the hw->mbx struct to correct values for vf mailbox
 */
void txgbe_init_mbx_params_vf(struct txgbe_hw *hw)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;

	/* start mailbox as timed out and let the reset_hw call set the timeout
	 * value to begin communications
	 */
	mbx->timeout = 0;
	mbx->usec_delay = TXGBE_VF_MBX_INIT_DELAY;

	mbx->size = TXGBE_P2VMBX_SIZE;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}

STATIC s32 txgbe_check_for_bit_pf(struct txgbe_hw *hw, u32 mask, s32 index)
{
	u32 mbvficr = rd32(hw, TXGBE_MBVFICR(index));
	s32 ret_val = TXGBE_ERR_MBX;

	if (mbvficr & mask) {
		ret_val = 0;
		wr32(hw, TXGBE_MBVFICR(index), mask);
	}

	return ret_val;
}

/**
 *  txgbe_check_for_msg_pf - checks to see if the VF has sent mail
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 txgbe_check_for_msg_pf(struct txgbe_hw *hw, u16 vf_number)
{
	s32 ret_val = TXGBE_ERR_MBX;
	s32 index = TXGBE_MBVFICR_INDEX(vf_number);
	u32 vf_bit = vf_number % 16;

	if (!txgbe_check_for_bit_pf(hw, TXGBE_MBVFICR_VFREQ_VF1 << vf_bit,
				    index)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

/**
 *  txgbe_check_for_ack_pf - checks to see if the VF has ACKed
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 txgbe_check_for_ack_pf(struct txgbe_hw *hw, u16 vf_number)
{
	s32 ret_val = TXGBE_ERR_MBX;
	s32 index = TXGBE_MBVFICR_INDEX(vf_number);
	u32 vf_bit = vf_number % 16;

	if (!txgbe_check_for_bit_pf(hw, TXGBE_MBVFICR_VFACK_VF1 << vf_bit,
				    index)) {
		ret_val = 0;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

/**
 *  txgbe_check_for_rst_pf - checks to see if the VF has reset
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns 0 if the VF has set the Status bit or else ERR_MBX
 **/
s32 txgbe_check_for_rst_pf(struct txgbe_hw *hw, u16 vf_number)
{
	u32 reg_offset = (vf_number < 32) ? 0 : 1;
	u32 vf_shift = vf_number % 32;
	u32 vflre = 0;
	s32 ret_val = TXGBE_ERR_MBX;

	vflre = rd32(hw, TXGBE_FLRVFE(reg_offset));
	if (vflre & (1 << vf_shift)) {
		ret_val = 0;
		wr32(hw, TXGBE_FLRVFEC(reg_offset), (1 << vf_shift));
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 *  txgbe_obtain_mbx_lock_pf - obtain mailbox lock
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  return 0 if we obtained the mailbox lock
 **/
STATIC s32 txgbe_obtain_mbx_lock_pf(struct txgbe_hw *hw, u16 vf_number)
{
	s32 ret_val = TXGBE_ERR_MBX;
	u32 p2v_mailbox;

	/* Take ownership of the buffer */
	wr32(hw, TXGBE_MBCTL(vf_number), TXGBE_MBCTL_PFU);

	/* reserve mailbox for vf use */
	p2v_mailbox = rd32(hw, TXGBE_MBCTL(vf_number));
	if (p2v_mailbox & TXGBE_MBCTL_PFU)
		ret_val = 0;
	else
		DEBUGOUT("Failed to obtain mailbox lock for VF%d", vf_number);


	return ret_val;
}

/**
 *  txgbe_write_mbx_pf - Places a message in the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  returns 0 if it successfully copied message into the buffer
 **/
s32 txgbe_write_mbx_pf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number)
{
	s32 ret_val;
	u16 i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = txgbe_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	txgbe_check_for_msg_pf(hw, vf_number);
	txgbe_check_for_ack_pf(hw, vf_number);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		wr32a(hw, TXGBE_MBMEM(vf_number), i, msg[i]);

	/* Interrupt VF to tell it a message has been sent and release buffer*/
	wr32(hw, TXGBE_MBCTL(vf_number), TXGBE_MBCTL_STS);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

out_no_write:
	return ret_val;
}

/**
 *  txgbe_read_mbx_pf - Read a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  This function copies a message from the mailbox buffer to the caller's
 *  memory buffer.  The presumption is that the caller knows that there was
 *  a message due to a VF request so no polling for message is needed.
 **/
s32 txgbe_read_mbx_pf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number)
{
	s32 ret_val;
	u16 i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = txgbe_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_read;

	/* copy the message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = rd32a(hw, TXGBE_MBMEM(vf_number), i);

	/* Acknowledge the message and release buffer */
	wr32(hw, TXGBE_MBCTL(vf_number), TXGBE_MBCTL_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

/**
 *  txgbe_init_mbx_params_pf - set initial values for pf mailbox
 *  @hw: pointer to the HW structure
 *
 *  Initializes the hw->mbx struct to correct values for pf mailbox
 */
void txgbe_init_mbx_params_pf(struct txgbe_hw *hw)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;

	mbx->timeout = 0;
	mbx->usec_delay = 0;

	mbx->size = TXGBE_P2VMBX_SIZE;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}
