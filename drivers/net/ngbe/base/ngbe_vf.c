/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_mbx.h"
#include "ngbe_vf.h"

/* ngbe_virt_clr_reg - Set register to default (power on) state.
 * @hw: pointer to hardware structure
 */
static void ngbe_virt_clr_reg(struct ngbe_hw *hw)
{
	u32 vfsrrctl;

	/* default values (BUF_SIZE = 2048, HDR_SIZE = 256) */
	vfsrrctl = NGBE_RXCFG_HDRLEN(NGBE_RX_HDR_SIZE);
	vfsrrctl |= NGBE_RXCFG_PKTLEN(NGBE_RX_BUF_SIZE);

	wr32m(hw, NGBE_RXCFG(0),
		(NGBE_RXCFG_HDRLEN_MASK | NGBE_RXCFG_PKTLEN_MASK),
		vfsrrctl);


	ngbe_flush(hw);
}

/**
 *  ngbe_start_hw_vf - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware by filling the bus info structure and media type, clears
 *  all on chip counters, initializes receive address registers, multicast
 *  table, VLAN filter table, calls routine to set up link and flow control
 *  settings, and leaves transmit and receive units disabled and uninitialized
 **/
s32 ngbe_start_hw_vf(struct ngbe_hw *hw)
{
	/* Clear adapter stopped flag */
	hw->adapter_stopped = false;

	return 0;
}

/**
 *  ngbe_init_hw_vf - virtual function hardware initialization
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting the hardware and then starting
 *  the hardware
 **/
s32 ngbe_init_hw_vf(struct ngbe_hw *hw)
{
	s32 status = hw->mac.start_hw(hw);

	hw->mac.get_mac_addr(hw, hw->mac.addr);

	return status;
}

/**
 *  ngbe_reset_hw_vf - Performs hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks and
 *  clears all interrupts.
 **/
s32 ngbe_reset_hw_vf(struct ngbe_hw *hw)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	u32 timeout = NGBE_VF_INIT_TIMEOUT;
	s32 ret_val = NGBE_ERR_INVALID_MAC_ADDR;
	u32 msgbuf[NGBE_VF_PERMADDR_MSG_LEN];
	u8 *addr = (u8 *)(&msgbuf[1]);

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.stop_hw(hw);

	/* reset the api version */
	hw->api_version = ngbe_mbox_api_10;

	/* backup msix vectors */
	mbx->timeout = NGBE_VF_MBX_INIT_TIMEOUT;
	msgbuf[0] = NGBE_VF_BACKUP;
	mbx->write_posted(hw, msgbuf, 1, 0);
	msec_delay(10);

	DEBUGOUT("Issuing a function level reset to MAC");
	wr32(hw, NGBE_VFRST, NGBE_VFRST_SET);
	ngbe_flush(hw);
	msec_delay(50);

	hw->offset_loaded = 1;

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	while (!mbx->check_for_rst(hw, 0) && timeout) {
		timeout--;
		usec_delay(5);
	}

	if (!timeout)
		return NGBE_ERR_RESET_FAILED;

	/* Reset VF registers to initial values */
	ngbe_virt_clr_reg(hw);

	/* mailbox timeout can now become active */
	mbx->timeout = NGBE_VF_MBX_INIT_TIMEOUT;

	msgbuf[0] = NGBE_VF_RESET;
	mbx->write_posted(hw, msgbuf, 1, 0);

	msec_delay(10);

	/*
	 * set our "perm_addr" based on info provided by PF
	 * also set up the mc_filter_type which is piggy backed
	 * on the mac address in word 3
	 */
	ret_val = mbx->read_posted(hw, msgbuf,
			NGBE_VF_PERMADDR_MSG_LEN, 0);
	if (ret_val)
		return ret_val;

	if (msgbuf[0] != (NGBE_VF_RESET | NGBE_VT_MSGTYPE_ACK) &&
	    msgbuf[0] != (NGBE_VF_RESET | NGBE_VT_MSGTYPE_NACK))
		return NGBE_ERR_INVALID_MAC_ADDR;

	if (msgbuf[0] == (NGBE_VF_RESET | NGBE_VT_MSGTYPE_ACK))
		memcpy(hw->mac.perm_addr, addr, ETH_ADDR_LEN);

	hw->mac.mc_filter_type = msgbuf[NGBE_VF_MC_TYPE_WORD];

	return ret_val;
}

/**
 *  ngbe_stop_hw_vf - Generic stop Tx/Rx units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within ngbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 ngbe_stop_hw_vf(struct ngbe_hw *hw)
{
	u16 i;

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = true;

	/* Clear interrupt mask to stop from interrupts being generated */
	wr32(hw, NGBE_VFIMC, NGBE_VFIMC_MASK);

	/* Clear any pending interrupts, flush previous writes */
	wr32(hw, NGBE_VFICR, NGBE_VFICR_MASK);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		wr32(hw, NGBE_TXCFG(i), NGBE_TXCFG_FLUSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++)
		wr32m(hw, NGBE_RXCFG(i), NGBE_RXCFG_ENA, 0);

	/* Clear packet split and pool config */
	wr32(hw, NGBE_VFPLCFG, 0);
	hw->rx_loaded = 1;

	/* flush all queues disables */
	ngbe_flush(hw);
	msec_delay(2);

	return 0;
}

STATIC s32 ngbevf_write_msg_read_ack(struct ngbe_hw *hw, u32 *msg,
				      u32 *retmsg, u16 size)
{
	struct ngbe_mbx_info *mbx = &hw->mbx;
	s32 retval = mbx->write_posted(hw, msg, size, 0);

	if (retval)
		return retval;

	return mbx->read_posted(hw, retmsg, size, 0);
}

/**
 *  ngbe_set_rar_vf - set device MAC address
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 **/
s32 ngbe_set_rar_vf(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		     u32 enable_addr)
{
	u32 msgbuf[3];
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;

	UNREFERENCED_PARAMETER(vmdq, enable_addr, index);

	memset(msgbuf, 0, 12);
	msgbuf[0] = NGBE_VF_SET_MAC_ADDR;
	memcpy(msg_addr, addr, 6);
	ret_val = ngbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 3);

	msgbuf[0] &= ~NGBE_VT_MSGTYPE_CTS;

	/* if nacked the address was rejected, use "perm_addr" */
	if (!ret_val &&
	    (msgbuf[0] == (NGBE_VF_SET_MAC_ADDR | NGBE_VT_MSGTYPE_NACK))) {
		ngbe_get_mac_addr_vf(hw, hw->mac.addr);
		return NGBE_ERR_MBX;
	}

	return ret_val;
}

/**
 *  ngbevf_update_xcast_mode - Update Multicast mode
 *  @hw: pointer to the HW structure
 *  @xcast_mode: new multicast mode
 *
 *  Updates the Multicast Mode of VF.
 **/
s32 ngbevf_update_xcast_mode(struct ngbe_hw *hw, int xcast_mode)
{
	u32 msgbuf[2];
	s32 err;

	switch (hw->api_version) {
	case ngbe_mbox_api_12:
		/* New modes were introduced in 1.3 version */
		if (xcast_mode > NGBEVF_XCAST_MODE_ALLMULTI)
			return NGBE_ERR_FEATURE_NOT_SUPPORTED;
		/* Fall through */
	case ngbe_mbox_api_13:
		break;
	default:
		return NGBE_ERR_FEATURE_NOT_SUPPORTED;
	}

	msgbuf[0] = NGBE_VF_UPDATE_XCAST_MODE;
	msgbuf[1] = xcast_mode;

	err = ngbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 2);
	if (err)
		return err;

	msgbuf[0] &= ~NGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] == (NGBE_VF_UPDATE_XCAST_MODE | NGBE_VT_MSGTYPE_NACK))
		return NGBE_ERR_FEATURE_NOT_SUPPORTED;
	return 0;
}

/**
 * ngbe_get_mac_addr_vf - Read device MAC address
 * @hw: pointer to the HW structure
 * @mac_addr: the MAC address
 **/
s32 ngbe_get_mac_addr_vf(struct ngbe_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < ETH_ADDR_LEN; i++)
		mac_addr[i] = hw->mac.perm_addr[i];

	return 0;
}

s32 ngbevf_set_uc_addr_vf(struct ngbe_hw *hw, u32 index, u8 *addr)
{
	u32 msgbuf[3], msgbuf_chk;
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;

	memset(msgbuf, 0, sizeof(msgbuf));
	/*
	 * If index is one then this is the start of a new list and needs
	 * indication to the PF so it can do it's own list management.
	 * If it is zero then that tells the PF to just clear all of
	 * this VF's macvlans and there is no new list.
	 */
	msgbuf[0] |= index << NGBE_VT_MSGINFO_SHIFT;
	msgbuf[0] |= NGBE_VF_SET_MACVLAN;
	msgbuf_chk = msgbuf[0];
	if (addr)
		memcpy(msg_addr, addr, 6);

	ret_val = ngbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 3);
	if (!ret_val) {
		msgbuf[0] &= ~NGBE_VT_MSGTYPE_CTS;

		if (msgbuf[0] == (msgbuf_chk | NGBE_VT_MSGTYPE_NACK))
			return NGBE_ERR_OUT_OF_MEM;
	}

	return ret_val;
}

/**
 *  ngbevf_rlpml_set_vf - Set the maximum receive packet length
 *  @hw: pointer to the HW structure
 *  @max_size: value to assign to max frame size
 **/
s32 ngbevf_rlpml_set_vf(struct ngbe_hw *hw, u16 max_size)
{
	u32 msgbuf[2];
	s32 retval;

	msgbuf[0] = NGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

	retval = ngbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 2);
	if (retval)
		return retval;
	if ((msgbuf[0] & NGBE_VF_SET_LPE) &&
	    (msgbuf[0] & NGBE_VT_MSGTYPE_NACK))
		return NGBE_ERR_MBX;

	return 0;
}

/**
 *  ngbevf_negotiate_api_version - Negotiate supported API version
 *  @hw: pointer to the HW structure
 *  @api: integer containing requested API version
 **/
int ngbevf_negotiate_api_version(struct ngbe_hw *hw, int api)
{
	int err;
	u32 msg[3];

	/* Negotiate the mailbox API version */
	msg[0] = NGBE_VF_API_NEGOTIATE;
	msg[1] = api;
	msg[2] = 0;

	err = ngbevf_write_msg_read_ack(hw, msg, msg, 3);
	if (!err) {
		msg[0] &= ~NGBE_VT_MSGTYPE_CTS;

		/* Store value and return 0 on success */
		if (msg[0] == (NGBE_VF_API_NEGOTIATE | NGBE_VT_MSGTYPE_ACK)) {
			hw->api_version = api;
			return 0;
		}

		err = NGBE_ERR_INVALID_ARGUMENT;
	}

	return err;
}

int ngbevf_get_queues(struct ngbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc)
{
	int err, i;
	u32 msg[5];

	/* do nothing if API doesn't support ngbevf_get_queues */
	switch (hw->api_version) {
	case ngbe_mbox_api_11:
	case ngbe_mbox_api_12:
	case ngbe_mbox_api_13:
		break;
	default:
		return 0;
	}

	/* Fetch queue configuration from the PF */
	msg[0] = NGBE_VF_GET_QUEUES;
	for (i = 1; i < 5; i++)
		msg[i] = 0;

	err = ngbevf_write_msg_read_ack(hw, msg, msg, 5);
	if (!err) {
		msg[0] &= ~NGBE_VT_MSGTYPE_CTS;

		/*
		 * if we didn't get an ACK there must have been
		 * some sort of mailbox error so we should treat it
		 * as such
		 */
		if (msg[0] != (NGBE_VF_GET_QUEUES | NGBE_VT_MSGTYPE_ACK))
			return NGBE_ERR_MBX;

		/* record and validate values from message */
		hw->mac.max_tx_queues = msg[NGBE_VF_TX_QUEUES];
		if (hw->mac.max_tx_queues == 0 ||
		    hw->mac.max_tx_queues > NGBE_VF_MAX_TX_QUEUES)
			hw->mac.max_tx_queues = NGBE_VF_MAX_TX_QUEUES;

		hw->mac.max_rx_queues = msg[NGBE_VF_RX_QUEUES];
		if (hw->mac.max_rx_queues == 0 ||
		    hw->mac.max_rx_queues > NGBE_VF_MAX_RX_QUEUES)
			hw->mac.max_rx_queues = NGBE_VF_MAX_RX_QUEUES;

		*num_tcs = msg[NGBE_VF_TRANS_VLAN];
		/* in case of unknown state assume we cannot tag frames */
		if (*num_tcs > hw->mac.max_rx_queues)
			*num_tcs = 1;

		*default_tc = msg[NGBE_VF_DEF_QUEUE];
		/* default to queue 0 on out-of-bounds queue number */
		if (*default_tc >= hw->mac.max_tx_queues)
			*default_tc = 0;
	}

	return err;
}

/**
 *  ngbe_init_ops_vf - Initialize the pointers for vf
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers, adapter-specific functions can
 *  override the assignment of generic function pointers by assigning
 *  their own adapter-specific function pointers.
 *  Does not touch the hardware.
 **/
s32 ngbe_init_ops_vf(struct ngbe_hw *hw)
{
	struct ngbe_mac_info *mac = &hw->mac;
	struct ngbe_mbx_info *mbx = &hw->mbx;

	/* MAC */
	mac->init_hw = ngbe_init_hw_vf;
	mac->reset_hw = ngbe_reset_hw_vf;
	mac->start_hw = ngbe_start_hw_vf;
	mac->stop_hw = ngbe_stop_hw_vf;
	mac->get_mac_addr = ngbe_get_mac_addr_vf;
	mac->negotiate_api_version = ngbevf_negotiate_api_version;

	/* RAR, Multicast */
	mac->set_rar = ngbe_set_rar_vf;
	mac->set_uc_addr = ngbevf_set_uc_addr_vf;
	mac->update_xcast_mode = ngbevf_update_xcast_mode;
	mac->set_rlpml = ngbevf_rlpml_set_vf;

	mac->max_tx_queues = 1;
	mac->max_rx_queues = 1;

	mbx->init_params = ngbe_init_mbx_params_vf;
	mbx->read = ngbe_read_mbx_vf;
	mbx->write = ngbe_write_mbx_vf;
	mbx->read_posted = ngbe_read_posted_mbx;
	mbx->write_posted = ngbe_write_posted_mbx;
	mbx->check_for_msg = ngbe_check_for_msg_vf;
	mbx->check_for_ack = ngbe_check_for_ack_vf;
	mbx->check_for_rst = ngbe_check_for_rst_vf;

	return 0;
}
