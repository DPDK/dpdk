/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"

#define CNXK_NB_RXQ_STATS 5
#define CNXK_NB_TXQ_STATS 4

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

int
cnxk_nix_xstats_get(struct rte_eth_dev *eth_dev, struct rte_eth_xstat *xstats,
		    unsigned int n)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_xstat roc_xstats[n];
	struct roc_nix *nix = &dev->nix;
	int roc_size, q, idx = 0, size;

	roc_size = roc_nix_xstats_get(nix, roc_xstats, n);

	if (roc_size < 0)
		return roc_size;

	/* Per Queue statistics also returned as part of xstats */
	size = roc_size + (dev->nb_rxq * CNXK_NB_RXQ_STATS) +
	       (dev->nb_txq * CNXK_NB_TXQ_STATS);

	/* If requested array do not have space then return with count */
	if (size > (int)n || xstats == NULL)
		return size;

	for (idx = 0; idx < roc_size; idx++) {
		xstats[idx].id = roc_xstats[idx].id;
		xstats[idx].value = roc_xstats[idx].value;
	}
	for (q = 0; q < dev->nb_rxq; q++) {
		struct roc_nix_stats_queue qstats;
		int rc;

		rc = roc_nix_stats_queue_get(nix, q, 1, &qstats);
		if (rc)
			return rc;

		xstats[idx].id = idx;
		xstats[idx].value = qstats.rx_pkts;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.rx_octs;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.rx_drop_pkts;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.rx_drop_octs;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.rx_error_pkts;
		idx++;
	}
	for (q = 0; q < dev->nb_txq; q++) {
		struct roc_nix_stats_queue qstats;
		int rc;

		rc = roc_nix_stats_queue_get(nix, q, 0, &qstats);
		if (rc)
			return rc;

		xstats[idx].id = idx;
		xstats[idx].value = qstats.tx_pkts;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.tx_octs;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.tx_drop_pkts;
		idx++;
		xstats[idx].id = idx;
		xstats[idx].value = qstats.tx_drop_octs;
		idx++;
	}

	return size;
}

int
cnxk_nix_xstats_get_names(struct rte_eth_dev *eth_dev,
			  struct rte_eth_xstat_name *xstats_names,
			  unsigned int limit)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_xstat_name roc_xstats_name[limit];
	struct roc_nix *nix = &dev->nix;
	int roc_size, size, i, q;

	roc_size = roc_nix_xstats_names_get(nix, NULL, 0);
	/* Per Queue statistics also returned as part of xstats */
	size = roc_size + (dev->nb_rxq * CNXK_NB_RXQ_STATS) +
	       (dev->nb_txq * CNXK_NB_TXQ_STATS);

	if (xstats_names == NULL)
		return size;

	if ((int)limit < size && xstats_names != NULL)
		return size;

	roc_size = roc_nix_xstats_names_get(nix, roc_xstats_name, limit);

	for (i = 0; i < roc_size; i++)
		rte_strscpy(xstats_names[i].name, roc_xstats_name[i].name,
			    sizeof(xstats_names[i].name));

	for (q = 0; q < dev->nb_rxq; q++) {
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "rxq_%d_pkts", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "rxq_%d_octs", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "rxq_%d_drop_pkts", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "rxq_%d_drop_octs", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "rxq_%d_err_pkts", q);
		i++;
	}

	for (q = 0; q < dev->nb_txq; q++) {
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "txq_%d_pkts", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "txq_%d_octs", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "txq_%d_drop_pkts", q);
		i++;
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "txq_%d_drop_octs", q);
		i++;
	}

	return size;
}

int
cnxk_nix_xstats_get_names_by_id(struct rte_eth_dev *eth_dev,
				const uint64_t *ids,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int limit)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint32_t nix_cnt = roc_nix_xstats_names_get(&dev->nix, NULL, 0);
	uint32_t stat_cnt = nix_cnt + (dev->nb_rxq * CNXK_NB_RXQ_STATS) +
			    (dev->nb_txq * CNXK_NB_TXQ_STATS);
	struct rte_eth_xstat_name xnames[stat_cnt];
	uint32_t i;

	if (limit < stat_cnt && ids == NULL)
		return stat_cnt;

	if (limit > stat_cnt)
		return -EINVAL;

	if (xstats_names == NULL)
		return -ENOMEM;

	cnxk_nix_xstats_get_names(eth_dev, xnames, stat_cnt);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= stat_cnt)
			return -EINVAL;

		rte_strscpy(xstats_names[i].name, xnames[ids[i]].name,
			    sizeof(xstats_names[i].name));
	}

	return limit;
}

int
cnxk_nix_xstats_get_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids,
			  uint64_t *values, unsigned int n)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint32_t nix_cnt = roc_nix_xstats_names_get(&dev->nix, NULL, 0);
	uint32_t stat_cnt = nix_cnt + (dev->nb_rxq * CNXK_NB_RXQ_STATS) +
			    (dev->nb_txq * CNXK_NB_TXQ_STATS);
	struct rte_eth_xstat xstats[stat_cnt];
	uint32_t i;

	if (n < stat_cnt && ids == NULL)
		return stat_cnt;

	if (n > stat_cnt)
		return -EINVAL;

	if (values == NULL)
		return -ENOMEM;

	cnxk_nix_xstats_get(eth_dev, xstats, stat_cnt);

	for (i = 0; i < n; i++) {
		if (ids[i] >= stat_cnt)
			return -EINVAL;
		values[i] = xstats[ids[i]].value;
	}

	return n;
}

int
cnxk_nix_xstats_reset(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc = 0, i;

	rc = roc_nix_stats_reset(nix);
	if (rc)
		goto exit;

	/* Reset Rx Queues */
	for (i = 0; i < dev->nb_rxq; i++) {
		rc = roc_nix_stats_queue_reset(nix, i, 1);
		if (rc)
			goto exit;
	}

	/* Reset Tx Queues */
	for (i = 0; i < dev->nb_txq; i++) {
		rc = roc_nix_stats_queue_reset(nix, i, 0);
		if (rc)
			goto exit;
	}

exit:
	return rc;
}
