/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "cn10k_ethdev.h"

static int
cn10k_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			 uint16_t nb_desc, unsigned int socket,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cn10k_eth_rxq *rxq;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc;

	RTE_SET_USED(socket);

	/* CQ Errata needs min 4K ring */
	if (dev->cq_min_4k && nb_desc < 4096)
		nb_desc = 4096;

	/* Common Rx queue setup */
	rc = cnxk_nix_rx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn10k_eth_rxq), rx_conf, mp);
	if (rc)
		return rc;

	rq = &dev->rqs[qid];
	cq = &dev->cqs[qid];

	/* Update fast path queue */
	rxq = eth_dev->data->rx_queues[qid];
	rxq->rq = qid;
	rxq->desc = (uintptr_t)cq->desc_base;
	rxq->cq_door = cq->door;
	rxq->cq_status = cq->status;
	rxq->wdata = cq->wdata;
	rxq->head = cq->head;
	rxq->qmask = cq->qmask;

	/* Data offset from data to start of mbuf is first_skip */
	rxq->data_off = rq->first_skip;
	rxq->mbuf_initializer = cnxk_nix_rxq_mbuf_setup(dev);
	return 0;
}

static int
cn10k_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc;

	/* Common nix configure */
	rc = cnxk_nix_configure(eth_dev);
	if (rc)
		return rc;

	plt_nix_dbg("Configured port%d platform specific rx_offload_flags=%x"
		    " tx_offload_flags=0x%x",
		    eth_dev->data->port_id, dev->rx_offload_flags,
		    dev->tx_offload_flags);
	return 0;
}

/* Update platform specific eth dev ops */
static void
nix_eth_dev_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_eth_dev_ops.dev_configure = cn10k_nix_configure;
	cnxk_eth_dev_ops.rx_queue_setup = cn10k_nix_rx_queue_setup;
}

static int
cn10k_nix_remove(struct rte_pci_device *pci_dev)
{
	return cnxk_nix_remove(pci_dev);
}

static int
cn10k_nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	int rc;

	if (RTE_CACHE_LINE_SIZE != 64) {
		plt_err("Driver not compiled for CN10K");
		return -EFAULT;
	}

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}

	nix_eth_dev_ops_override();

	/* Common probe */
	rc = cnxk_nix_probe(pci_drv, pci_dev);
	if (rc)
		return rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
		if (!eth_dev)
			return -ENOENT;
	}
	return 0;
}

static const struct rte_pci_id cn10k_pci_nix_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_AF_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn10k_pci_nix = {
	.id_table = cn10k_pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = cn10k_nix_probe,
	.remove = cn10k_nix_remove,
};

RTE_PMD_REGISTER_PCI(net_cn10k, cn10k_pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_cn10k, cn10k_pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_cn10k, "vfio-pci");
