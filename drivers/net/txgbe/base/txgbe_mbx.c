/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
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

	DEBUGFUNC("txgbe_read_mbx");

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

	DEBUGFUNC("txgbe_write_mbx");

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

	DEBUGFUNC("txgbe_check_for_msg");

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

	DEBUGFUNC("txgbe_check_for_ack");

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

	DEBUGFUNC("txgbe_check_for_rst");

	if (mbx->check_for_rst)
		ret_val = mbx->check_for_rst(hw, mbx_id);

	return ret_val;
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

	DEBUGFUNC("txgbe_check_for_msg_pf");

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

	DEBUGFUNC("txgbe_check_for_ack_pf");

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

	DEBUGFUNC("txgbe_check_for_rst_pf");

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

	DEBUGFUNC("txgbe_obtain_mbx_lock_pf");

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

	DEBUGFUNC("txgbe_write_mbx_pf");

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

	DEBUGFUNC("txgbe_read_mbx_pf");

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
