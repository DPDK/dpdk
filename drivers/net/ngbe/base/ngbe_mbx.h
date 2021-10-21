/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_MBX_H_
#define _NGBE_MBX_H_

#define NGBE_ERR_MBX		-100

/* If it's a NGBE_VF_* msg then it originates in the VF and is sent to the
 * PF.  The reverse is true if it is NGBE_PF_*.
 * Message ACK's are the value or'd with 0xF0000000
 */
/* Messages below or'd with this are the ACK */
#define NGBE_VT_MSGTYPE_ACK	0x80000000
/* Messages below or'd with this are the NACK */
#define NGBE_VT_MSGTYPE_NACK	0x40000000
/* Indicates that VF is still clear to send requests */
#define NGBE_VT_MSGTYPE_CTS	0x20000000

#define NGBE_VT_MSGINFO_SHIFT	16
/* bits 23:16 are used for extra info for certain messages */
#define NGBE_VT_MSGINFO_MASK	(0xFF << NGBE_VT_MSGINFO_SHIFT)

/*
 * each element denotes a version of the API; existing numbers may not
 * change; any additions must go at the end
 */
enum ngbe_pfvf_api_rev {
	ngbe_mbox_api_null,
	ngbe_mbox_api_10,	/* API version 1.0, linux/freebsd VF driver */
	ngbe_mbox_api_11,	/* API version 1.1, linux/freebsd VF driver */
	ngbe_mbox_api_12,	/* API version 1.2, linux/freebsd VF driver */
	ngbe_mbox_api_13,	/* API version 1.3, linux/freebsd VF driver */
	ngbe_mbox_api_20,	/* API version 2.0, solaris Phase1 VF driver */
	/* This value should always be last */
	ngbe_mbox_api_unknown,	/* indicates that API version is not known */
};

/* mailbox API, legacy requests */
#define NGBE_VF_RESET		0x01 /* VF requests reset */
#define NGBE_VF_SET_MAC_ADDR	0x02 /* VF requests PF to set MAC addr */
#define NGBE_VF_SET_MULTICAST	0x03 /* VF requests PF to set MC addr */
#define NGBE_VF_SET_VLAN	0x04 /* VF requests PF to set VLAN */

/* mailbox API, version 1.0 VF requests */
#define NGBE_VF_SET_LPE	0x05 /* VF requests PF to set VMOLR.LPE */
#define NGBE_VF_SET_MACVLAN	0x06 /* VF requests PF for unicast filter */
#define NGBE_VF_API_NEGOTIATE	0x08 /* negotiate API version */

/* mailbox API, version 1.1 VF requests */
#define NGBE_VF_GET_QUEUES	0x09 /* get queue configuration */

/* mailbox API, version 1.2 VF requests */
#define NGBE_VF_GET_RETA      0x0a    /* VF request for RETA */
#define NGBE_VF_GET_RSS_KEY	0x0b    /* get RSS key */
#define NGBE_VF_UPDATE_XCAST_MODE	0x0c

/* mode choices for NGBE_VF_UPDATE_XCAST_MODE */
enum ngbevf_xcast_modes {
	NGBEVF_XCAST_MODE_NONE = 0,
	NGBEVF_XCAST_MODE_MULTI,
	NGBEVF_XCAST_MODE_ALLMULTI,
	NGBEVF_XCAST_MODE_PROMISC,
};

/* GET_QUEUES return data indices within the mailbox */
#define NGBE_VF_TX_QUEUES	1	/* number of Tx queues supported */
#define NGBE_VF_RX_QUEUES	2	/* number of Rx queues supported */
#define NGBE_VF_TRANS_VLAN	3	/* Indication of port vlan */
#define NGBE_VF_DEF_QUEUE	4	/* Default queue offset */

/* length of permanent address message returned from PF */
#define NGBE_VF_PERMADDR_MSG_LEN	4
s32 ngbe_read_mbx(struct ngbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 ngbe_write_mbx(struct ngbe_hw *hw, u32 *msg, u16 size, u16 mbx_id);
s32 ngbe_check_for_msg(struct ngbe_hw *hw, u16 mbx_id);
s32 ngbe_check_for_ack(struct ngbe_hw *hw, u16 mbx_id);
s32 ngbe_check_for_rst(struct ngbe_hw *hw, u16 mbx_id);
void ngbe_init_mbx_params_pf(struct ngbe_hw *hw);

s32 ngbe_read_mbx_pf(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
s32 ngbe_write_mbx_pf(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
s32 ngbe_check_for_msg_pf(struct ngbe_hw *hw, u16 vf_number);
s32 ngbe_check_for_ack_pf(struct ngbe_hw *hw, u16 vf_number);
s32 ngbe_check_for_rst_pf(struct ngbe_hw *hw, u16 vf_number);

#endif /* _NGBE_MBX_H_ */
