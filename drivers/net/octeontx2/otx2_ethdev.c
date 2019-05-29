/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "otx2_ethdev.h"

static inline void
otx2_eth_set_rx_function(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
}

static inline void
otx2_eth_set_tx_function(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
}

static inline uint64_t
nix_get_rx_offload_capa(struct otx2_eth_dev *dev)
{
	uint64_t capa = NIX_RX_OFFLOAD_CAPA;

	if (otx2_dev_is_vf(dev))
		capa &= ~DEV_RX_OFFLOAD_TIMESTAMP;

	return capa;
}

static inline uint64_t
nix_get_tx_offload_capa(struct otx2_eth_dev *dev)
{
	RTE_SET_USED(dev);

	return NIX_TX_OFFLOAD_CAPA;
}

static const struct otx2_dev_ops otx2_dev_ops = {
	.link_status_update = otx2_eth_dev_link_status_update,
};

static int
nix_lf_alloc(struct otx2_eth_dev *dev, uint32_t nb_rxq, uint32_t nb_txq)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_lf_alloc_req *req;
	struct nix_lf_alloc_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_nix_lf_alloc(mbox);
	req->rq_cnt = nb_rxq;
	req->sq_cnt = nb_txq;
	req->cq_cnt = nb_rxq;
	/* XQE_SZ should be in Sync with NIX_CQ_ENTRY_SZ */
	RTE_BUILD_BUG_ON(NIX_CQ_ENTRY_SZ != 128);
	req->xqe_sz = NIX_XQESZ_W16;
	req->rss_sz = dev->rss_info.rss_size;
	req->rss_grps = NIX_RSS_GRPS;
	req->npa_func = otx2_npa_pf_func_get();
	req->sso_func = otx2_sso_pf_func_get();
	req->rx_cfg = BIT_ULL(35 /* DIS_APAD */);
	if (dev->rx_offloads & (DEV_RX_OFFLOAD_TCP_CKSUM |
			 DEV_RX_OFFLOAD_UDP_CKSUM)) {
		req->rx_cfg |= BIT_ULL(37 /* CSUM_OL4 */);
		req->rx_cfg |= BIT_ULL(36 /* CSUM_IL4 */);
	}

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->sqb_size = rsp->sqb_size;
	dev->tx_chan_base = rsp->tx_chan_base;
	dev->rx_chan_base = rsp->rx_chan_base;
	dev->rx_chan_cnt = rsp->rx_chan_cnt;
	dev->tx_chan_cnt = rsp->tx_chan_cnt;
	dev->lso_tsov4_idx = rsp->lso_tsov4_idx;
	dev->lso_tsov6_idx = rsp->lso_tsov6_idx;
	dev->lf_tx_stats = rsp->lf_tx_stats;
	dev->lf_rx_stats = rsp->lf_rx_stats;
	dev->cints = rsp->cints;
	dev->qints = rsp->qints;
	dev->npc_flow.channel = dev->rx_chan_base;

	return 0;
}

static int
nix_lf_free(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_lf_free_req *req;
	struct ndc_sync_op *ndc_req;
	int rc;

	/* Sync NDC-NIX for LF */
	ndc_req = otx2_mbox_alloc_msg_ndc_sync_op(mbox);
	ndc_req->nix_lf_tx_sync = 1;
	ndc_req->nix_lf_rx_sync = 1;
	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Error on NDC-NIX-[TX, RX] LF sync, rc %d", rc);

	req = otx2_mbox_alloc_msg_nix_lf_free(mbox);
	/* Let AF driver free all this nix lf's
	 * NPC entries allocated using NPC MBOX.
	 */
	req->flags = 0;

	return otx2_mbox_process(mbox);
}

static int
otx2_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	char ea_fmt[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *ea;
	uint8_t nb_rxq, nb_txq;
	int rc;

	rc = -EINVAL;

	/* Sanity checks */
	if (rte_eal_has_hugepages() == 0) {
		otx2_err("Huge page is not configured");
		goto fail;
	}

	if (rte_eal_iova_mode() != RTE_IOVA_VA) {
		otx2_err("iova mode should be va");
		goto fail;
	}

	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		otx2_err("Setting link speed/duplex not supported");
		goto fail;
	}

	if (conf->dcb_capability_en == 1) {
		otx2_err("dcb enable is not supported");
		goto fail;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		otx2_err("Flow director is not supported");
		goto fail;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
	    rxmode->mq_mode != ETH_MQ_RX_RSS) {
		otx2_err("Unsupported mq rx mode %d", rxmode->mq_mode);
		goto fail;
	}

	if (txmode->mq_mode != ETH_MQ_TX_NONE) {
		otx2_err("Unsupported mq tx mode %d", txmode->mq_mode);
		goto fail;
	}

	/* Free the resources allocated from the previous configure */
	if (dev->configured == 1) {
		oxt2_nix_unregister_queue_irqs(eth_dev);
		nix_lf_free(dev);
	}

	if (otx2_dev_is_A0(dev) &&
	    (txmode->offloads & DEV_TX_OFFLOAD_SCTP_CKSUM) &&
	    ((txmode->offloads & DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) ||
	    (txmode->offloads & DEV_TX_OFFLOAD_OUTER_UDP_CKSUM))) {
		otx2_err("Outer IP and SCTP checksum unsupported");
		rc = -EINVAL;
		goto fail;
	}

	dev->rx_offloads = rxmode->offloads;
	dev->tx_offloads = txmode->offloads;
	dev->rss_info.rss_grps = NIX_RSS_GRPS;

	nb_rxq = RTE_MAX(data->nb_rx_queues, 1);
	nb_txq = RTE_MAX(data->nb_tx_queues, 1);

	/* Alloc a nix lf */
	rc = nix_lf_alloc(dev, nb_rxq, nb_txq);
	if (rc) {
		otx2_err("Failed to init nix_lf rc=%d", rc);
		goto fail;
	}

	/* Register queue IRQs */
	rc = oxt2_nix_register_queue_irqs(eth_dev);
	if (rc) {
		otx2_err("Failed to register queue interrupts rc=%d", rc);
		goto free_nix_lf;
	}

	/* Update the mac address */
	ea = eth_dev->data->mac_addrs;
	memcpy(ea, dev->mac_addr, RTE_ETHER_ADDR_LEN);
	if (rte_is_zero_ether_addr(ea))
		rte_eth_random_addr((uint8_t *)ea);

	rte_ether_format_addr(ea_fmt, RTE_ETHER_ADDR_FMT_SIZE, ea);

	otx2_nix_dbg("Configured port%d mac=%s nb_rxq=%d nb_txq=%d"
		" rx_offloads=0x%" PRIx64 " tx_offloads=0x%" PRIx64 ""
		" rx_flags=0x%x tx_flags=0x%x",
		eth_dev->data->port_id, ea_fmt, nb_rxq,
		nb_txq, dev->rx_offloads, dev->tx_offloads,
		dev->rx_offload_flags, dev->tx_offload_flags);

	/* All good */
	dev->configured = 1;
	dev->configured_nb_rx_qs = data->nb_rx_queues;
	dev->configured_nb_tx_qs = data->nb_tx_queues;
	return 0;

free_nix_lf:
	rc = nix_lf_free(dev);
fail:
	return rc;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops otx2_eth_dev_ops = {
	.dev_infos_get            = otx2_nix_info_get,
	.dev_configure            = otx2_nix_configure,
	.link_update              = otx2_nix_link_update,
	.stats_get                = otx2_nix_dev_stats_get,
	.stats_reset              = otx2_nix_dev_stats_reset,
	.get_reg                  = otx2_nix_dev_get_reg,
	.promiscuous_enable       = otx2_nix_promisc_enable,
	.promiscuous_disable      = otx2_nix_promisc_disable,
	.allmulticast_enable      = otx2_nix_allmulticast_enable,
	.allmulticast_disable     = otx2_nix_allmulticast_disable,
	.queue_stats_mapping_set  = otx2_nix_queue_stats_mapping,
	.xstats_get               = otx2_nix_xstats_get,
	.xstats_get_names         = otx2_nix_xstats_get_names,
	.xstats_reset             = otx2_nix_xstats_reset,
	.xstats_get_by_id         = otx2_nix_xstats_get_by_id,
	.xstats_get_names_by_id   = otx2_nix_xstats_get_names_by_id,
};

static inline int
nix_lf_attach(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct rsrc_attach_req *req;

	/* Attach NIX(lf) */
	req = otx2_mbox_alloc_msg_attach_resources(mbox);
	req->modify = true;
	req->nixlf = true;

	return otx2_mbox_process(mbox);
}

static inline int
nix_lf_get_msix_offset(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct msix_offset_rsp *msix_rsp;
	int rc;

	/* Get NPA and NIX MSIX vector offsets */
	otx2_mbox_alloc_msg_msix_offset(mbox);

	rc = otx2_mbox_process_msg(mbox, (void *)&msix_rsp);

	dev->nix_msixoff = msix_rsp->nix_msixoff;

	return rc;
}

static inline int
otx2_eth_dev_lf_detach(struct otx2_mbox *mbox)
{
	struct rsrc_detach_req *req;

	req = otx2_mbox_alloc_msg_detach_resources(mbox);

	/* Detach all except npa lf */
	req->partial = true;
	req->nixlf = true;
	req->sso = true;
	req->ssow = true;
	req->timlfs = true;
	req->cptlfs = true;

	return otx2_mbox_process(mbox);
}

static int
otx2_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_pci_device *pci_dev;
	int rc, max_entries;

	eth_dev->dev_ops = &otx2_eth_dev_ops;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* Setup callbacks for secondary process */
		otx2_eth_set_tx_function(eth_dev);
		otx2_eth_set_rx_function(eth_dev);
		return 0;
	}

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	/* Zero out everything after OTX2_DEV to allow proper dev_reset() */
	memset(&dev->otx2_eth_dev_data_start, 0, sizeof(*dev) -
		offsetof(struct otx2_eth_dev, otx2_eth_dev_data_start));

	/* Parse devargs string */
	rc = otx2_ethdev_parse_devargs(eth_dev->device->devargs, dev);
	if (rc) {
		otx2_err("Failed to parse devargs rc=%d", rc);
		goto error;
	}

	if (!dev->mbox_active) {
		/* Initialize the base otx2_dev object
		 * only if already present
		 */
		rc = otx2_dev_init(pci_dev, dev);
		if (rc) {
			otx2_err("Failed to initialize otx2_dev rc=%d", rc);
			goto error;
		}
	}
	/* Device generic callbacks */
	dev->ops = &otx2_dev_ops;
	dev->eth_dev = eth_dev;

	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc)
		goto otx2_dev_uninit;

	dev->configured = 0;
	dev->drv_inited = true;
	dev->base = dev->bar2 + (RVU_BLOCK_ADDR_NIX0 << 20);
	dev->lmt_addr = dev->bar2 + (RVU_BLOCK_ADDR_LMT << 20);

	/* Attach NIX LF */
	rc = nix_lf_attach(dev);
	if (rc)
		goto otx2_npa_uninit;

	/* Get NIX MSIX offset */
	rc = nix_lf_get_msix_offset(dev);
	if (rc)
		goto otx2_npa_uninit;

	/* Register LF irq handlers */
	rc = otx2_nix_register_irqs(eth_dev);
	if (rc)
		goto mbox_detach;

	/* Get maximum number of supported MAC entries */
	max_entries = otx2_cgx_mac_max_entries_get(dev);
	if (max_entries < 0) {
		otx2_err("Failed to get max entries for mac addr");
		rc = -ENOTSUP;
		goto unregister_irq;
	}

	/* For VFs, returned max_entries will be 0. But to keep default MAC
	 * address, one entry must be allocated. So setting up to 1.
	 */
	if (max_entries == 0)
		max_entries = 1;

	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", max_entries *
					       RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		otx2_err("Failed to allocate memory for mac addr");
		rc = -ENOMEM;
		goto unregister_irq;
	}

	dev->max_mac_entries = max_entries;

	rc = otx2_nix_mac_addr_get(eth_dev, dev->mac_addr);
	if (rc)
		goto free_mac_addrs;

	/* Update the mac address */
	memcpy(eth_dev->data->mac_addrs, dev->mac_addr, RTE_ETHER_ADDR_LEN);

	/* Also sync same MAC address to CGX table */
	otx2_cgx_mac_addr_set(eth_dev, &eth_dev->data->mac_addrs[0]);

	dev->tx_offload_capa = nix_get_tx_offload_capa(dev);
	dev->rx_offload_capa = nix_get_rx_offload_capa(dev);

	if (otx2_dev_is_A0(dev)) {
		dev->hwcap |= OTX2_FIXUP_F_MIN_4K_Q;
		dev->hwcap |= OTX2_FIXUP_F_LIMIT_CQ_FULL;
	}

	otx2_nix_dbg("Port=%d pf=%d vf=%d ver=%s msix_off=%d hwcap=0x%" PRIx64
		     " rxoffload_capa=0x%" PRIx64 " txoffload_capa=0x%" PRIx64,
		     eth_dev->data->port_id, dev->pf, dev->vf,
		     OTX2_ETH_DEV_PMD_VERSION, dev->nix_msixoff, dev->hwcap,
		     dev->rx_offload_capa, dev->tx_offload_capa);
	return 0;

free_mac_addrs:
	rte_free(eth_dev->data->mac_addrs);
unregister_irq:
	otx2_nix_unregister_irqs(eth_dev);
mbox_detach:
	otx2_eth_dev_lf_detach(dev->mbox);
otx2_npa_uninit:
	otx2_npa_lf_fini();
otx2_dev_uninit:
	otx2_dev_fini(pci_dev, dev);
error:
	otx2_err("Failed to init nix eth_dev rc=%d", rc);
	return rc;
}

static int
otx2_eth_dev_uninit(struct rte_eth_dev *eth_dev, bool mbox_close)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_pci_device *pci_dev;
	int rc;

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Unregister queue irqs */
	oxt2_nix_unregister_queue_irqs(eth_dev);

	rc = nix_lf_free(dev);
	if (rc)
		otx2_err("Failed to free nix lf, rc=%d", rc);

	rc = otx2_npa_lf_fini();
	if (rc)
		otx2_err("Failed to cleanup npa lf, rc=%d", rc);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	dev->drv_inited = false;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	otx2_nix_unregister_irqs(eth_dev);

	rc = otx2_eth_dev_lf_detach(dev->mbox);
	if (rc)
		otx2_err("Failed to detach resources, rc=%d", rc);

	/* Check if mbox close is needed */
	if (!mbox_close)
		return 0;

	if (otx2_npa_lf_active(dev) || otx2_dev_active_vfs(dev)) {
		/* Will be freed later by PMD */
		eth_dev->data->dev_private = NULL;
		return 0;
	}

	otx2_dev_fini(pci_dev, dev);
	return 0;
}

static int
nix_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_dev *otx2_dev;
	int rc;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (eth_dev) {
		/* Cleanup eth dev */
		rc = otx2_eth_dev_uninit(eth_dev, true);
		if (rc)
			return rc;

		rte_eth_dev_pci_release(eth_dev);
	}

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Check for common resources */
	idev = otx2_intra_dev_get_cfg();
	if (!idev || !idev->npa_lf || idev->npa_lf->pci_dev != pci_dev)
		return 0;

	otx2_dev = container_of(idev->npa_lf, struct otx2_dev, npalf);

	if (otx2_npa_lf_active(otx2_dev) || otx2_dev_active_vfs(otx2_dev))
		goto exit;

	/* Safe to cleanup mbox as no more users */
	otx2_dev_fini(pci_dev, otx2_dev);
	rte_free(otx2_dev);
	return 0;

exit:
	otx2_info("%s: common resource in use by other devices", pci_dev->name);
	return -EAGAIN;
}

static int
nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct otx2_eth_dev),
					   otx2_eth_dev_init);

	/* On error on secondary, recheck if port exists in primary or
	 * in mid of detach state.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY && rc)
		if (!rte_eth_dev_allocated(pci_dev->device.name))
			return 0;
	return rc;
}

static const struct rte_pci_id pci_nix_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_AF_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_nix = {
	.id_table = pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA |
			RTE_PCI_DRV_INTR_LSC,
	.probe = nix_probe,
	.remove = nix_remove,
};

RTE_PMD_REGISTER_PCI(net_octeontx2, pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_octeontx2, pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_octeontx2, "vfio-pci");
