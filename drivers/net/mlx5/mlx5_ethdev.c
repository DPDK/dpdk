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
	       rss_hash_default_key,
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
	if ((priv->link_speed_capa & RTE_ETH_LINK_SPEED_200G) |
		(priv->link_speed_capa & RTE_ETH_LINK_SPEED_100G)) {
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

	/* FIXME: we should ask the device for these values. */
	info->min_rx_bufsize = 32;
	info->max_rx_pktlen = 65536;
	info->max_lro_pkt_size = MLX5_MAX_LRO_SIZE;
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
	if (priv->sh->cdev->config.hca_attr.mem_rq_rmp &&
	    priv->obj_ops.rxq_obj_new == devx_obj_ops.rxq_obj_new)
		info->dev_capa |= RTE_ETH_DEV_CAPA_RXQ_SHARE;
	info->switch_info.name = dev->data->name;
	info->switch_info.domain_id = priv->domain_id;
	info->switch_info.port_id = priv->representor_id;
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

/**
 * DPDK callback to get information about representor.
 *
 * Representor ID bits definition:
 *   vf/sf: 12
 *   type: 2
 *   pf: 2
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
	struct mlx5_priv *priv = dev->data->dev_private;
	int n_type = 5; /* Representor types: PF, VF, HPF@VF, SF and HPF@SF. */
	int n_pf = 2; /* Number of PFs. */
	int i = 0, pf;
	int n_entries;

	if (info == NULL)
		goto out;

	n_entries = n_type * n_pf;
	if ((uint32_t)n_entries > info->nb_ranges_alloc)
		n_entries = info->nb_ranges_alloc;

	info->controller = 0;
	info->pf = 0;
	if (mlx5_is_port_on_mpesw_device(priv)) {
		info->pf = priv->mpesw_port;
		/* PF range, both ports will show the same information. */
		info->ranges[i].type = RTE_ETH_REPRESENTOR_PF;
		info->ranges[i].controller = 0;
		info->ranges[i].pf = priv->mpesw_owner + 1;
		info->ranges[i].vf = 0;
		/*
		 * The representor indexes should be the values set of "priv->mpesw_port".
		 * In the real case now, only 1 PF/UPLINK representor is supported.
		 * The port index will always be the value of "owner + 1".
		 */
		info->ranges[i].id_base =
			MLX5_REPRESENTOR_ID(priv->mpesw_owner, info->ranges[i].type,
					    info->ranges[i].pf);
		info->ranges[i].id_end =
			MLX5_REPRESENTOR_ID(priv->mpesw_owner, info->ranges[i].type,
					    info->ranges[i].pf);
		snprintf(info->ranges[i].name, sizeof(info->ranges[i].name),
			 "pf%d", info->ranges[i].pf);
		i++;
	} else if (priv->pf_bond >= 0)
		info->pf = priv->pf_bond;
	for (pf = 0; pf < n_pf; ++pf) {
		/* VF range. */
		info->ranges[i].type = RTE_ETH_REPRESENTOR_VF;
		info->ranges[i].controller = 0;
		info->ranges[i].pf = pf;
		info->ranges[i].vf = 0;
		info->ranges[i].id_base =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, 0);
		info->ranges[i].id_end =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		snprintf(info->ranges[i].name,
			 sizeof(info->ranges[i].name), "pf%dvf", pf);
		i++;
		if (i == n_entries)
			break;
		/* HPF range of VF type. */
		info->ranges[i].type = RTE_ETH_REPRESENTOR_VF;
		info->ranges[i].controller = 0;
		info->ranges[i].pf = pf;
		info->ranges[i].vf = UINT16_MAX;
		info->ranges[i].id_base =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		info->ranges[i].id_end =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		snprintf(info->ranges[i].name,
			 sizeof(info->ranges[i].name), "pf%dvf", pf);
		i++;
		if (i == n_entries)
			break;
		/* SF range. */
		info->ranges[i].type = RTE_ETH_REPRESENTOR_SF;
		info->ranges[i].controller = 0;
		info->ranges[i].pf = pf;
		info->ranges[i].vf = 0;
		info->ranges[i].id_base =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, 0);
		info->ranges[i].id_end =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		snprintf(info->ranges[i].name,
			 sizeof(info->ranges[i].name), "pf%dsf", pf);
		i++;
		if (i == n_entries)
			break;
		/* HPF range of SF type. */
		info->ranges[i].type = RTE_ETH_REPRESENTOR_SF;
		info->ranges[i].controller = 0;
		info->ranges[i].pf = pf;
		info->ranges[i].vf = UINT16_MAX;
		info->ranges[i].id_base =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		info->ranges[i].id_end =
			MLX5_REPRESENTOR_ID(pf, info->ranges[i].type, -1);
		snprintf(info->ranges[i].name,
			 sizeof(info->ranges[i].name), "pf%dsf", pf);
		i++;
		if (i == n_entries)
			break;
	}
	info->nb_ranges = i;
out:
	return n_type * n_pf;
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
mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev)
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
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == mlx5_rx_burst ||
	    dev->rx_pkt_burst == mlx5_rx_burst_mprq ||
	    dev->rx_pkt_burst == mlx5_rx_burst_vec ||
	    dev->rx_pkt_burst == mlx5_rx_burst_mprq_vec)
		return ptypes;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t kern_mtu = 0;
	int ret;

	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;
	/* Set kernel interface MTU first. */
	ret = mlx5_set_mtu(dev, mtu);
	if (ret)
		return ret;
	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;
	if (kern_mtu == mtu) {
		priv->mtu = mtu;
		DRV_LOG(DEBUG, "port %u adapter MTU set to %u",
			dev->data->port_id, mtu);
		return 0;
	}
	rte_errno = EAGAIN;
	return -rte_errno;
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
	if (mlx5_check_vec_rx_support(dev) > 0) {
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
