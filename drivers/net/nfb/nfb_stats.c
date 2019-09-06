/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include "nfb_stats.h"
#include "nfb.h"

int
nfb_eth_stats_get(struct rte_eth_dev *dev,
	struct rte_eth_stats *stats)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;
	uint64_t rx_total = 0;
	uint64_t tx_total = 0;
	uint64_t tx_err_total = 0;
	uint64_t rx_total_bytes = 0;
	uint64_t tx_total_bytes = 0;

	struct ndp_rx_queue *rx_queue = *((struct ndp_rx_queue **)
		dev->data->rx_queues);
	struct ndp_tx_queue *tx_queue = *((struct ndp_tx_queue **)
		dev->data->tx_queues);

	for (i = 0; i < nb_rx; i++) {
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rx_queue[i].rx_pkts;
			stats->q_ibytes[i] = rx_queue[i].rx_bytes;
		}
		rx_total += rx_queue[i].rx_pkts;
		rx_total_bytes += rx_queue[i].rx_bytes;
	}

	for (i = 0; i < nb_tx; i++) {
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = tx_queue[i].tx_pkts;
			stats->q_obytes[i] = tx_queue[i].tx_bytes;
		}
		tx_total += tx_queue[i].tx_pkts;
		tx_total_bytes += tx_queue[i].tx_bytes;
		tx_err_total += tx_queue[i].err_pkts;
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
	stats->oerrors = tx_err_total;
	return 0;
}

int
nfb_eth_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	struct ndp_rx_queue *rx_queue = *((struct ndp_rx_queue **)
		dev->data->rx_queues);
	struct ndp_tx_queue *tx_queue = *((struct ndp_tx_queue **)
		dev->data->tx_queues);

	for (i = 0; i < nb_rx; i++) {
		rx_queue[i].rx_pkts = 0;
		rx_queue[i].rx_bytes = 0;
		rx_queue[i].err_pkts = 0;
	}
	for (i = 0; i < nb_tx; i++) {
		tx_queue[i].tx_pkts = 0;
		tx_queue[i].tx_bytes = 0;
		tx_queue[i].err_pkts = 0;
	}

	return 0;
}
