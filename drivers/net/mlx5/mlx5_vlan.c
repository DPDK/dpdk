/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

/*
 * Not needed by this file; included to work around the lack of off_t
 * definition for mlx5dv.h with unpatched rdma-core versions.
 */
#include <sys/types.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_glue.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

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
	assert(priv->vlan_filter_n <= RTE_DIM(priv->vlan_filter));
	for (i = 0; (i != priv->vlan_filter_n); ++i)
		if (priv->vlan_filter[i] == vlan_id)
			break;
	/* Check if there's room for another VLAN filter. */
	if (i == RTE_DIM(priv->vlan_filter)) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (i < priv->vlan_filter_n) {
		assert(priv->vlan_filter_n != 0);
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
		assert(i == priv->vlan_filter_n);
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
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[queue];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	uint16_t vlan_offloads =
		(on ? IBV_WQ_FLAGS_CVLAN_STRIPPING : 0) |
		0;
	int ret;

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
	DRV_LOG(DEBUG, "port %u set VLAN offloads 0x%x for port %uqueue %d",
		dev->data->port_id, vlan_offloads, rxq->port_id, queue);
	if (!rxq_ctrl->ibv) {
		/* Update related bits in RX queue. */
		rxq->vlan_strip = !!on;
		return;
	}
	mod = (struct ibv_wq_attr){
		.attr_mask = IBV_WQ_ATTR_FLAGS,
		.flags_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING,
		.flags = vlan_offloads,
	};
	ret = mlx5_glue->modify_wq(rxq_ctrl->ibv->wq, &mod);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to modified stripping mode: %s",
			dev->data->port_id, strerror(rte_errno));
		return;
	}
	/* Update related bits in RX queue. */
	rxq->vlan_strip = !!on;
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

	if (mask & ETH_VLAN_STRIP_MASK) {
		int hw_vlan_strip = !!(dev->data->dev_conf.rxmode.offloads &
				       DEV_RX_OFFLOAD_VLAN_STRIP);

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
