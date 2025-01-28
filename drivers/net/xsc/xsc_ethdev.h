/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_ETHDEV_H_
#define _XSC_ETHDEV_H_

#include "xsc_dev.h"

struct xsc_ethdev_priv {
	struct rte_eth_dev *eth_dev;
	struct rte_pci_device *pci_dev;
	struct xsc_dev *xdev;
};

#define TO_XSC_ETHDEV_PRIV(dev) ((struct xsc_ethdev_priv *)(dev)->data->dev_private)

#endif /* _XSC_ETHDEV_H_ */
