/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev_driver.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->promiscuous = 1;
	if (priv->isolated) {
		DRV_LOG(WARNING,
			"port %u cannot enable promiscuous mode"
			" in flow isolation mode",
			dev->data->port_id);
		return;
	}
	if (priv->config.vf)
		mlx5_nl_promisc(dev, 1);
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot enable promiscuous mode: %s",
			dev->data->port_id, strerror(rte_errno));
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->promiscuous = 0;
	if (priv->config.vf)
		mlx5_nl_promisc(dev, 0);
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot disable promiscuous mode: %s",
			dev->data->port_id, strerror(rte_errno));
}

/**
 * DPDK callback to enable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->all_multicast = 1;
	if (priv->isolated) {
		DRV_LOG(WARNING,
			"port %u cannot enable allmulticast mode"
			" in flow isolation mode",
			dev->data->port_id);
		return;
	}
	if (priv->config.vf)
		mlx5_nl_allmulti(dev, 1);
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot enable allmulicast mode: %s",
			dev->data->port_id, strerror(rte_errno));
}

/**
 * DPDK callback to disable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->all_multicast = 0;
	if (priv->config.vf)
		mlx5_nl_allmulti(dev, 0);
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot disable allmulicast mode: %s",
			dev->data->port_id, strerror(rte_errno));
}
