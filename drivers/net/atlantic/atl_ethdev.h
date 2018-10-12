/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#ifndef _ATLANTIC_ETHDEV_H_
#define _ATLANTIC_ETHDEV_H_
#include <rte_errno.h>
#include "rte_ethdev.h"

#include "atl_types.h"
#include "hw_atl/hw_atl_utils.h"

#define ATL_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct atl_adapter *)adapter)->hw)

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct atl_adapter {
	struct aq_hw_s             hw;
	struct aq_hw_cfg_s         hw_cfg;
};

/*
 * RX/TX function prototypes
 */
int atl_rx_init(struct rte_eth_dev *dev);
int atl_tx_init(struct rte_eth_dev *dev);

uint16_t atl_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t atl_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t atl_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

#endif /* _ATLANTIC_ETHDEV_H_ */
