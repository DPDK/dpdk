/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_malloc.h>

#include "base/ixgbe_type.h"
#include "base/ixgbe_vf.h"
#include "ixgbe_ethdev.h"
#include "ixgbe_rxtx.h"
#include "rte_pmd_ixgbe.h"


static int
ixgbe_vf_representor_link_update(struct rte_eth_dev *ethdev,
	int wait_to_complete)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;

	return ixgbe_dev_link_update_share(representor->pf_ethdev,
		wait_to_complete, 0);
}

static int
ixgbe_vf_representor_mac_addr_set(struct rte_eth_dev *ethdev,
	struct ether_addr *mac_addr)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;

	return rte_pmd_ixgbe_set_vf_mac_addr(
		representor->pf_ethdev->data->port_id,
		representor->vf_id, mac_addr);
}

static void
ixgbe_vf_representor_dev_infos_get(struct rte_eth_dev *ethdev,
	struct rte_eth_dev_info *dev_info)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;

	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(
		representor->pf_ethdev->data->dev_private);

	dev_info->device = representor->pf_ethdev->device;

	dev_info->min_rx_bufsize = 1024;
	/**< Minimum size of RX buffer. */
	dev_info->max_rx_pktlen = 9728;
	/**< Maximum configurable length of RX pkt. */
	dev_info->max_rx_queues = IXGBE_VF_MAX_RX_QUEUES;
	/**< Maximum number of RX queues. */
	dev_info->max_tx_queues = IXGBE_VF_MAX_TX_QUEUES;
	/**< Maximum number of TX queues. */

	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	/**< Maximum number of MAC addresses. */

	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |	DEV_RX_OFFLOAD_UDP_CKSUM  |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	/**< Device RX offload capabilities. */

	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM | DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO;
	/**< Device TX offload capabilities. */

	dev_info->speed_capa =
		representor->pf_ethdev->data->dev_link.link_speed;
	/**< Supported speeds bitmap (ETH_LINK_SPEED_). */

	dev_info->switch_info.name =
		representor->pf_ethdev->device->name;
	dev_info->switch_info.domain_id = representor->switch_domain_id;
	dev_info->switch_info.port_id = representor->vf_id;
}

static int ixgbe_vf_representor_dev_configure(
		__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int ixgbe_vf_representor_rx_queue_setup(
	__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id,
	__rte_unused uint16_t nb_rx_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_rxconf *rx_conf,
	__rte_unused struct rte_mempool *mb_pool)
{
	return 0;
}

static int ixgbe_vf_representor_tx_queue_setup(
	__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id,
	__rte_unused uint16_t nb_rx_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_txconf *tx_conf)
{
	return 0;
}

static int ixgbe_vf_representor_dev_start(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static void ixgbe_vf_representor_dev_stop(__rte_unused struct rte_eth_dev *dev)
{
}

static int
ixgbe_vf_representor_vlan_filter_set(struct rte_eth_dev *ethdev,
	uint16_t vlan_id, int on)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;
	uint64_t vf_mask = 1ULL << representor->vf_id;

	return rte_pmd_ixgbe_set_vf_vlan_filter(
		representor->pf_ethdev->data->port_id, vlan_id, vf_mask, on);
}

static void
ixgbe_vf_representor_vlan_strip_queue_set(struct rte_eth_dev *ethdev,
	__rte_unused uint16_t rx_queue_id, int on)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;

	rte_pmd_ixgbe_set_vf_vlan_stripq(representor->pf_ethdev->data->port_id,
		representor->vf_id, on);
}

struct eth_dev_ops ixgbe_vf_representor_dev_ops = {
	.dev_infos_get		= ixgbe_vf_representor_dev_infos_get,

	.dev_start		= ixgbe_vf_representor_dev_start,
	.dev_configure		= ixgbe_vf_representor_dev_configure,
	.dev_stop		= ixgbe_vf_representor_dev_stop,

	.rx_queue_setup		= ixgbe_vf_representor_rx_queue_setup,
	.tx_queue_setup		= ixgbe_vf_representor_tx_queue_setup,

	.link_update		= ixgbe_vf_representor_link_update,

	.vlan_filter_set	= ixgbe_vf_representor_vlan_filter_set,
	.vlan_strip_queue_set	= ixgbe_vf_representor_vlan_strip_queue_set,

	.mac_addr_set		= ixgbe_vf_representor_mac_addr_set,
};

static uint16_t
ixgbe_vf_representor_rx_burst(__rte_unused void *rx_queue,
	__rte_unused struct rte_mbuf **rx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
ixgbe_vf_representor_tx_burst(__rte_unused void *tx_queue,
	__rte_unused struct rte_mbuf **tx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
ixgbe_vf_representor_init(struct rte_eth_dev *ethdev, void *init_params)
{
	struct ixgbe_vf_representor *representor = ethdev->data->dev_private;

	struct ixgbe_vf_info *vf_data;
	struct rte_pci_device *pci_dev;
	struct rte_eth_link *link;

	if (!representor)
		return -ENOMEM;

	representor->vf_id =
		((struct ixgbe_vf_representor *)init_params)->vf_id;
	representor->switch_domain_id =
		((struct ixgbe_vf_representor *)init_params)->switch_domain_id;
	representor->pf_ethdev =
		((struct ixgbe_vf_representor *)init_params)->pf_ethdev;

	pci_dev = RTE_ETH_DEV_TO_PCI(representor->pf_ethdev);

	if (representor->vf_id >= pci_dev->max_vfs)
		return -ENODEV;

	ethdev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;

	/* Set representor device ops */
	ethdev->dev_ops = &ixgbe_vf_representor_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with the likes of testpmd.
	 */
	ethdev->rx_pkt_burst = ixgbe_vf_representor_rx_burst;
	ethdev->tx_pkt_burst = ixgbe_vf_representor_tx_burst;

	/* Setting the number queues allocated to the VF */
	ethdev->data->nb_rx_queues = IXGBE_VF_MAX_RX_QUEUES;
	ethdev->data->nb_tx_queues = IXGBE_VF_MAX_RX_QUEUES;

	/* Reference VF mac address from PF data structure */
	vf_data = *IXGBE_DEV_PRIVATE_TO_P_VFDATA(
		representor->pf_ethdev->data->dev_private);

	ethdev->data->mac_addrs = (struct ether_addr *)
		vf_data[representor->vf_id].vf_mac_addresses;

	/* Link state. Inherited from PF */
	link = &representor->pf_ethdev->data->dev_link;

	ethdev->data->dev_link.link_speed = link->link_speed;
	ethdev->data->dev_link.link_duplex = link->link_duplex;
	ethdev->data->dev_link.link_status = link->link_status;
	ethdev->data->dev_link.link_autoneg = link->link_autoneg;

	return 0;
}

int
ixgbe_vf_representor_uninit(struct rte_eth_dev *ethdev __rte_unused)
{
	return 0;
}
