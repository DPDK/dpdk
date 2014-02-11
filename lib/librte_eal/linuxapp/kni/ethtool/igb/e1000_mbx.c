/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "e1000_mbx.h"

/**
 *  e1000_null_mbx_check_for_flag - No-op function, return 0
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_null_mbx_check_for_flag(struct e1000_hw E1000_UNUSEDARG *hw,
					 u16 E1000_UNUSEDARG mbx_id)
{
	DEBUGFUNC("e1000_null_mbx_check_flag");

	return E1000_SUCCESS;
}

/**
 *  e1000_null_mbx_transact - No-op function, return 0
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_null_mbx_transact(struct e1000_hw E1000_UNUSEDARG *hw,
				   u32 E1000_UNUSEDARG *msg,
				   u16 E1000_UNUSEDARG size,
				   u16 E1000_UNUSEDARG mbx_id)
{
	DEBUGFUNC("e1000_null_mbx_rw_msg");

	return E1000_SUCCESS;
}

/**
 *  e1000_read_mbx - Reads a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to read
 *
 *  returns SUCCESS if it successfuly read message from buffer
 **/
s32 e1000_read_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_read_mbx");

	/* limit read to size of mailbox */
	if (size > mbx->size)
		size = mbx->size;

	if (mbx->ops.read)
		ret_val = mbx->ops.read(hw, msg, size, mbx_id);

	return ret_val;
}

/**
 *  e1000_write_mbx - Write a message to the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully copied message into the buffer
 **/
s32 e1000_write_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_write_mbx");

	if (size > mbx->size)
		ret_val = -E1000_ERR_MBX;

	else if (mbx->ops.write)
		ret_val = mbx->ops.write(hw, msg, size, mbx_id);

	return ret_val;
}

/**
 *  e1000_check_for_msg - checks to see if someone sent us mail
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 e1000_check_for_msg(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_msg");

	if (mbx->ops.check_for_msg)
		ret_val = mbx->ops.check_for_msg(hw, mbx_id);

	return ret_val;
}

/**
 *  e1000_check_for_ack - checks to see if someone sent us ACK
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 e1000_check_for_ack(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_ack");

	if (mbx->ops.check_for_ack)
		ret_val = mbx->ops.check_for_ack(hw, mbx_id);

	return ret_val;
}

/**
 *  e1000_check_for_rst - checks to see if other side has reset
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to check
 *
 *  returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 e1000_check_for_rst(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_rst");

	if (mbx->ops.check_for_rst)
		ret_val = mbx->ops.check_for_rst(hw, mbx_id);

	return ret_val;
}

/**
 *  e1000_poll_for_msg - Wait for message notification
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification
 **/
static s32 e1000_poll_for_msg(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	DEBUGFUNC("e1000_poll_for_msg");

	if (!countdown || !mbx->ops.check_for_msg)
		goto out;

	while (countdown && mbx->ops.check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	/* if we failed, all future posted messages fail until reset */
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? E1000_SUCCESS : -E1000_ERR_MBX;
}

/**
 *  e1000_poll_for_ack - Wait for message acknowledgement
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message acknowledgement
 **/
static s32 e1000_poll_for_ack(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	DEBUGFUNC("e1000_poll_for_ack");

	if (!countdown || !mbx->ops.check_for_ack)
		goto out;

	while (countdown && mbx->ops.check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	/* if we failed, all future posted messages fail until reset */
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? E1000_SUCCESS : -E1000_ERR_MBX;
}

/**
 *  e1000_read_posted_mbx - Wait for message notification and receive message
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification and
 *  copied it into the receive buffer.
 **/
s32 e1000_read_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_read_posted_mbx");

	if (!mbx->ops.read)
		goto out;

	ret_val = e1000_poll_for_msg(hw, mbx_id);

	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size, mbx_id);
out:
	return ret_val;
}

/**
 *  e1000_write_posted_mbx - Write a message to the mailbox, wait for ack
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully copied message into the buffer and
 *  received an ack to that message within delay * timeout period
 **/
s32 e1000_write_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_write_posted_mbx");

	/* exit if either we can't write or there isn't a defined timeout */
	if (!mbx->ops.write || !mbx->timeout)
		goto out;

	/* send msg */
	ret_val = mbx->ops.write(hw, msg, size, mbx_id);

	/* if msg sent wait until we receive an ack */
	if (!ret_val)
		ret_val = e1000_poll_for_ack(hw, mbx_id);
out:
	return ret_val;
}

/**
 *  e1000_init_mbx_ops_generic - Initialize mbx function pointers
 *  @hw: pointer to the HW structure
 *
 *  Sets the function pointers to no-op functions
 **/
void e1000_init_mbx_ops_generic(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	mbx->ops.init_params = e1000_null_ops_generic;
	mbx->ops.read = e1000_null_mbx_transact;
	mbx->ops.write = e1000_null_mbx_transact;
	mbx->ops.check_for_msg = e1000_null_mbx_check_for_flag;
	mbx->ops.check_for_ack = e1000_null_mbx_check_for_flag;
	mbx->ops.check_for_rst = e1000_null_mbx_check_for_flag;
	mbx->ops.read_posted = e1000_read_posted_mbx;
	mbx->ops.write_posted = e1000_write_posted_mbx;
}

static s32 e1000_check_for_bit_pf(struct e1000_hw *hw, u32 mask)
{
	u32 mbvficr = E1000_READ_REG(hw, E1000_MBVFICR);
	s32 ret_val = -E1000_ERR_MBX;

	if (mbvficr & mask) {
		ret_val = E1000_SUCCESS;
		E1000_WRITE_REG(hw, E1000_MBVFICR, mask);
	}

	return ret_val;
}

/**
 *  e1000_check_for_msg_pf - checks to see if the VF has sent mail
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 e1000_check_for_msg_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_msg_pf");

	if (!e1000_check_for_bit_pf(hw, E1000_MBVFICR_VFREQ_VF1 << vf_number)) {
		ret_val = E1000_SUCCESS;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

/**
 *  e1000_check_for_ack_pf - checks to see if the VF has ACKed
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 e1000_check_for_ack_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_ack_pf");

	if (!e1000_check_for_bit_pf(hw, E1000_MBVFICR_VFACK_VF1 << vf_number)) {
		ret_val = E1000_SUCCESS;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

/**
 *  e1000_check_for_rst_pf - checks to see if the VF has reset
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 e1000_check_for_rst_pf(struct e1000_hw *hw, u16 vf_number)
{
	u32 vflre = E1000_READ_REG(hw, E1000_VFLRE);
	s32 ret_val = -E1000_ERR_MBX;

	DEBUGFUNC("e1000_check_for_rst_pf");

	if (vflre & (1 << vf_number)) {
		ret_val = E1000_SUCCESS;
		E1000_WRITE_REG(hw, E1000_VFLRE, (1 << vf_number));
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 *  e1000_obtain_mbx_lock_pf - obtain mailbox lock
 *  @hw: pointer to the HW structure
 *  @vf_number: the VF index
 *
 *  return SUCCESS if we obtained the mailbox lock
 **/
static s32 e1000_obtain_mbx_lock_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;
	u32 p2v_mailbox;

	DEBUGFUNC("e1000_obtain_mbx_lock_pf");

	/* Take ownership of the buffer */
	E1000_WRITE_REG(hw, E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_PFU);

	/* reserve mailbox for vf use */
	p2v_mailbox = E1000_READ_REG(hw, E1000_P2VMAILBOX(vf_number));
	if (p2v_mailbox & E1000_P2VMAILBOX_PFU)
		ret_val = E1000_SUCCESS;

	return ret_val;
}

/**
 *  e1000_write_mbx_pf - Places a message in the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  returns SUCCESS if it successfully copied message into the buffer
 **/
static s32 e1000_write_mbx_pf(struct e1000_hw *hw, u32 *msg, u16 size,
			      u16 vf_number)
{
	s32 ret_val;
	u16 i;

	DEBUGFUNC("e1000_write_mbx_pf");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = e1000_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	e1000_check_for_msg_pf(hw, vf_number);
	e1000_check_for_ack_pf(hw, vf_number);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_VMBMEM(vf_number), i, msg[i]);

	/* Interrupt VF to tell it a message has been sent and release buffer*/
	E1000_WRITE_REG(hw, E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_STS);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

out_no_write:
	return ret_val;

}

/**
 *  e1000_read_mbx_pf - Read a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @vf_number: the VF index
 *
 *  This function copies a message from the mailbox buffer to the caller's
 *  memory buffer.  The presumption is that the caller knows that there was
 *  a message due to a VF request so no polling for message is needed.
 **/
static s32 e1000_read_mbx_pf(struct e1000_hw *hw, u32 *msg, u16 size,
			     u16 vf_number)
{
	s32 ret_val;
	u16 i;

	DEBUGFUNC("e1000_read_mbx_pf");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = e1000_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_read;

	/* copy the message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = E1000_READ_REG_ARRAY(hw, E1000_VMBMEM(vf_number), i);

	/* Acknowledge the message and release buffer */
	E1000_WRITE_REG(hw, E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

/**
 *  e1000_init_mbx_params_pf - set initial values for pf mailbox
 *  @hw: pointer to the HW structure
 *
 *  Initializes the hw->mbx struct to correct values for pf mailbox
 */
s32 e1000_init_mbx_params_pf(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;

	switch (hw->mac.type) {
	case e1000_82576:
	case e1000_i350:
	case e1000_i354:
		mbx->timeout = 0;
		mbx->usec_delay = 0;

		mbx->size = E1000_VFMAILBOX_SIZE;

		mbx->ops.read = e1000_read_mbx_pf;
		mbx->ops.write = e1000_write_mbx_pf;
		mbx->ops.read_posted = e1000_read_posted_mbx;
		mbx->ops.write_posted = e1000_write_posted_mbx;
		mbx->ops.check_for_msg = e1000_check_for_msg_pf;
		mbx->ops.check_for_ack = e1000_check_for_ack_pf;
		mbx->ops.check_for_rst = e1000_check_for_rst_pf;

		mbx->stats.msgs_tx = 0;
		mbx->stats.msgs_rx = 0;
		mbx->stats.reqs = 0;
		mbx->stats.acks = 0;
		mbx->stats.rsts = 0;
	default:
		return E1000_SUCCESS;
	}
}

