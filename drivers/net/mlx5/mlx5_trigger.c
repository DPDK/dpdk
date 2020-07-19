/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <unistd.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>

#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "rte_pmd_mlx5.h"

/**
 * Stop traffic on Tx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_txq_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->txqs_n; ++i)
		mlx5_txq_release(dev, i);
}

/**
 * Start traffic on Tx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_txq_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	for (i = 0; i != priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_txq_get(dev, i);

		if (!txq_ctrl)
			continue;
		if (txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN) {
			txq_ctrl->obj = mlx5_txq_obj_new
				(dev, i, MLX5_TXQ_OBJ_TYPE_DEVX_HAIRPIN);
		} else {
			txq_alloc_elts(txq_ctrl);
			txq_ctrl->obj = mlx5_txq_obj_new
				(dev, i, priv->txpp_en ?
				MLX5_TXQ_OBJ_TYPE_DEVX_SQ :
				MLX5_TXQ_OBJ_TYPE_IBV);
		}
		if (!txq_ctrl->obj) {
			rte_errno = ENOMEM;
			goto error;
		}
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	do {
		mlx5_txq_release(dev, i);
	} while (i-- != 0);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Stop traffic on Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_rxq_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i)
		mlx5_rxq_release(dev, i);
}

/**
 * Start traffic on Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret = 0;
	enum mlx5_rxq_obj_type obj_type = MLX5_RXQ_OBJ_TYPE_IBV;
	struct mlx5_rxq_data *rxq = NULL;

	for (i = 0; i < priv->rxqs_n; ++i) {
		rxq = (*priv->rxqs)[i];
		if (rxq && rxq->lro) {
			obj_type =  MLX5_RXQ_OBJ_TYPE_DEVX_RQ;
			break;
		}
	}
	/* Allocate/reuse/resize mempool for Multi-Packet RQ. */
	if (mlx5_mprq_alloc_mp(dev)) {
		/* Should not release Rx queues but return immediately. */
		return -rte_errno;
	}
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_get(dev, i);
		struct rte_mempool *mp;

		if (!rxq_ctrl)
			continue;
		if (rxq_ctrl->type == MLX5_RXQ_TYPE_HAIRPIN) {
			rxq_ctrl->obj = mlx5_rxq_obj_new
				(dev, i, MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN);
			if (!rxq_ctrl->obj)
				goto error;
			continue;
		}
		/* Pre-register Rx mempool. */
		mp = mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq) ?
		     rxq_ctrl->rxq.mprq_mp : rxq_ctrl->rxq.mp;
		DRV_LOG(DEBUG,
			"port %u Rx queue %u registering"
			" mp %s having %u chunks",
			dev->data->port_id, rxq_ctrl->rxq.idx,
			mp->name, mp->nb_mem_chunks);
		mlx5_mr_update_mp(dev, &rxq_ctrl->rxq.mr_ctrl, mp);
		ret = rxq_alloc_elts(rxq_ctrl);
		if (ret)
			goto error;
		rxq_ctrl->obj = mlx5_rxq_obj_new(dev, i, obj_type);
		if (!rxq_ctrl->obj)
			goto error;
		if (obj_type == MLX5_RXQ_OBJ_TYPE_IBV)
			rxq_ctrl->wqn = rxq_ctrl->obj->wq->wq_num;
		else if (obj_type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ)
			rxq_ctrl->wqn = rxq_ctrl->obj->rq->id;
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	do {
		mlx5_rxq_release(dev, i);
	} while (i-- != 0);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Binds Tx queues to Rx queues for hairpin.
 *
 * Binds Tx queues to the target Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_hairpin_bind(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_modify_sq_attr sq_attr = { 0 };
	struct mlx5_devx_modify_rq_attr rq_attr = { 0 };
	struct mlx5_txq_ctrl *txq_ctrl;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct mlx5_devx_obj *sq;
	struct mlx5_devx_obj *rq;
	unsigned int i;
	int ret = 0;

	for (i = 0; i != priv->txqs_n; ++i) {
		txq_ctrl = mlx5_txq_get(dev, i);
		if (!txq_ctrl)
			continue;
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (!txq_ctrl->obj) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no txq object found: %d",
				dev->data->port_id, i);
			mlx5_txq_release(dev, i);
			return -rte_errno;
		}
		sq = txq_ctrl->obj->sq;
		rxq_ctrl = mlx5_rxq_get(dev,
					txq_ctrl->hairpin_conf.peers[0].queue);
		if (!rxq_ctrl) {
			mlx5_txq_release(dev, i);
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u no rxq object found: %d",
				dev->data->port_id,
				txq_ctrl->hairpin_conf.peers[0].queue);
			return -rte_errno;
		}
		if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN ||
		    rxq_ctrl->hairpin_conf.peers[0].queue != i) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u Tx queue %d can't be binded to "
				"Rx queue %d", dev->data->port_id,
				i, txq_ctrl->hairpin_conf.peers[0].queue);
			goto error;
		}
		rq = rxq_ctrl->obj->rq;
		if (!rq) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u hairpin no matching rxq: %d",
				dev->data->port_id,
				txq_ctrl->hairpin_conf.peers[0].queue);
			goto error;
		}
		sq_attr.state = MLX5_SQC_STATE_RDY;
		sq_attr.sq_state = MLX5_SQC_STATE_RST;
		sq_attr.hairpin_peer_rq = rq->id;
		sq_attr.hairpin_peer_vhca = priv->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_sq(sq, &sq_attr);
		if (ret)
			goto error;
		rq_attr.state = MLX5_SQC_STATE_RDY;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		rq_attr.hairpin_peer_sq = sq->id;
		rq_attr.hairpin_peer_vhca = priv->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_rq(rq, &rq_attr);
		if (ret)
			goto error;
		mlx5_txq_release(dev, i);
		mlx5_rxq_release(dev, txq_ctrl->hairpin_conf.peers[0].queue);
	}
	return 0;
error:
	mlx5_txq_release(dev, i);
	mlx5_rxq_release(dev, txq_ctrl->hairpin_conf.peers[0].queue);
	return -rte_errno;
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	int fine_inline;

	DRV_LOG(DEBUG, "port %u starting device", dev->data->port_id);
	fine_inline = rte_mbuf_dynflag_lookup
		(RTE_PMD_MLX5_FINE_GRANULARITY_INLINE, NULL);
	if (fine_inline > 0)
		rte_net_mlx5_dynf_inline_mask = 1UL << fine_inline;
	else
		rte_net_mlx5_dynf_inline_mask = 0;
	if (dev->data->nb_rx_queues > 0) {
		ret = mlx5_dev_configure_rss_reta(dev);
		if (ret) {
			DRV_LOG(ERR, "port %u reta config failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return -rte_errno;
		}
	}
	ret = mlx5_txpp_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Tx packet pacing init failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	ret = mlx5_txq_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Tx queue allocation failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	ret = mlx5_rxq_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Rx queue allocation failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	ret = mlx5_hairpin_bind(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u hairpin binding failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	/* Set started flag here for the following steps like control flow. */
	dev->data->dev_started = 1;
	ret = mlx5_rx_intr_vec_enable(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Rx interrupt vector creation failed",
			dev->data->port_id);
		goto error;
	}
	mlx5_os_stats_init(dev);
	ret = mlx5_traffic_enable(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to set defaults flows",
			dev->data->port_id);
		goto error;
	}
	/* Set a mask and offset of dynamic metadata flows into Rx queues. */
	mlx5_flow_rxq_dynf_metadata_set(dev);
	/* Set flags and context to convert Rx timestamps. */
	mlx5_rxq_timestamp_set(dev);
	/* Set a mask and offset of scheduling on timestamp into Tx queues. */
	mlx5_txq_dynf_timestamp_set(dev);
	/*
	 * In non-cached mode, it only needs to start the default mreg copy
	 * action and no flow created by application exists anymore.
	 * But it is worth wrapping the interface for further usage.
	 */
	ret = mlx5_flow_start_default(dev);
	if (ret) {
		DRV_LOG(DEBUG, "port %u failed to start default actions: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	rte_wmb();
	dev->tx_pkt_burst = mlx5_select_tx_function(dev);
	dev->rx_pkt_burst = mlx5_select_rx_function(dev);
	/* Enable datapath on secondary process. */
	mlx5_mp_os_req_start_rxtx(dev);
	if (priv->sh->intr_handle.fd >= 0) {
		priv->sh->port[priv->dev_port - 1].ih_port_id =
					(uint32_t)dev->data->port_id;
	} else {
		DRV_LOG(INFO, "port %u starts without LSC and RMV interrupts.",
			dev->data->port_id);
		dev->data->dev_conf.intr_conf.lsc = 0;
		dev->data->dev_conf.intr_conf.rmv = 0;
	}
	if (priv->sh->intr_handle_devx.fd >= 0)
		priv->sh->port[priv->dev_port - 1].devx_ih_port_id =
					(uint32_t)dev->data->port_id;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	/* Rollback. */
	dev->data->dev_started = 0;
	mlx5_flow_stop_default(dev);
	mlx5_traffic_disable(dev);
	mlx5_txq_stop(dev);
	mlx5_rxq_stop(dev);
	mlx5_txpp_stop(dev); /* Stop last. */
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
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
	struct mlx5_priv *priv = dev->data->dev_private;

	dev->data->dev_started = 0;
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_os_req_stop_rxtx(dev);
	usleep(1000 * priv->rxqs_n);
	DRV_LOG(DEBUG, "port %u stopping device", dev->data->port_id);
	mlx5_flow_stop_default(dev);
	/* Control flows for default traffic can be removed firstly. */
	mlx5_traffic_disable(dev);
	/* All RX queue flags will be cleared in the flush interface. */
	mlx5_flow_list_flush(dev, &priv->flows, true);
	mlx5_rx_intr_vec_disable(dev);
	priv->sh->port[priv->dev_port - 1].ih_port_id = RTE_MAX_ETHPORTS;
	priv->sh->port[priv->dev_port - 1].devx_ih_port_id = RTE_MAX_ETHPORTS;
	mlx5_txq_stop(dev);
	mlx5_rxq_stop(dev);
	mlx5_txpp_stop(dev);
}

/**
 * Enable traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_traffic_enable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
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
	const struct rte_ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;
	unsigned int j;
	int ret;

	/*
	 * Hairpin txq default flow should be created no matter if it is
	 * isolation mode. Or else all the packets to be sent will be sent
	 * out directly without the TX flow actions, e.g. encapsulation.
	 */
	for (i = 0; i != priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_txq_get(dev, i);
		if (!txq_ctrl)
			continue;
		if (txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN) {
			ret = mlx5_ctrl_flow_source_queue(dev, i);
			if (ret) {
				mlx5_txq_release(dev, i);
				goto error;
			}
		}
		mlx5_txq_release(dev, i);
	}
	if (priv->config.dv_esw_en && !priv->config.vf) {
		if (mlx5_flow_create_esw_table_zero_flow(dev))
			priv->fdb_def_rule = 1;
		else
			DRV_LOG(INFO, "port %u FDB default rule cannot be"
				" configured - only Eswitch group 0 flows are"
				" supported.", dev->data->port_id);
	}
	if (!priv->config.lacp_by_user && priv->pf_bond >= 0) {
		ret = mlx5_flow_lacp_miss(dev);
		if (ret)
			DRV_LOG(INFO, "port %u LACP rule cannot be created - "
				"forward LACP to kernel.", dev->data->port_id);
		else
			DRV_LOG(INFO, "LACP traffic will be missed in port %u."
				, dev->data->port_id);
	}
	if (priv->isolated)
		return 0;
	if (dev->data->promiscuous) {
		struct rte_flow_item_eth promisc = {
			.dst.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &promisc, &promisc);
		if (ret)
			goto error;
	}
	if (dev->data->all_multicast) {
		struct rte_flow_item_eth multicast = {
			.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &multicast, &multicast);
		if (ret)
			goto error;
	} else {
		/* Add broadcast/multicast flows. */
		for (i = 0; i != vlan_filter_n; ++i) {
			uint16_t vlan = priv->vlan_filter[i];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask =
				rte_flow_item_vlan_mask;

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
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&unicast.dst.addr_bytes,
		       mac->addr_bytes,
		       RTE_ETHER_ADDR_LEN);
		for (j = 0; j != vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask =
				rte_flow_item_vlan_mask;

			ret = mlx5_ctrl_flow_vlan(dev, &unicast,
						  &unicast_mask,
						  &vlan_spec,
						  &vlan_mask);
			if (ret)
				goto error;
		}
		if (!vlan_filter_n) {
			ret = mlx5_ctrl_flow(dev, &unicast, &unicast_mask);
			if (ret)
				goto error;
		}
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_flow_list_flush(dev, &priv->ctrl_flows, false);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}


/**
 * Disable traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
 */
void
mlx5_traffic_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->ctrl_flows, false);
}

/**
 * Restart traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_traffic_restart(struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		mlx5_traffic_disable(dev);
		return mlx5_traffic_enable(dev);
	}
	return 0;
}
