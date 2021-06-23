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
	devinfo->max_mtu = devinfo->max_rx_pktlen -
				(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN);
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

int
cnxk_nix_mac_addr_set(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	/* Update mac address at NPC */
	rc = roc_nix_npc_mac_addr_set(nix, addr->addr_bytes);
	if (rc)
		goto exit;

	/* Update mac address at CGX for PFs only */
	if (!roc_nix_is_vf_or_sdp(nix)) {
		rc = roc_nix_mac_addr_set(nix, addr->addr_bytes);
		if (rc) {
			/* Rollback to previous mac address */
			roc_nix_npc_mac_addr_set(nix, dev->mac_addr);
			goto exit;
		}
	}

	/* Update mac address to cnxk ethernet device */
	rte_memcpy(dev->mac_addr, addr->addr_bytes, RTE_ETHER_ADDR_LEN);

exit:
	return rc;
}

int
cnxk_nix_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	uint32_t old_frame_size, frame_size = mtu + CNXK_NIX_L2_OVERHEAD;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix *nix = &dev->nix;
	int rc = -EINVAL;
	uint32_t buffsz;

	/* Check if MTU is within the allowed range */
	if ((frame_size - RTE_ETHER_CRC_LEN) < NIX_MIN_HW_FRS) {
		plt_err("MTU is lesser than minimum");
		goto exit;
	}

	if ((frame_size - RTE_ETHER_CRC_LEN) >
	    ((uint32_t)roc_nix_max_pkt_len(nix))) {
		plt_err("MTU is greater than maximum");
		goto exit;
	}

	buffsz = data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM;
	old_frame_size = data->mtu + CNXK_NIX_L2_OVERHEAD;

	/* Refuse MTU that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (data->dev_started && frame_size > buffsz &&
	    !(dev->rx_offloads & DEV_RX_OFFLOAD_SCATTER)) {
		plt_err("Scatter offload is not enabled for mtu");
		goto exit;
	}

	/* Check <seg size> * <max_seg>  >= max_frame */
	if ((dev->rx_offloads & DEV_RX_OFFLOAD_SCATTER)	&&
	    frame_size > (buffsz * CNXK_NIX_RX_NB_SEG_MAX)) {
		plt_err("Greater than maximum supported packet length");
		goto exit;
	}

	frame_size -= RTE_ETHER_CRC_LEN;

	/* Update mtu on Tx */
	rc = roc_nix_mac_mtu_set(nix, frame_size);
	if (rc) {
		plt_err("Failed to set MTU, rc=%d", rc);
		goto exit;
	}

	/* Sync same frame size on Rx */
	rc = roc_nix_mac_max_rx_len_set(nix, frame_size);
	if (rc) {
		/* Rollback to older mtu */
		roc_nix_mac_mtu_set(nix,
				    old_frame_size - RTE_ETHER_CRC_LEN);
		plt_err("Failed to max Rx frame length, rc=%d", rc);
		goto exit;
	}

	frame_size += RTE_ETHER_CRC_LEN;

	if (frame_size > RTE_ETHER_MAX_LEN)
		dev->rx_offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev->rx_offloads &= ~DEV_RX_OFFLOAD_JUMBO_FRAME;

	/* Update max_rx_pkt_len */
	data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

exit:
	return rc;
}
