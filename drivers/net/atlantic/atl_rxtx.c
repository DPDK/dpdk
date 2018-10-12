/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include "atl_ethdev.h"

int
atl_tx_init(struct rte_eth_dev *eth_dev __rte_unused)
{
	return 0;
}

int
atl_rx_init(struct rte_eth_dev *eth_dev __rte_unused)
{
	return 0;
}

uint16_t
atl_prep_pkts(void *tx_queue __rte_unused,
	      struct rte_mbuf **tx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

uint16_t
atl_recv_pkts(void *rx_queue __rte_unused,
	      struct rte_mbuf **rx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

uint16_t
atl_xmit_pkts(void *tx_queue __rte_unused,
	      struct rte_mbuf **tx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

