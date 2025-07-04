/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_net_cmsg.h"

#include <rte_malloc.h>

#include "nfp_logs.h"

struct nfp_net_cmsg *
nfp_net_cmsg_alloc(uint32_t msg_size)
{
	struct nfp_net_cmsg *cmsg;

	cmsg = rte_zmalloc(NULL, msg_size, 0);
	if (cmsg == NULL) {
		PMD_DRV_LOG(ERR, "Failed malloc memory.");
		return NULL;
	}

	return cmsg;
}

void
nfp_net_cmsg_free(struct nfp_net_cmsg *cmsg)
{
	rte_free(cmsg);
}

int
nfp_net_cmsg_xmit(struct nfp_net_hw *hw,
		struct nfp_net_cmsg *cmsg,
		uint32_t msg_size)
{
	int ret;
	uint32_t i;
	uint32_t data_num = msg_size / sizeof(uint32_t);

	for (i = 0; i < data_num; i++)
		nn_cfg_writel(&hw->super, NFP_NET_CFG_MBOX_VAL + 4 * i, *((uint32_t *)cmsg + i));

	ret = nfp_net_mbox_reconfig(hw, NFP_NET_CFG_MBOX_CMD_FLOW_STEER);
	switch (ret) {
	case NFP_NET_CFG_MBOX_RET_FS_OK:
		break;
	case NFP_NET_CFG_MBOX_RET_FS_ERR_NO_SPACE:
		PMD_DRV_LOG(ERR, "Not enough space for cmd %u.", cmsg->cmd);
		ret = -ENOSPC;
		break;
	case NFP_NET_CFG_MBOX_RET_FS_ERR_MASK_FULL:
		PMD_DRV_LOG(ERR, "The mask table is full for cmd %u.", cmsg->cmd);
		ret = -EXFULL;
		break;
	case NFP_NET_CFG_MBOX_RET_FS_ERR_CMD_INVALID:
		PMD_DRV_LOG(ERR, "The mbox cmd %u invalid.", cmsg->cmd);
		ret = -EINVAL;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unrecognized mbox cmd %u.", cmsg->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
