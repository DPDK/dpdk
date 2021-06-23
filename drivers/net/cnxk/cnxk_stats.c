/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"

int
cnxk_nix_stats_get(struct rte_eth_dev *eth_dev, struct rte_eth_stats *stats)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_stats nix_stats;
	int rc = 0, i;

	rc = roc_nix_stats_get(nix, &nix_stats);
	if (rc)
		goto exit;

	stats->opackets = nix_stats.tx_ucast;
	stats->opackets += nix_stats.tx_mcast;
	stats->opackets += nix_stats.tx_bcast;
	stats->oerrors = nix_stats.tx_drop;
	stats->obytes = nix_stats.tx_octs;

	stats->ipackets = nix_stats.rx_ucast;
	stats->ipackets += nix_stats.rx_mcast;
	stats->ipackets += nix_stats.rx_bcast;
	stats->imissed = nix_stats.rx_drop;
	stats->ibytes = nix_stats.rx_octs;
	stats->ierrors = nix_stats.rx_err;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
		struct roc_nix_stats_queue qstats;
		uint16_t qidx;

		if (dev->txq_stat_map[i] & (1U << 31)) {
			qidx = dev->txq_stat_map[i] & 0xFFFF;
			rc = roc_nix_stats_queue_get(nix, qidx, 0, &qstats);
			if (rc)
				goto exit;
			stats->q_opackets[i] = qstats.tx_pkts;
			stats->q_obytes[i] = qstats.tx_octs;
			stats->q_errors[i] = qstats.tx_drop_pkts;
		}

		if (dev->rxq_stat_map[i] & (1U << 31)) {
			qidx = dev->rxq_stat_map[i] & 0xFFFF;
			rc = roc_nix_stats_queue_get(nix, qidx, 1, &qstats);
			if (rc)
				goto exit;
			stats->q_ipackets[i] = qstats.rx_pkts;
			stats->q_ibytes[i] = qstats.rx_octs;
			stats->q_errors[i] += qstats.rx_drop_pkts;
		}
	}
exit:
	return rc;
}

int
cnxk_nix_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	return roc_nix_stats_reset(&dev->nix);
}

int
cnxk_nix_queue_stats_mapping(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			     uint8_t stat_idx, uint8_t is_rx)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	if (is_rx) {
		if (queue_id >= dev->nb_rxq)
			return -EINVAL;
		dev->rxq_stat_map[stat_idx] = ((1U << 31) | queue_id);
	} else {
		if (queue_id >= dev->nb_txq)
			return -EINVAL;
		dev->txq_stat_map[stat_idx] = ((1U << 31) | queue_id);
	}

	return 0;
}
