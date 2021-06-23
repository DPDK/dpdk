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
cnxk_nix_rx_burst_mode_get(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			   struct rte_eth_burst_mode *mode)
{
	ssize_t bytes = 0, str_size = RTE_ETH_BURST_MODE_INFO_SIZE, rc;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct burst_info {
		uint64_t flags;
		const char *output;
	} rx_offload_map[] = {
		{DEV_RX_OFFLOAD_VLAN_STRIP, " VLAN Strip,"},
		{DEV_RX_OFFLOAD_IPV4_CKSUM, " Inner IPv4 Checksum,"},
		{DEV_RX_OFFLOAD_UDP_CKSUM, " UDP Checksum,"},
		{DEV_RX_OFFLOAD_TCP_CKSUM, " TCP Checksum,"},
		{DEV_RX_OFFLOAD_TCP_LRO, " TCP LRO,"},
		{DEV_RX_OFFLOAD_QINQ_STRIP, " QinQ VLAN Strip,"},
		{DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPv4 Checksum,"},
		{DEV_RX_OFFLOAD_MACSEC_STRIP, " MACsec Strip,"},
		{DEV_RX_OFFLOAD_HEADER_SPLIT, " Header Split,"},
		{DEV_RX_OFFLOAD_VLAN_FILTER, " VLAN Filter,"},
		{DEV_RX_OFFLOAD_VLAN_EXTEND, " VLAN Extend,"},
		{DEV_RX_OFFLOAD_JUMBO_FRAME, " Jumbo Frame,"},
		{DEV_RX_OFFLOAD_SCATTER, " Scattered,"},
		{DEV_RX_OFFLOAD_TIMESTAMP, " Timestamp,"},
		{DEV_RX_OFFLOAD_SECURITY, " Security,"},
		{DEV_RX_OFFLOAD_KEEP_CRC, " Keep CRC,"},
		{DEV_RX_OFFLOAD_SCTP_CKSUM, " SCTP,"},
		{DEV_RX_OFFLOAD_OUTER_UDP_CKSUM, " Outer UDP Checksum,"},
		{DEV_RX_OFFLOAD_RSS_HASH, " RSS,"}
	};
	static const char *const burst_mode[] = {"Vector Neon, Rx Offloads:",
						 "Scalar, Rx Offloads:"
	};
	uint32_t i;

	PLT_SET_USED(queue_id);

	/* Update burst mode info */
	rc = rte_strscpy(mode->info + bytes, burst_mode[dev->scalar_ena],
			 str_size - bytes);
	if (rc < 0)
		goto done;

	bytes += rc;

	/* Update Rx offload info */
	for (i = 0; i < RTE_DIM(rx_offload_map); i++) {
		if (dev->rx_offloads & rx_offload_map[i].flags) {
			rc = rte_strscpy(mode->info + bytes,
					 rx_offload_map[i].output,
					 str_size - bytes);
			if (rc < 0)
				goto done;

			bytes += rc;
		}
	}

done:
	return 0;
}

int
cnxk_nix_tx_burst_mode_get(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			   struct rte_eth_burst_mode *mode)
{
	ssize_t bytes = 0, str_size = RTE_ETH_BURST_MODE_INFO_SIZE, rc;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct burst_info {
		uint64_t flags;
		const char *output;
	} tx_offload_map[] = {
		{DEV_TX_OFFLOAD_VLAN_INSERT, " VLAN Insert,"},
		{DEV_TX_OFFLOAD_IPV4_CKSUM, " Inner IPv4 Checksum,"},
		{DEV_TX_OFFLOAD_UDP_CKSUM, " UDP Checksum,"},
		{DEV_TX_OFFLOAD_TCP_CKSUM, " TCP Checksum,"},
		{DEV_TX_OFFLOAD_SCTP_CKSUM, " SCTP Checksum,"},
		{DEV_TX_OFFLOAD_TCP_TSO, " TCP TSO,"},
		{DEV_TX_OFFLOAD_UDP_TSO, " UDP TSO,"},
		{DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPv4 Checksum,"},
		{DEV_TX_OFFLOAD_QINQ_INSERT, " QinQ VLAN Insert,"},
		{DEV_TX_OFFLOAD_VXLAN_TNL_TSO, " VXLAN Tunnel TSO,"},
		{DEV_TX_OFFLOAD_GRE_TNL_TSO, " GRE Tunnel TSO,"},
		{DEV_TX_OFFLOAD_IPIP_TNL_TSO, " IP-in-IP Tunnel TSO,"},
		{DEV_TX_OFFLOAD_GENEVE_TNL_TSO, " Geneve Tunnel TSO,"},
		{DEV_TX_OFFLOAD_MACSEC_INSERT, " MACsec Insert,"},
		{DEV_TX_OFFLOAD_MT_LOCKFREE, " Multi Thread Lockless Tx,"},
		{DEV_TX_OFFLOAD_MULTI_SEGS, " Scattered,"},
		{DEV_TX_OFFLOAD_MBUF_FAST_FREE, " H/W MBUF Free,"},
		{DEV_TX_OFFLOAD_SECURITY, " Security,"},
		{DEV_TX_OFFLOAD_UDP_TNL_TSO, " UDP Tunnel TSO,"},
		{DEV_TX_OFFLOAD_IP_TNL_TSO, " IP Tunnel TSO,"},
		{DEV_TX_OFFLOAD_OUTER_UDP_CKSUM, " Outer UDP Checksum,"},
		{DEV_TX_OFFLOAD_SEND_ON_TIMESTAMP, " Timestamp,"}
	};
	static const char *const burst_mode[] = {"Vector Neon, Tx Offloads:",
						 "Scalar, Tx Offloads:"
	};
	uint32_t i;

	PLT_SET_USED(queue_id);

	/* Update burst mode info */
	rc = rte_strscpy(mode->info + bytes, burst_mode[dev->scalar_ena],
			 str_size - bytes);
	if (rc < 0)
		goto done;

	bytes += rc;

	/* Update Tx offload info */
	for (i = 0; i < RTE_DIM(tx_offload_map); i++) {
		if (dev->tx_offloads & tx_offload_map[i].flags) {
			rc = rte_strscpy(mode->info + bytes,
					 tx_offload_map[i].output,
					 str_size - bytes);
			if (rc < 0)
				goto done;

			bytes += rc;
		}
	}

done:
	return 0;
}

int
cnxk_nix_flow_ctrl_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_fc_conf *fc_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	enum rte_eth_fc_mode mode_map[] = {
					   RTE_FC_NONE, RTE_FC_RX_PAUSE,
					   RTE_FC_TX_PAUSE, RTE_FC_FULL
					  };
	struct roc_nix *nix = &dev->nix;
	int mode;

	mode = roc_nix_fc_mode_get(nix);
	if (mode < 0)
		return mode;

	memset(fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	fc_conf->mode = mode_map[mode];
	return 0;
}

static int
nix_fc_cq_config_set(struct cnxk_eth_dev *dev, uint16_t qid, bool enable)
{
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_fc_cfg fc_cfg;
	struct roc_nix_cq *cq;

	memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
	cq = &dev->cqs[qid];
	fc_cfg.cq_cfg_valid = true;
	fc_cfg.cq_cfg.enable = enable;
	fc_cfg.cq_cfg.rq = qid;
	fc_cfg.cq_cfg.cq_drop = cq->drop_thresh;

	return roc_nix_fc_config_set(nix, &fc_cfg);
}

int
cnxk_nix_flow_ctrl_set(struct rte_eth_dev *eth_dev,
		       struct rte_eth_fc_conf *fc_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	enum roc_nix_fc_mode mode_map[] = {
					   ROC_NIX_FC_NONE, ROC_NIX_FC_RX,
					   ROC_NIX_FC_TX, ROC_NIX_FC_FULL
					  };
	struct rte_eth_dev_data *data = eth_dev->data;
	struct cnxk_fc_cfg *fc = &dev->fc_cfg;
	struct roc_nix *nix = &dev->nix;
	uint8_t rx_pause, tx_pause;
	int rc, i;

	if (roc_nix_is_vf_or_sdp(nix)) {
		plt_err("Flow control configuration is not allowed on VFs");
		return -ENOTSUP;
	}

	if (fc_conf->high_water || fc_conf->low_water || fc_conf->pause_time ||
	    fc_conf->mac_ctrl_frame_fwd || fc_conf->autoneg) {
		plt_info("Only MODE configuration is supported");
		return -EINVAL;
	}

	if (fc_conf->mode == fc->mode)
		return 0;

	rx_pause = (fc_conf->mode == RTE_FC_FULL) ||
		    (fc_conf->mode == RTE_FC_RX_PAUSE);
	tx_pause = (fc_conf->mode == RTE_FC_FULL) ||
		    (fc_conf->mode == RTE_FC_TX_PAUSE);

	/* Check if TX pause frame is already enabled or not */
	if (fc->tx_pause ^ tx_pause) {
		if (roc_model_is_cn96_ax() && data->dev_started) {
			/* On Ax, CQ should be in disabled state
			 * while setting flow control configuration.
			 */
			plt_info("Stop the port=%d for setting flow control",
				 data->port_id);
			return 0;
		}

		for (i = 0; i < data->nb_rx_queues; i++) {
			rc = nix_fc_cq_config_set(dev, i, tx_pause);
			if (rc)
				return rc;
		}
	}

	rc = roc_nix_fc_mode_set(nix, mode_map[fc_conf->mode]);
	if (rc)
		return rc;

	fc->rx_pause = rx_pause;
	fc->tx_pause = tx_pause;
	fc->mode = fc_conf->mode;

	return rc;
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
cnxk_nix_mac_addr_add(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr,
		      uint32_t index, uint32_t pool)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	PLT_SET_USED(index);
	PLT_SET_USED(pool);

	rc = roc_nix_mac_addr_add(nix, addr->addr_bytes);
	if (rc < 0) {
		plt_err("Failed to add mac address, rc=%d", rc);
		return rc;
	}

	/* Enable promiscuous mode at NIX level */
	roc_nix_npc_promisc_ena_dis(nix, true);
	dev->dmac_filter_enable = true;
	eth_dev->data->promiscuous = false;

	return 0;
}

void
cnxk_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_mac_addr_del(nix, index);
	if (rc)
		plt_err("Failed to delete mac address, rc=%d", rc);
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

int
cnxk_nix_promisc_enable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc = 0;

	if (roc_nix_is_vf_or_sdp(nix))
		return rc;

	rc = roc_nix_npc_promisc_ena_dis(nix, true);
	if (rc) {
		plt_err("Failed to setup promisc mode in npc, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		return rc;
	}

	rc = roc_nix_mac_promisc_mode_enable(nix, true);
	if (rc) {
		plt_err("Failed to setup promisc mode in mac, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		roc_nix_npc_promisc_ena_dis(nix, false);
		return rc;
	}

	return 0;
}

int
cnxk_nix_promisc_disable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc = 0;

	if (roc_nix_is_vf_or_sdp(nix))
		return rc;

	rc = roc_nix_npc_promisc_ena_dis(nix, dev->dmac_filter_enable);
	if (rc) {
		plt_err("Failed to setup promisc mode in npc, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		return rc;
	}

	rc = roc_nix_mac_promisc_mode_enable(nix, false);
	if (rc) {
		plt_err("Failed to setup promisc mode in mac, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		roc_nix_npc_promisc_ena_dis(nix, !dev->dmac_filter_enable);
		return rc;
	}

	dev->dmac_filter_enable = false;
	return 0;
}

int
cnxk_nix_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	return roc_nix_npc_mcast_config(&dev->nix, true, false);
}

int
cnxk_nix_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	return roc_nix_npc_mcast_config(&dev->nix, false,
					eth_dev->data->promiscuous);
}
