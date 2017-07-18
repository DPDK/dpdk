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

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include "failsafe_private.h"

/*
 * TODO: write fast version,
 * without additional checks, to be activated once
 * everything has been verified to comply.
 */
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
		if (unlikely(ETH(sdev) == NULL))
			continue;
		if (unlikely(ETH(sdev)->rx_pkt_burst == NULL))
			continue;
		if (unlikely(sdev->state != DEV_STARTED))
			continue;
		sub_rxq = ETH(sdev)->data->rx_queues[rxq->qid];
		nb_rx = ETH(sdev)->
			rx_pkt_burst(sub_rxq, rx_pkts, nb_pkts);
		if (nb_rx) {
			rxq->last_polled = i;
			return nb_rx;
		}
	}
	return 0;
}

/*
 * TODO: write fast version,
 * without additional checks, to be activated once
 * everything has been verified to comply.
 */
uint16_t
failsafe_tx_burst(void *queue,
		  struct rte_mbuf **tx_pkts,
		  uint16_t nb_pkts)
{
	struct sub_device *sdev;
	struct txq *txq;
	void *sub_txq;

	txq = queue;
	sdev = TX_SUBDEV(txq->priv->dev);
	if (unlikely(sdev == NULL))
		return 0;
	if (unlikely(ETH(sdev) == NULL))
		return 0;
	if (unlikely(ETH(sdev)->tx_pkt_burst == NULL))
		return 0;
	sub_txq = ETH(sdev)->data->tx_queues[txq->qid];
	return ETH(sdev)->tx_pkt_burst(sub_txq, tx_pkts, nb_pkts);
}
