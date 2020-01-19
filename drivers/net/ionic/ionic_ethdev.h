/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_ETHDEV_H_
#define _IONIC_ETHDEV_H_

#define IONIC_ETH_DEV_TO_LIF(eth_dev) ((struct ionic_lif *) \
	(eth_dev)->data->dev_private)
#define IONIC_ETH_DEV_TO_ADAPTER(eth_dev) \
	(IONIC_ETH_DEV_TO_LIF(eth_dev)->adapter)

#endif /* _IONIC_ETHDEV_H_ */

