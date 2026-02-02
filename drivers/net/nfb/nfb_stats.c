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

	for (i = 0; i < nb_rx; i++) {
		struct ndp_rx_queue *rx_queue = dev->data->rx_queues[i];
		if (rx_queue == NULL)
			continue;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rx_queue->rx_pkts;
			stats->q_ibytes[i] = rx_queue->rx_bytes;
		}
		rx_total += rx_queue->rx_pkts;
		rx_total_bytes += rx_queue->rx_bytes;
	}

	for (i = 0; i < nb_tx; i++) {
		struct ndp_tx_queue *tx_queue = dev->data->tx_queues[i];
		if (tx_queue == NULL)
			continue;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = tx_queue->tx_pkts;
			stats->q_obytes[i] = tx_queue->tx_bytes;
		}
		tx_total += tx_queue->tx_pkts;
		tx_total_bytes += tx_queue->tx_bytes;
		tx_err_total += tx_queue->err_pkts;
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

	for (i = 0; i < nb_rx; i++) {
		struct ndp_rx_queue *rx_queue = dev->data->rx_queues[i];
		if (rx_queue == NULL)
			continue;

		rx_queue->rx_pkts = 0;
		rx_queue->rx_bytes = 0;
		rx_queue->err_pkts = 0;
	}

	for (i = 0; i < nb_tx; i++) {
		struct ndp_tx_queue *tx_queue = dev->data->tx_queues[i];
		if (tx_queue == NULL)
			continue;

		tx_queue->tx_pkts = 0;
		tx_queue->tx_bytes = 0;
		tx_queue->err_pkts = 0;
	}

	return 0;
}
