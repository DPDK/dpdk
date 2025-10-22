/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_PRODUCT_BASE_H_
#define _NBL_DEF_PRODUCT_BASE_H_

#include "nbl_include.h"

struct nbl_product_core_ops {
	int (*hw_init)(void *p);
	void (*hw_remove)(void *p);
	int (*res_init)(void *p, const struct rte_eth_dev *eth_dev);
	void (*res_remove)(void *p);
	int (*chan_init)(void *p);
	void (*chan_remove)(void *p);
};

struct nbl_product_dev_ops {
	int (*dev_init)(void *adapter);
	void (*dev_uninit)(void *adapter);
	int (*dev_start)(void *adapter);
	void (*dev_stop)(void *adapter);
};

struct nbl_product_dispatch_ops {
	int (*dispatch_init)(void *mgt);
	int (*dispatch_uninit)(void *mgt);
};

#endif
