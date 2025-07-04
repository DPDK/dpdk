/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <string.h>

#include "rnp_hw.h"
#include "rnp_mbx.h"
#include "../rnp.h"

/****************************PF MBX OPS************************************/
static inline u16
rnp_mbx_get_req(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 reg = 0;

	if (mbx_id == RNP_MBX_FW)
		reg = RNP_FW2PF_SYNC;
	else
		reg = RNP_VF2PF_SYNC(mbx_id);
	mb();

	return RNP_E_REG_RD(hw, reg) & RNP_MBX_SYNC_REQ_MASK;
}

static inline u16
rnp_mbx_get_ack(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 reg = 0;
	u32 v = 0;

	if (mbx_id == RNP_MBX_FW)
		reg = RNP_FW2PF_SYNC;
	else
		reg = RNP_VF2PF_SYNC(mbx_id);
	mb();
	v = RNP_E_REG_RD(hw, reg);

	return (v & RNP_MBX_SYNC_ACK_MASK) >> RNP_MBX_SYNC_ACK_S;
}

/*
 * rnp_mbx_inc_pf_ack - increase ack num of mailbox sync info
 * @hw pointer to the HW structure
 * @sync_base: addr of sync
 */
static inline void
rnp_mbx_inc_pf_req(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 sync_base;
	u32 req;
	u32 v;

	if (mbx_id == RNP_MBX_FW)
		sync_base = RNP_PF2FW_SYNC;
	else
		sync_base = RNP_PF2VF_SYNC(mbx_id);
	v = RNP_E_REG_RD(hw, sync_base);
	req = (v & RNP_MBX_SYNC_REQ_MASK);
	req++;
	/* clear sync req value */
	v &= ~(RNP_MBX_SYNC_REQ_MASK);
	v |= req;

	mb();
	RNP_E_REG_WR(hw, sync_base, v);
}

/*
 * rnp_mbx_inc_pf_ack - increase ack num of maixbox sync info
 * @hw pointer to the HW structure
 * @sync_base: addr of sync
 */
static inline void
rnp_mbx_inc_pf_ack(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 ack;
	u32 reg;
	u32 v;

	if (mbx_id == RNP_MBX_FW)
		reg = RNP_PF2FW_SYNC;
	else
		reg = RNP_PF2VF_SYNC(mbx_id);
	v = RNP_E_REG_RD(hw, reg);
	ack = (v & RNP_MBX_SYNC_ACK_MASK) >> RNP_MBX_SYNC_ACK_S;
	ack++;
	/* clear old sync ack */
	v &= ~RNP_MBX_SYNC_ACK_MASK;
	v |= (ack << RNP_MBX_SYNC_ACK_S);
	mb();
	RNP_E_REG_WR(hw, reg, v);
}

static void
rnp_mbx_write_msg(struct rnp_hw *hw,
		  u32 *msg, u16 size,
		  enum RNP_MBX_ID mbx_id)
{
	u32 msg_base;
	u16 i = 0;

	if (mbx_id == RNP_MBX_FW)
		msg_base = RNP_FW2PF_MSG_DATA;
	else
		msg_base = RNP_PF2VF_MSG_DATA(mbx_id);
	for (i = 0; i < size; i++)
		RNP_E_REG_WR(hw, msg_base + i * 4, msg[i]);
}

static void
rnp_mbx_read_msg(struct rnp_hw *hw,
		 u32 *msg, u16 size,
		 enum RNP_MBX_ID mbx_id)
{
	u32 msg_base;
	u16 i = 0;

	if (mbx_id == RNP_MBX_FW)
		msg_base = RNP_FW2PF_MSG_DATA;
	else
		msg_base = RNP_PF2VF_MSG_DATA(mbx_id);
	for (i = 0; i < size; i++)
		msg[i] = RNP_E_REG_RD(hw, msg_base + 4 * i);
	mb();
	/* clear msg cmd */
	RNP_E_REG_WR(hw, msg_base, 0);
}

/*
 *  rnp_poll_for_msg - Wait for message notification
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification
 */
static int
rnp_poll_for_msg(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_info *mbx = &hw->mbx;
	u32 countdown = mbx->timeout;

	if (!countdown || !mbx->ops->check_for_msg)
		goto out;

	while (countdown && mbx->ops->check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->usec_delay);
	}
out:
	return countdown ? 0 : -ETIMEDOUT;
}

/*
 *  rnp_poll_for_ack - Wait for message acknowledgment
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message acknowledgment
 */
static int
rnp_poll_for_ack(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_info *mbx = &hw->mbx;
	u32 countdown = mbx->timeout;

	if (!countdown || !mbx->ops->check_for_ack)
		goto out;

	while (countdown && mbx->ops->check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->usec_delay);
	}

out:
	return countdown ? 0 : -ETIMEDOUT;
}

static int
rnp_read_mbx_msg(struct rnp_hw *hw, u32 *msg, u16 size,
		 enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_info *mbx = &hw->mbx;
	int ret = RNP_ERR_MBX;

	if (size > mbx->size)
		return -EINVAL;
	if (mbx->ops->read)
		return mbx->ops->read(hw, msg, size, mbx_id);
	return ret;
}

static int
rnp_write_mbx_msg(struct rnp_hw *hw, u32 *msg, u16 size,
		  enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_info *mbx = &hw->mbx;
	int ret = RNP_ERR_MBX;

	/* exit if either we can't write or there isn't a defined timeout */
	if (size > mbx->size)
		return -EINVAL;
	if (mbx->ops->write)
		return mbx->ops->write(hw, msg, size, mbx_id);
	return ret;
}

/*
 *  rnp_obtain_mbx_lock_pf - obtain mailbox lock
 *  @hw: pointer to the HW structure
 *  @ctrl_base: ctrl mbx addr
 *
 *  return SUCCESS if we obtained the mailbox lock
 */
static int rnp_obtain_mbx_lock_pf(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	int ret_val = -ETIMEDOUT;
	u32 try_cnt = 5000;  /* 500ms */
	u32 ctrl_base;

	if (mbx_id == RNP_MBX_FW)
		ctrl_base = RNP_PF2FW_MBX_CTRL;
	else
		ctrl_base = RNP_PF2VF_MBX_CTRL(mbx_id);
	while (try_cnt-- > 0) {
		/* take ownership of the buffer */
		RNP_E_REG_WR(hw, ctrl_base, RNP_MBX_CTRL_PF_HOLD);
		wmb();
		/* reserve mailbox for pf used */
		if (RNP_E_REG_RD(hw, ctrl_base) & RNP_MBX_CTRL_PF_HOLD)
			return 0;
		udelay(100);
	}

	RNP_PMD_LOG(WARNING, "%s: failed to get:lock",
			__func__);
	return ret_val;
}

static void
rnp_obtain_mbx_unlock_pf(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 ctrl_base;

	if (mbx_id == RNP_MBX_FW)
		ctrl_base = RNP_PF2FW_MBX_CTRL;
	else
		ctrl_base = RNP_PF2VF_MBX_CTRL(mbx_id);
	RNP_E_REG_WR(hw, ctrl_base, 0);
}

static void
rnp_mbx_send_irq_pf(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	u32 ctrl_base;

	if (mbx_id == RNP_MBX_FW)
		ctrl_base = RNP_PF2FW_MBX_CTRL;
	else
		ctrl_base = RNP_PF2VF_MBX_CTRL(mbx_id);

	RNP_E_REG_WR(hw, ctrl_base, RNP_MBX_CTRL_REQ);
}
/*
 *  rnp_read_mbx_pf - Read a message from the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of request mbx target
 *
 *  This function copies a message from the mailbox buffer to the caller's
 *  memory buffer.  The presumption is that the caller knows that there was
 *  a message due to a VF/FW request so no polling for message is needed.
 */
static int rnp_read_mbx_pf(struct rnp_hw *hw, u32 *msg,
			   u16 size, enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_sync *sync = &hw->mbx.syncs[mbx_id];
	struct rnp_mbx_info *mbx = &hw->mbx;
	int ret_val = -EBUSY;

	if (size > mbx->size) {
		RNP_PMD_LOG(ERR, "%s: mbx msg block size:%d should <%d",
				__func__, size, mbx->size);
		return -EINVAL;
	}
	memset(msg, 0, sizeof(*msg) * size);
	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = rnp_obtain_mbx_lock_pf(hw, mbx_id);
	if (ret_val)
		goto out_no_read;
	/* copy the message from the mailbox memory buffer */
	rnp_mbx_read_msg(hw, msg, size, mbx_id);
	/* update req. sync with fw or vf */
	sync->req = rnp_mbx_get_req(hw, mbx_id);
	/* Acknowledge receipt and release mailbox, then we're done */
	rnp_mbx_inc_pf_ack(hw, mbx_id);
	mb();
	/* free ownership of the buffer */
	rnp_obtain_mbx_unlock_pf(hw, mbx_id);

out_no_read:

	return ret_val;
}

/*
 *  rnp_write_mbx_pf - Places a message in the mailbox
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of request mbx target
 *
 *  returns SUCCESS if it successfully copied message into the buffer
 */
static int rnp_write_mbx_pf(struct rnp_hw *hw, u32 *msg, u16 size,
			    enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_sync *sync = &hw->mbx.syncs[mbx_id];
	struct rnp_mbx_info *mbx = &hw->mbx;
	int ret_val = 0;

	if (size > mbx->size) {
		RNP_PMD_LOG(ERR, "%s: size:%d should <%d", __func__, size,
				mbx->size);
		return -EINVAL;
	}
	/* lock the mailbox to prevent pf/vf/cpu race condition */
	ret_val = rnp_obtain_mbx_lock_pf(hw, mbx_id);
	if (ret_val) {
		RNP_PMD_LOG(ERR, "%s: get mbx:%d wlock failed. "
				"msg:0x%08x-0x%08x", __func__, mbx_id,
				msg[0], msg[1]);
		goto out_no_write;
	}
	/* copy the caller specified message to the mailbox memory buffer */
	rnp_mbx_write_msg(hw, msg, size, mbx_id);
	/* flush msg and acks as we are overwriting the message buffer */
	sync->ack = rnp_mbx_get_ack(hw, mbx_id);
	rnp_mbx_inc_pf_req(hw, mbx_id);
	udelay(300);
	mb();
	/* interrupt VF/FW to tell it a message has been sent and release buf */
	rnp_mbx_send_irq_pf(hw, mbx_id);

out_no_write:

	return ret_val;
}

/*
 *  rnp_read_posted_mbx - Wait for message notification and receive message
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully received a message notification and
 *  copied it into the receive buffer.
 */
static int32_t
rnp_read_posted_mbx(struct rnp_hw *hw,
		    u32 *msg, u16 size, enum RNP_MBX_ID mbx_id)
{
	int ret_val = -EINVAL;

	ret_val = rnp_poll_for_msg(hw, mbx_id);
	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		return rnp_read_mbx_msg(hw, msg, size, mbx_id);
	return ret_val;
}

/*
 *  rnp_write_posted_mbx - Write a message to the mailbox, wait for ack
 *  @hw: pointer to the HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if it successfully copied message into the buffer and
 *  received an ack to that message within delay * timeout period
 */
static int rnp_write_posted_mbx(struct rnp_hw *hw,
				u32 *msg, u16 size,
				enum RNP_MBX_ID mbx_id)
{
	int ret_val = RNP_ERR_MBX;

	ret_val = rnp_write_mbx_msg(hw, msg, size, mbx_id);
	if (ret_val < 0)
		return ret_val;
	/* if msg sent wait until we receive an ack */
	return rnp_poll_for_ack(hw, mbx_id);
}

/*
 *  rnp_check_for_msg_pf - checks to see if the VF/FW has sent mail
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 */
static int rnp_check_for_msg_pf(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_sync *sync = &hw->mbx.syncs[mbx_id];
	int ret_val = RNP_ERR_MBX;

	if (rnp_mbx_get_req(hw, mbx_id) != sync->req)
		ret_val = 0;

	return ret_val;
}

/*
 *  rnp_check_for_ack_pf - checks to see if the VF/FW has ACKed
 *  @hw: pointer to the HW structure
 *  @mbx_id: id of mailbox to write
 *
 *  returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 */
static int rnp_check_for_ack_pf(struct rnp_hw *hw, enum RNP_MBX_ID mbx_id)
{
	struct rnp_mbx_sync *sync = &hw->mbx.syncs[mbx_id];
	int ret_val = RNP_ERR_MBX;

	if (rnp_mbx_get_ack(hw, mbx_id) != sync->ack)
		ret_val = 0;

	return ret_val;
}

const struct rnp_mbx_ops rnp_mbx_ops_pf = {
	.read = rnp_read_mbx_pf,
	.write = rnp_write_mbx_pf,
	.read_posted = rnp_read_posted_mbx,
	.write_posted = rnp_write_posted_mbx,
	.check_for_msg = rnp_check_for_msg_pf,
	.check_for_ack = rnp_check_for_ack_pf,
};

static u32 rnp_get_pfvfnum(struct rnp_hw *hw)
{
	u32 val = 0;

	val = RNP_C_REG_RD(hw, RNP_SRIOV_INFO);

	return val >> RNP_PFVF_SHIFT;
}

static void rnp_mbx_reset(struct rnp_hw *hw)
{
	struct rnp_mbx_sync *sync = hw->mbx.syncs;
	int idx = 0;
	u32 v = 0;

	for (idx = 0; idx < hw->mbx.en_vfs; idx++) {
		v = RNP_E_REG_RD(hw, RNP_VF2PF_SYNC(idx));
		sync[idx].ack = (v & RNP_MBX_SYNC_ACK_MASK) >> RNP_MBX_SYNC_ACK_S;
		sync[idx].req = v & RNP_MBX_SYNC_REQ_MASK;
		/* release pf<->vf pf used buffer lock */
		RNP_E_REG_WR(hw, RNP_PF2VF_MBX_CTRL(idx), 0);
	}
	/* reset pf->fw status */
	v = RNP_E_REG_RD(hw, RNP_FW2PF_SYNC);
	sync[RNP_MBX_FW].ack = (v & RNP_MBX_SYNC_ACK_MASK) >> RNP_MBX_SYNC_ACK_S;
	sync[RNP_MBX_FW].req = v & RNP_MBX_SYNC_REQ_MASK;

	RNP_PMD_LOG(INFO, "now fw_req %d fw_ack %d",
			hw->mbx.syncs[idx].req, hw->mbx.syncs[idx].ack);
	/* release pf->fw buffer lock */
	RNP_E_REG_WR(hw, RNP_PF2FW_MBX_CTRL, 0);
	/* setup mailbox vec id */
	RNP_E_REG_WR(hw, RNP_FW2PF_MBOX_VEC, RNP_MISC_VEC_ID);
	/* enable 0-31 vf interrupt */
	RNP_E_REG_WR(hw, RNP_PF2VF_INTR_MASK(0), 0);
	/* enable 32-63 vf interrupt */
	RNP_E_REG_WR(hw, RNP_PF2VF_INTR_MASK(33), 0);
	/* enable firmware interrupt */
	RNP_E_REG_WR(hw, RNP_FW2PF_INTR_MASK, 0);
	RNP_C_REG_WR(hw, RNP_SRIOV_ADDR_CTRL, RNP_SRIOV_ADDR_EN);
}

int rnp_init_mbx_pf(struct rnp_hw *hw)
{
	struct rnp_proc_priv *proc_priv = RNP_DEV_TO_PROC_PRIV(hw->back->eth_dev);
	struct rnp_mbx_info *mbx = &hw->mbx;
	u32 pf_vf_num;

	pf_vf_num = rnp_get_pfvfnum(hw);
	mbx->usec_delay = RNP_MBX_DELAY_US;
	mbx->timeout = RNP_MBX_MAX_TM_SEC / mbx->usec_delay;
	mbx->size = RNP_MBX_MSG_BLOCK_SIZE;
	mbx->pf_num = (pf_vf_num & RNP_PF_BIT_MASK) ? 1 : 0;
	mbx->vf_num = UINT16_MAX;
	mbx->ops = &rnp_mbx_ops_pf;
	proc_priv->mbx_ops = &rnp_mbx_ops_pf;
	hw->pf_vf_num = pf_vf_num;
	mbx->is_pf = 1;
	rnp_mbx_reset(hw);

	return 0;
}
