/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_atomic.h>
#include <rte_debug.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include "failsafe_private.h"

static inline int
fs_rx_unsafe(struct sub_device *sdev)
{
	return (ETH(sdev) == NULL) ||
		(ETH(sdev)->rx_pkt_burst == NULL) ||
		(sdev->state != DEV_STARTED);
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
	struct fs_priv *priv;
	struct sub_device *sdev;
	struct rxq *rxq;
	void *sub_rxq;
	uint16_t nb_rx;
	uint8_t nb_polled, nb_subs;
	uint8_t i;

	rxq = queue;
	priv = rxq->priv;
	nb_subs = priv->subs_tail - priv->subs_head;
	nb_polled = 0;
	for (i = rxq->last_polled; nb_polled < nb_subs; nb_polled++) {
		i++;
		if (i == priv->subs_tail)
			i = priv->subs_head;
		sdev = &priv->subs[i];
		if (unlikely(fs_rx_unsafe(sdev)))
			continue;
		sub_rxq = ETH(sdev)->data->rx_queues[rxq->qid];
		FS_ATOMIC_P(rxq->refcnt[sdev->sid]);
		nb_rx = ETH(sdev)->
			rx_pkt_burst(sub_rxq, rx_pkts, nb_pkts);
		FS_ATOMIC_V(rxq->refcnt[sdev->sid]);
		if (nb_rx) {
			rxq->last_polled = i;
			return nb_rx;
		}
	}
	return 0;
}

uint16_t
failsafe_rx_burst_fast(void *queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct fs_priv *priv;
	struct sub_device *sdev;
	struct rxq *rxq;
	void *sub_rxq;
	uint16_t nb_rx;
	uint8_t nb_polled, nb_subs;
	uint8_t i;

	rxq = queue;
	priv = rxq->priv;
	nb_subs = priv->subs_tail - priv->subs_head;
	nb_polled = 0;
	for (i = rxq->last_polled; nb_polled < nb_subs; nb_polled++) {
		i++;
		if (i == priv->subs_tail)
			i = priv->subs_head;
		sdev = &priv->subs[i];
		RTE_ASSERT(!fs_rx_unsafe(sdev));
		sub_rxq = ETH(sdev)->data->rx_queues[rxq->qid];
		FS_ATOMIC_P(rxq->refcnt[sdev->sid]);
		nb_rx = ETH(sdev)->
			rx_pkt_burst(sub_rxq, rx_pkts, nb_pkts);
		FS_ATOMIC_V(rxq->refcnt[sdev->sid]);
		if (nb_rx) {
			rxq->last_polled = i;
			return nb_rx;
		}
	}
	return 0;
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
