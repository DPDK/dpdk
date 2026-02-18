/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include "nfb_stats.h"
#include "nfb.h"

int
nfb_eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
		  struct eth_queue_stats *qstats)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	int ret;
	struct pmd_internals *internals = dev->process_private;
	struct nc_rxmac_counters rx_cntrs;
	struct nc_txmac_counters tx_cntrs;

	for (i = 0; i < nb_rx; i++) {
		struct ndp_rx_queue *rx_queue = dev->data->rx_queues[i];
		if (rx_queue == NULL)
			continue;

		if (qstats && i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			qstats->q_ipackets[i] = rx_queue->rx_pkts;
			qstats->q_ibytes[i] = rx_queue->rx_bytes;
		}
	}

	for (i = 0; i < nb_tx; i++) {
		struct ndp_tx_queue *tx_queue = dev->data->tx_queues[i];
		if (tx_queue == NULL)
			continue;

		if (qstats && i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			qstats->q_opackets[i] = tx_queue->tx_pkts;
			qstats->q_obytes[i] = tx_queue->tx_bytes;
		}
	}

	for (i = 0; i < internals->max_rxmac; i++) {
		ret = nc_rxmac_read_counters(internals->rxmac[i], &rx_cntrs, NULL);
		if (ret)
			return -EIO;

		stats->ipackets += rx_cntrs.cnt_received;
		stats->ibytes += rx_cntrs.cnt_octets;
		stats->ierrors += rx_cntrs.cnt_erroneous;
		stats->imissed += rx_cntrs.cnt_overflowed;
	}

	for (i = 0; i < internals->max_txmac; i++) {
		ret = nc_txmac_read_counters(internals->txmac[i], &tx_cntrs);
		if (ret)
			return -EIO;

		stats->opackets += tx_cntrs.cnt_sent;
		stats->obytes += tx_cntrs.cnt_octets;
		stats->oerrors += tx_cntrs.cnt_drop;
	}
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
