/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_CMSG_H__
#define __NFP_NET_CMSG_H__

#include "nfp_net_common.h"

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
	uint16_t mark_id;
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
