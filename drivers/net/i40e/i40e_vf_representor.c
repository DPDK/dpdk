/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_malloc.h>

#include "base/i40e_type.h"
#include "base/virtchnl.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "rte_pmd_i40e.h"

static int
i40e_vf_representor_link_update(struct rte_eth_dev *ethdev,
	int wait_to_complete)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	return i40e_dev_link_update(representor->adapter->eth_dev,
		wait_to_complete);
}
static void
i40e_vf_representor_dev_infos_get(struct rte_eth_dev *ethdev,
	struct rte_eth_dev_info *dev_info)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	/* get dev info for the vdev */
	dev_info->device = ethdev->device;

	dev_info->max_rx_queues = ethdev->data->nb_rx_queues;
	dev_info->max_tx_queues = ethdev->data->nb_tx_queues;

	dev_info->min_rx_bufsize = I40E_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = I40E_FRAME_SIZE_MAX;
	dev_info->hash_key_size = (I40E_VFQF_HKEY_MAX_INDEX + 1) *
		sizeof(uint32_t);
	dev_info->reta_size = ETH_RSS_RETA_SIZE_64;
	dev_info->flow_type_rss_offloads = I40E_RSS_OFFLOAD_ALL;
	dev_info->max_mac_addrs = I40E_NUM_MACADDR_MAX;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_MULTI_SEGS  |
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_QINQ_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
		DEV_TX_OFFLOAD_GRE_TNL_TSO |
		DEV_TX_OFFLOAD_IPIP_TNL_TSO |
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = I40E_DEFAULT_RX_PTHRESH,
			.hthresh = I40E_DEFAULT_RX_HTHRESH,
			.wthresh = I40E_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = I40E_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = I40E_DEFAULT_TX_PTHRESH,
			.hthresh = I40E_DEFAULT_TX_HTHRESH,
			.wthresh = I40E_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = I40E_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = I40E_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = I40E_MAX_RING_DESC,
		.nb_min = I40E_MIN_RING_DESC,
		.nb_align = I40E_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = I40E_MAX_RING_DESC,
		.nb_min = I40E_MIN_RING_DESC,
		.nb_align = I40E_ALIGN_RING_DESC,
	};

	dev_info->switch_info.name =
		representor->adapter->eth_dev->device->name;
	dev_info->switch_info.domain_id = representor->switch_domain_id;
	dev_info->switch_info.port_id = representor->vf_id;
}

static int
i40e_vf_representor_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
i40e_vf_representor_dev_start(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static void
i40e_vf_representor_dev_stop(__rte_unused struct rte_eth_dev *dev)
{
}

static int
i40e_vf_representor_rx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id,
	__rte_unused uint16_t nb_rx_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_rxconf *rx_conf,
	__rte_unused struct rte_mempool *mb_pool)
{
	return 0;
}

static int
i40e_vf_representor_tx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id,
	__rte_unused uint16_t nb_rx_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_txconf *tx_conf)
{
	return 0;
}

static void
i40evf_stat_update_48(uint64_t *offset,
		   uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = *stat - *offset;
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << I40E_48_BIT_WIDTH)) - *offset);

	*stat &= I40E_48_BIT_MASK;
}

static void
i40evf_stat_update_32(uint64_t *offset,
		   uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = (uint64_t)(*stat - *offset);
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << I40E_32_BIT_WIDTH)) - *offset);
}

static int
rte_pmd_i40e_get_vf_native_stats(uint16_t port,
			  uint16_t vf_id,
			  struct i40e_eth_stats *stats)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	i40e_update_vsi_stats(vsi);
	memcpy(stats, &vsi->eth_stats, sizeof(vsi->eth_stats));

	return 0;
}

static int
i40e_vf_representor_stats_get(struct rte_eth_dev *ethdev,
		struct rte_eth_stats *stats)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;
	struct i40e_eth_stats native_stats;
	int ret;

	ret = rte_pmd_i40e_get_vf_native_stats(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, &native_stats);
	if (ret == 0) {
		i40evf_stat_update_48(
			&representor->stats_offset.rx_bytes,
			&native_stats.rx_bytes);
		i40evf_stat_update_48(
			&representor->stats_offset.rx_unicast,
			&native_stats.rx_unicast);
		i40evf_stat_update_48(
			&representor->stats_offset.rx_multicast,
			&native_stats.rx_multicast);
		i40evf_stat_update_48(
			&representor->stats_offset.rx_broadcast,
			&native_stats.rx_broadcast);
		i40evf_stat_update_32(
			&representor->stats_offset.rx_discards,
			&native_stats.rx_discards);
		i40evf_stat_update_32(
			&representor->stats_offset.rx_unknown_protocol,
			&native_stats.rx_unknown_protocol);
		i40evf_stat_update_48(
			&representor->stats_offset.tx_bytes,
			&native_stats.tx_bytes);
		i40evf_stat_update_48(
			&representor->stats_offset.tx_unicast,
			&native_stats.tx_unicast);
		i40evf_stat_update_48(
			&representor->stats_offset.tx_multicast,
			&native_stats.tx_multicast);
		i40evf_stat_update_48(
			&representor->stats_offset.tx_broadcast,
			&native_stats.tx_broadcast);
		i40evf_stat_update_32(
			&representor->stats_offset.tx_errors,
			&native_stats.tx_errors);
		i40evf_stat_update_32(
			&representor->stats_offset.tx_discards,
			&native_stats.tx_discards);

		stats->ipackets = native_stats.rx_unicast +
			native_stats.rx_multicast +
			native_stats.rx_broadcast;
		stats->opackets = native_stats.tx_unicast +
			native_stats.tx_multicast +
			native_stats.tx_broadcast;
		stats->ibytes   = native_stats.rx_bytes;
		stats->obytes   = native_stats.tx_bytes;
		stats->ierrors  = native_stats.rx_discards;
		stats->oerrors  = native_stats.tx_errors + native_stats.tx_discards;
	}
	return ret;
}

static void
i40e_vf_representor_stats_reset(struct rte_eth_dev *ethdev)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_get_vf_native_stats(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, &representor->stats_offset);
}

static void
i40e_vf_representor_promiscuous_enable(struct rte_eth_dev *ethdev)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_set_vf_unicast_promisc(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, 1);
}

static void
i40e_vf_representor_promiscuous_disable(struct rte_eth_dev *ethdev)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_set_vf_unicast_promisc(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, 0);
}

static void
i40e_vf_representor_allmulticast_enable(struct rte_eth_dev *ethdev)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_set_vf_multicast_promisc(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id,  1);
}

static void
i40e_vf_representor_allmulticast_disable(struct rte_eth_dev *ethdev)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_set_vf_multicast_promisc(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id,  0);
}

static void
i40e_vf_representor_mac_addr_remove(struct rte_eth_dev *ethdev, uint32_t index)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_remove_vf_mac_addr(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, &ethdev->data->mac_addrs[index]);
}

static int
i40e_vf_representor_mac_addr_set(struct rte_eth_dev *ethdev,
		struct ether_addr *mac_addr)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	return rte_pmd_i40e_set_vf_mac_addr(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, mac_addr);
}

static int
i40e_vf_representor_vlan_filter_set(struct rte_eth_dev *ethdev,
		uint16_t vlan_id, int on)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;
	uint64_t vf_mask = 1ULL << representor->vf_id;

	return rte_pmd_i40e_set_vf_vlan_filter(
		representor->adapter->eth_dev->data->port_id,
		vlan_id, vf_mask, on);
}

static int
i40e_vf_representor_vlan_offload_set(struct rte_eth_dev *ethdev, int mask)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;
	struct rte_eth_dev *pdev;
	struct i40e_pf_vf *vf;
	struct i40e_vsi *vsi;
	struct i40e_pf *pf;
	uint32_t vfid;

	pdev = representor->adapter->eth_dev;
	vfid = representor->vf_id;

	if (!is_i40e_supported(pdev)) {
		PMD_DRV_LOG(ERR, "Invalid PF dev.");
		return -EINVAL;
	}

	pf = I40E_DEV_PRIVATE_TO_PF(pdev->data->dev_private);

	if (vfid >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vf = &pf->vfs[vfid];
	vsi = vf->vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		/* Enable or disable VLAN filtering offload */
		if (ethdev->data->dev_conf.rxmode.offloads &
		    DEV_RX_OFFLOAD_VLAN_FILTER)
			return i40e_vsi_config_vlan_filter(vsi, TRUE);
		else
			return i40e_vsi_config_vlan_filter(vsi, FALSE);
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping offload */
		if (ethdev->data->dev_conf.rxmode.offloads &
		    DEV_RX_OFFLOAD_VLAN_STRIP)
			return i40e_vsi_config_vlan_stripping(vsi, TRUE);
		else
			return i40e_vsi_config_vlan_stripping(vsi, FALSE);
	}

	return -EINVAL;
}

static void
i40e_vf_representor_vlan_strip_queue_set(struct rte_eth_dev *ethdev,
	__rte_unused uint16_t rx_queue_id, int on)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_i40e_set_vf_vlan_stripq(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, on);
}

static int
i40e_vf_representor_vlan_pvid_set(struct rte_eth_dev *ethdev, uint16_t vlan_id,
	__rte_unused int on)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	return rte_pmd_i40e_set_vf_vlan_insert(
		representor->adapter->eth_dev->data->port_id,
		representor->vf_id, vlan_id);
}

static const struct eth_dev_ops i40e_representor_dev_ops = {
	.dev_infos_get        = i40e_vf_representor_dev_infos_get,

	.dev_start            = i40e_vf_representor_dev_start,
	.dev_configure        = i40e_vf_representor_dev_configure,
	.dev_stop             = i40e_vf_representor_dev_stop,

	.rx_queue_setup       = i40e_vf_representor_rx_queue_setup,
	.tx_queue_setup       = i40e_vf_representor_tx_queue_setup,

	.link_update          = i40e_vf_representor_link_update,

	.stats_get            = i40e_vf_representor_stats_get,
	.stats_reset          = i40e_vf_representor_stats_reset,

	.promiscuous_enable   = i40e_vf_representor_promiscuous_enable,
	.promiscuous_disable  = i40e_vf_representor_promiscuous_disable,

	.allmulticast_enable  = i40e_vf_representor_allmulticast_enable,
	.allmulticast_disable = i40e_vf_representor_allmulticast_disable,

	.mac_addr_remove      = i40e_vf_representor_mac_addr_remove,
	.mac_addr_set         = i40e_vf_representor_mac_addr_set,

	.vlan_filter_set      = i40e_vf_representor_vlan_filter_set,
	.vlan_offload_set     = i40e_vf_representor_vlan_offload_set,
	.vlan_strip_queue_set = i40e_vf_representor_vlan_strip_queue_set,
	.vlan_pvid_set        = i40e_vf_representor_vlan_pvid_set

};

static uint16_t
i40e_vf_representor_rx_burst(__rte_unused void *rx_queue,
	__rte_unused struct rte_mbuf **rx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
i40e_vf_representor_tx_burst(__rte_unused void *tx_queue,
	__rte_unused struct rte_mbuf **tx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
i40e_vf_representor_init(struct rte_eth_dev *ethdev, void *init_params)
{
	struct i40e_vf_representor *representor = ethdev->data->dev_private;

	struct i40e_pf *pf;
	struct i40e_pf_vf *vf;
	struct rte_eth_link *link;

	representor->vf_id =
		((struct i40e_vf_representor *)init_params)->vf_id;
	representor->switch_domain_id =
		((struct i40e_vf_representor *)init_params)->switch_domain_id;
	representor->adapter =
		((struct i40e_vf_representor *)init_params)->adapter;

	pf = I40E_DEV_PRIVATE_TO_PF(
		representor->adapter->eth_dev->data->dev_private);

	if (representor->vf_id >= pf->vf_num)
		return -ENODEV;

	/* Set representor device ops */
	ethdev->dev_ops = &i40e_representor_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with the likes of testpmd.
	 */
	ethdev->rx_pkt_burst = i40e_vf_representor_rx_burst;
	ethdev->tx_pkt_burst = i40e_vf_representor_tx_burst;

	vf = &pf->vfs[representor->vf_id];

	if (!vf->vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -ENODEV;
	}

	ethdev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	ethdev->data->representor_id = representor->vf_id;

	/* Setting the number queues allocated to the VF */
	ethdev->data->nb_rx_queues = vf->vsi->nb_qps;
	ethdev->data->nb_tx_queues = vf->vsi->nb_qps;

	ethdev->data->mac_addrs = &vf->mac_addr;

	/* Link state. Inherited from PF */
	link = &representor->adapter->eth_dev->data->dev_link;

	ethdev->data->dev_link.link_speed = link->link_speed;
	ethdev->data->dev_link.link_duplex = link->link_duplex;
	ethdev->data->dev_link.link_status = link->link_status;
	ethdev->data->dev_link.link_autoneg = link->link_autoneg;

	return 0;
}

int
i40e_vf_representor_uninit(struct rte_eth_dev *ethdev)
{
	/* mac_addrs must not be freed because part of i40e_pf_vf */
	ethdev->data->mac_addrs = NULL;

	return 0;
}
