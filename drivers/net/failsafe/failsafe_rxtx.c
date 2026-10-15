/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <rte_stdatomic.h>
#include <rte_debug.h>
#include <rte_mbuf.h>
#include <ethdev_driver.h>

#include "failsafe_private.h"

static inline int
fs_rx_unsafe(struct sub_device *sdev)
{
	return (ETH(sdev) == NULL) ||
		(ETH(sdev)->rx_pkt_burst == NULL) ||
		(sdev->state != DEV_STARTED) ||
		(sdev->remove != 0);
}

static inline int
fs_tx_unsafe(struct sub_device *sdev)
{
	return (sdev == NULL) ||
		(ETH(sdev) == NULL) ||
		(ETH(sdev)->tx_pkt_burst == NULL) ||
		(sdev->state != DEV_STARTED);
}

/*
 * Override source port in Rx packets.
 *
 * Make Rx packets originate from this PMD instance instead of one of its
 * sub-devices. This is mandatory to avoid breaking applications.
 */
static void
failsafe_rx_set_port(struct rte_mbuf **rx_pkts, uint16_t nb_pkts, uint16_t port)
{
	unsigned int i;

	for (i = 0; i != nb_pkts; ++i)
		rx_pkts[i]->port = port;
}

uint16_t
failsafe_rx_burst(void *queue,
		  struct rte_mbuf **rx_pkts,
		  uint16_t nb_pkts)
{
	struct sub_device *sdev;
	struct rxq *rxq;
	void *sub_rxq;
	uint16_t nb_rx;

	rxq = queue;
	sdev = rxq->sdev;
	do {
		if (unlikely(fs_rx_unsafe(sdev))) {
			nb_rx = 0;
			sdev = sdev->next;
			continue;
		}
		sub_rxq = ETH(sdev)->data->rx_queues[rxq->qid];
		FS_ATOMIC_P(rxq->refcnt[sdev->sid]);
		nb_rx = ETH(sdev)->
			rx_pkt_burst(sub_rxq, rx_pkts, nb_pkts);
		FS_ATOMIC_V(rxq->refcnt[sdev->sid]);
		sdev = sdev->next;
	} while (nb_rx == 0 && sdev != rxq->sdev);
	rxq->sdev = sdev;
	if (nb_rx)
		failsafe_rx_set_port(rx_pkts, nb_rx,
				     rxq->priv->data->port_id);
	return nb_rx;
}

uint16_t
failsafe_tx_burst(void *queue,
		  struct rte_mbuf **tx_pkts,
		  uint16_t nb_pkts)
{
	struct sub_device *sdev;
	struct txq *txq;
	void *sub_txq;
	uint16_t nb_tx;

	txq = queue;
	sdev = TX_SUBDEV(&rte_eth_devices[txq->priv->data->port_id]);
	if (unlikely(fs_tx_unsafe(sdev)))
		return 0;
	sub_txq = ETH(sdev)->data->tx_queues[txq->qid];
	FS_ATOMIC_P(txq->refcnt[sdev->sid]);
	nb_tx = ETH(sdev)->tx_pkt_burst(sub_txq, tx_pkts, nb_pkts);
	FS_ATOMIC_V(txq->refcnt[sdev->sid]);
	return nb_tx;
}
