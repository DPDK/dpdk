/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox.
 */

#include <unistd.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

static void
priv_txq_stop(struct priv *priv)
{
	unsigned int i;

	for (i = 0; i != priv->txqs_n; ++i)
		mlx5_priv_txq_release(priv, i);
}

static int
priv_txq_start(struct priv *priv)
{
	unsigned int i;
	int ret = 0;

	/* Add memory regions to Tx queues. */
	for (i = 0; i != priv->txqs_n; ++i) {
		unsigned int idx = 0;
		struct mlx5_mr *mr;
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_priv_txq_get(priv, i);

		if (!txq_ctrl)
			continue;
		LIST_FOREACH(mr, &priv->mr, next) {
			priv_txq_mp2mr_reg(priv, &txq_ctrl->txq, mr->mp, idx++);
			if (idx == MLX5_PMD_TX_MP_CACHE)
				break;
		}
		txq_alloc_elts(txq_ctrl);
		txq_ctrl->ibv = mlx5_priv_txq_ibv_new(priv, i);
		if (!txq_ctrl->ibv) {
			ret = ENOMEM;
			goto error;
		}
	}
	ret = priv_tx_uar_remap(priv, priv->ctx->cmd_fd);
	if (ret)
		goto error;
	return ret;
error:
	priv_txq_stop(priv);
	return ret;
}

static void
priv_rxq_stop(struct priv *priv)
{
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i)
		mlx5_priv_rxq_release(priv, i);
}

static int
priv_rxq_start(struct priv *priv)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_priv_rxq_get(priv, i);

		if (!rxq_ctrl)
			continue;
		ret = rxq_alloc_elts(rxq_ctrl);
		if (ret)
			goto error;
		rxq_ctrl->ibv = mlx5_priv_rxq_ibv_new(priv, i);
		if (!rxq_ctrl->ibv) {
			ret = ENOMEM;
			goto error;
		}
	}
	return -ret;
error:
	priv_rxq_stop(priv);
	return -ret;
}

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by attaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_start(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_mr *mr = NULL;
	int err;

	dev->data->dev_started = 1;
	priv_lock(priv);
	err = priv_flow_create_drop_queue(priv);
	if (err) {
		ERROR("%p: Drop queue allocation failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	DEBUG("%p: allocating and configuring hash RX queues", (void *)dev);
	rte_mempool_walk(mlx5_mp2mr_iter, priv);
	err = priv_txq_start(priv);
	if (err) {
		ERROR("%p: TXQ allocation failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	err = priv_rxq_start(priv);
	if (err) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	err = priv_rx_intr_vec_enable(priv);
	if (err) {
		ERROR("%p: RX interrupt vector creation failed",
		      (void *)priv);
		goto error;
	}
	priv_xstats_init(priv);
	/* Update link status and Tx/Rx callbacks for the first time. */
	memset(&dev->data->dev_link, 0, sizeof(struct rte_eth_link));
	INFO("Forcing port %u link to be up", dev->data->port_id);
	err = priv_force_link_status_change(priv, ETH_LINK_UP);
	if (err) {
		DEBUG("Failed to set port %u link to be up",
		      dev->data->port_id);
		goto error;
	}
	priv_dev_interrupt_handler_install(priv, dev);
	priv_unlock(priv);
	return 0;
error:
	/* Rollback. */
	dev->data->dev_started = 0;
	for (mr = LIST_FIRST(&priv->mr); mr; mr = LIST_FIRST(&priv->mr))
		priv_mr_release(priv, mr);
	priv_flow_stop(priv, &priv->flows);
	priv_dev_traffic_disable(priv, dev);
	priv_txq_stop(priv);
	priv_rxq_stop(priv);
	priv_flow_delete_drop_queue(priv);
	priv_unlock(priv);
	return err;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_dev_stop(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_mr *mr;

	priv_lock(priv);
	dev->data->dev_started = 0;
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	usleep(1000 * priv->rxqs_n);
	DEBUG("%p: cleaning up and destroying hash RX queues", (void *)dev);
	priv_flow_stop(priv, &priv->flows);
	priv_dev_traffic_disable(priv, dev);
	priv_rx_intr_vec_disable(priv);
	priv_dev_interrupt_handler_uninstall(priv, dev);
	priv_txq_stop(priv);
	priv_rxq_stop(priv);
	for (mr = LIST_FIRST(&priv->mr); mr; mr = LIST_FIRST(&priv->mr))
		priv_mr_release(priv, mr);
	priv_flow_delete_drop_queue(priv);
	priv_unlock(priv);
}

/**
 * Enable traffic flows configured by control plane
 *
 * @param priv
 *   Pointer to Ethernet device private data.
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success.
 */
int
priv_dev_traffic_enable(struct priv *priv, struct rte_eth_dev *dev)
{
	struct rte_flow_item_eth bcast = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	struct rte_flow_item_eth ipv6_multi_spec = {
		.dst.addr_bytes = "\x33\x33\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth ipv6_multi_mask = {
		.dst.addr_bytes = "\xff\xff\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast = {
		.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	const unsigned int vlan_filter_n = priv->vlan_filter_n;
	const struct ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;
	unsigned int j;
	int ret;

	if (priv->isolated)
		return 0;
	if (dev->data->promiscuous) {
		struct rte_flow_item_eth promisc = {
			.dst.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		claim_zero(mlx5_ctrl_flow(dev, &promisc, &promisc));
		return 0;
	}
	if (dev->data->all_multicast) {
		struct rte_flow_item_eth multicast = {
			.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		claim_zero(mlx5_ctrl_flow(dev, &multicast, &multicast));
	} else {
		/* Add broadcast/multicast flows. */
		for (i = 0; i != vlan_filter_n; ++i) {
			uint16_t vlan = priv->vlan_filter[i];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask = {
				.tci = 0xffff,
			};

			ret = mlx5_ctrl_flow_vlan(dev, &bcast, &bcast,
						  &vlan_spec, &vlan_mask);
			if (ret)
				goto error;
			ret = mlx5_ctrl_flow_vlan(dev, &ipv6_multi_spec,
						  &ipv6_multi_mask,
						  &vlan_spec, &vlan_mask);
			if (ret)
				goto error;
		}
		if (!vlan_filter_n) {
			ret = mlx5_ctrl_flow(dev, &bcast, &bcast);
			if (ret)
				goto error;
			ret = mlx5_ctrl_flow(dev, &ipv6_multi_spec,
					     &ipv6_multi_mask);
			if (ret)
				goto error;
		}
	}
	/* Add MAC address flows. */
	for (i = 0; i != MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&unicast.dst.addr_bytes,
		       mac->addr_bytes,
		       ETHER_ADDR_LEN);
		for (j = 0; j != vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask = {
				.tci = 0xffff,
			};

			ret = mlx5_ctrl_flow_vlan(dev, &unicast,
						  &unicast_mask,
						  &vlan_spec,
						  &vlan_mask);
			if (ret)
				goto error;
		}
		if (!vlan_filter_n) {
			ret = mlx5_ctrl_flow(dev, &unicast,
					     &unicast_mask);
			if (ret)
				goto error;
		}
	}
	return 0;
error:
	return rte_errno;
}


/**
 * Disable traffic flows configured by control plane
 *
 * @param priv
 *   Pointer to Ethernet device private data.
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success.
 */
int
priv_dev_traffic_disable(struct priv *priv, struct rte_eth_dev *dev)
{
	(void)dev;
	priv_flow_flush(priv, &priv->ctrl_flows);
	return 0;
}

/**
 * Restart traffic flows configured by control plane
 *
 * @param priv
 *   Pointer to Ethernet device private data.
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success.
 */
int
priv_dev_traffic_restart(struct priv *priv, struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		priv_dev_traffic_disable(priv, dev);
		priv_dev_traffic_enable(priv, dev);
	}
	return 0;
}

/**
 * Restart traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success.
 */
int
mlx5_traffic_restart(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	priv_dev_traffic_restart(priv, dev);
	priv_unlock(priv);
	return 0;
}
