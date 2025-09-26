/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_RESOURCE_H_
#define _NBL_RESOURCE_H_

#include "nbl_ethdev.h"
#include "nbl_include.h"
#include <stdint.h>

#define NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)	((res_mgt)->hw_ops_tbl)
#define NBL_RES_MGT_TO_HW_OPS(res_mgt)		(NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)->ops)
#define NBL_RES_MGT_TO_HW_PRIV(res_mgt)		(NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)->priv)
#define NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)	((res_mgt)->chan_ops_tbl)
#define NBL_RES_MGT_TO_CHAN_OPS(res_mgt)	(NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)->ops)
#define NBL_RES_MGT_TO_CHAN_PRIV(res_mgt)	(NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)->priv)
#define NBL_RES_MGT_TO_ETH_DEV(res_mgt)		((res_mgt)->eth_dev)
#define NBL_RES_MGT_TO_TXRX_MGT(res_mgt)	((res_mgt)->txrx_mgt)

struct nbl_res_tx_ring {
};

struct nbl_res_rx_ring {
};

struct nbl_txrx_mgt {
	rte_spinlock_t tx_lock;
	struct nbl_res_tx_ring **tx_rings;
	struct nbl_res_rx_ring **rx_rings;
	u8 tx_ring_num;
	u8 rx_ring_num;
};

struct nbl_resource_mgt {
	const struct rte_eth_dev *eth_dev;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_hw_ops_tbl *hw_ops_tbl;
	struct nbl_txrx_mgt *txrx_mgt;
};

struct nbl_resource_mgt_leonis {
	struct nbl_resource_mgt res_mgt;
};

int nbl_txrx_mgt_start(struct nbl_resource_mgt *res_mgt);
void nbl_txrx_mgt_stop(struct nbl_resource_mgt *res_mgt);
int nbl_txrx_setup_ops(struct nbl_resource_ops *resource_ops);
void nbl_txrx_remove_ops(struct nbl_resource_ops *resource_ops);

#endif
