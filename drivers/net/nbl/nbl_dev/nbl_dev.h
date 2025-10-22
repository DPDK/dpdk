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

struct nbl_dev_mgt {
	struct nbl_dispatch_ops_tbl *disp_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
};

const struct nbl_product_dev_ops *nbl_dev_get_product_ops(enum nbl_product_type product_type);
int nbl_dev_configure(struct rte_eth_dev *eth_dev);
int nbl_dev_port_start(struct rte_eth_dev *eth_dev);
int nbl_dev_port_stop(struct rte_eth_dev *eth_dev);
int nbl_dev_port_close(struct rte_eth_dev *eth_dev);

#endif
