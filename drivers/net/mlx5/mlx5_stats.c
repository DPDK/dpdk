/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 */
void
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct rte_eth_stats tmp = {0};
	unsigned int i;
	unsigned int idx;

	priv_lock(priv);
	/* Add software counters. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		idx = rxq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_ipackets[idx] += rxq->stats.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes;
#endif
			tmp.q_errors[idx] += (rxq->stats.idropped +
					      rxq->stats.rx_nombuf);
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.ipackets += rxq->stats.ipackets;
		tmp.ibytes += rxq->stats.ibytes;
#endif
		tmp.ierrors += rxq->stats.idropped;
		tmp.rx_nombuf += rxq->stats.rx_nombuf;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct txq *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
#endif
			tmp.q_errors[idx] += txq->stats.odropped;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.opackets += txq->stats.opackets;
		tmp.obytes += txq->stats.obytes;
#endif
		tmp.oerrors += txq->stats.odropped;
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: retrieve and add hardware counters. */
#endif
	*stats = tmp;
	priv_unlock(priv);
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_stats_reset(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int idx;

	priv_lock(priv);
	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		idx = (*priv->rxqs)[i]->stats.idx;
		(*priv->rxqs)[i]->stats =
			(struct mlx5_rxq_stats){ .idx = idx };
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		if ((*priv->txqs)[i] == NULL)
			continue;
		idx = (*priv->txqs)[i]->stats.idx;
		(*priv->txqs)[i]->stats =
			(struct mlx5_txq_stats){ .idx = idx };
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: reset hardware counters. */
#endif
	priv_unlock(priv);
}
