/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_CMSG_H__
#define __NFP_NET_CMSG_H__

#include "nfp_net_common.h"

/**
 * Match EtherType data
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-------------------------------+-------------------------------+
 *    0  |                               |            Ethtype            |
 *       +-----------------+-------------+-------------------------------+
 */
struct nfp_net_cmsg_match_eth {
	uint16_t ether_type;
	uint16_t spare;
};

/**
 * Match IPv4 data
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-----+-------------------------+---------------+---------------+
 *    0  |     |        Position         |   L4 Proto    | L4 Proto Mask |
 *       +-----+-------------------------+---------------+---------------+
 *    1  |                             SIP4                              |
 *       +---------------------------------------------------------------+
 *    2  |                           SIP4 Mask                           |
 *       +---------------------------------------------------------------+
 *    3  |                             DIP4                              |
 *       +---------------------------------------------------------------+
 *    4  |                           DIP4 Mask                           |
 *       +-------------------------------+-------------------------------+
 *    5  |             SPort             |           SPort Mask          |
 *       +-------------------------------+-------------------------------+
 *    6  |             DPort             |           DPort Mask          |
 *       +-----------------+-------------+-------------------------------+
 *
 * Position – Position index of the rule, 13bits.
 *            As priority, smaller value indicates higher priority.
 */
struct nfp_net_cmsg_match_v4 {
	uint8_t l4_protocol_mask;
	uint8_t l4_protocol;
	uint16_t position;
	uint32_t src_ipv4;
	uint32_t src_ipv4_mask;
	uint32_t dst_ipv4;
	uint32_t dst_ipv4_mask;
	uint16_t src_port_mask;
	uint16_t src_port;
	uint16_t dst_port_mask;
	uint16_t dst_port;
};

/**
 * Match IPv6 data
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-----+-------------------------+---------------+---------------+
 *    0  |     |        Position         |   L4 Proto    | L4 Proto Mask |
 *       +-----+-------------------------+---------------+---------------+
 *    1  |                            SIP6 (0-3B)                        |
 *       +---------------------------------------------------------------+
 *    2  |                            SIP6 (4-7B)                        |
 *       +---------------------------------------------------------------+
 *    3  |                            SIP6 (8-11B)                       |
 *       +---------------------------------------------------------------+
 *    4  |                            SIP6 (12-15B)                      |
 *       +---------------------------------------------------------------+
 *    5  |                         SIP6 Mask (0-3B)                      |
 *       +---------------------------------------------------------------+
 *    6  |                         SIP6 Mask (4-7B)                      |
 *       +---------------------------------------------------------------+
 *    7  |                         SIP6 Mask (8-11B)                     |
 *       +---------------------------------------------------------------+
 *    8  |                         SIP6 Mask (12-15B)                    |
 *       +---------------------------------------------------------------+
 *    9  |                            DIP6 (0-3B)                        |
 *       +---------------------------------------------------------------+
 *    10 |                            DIP6 (4-7B)                        |
 *       +---------------------------------------------------------------+
 *    11 |                            DIP6 (8-11B)                       |
 *       +---------------------------------------------------------------+
 *    12 |                            DIP6 (12-15B)                      |
 *       +---------------------------------------------------------------+
 *    13 |                         DIP6 Mask (0-3B)                      |
 *       +---------------------------------------------------------------+
 *    14 |                         DIP6 Mask (4-7B)                      |
 *       +---------------------------------------------------------------+
 *    15 |                         DIP6 Mask (8-11B)                     |
 *       +---------------------------------------------------------------+
 *    16 |                         DIP6 Mask (12-15B)                    |
 *       +-------------------------------+-------------------------------+
 *    17 |             SPort             |           SPort Mask          |
 *       +-------------------------------+-------------------------------+
 *    18 |             DPort             |           DPort Mask          |
 *       +-----------------+-------------+-------------------------------+
 *
 * Position – Position index of the rule, 13bits.
 *            As priority, smaller value indicates higher priority.
 */
struct nfp_net_cmsg_match_v6 {
	uint8_t l4_protocol_mask;
	uint8_t l4_protocol;
	uint16_t position;
	uint8_t src_ipv6[16];
	uint8_t src_ipv6_mask[16];
	uint8_t dst_ipv6[16];
	uint8_t dst_ipv6_mask[16];
	uint16_t src_port_mask;
	uint16_t src_port;
	uint16_t dst_port_mask;
	uint16_t dst_port;
};

#define NFP_NET_CMSG_ACTION_DROP          (0x1 << 0) /* Drop action */
#define NFP_NET_CMSG_ACTION_QUEUE         (0x1 << 1) /* Queue action */
#define NFP_NET_CMSG_ACTION_MARK          (0x1 << 2) /* Mark action */

/**
 * Action data
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-------------------------------+-------------------------------+
 *    0  |                 |    Queue    |              Actions          |
 *       +-----------------+-------------+-------------------------------+
 *    1  |                         Mark ID                               |
 *       +---------------------------------------------------------------+
 *
 * Queue – Queue ID, 7 bits.
 * Actions – An action bitmap, each bit represents an action type:
 *       - Bit 0: Drop action. Drop the packet.
 *       - Bit 1: Queue action. Use the queue specified by “Queue” field.
 *                If not set, the queue is usually specified by RSS.
 *       - Bit 2: Mark action. Mark packet with Mark ID.
 */
struct nfp_net_cmsg_action {
	uint16_t action;
	uint8_t queue;
	uint8_t spare;
	uint32_t mark_id;
};

enum nfp_net_cfg_mbox_cmd {
	NFP_NET_CFG_MBOX_CMD_FS_ADD_V4,       /* Add Flow Steer rule for V4 table */
	NFP_NET_CFG_MBOX_CMD_FS_DEL_V4,       /* Delete Flow Steer rule for V4 table */
	NFP_NET_CFG_MBOX_CMD_FS_ADD_V6,       /* Add Flow Steer rule for V4 table */
	NFP_NET_CFG_MBOX_CMD_FS_DEL_V6,       /* Delete Flow Steer rule for V4 table */
	NFP_NET_CFG_MBOX_CMD_FS_ADD_ETHTYPE,  /* Add Flow Steer rule for Ethtype table */
	NFP_NET_CFG_MBOX_CMD_FS_DEL_ETHTYPE,  /* Delete Flow Steer rule for Ethtype table */
};

enum nfp_net_cfg_mbox_ret {
	NFP_NET_CFG_MBOX_RET_FS_OK,               /* No error happen */
	NFP_NET_CFG_MBOX_RET_FS_ERR_NO_SPACE,     /* Return error code no space */
	NFP_NET_CFG_MBOX_RET_FS_ERR_MASK_FULL,    /* Return error code mask table full */
	NFP_NET_CFG_MBOX_RET_FS_ERR_CMD_INVALID,  /* Return error code invalid cmd */
};

/* 4B cmd, and up to 500B data. */
struct nfp_net_cmsg {
	uint32_t cmd;     /**< One of nfp_net_cfg_mbox_cmd */
	uint32_t data[];
};

struct nfp_net_cmsg *nfp_net_cmsg_alloc(uint32_t msg_size);
void nfp_net_cmsg_free(struct nfp_net_cmsg *cmsg);
int nfp_net_cmsg_xmit(struct nfp_net_hw *hw, struct nfp_net_cmsg *cmsg, uint32_t msg_size);

#endif /* __NFP_NET_CMSG_H__ */
