/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cnxk_ethdev.h>

int
cnxk_nix_info_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *devinfo)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int max_rx_pktlen;

	max_rx_pktlen = (roc_nix_max_pkt_len(&dev->nix) + RTE_ETHER_CRC_LEN -
			 CNXK_NIX_MAX_VTAG_ACT_SIZE);

	devinfo->min_rx_bufsize = NIX_MIN_HW_FRS + RTE_ETHER_CRC_LEN;
	devinfo->max_rx_pktlen = max_rx_pktlen;
	devinfo->max_rx_queues = RTE_MAX_QUEUES_PER_PORT;
	devinfo->max_tx_queues = RTE_MAX_QUEUES_PER_PORT;
	devinfo->max_mac_addrs = dev->max_mac_entries;
	devinfo->max_vfs = pci_dev->max_vfs;
	devinfo->max_mtu = devinfo->max_rx_pktlen - CNXK_NIX_L2_OVERHEAD;
	devinfo->min_mtu = devinfo->min_rx_bufsize - CNXK_NIX_L2_OVERHEAD;

	devinfo->rx_offload_capa = dev->rx_offload_capa;
	devinfo->tx_offload_capa = dev->tx_offload_capa;
	devinfo->rx_queue_offload_capa = 0;
	devinfo->tx_queue_offload_capa = 0;

	devinfo->reta_size = dev->nix.reta_sz;
	devinfo->hash_key_size = ROC_NIX_RSS_KEY_LEN;
	devinfo->flow_type_rss_offloads = CNXK_NIX_RSS_OFFLOAD;

	devinfo->default_rxconf = (struct rte_eth_rxconf){
		.rx_drop_en = 0,
		.offloads = 0,
	};

	devinfo->default_txconf = (struct rte_eth_txconf){
		.offloads = 0,
	};

	devinfo->default_rxportconf = (struct rte_eth_dev_portconf){
		.ring_size = CNXK_NIX_RX_DEFAULT_RING_SZ,
	};

	devinfo->rx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = UINT16_MAX,
		.nb_min = CNXK_NIX_RX_MIN_DESC,
		.nb_align = CNXK_NIX_RX_MIN_DESC_ALIGN,
		.nb_seg_max = CNXK_NIX_RX_NB_SEG_MAX,
		.nb_mtu_seg_max = CNXK_NIX_RX_NB_SEG_MAX,
	};
	devinfo->rx_desc_lim.nb_max =
		RTE_ALIGN_MUL_FLOOR(devinfo->rx_desc_lim.nb_max,
				    CNXK_NIX_RX_MIN_DESC_ALIGN);

	devinfo->tx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = UINT16_MAX,
		.nb_min = 1,
		.nb_align = 1,
		.nb_seg_max = CNXK_NIX_TX_NB_SEG_MAX,
		.nb_mtu_seg_max = CNXK_NIX_TX_NB_SEG_MAX,
	};

	devinfo->speed_capa = dev->speed_capa;
	devinfo->dev_capa = RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
			    RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;
	return 0;
}
