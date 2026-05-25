/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <ethdev_driver.h>
#include <bus_pci_driver.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_rwlock.h>
#include <rte_cycles.h>

#include <mlx5_malloc.h>

#include "mlx5_rxtx.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_autoconf.h"
#include "mlx5_devx.h"
#include "rte_pmd_mlx5.h"

/**
 * Get the interface index from device name.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Nonzero interface index on success, zero otherwise and rte_errno is set.
 */
unsigned int
mlx5_ifindex(const struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int ifindex;

	MLX5_ASSERT(priv);
	MLX5_ASSERT(priv->if_index);
	if (priv->master && priv->sh->bond.ifindex > 0)
		ifindex = priv->sh->bond.ifindex;
	else
		ifindex = priv->if_index;
	if (!ifindex)
		rte_errno = ENXIO;
	return ifindex;
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_configure(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int txqs_n = dev->data->nb_tx_queues;
	const uint8_t use_app_rss_key =
		!!dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key;
	int ret = 0;

	if (use_app_rss_key &&
	    (dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key_len !=
	     MLX5_RSS_HASH_KEY_LEN)) {
		DRV_LOG(ERR, "port %u RSS key len must be %s Bytes long",
			dev->data->port_id, RTE_STR(MLX5_RSS_HASH_KEY_LEN));
		rte_errno = EINVAL;
		return -rte_errno;
	}
	priv->rss_conf.rss_key = mlx5_realloc(priv->rss_conf.rss_key,
					      MLX5_MEM_RTE,
					      MLX5_RSS_HASH_KEY_LEN, 0,
					      SOCKET_ID_ANY);
	if (!priv->rss_conf.rss_key) {
		DRV_LOG(ERR, "port %u cannot allocate RSS hash key memory (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	if ((dev->data->dev_conf.txmode.offloads &
			RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP) &&
			rte_mbuf_dyn_tx_timestamp_register(NULL, NULL) != 0) {
		DRV_LOG(ERR, "port %u cannot register Tx timestamp field/flag",
			dev->data->port_id);
		return -rte_errno;
	}
	memcpy(priv->rss_conf.rss_key,
	       use_app_rss_key ?
	       dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key :
	       mlx5_rss_hash_default_key,
	       MLX5_RSS_HASH_KEY_LEN);
	priv->rss_conf.rss_key_len = MLX5_RSS_HASH_KEY_LEN;
	priv->rss_conf.rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	priv->rxq_privs = mlx5_realloc(priv->rxq_privs,
				       MLX5_MEM_RTE | MLX5_MEM_ZERO,
				       sizeof(void *) * rxqs_n, 0,
				       SOCKET_ID_ANY);
	if (rxqs_n && priv->rxq_privs == NULL) {
		DRV_LOG(ERR, "port %u cannot allocate rxq private data",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		DRV_LOG(INFO, "port %u Tx queues number update: %u -> %u",
			dev->data->port_id, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (priv->ext_txqs && txqs_n >= MLX5_EXTERNAL_TX_QUEUE_ID_MIN) {
		DRV_LOG(ERR, "port %u cannot handle this many Tx queues (%u), "
			"the maximal number of internal Tx queues is %u",
			dev->data->port_id, txqs_n,
			MLX5_EXTERNAL_TX_QUEUE_ID_MIN - 1);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rxqs_n > priv->sh->dev_cap.ind_table_max_size) {
		DRV_LOG(ERR, "port %u cannot handle this many Rx queues (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (priv->ext_rxqs && rxqs_n >= RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN) {
		DRV_LOG(ERR, "port %u cannot handle this many Rx queues (%u), "
			"the maximal number of internal Rx queues is %u",
			dev->data->port_id, rxqs_n,
			RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN - 1);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rxqs_n != priv->rxqs_n) {
		DRV_LOG(INFO, "port %u Rx queues number update: %u -> %u",
			dev->data->port_id, priv->rxqs_n, rxqs_n);
		priv->rxqs_n = rxqs_n;
	}
	priv->skip_default_rss_reta = 0;
	ret = mlx5_proc_priv_init(dev);
	if (ret)
		return ret;
	ret = mlx5_dev_set_mtu(dev, dev->data->mtu);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to set MTU to %u", dev->data->port_id,
			dev->data->mtu);
		return ret;
	}
	return 0;
}

/**
 * Configure default RSS reta.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_configure_rss_reta(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int i;
	unsigned int j;
	unsigned int reta_idx_n;
	int ret = 0;
	unsigned int *rss_queue_arr = NULL;
	unsigned int rss_queue_n = 0;

	if (priv->skip_default_rss_reta)
		return ret;
	rss_queue_arr = mlx5_malloc(0, rxqs_n * sizeof(unsigned int), 0,
				    SOCKET_ID_ANY);
	if (!rss_queue_arr) {
		DRV_LOG(ERR, "port %u cannot allocate RSS queue list (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	for (i = 0, j = 0; i < rxqs_n; i++) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_ctrl_get(dev, i);

		if (rxq_ctrl && !rxq_ctrl->is_hairpin)
			rss_queue_arr[j++] = i;
	}
	rss_queue_n = j;
	if (rss_queue_n > priv->sh->dev_cap.ind_table_max_size) {
		DRV_LOG(ERR, "port %u cannot handle this many Rx queues (%u)",
			dev->data->port_id, rss_queue_n);
		rte_errno = EINVAL;
		mlx5_free(rss_queue_arr);
		return -rte_errno;
	}
	DRV_LOG(INFO, "port %u Rx queues number update: %u -> %u",
		dev->data->port_id, priv->rxqs_n, rxqs_n);
	priv->rxqs_n = rxqs_n;
	/*
	 * If the requested number of RX queues is not a power of two,
	 * use the maximum indirection table size for better balancing.
	 * The result is always rounded to the next power of two.
	 */
	reta_idx_n = (1 << log2above((rss_queue_n & (rss_queue_n - 1)) ?
				     priv->sh->dev_cap.ind_table_max_size :
				     rss_queue_n));
	ret = mlx5_rss_reta_index_resize(dev, reta_idx_n);
	if (ret) {
		mlx5_free(rss_queue_arr);
		return ret;
	}
	/*
	 * When the number of RX queues is not a power of two,
	 * the remaining table entries are padded with reused WQs
	 * and hashes are not spread uniformly.
	 */
	for (i = 0, j = 0; (i != reta_idx_n); ++i) {
		(*priv->reta_idx)[i] = rss_queue_arr[j];
		if (++j == rss_queue_n)
			j = 0;
	}
	mlx5_free(rss_queue_arr);
	return ret;
}

/**
 * Sets default tuning parameters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] info
 *   Info structure output buffer.
 */
static void
mlx5_set_default_params(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	/* Minimum CPU utilization. */
	info->default_rxportconf.ring_size = 256;
	info->default_txportconf.ring_size = 256;
	info->default_rxportconf.burst_size = MLX5_RX_DEFAULT_BURST;
	info->default_txportconf.burst_size = MLX5_TX_DEFAULT_BURST;
	if (priv->link_speed_capa >> rte_bsf32(RTE_ETH_LINK_SPEED_100G)) {
		/* if supports at least 100G */
		info->default_rxportconf.nb_queues = 16;
		info->default_txportconf.nb_queues = 16;
		if (dev->data->nb_rx_queues > 2 ||
		    dev->data->nb_tx_queues > 2) {
			/* Max Throughput. */
			info->default_rxportconf.ring_size = 2048;
			info->default_txportconf.ring_size = 2048;
		}
	} else {
		info->default_rxportconf.nb_queues = 8;
		info->default_txportconf.nb_queues = 8;
		if (dev->data->nb_rx_queues > 2 ||
		    dev->data->nb_tx_queues > 2) {
			/* Max Throughput. */
			info->default_rxportconf.ring_size = 4096;
			info->default_txportconf.ring_size = 4096;
		}
	}
}

/**
 * Sets tx mbuf limiting parameters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] info
 *   Info structure output buffer.
 */
static void
mlx5_set_txlimit_params(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_port_config *config = &priv->config;
	unsigned int inlen;
	uint16_t nb_max;

	inlen = (config->txq_inline_max == MLX5_ARG_UNSET) ?
		MLX5_SEND_DEF_INLINE_LEN :
		(unsigned int)config->txq_inline_max;
	MLX5_ASSERT(config->txq_inline_min >= 0);
	inlen = RTE_MAX(inlen, (unsigned int)config->txq_inline_min);
	inlen = RTE_MIN(inlen, MLX5_WQE_SIZE_MAX +
			       MLX5_ESEG_MIN_INLINE_SIZE -
			       MLX5_WQE_CSEG_SIZE -
			       MLX5_WQE_ESEG_SIZE -
			       MLX5_WQE_DSEG_SIZE * 2);
	nb_max = (MLX5_WQE_SIZE_MAX +
		  MLX5_ESEG_MIN_INLINE_SIZE -
		  MLX5_WQE_CSEG_SIZE -
		  MLX5_WQE_ESEG_SIZE -
		  MLX5_WQE_DSEG_SIZE -
		  inlen) / MLX5_WSEG_SIZE;
	info->tx_desc_lim.nb_seg_max = nb_max;
	info->tx_desc_lim.nb_mtu_seg_max = nb_max;
}

/**
 * Get maximal work queue size in WQEs
 *
 * @param sh
 *   Pointer to the device shared context.
 * @return
 *   Maximal number of WQEs in queue
 */
uint16_t
mlx5_dev_get_max_wq_size(struct mlx5_dev_ctx_shared *sh)
{
	uint16_t max_wqe = MLX5_WQ_INDEX_MAX;

	if (sh->cdev->config.devx) {
		/* use HCA properties for DevX config */
		MLX5_ASSERT(sh->cdev->config.hca_attr.log_max_wq_sz != 0);
		MLX5_ASSERT(sh->cdev->config.hca_attr.log_max_wq_sz < MLX5_WQ_INDEX_WIDTH);
		if (sh->cdev->config.hca_attr.log_max_wq_sz != 0 &&
		    sh->cdev->config.hca_attr.log_max_wq_sz < MLX5_WQ_INDEX_WIDTH)
			max_wqe = 1u << sh->cdev->config.hca_attr.log_max_wq_sz;
	} else {
		/* use IB device capabilities */
		MLX5_ASSERT(sh->dev_cap.max_qp_wr > 0);
		MLX5_ASSERT((unsigned int)sh->dev_cap.max_qp_wr <= MLX5_WQ_INDEX_MAX);
		if (sh->dev_cap.max_qp_wr > 0 &&
		    (uint32_t)sh->dev_cap.max_qp_wr <= MLX5_WQ_INDEX_MAX)
			max_wqe = (uint16_t)sh->dev_cap.max_qp_wr;
	}
	return max_wqe;
}

/**
 * Get switch port ID for given DPDK port.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @return
 *   Switch port ID reported through rte_eth_dev_info_get().
 */
static uint16_t
mlx5_dev_switch_info_port_id_get(struct rte_eth_dev *dev)
{
	if (rte_eth_dev_is_repr(dev))
		return dev->data->port_id;

	return UINT16_MAX;
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
int
mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int max;
	uint16_t max_wqe;

	info->min_mtu = priv->min_mtu;
	info->max_mtu = priv->max_mtu;
	info->max_rx_pktlen = info->max_mtu + MLX5_ETH_OVERHEAD;
	info->min_rx_bufsize = 32;
	info->max_lro_pkt_size = priv->sh->config.lro_allowed ?
				 MLX5_MAX_LRO_SIZE : 0;
	/*
	 * Since we need one CQ per QP, the limit is the minimum number
	 * between the two values.
	 */
	max = RTE_MIN(priv->sh->dev_cap.max_cq, priv->sh->dev_cap.max_qp);
	/* max_rx_queues is uint16_t. */
	max = RTE_MIN(max, (unsigned int)UINT16_MAX);
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	info->max_mac_addrs = MLX5_MAX_UC_MAC_ADDRESSES;
	info->rx_queue_offload_capa = mlx5_get_rx_queue_offloads(dev);
	info->rx_seg_capa.max_nseg = MLX5_MAX_RXQ_NSEG;
	info->rx_seg_capa.multi_pools = !priv->config.mprq.enabled;
	info->rx_seg_capa.offset_allowed = !priv->config.mprq.enabled;
	info->rx_seg_capa.offset_align_log2 = 0;
	info->rx_seg_capa.selective_rx = !!priv->sh->null_mr;
	info->rx_offload_capa = (mlx5_get_rx_port_offloads() |
				 info->rx_queue_offload_capa);
	info->tx_offload_capa = mlx5_get_tx_port_offloads(dev);
	info->dev_capa = RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP;
	info->if_index = mlx5_ifindex(dev);
	info->reta_size = priv->reta_idx_n ?
		priv->reta_idx_n : priv->sh->dev_cap.ind_table_max_size;
	info->hash_key_size = MLX5_RSS_HASH_KEY_LEN;
	info->speed_capa = priv->link_speed_capa;
	info->flow_type_rss_offloads = ~MLX5_RSS_HF_MASK;
	mlx5_set_default_params(dev, info);
	mlx5_set_txlimit_params(dev, info);
	max_wqe = mlx5_dev_get_max_wq_size(priv->sh);
	info->rx_desc_lim.nb_max = max_wqe;
	info->tx_desc_lim.nb_max = max_wqe;
	if (priv->sh->cdev->config.hca_attr.mem_rq_rmp &&
	    priv->obj_ops.rxq_obj_new == mlx5_devx_obj_ops.rxq_obj_new)
		info->dev_capa |= RTE_ETH_DEV_CAPA_RXQ_SHARE;
	info->switch_info.name = dev->data->name;
	info->switch_info.domain_id = priv->domain_id;
	info->switch_info.port_id = mlx5_dev_switch_info_port_id_get(dev);
	info->switch_info.rx_domain = 0; /* No sub Rx domains. */
	if (priv->representor) {
		uint16_t port_id;

		MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;

			if (!opriv ||
			    opriv->representor ||
			    opriv->sh != priv->sh ||
			    opriv->domain_id != priv->domain_id)
				continue;
			/*
			 * Override switch name with that of the master
			 * device.
			 */
			info->switch_info.name = opriv->dev_data->name;
			break;
		}
	}
	return 0;
}

/**
 * Calculate representor ID from port switch info.
 *
 * Uint16 representor ID bits definition:
 *   pf: 2
 *   type: 2
 *   vf/sf: 12
 *
 * @param info
 *   Port switch info.
 * @param hpf_type
 *   Use this type if port is HPF.
 *
 * @return
 *   Encoded representor ID.
 */
uint16_t
mlx5_representor_id_encode(const struct mlx5_switch_info *info,
			   enum rte_eth_representor_type hpf_type)
{
	enum rte_eth_representor_type type;
	uint16_t repr = info->port_name;
	int32_t pf = info->pf_num;

	switch (info->name_type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		if (!info->representor)
			return UINT16_MAX;
		type = RTE_ETH_REPRESENTOR_PF;
		pf = info->mpesw_owner;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
		type = RTE_ETH_REPRESENTOR_SF;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
		type = hpf_type;
		repr = UINT16_MAX;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
	default:
		type = RTE_ETH_REPRESENTOR_VF;
		break;
	}
	return MLX5_REPRESENTOR_ID(pf, type, repr);
}

static unsigned int
mlx5_representor_info_count_one(struct mlx5_priv *priv)
{
	switch (priv->port_info.type) {
	case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
		return 2;
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		/* Only representor uplinks should be reported */
		if (!priv->representor)
			return 0;
		return 1;
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		return 1;
	}
}

static unsigned int
mlx5_representor_info_count(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t port_id;
	unsigned int count = 0;

	MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
		struct mlx5_priv *opriv = rte_eth_devices[port_id].data->dev_private;

		if (!opriv ||
		    opriv->sh != priv->sh ||
		    opriv->domain_id != priv->domain_id)
			continue;

		count += mlx5_representor_info_count_one(opriv);
	}

	return count;
}

static void
mlx5_representor_info_fill_one(struct mlx5_priv *priv,
			       struct rte_eth_representor_info *info)
{
	struct rte_eth_representor_range *range;
	unsigned int count;

	count = mlx5_representor_info_count_one(priv);
	if (count == 0)
		return;

	if (info->nb_ranges + count > info->nb_ranges_alloc) {
		DRV_LOG(ERR, "port %u representor info already full", priv->dev_data->port_id);
		return;
	}

	range = &info->ranges[info->nb_ranges];

	switch (priv->port_info.type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		range->type = RTE_ETH_REPRESENTOR_PF;
		range->controller = priv->port_info.ctrl_num;
		range->pf = priv->port_info.port_num;
		range->id_base = priv->dev_data->port_id;
		range->id_end = range->id_base;
		snprintf(range->name, sizeof(range->name), "pf%d", range->pf);
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
		/* Secondly, fill in SF variant. */
		range->type = RTE_ETH_REPRESENTOR_SF;
		range->controller = priv->port_info.ctrl_num;
		range->pf = priv->port_info.pf_num;
		range->sf = priv->port_info.port_num;
		range->id_base = priv->dev_data->port_id;
		range->id_end = range->id_base;
		snprintf(range->name, sizeof(range->name), "pf%dsf", range->pf);
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
		/*
		 * Host PF can be probed either through VF(0xffff) or SF(0xffff).
		 * Firstly fill in VF variant.
		 */
		range->type = RTE_ETH_REPRESENTOR_VF;
		range->controller = priv->port_info.ctrl_num;
		range->pf = priv->port_info.pf_num;
		range->vf = UINT16_MAX;
		range->id_base = priv->dev_data->port_id;
		range->id_end = range->id_base;
		snprintf(range->name, sizeof(range->name), "pf%dvf", range->pf);

		/* Move the SF variant. */
		range++;

		/* Fill in SF variant. */
		range->type = RTE_ETH_REPRESENTOR_SF;
		range->controller = priv->port_info.ctrl_num;
		range->pf = priv->port_info.pf_num;
		range->sf = UINT16_MAX;
		range->id_base = priv->dev_data->port_id;
		range->id_end = range->id_base;
		snprintf(range->name, sizeof(range->name), "pf%dsf", range->pf);
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* FALLTHROUGH */
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		range->type = RTE_ETH_REPRESENTOR_VF;
		range->controller = priv->port_info.ctrl_num;
		range->pf = priv->port_info.pf_num;
		range->vf = priv->port_info.port_num;
		range->id_base = priv->dev_data->port_id;
		range->id_end = range->id_base;
		snprintf(range->name, sizeof(range->name), "pf%dvf", range->pf);
		break;
	}

	info->nb_ranges += count;
}

static unsigned int
mlx5_representor_info_fill(struct rte_eth_dev *dev,
			   struct rte_eth_representor_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t port_id;

	info->controller = priv->port_info.ctrl_num;
	info->pf = RTE_BUS_DEVICE(dev->device, struct rte_pci_device)->addr.function;

	MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
		struct mlx5_priv *opriv = rte_eth_devices[port_id].data->dev_private;

		if (!opriv ||
		    opriv->sh != priv->sh ||
		    opriv->domain_id != priv->domain_id)
			continue;

		mlx5_representor_info_fill_one(opriv, info);
	}

	return info->nb_ranges;
}

/**
 * DPDK callback to get information about representor.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Nullable info structure output buffer.
 *
 * @return
 *   negative on error, or the number of representor ranges.
 */
int
mlx5_representor_info_get(struct rte_eth_dev *dev,
			  struct rte_eth_representor_info *info)
{
	if (info == NULL)
		return mlx5_representor_info_count(dev);

	return mlx5_representor_info_fill(dev, info);

}

/**
 * Get firmware version of a device.
 *
 * @param dev
 *   Ethernet device port.
 * @param fw_ver
 *   String output allocated by caller.
 * @param fw_size
 *   Size of the output string, including terminating null byte.
 *
 * @return
 *   0 on success, or the size of the non truncated string if too big.
 */
int
mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_cap *attr = &priv->sh->dev_cap;
	size_t size = strnlen(attr->fw_ver, sizeof(attr->fw_ver)) + 1;

	if (fw_size < size)
		return size;
	if (fw_ver != NULL)
		strlcpy(fw_ver, attr->fw_ver, fw_size);
	return 0;
}

/**
 * Get supported packet types.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   A pointer to the supported Packet types array.
 */
const uint32_t *
mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev, size_t *no_of_elements)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
	};

	if (dev->rx_pkt_burst == mlx5_rx_burst ||
	    dev->rx_pkt_burst == mlx5_rx_burst_out_of_order ||
	    dev->rx_pkt_burst == mlx5_rx_burst_mprq ||
	    dev->rx_pkt_burst == mlx5_rx_burst_vec ||
	    dev->rx_pkt_burst == mlx5_rx_burst_mprq_vec) {
		*no_of_elements = RTE_DIM(ptypes);
		return ptypes;
	}
	return NULL;
}

/**
 * DPDK callback to change the MTU.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param in_mtu
 *   New MTU.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	uint16_t kern_mtu = 0;
	int ret;

	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;

	if (kern_mtu == mtu) {
		DRV_LOG(DEBUG, "port %u adapter MTU was already set to %u",
			dev->data->port_id, mtu);
		return 0;
	}

	/* Set kernel interface MTU first. */
	ret = mlx5_set_mtu(dev, mtu);
	if (ret)
		return ret;
	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;
	if (kern_mtu == mtu) {
		DRV_LOG(DEBUG, "port %u adapter MTU set to %u",
			dev->data->port_id, mtu);
		return 0;
	}
	rte_errno = EAGAIN;
	return -rte_errno;
}

static bool
mlx5_selective_rx_enabled(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	for (uint32_t q = 0; q < priv->rxqs_n; ++q) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_ctrl_get(dev, q);

		if (rxq_ctrl == NULL || rxq_ctrl->is_hairpin)
			continue;
		for (uint16_t s = 0; s < rxq_ctrl->rxq.rxseg_n; s++) {
			if (rxq_ctrl->rxq.rxseg[s].mp == NULL)
				return true;
		}
	}

	return false;
}

/**
 * Configure the RX function to use.
 *
 * @param dev
 *   Pointer to private data structure.
 *
 * @return
 *   Pointer to selected Rx burst function.
 */
eth_rx_burst_t
mlx5_select_rx_function(struct rte_eth_dev *dev)
{
	eth_rx_burst_t rx_pkt_burst = mlx5_rx_burst;

	MLX5_ASSERT(dev != NULL);
	if (mlx5_selective_rx_enabled(dev)) {
		DRV_LOG(DEBUG, "port %u forced to scalar SPRQ Rx (selective Rx configured)",
			dev->data->port_id);
		return rx_pkt_burst;
	}
	if (mlx5_shared_rq_enabled(dev)) {
		rx_pkt_burst = mlx5_rx_burst_out_of_order;
		DRV_LOG(DEBUG, "port %u forced to use SPRQ"
			" Rx function with Out-of-Order completions",
			dev->data->port_id);
	} else if (mlx5_check_vec_rx_support(dev) > 0) {
		if (mlx5_mprq_enabled(dev)) {
			rx_pkt_burst = mlx5_rx_burst_mprq_vec;
			DRV_LOG(DEBUG, "port %u selected vectorized"
				" MPRQ Rx function", dev->data->port_id);
		} else {
			rx_pkt_burst = mlx5_rx_burst_vec;
			DRV_LOG(DEBUG, "port %u selected vectorized"
				" SPRQ Rx function", dev->data->port_id);
		}
	} else if (mlx5_mprq_enabled(dev)) {
		rx_pkt_burst = mlx5_rx_burst_mprq;
		DRV_LOG(DEBUG, "port %u selected MPRQ Rx function",
			dev->data->port_id);
	} else {
		DRV_LOG(DEBUG, "port %u selected SPRQ Rx function",
			dev->data->port_id);
	}
	return rx_pkt_burst;
}

/**
 * Get the E-Switch parameters by port id.
 *
 * @param[in] port
 *   Device port id.
 * @param[in] valid
 *   Device port id is valid, skip check. This flag is useful
 *   when trials are performed from probing and device is not
 *   flagged as valid yet (in attaching process).
 * @param[out] es_domain_id
 *   E-Switch domain id.
 * @param[out] es_port_id
 *   The port id of the port in the E-Switch.
 *
 * @return
 *   pointer to device private data structure containing data needed
 *   on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_priv *
mlx5_port_to_eswitch_info(uint16_t port, bool valid)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;

	if (port >= RTE_MAX_ETHPORTS) {
		rte_errno = EINVAL;
		return NULL;
	}
	if (!valid && !rte_eth_dev_is_valid_port(port)) {
		rte_errno = ENODEV;
		return NULL;
	}
	dev = &rte_eth_devices[port];
	priv = dev->data->dev_private;
	if (!priv->sh->esw_mode) {
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

/**
 * Get the E-Switch parameters by device instance.
 *
 * @param[in] port
 *   Device port id.
 * @param[out] es_domain_id
 *   E-Switch domain id.
 * @param[out] es_port_id
 *   The port id of the port in the E-Switch.
 *
 * @return
 *   pointer to device private data structure containing data needed
 *   on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_priv *
mlx5_dev_to_eswitch_info(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv;

	priv = dev->data->dev_private;
	if (!priv->sh->esw_mode) {
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

/**
 * DPDK callback to retrieve hairpin capabilities.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] cap
 *   Storage for hairpin capability data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hairpin_cap_get(struct rte_eth_dev *dev, struct rte_eth_hairpin_cap *cap)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *hca_attr;

	if (!mlx5_devx_obj_ops_en(priv->sh)) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	cap->max_nb_queues = UINT16_MAX;
	cap->max_rx_2_tx = 1;
	cap->max_tx_2_rx = 1;
	cap->max_nb_desc = 8192;
	hca_attr = &priv->sh->cdev->config.hca_attr;
	cap->rx_cap.locked_device_memory = hca_attr->hairpin_data_buffer_locked;
	cap->rx_cap.rte_memory = 0;
	cap->tx_cap.locked_device_memory = 0;
	cap->tx_cap.rte_memory = hca_attr->hairpin_sq_wq_in_host_mem;
	return 0;
}

/**
 * Indicate to ethdev layer, what configuration must be restored.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] op
 *   Type of operation which might require.
 * @param[out] flags
 *   Restore flags will be stored here.
 */
uint64_t
mlx5_get_restore_flags(__rte_unused struct rte_eth_dev *dev,
		       __rte_unused enum rte_eth_dev_operation op)
{
	/* mlx5 PMD does not require any configuration restore. */
	return 0;
}

/**
 * Query minimum and maximum allowed MTU value on the device.
 *
 * This functions will always return valid MTU bounds.
 * In case platform-specific implementation fails or current platform does not support it,
 * the fallback default values will be used.
 *
 * @param[in] dev
 *   Pointer to Ethernet device
 * @param[out] min_mtu
 *   Minimum MTU value output buffer.
 * @param[out] max_mtu
 *   Maximum MTU value output buffer.
 */
void
mlx5_get_mtu_bounds(struct rte_eth_dev *dev, uint16_t *min_mtu, uint16_t *max_mtu)
{
	int ret;

	MLX5_ASSERT(min_mtu != NULL);
	MLX5_ASSERT(max_mtu != NULL);

	ret = mlx5_os_get_mtu_bounds(dev, min_mtu, max_mtu);
	if (ret < 0) {
		if (ret != -ENOTSUP)
			DRV_LOG(INFO, "port %u failed to query MTU bounds, using fallback values",
				dev->data->port_id);
		*min_mtu = MLX5_ETH_MIN_MTU;
		*max_mtu = MLX5_ETH_MAX_MTU;

		/* This function does not fail. Clear rte_errno. */
		rte_errno = 0;
	}

	DRV_LOG(INFO, "port %u minimum MTU is %u", dev->data->port_id, *min_mtu);
	DRV_LOG(INFO, "port %u maximum MTU is %u", dev->data->port_id, *max_mtu);
}
