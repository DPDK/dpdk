/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_mbx.h"
#include "txgbe_vf.h"

/**
 *  txgbe_init_ops_vf - Initialize the pointers for vf
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers, adapter-specific functions can
 *  override the assignment of generic function pointers by assigning
 *  their own adapter-specific function pointers.
 *  Does not touch the hardware.
 **/
s32 txgbe_init_ops_vf(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;
	struct txgbe_mbx_info *mbx = &hw->mbx;

	/* MAC */
	mac->reset_hw = txgbe_reset_hw_vf;
	mac->start_hw = txgbe_start_hw_vf;
	/* Cannot clear stats on VF */
	mac->get_mac_addr = txgbe_get_mac_addr_vf;
	mac->stop_hw = txgbe_stop_hw_vf;
	mac->negotiate_api_version = txgbevf_negotiate_api_version;
	mac->update_mc_addr_list = txgbe_update_mc_addr_list_vf;

	/* Link */
	mac->check_link = txgbe_check_mac_link_vf;

	/* RAR, Multicast, VLAN */
	mac->set_rar = txgbe_set_rar_vf;
	mac->set_uc_addr = txgbevf_set_uc_addr_vf;
	mac->update_xcast_mode = txgbevf_update_xcast_mode;
	mac->set_vfta = txgbe_set_vfta_vf;
	mac->set_rlpml = txgbevf_rlpml_set_vf;

	mac->max_tx_queues = 1;
	mac->max_rx_queues = 1;

	mbx->init_params = txgbe_init_mbx_params_vf;
	mbx->read = txgbe_read_mbx_vf;
	mbx->write = txgbe_write_mbx_vf;
	mbx->read_posted = txgbe_read_posted_mbx;
	mbx->write_posted = txgbe_write_posted_mbx;
	mbx->check_for_msg = txgbe_check_for_msg_vf;
	mbx->check_for_ack = txgbe_check_for_ack_vf;
	mbx->check_for_rst = txgbe_check_for_rst_vf;

	return 0;
}

/* txgbe_virt_clr_reg - Set register to default (power on) state.
 * @hw: pointer to hardware structure
 */
static void txgbe_virt_clr_reg(struct txgbe_hw *hw)
{
	int i;
	u32 vfsrrctl;

	/* default values (BUF_SIZE = 2048, HDR_SIZE = 256) */
	vfsrrctl = TXGBE_RXCFG_HDRLEN(TXGBE_RX_HDR_SIZE);
	vfsrrctl |= TXGBE_RXCFG_PKTLEN(TXGBE_RX_BUF_SIZE);

	for (i = 0; i < 8; i++) {
		wr32m(hw, TXGBE_RXCFG(i),
			(TXGBE_RXCFG_HDRLEN_MASK | TXGBE_RXCFG_PKTLEN_MASK),
			vfsrrctl);
	}

	txgbe_flush(hw);
}

/**
 *  txgbe_start_hw_vf - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware by filling the bus info structure and media type, clears
 *  all on chip counters, initializes receive address registers, multicast
 *  table, VLAN filter table, calls routine to set up link and flow control
 *  settings, and leaves transmit and receive units disabled and uninitialized
 **/
s32 txgbe_start_hw_vf(struct txgbe_hw *hw)
{
	/* Clear adapter stopped flag */
	hw->adapter_stopped = false;

	return 0;
}

/**
 *  txgbe_reset_hw_vf - Performs hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks and
 *  clears all interrupts.
 **/
s32 txgbe_reset_hw_vf(struct txgbe_hw *hw)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	u32 timeout = TXGBE_VF_INIT_TIMEOUT;
	s32 ret_val = TXGBE_ERR_INVALID_MAC_ADDR;
	u32 msgbuf[TXGBE_VF_PERMADDR_MSG_LEN];
	u8 *addr = (u8 *)(&msgbuf[1]);

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.stop_hw(hw);

	/* reset the api version */
	hw->api_version = txgbe_mbox_api_10;

	/* backup msix vectors */
	mbx->timeout = TXGBE_VF_MBX_INIT_TIMEOUT;
	msgbuf[0] = TXGBE_VF_BACKUP;
	mbx->write_posted(hw, msgbuf, 1, 0);
	msec_delay(10);

	DEBUGOUT("Issuing a function level reset to MAC");
	wr32(hw, TXGBE_VFRST, TXGBE_VFRST_SET);
	txgbe_flush(hw);
	msec_delay(50);

	hw->offset_loaded = 1;

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	while (!mbx->check_for_rst(hw, 0) && timeout) {
		timeout--;
		/* if it doesn't work, try in 1 ms */
		usec_delay(5);
	}

	if (!timeout)
		return TXGBE_ERR_RESET_FAILED;

	/* Reset VF registers to initial values */
	txgbe_virt_clr_reg(hw);

	/* mailbox timeout can now become active */
	mbx->timeout = TXGBE_VF_MBX_INIT_TIMEOUT;

	msgbuf[0] = TXGBE_VF_RESET;
	mbx->write_posted(hw, msgbuf, 1, 0);

	msec_delay(10);

	/*
	 * set our "perm_addr" based on info provided by PF
	 * also set up the mc_filter_type which is piggy backed
	 * on the mac address in word 3
	 */
	ret_val = mbx->read_posted(hw, msgbuf,
			TXGBE_VF_PERMADDR_MSG_LEN, 0);
	if (ret_val)
		return ret_val;

	if (msgbuf[0] != (TXGBE_VF_RESET | TXGBE_VT_MSGTYPE_ACK) &&
	    msgbuf[0] != (TXGBE_VF_RESET | TXGBE_VT_MSGTYPE_NACK))
		return TXGBE_ERR_INVALID_MAC_ADDR;

	if (msgbuf[0] == (TXGBE_VF_RESET | TXGBE_VT_MSGTYPE_ACK))
		memcpy(hw->mac.perm_addr, addr, ETH_ADDR_LEN);

	hw->mac.mc_filter_type = msgbuf[TXGBE_VF_MC_TYPE_WORD];

	return ret_val;
}

/**
 *  txgbe_stop_hw_vf - Generic stop Tx/Rx units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within txgbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 txgbe_stop_hw_vf(struct txgbe_hw *hw)
{
	u16 i;

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = true;

	/* Clear interrupt mask to stop from interrupts being generated */
	wr32(hw, TXGBE_VFIMC, TXGBE_VFIMC_MASK);

	/* Clear any pending interrupts, flush previous writes */
	wr32(hw, TXGBE_VFICR, TXGBE_VFICR_MASK);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		wr32(hw, TXGBE_TXCFG(i), TXGBE_TXCFG_FLUSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++)
		wr32m(hw, TXGBE_RXCFG(i), TXGBE_RXCFG_ENA, 0);

	/* Clear packet split and pool config */
	wr32(hw, TXGBE_VFPLCFG, 0);
	hw->rx_loaded = 1;

	/* flush all queues disables */
	txgbe_flush(hw);
	msec_delay(2);

	return 0;
}

/**
 *  txgbe_mta_vector - Determines bit-vector in multicast table to set
 *  @hw: pointer to hardware structure
 *  @mc_addr: the multicast address
 **/
STATIC s32 txgbe_mta_vector(struct txgbe_hw *hw, u8 *mc_addr)
{
	u32 vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((mc_addr[4] >> 4) | (((u16)mc_addr[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((mc_addr[4] >> 3) | (((u16)mc_addr[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((mc_addr[4] >> 2) | (((u16)mc_addr[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((mc_addr[4]) | (((u16)mc_addr[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		DEBUGOUT("MC filter type param set incorrectly");
		ASSERT(0);
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

STATIC s32 txgbevf_write_msg_read_ack(struct txgbe_hw *hw, u32 *msg,
				      u32 *retmsg, u16 size)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	s32 retval = mbx->write_posted(hw, msg, size, 0);

	if (retval)
		return retval;

	return mbx->read_posted(hw, retmsg, size, 0);
}

/**
 *  txgbe_set_rar_vf - set device MAC address
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 **/
s32 txgbe_set_rar_vf(struct txgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		     u32 enable_addr)
{
	u32 msgbuf[3];
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;
	UNREFERENCED_PARAMETER(vmdq, enable_addr, index);

	memset(msgbuf, 0, 12);
	msgbuf[0] = TXGBE_VF_SET_MAC_ADDR;
	memcpy(msg_addr, addr, 6);
	ret_val = txgbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 3);

	msgbuf[0] &= ~TXGBE_VT_MSGTYPE_CTS;

	/* if nacked the address was rejected, use "perm_addr" */
	if (!ret_val &&
	    (msgbuf[0] == (TXGBE_VF_SET_MAC_ADDR | TXGBE_VT_MSGTYPE_NACK))) {
		txgbe_get_mac_addr_vf(hw, hw->mac.addr);
		return TXGBE_ERR_MBX;
	}

	return ret_val;
}

/**
 *  txgbe_update_mc_addr_list_vf - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *  @next: caller supplied function to return next address in list
 *  @clear: unused
 *
 *  Updates the Multicast Table Array.
 **/
s32 txgbe_update_mc_addr_list_vf(struct txgbe_hw *hw, u8 *mc_addr_list,
				 u32 mc_addr_count, txgbe_mc_addr_itr next,
				 bool clear)
{
	struct txgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[TXGBE_P2VMBX_SIZE];
	u16 *vector_list = (u16 *)&msgbuf[1];
	u32 vector;
	u32 cnt, i;
	u32 vmdq;

	UNREFERENCED_PARAMETER(clear);

	/* Each entry in the list uses 1 16 bit word.  We have 30
	 * 16 bit words available in our HW msg buffer (minus 1 for the
	 * msg type).  That's 30 hash values if we pack 'em right.  If
	 * there are more than 30 MC addresses to add then punt the
	 * extras for now and then add code to handle more than 30 later.
	 * It would be unusual for a server to request that many multi-cast
	 * addresses except for in large enterprise network environments.
	 */

	DEBUGOUT("MC Addr Count = %d", mc_addr_count);

	cnt = (mc_addr_count > 30) ? 30 : mc_addr_count;
	msgbuf[0] = TXGBE_VF_SET_MULTICAST;
	msgbuf[0] |= cnt << TXGBE_VT_MSGINFO_SHIFT;

	for (i = 0; i < cnt; i++) {
		vector = txgbe_mta_vector(hw, next(hw, &mc_addr_list, &vmdq));
		DEBUGOUT("Hash value = 0x%03X", vector);
		vector_list[i] = (u16)vector;
	}

	return mbx->write_posted(hw, msgbuf, TXGBE_P2VMBX_SIZE, 0);
}

/**
 *  txgbevf_update_xcast_mode - Update Multicast mode
 *  @hw: pointer to the HW structure
 *  @xcast_mode: new multicast mode
 *
 *  Updates the Multicast Mode of VF.
 **/
s32 txgbevf_update_xcast_mode(struct txgbe_hw *hw, int xcast_mode)
{
	u32 msgbuf[2];
	s32 err;

	switch (hw->api_version) {
	case txgbe_mbox_api_12:
		/* New modes were introduced in 1.3 version */
		if (xcast_mode > TXGBEVF_XCAST_MODE_ALLMULTI)
			return TXGBE_ERR_FEATURE_NOT_SUPPORTED;
		/* Fall through */
	case txgbe_mbox_api_13:
		break;
	default:
		return TXGBE_ERR_FEATURE_NOT_SUPPORTED;
	}

	msgbuf[0] = TXGBE_VF_UPDATE_XCAST_MODE;
	msgbuf[1] = xcast_mode;

	err = txgbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 2);
	if (err)
		return err;

	msgbuf[0] &= ~TXGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] == (TXGBE_VF_UPDATE_XCAST_MODE | TXGBE_VT_MSGTYPE_NACK))
		return TXGBE_ERR_FEATURE_NOT_SUPPORTED;
	return 0;
}

/**
 *  txgbe_set_vfta_vf - Set/Unset vlan filter table address
 *  @hw: pointer to the HW structure
 *  @vlan: 12 bit VLAN ID
 *  @vind: unused by VF drivers
 *  @vlan_on: if true then set bit, else clear bit
 *  @vlvf_bypass: boolean flag indicating updating default pool is okay
 *
 *  Turn on/off specified VLAN in the VLAN filter table.
 **/
s32 txgbe_set_vfta_vf(struct txgbe_hw *hw, u32 vlan, u32 vind,
		      bool vlan_on, bool vlvf_bypass)
{
	u32 msgbuf[2];
	s32 ret_val;
	UNREFERENCED_PARAMETER(vind, vlvf_bypass);

	msgbuf[0] = TXGBE_VF_SET_VLAN;
	msgbuf[1] = vlan;
	/* Setting the 8 bit field MSG INFO to TRUE indicates "add" */
	msgbuf[0] |= vlan_on << TXGBE_VT_MSGINFO_SHIFT;

	ret_val = txgbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 2);
	if (!ret_val && (msgbuf[0] & TXGBE_VT_MSGTYPE_ACK))
		return 0;

	return ret_val | (msgbuf[0] & TXGBE_VT_MSGTYPE_NACK);
}

/**
 * txgbe_get_mac_addr_vf - Read device MAC address
 * @hw: pointer to the HW structure
 * @mac_addr: the MAC address
 **/
s32 txgbe_get_mac_addr_vf(struct txgbe_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < ETH_ADDR_LEN; i++)
		mac_addr[i] = hw->mac.perm_addr[i];

	return 0;
}

s32 txgbevf_set_uc_addr_vf(struct txgbe_hw *hw, u32 index, u8 *addr)
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
	msgbuf[0] |= index << TXGBE_VT_MSGINFO_SHIFT;
	msgbuf[0] |= TXGBE_VF_SET_MACVLAN;
	msgbuf_chk = msgbuf[0];
	if (addr)
		memcpy(msg_addr, addr, 6);

	ret_val = txgbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 3);
	if (!ret_val) {
		msgbuf[0] &= ~TXGBE_VT_MSGTYPE_CTS;

		if (msgbuf[0] == (msgbuf_chk | TXGBE_VT_MSGTYPE_NACK))
			return TXGBE_ERR_OUT_OF_MEM;
	}

	return ret_val;
}

/**
 *  txgbe_check_mac_link_vf - Get link/speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: true is link is up, false otherwise
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 txgbe_check_mac_link_vf(struct txgbe_hw *hw, u32 *speed,
			    bool *link_up, bool wait_to_complete)
{
	/**
	 * for a quick link status checking, wait_to_compelet == 0,
	 * skip PF link status checking
	 */
	bool no_pflink_check = wait_to_complete == 0;
	struct txgbe_mbx_info *mbx = &hw->mbx;
	struct txgbe_mac_info *mac = &hw->mac;
	s32 ret_val = 0;
	u32 links_reg;
	u32 in_msg = 0;

	/* If we were hit with a reset drop the link */
	if (!mbx->check_for_rst(hw, 0) || !mbx->timeout)
		mac->get_link_status = true;

	if (!mac->get_link_status)
		goto out;

	/* if link status is down no point in checking to see if pf is up */
	links_reg = rd32(hw, TXGBE_VFSTATUS);
	if (!(links_reg & TXGBE_VFSTATUS_UP))
		goto out;

	/* for SFP+ modules and DA cables it can take up to 500usecs
	 * before the link status is correct
	 */
	if (mac->type == txgbe_mac_raptor_vf && wait_to_complete) {
		if (po32m(hw, TXGBE_VFSTATUS, TXGBE_VFSTATUS_UP,
			0, NULL, 5, 100))
			goto out;
	}

	switch (links_reg & TXGBE_VFSTATUS_BW_MASK) {
	case TXGBE_VFSTATUS_BW_10G:
		*speed = TXGBE_LINK_SPEED_10GB_FULL;
		break;
	case TXGBE_VFSTATUS_BW_1G:
		*speed = TXGBE_LINK_SPEED_1GB_FULL;
		break;
	case TXGBE_VFSTATUS_BW_100M:
		*speed = TXGBE_LINK_SPEED_100M_FULL;
		break;
	default:
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
	}

	if (no_pflink_check) {
		if (*speed == TXGBE_LINK_SPEED_UNKNOWN)
			mac->get_link_status = true;
		else
			mac->get_link_status = false;

		goto out;
	}

	/* if the read failed it could just be a mailbox collision, best wait
	 * until we are called again and don't report an error
	 */
	if (mbx->read(hw, &in_msg, 1, 0))
		goto out;

	if (!(in_msg & TXGBE_VT_MSGTYPE_CTS)) {
		/* msg is not CTS and is NACK we must have lost CTS status */
		if (in_msg & TXGBE_VT_MSGTYPE_NACK)
			ret_val = -1;
		goto out;
	}

	/* the pf is talking, if we timed out in the past we reinit */
	if (!mbx->timeout) {
		ret_val = -1;
		goto out;
	}

	/* if we passed all the tests above then the link is up and we no
	 * longer need to check for link
	 */
	mac->get_link_status = false;

out:
	*link_up = !mac->get_link_status;
	return ret_val;
}

/**
 *  txgbevf_rlpml_set_vf - Set the maximum receive packet length
 *  @hw: pointer to the HW structure
 *  @max_size: value to assign to max frame size
 **/
s32 txgbevf_rlpml_set_vf(struct txgbe_hw *hw, u16 max_size)
{
	u32 msgbuf[2];
	s32 retval;

	msgbuf[0] = TXGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

	retval = txgbevf_write_msg_read_ack(hw, msgbuf, msgbuf, 2);
	if (retval)
		return retval;
	if ((msgbuf[0] & TXGBE_VF_SET_LPE) &&
	    (msgbuf[0] & TXGBE_VT_MSGTYPE_NACK))
		return TXGBE_ERR_MBX;

	return 0;
}

/**
 *  txgbevf_negotiate_api_version - Negotiate supported API version
 *  @hw: pointer to the HW structure
 *  @api: integer containing requested API version
 **/
int txgbevf_negotiate_api_version(struct txgbe_hw *hw, int api)
{
	int err;
	u32 msg[3];

	/* Negotiate the mailbox API version */
	msg[0] = TXGBE_VF_API_NEGOTIATE;
	msg[1] = api;
	msg[2] = 0;

	err = txgbevf_write_msg_read_ack(hw, msg, msg, 3);
	if (!err) {
		msg[0] &= ~TXGBE_VT_MSGTYPE_CTS;

		/* Store value and return 0 on success */
		if (msg[0] == (TXGBE_VF_API_NEGOTIATE | TXGBE_VT_MSGTYPE_ACK)) {
			hw->api_version = api;
			return 0;
		}

		err = TXGBE_ERR_INVALID_ARGUMENT;
	}

	return err;
}

int txgbevf_get_queues(struct txgbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc)
{
	int err, i;
	u32 msg[5];

	/* do nothing if API doesn't support txgbevf_get_queues */
	switch (hw->api_version) {
	case txgbe_mbox_api_11:
	case txgbe_mbox_api_12:
	case txgbe_mbox_api_13:
		break;
	default:
		return 0;
	}

	/* Fetch queue configuration from the PF */
	msg[0] = TXGBE_VF_GET_QUEUES;
	for (i = 1; i < 5; i++)
		msg[i] = 0;

	err = txgbevf_write_msg_read_ack(hw, msg, msg, 5);
	if (!err) {
		msg[0] &= ~TXGBE_VT_MSGTYPE_CTS;

		/*
		 * if we didn't get an ACK there must have been
		 * some sort of mailbox error so we should treat it
		 * as such
		 */
		if (msg[0] != (TXGBE_VF_GET_QUEUES | TXGBE_VT_MSGTYPE_ACK))
			return TXGBE_ERR_MBX;

		/* record and validate values from message */
		hw->mac.max_tx_queues = msg[TXGBE_VF_TX_QUEUES];
		if (hw->mac.max_tx_queues == 0 ||
		    hw->mac.max_tx_queues > TXGBE_VF_MAX_TX_QUEUES)
			hw->mac.max_tx_queues = TXGBE_VF_MAX_TX_QUEUES;

		hw->mac.max_rx_queues = msg[TXGBE_VF_RX_QUEUES];
		if (hw->mac.max_rx_queues == 0 ||
		    hw->mac.max_rx_queues > TXGBE_VF_MAX_RX_QUEUES)
			hw->mac.max_rx_queues = TXGBE_VF_MAX_RX_QUEUES;

		*num_tcs = msg[TXGBE_VF_TRANS_VLAN];
		/* in case of unknown state assume we cannot tag frames */
		if (*num_tcs > hw->mac.max_rx_queues)
			*num_tcs = 1;

		*default_tc = msg[TXGBE_VF_DEF_QUEUE];
		/* default to queue 0 on out-of-bounds queue number */
		if (*default_tc >= hw->mac.max_tx_queues)
			*default_tc = 0;
	}

	return err;
}
