/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>

#include <ethdev_driver.h>

#include <mlx5_glue.h>
#include "mlx5.h"
#include "mlx5_utils.h"

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
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
		return 0;
	}
	if (priv->config.vf || priv->config.sf) {
		ret = mlx5_os_set_promisc(dev, 1);
		if (ret)
			return ret;
	}
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot enable promiscuous mode: %s",
			dev->data->port_id, strerror(rte_errno));

	/*
	 * rte_eth_dev_promiscuous_enable() rollback
	 * dev->data->promiscuous in the case of failure.
	 */
	return ret;
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->promiscuous = 0;
	if (priv->config.vf || priv->config.sf) {
		ret = mlx5_os_set_promisc(dev, 0);
		if (ret)
			return ret;
	}
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot disable promiscuous mode: %s",
			dev->data->port_id, strerror(rte_errno));

	/*
	 * rte_eth_dev_promiscuous_disable() rollback
	 * dev->data->promiscuous in the case of failure.
	 */
	return ret;
}

/**
 * DPDK callback to enable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
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
		return 0;
	}
	if (priv->config.vf || priv->config.sf) {
		ret = mlx5_os_set_allmulti(dev, 1);
		if (ret)
			goto error;
	}
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot enable allmulicast mode: %s",
			dev->data->port_id, strerror(rte_errno));
error:
	/*
	 * rte_eth_allmulticast_enable() rollback
	 * dev->data->all_multicast in the case of failure.
	 */
	return ret;
}

/**
 * DPDK callback to disable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	dev->data->all_multicast = 0;
	if (priv->config.vf || priv->config.sf) {
		ret = mlx5_os_set_allmulti(dev, 0);
		if (ret)
			goto error;
	}
	ret = mlx5_traffic_restart(dev);
	if (ret)
		DRV_LOG(ERR, "port %u cannot disable allmulicast mode: %s",
			dev->data->port_id, strerror(rte_errno));
error:
	/*
	 * rte_eth_allmulticast_disable() rollback
	 * dev->data->all_multicast in the case of failure.
	 */
	return ret;
}
