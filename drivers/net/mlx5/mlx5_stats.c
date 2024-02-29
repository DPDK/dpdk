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
			stats[i].id = i;
			if (xstats_ctrl->info[i].dev) {
				uint64_t wrap_n;
				uint64_t hw_stat = xstats_ctrl->hw_stats[i];

				stats[i].value = (counters[i] -
						  xstats_ctrl->base[i]) &
						  (uint64_t)UINT32_MAX;
				wrap_n = hw_stat >> 32;
				if (stats[i].value <
					    (hw_stat & (uint64_t)UINT32_MAX))
					wrap_n++;
				stats[i].value |= (wrap_n) << 32;
				xstats_ctrl->hw_stats[i] = stats[i].value;
			} else {
				stats[i].value =
					(counters[i] - xstats_ctrl->base[i]);
			}
		}
	}
	mlx5_stats_n = mlx5_txpp_xstats_get(dev, stats, n, mlx5_stats_n);
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
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
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
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_ipackets[idx] += rxq->stats.ipackets -
				rxq->stats_reset.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes -
				rxq->stats_reset.ibytes;
#endif
			tmp.q_errors[idx] += (rxq->stats.idropped +
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
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_opackets[idx] += txq->stats.opackets -
						txq->stats_reset.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes -
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
	mlx5_txpp_xstats_reset(dev);
	mlx5_free(counters);
	return 0;
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

	if (n >= mlx5_xstats_n && xstats_names) {
		for (i = 0; i != mlx5_xstats_n; ++i) {
			strlcpy(xstats_names[i].name,
				xstats_ctrl->info[i].dpdk_name,
				RTE_ETH_XSTATS_NAME_SIZE);
		}
	}
	mlx5_xstats_n = mlx5_txpp_xstats_get_names(dev, xstats_names,
						   n, mlx5_xstats_n);
	return mlx5_xstats_n;
}
