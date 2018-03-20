/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <rte_atomic.h>
#include <rte_debug.h>
#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>

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

void
set_burst_fn(struct rte_eth_dev *dev, int force_safe)
{
	struct sub_device *sdev;
	uint8_t i;
	int need_safe;
	int safe_set;

	need_safe = force_safe;
	FOREACH_SUBDEV(sdev, i, dev)
		need_safe |= fs_rx_unsafe(sdev);
	safe_set = (dev->rx_pkt_burst == &failsafe_rx_burst);
	if (need_safe && !safe_set) {
		DEBUG("Using safe RX bursts%s",
		      (force_safe ? " (forced)" : ""));
		dev->rx_pkt_burst = &failsafe_rx_burst;
	} else if (!need_safe && safe_set) {
		DEBUG("Using fast RX bursts");
		dev->rx_pkt_burst = &failsafe_rx_burst_fast;
	}
	need_safe = force_safe || fs_tx_unsafe(TX_SUBDEV(dev));
	safe_set = (dev->tx_pkt_burst == &failsafe_tx_burst);
	if (need_safe && !safe_set) {
		DEBUG("Using safe TX bursts%s",
		      (force_safe ? " (forced)" : ""));
		dev->tx_pkt_burst = &failsafe_tx_burst;
	} else if (!need_safe && safe_set) {
		DEBUG("Using fast TX bursts");
		dev->tx_pkt_burst = &failsafe_tx_burst_fast;
	}
	rte_wmb();
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
		if (fs_rx_unsafe(sdev)) {
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
	return nb_rx;
}

uint16_t
failsafe_rx_burst_fast(void *queue,
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
		RTE_ASSERT(!fs_rx_unsafe(sdev));
		sub_rxq = ETH(sdev)->data->rx_queues[rxq->qid];
		FS_ATOMIC_P(rxq->refcnt[sdev->sid]);
		nb_rx = ETH(sdev)->
			rx_pkt_burst(sub_rxq, rx_pkts, nb_pkts);
		FS_ATOMIC_V(rxq->refcnt[sdev->sid]);
		sdev = sdev->next;
	} while (nb_rx == 0 && sdev != rxq->sdev);
	rxq->sdev = sdev;
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
	sdev = TX_SUBDEV(txq->priv->dev);
	if (unlikely(fs_tx_unsafe(sdev)))
		return 0;
	sub_txq = ETH(sdev)->data->tx_queues[txq->qid];
	FS_ATOMIC_P(txq->refcnt[sdev->sid]);
	nb_tx = ETH(sdev)->tx_pkt_burst(sub_txq, tx_pkts, nb_pkts);
	FS_ATOMIC_V(txq->refcnt[sdev->sid]);
	return nb_tx;
}

uint16_t
failsafe_tx_burst_fast(void *queue,
			 struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	struct sub_device *sdev;
	struct txq *txq;
	void *sub_txq;
	uint16_t nb_tx;

	txq = queue;
	sdev = TX_SUBDEV(txq->priv->dev);
	RTE_ASSERT(!fs_tx_unsafe(sdev));
	sub_txq = ETH(sdev)->data->tx_queues[txq->qid];
	FS_ATOMIC_P(txq->refcnt[sdev->sid]);
	nb_tx = ETH(sdev)->tx_pkt_burst(sub_txq, tx_pkts, nb_pkts);
	FS_ATOMIC_V(txq->refcnt[sdev->sid]);
	return nb_tx;
}
