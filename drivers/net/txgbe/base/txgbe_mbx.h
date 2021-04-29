/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_MBX_H_
#define _TXGBE_MBX_H_

#include "txgbe_type.h"

#define TXGBE_ERR_MBX		-100

/* If it's a TXGBE_VF_* msg then it originates in the VF and is sent to the
 * PF.  The reverse is true if it is TXGBE_PF_*.
 * Message ACK's are the value or'd with 0xF0000000
 */
/* Messages below or'd with this are the ACK */
#define TXGBE_VT_MSGTYPE_ACK	0x80000000
/* Messages below or'd with this are the NACK */
#define TXGBE_VT_MSGTYPE_NACK	0x40000000
/* Indicates that VF is still clear to send requests */
#define TXGBE_VT_MSGTYPE_CTS	0x20000000

#define TXGBE_VT_MSGINFO_SHIFT	16
/* bits 23:16 are used for extra info for certain messages */
#define TXGBE_VT_MSGINFO_MASK	(0xFF << TXGBE_VT_MSGINFO_SHIFT)

/* definitions to support mailbox API version negotiation */

/*
 * each element denotes a version of the API; existing numbers may not
 * change; any additions must go at the end
 */
enum txgbe_pfvf_api_rev {
	txgbe_mbox_api_null,
	txgbe_mbox_api_10,	/* API version 1.0, linux/freebsd VF driver */
	txgbe_mbox_api_11,	/* API version 1.1, linux/freebsd VF driver */
	txgbe_mbox_api_12,	/* API version 1.2, linux/freebsd VF driver */
	txgbe_mbox_api_13,	/* API version 1.3, linux/freebsd VF driver */
	txgbe_mbox_api_20,	/* API version 2.0, solaris Phase1 VF driver */
	/* This value should always be last */
	txgbe_mbox_api_unknown,	/* indicates that API version is not known */
};

/* mailbox API, legacy requests */
#define TXGBE_VF_RESET		0x01 /* VF requests reset */
#define TXGBE_VF_SET_MAC_ADDR	0x02 /* VF requests PF to set MAC addr */
#define TXGBE_VF_SET_MULTICAST	0x03 /* VF requests PF to set MC addr */
#define TXGBE_VF_SET_VLAN	0x04 /* VF requests PF to set VLAN */

/* mailbox API, version 1.0 VF requests */
#define TXGBE_VF_SET_LPE	0x05 /* VF requests PF to set VMOLR.LPE */
#define TXGBE_VF_SET_MACVLAN	0x06 /* VF requests PF for unicast filter */
#define TXGBE_VF_API_NEGOTIATE	0x08 /* negotiate API version */

/* mailbox API, version 1.1 VF requests */
#define TXGBE_VF_GET_QUEUES	0x09 /* get queue configuration */

/* mailbox API, version 1.2 VF requests */
#define TXGBE_VF_GET_RETA      0x0a    /* VF request for RETA */
#define TXGBE_VF_GET_RSS_KEY	0x0b    /* get RSS key */
#define TXGBE_VF_UPDATE_XCAST_MODE	0x0c

#define TXGBE_VF_BACKUP		0x8001 /* VF requests backup */

/* mode choices for TXGBE_VF_UPDATE_XCAST_MODE */
enum txgbevf_xcast_modes {
	TXGBEVF_XCAST_MODE_NONE = 0,
	TXGBEVF_XCAST_MODE_MULTI,
	TXGBEVF_XCAST_MODE_ALLMULTI,
	TXGBEVF_XCAST_MODE_PROMISC,
};

/* GET_QUEUES return data indices within the mailbox */
#define TXGBE_VF_TX_QUEUES	1	/* number of Tx queues supported */
#define TXGBE_VF_RX_QUEUES	2	/* number of Rx queues supported */
#define TXGBE_VF_TRANS_VLAN	3	/* Indication of port vlan */
#define TXGBE_VF_DEF_QUEUE	4	/* Default queue offset */

/* length of permanent address message returned from PF */
#define TXGBE_VF_PERMADDR_MSG_LEN	4
/* word in permanent address message with the current multicast type */
#define TXGBE_VF_MC_TYPE_WORD		3

#define TXGBE_PF_CONTROL_MSG		0x0100 /* PF control message */

#define TXGBE_VF_MBX_INIT_TIMEOUT	2000 /* number of retries on mailbox */
#define TXGBE_VF_MBX_INIT_DELAY		500  /* microseconds between retries */

s32 txgbe_read_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_write_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_read_posted_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_write_posted_mbx(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_check_for_msg(struct txgbe_hw *hw, u16 mbx_id);
s32 txgbe_check_for_ack(struct txgbe_hw *hw, u16 mbx_id);
s32 txgbe_check_for_rst(struct txgbe_hw *hw, u16 mbx_id);
void txgbe_init_mbx_params_vf(struct txgbe_hw *hw);
void txgbe_init_mbx_params_pf(struct txgbe_hw *hw);

s32 txgbe_read_mbx_pf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
s32 txgbe_write_mbx_pf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
s32 txgbe_check_for_msg_pf(struct txgbe_hw *hw, u16 vf_number);
s32 txgbe_check_for_ack_pf(struct txgbe_hw *hw, u16 vf_number);
s32 txgbe_check_for_rst_pf(struct txgbe_hw *hw, u16 vf_number);

s32 txgbe_read_mbx_vf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_write_mbx_vf(struct txgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 txgbe_check_for_msg_vf(struct txgbe_hw *hw, u16 mbx_id);
s32 txgbe_check_for_ack_vf(struct txgbe_hw *hw, u16 mbx_id);
s32 txgbe_check_for_rst_vf(struct txgbe_hw *hw, u16 mbx_id);

#endif /* _TXGBE_MBX_H_ */
