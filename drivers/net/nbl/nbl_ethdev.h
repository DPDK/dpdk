/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_ETHDEV_H_
#define _NBL_ETHDEV_H_

#include "nbl_core.h"

#define ETH_DEV_TO_NBL_DEV_PF_PRIV(eth_dev) \
	((struct nbl_adapter *)((eth_dev)->data->dev_private))

#endif
