/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_ethdev.h>

/* CNXK platform independent eth dev ops */
struct eth_dev_ops cnxk_eth_dev_ops;

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

	/* Initialize base roc nix */
	nix->pci_dev = pci_dev;
	rc = roc_nix_dev_init(nix);
	if (rc) {
		plt_err("Failed to initialize roc nix rc=%d", rc);
		goto error;
	}

	dev->eth_dev = eth_dev;

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
