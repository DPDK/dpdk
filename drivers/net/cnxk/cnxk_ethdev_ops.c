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
			    RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP |
			    RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;

	devinfo->max_rx_mempools = CNXK_NIX_NUM_POOLS_MAX;

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
		{RTE_ETH_RX_OFFLOAD_VLAN_STRIP, " VLAN Strip,"},
		{RTE_ETH_RX_OFFLOAD_IPV4_CKSUM, " Inner IPv4 Checksum,"},
		{RTE_ETH_RX_OFFLOAD_UDP_CKSUM, " UDP Checksum,"},
		{RTE_ETH_RX_OFFLOAD_TCP_CKSUM, " TCP Checksum,"},
		{RTE_ETH_RX_OFFLOAD_TCP_LRO, " TCP LRO,"},
		{RTE_ETH_RX_OFFLOAD_QINQ_STRIP, " QinQ VLAN Strip,"},
		{RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPv4 Checksum,"},
		{RTE_ETH_RX_OFFLOAD_MACSEC_STRIP, " MACsec Strip,"},
		{RTE_ETH_RX_OFFLOAD_VLAN_FILTER, " VLAN Filter,"},
		{RTE_ETH_RX_OFFLOAD_VLAN_EXTEND, " VLAN Extend,"},
		{RTE_ETH_RX_OFFLOAD_SCATTER, " Scattered,"},
		{RTE_ETH_RX_OFFLOAD_TIMESTAMP, " Timestamp,"},
		{RTE_ETH_RX_OFFLOAD_SECURITY, " Security,"},
		{RTE_ETH_RX_OFFLOAD_KEEP_CRC, " Keep CRC,"},
		{RTE_ETH_RX_OFFLOAD_SCTP_CKSUM, " SCTP,"},
		{RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM, " Outer UDP Checksum,"},
		{RTE_ETH_RX_OFFLOAD_RSS_HASH, " RSS,"}
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
		{RTE_ETH_TX_OFFLOAD_VLAN_INSERT, " VLAN Insert,"},
		{RTE_ETH_TX_OFFLOAD_IPV4_CKSUM, " Inner IPv4 Checksum,"},
		{RTE_ETH_TX_OFFLOAD_UDP_CKSUM, " UDP Checksum,"},
		{RTE_ETH_TX_OFFLOAD_TCP_CKSUM, " TCP Checksum,"},
		{RTE_ETH_TX_OFFLOAD_SCTP_CKSUM, " SCTP Checksum,"},
		{RTE_ETH_TX_OFFLOAD_TCP_TSO, " TCP TSO,"},
		{RTE_ETH_TX_OFFLOAD_UDP_TSO, " UDP TSO,"},
		{RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPv4 Checksum,"},
		{RTE_ETH_TX_OFFLOAD_QINQ_INSERT, " QinQ VLAN Insert,"},
		{RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO, " VXLAN Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO, " GRE Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO, " IP-in-IP Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO, " Geneve Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_MACSEC_INSERT, " MACsec Insert,"},
		{RTE_ETH_TX_OFFLOAD_MT_LOCKFREE, " Multi Thread Lockless Tx,"},
		{RTE_ETH_TX_OFFLOAD_MULTI_SEGS, " Scattered,"},
		{RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE, " H/W MBUF Free,"},
		{RTE_ETH_TX_OFFLOAD_SECURITY, " Security,"},
		{RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO, " UDP Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_IP_TNL_TSO, " IP Tunnel TSO,"},
		{RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM, " Outer UDP Checksum,"},
		{RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP, " Timestamp,"}
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
	enum rte_eth_fc_mode mode_map[2][2] = {
		[0][0] = RTE_ETH_FC_NONE,
		[0][1] = RTE_ETH_FC_TX_PAUSE,
		[1][0] = RTE_ETH_FC_RX_PAUSE,
		[1][1] = RTE_ETH_FC_FULL,
	};
	struct roc_nix *nix = &dev->nix;
	uint8_t rx_pause, tx_pause;
	int mode, i;

	if (roc_nix_is_sdp(nix))
		return 0;

	mode = roc_nix_fc_mode_get(nix);
	if (mode < 0)
		return mode;

	rx_pause = (mode == ROC_NIX_FC_FULL) || (mode == ROC_NIX_FC_RX);
	tx_pause = (mode == ROC_NIX_FC_FULL) || (mode == ROC_NIX_FC_TX);

	/* Report flow control as disabled even if one RQ/SQ has it disabled */
	for (i = 0; i < dev->nb_rxq; i++) {
		if (dev->rqs[i].tc == ROC_NIX_PFC_CLASS_INVALID)
			tx_pause = 0;
	}

	for (i = 0; i < dev->nb_txq; i++) {
		if (dev->sqs[i].tc == ROC_NIX_PFC_CLASS_INVALID)
			rx_pause = 0;
	}

	memset(fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	fc_conf->mode = mode_map[rx_pause][tx_pause];
	return 0;
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
	struct cnxk_eth_rxq_sp *rxq;
	struct cnxk_eth_txq_sp *txq;
	uint8_t rx_pause, tx_pause;
	struct roc_nix_sq *sq;
	struct roc_nix_cq *cq;
	struct roc_nix_rq *rq;
	uint8_t tc;
	int rc, i;

	if (roc_nix_is_sdp(nix))
		return 0;

	if (dev->pfc_cfg.rx_pause_en || dev->pfc_cfg.tx_pause_en) {
		plt_err("Disable PFC before configuring Flow Control");
		return -ENOTSUP;
	}

	if (fc_conf->high_water || fc_conf->low_water || fc_conf->pause_time ||
	    fc_conf->mac_ctrl_frame_fwd || fc_conf->autoneg) {
		plt_info("Only MODE configuration is supported");
		return -EINVAL;
	}

	/* Disallow flow control changes when device is in started state */
	if (data->dev_started) {
		plt_info("Stop the port=%d for setting flow control", data->port_id);
		return -EBUSY;
	}

	rx_pause = (fc_conf->mode == RTE_ETH_FC_FULL) || (fc_conf->mode == RTE_ETH_FC_RX_PAUSE);
	tx_pause = (fc_conf->mode == RTE_ETH_FC_FULL) || (fc_conf->mode == RTE_ETH_FC_TX_PAUSE);

	/* Check if TX pause frame is already enabled or not */
	tc = tx_pause ? 0 : ROC_NIX_PFC_CLASS_INVALID;
	for (i = 0; i < data->nb_rx_queues; i++) {
		struct roc_nix_fc_cfg fc_cfg;

		/* Skip if RQ does not exist */
		if (!data->rx_queues[i])
			continue;

		rxq = cnxk_eth_rxq_to_sp(data->rx_queues[i]);
		rq = &dev->rqs[rxq->qid];
		cq = &dev->cqs[rxq->qid];

		/* Skip if RQ is in expected state */
		if (fc->tx_pause == tx_pause && rq->tc == tc)
			continue;

		memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
		fc_cfg.type = ROC_NIX_FC_RQ_CFG;
		fc_cfg.rq_cfg.enable = !!tx_pause;
		fc_cfg.rq_cfg.tc = 0;
		fc_cfg.rq_cfg.rq = rq->qid;
		fc_cfg.rq_cfg.pool = rq->aura_handle;
		fc_cfg.rq_cfg.spb_pool = rq->spb_aura_handle;
		fc_cfg.rq_cfg.cq_drop = cq->drop_thresh;
		fc_cfg.rq_cfg.pool_drop_pct = ROC_NIX_AURA_THRESH;

		rc = roc_nix_fc_config_set(nix, &fc_cfg);
		if (rc)
			return rc;
		rxq->tx_pause = !!tx_pause;
	}

	/* Check if RX pause frame is enabled or not */
	tc = rx_pause ? 0 : ROC_NIX_PFC_CLASS_INVALID;
	for (i = 0; i < data->nb_tx_queues; i++) {
		struct roc_nix_fc_cfg fc_cfg;

		/* Skip if SQ does not exist */
		if (!data->tx_queues[i])
			continue;

		txq = cnxk_eth_txq_to_sp(data->tx_queues[i]);
		sq = &dev->sqs[txq->qid];

		/* Skip if SQ is in expected state */
		if (fc->rx_pause == rx_pause && sq->tc == tc)
			continue;

		memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
		fc_cfg.type = ROC_NIX_FC_TM_CFG;
		fc_cfg.tm_cfg.sq = txq->qid;
		fc_cfg.tm_cfg.tc = 0;
		fc_cfg.tm_cfg.enable = !!rx_pause;
		rc = roc_nix_fc_config_set(nix, &fc_cfg);
		if (rc && rc != EEXIST)
			return rc;
	}

	/* Skip mode set if it is we are in same state */
	if (fc->rx_pause == rx_pause && fc->tx_pause == tx_pause)
		return 0;

	rc = roc_nix_fc_mode_set(nix, mode_map[fc_conf->mode]);
	if (rc)
		return rc;

	fc->rx_pause = rx_pause;
	fc->tx_pause = tx_pause;
	fc->mode = fc_conf->mode;
	return rc;
}

int
cnxk_nix_priority_flow_ctrl_queue_info_get(struct rte_eth_dev *eth_dev,
				struct rte_eth_pfc_queue_info *pfc_info)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	pfc_info->tc_max = roc_nix_chan_count_get(&dev->nix);
	pfc_info->mode_capa = RTE_ETH_FC_FULL;
	return 0;
}

int
cnxk_nix_priority_flow_ctrl_queue_config(struct rte_eth_dev *eth_dev,
				 struct rte_eth_pfc_queue_conf *pfc_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix *nix = &dev->nix;
	enum rte_eth_fc_mode mode;
	uint8_t en, tc;
	uint16_t qid;
	int rc = 0;

	if (dev->fc_cfg.mode != RTE_ETH_FC_NONE) {
		plt_err("Disable Flow Control before configuring PFC");
		return -ENOTSUP;
	}

	if (roc_nix_is_sdp(nix) || roc_nix_is_lbk(nix)) {
		plt_nix_dbg("Prio flow ctrl config is not allowed on SDP/LBK");
		return -ENOTSUP;
	}

	/* Disallow flow control changes when device is in started state */
	if (data->dev_started) {
		plt_info("Stop the port=%d for setting PFC", data->port_id);
		return -EBUSY;
	}

	mode = pfc_conf->mode;

	/* Perform Tx pause configuration on RQ */
	qid = pfc_conf->tx_pause.rx_qid;
	if (qid < eth_dev->data->nb_rx_queues) {
		en = (mode == RTE_ETH_FC_FULL) || (mode == RTE_ETH_FC_TX_PAUSE);
		tc = pfc_conf->tx_pause.tc;
		rc = nix_priority_flow_ctrl_rq_conf(eth_dev, qid, en, tc);
	}

	/* Perform Rx pause configuration on SQ */
	qid = pfc_conf->rx_pause.tx_qid;
	if (qid < eth_dev->data->nb_tx_queues) {
		en = (mode == RTE_ETH_FC_FULL) || (mode == RTE_ETH_FC_RX_PAUSE);
		tc = pfc_conf->rx_pause.tc;
		rc |= nix_priority_flow_ctrl_sq_conf(eth_dev, qid, en, tc);
	}

	return rc;
}

int
cnxk_nix_flow_ops_get(struct rte_eth_dev *eth_dev,
		      const struct rte_flow_ops **ops)
{
	RTE_SET_USED(eth_dev);

	*ops = &cnxk_flow_ops;
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

	dev->dmac_idx_map[index] = rc;

	/* Enable promiscuous mode at NIX level */
	roc_nix_npc_promisc_ena_dis(nix, true);
	dev->dmac_filter_enable = true;
	eth_dev->data->promiscuous = false;
	dev->dmac_filter_count++;

	return 0;
}

void
cnxk_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_mac_addr_del(nix, dev->dmac_idx_map[index]);
	if (rc)
		plt_err("Failed to delete mac address, rc=%d", rc);

	dev->dmac_filter_count--;
}

int
cnxk_nix_sq_flush(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	int i, rc = 0;

	/* Flush all tx queues */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		struct roc_nix_sq *sq = &dev->sqs[i];

		if (eth_dev->data->tx_queues[i] == NULL)
			continue;

		rc = roc_nix_tm_sq_aura_fc(sq, false);
		if (rc) {
			plt_err("Failed to disable sqb aura fc, rc=%d", rc);
			goto exit;
		}

		/* Wait for sq entries to be flushed */
		rc = roc_nix_tm_sq_flush_spin(sq);
		if (rc) {
			plt_err("Failed to drain sq, rc=%d\n", rc);
			goto exit;
		}
		if (data->tx_queue_state[i] == RTE_ETH_QUEUE_STATE_STARTED) {
			rc = roc_nix_tm_sq_aura_fc(sq, true);
			if (rc) {
				plt_err("Failed to enable sq aura fc, txq=%u, rc=%d", i, rc);
				goto exit;
			}
		}
	}
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
	struct cnxk_eth_rxq_sp *rxq_sp;
	uint32_t buffsz = 0;
	int rc = -EINVAL;

	frame_size += CNXK_NIX_TIMESYNC_RX_OFFSET * dev->ptp_en;

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

	if (!eth_dev->data->nb_rx_queues)
		goto skip_buffsz_check;

	/* Perform buff size check */
	if (data->min_rx_buf_size) {
		buffsz = data->min_rx_buf_size;
	} else if (eth_dev->data->rx_queues && eth_dev->data->rx_queues[0]) {
		rxq_sp = cnxk_eth_rxq_to_sp(data->rx_queues[0]);

		if (rxq_sp->qconf.mp)
			buffsz = rte_pktmbuf_data_room_size(rxq_sp->qconf.mp);
	}

	/* Skip validation if RQ's are not yet setup */
	if (!buffsz)
		goto skip_buffsz_check;

	buffsz -= RTE_PKTMBUF_HEADROOM;

	/* Refuse MTU that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (data->dev_started && frame_size > buffsz &&
	    !(dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)) {
		plt_err("Scatter offload is not enabled for mtu");
		goto exit;
	}

	/* Check <seg size> * <max_seg>  >= max_frame */
	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)	&&
	    frame_size > (buffsz * CNXK_NIX_RX_NB_SEG_MAX)) {
		plt_err("Greater than maximum supported packet length");
		goto exit;
	}

skip_buffsz_check:
	old_frame_size = data->mtu + CNXK_NIX_L2_OVERHEAD;
	/* if new MTU was smaller than old one, then flush all SQs before MTU change */
	if (old_frame_size > frame_size) {
		if (data->dev_started) {
			plt_err("Reducing MTU is not supported when device started");
			goto exit;
		}
		cnxk_nix_sq_flush(eth_dev);
	}

	frame_size -= RTE_ETHER_CRC_LEN;

	/* Set frame size on Rx */
	rc = roc_nix_mac_max_rx_len_set(nix, frame_size);
	if (rc) {
		plt_err("Failed to max Rx frame length, rc=%d", rc);
		goto exit;
	}
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

	return roc_nix_npc_mcast_config(&dev->nix, true,
					eth_dev->data->promiscuous);
}

int
cnxk_nix_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	return roc_nix_npc_mcast_config(&dev->nix, false,
					eth_dev->data->promiscuous);
}

int
cnxk_nix_set_link_up(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc, i;

	if (roc_nix_is_vf_or_sdp(nix))
		return -ENOTSUP;

	rc = roc_nix_mac_link_state_set(nix, true);
	if (rc)
		goto exit;

	/* Start tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = cnxk_nix_tx_queue_start(eth_dev, i);
		if (rc)
			goto exit;
	}

exit:
	return rc;
}

int
cnxk_nix_set_link_down(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc, i;

	if (roc_nix_is_vf_or_sdp(nix))
		return -ENOTSUP;

	/* Stop tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = cnxk_nix_tx_queue_stop(eth_dev, i);
		if (rc)
			goto exit;
	}

	rc = roc_nix_mac_link_state_set(nix, false);
exit:
	return rc;
}

int
cnxk_nix_get_module_info(struct rte_eth_dev *eth_dev,
			 struct rte_eth_dev_module_info *modinfo)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_eeprom_info eeprom_info = {0};
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_eeprom_info_get(nix, &eeprom_info);
	if (rc)
		return rc;

	modinfo->type = eeprom_info.sff_id;
	modinfo->eeprom_len = ROC_NIX_EEPROM_SIZE;
	return 0;
}

int
cnxk_nix_get_module_eeprom(struct rte_eth_dev *eth_dev,
			   struct rte_dev_eeprom_info *info)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_eeprom_info eeprom_info = {0};
	struct roc_nix *nix = &dev->nix;
	int rc = -EINVAL;

	if (!info->data || !info->length ||
	    (info->offset + info->length > ROC_NIX_EEPROM_SIZE))
		return rc;

	rc = roc_nix_eeprom_info_get(nix, &eeprom_info);
	if (rc)
		return rc;

	rte_memcpy(info->data, eeprom_info.buf + info->offset, info->length);
	return 0;
}

int
cnxk_nix_rx_queue_intr_enable(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	roc_nix_rx_queue_intr_enable(&dev->nix, rx_queue_id);
	return 0;
}

int
cnxk_nix_rx_queue_intr_disable(struct rte_eth_dev *eth_dev,
			       uint16_t rx_queue_id)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	roc_nix_rx_queue_intr_disable(&dev->nix, rx_queue_id);
	return 0;
}

int
cnxk_nix_pool_ops_supported(struct rte_eth_dev *eth_dev, const char *pool)
{
	RTE_SET_USED(eth_dev);

	if (!strcmp(pool, rte_mbuf_platform_mempool_ops()))
		return 0;

	return -ENOTSUP;
}

int
cnxk_nix_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version,
			size_t fw_size)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const char *str = roc_npc_profile_name_get(&dev->npc);
	uint32_t size = strlen(str) + 1;

	if (fw_size > size)
		fw_size = size;

	rte_strlcpy(fw_version, str, fw_size);

	if (fw_size < size)
		return size;

	return 0;
}

void
cnxk_nix_rxq_info_get(struct rte_eth_dev *eth_dev, uint16_t qid,
		      struct rte_eth_rxq_info *qinfo)
{
	void *rxq = eth_dev->data->rx_queues[qid];
	struct cnxk_eth_rxq_sp *rxq_sp = cnxk_eth_rxq_to_sp(rxq);

	memset(qinfo, 0, sizeof(*qinfo));

	qinfo->mp = rxq_sp->qconf.mp;
	qinfo->scattered_rx = eth_dev->data->scattered_rx;
	qinfo->nb_desc = rxq_sp->qconf.nb_desc;

	memcpy(&qinfo->conf, &rxq_sp->qconf.conf.rx, sizeof(qinfo->conf));
}

void
cnxk_nix_txq_info_get(struct rte_eth_dev *eth_dev, uint16_t qid,
		      struct rte_eth_txq_info *qinfo)
{
	void *txq = eth_dev->data->tx_queues[qid];
	struct cnxk_eth_txq_sp *txq_sp = cnxk_eth_txq_to_sp(txq);

	memset(qinfo, 0, sizeof(*qinfo));

	qinfo->nb_desc = txq_sp->qconf.nb_desc;

	memcpy(&qinfo->conf, &txq_sp->qconf.conf.tx, sizeof(qinfo->conf));
}

uint32_t
cnxk_nix_rx_queue_count(void *rxq)
{
	struct cnxk_eth_rxq_sp *rxq_sp = cnxk_eth_rxq_to_sp(rxq);
	struct roc_nix *nix = &rxq_sp->dev->nix;
	uint32_t head, tail;

	roc_nix_cq_head_tail_get(nix, rxq_sp->qid, &head, &tail);
	return (tail - head) % (rxq_sp->qconf.nb_desc);
}

static inline int
nix_offset_has_packet(uint32_t head, uint32_t tail, uint16_t offset, bool is_rx)
{
	/* Check given offset(queue index) has packet filled/xmit by HW
	 * in case of Rx or Tx.
	 * Also, checks for wrap around case.
	 */
	return ((tail > head && offset <= tail && offset >= head) ||
		(head > tail && (offset >= head || offset <= tail))) ?
		       is_rx :
		       !is_rx;
}

int
cnxk_nix_rx_descriptor_status(void *rxq, uint16_t offset)
{
	struct cnxk_eth_rxq_sp *rxq_sp = cnxk_eth_rxq_to_sp(rxq);
	struct roc_nix *nix = &rxq_sp->dev->nix;
	uint32_t head, tail;

	if (rxq_sp->qconf.nb_desc <= offset)
		return -EINVAL;

	roc_nix_cq_head_tail_get(nix, rxq_sp->qid, &head, &tail);

	if (nix_offset_has_packet(head, tail, offset, 1))
		return RTE_ETH_RX_DESC_DONE;
	else
		return RTE_ETH_RX_DESC_AVAIL;
}

int
cnxk_nix_tx_descriptor_status(void *txq, uint16_t offset)
{
	struct cnxk_eth_txq_sp *txq_sp = cnxk_eth_txq_to_sp(txq);
	struct roc_nix *nix = &txq_sp->dev->nix;
	uint32_t head = 0, tail = 0;

	if (txq_sp->qconf.nb_desc <= offset)
		return -EINVAL;

	roc_nix_sq_head_tail_get(nix, txq_sp->qid, &head, &tail);

	if (nix_offset_has_packet(head, tail, offset, 0))
		return RTE_ETH_TX_DESC_DONE;
	else
		return RTE_ETH_TX_DESC_FULL;
}

/* It is a NOP for cnxk as HW frees the buffer on xmit */
int
cnxk_nix_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	RTE_SET_USED(txq);
	RTE_SET_USED(free_cnt);

	return 0;
}

int
cnxk_nix_dev_get_reg(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint64_t *data = regs->data;
	int rc = -ENOTSUP;

	if (data == NULL) {
		rc = roc_nix_lf_get_reg_count(nix);
		if (rc > 0) {
			regs->length = rc;
			regs->width = 8;
			rc = 0;
		}
		return rc;
	}

	if (!regs->length ||
	    regs->length == (uint32_t)roc_nix_lf_get_reg_count(nix))
		return roc_nix_lf_reg_dump(nix, data);

	return rc;
}

int
cnxk_nix_reta_update(struct rte_eth_dev *eth_dev,
		     struct rte_eth_rss_reta_entry64 *reta_conf,
		     uint16_t reta_size)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint16_t reta[ROC_NIX_RSS_RETA_MAX];
	struct roc_nix *nix = &dev->nix;
	int i, j, rc = -EINVAL, idx = 0;

	if (reta_size != dev->nix.reta_sz) {
		plt_err("Size of hash lookup table configured (%d) does not "
			"match the number hardware can supported (%d)",
			reta_size, dev->nix.reta_sz);
		goto fail;
	}

	roc_nix_rss_reta_get(nix, 0, reta);

	/* Copy RETA table */
	for (i = 0; i < (int)(dev->nix.reta_sz / RTE_ETH_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++) {
			if ((reta_conf[i].mask >> j) & 0x01)
				reta[idx] = reta_conf[i].reta[j];
			idx++;
		}
	}

	return roc_nix_rss_reta_set(nix, 0, reta);

fail:
	return rc;
}

int
cnxk_nix_reta_query(struct rte_eth_dev *eth_dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint16_t reta[ROC_NIX_RSS_RETA_MAX];
	struct roc_nix *nix = &dev->nix;
	int rc = -EINVAL, i, j, idx = 0;

	if (reta_size != dev->nix.reta_sz) {
		plt_err("Size of hash lookup table configured (%d) does not "
			"match the number hardware can supported (%d)",
			reta_size, dev->nix.reta_sz);
		goto fail;
	}

	rc = roc_nix_rss_reta_get(nix, 0, reta);
	if (rc)
		goto fail;

	/* Copy RETA table */
	for (i = 0; i < (int)(dev->nix.reta_sz / RTE_ETH_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++) {
			if ((reta_conf[i].mask >> j) & 0x01)
				reta_conf[i].reta[j] = reta[idx];
			idx++;
		}
	}

	return 0;

fail:
	return rc;
}

int
cnxk_nix_eth_dev_priv_dump(struct rte_eth_dev *eth_dev, FILE *file)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	int i;

	roc_nix_dump(roc_nix, file);

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		roc_nix_rq_dump(&dev->rqs[i], file);

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		roc_nix_cq_dump(&dev->cqs[i], file);

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		roc_nix_sq_dump(&dev->sqs[i], file);

	roc_nix_queues_ctx_dump(roc_nix, file);

	roc_nix_tm_dump(roc_nix, file);

	roc_nix_inl_dev_dump(NULL, file);

	roc_nix_inl_outb_cpt_lfs_dump(roc_nix, file);

	return 0;
}

int
cnxk_nix_rss_hash_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint8_t rss_hash_level;
	uint32_t flowkey_cfg;
	int rc = -EINVAL;
	uint8_t alg_idx;

	if (rss_conf->rss_key && rss_conf->rss_key_len != ROC_NIX_RSS_KEY_LEN) {
		plt_err("Hash key size mismatch %d vs %d",
			rss_conf->rss_key_len, ROC_NIX_RSS_KEY_LEN);
		goto fail;
	}

	if (rss_conf->rss_key)
		roc_nix_rss_key_set(nix, rss_conf->rss_key);

	rss_hash_level = RTE_ETH_RSS_LEVEL(rss_conf->rss_hf);
	if (rss_hash_level)
		rss_hash_level -= 1;
	flowkey_cfg =
		cnxk_rss_ethdev_to_nix(dev, rss_conf->rss_hf, rss_hash_level);

	rc = roc_nix_rss_flowkey_set(nix, &alg_idx, flowkey_cfg,
				     ROC_NIX_RSS_GROUP_DEFAULT,
				     ROC_NIX_RSS_MCAM_IDX_DEFAULT);
	if (rc) {
		plt_err("Failed to set RSS hash function rc=%d", rc);
		return rc;
	}

fail:
	return rc;
}

int
cnxk_nix_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	if (rss_conf->rss_key)
		roc_nix_rss_key_get(&dev->nix, rss_conf->rss_key);

	rss_conf->rss_key_len = ROC_NIX_RSS_KEY_LEN;
	rss_conf->rss_hf = dev->ethdev_rss_hf;

	return 0;
}

int
cnxk_nix_mc_addr_list_configure(struct rte_eth_dev *eth_dev,
				struct rte_ether_addr *mc_addr_set,
				uint32_t nb_mc_addr)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_ether_addr null_mac_addr;
	struct roc_nix *nix = &dev->nix;
	int rc, index;
	uint32_t i;

	memset(&null_mac_addr, 0, sizeof(null_mac_addr));

	/* All configured multicast filters should be flushed first */
	for (i = 0; i < dev->max_mac_entries; i++) {
		if (rte_is_multicast_ether_addr(&data->mac_addrs[i])) {
			rc = roc_nix_mac_addr_del(nix, i);
			if (rc) {
				plt_err("Failed to flush mcast address, rc=%d",
					rc);
				return rc;
			}

			dev->dmac_filter_count--;
			/* Update address in NIC data structure */
			rte_ether_addr_copy(&null_mac_addr,
					    &data->mac_addrs[i]);
		}
	}

	if (!mc_addr_set || !nb_mc_addr)
		return 0;

	/* Check for available space */
	if (nb_mc_addr >
	    ((uint32_t)(dev->max_mac_entries - dev->dmac_filter_count))) {
		plt_err("No space is available to add multicast filters");
		return -ENOSPC;
	}

	/* Multicast addresses are to be installed */
	for (i = 0; i < nb_mc_addr; i++) {
		index = roc_nix_mac_addr_add(nix, mc_addr_set[i].addr_bytes);
		if (index < 0) {
			plt_err("Failed to add mcast mac address, rc=%d",
				index);
			return index;
		}

		dev->dmac_filter_count++;
		/* Update address in NIC data structure */
		rte_ether_addr_copy(&mc_addr_set[i], &data->mac_addrs[index]);
	}

	roc_nix_npc_promisc_ena_dis(nix, true);
	dev->dmac_filter_enable = true;
	eth_dev->data->promiscuous = false;

	return 0;
}

int
nix_priority_flow_ctrl_rq_conf(struct rte_eth_dev *eth_dev, uint16_t qid,
			       uint8_t tx_pause, uint8_t tc)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct cnxk_pfc_cfg *pfc = &dev->pfc_cfg;
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_pfc_cfg pfc_cfg;
	struct roc_nix_fc_cfg fc_cfg;
	struct cnxk_eth_rxq_sp *rxq;
	enum roc_nix_fc_mode mode;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc, i;

	if (data->dev_started) {
		/* RQ should be in disabled state
		 * while setting flow control configuration.
		 */
		plt_info("Stop the port=%d for setting flow control",
			 data->port_id);
		return 0;
	}

	if (data->rx_queues == NULL)
		return -EINVAL;

	if (qid >= eth_dev->data->nb_rx_queues)
		return -ENOTSUP;

	/* Configure RQ */
	rxq = ((struct cnxk_eth_rxq_sp *)data->rx_queues[qid]) - 1;
	rq = &dev->rqs[qid];
	cq = &dev->cqs[qid];

	memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
	fc_cfg.type = ROC_NIX_FC_RQ_CFG;
	fc_cfg.rq_cfg.tc = tc;
	fc_cfg.rq_cfg.enable = !!tx_pause;
	fc_cfg.rq_cfg.rq = rq->qid;
	fc_cfg.rq_cfg.pool = rxq->qconf.mp->pool_id;
	fc_cfg.rq_cfg.spb_pool = rq->spb_aura_handle;
	fc_cfg.rq_cfg.cq_drop = cq->drop_thresh;
	fc_cfg.rq_cfg.pool_drop_pct = ROC_NIX_AURA_THRESH;
	rc = roc_nix_fc_config_set(nix, &fc_cfg);
	if (rc)
		return rc;

	rxq->tx_pause = !!tx_pause;
	rxq->tc = tc;
	/* Recheck number of RQ's that have PFC enabled */
	pfc->tx_pause_en = 0;
	for (i = 0; i < dev->nb_rxq; i++)
		if (dev->rqs[i].tc != ROC_NIX_PFC_CLASS_INVALID)
			pfc->tx_pause_en++;

	/* Skip if PFC already enabled in mac */
	if (pfc->tx_pause_en > 1)
		return 0;

	/* Configure MAC block */
	pfc->class_en = pfc->tx_pause_en ? 0xFF : 0x0;

	if (pfc->rx_pause_en)
		mode = pfc->tx_pause_en ? ROC_NIX_FC_FULL : ROC_NIX_FC_RX;
	else
		mode = pfc->tx_pause_en ? ROC_NIX_FC_TX : ROC_NIX_FC_NONE;

	memset(&pfc_cfg, 0, sizeof(struct roc_nix_pfc_cfg));
	pfc_cfg.mode = mode;
	pfc_cfg.tc = pfc->class_en;
	return roc_nix_pfc_mode_set(nix, &pfc_cfg);
}

int
nix_priority_flow_ctrl_sq_conf(struct rte_eth_dev *eth_dev, uint16_t qid,
			       uint8_t rx_pause, uint8_t tc)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct cnxk_pfc_cfg *pfc = &dev->pfc_cfg;
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_pfc_cfg pfc_cfg;
	struct roc_nix_fc_cfg fc_cfg;
	struct cnxk_eth_txq_sp *txq;
	enum roc_nix_fc_mode mode;
	struct roc_nix_sq *sq;
	int rc, i;

	if (data->tx_queues == NULL)
		return -EINVAL;

	if (qid >= eth_dev->data->nb_tx_queues)
		return -ENOTSUP;

	/* Check if RX pause frame is enabled or not and
	 * confirm user requested for PFC.
	 */
	if (!pfc->rx_pause_en && rx_pause) {
		if ((roc_nix_tm_tree_type_get(nix) == ROC_NIX_TM_DEFAULT) &&
		    eth_dev->data->nb_tx_queues > 1) {
			/*
			 * Disabled xmit will be enabled when
			 * new topology is available.
			 */
			rc = roc_nix_tm_hierarchy_disable(nix);
			if (rc)
				goto exit;

			rc = roc_nix_tm_pfc_prepare_tree(nix);
			if (rc)
				goto exit;

			rc = roc_nix_tm_hierarchy_enable(nix, ROC_NIX_TM_PFC,
							 true);
			if (rc)
				goto exit;
		}
	}

	txq = ((struct cnxk_eth_txq_sp *)data->tx_queues[qid]) - 1;
	sq = &dev->sqs[txq->qid];
	memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
	fc_cfg.type = ROC_NIX_FC_TM_CFG;
	fc_cfg.tm_cfg.sq = sq->qid;
	fc_cfg.tm_cfg.tc = tc;
	fc_cfg.tm_cfg.enable = !!rx_pause;
	rc = roc_nix_fc_config_set(nix, &fc_cfg);
	if (rc)
		return rc;

	/* Recheck number of SQ's that have PFC enabled */
	pfc->rx_pause_en = 0;
	for (i = 0; i < dev->nb_txq; i++)
		if (dev->sqs[i].tc != ROC_NIX_PFC_CLASS_INVALID)
			pfc->rx_pause_en++;

	if (pfc->rx_pause_en > 1)
		goto exit;

	if (pfc->tx_pause_en)
		mode = pfc->rx_pause_en ? ROC_NIX_FC_FULL : ROC_NIX_FC_TX;
	else
		mode = pfc->rx_pause_en ? ROC_NIX_FC_RX : ROC_NIX_FC_NONE;

	memset(&pfc_cfg, 0, sizeof(struct roc_nix_pfc_cfg));
	pfc_cfg.mode = mode;
	pfc_cfg.tc = pfc->class_en;
	rc = roc_nix_pfc_mode_set(nix, &pfc_cfg);
exit:
	return rc;
}
