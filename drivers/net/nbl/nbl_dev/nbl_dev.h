/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEV_H_
#define _NBL_DEV_H_

#include "nbl_common.h"

#define NBL_DEV_MGT_TO_DISP_OPS_TBL(dev_mgt)	((dev_mgt)->disp_ops_tbl)
#define NBL_DEV_MGT_TO_DISP_OPS(dev_mgt)	(NBL_DEV_MGT_TO_DISP_OPS_TBL(dev_mgt)->ops)
#define NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt)	(NBL_DEV_MGT_TO_DISP_OPS_TBL(dev_mgt)->priv)
#define NBL_DEV_MGT_TO_CHAN_OPS_TBL(dev_mgt)	((dev_mgt)->chan_ops_tbl)
#define NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt)	(NBL_DEV_MGT_TO_CHAN_OPS_TBL(dev_mgt)->ops)
#define NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt)	(NBL_DEV_MGT_TO_CHAN_OPS_TBL(dev_mgt)->priv)
#define NBL_DEV_MGT_TO_NET_DEV(dev_mgt)		((dev_mgt)->net_dev)
#define NBL_DEV_MGT_TO_ETH_DEV(dev_mgt)		((dev_mgt)->net_dev->eth_dev)
#define NBL_DEV_MGT_TO_COMMON(dev_mgt)		((dev_mgt)->common)

struct nbl_dev_ring {
	u16 index;
	u64 dma;
	u16 local_queue_id;
	u16 global_queue_id;
	u32 desc_num;
};

struct nbl_dev_ring_mgt {
	struct nbl_dev_ring *tx_rings;
	struct nbl_dev_ring *rx_rings;
	u16 queue_offset;
	u8 tx_ring_num;
	u8 rx_ring_num;
	u8 active_ring_num;
};

struct nbl_dev_net_mgt {
	const struct rte_eth_dev *eth_dev;
	struct nbl_dev_ring_mgt ring_mgt;
	u16 vsi_id;
	u8 eth_mode;
	u8 eth_id;
	u16 max_mac_num;
	bool trust;
};

struct nbl_dev_mgt {
	struct nbl_dispatch_ops_tbl *disp_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_dev_net_mgt *net_dev;
	struct nbl_common_info *common;
};

const struct nbl_product_dev_ops *nbl_dev_get_product_ops(enum nbl_product_type product_type);
int nbl_dev_configure(struct rte_eth_dev *eth_dev);
int nbl_dev_port_start(struct rte_eth_dev *eth_dev);
int nbl_dev_port_stop(struct rte_eth_dev *eth_dev);
int nbl_dev_port_close(struct rte_eth_dev *eth_dev);

#endif
