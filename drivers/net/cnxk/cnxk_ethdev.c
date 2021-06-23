/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_ethdev.h>

static inline uint64_t
nix_get_rx_offload_capa(struct cnxk_eth_dev *dev)
{
	uint64_t capa = CNXK_NIX_RX_OFFLOAD_CAPA;

	if (roc_nix_is_vf_or_sdp(&dev->nix))
		capa &= ~DEV_RX_OFFLOAD_TIMESTAMP;

	return capa;
}

static inline uint64_t
nix_get_tx_offload_capa(struct cnxk_eth_dev *dev)
{
	RTE_SET_USED(dev);
	return CNXK_NIX_TX_OFFLOAD_CAPA;
}

static inline uint32_t
nix_get_speed_capa(struct cnxk_eth_dev *dev)
{
	uint32_t speed_capa;

	/* Auto negotiation disabled */
	speed_capa = ETH_LINK_SPEED_FIXED;
	if (!roc_nix_is_vf_or_sdp(&dev->nix) && !roc_nix_is_lbk(&dev->nix)) {
		speed_capa |= ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G |
			      ETH_LINK_SPEED_25G | ETH_LINK_SPEED_40G |
			      ETH_LINK_SPEED_50G | ETH_LINK_SPEED_100G;
	}

	return speed_capa;
}

uint32_t
cnxk_rss_ethdev_to_nix(struct cnxk_eth_dev *dev, uint64_t ethdev_rss,
		       uint8_t rss_level)
{
	uint32_t flow_key_type[RSS_MAX_LEVELS][6] = {
		{FLOW_KEY_TYPE_IPV4, FLOW_KEY_TYPE_IPV6, FLOW_KEY_TYPE_TCP,
		 FLOW_KEY_TYPE_UDP, FLOW_KEY_TYPE_SCTP, FLOW_KEY_TYPE_ETH_DMAC},
		{FLOW_KEY_TYPE_INNR_IPV4, FLOW_KEY_TYPE_INNR_IPV6,
		 FLOW_KEY_TYPE_INNR_TCP, FLOW_KEY_TYPE_INNR_UDP,
		 FLOW_KEY_TYPE_INNR_SCTP, FLOW_KEY_TYPE_INNR_ETH_DMAC},
		{FLOW_KEY_TYPE_IPV4 | FLOW_KEY_TYPE_INNR_IPV4,
		 FLOW_KEY_TYPE_IPV6 | FLOW_KEY_TYPE_INNR_IPV6,
		 FLOW_KEY_TYPE_TCP | FLOW_KEY_TYPE_INNR_TCP,
		 FLOW_KEY_TYPE_UDP | FLOW_KEY_TYPE_INNR_UDP,
		 FLOW_KEY_TYPE_SCTP | FLOW_KEY_TYPE_INNR_SCTP,
		 FLOW_KEY_TYPE_ETH_DMAC | FLOW_KEY_TYPE_INNR_ETH_DMAC}
	};
	uint32_t flowkey_cfg = 0;

	dev->ethdev_rss_hf = ethdev_rss;

	if (ethdev_rss & ETH_RSS_L2_PAYLOAD)
		flowkey_cfg |= FLOW_KEY_TYPE_CH_LEN_90B;

	if (ethdev_rss & ETH_RSS_C_VLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VLAN;

	if (ethdev_rss & ETH_RSS_L3_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_SRC;

	if (ethdev_rss & ETH_RSS_L3_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_DST;

	if (ethdev_rss & ETH_RSS_L4_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_SRC;

	if (ethdev_rss & ETH_RSS_L4_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_DST;

	if (ethdev_rss & RSS_IPV4_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV4_INDEX];

	if (ethdev_rss & RSS_IPV6_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV6_INDEX];

	if (ethdev_rss & ETH_RSS_TCP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_TCP_INDEX];

	if (ethdev_rss & ETH_RSS_UDP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_UDP_INDEX];

	if (ethdev_rss & ETH_RSS_SCTP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_SCTP_INDEX];

	if (ethdev_rss & ETH_RSS_L2_PAYLOAD)
		flowkey_cfg |= flow_key_type[rss_level][RSS_DMAC_INDEX];

	if (ethdev_rss & RSS_IPV6_EX_ENABLE)
		flowkey_cfg |= FLOW_KEY_TYPE_IPV6_EXT;

	if (ethdev_rss & ETH_RSS_PORT)
		flowkey_cfg |= FLOW_KEY_TYPE_PORT;

	if (ethdev_rss & ETH_RSS_NVGRE)
		flowkey_cfg |= FLOW_KEY_TYPE_NVGRE;

	if (ethdev_rss & ETH_RSS_VXLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VXLAN;

	if (ethdev_rss & ETH_RSS_GENEVE)
		flowkey_cfg |= FLOW_KEY_TYPE_GENEVE;

	if (ethdev_rss & ETH_RSS_GTPU)
		flowkey_cfg |= FLOW_KEY_TYPE_GTPU;

	return flowkey_cfg;
}

static void
nix_free_queue_mem(struct cnxk_eth_dev *dev)
{
	plt_free(dev->rqs);
	plt_free(dev->cqs);
	plt_free(dev->sqs);
	dev->rqs = NULL;
	dev->cqs = NULL;
	dev->sqs = NULL;
}

static int
nix_rss_default_setup(struct cnxk_eth_dev *dev)
{
	struct rte_eth_dev *eth_dev = dev->eth_dev;
	uint8_t rss_hash_level;
	uint32_t flowkey_cfg;
	uint64_t rss_hf;

	rss_hf = eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	rss_hash_level = ETH_RSS_LEVEL(rss_hf);
	if (rss_hash_level)
		rss_hash_level -= 1;

	flowkey_cfg = cnxk_rss_ethdev_to_nix(dev, rss_hf, rss_hash_level);
	return roc_nix_rss_default_setup(&dev->nix, flowkey_cfg);
}

static int
nix_store_queue_cfg_and_then_release(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct cnxk_eth_qconf *tx_qconf = NULL;
	struct cnxk_eth_qconf *rx_qconf = NULL;
	struct cnxk_eth_rxq_sp *rxq_sp;
	struct cnxk_eth_txq_sp *txq_sp;
	int i, nb_rxq, nb_txq;
	void **txq, **rxq;

	nb_rxq = RTE_MIN(dev->nb_rxq, eth_dev->data->nb_rx_queues);
	nb_txq = RTE_MIN(dev->nb_txq, eth_dev->data->nb_tx_queues);

	tx_qconf = malloc(nb_txq * sizeof(*tx_qconf));
	if (tx_qconf == NULL) {
		plt_err("Failed to allocate memory for tx_qconf");
		goto fail;
	}

	rx_qconf = malloc(nb_rxq * sizeof(*rx_qconf));
	if (rx_qconf == NULL) {
		plt_err("Failed to allocate memory for rx_qconf");
		goto fail;
	}

	txq = eth_dev->data->tx_queues;
	for (i = 0; i < nb_txq; i++) {
		if (txq[i] == NULL) {
			tx_qconf[i].valid = false;
			plt_info("txq[%d] is already released", i);
			continue;
		}
		txq_sp = cnxk_eth_txq_to_sp(txq[i]);
		memcpy(&tx_qconf[i], &txq_sp->qconf, sizeof(*tx_qconf));
		tx_qconf[i].valid = true;
		dev_ops->tx_queue_release(txq[i]);
		eth_dev->data->tx_queues[i] = NULL;
	}

	rxq = eth_dev->data->rx_queues;
	for (i = 0; i < nb_rxq; i++) {
		if (rxq[i] == NULL) {
			rx_qconf[i].valid = false;
			plt_info("rxq[%d] is already released", i);
			continue;
		}
		rxq_sp = cnxk_eth_rxq_to_sp(rxq[i]);
		memcpy(&rx_qconf[i], &rxq_sp->qconf, sizeof(*rx_qconf));
		rx_qconf[i].valid = true;
		dev_ops->rx_queue_release(rxq[i]);
		eth_dev->data->rx_queues[i] = NULL;
	}

	dev->tx_qconf = tx_qconf;
	dev->rx_qconf = rx_qconf;
	return 0;

fail:
	free(tx_qconf);
	free(rx_qconf);
	return -ENOMEM;
}

static int
nix_restore_queue_cfg(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct cnxk_eth_qconf *tx_qconf = dev->tx_qconf;
	struct cnxk_eth_qconf *rx_qconf = dev->rx_qconf;
	int rc, i, nb_rxq, nb_txq;
	void **txq, **rxq;

	nb_rxq = RTE_MIN(dev->nb_rxq, eth_dev->data->nb_rx_queues);
	nb_txq = RTE_MIN(dev->nb_txq, eth_dev->data->nb_tx_queues);

	rc = -ENOMEM;
	/* Setup tx & rx queues with previous configuration so
	 * that the queues can be functional in cases like ports
	 * are started without re configuring queues.
	 *
	 * Usual re config sequence is like below:
	 * port_configure() {
	 *      if(reconfigure) {
	 *              queue_release()
	 *              queue_setup()
	 *      }
	 *      queue_configure() {
	 *              queue_release()
	 *              queue_setup()
	 *      }
	 * }
	 * port_start()
	 *
	 * In some application's control path, queue_configure() would
	 * NOT be invoked for TXQs/RXQs in port_configure().
	 * In such cases, queues can be functional after start as the
	 * queues are already setup in port_configure().
	 */
	for (i = 0; i < nb_txq; i++) {
		if (!tx_qconf[i].valid)
			continue;
		rc = dev_ops->tx_queue_setup(eth_dev, i, tx_qconf[i].nb_desc, 0,
					     &tx_qconf[i].conf.tx);
		if (rc) {
			plt_err("Failed to setup tx queue rc=%d", rc);
			txq = eth_dev->data->tx_queues;
			for (i -= 1; i >= 0; i--)
				dev_ops->tx_queue_release(txq[i]);
			goto fail;
		}
	}

	free(tx_qconf);
	tx_qconf = NULL;

	for (i = 0; i < nb_rxq; i++) {
		if (!rx_qconf[i].valid)
			continue;
		rc = dev_ops->rx_queue_setup(eth_dev, i, rx_qconf[i].nb_desc, 0,
					     &rx_qconf[i].conf.rx,
					     rx_qconf[i].mp);
		if (rc) {
			plt_err("Failed to setup rx queue rc=%d", rc);
			rxq = eth_dev->data->rx_queues;
			for (i -= 1; i >= 0; i--)
				dev_ops->rx_queue_release(rxq[i]);
			goto tx_queue_release;
		}
	}

	free(rx_qconf);
	rx_qconf = NULL;

	return 0;

tx_queue_release:
	txq = eth_dev->data->tx_queues;
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		dev_ops->tx_queue_release(txq[i]);
fail:
	if (tx_qconf)
		free(tx_qconf);
	if (rx_qconf)
		free(rx_qconf);

	return rc;
}

static uint16_t
nix_eth_nop_burst(void *queue, struct rte_mbuf **mbufs, uint16_t pkts)
{
	RTE_SET_USED(queue);
	RTE_SET_USED(mbufs);
	RTE_SET_USED(pkts);

	return 0;
}

static void
nix_set_nop_rxtx_function(struct rte_eth_dev *eth_dev)
{
	/* These dummy functions are required for supporting
	 * some applications which reconfigure queues without
	 * stopping tx burst and rx burst threads(eg kni app)
	 * When the queues context is saved, txq/rxqs are released
	 * which caused app crash since rx/tx burst is still
	 * on different lcores
	 */
	eth_dev->tx_pkt_burst = nix_eth_nop_burst;
	eth_dev->rx_pkt_burst = nix_eth_nop_burst;
	rte_mb();
}

static int
nix_lso_tun_fmt_update(struct cnxk_eth_dev *dev)
{
	uint8_t udp_tun[ROC_NIX_LSO_TUN_MAX];
	uint8_t tun[ROC_NIX_LSO_TUN_MAX];
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_lso_fmt_get(nix, udp_tun, tun);
	if (rc)
		return rc;

	dev->lso_tun_fmt = ((uint64_t)tun[ROC_NIX_LSO_TUN_V4V4] |
			    (uint64_t)tun[ROC_NIX_LSO_TUN_V4V6] << 8 |
			    (uint64_t)tun[ROC_NIX_LSO_TUN_V6V4] << 16 |
			    (uint64_t)tun[ROC_NIX_LSO_TUN_V6V6] << 24);

	dev->lso_tun_fmt |= ((uint64_t)udp_tun[ROC_NIX_LSO_TUN_V4V4] << 32 |
			     (uint64_t)udp_tun[ROC_NIX_LSO_TUN_V4V6] << 40 |
			     (uint64_t)udp_tun[ROC_NIX_LSO_TUN_V6V4] << 48 |
			     (uint64_t)udp_tun[ROC_NIX_LSO_TUN_V6V6] << 56);
	return 0;
}

static int
nix_lso_fmt_setup(struct cnxk_eth_dev *dev)
{
	struct roc_nix *nix = &dev->nix;
	int rc;

	/* Nothing much to do if offload is not enabled */
	if (!(dev->tx_offloads &
	      (DEV_TX_OFFLOAD_TCP_TSO | DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
	       DEV_TX_OFFLOAD_GENEVE_TNL_TSO | DEV_TX_OFFLOAD_GRE_TNL_TSO)))
		return 0;

	/* Setup LSO formats in AF. Its a no-op if other ethdev has
	 * already set it up
	 */
	rc = roc_nix_lso_fmt_setup(nix);
	if (rc)
		return rc;

	return nix_lso_tun_fmt_update(dev);
}

int
cnxk_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	char ea_fmt[RTE_ETHER_ADDR_FMT_SIZE];
	struct roc_nix *nix = &dev->nix;
	struct rte_ether_addr *ea;
	uint8_t nb_rxq, nb_txq;
	uint64_t rx_cfg;
	void *qs;
	int rc;

	rc = -EINVAL;

	/* Sanity checks */
	if (rte_eal_has_hugepages() == 0) {
		plt_err("Huge page is not configured");
		goto fail_configure;
	}

	if (conf->dcb_capability_en == 1) {
		plt_err("dcb enable is not supported");
		goto fail_configure;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		plt_err("Flow director is not supported");
		goto fail_configure;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
	    rxmode->mq_mode != ETH_MQ_RX_RSS) {
		plt_err("Unsupported mq rx mode %d", rxmode->mq_mode);
		goto fail_configure;
	}

	if (txmode->mq_mode != ETH_MQ_TX_NONE) {
		plt_err("Unsupported mq tx mode %d", txmode->mq_mode);
		goto fail_configure;
	}

	/* Free the resources allocated from the previous configure */
	if (dev->configured == 1) {
		/* Unregister queue irq's */
		roc_nix_unregister_queue_irqs(nix);

		/* Unregister CQ irqs if present */
		if (eth_dev->data->dev_conf.intr_conf.rxq)
			roc_nix_unregister_cq_irqs(nix);

		/* Set no-op functions */
		nix_set_nop_rxtx_function(eth_dev);
		/* Store queue config for later */
		rc = nix_store_queue_cfg_and_then_release(eth_dev);
		if (rc)
			goto fail_configure;
		roc_nix_tm_fini(nix);
		roc_nix_lf_free(nix);
	}

	dev->rx_offloads = rxmode->offloads;
	dev->tx_offloads = txmode->offloads;

	/* Prepare rx cfg */
	rx_cfg = ROC_NIX_LF_RX_CFG_DIS_APAD;
	if (dev->rx_offloads &
	    (DEV_RX_OFFLOAD_TCP_CKSUM | DEV_RX_OFFLOAD_UDP_CKSUM)) {
		rx_cfg |= ROC_NIX_LF_RX_CFG_CSUM_OL4;
		rx_cfg |= ROC_NIX_LF_RX_CFG_CSUM_IL4;
	}
	rx_cfg |= (ROC_NIX_LF_RX_CFG_DROP_RE | ROC_NIX_LF_RX_CFG_L2_LEN_ERR |
		   ROC_NIX_LF_RX_CFG_LEN_IL4 | ROC_NIX_LF_RX_CFG_LEN_IL3 |
		   ROC_NIX_LF_RX_CFG_LEN_OL4 | ROC_NIX_LF_RX_CFG_LEN_OL3);

	nb_rxq = RTE_MAX(data->nb_rx_queues, 1);
	nb_txq = RTE_MAX(data->nb_tx_queues, 1);

	/* Alloc a nix lf */
	rc = roc_nix_lf_alloc(nix, nb_rxq, nb_txq, rx_cfg);
	if (rc) {
		plt_err("Failed to init nix_lf rc=%d", rc);
		goto fail_configure;
	}

	nb_rxq = data->nb_rx_queues;
	nb_txq = data->nb_tx_queues;
	rc = -ENOMEM;
	if (nb_rxq) {
		/* Allocate memory for roc rq's and cq's */
		qs = plt_zmalloc(sizeof(struct roc_nix_rq) * nb_rxq, 0);
		if (!qs) {
			plt_err("Failed to alloc rqs");
			goto free_nix_lf;
		}
		dev->rqs = qs;

		qs = plt_zmalloc(sizeof(struct roc_nix_cq) * nb_rxq, 0);
		if (!qs) {
			plt_err("Failed to alloc cqs");
			goto free_nix_lf;
		}
		dev->cqs = qs;
	}

	if (nb_txq) {
		/* Allocate memory for roc sq's */
		qs = plt_zmalloc(sizeof(struct roc_nix_sq) * nb_txq, 0);
		if (!qs) {
			plt_err("Failed to alloc sqs");
			goto free_nix_lf;
		}
		dev->sqs = qs;
	}

	/* Re-enable NIX LF error interrupts */
	roc_nix_err_intr_ena_dis(nix, true);
	roc_nix_ras_intr_ena_dis(nix, true);

	if (nix->rx_ptp_ena) {
		plt_err("Both PTP and switch header enabled");
		goto free_nix_lf;
	}

	/* Setup LSO if needed */
	rc = nix_lso_fmt_setup(dev);
	if (rc) {
		plt_err("Failed to setup nix lso format fields, rc=%d", rc);
		goto free_nix_lf;
	}

	/* Configure RSS */
	rc = nix_rss_default_setup(dev);
	if (rc) {
		plt_err("Failed to configure rss rc=%d", rc);
		goto free_nix_lf;
	}

	/* Init the default TM scheduler hierarchy */
	rc = roc_nix_tm_init(nix);
	if (rc) {
		plt_err("Failed to init traffic manager, rc=%d", rc);
		goto free_nix_lf;
	}

	rc = roc_nix_tm_hierarchy_enable(nix, ROC_NIX_TM_DEFAULT, false);
	if (rc) {
		plt_err("Failed to enable default tm hierarchy, rc=%d", rc);
		goto tm_fini;
	}

	/* Register queue IRQs */
	rc = roc_nix_register_queue_irqs(nix);
	if (rc) {
		plt_err("Failed to register queue interrupts rc=%d", rc);
		goto tm_fini;
	}

	/* Register cq IRQs */
	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		if (eth_dev->data->nb_rx_queues > dev->nix.cints) {
			plt_err("Rx interrupt cannot be enabled, rxq > %d",
				dev->nix.cints);
			goto q_irq_fini;
		}
		/* Rx interrupt feature cannot work with vector mode because,
		 * vector mode does not process packets unless min 4 pkts are
		 * received, while cq interrupts are generated even for 1 pkt
		 * in the CQ.
		 */
		dev->scalar_ena = true;

		rc = roc_nix_register_cq_irqs(nix);
		if (rc) {
			plt_err("Failed to register CQ interrupts rc=%d", rc);
			goto q_irq_fini;
		}
	}

	/* Configure loop back mode */
	rc = roc_nix_mac_loopback_enable(nix,
					 eth_dev->data->dev_conf.lpbk_mode);
	if (rc) {
		plt_err("Failed to configure cgx loop back mode rc=%d", rc);
		goto cq_fini;
	}

	/*
	 * Restore queue config when reconfigure followed by
	 * reconfigure and no queue configure invoked from application case.
	 */
	if (dev->configured == 1) {
		rc = nix_restore_queue_cfg(eth_dev);
		if (rc)
			goto cq_fini;
	}

	/* Update the mac address */
	ea = eth_dev->data->mac_addrs;
	memcpy(ea, dev->mac_addr, RTE_ETHER_ADDR_LEN);
	if (rte_is_zero_ether_addr(ea))
		rte_eth_random_addr((uint8_t *)ea);

	rte_ether_format_addr(ea_fmt, RTE_ETHER_ADDR_FMT_SIZE, ea);

	plt_nix_dbg("Configured port%d mac=%s nb_rxq=%d nb_txq=%d"
		    " rx_offloads=0x%" PRIx64 " tx_offloads=0x%" PRIx64 "",
		    eth_dev->data->port_id, ea_fmt, nb_rxq, nb_txq,
		    dev->rx_offloads, dev->tx_offloads);

	/* All good */
	dev->configured = 1;
	dev->nb_rxq = data->nb_rx_queues;
	dev->nb_txq = data->nb_tx_queues;
	return 0;

cq_fini:
	roc_nix_unregister_cq_irqs(nix);
q_irq_fini:
	roc_nix_unregister_queue_irqs(nix);
tm_fini:
	roc_nix_tm_fini(nix);
free_nix_lf:
	nix_free_queue_mem(dev);
	rc |= roc_nix_lf_free(nix);
fail_configure:
	dev->configured = 0;
	return rc;
}

/* CNXK platform independent eth dev ops */
struct eth_dev_ops cnxk_eth_dev_ops = {
	.dev_infos_get = cnxk_nix_info_get,
};

static int
cnxk_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct rte_pci_device *pci_dev;
	int rc, max_entries;

	eth_dev->dev_ops = &cnxk_eth_dev_ops;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* Parse devargs string */
	rc = cnxk_ethdev_parse_devargs(eth_dev->device->devargs, dev);
	if (rc) {
		plt_err("Failed to parse devargs rc=%d", rc);
		goto error;
	}

	/* Initialize base roc nix */
	nix->pci_dev = pci_dev;
	rc = roc_nix_dev_init(nix);
	if (rc) {
		plt_err("Failed to initialize roc nix rc=%d", rc);
		goto error;
	}

	dev->eth_dev = eth_dev;
	dev->configured = 0;

	/* For vfs, returned max_entries will be 0. but to keep default mac
	 * address, one entry must be allocated. so setting up to 1.
	 */
	if (roc_nix_is_vf_or_sdp(nix))
		max_entries = 1;
	else
		max_entries = roc_nix_mac_max_entries_get(nix);

	if (max_entries <= 0) {
		plt_err("Failed to get max entries for mac addr");
		rc = -ENOTSUP;
		goto dev_fini;
	}

	eth_dev->data->mac_addrs =
		rte_zmalloc("mac_addr", max_entries * RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		plt_err("Failed to allocate memory for mac addr");
		rc = -ENOMEM;
		goto dev_fini;
	}

	dev->max_mac_entries = max_entries;

	/* Get mac address */
	rc = roc_nix_npc_mac_addr_get(nix, dev->mac_addr);
	if (rc) {
		plt_err("Failed to get mac addr, rc=%d", rc);
		goto free_mac_addrs;
	}

	/* Update the mac address */
	memcpy(eth_dev->data->mac_addrs, dev->mac_addr, RTE_ETHER_ADDR_LEN);

	if (!roc_nix_is_vf_or_sdp(nix)) {
		/* Sync same MAC address to CGX/RPM table */
		rc = roc_nix_mac_addr_set(nix, dev->mac_addr);
		if (rc) {
			plt_err("Failed to set mac addr, rc=%d", rc);
			goto free_mac_addrs;
		}
	}

	/* Union of all capabilities supported by CNXK.
	 * Platform specific capabilities will be
	 * updated later.
	 */
	dev->rx_offload_capa = nix_get_rx_offload_capa(dev);
	dev->tx_offload_capa = nix_get_tx_offload_capa(dev);
	dev->speed_capa = nix_get_speed_capa(dev);

	/* Initialize roc npc */
	plt_nix_dbg("Port=%d pf=%d vf=%d ver=%s hwcap=0x%" PRIx64
		    " rxoffload_capa=0x%" PRIx64 " txoffload_capa=0x%" PRIx64,
		    eth_dev->data->port_id, roc_nix_get_pf(nix),
		    roc_nix_get_vf(nix), CNXK_ETH_DEV_PMD_VERSION, dev->hwcap,
		    dev->rx_offload_capa, dev->tx_offload_capa);
	return 0;

free_mac_addrs:
	rte_free(eth_dev->data->mac_addrs);
dev_fini:
	roc_nix_dev_fini(nix);
error:
	plt_err("Failed to init nix eth_dev rc=%d", rc);
	return rc;
}

static int
cnxk_eth_dev_uninit(struct rte_eth_dev *eth_dev, bool mbox_close)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct roc_nix *nix = &dev->nix;
	int rc, i;

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Clear the flag since we are closing down */
	dev->configured = 0;

	roc_nix_npc_rx_ena_dis(nix, false);

	/* Free up SQs */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		dev_ops->tx_queue_release(eth_dev->data->tx_queues[i]);
		eth_dev->data->tx_queues[i] = NULL;
	}
	eth_dev->data->nb_tx_queues = 0;

	/* Free up RQ's and CQ's */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		dev_ops->rx_queue_release(eth_dev->data->rx_queues[i]);
		eth_dev->data->rx_queues[i] = NULL;
	}
	eth_dev->data->nb_rx_queues = 0;

	/* Free tm resources */
	roc_nix_tm_fini(nix);

	/* Unregister queue irqs */
	roc_nix_unregister_queue_irqs(nix);

	/* Unregister cq irqs */
	if (eth_dev->data->dev_conf.intr_conf.rxq)
		roc_nix_unregister_cq_irqs(nix);

	/* Free ROC RQ's, SQ's and CQ's memory */
	nix_free_queue_mem(dev);

	/* Free nix lf resources */
	rc = roc_nix_lf_free(nix);
	if (rc)
		plt_err("Failed to free nix lf, rc=%d", rc);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	/* Check if mbox close is needed */
	if (!mbox_close)
		return 0;

	rc = roc_nix_dev_fini(nix);
	/* Can be freed later by PMD if NPA LF is in use */
	if (rc == -EAGAIN) {
		eth_dev->data->dev_private = NULL;
		return 0;
	} else if (rc) {
		plt_err("Failed in nix dev fini, rc=%d", rc);
	}

	return rc;
}

int
cnxk_nix_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	struct roc_nix *nix;
	int rc = -EINVAL;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (eth_dev) {
		/* Cleanup eth dev */
		rc = cnxk_eth_dev_uninit(eth_dev, true);
		if (rc)
			return rc;

		rte_eth_dev_release_port(eth_dev);
	}

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Check if this device is hosting common resource */
	nix = roc_idev_npa_nix_get();
	if (nix->pci_dev != pci_dev)
		return 0;

	/* Try nix fini now */
	rc = roc_nix_dev_fini(nix);
	if (rc == -EAGAIN) {
		plt_info("%s: common resource in use by other devices",
			 pci_dev->name);
		goto exit;
	} else if (rc) {
		plt_err("Failed in nix dev fini, rc=%d", rc);
		goto exit;
	}

	/* Free device pointer as rte_ethdev does not have it anymore */
	rte_free(nix);
exit:
	return rc;
}

int
cnxk_nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct cnxk_eth_dev),
					   cnxk_eth_dev_init);

	/* On error on secondary, recheck if port exists in primary or
	 * in mid of detach state.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY && rc)
		if (!rte_eth_dev_allocated(pci_dev->device.name))
			return 0;
	return rc;
}
