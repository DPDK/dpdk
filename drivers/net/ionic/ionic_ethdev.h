/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_ETHDEV_H_
#define _IONIC_ETHDEV_H_

#include <rte_ethdev.h>

#define IONIC_ETH_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP)

#define IONIC_ETH_DEV_TO_LIF(eth_dev) ((struct ionic_lif *) \
	(eth_dev)->data->dev_private)
#define IONIC_ETH_DEV_TO_ADAPTER(eth_dev) \
	(IONIC_ETH_DEV_TO_LIF(eth_dev)->adapter)

int ionic_dev_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete);

#endif /* _IONIC_ETHDEV_H_ */
