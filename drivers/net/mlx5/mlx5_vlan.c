/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include <ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_hypervisor.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_rxtx.h"
#include "mlx5_rx.h"
#include "mlx5_utils.h"
#include "mlx5_devx.h"

/**
 * DPDK callback to configure a VLAN filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   VLAN ID to filter.
 * @param on
 *   Toggle filter.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	DRV_LOG(DEBUG, "port %u %s VLAN filter ID %" PRIu16,
		dev->data->port_id, (on ? "enable" : "disable"), vlan_id);
	MLX5_ASSERT(priv->vlan_filter_n <= RTE_DIM(priv->vlan_filter));
	for (i = 0; (i != priv->vlan_filter_n); ++i)
		if (priv->vlan_filter[i] == vlan_id)
			break;
	/* Check if there's room for another VLAN filter. */
	if (i == RTE_DIM(priv->vlan_filter)) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (i < priv->vlan_filter_n) {
		MLX5_ASSERT(priv->vlan_filter_n != 0);
		/* Enabling an existing VLAN filter has no effect. */
		if (on)
			goto out;
		/* Remove VLAN filter from list. */
		--priv->vlan_filter_n;
		memmove(&priv->vlan_filter[i],
			&priv->vlan_filter[i + 1],
			sizeof(priv->vlan_filter[i]) *
			(priv->vlan_filter_n - i));
		priv->vlan_filter[priv->vlan_filter_n] = 0;
	} else {
		MLX5_ASSERT(i == priv->vlan_filter_n);
		/* Disabling an unknown VLAN filter has no effect. */
		if (!on)
			goto out;
		/* Add new VLAN filter. */
		priv->vlan_filter[priv->vlan_filter_n] = vlan_id;
		++priv->vlan_filter_n;
	}
out:
	if (dev->data->dev_started)
		return mlx5_traffic_restart(dev);
	return 0;
}

/**
 * Callback to set/reset VLAN stripping for a specific queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param queue
 *   RX queue index.
 * @param on
 *   Enable/disable VLAN stripping.
 */
void
mlx5_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, queue);
	struct mlx5_rxq_data *rxq_data = &rxq->ctrl->rxq;
	int ret = 0;

	MLX5_ASSERT(rxq != NULL && rxq->ctrl != NULL);
	/* Validate hw support */
	if (!priv->config.hw_vlan_strip) {
		DRV_LOG(ERR, "port %u VLAN stripping is not supported",
			dev->data->port_id);
		return;
	}
	/* Validate queue number */
	if (queue >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u VLAN stripping, invalid queue number %d",
			dev->data->port_id, queue);
		return;
	}
	DRV_LOG(DEBUG, "port %u set VLAN stripping offloads %d for port %uqueue %d",
		dev->data->port_id, on, rxq_data->port_id, queue);
	if (rxq->ctrl->obj == NULL) {
		/* Update related bits in RX queue. */
		rxq_data->vlan_strip = !!on;
		return;
	}
	ret = priv->obj_ops.rxq_obj_modify_vlan_strip(rxq, on);
	if (ret) {
		DRV_LOG(ERR, "Port %u failed to modify object stripping mode:"
			" %s", dev->data->port_id, strerror(rte_errno));
		return;
	}
	/* Update related bits in RX queue. */
	rxq_data->vlan_strip = !!on;
}

/**
 * Callback to set/reset VLAN offloads for a port.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mask
 *   VLAN offload bit mask.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		int hw_vlan_strip = !!(dev->data->dev_conf.rxmode.offloads &
				       RTE_ETH_RX_OFFLOAD_VLAN_STRIP);

		if (!priv->config.hw_vlan_strip) {
			DRV_LOG(ERR, "port %u VLAN stripping is not supported",
				dev->data->port_id);
			return 0;
		}
		/* Run on every RX queue and set/reset VLAN stripping. */
		for (i = 0; (i != priv->rxqs_n); i++)
			mlx5_vlan_strip_queue_set(dev, i, hw_vlan_strip);
	}
	return 0;
}
