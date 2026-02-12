/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include <mlx5_common.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_malloc.h"


static const struct mlx5_xstats_name_off mlx5_rxq_stats_strings[] = {
	{"out_of_buffer", offsetof(struct mlx5_rq_stats, q_oobs)},
};

#define NB_RXQ_STATS RTE_DIM(mlx5_rxq_stats_strings)

/**
 * Retrieve extended device statistics
 * for Rx queues. It appends the specific statistics
 * before the parts filled by preceding modules (eth stats, etc.)
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Pointer to an array to store the retrieved statistics.
 * @return
 *   Number of extended stats is filled,
 *   negative on error and rte_errno is set.
 */
static int
mlx5_rq_xstats_get(struct rte_eth_dev *dev,
					struct rte_eth_xstat *stats)
{
	uint16_t n_stats_rq = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	int cnt_used_entries = 0;

	for (unsigned int idx = 0; idx < n_stats_rq; idx++) {
		struct mlx5_rxq_data *rxq_data = mlx5_rxq_data_get(dev, idx);
		struct mlx5_rxq_priv *rxq_priv = mlx5_rxq_get(dev, idx);

		if (rxq_data == NULL)
			continue;

		struct mlx5_rxq_stat *rxq_stat = &rxq_data->stats.oobs;
		if (rxq_stat == NULL)
			continue;

		/* Handle initial stats setup - Flag uninitialized stat */
		rxq_stat->id = -1;

		/* Handle hairpin statistics */
		if (rxq_priv && rxq_priv->ctrl->is_hairpin) {
			if (stats) {
				mlx5_read_queue_counter(rxq_priv->q_counter, "hairpin_out_of_buffer",
					&rxq_stat->count);

				stats[cnt_used_entries].id = cnt_used_entries;
				stats[cnt_used_entries].value = rxq_stat->count -
					rxq_data->stats_reset.oobs.count;
			}
			rxq_stat->ctrl.enable = mlx5_enable_per_queue_hairpin_counter;
			rxq_stat->ctrl.disable = mlx5_disable_per_queue_hairpin_counter;
			rxq_stat->id = cnt_used_entries;
			cnt_used_entries++;
		}
	}
	return cnt_used_entries;
}

/**
 * Retrieve names of extended device statistics
 * for Rx queues. It appends the specific stats names
 * before the parts filled by preceding modules (eth stats, etc.)
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 *
 * @return
 *   Number of xstats names.
 */
static int
mlx5_rq_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
			       struct rte_eth_xstat_name *xstats_names)
{
	struct mlx5_rxq_priv *rxq;
	unsigned int i;
	int cnt_used_entries = 0;

	uint16_t n_stats_rq = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	for (i = 0; (i != n_stats_rq); ++i) {
		rxq = mlx5_rxq_get(dev, i);

		if (rxq == NULL)
			continue;

		if (rxq->ctrl->is_hairpin) {
			if (xstats_names)
				snprintf(xstats_names[cnt_used_entries].name,
					sizeof(xstats_names[0].name),
					"hairpin_%s_rxq%u",
					mlx5_rxq_stats_strings[0].name, i);
			cnt_used_entries++;
		}
	}
	return cnt_used_entries;
}

/**
 * DPDK callback to get extended device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Pointer to rte extended stats table.
 * @param n
 *   The size of the stats table.
 *
 * @return
 *   Number of extended stats on success and stats is filled,
 *   negative on error and rte_errno is set.
 */
int
mlx5_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
		unsigned int n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint64_t counters[MLX5_MAX_XSTATS];
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	uint16_t stats_n = 0;
	uint16_t stats_n_2nd = 0;
	uint16_t mlx5_stats_n = xstats_ctrl->mlx5_stats_n;
	bool bond_master = (priv->master && priv->pf_bond >= 0);
	int n_used = mlx5_rq_xstats_get(dev, stats);

	if (n >= mlx5_stats_n && stats) {
		int ret;

		ret = mlx5_os_get_stats_n(dev, bond_master, &stats_n, &stats_n_2nd);
		if (ret < 0)
			return ret;
		/*
		 * The number of statistics fetched via "ETH_SS_STATS" may vary because
		 * of the port configuration each time. This is also true between 2
		 * ports. There might be a case that the numbers are the same even if
		 * configurations are different.
		 * It is not recommended to change the configuration without using
		 * RTE API. The port(traffic) restart may trigger another initialization
		 * to make sure the map are correct.
		 */
		if (xstats_ctrl->stats_n != stats_n ||
		    (bond_master && xstats_ctrl->stats_n_2nd != stats_n_2nd))
			mlx5_os_stats_init(dev);
		ret = mlx5_os_read_dev_counters(dev, bond_master, counters);
		if (ret < 0)
			return ret;
		for (i = 0; i != mlx5_stats_n; i++) {
			stats[i + n_used].id = i + n_used;
			if (xstats_ctrl->info[i].dev) {
				uint64_t wrap_n;
				uint64_t hw_stat = xstats_ctrl->hw_stats[i];

				stats[i + n_used].value = (counters[i] -
						  xstats_ctrl->base[i]) &
						  (uint64_t)UINT32_MAX;
				wrap_n = hw_stat >> 32;
				if (stats[i + n_used].value <
					    (hw_stat & (uint64_t)UINT32_MAX))
					wrap_n++;
				stats[i + n_used].value |= (wrap_n) << 32;
				xstats_ctrl->hw_stats[i] = stats[i + n_used].value;
			} else {
				stats[i + n_used].value =
					(counters[i] - xstats_ctrl->base[i]);
			}
		}
	}
	mlx5_stats_n = mlx5_txpp_xstats_get(dev, stats, n, mlx5_stats_n + n_used);
	return mlx5_stats_n;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
	       struct eth_queue_stats *qstats)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_stats_ctrl *stats_ctrl = &priv->stats_ctrl;
	struct rte_eth_stats tmp;
	unsigned int i;
	unsigned int idx;
	uint64_t wrap_n;
	int ret;

	memset(&tmp, 0, sizeof(tmp));
	/* Add software counters. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct mlx5_rxq_data *rxq = mlx5_rxq_data_get(dev, i);

		if (rxq == NULL)
			continue;
		idx = rxq->idx;
		if (qstats != NULL && idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			qstats->q_ipackets[idx] += rxq->stats.ipackets -
				rxq->stats_reset.ipackets;
			qstats->q_ibytes[idx] += rxq->stats.ibytes -
				rxq->stats_reset.ibytes;
#endif
			qstats->q_errors[idx] += (rxq->stats.idropped +
					      rxq->stats.rx_nombuf) -
					      (rxq->stats_reset.idropped +
					      rxq->stats_reset.rx_nombuf);
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.ipackets += rxq->stats.ipackets - rxq->stats_reset.ipackets;
		tmp.ibytes += rxq->stats.ibytes - rxq->stats_reset.ibytes;
#endif
		tmp.ierrors += rxq->stats.idropped - rxq->stats_reset.idropped;
		tmp.rx_nombuf += rxq->stats.rx_nombuf -
					rxq->stats_reset.rx_nombuf;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct mlx5_txq_data *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->idx;
		if (qstats != NULL && idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			qstats->q_opackets[idx] += txq->stats.opackets -
						txq->stats_reset.opackets;
			qstats->q_obytes[idx] += txq->stats.obytes -
						txq->stats_reset.obytes;
#endif
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.opackets += txq->stats.opackets - txq->stats_reset.opackets;
		tmp.obytes += txq->stats.obytes - txq->stats_reset.obytes;
#endif
		tmp.oerrors += txq->stats.oerrors - txq->stats_reset.oerrors;
	}
	ret = mlx5_os_read_dev_stat(priv, "out_of_buffer", &tmp.imissed);
	if (ret == 0) {
		tmp.imissed = (tmp.imissed - stats_ctrl->imissed_base) &
				 (uint64_t)UINT32_MAX;
		wrap_n = stats_ctrl->imissed >> 32;
		if (tmp.imissed < (stats_ctrl->imissed & (uint64_t)UINT32_MAX))
			wrap_n++;
		tmp.imissed |= (wrap_n) << 32;
		stats_ctrl->imissed = tmp.imissed;
	} else {
		tmp.imissed = stats_ctrl->imissed;
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: retrieve and add hardware counters. */
#endif
	*stats = tmp;
	return 0;
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   always 0 on success and stats is reset
 */
int
mlx5_stats_reset(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_stats_ctrl *stats_ctrl = &priv->stats_ctrl;
	unsigned int i;

	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct mlx5_rxq_data *rxq_data = mlx5_rxq_data_get(dev, i);

		if (rxq_data == NULL)
			continue;
		rxq_data->stats_reset = rxq_data->stats;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct mlx5_txq_data *txq_data = (*priv->txqs)[i];

		if (txq_data == NULL)
			continue;
		txq_data->stats_reset = txq_data->stats;
	}
	mlx5_os_read_dev_stat(priv, "out_of_buffer", &stats_ctrl->imissed_base);
	stats_ctrl->imissed = 0;
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: reset hardware counters. */
#endif

	return 0;
}

/**
 * DPDK callback to clear device extended statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is reset, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_xstats_reset(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	uint64_t *counters;
	int ret;
	uint16_t stats_n = 0;
	uint16_t stats_n_2nd = 0;
	bool bond_master = (priv->master && priv->pf_bond >= 0);

	ret = mlx5_os_get_stats_n(dev, bond_master, &stats_n, &stats_n_2nd);
	if (ret < 0) {
		DRV_LOG(ERR, "port %u cannot get stats: %s", dev->data->port_id,
			strerror(-ret));
		return ret;
	}
	if (xstats_ctrl->stats_n != stats_n ||
	    (bond_master && xstats_ctrl->stats_n_2nd != stats_n_2nd))
		mlx5_os_stats_init(dev);
	/* Considering to use stack directly. */
	counters = mlx5_malloc(MLX5_MEM_SYS, sizeof(*counters) * xstats_ctrl->mlx5_stats_n,
			       0, SOCKET_ID_ANY);
	if (!counters) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for xstats counters",
		     dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ret = mlx5_os_read_dev_counters(dev, bond_master, counters);
	if (ret) {
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
		mlx5_free(counters);
		return ret;
	}
	for (i = 0; i != xstats_ctrl->mlx5_stats_n; ++i) {
		xstats_ctrl->base[i] = counters[i];
		xstats_ctrl->hw_stats[i] = 0;
	}
	mlx5_reset_xstats_rq(dev);
	mlx5_txpp_xstats_reset(dev);
	mlx5_free(counters);
	return 0;
}

void
mlx5_reset_xstats_by_name(struct mlx5_priv *priv, const char *ctr_name)
{
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int mlx5_xstats_n = xstats_ctrl->mlx5_stats_n;
	unsigned int i;

	for (i = 0; i != mlx5_xstats_n; ++i) {
		if (strcmp(xstats_ctrl->info[i].ctr_name, ctr_name) == 0) {
			xstats_ctrl->base[i] = 0;
			xstats_ctrl->hw_stats[i] = 0;
			xstats_ctrl->xstats[i] = 0;
			return;
		}
	}
}

/**
 * Clear device extended statistics for each Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_reset_xstats_rq(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_priv *rxq;
	struct mlx5_rxq_data *rxq_data;
	unsigned int i;

	for (i = 0; (i != priv->rxqs_n); ++i) {
		rxq = mlx5_rxq_get(dev, i);
		rxq_data = mlx5_rxq_data_get(dev, i);

		if (rxq == NULL || rxq_data == NULL || rxq->q_counter == NULL)
			continue;
		if (rxq->ctrl->is_hairpin)
			mlx5_read_queue_counter(rxq->q_counter,
				"hairpin_out_of_buffer", &rxq_data->stats_reset.oobs.count);
		else
			mlx5_read_queue_counter(rxq->q_counter,
				"out_of_buffer", &rxq_data->stats_reset.oobs.count);
	}
}

/**
 * DPDK callback to retrieve names of extended device statistics
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 * @param n
 *   Number of names.
 *
 * @return
 *   Number of xstats names.
 */
int
mlx5_xstats_get_names(struct rte_eth_dev *dev,
		      struct rte_eth_xstat_name *xstats_names, unsigned int n)
{
	unsigned int i;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int mlx5_xstats_n = xstats_ctrl->mlx5_stats_n;
	unsigned int n_used = mlx5_rq_xstats_get_names(dev, xstats_names);

	if (n >= mlx5_xstats_n && xstats_names) {
		for (i = 0; i != mlx5_xstats_n; ++i) {
			rte_strscpy(xstats_names[i + n_used].name,
				xstats_ctrl->info[i].dpdk_name,
				RTE_ETH_XSTATS_NAME_SIZE);
			xstats_names[i + n_used].name[RTE_ETH_XSTATS_NAME_SIZE - 1] = 0;
		}
	}
	mlx5_xstats_n = mlx5_txpp_xstats_get_names(dev, xstats_names,
						   n, mlx5_xstats_n + n_used);
	return mlx5_xstats_n;
}

static struct mlx5_stat_counter_ctrl*
mlx5_rxq_get_counter_by_id(struct rte_eth_dev *dev, uint64_t id, uint64_t *rq_id)
{
	uint16_t n_stats_rq = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	for (int i = 0; (i != n_stats_rq); i++) {
		struct mlx5_rxq_data *rxq_data = mlx5_rxq_data_get(dev, i);
		if (rxq_data == NULL || rxq_data->stats.oobs.id == -1)
			continue;

		if ((uint64_t)rxq_data->stats.oobs.id == id) {
			*rq_id = rxq_data->idx;
			return &rxq_data->stats.oobs.ctrl;
		}
	}

	return NULL;
}

/**
 * Callback to enable an xstat counter of the given id.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param id
 *   The ID of the counter to enable
 *
 * @return
 *   1 xstat is enabled, 0 if xstat is disabled,
 *   -ENOTSUP if enabling/disabling is not implemented and -EINVAL if xstat id is invalid.
 */
int
mlx5_xstats_enable(struct rte_eth_dev *dev, uint64_t id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct mlx5_stat_counter_ctrl *counter_ctrl = NULL;
	uint16_t n_stats_rq = mlx5_rq_xstats_get(dev, NULL);

	if (id < n_stats_rq)
		counter_ctrl = mlx5_rxq_get_counter_by_id(dev, id, &id);
	else
		counter_ctrl = &xstats_ctrl->info[id - n_stats_rq].ctrl;

	if (counter_ctrl == NULL)
		return -EINVAL;

	if (counter_ctrl->enable == NULL)
		return -ENOTSUP;

	counter_ctrl->enabled = counter_ctrl->enable(dev, id) == 0 ? 1 : 0;
	return counter_ctrl->enabled;
}

/**
 * Callback to disable an xstat counter of the given id.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param id
 *   The ID of the counter to enable
 *
 * @return
 *   1 if xstat is disabled, 0 xstat is enabled,
 *   -ENOTSUP if enabling/disabling is not implemented and -EINVAL if xstat id is invalid.
 */
int
mlx5_xstats_disable(struct rte_eth_dev *dev, uint64_t id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct mlx5_stat_counter_ctrl *counter_ctrl = NULL;

	uint16_t n_stats_rq = mlx5_rq_xstats_get(dev, NULL);
	if (id < n_stats_rq)
		counter_ctrl = mlx5_rxq_get_counter_by_id(dev, id, &id);
	else
		counter_ctrl = &xstats_ctrl->info[id - n_stats_rq].ctrl;

	if (counter_ctrl == NULL)
		return -EINVAL;

	if (counter_ctrl->disable == NULL)
		return -ENOTSUP;

	counter_ctrl->enabled = counter_ctrl->disable(dev, id) == 0 ? 0 : 1;
	return counter_ctrl->enabled;
}

/**
 * Query the state of the xstat counter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param id
 *   The ID of the counter to enable
 *
 * @return
 *   1 if xstat is disabled, 0 xstat is enabled,
 *   -ENOTSUP if enabling/disabling is not implemented and -EINVAL if xstat id is invalid.
 */
int
mlx5_xstats_query_state(struct rte_eth_dev *dev, uint64_t id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct mlx5_stat_counter_ctrl *counter_ctrl = NULL;

	uint16_t n_stats_rq = mlx5_rq_xstats_get(dev, NULL);
	if (id < n_stats_rq)
		counter_ctrl = mlx5_rxq_get_counter_by_id(dev, id, &id);
	else
		counter_ctrl = &xstats_ctrl->info[id - n_stats_rq].ctrl;

	if (counter_ctrl == NULL)
		return -EINVAL;

	if (counter_ctrl->disable == NULL)
		return -ENOTSUP;

	return counter_ctrl->enabled;
}
