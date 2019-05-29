/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"

static void
nix_cgx_promisc_config(struct rte_eth_dev *eth_dev, int en)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;

	if (otx2_dev_is_vf(dev))
		return;

	if (en)
		otx2_mbox_alloc_msg_cgx_promisc_enable(mbox);
	else
		otx2_mbox_alloc_msg_cgx_promisc_disable(mbox);

	otx2_mbox_process(mbox);
}

void
otx2_nix_promisc_config(struct rte_eth_dev *eth_dev, int en)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_rx_mode *req;

	if (otx2_dev_is_vf(dev))
		return;

	req = otx2_mbox_alloc_msg_nix_set_rx_mode(mbox);

	if (en)
		req->mode = NIX_RX_MODE_UCAST | NIX_RX_MODE_PROMISC;

	otx2_mbox_process(mbox);
	eth_dev->data->promiscuous = en;
}

void
otx2_nix_promisc_enable(struct rte_eth_dev *eth_dev)
{
	otx2_nix_promisc_config(eth_dev, 1);
	nix_cgx_promisc_config(eth_dev, 1);
}

void
otx2_nix_promisc_disable(struct rte_eth_dev *eth_dev)
{
	otx2_nix_promisc_config(eth_dev, 0);
	nix_cgx_promisc_config(eth_dev, 0);
}

static void
nix_allmulticast_config(struct rte_eth_dev *eth_dev, int en)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_rx_mode *req;

	if (otx2_dev_is_vf(dev))
		return;

	req = otx2_mbox_alloc_msg_nix_set_rx_mode(mbox);

	if (en)
		req->mode = NIX_RX_MODE_UCAST | NIX_RX_MODE_ALLMULTI;
	else if (eth_dev->data->promiscuous)
		req->mode = NIX_RX_MODE_UCAST | NIX_RX_MODE_PROMISC;

	otx2_mbox_process(mbox);
}

void
otx2_nix_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	nix_allmulticast_config(eth_dev, 1);
}

void
otx2_nix_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	nix_allmulticast_config(eth_dev, 0);
}

void
otx2_nix_info_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *devinfo)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	devinfo->min_rx_bufsize = NIX_MIN_FRS;
	devinfo->max_rx_pktlen = NIX_MAX_FRS;
	devinfo->max_rx_queues = RTE_MAX_QUEUES_PER_PORT;
	devinfo->max_tx_queues = RTE_MAX_QUEUES_PER_PORT;
	devinfo->max_mac_addrs = dev->max_mac_entries;
	devinfo->max_vfs = pci_dev->max_vfs;
	devinfo->max_mtu = devinfo->max_rx_pktlen - NIX_L2_OVERHEAD;
	devinfo->min_mtu = devinfo->min_rx_bufsize - NIX_L2_OVERHEAD;

	devinfo->rx_offload_capa = dev->rx_offload_capa;
	devinfo->tx_offload_capa = dev->tx_offload_capa;
	devinfo->rx_queue_offload_capa = 0;
	devinfo->tx_queue_offload_capa = 0;

	devinfo->reta_size = dev->rss_info.rss_size;
	devinfo->hash_key_size = NIX_HASH_KEY_SIZE;
	devinfo->flow_type_rss_offloads = NIX_RSS_OFFLOAD;

	devinfo->default_rxconf = (struct rte_eth_rxconf) {
		.rx_drop_en = 0,
		.offloads = 0,
	};

	devinfo->default_txconf = (struct rte_eth_txconf) {
		.offloads = 0,
	};

	devinfo->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = UINT16_MAX,
		.nb_min = NIX_RX_MIN_DESC,
		.nb_align = NIX_RX_MIN_DESC_ALIGN,
		.nb_seg_max = NIX_RX_NB_SEG_MAX,
		.nb_mtu_seg_max = NIX_RX_NB_SEG_MAX,
	};
	devinfo->rx_desc_lim.nb_max =
		RTE_ALIGN_MUL_FLOOR(devinfo->rx_desc_lim.nb_max,
				    NIX_RX_MIN_DESC_ALIGN);

	devinfo->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = UINT16_MAX,
		.nb_min = 1,
		.nb_align = 1,
		.nb_seg_max = NIX_TX_NB_SEG_MAX,
		.nb_mtu_seg_max = NIX_TX_NB_SEG_MAX,
	};

	/* Auto negotiation disabled */
	devinfo->speed_capa = ETH_LINK_SPEED_FIXED;
	devinfo->speed_capa |= ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G |
				ETH_LINK_SPEED_25G | ETH_LINK_SPEED_40G |
				ETH_LINK_SPEED_50G | ETH_LINK_SPEED_100G;

	devinfo->dev_capa = RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
				RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;
}
