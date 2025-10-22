/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_DEV_H_
#define _NBL_DEF_DEV_H_

#include "nbl_include.h"

#define NBL_DEV_OPS_TBL_TO_OPS(dev_ops_tbl)	((dev_ops_tbl)->ops)
#define NBL_DEV_OPS_TBL_TO_PRIV(dev_ops_tbl)	((dev_ops_tbl)->priv)

struct nbl_dev_ops {
};

struct nbl_dev_ops_tbl {
	struct nbl_dev_ops *ops;
	void *priv;
};

int nbl_dev_init(void *p, const struct rte_eth_dev *eth_dev);
void nbl_dev_remove(void *p);
int nbl_dev_start(void *p);
void nbl_dev_stop(void *p);

#endif
