/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_mbx.h"
#include "ngbe_vf.h"

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

	mac->negotiate_api_version = ngbevf_negotiate_api_version;

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
