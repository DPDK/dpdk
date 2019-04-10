/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <inttypes.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

static const struct mlx5_counter_ctrl mlx5_counters_init[] = {
	{
		.dpdk_name = "rx_port_unicast_bytes",
		.ctr_name = "rx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "rx_port_multicast_bytes",
		.ctr_name = "rx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "rx_port_broadcast_bytes",
		.ctr_name = "rx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "rx_port_unicast_packets",
		.ctr_name = "rx_vport_unicast_packets",
	},
	{
		.dpdk_name = "rx_port_multicast_packets",
		.ctr_name = "rx_vport_multicast_packets",
	},
	{
		.dpdk_name = "rx_port_broadcast_packets",
		.ctr_name = "rx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "tx_port_unicast_bytes",
		.ctr_name = "tx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "tx_port_multicast_bytes",
		.ctr_name = "tx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "tx_port_broadcast_bytes",
		.ctr_name = "tx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "tx_port_unicast_packets",
		.ctr_name = "tx_vport_unicast_packets",
	},
	{
		.dpdk_name = "tx_port_multicast_packets",
		.ctr_name = "tx_vport_multicast_packets",
	},
	{
		.dpdk_name = "tx_port_broadcast_packets",
		.ctr_name = "tx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "rx_wqe_err",
		.ctr_name = "rx_wqe_err",
	},
	{
		.dpdk_name = "rx_crc_errors_phy",
		.ctr_name = "rx_crc_errors_phy",
	},
	{
		.dpdk_name = "rx_in_range_len_errors_phy",
		.ctr_name = "rx_in_range_len_errors_phy",
	},
	{
		.dpdk_name = "rx_symbol_err_phy",
		.ctr_name = "rx_symbol_err_phy",
	},
	{
		.dpdk_name = "tx_errors_phy",
		.ctr_name = "tx_errors_phy",
	},
	{
		.dpdk_name = "rx_out_of_buffer",
		.ctr_name = "out_of_buffer",
		.ib = 1,
	},
	{
		.dpdk_name = "tx_packets_phy",
		.ctr_name = "tx_packets_phy",
	},
	{
		.dpdk_name = "rx_packets_phy",
		.ctr_name = "rx_packets_phy",
	},
	{
		.dpdk_name = "tx_discards_phy",
		.ctr_name = "tx_discards_phy",
	},
	{
		.dpdk_name = "rx_discards_phy",
		.ctr_name = "rx_discards_phy",
	},
	{
		.dpdk_name = "tx_bytes_phy",
		.ctr_name = "tx_bytes_phy",
	},
	{
		.dpdk_name = "rx_bytes_phy",
		.ctr_name = "rx_bytes_phy",
	},
	/* Representor only */
	{
		.dpdk_name = "rx_packets",
		.ctr_name = "vport_rx_packets",
	},
	{
		.dpdk_name = "rx_bytes",
		.ctr_name = "vport_rx_bytes",
	},
	{
		.dpdk_name = "tx_packets",
		.ctr_name = "vport_tx_packets",
	},
	{
		.dpdk_name = "tx_bytes",
		.ctr_name = "vport_tx_bytes",
	},
};

static const unsigned int xstats_n = RTE_DIM(mlx5_counters_init);

static inline void
mlx5_read_ib_stat(struct mlx5_priv *priv, const char *ctr_name, uint64_t *stat)
{
	FILE *file;
	if (priv->sh) {
		MKSTR(path, "%s/ports/%d/hw_counters/%s",
			  priv->sh->ibdev_path,
			  priv->ibv_port,
			  ctr_name);

		file = fopen(path, "rb");
		if (file) {
			int n = fscanf(file, "%" SCNu64, stat);

			fclose(file);
			if (n == 1)
				return;
		}
	}
	*stat = 0;
}

/**
 * Read device counters table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Counters table output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_read_dev_counters(struct rte_eth_dev *dev, uint64_t *stats)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	struct ifreq ifr;
	unsigned int stats_sz = xstats_ctrl->stats_n * sizeof(uint64_t);
	unsigned char et_stat_buf[sizeof(struct ethtool_stats) + stats_sz];
	struct ethtool_stats *et_stats = (struct ethtool_stats *)et_stat_buf;
	int ret;

	et_stats->cmd = ETHTOOL_GSTATS;
	et_stats->n_stats = xstats_ctrl->stats_n;
	ifr.ifr_data = (caddr_t)et_stats;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u unable to read statistic values from device",
			dev->data->port_id);
		return ret;
	}
	for (i = 0; i != xstats_ctrl->mlx5_stats_n; ++i) {
		if (xstats_ctrl->info[i].ib) {
			mlx5_read_ib_stat(priv, xstats_ctrl->info[i].ctr_name,
					  &stats[i]);
		} else {
			stats[i] = (uint64_t)
				et_stats->data[xstats_ctrl->dev_table_idx[i]];
		}
	}
	return 0;
}

/**
 * Query the number of statistics provided by ETHTOOL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Number of statistics on success, negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_ethtool_get_stats_n(struct rte_eth_dev *dev) {
	struct ethtool_drvinfo drvinfo;
	struct ifreq ifr;
	int ret;

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to query number of statistics",
			dev->data->port_id);
		return ret;
	}
	return drvinfo.n_stats;
}

/**
 * Init the structures to read device counters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_stats_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct mlx5_stats_ctrl *stats_ctrl = &priv->stats_ctrl;
	unsigned int i;
	unsigned int j;
	struct ifreq ifr;
	struct ethtool_gstrings *strings = NULL;
	unsigned int dev_stats_n;
	unsigned int str_sz;
	int ret;

	/* So that it won't aggregate for each init. */
	xstats_ctrl->mlx5_stats_n = 0;
	ret = mlx5_ethtool_get_stats_n(dev);
	if (ret < 0) {
		DRV_LOG(WARNING, "port %u no extended statistics available",
			dev->data->port_id);
		return;
	}
	dev_stats_n = ret;
	/* Allocate memory to grab stat names and values. */
	str_sz = dev_stats_n * ETH_GSTRING_LEN;
	strings = (struct ethtool_gstrings *)
		  rte_malloc("xstats_strings",
			     str_sz + sizeof(struct ethtool_gstrings), 0);
	if (!strings) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for xstats",
		     dev->data->port_id);
		return;
	}
	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_STATS;
	strings->len = dev_stats_n;
	ifr.ifr_data = (caddr_t)strings;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get statistic names",
			dev->data->port_id);
		goto free;
	}
	for (i = 0; i != dev_stats_n; ++i) {
		const char *curr_string = (const char *)
			&strings->data[i * ETH_GSTRING_LEN];

		for (j = 0; j != xstats_n; ++j) {
			if (!strcmp(mlx5_counters_init[j].ctr_name,
				    curr_string)) {
				unsigned int idx = xstats_ctrl->mlx5_stats_n++;

				xstats_ctrl->dev_table_idx[idx] = i;
				xstats_ctrl->info[idx] = mlx5_counters_init[j];
				break;
			}
		}
	}
	/* Add IB counters. */
	for (i = 0; i != xstats_n; ++i) {
		if (mlx5_counters_init[i].ib) {
			unsigned int idx = xstats_ctrl->mlx5_stats_n++;

			xstats_ctrl->info[idx] = mlx5_counters_init[i];
		}
	}
	assert(xstats_ctrl->mlx5_stats_n <= MLX5_MAX_XSTATS);
	xstats_ctrl->stats_n = dev_stats_n;
	/* Copy to base at first time. */
	ret = mlx5_read_dev_counters(dev, xstats_ctrl->base);
	if (ret)
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
	mlx5_read_ib_stat(priv, "out_of_buffer", &stats_ctrl->imissed_base);
free:
	rte_free(strings);
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
	unsigned int i;
	uint64_t counters[n];
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	uint16_t mlx5_stats_n = xstats_ctrl->mlx5_stats_n;

	if (n >= mlx5_stats_n && stats) {
		int stats_n;
		int ret;

		stats_n = mlx5_ethtool_get_stats_n(dev);
		if (stats_n < 0)
			return stats_n;
		if (xstats_ctrl->stats_n != stats_n)
			mlx5_stats_init(dev);
		ret = mlx5_read_dev_counters(dev, counters);
		if (ret)
			return ret;
		for (i = 0; i != mlx5_stats_n; ++i) {
			stats[i].id = i;
			stats[i].value = (counters[i] - xstats_ctrl->base[i]);
		}
	}
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
	struct rte_eth_stats tmp;
	unsigned int i;
	unsigned int idx;

	memset(&tmp, 0, sizeof(tmp));
	/* Add software counters. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		idx = rxq->idx;
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
		struct mlx5_txq_data *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
#endif
			tmp.q_errors[idx] += txq->stats.oerrors;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.opackets += txq->stats.opackets;
		tmp.obytes += txq->stats.obytes;
#endif
		tmp.oerrors += txq->stats.oerrors;
	}
	mlx5_read_ib_stat(priv, "out_of_buffer", &tmp.imissed);
	tmp.imissed -= priv->stats_ctrl.imissed_base;
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
 */
void
mlx5_stats_reset(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_stats_ctrl *stats_ctrl = &priv->stats_ctrl;
	unsigned int i;

	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		memset(&(*priv->rxqs)[i]->stats, 0,
		       sizeof(struct mlx5_rxq_stats));
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		if ((*priv->txqs)[i] == NULL)
			continue;
		memset(&(*priv->txqs)[i]->stats, 0,
		       sizeof(struct mlx5_txq_stats));
	}
	mlx5_read_ib_stat(priv, "out_of_buffer", &stats_ctrl->imissed_base);
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: reset hardware counters. */
#endif
}

/**
 * DPDK callback to clear device extended statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_xstats_reset(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	int stats_n;
	unsigned int i;
	unsigned int n = xstats_ctrl->mlx5_stats_n;
	uint64_t counters[n];
	int ret;

	stats_n = mlx5_ethtool_get_stats_n(dev);
	if (stats_n < 0) {
		DRV_LOG(ERR, "port %u cannot get stats: %s", dev->data->port_id,
			strerror(-stats_n));
		return;
	}
	if (xstats_ctrl->stats_n != stats_n)
		mlx5_stats_init(dev);
	ret = mlx5_read_dev_counters(dev, counters);
	if (ret) {
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
		return;
	}
	for (i = 0; i != n; ++i)
		xstats_ctrl->base[i] = counters[i];
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
mlx5_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
		      struct rte_eth_xstat_name *xstats_names, unsigned int n)
{
	unsigned int i;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int mlx5_xstats_n = xstats_ctrl->mlx5_stats_n;

	if (n >= mlx5_xstats_n && xstats_names) {
		for (i = 0; i != mlx5_xstats_n; ++i) {
			strncpy(xstats_names[i].name,
				xstats_ctrl->info[i].dpdk_name,
				RTE_ETH_XSTATS_NAME_SIZE);
			xstats_names[i].name[RTE_ETH_XSTATS_NAME_SIZE - 1] = 0;
		}
	}
	return mlx5_xstats_n;
}
