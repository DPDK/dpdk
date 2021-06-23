/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "cn10k_ethdev.h"
#include "cn10k_rx.h"
#include "cn10k_tx.h"

static int
cn10k_nix_ptypes_set(struct rte_eth_dev *eth_dev, uint32_t ptype_mask)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	if (ptype_mask) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_PTYPE_F;
		dev->ptype_disable = 0;
	} else {
		dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_PTYPE_F;
		dev->ptype_disable = 1;
	}

	return 0;
}

static void
nix_form_default_desc(struct cnxk_eth_dev *dev, struct cn10k_eth_txq *txq,
		      uint16_t qid)
{
	struct nix_send_ext_s *send_hdr_ext;
	union nix_send_hdr_w0_u send_hdr_w0;
	union nix_send_sg_s sg_w0;

	RTE_SET_USED(dev);

	/* Initialize the fields based on basic single segment packet */
	memset(&txq->cmd, 0, sizeof(txq->cmd));
	send_hdr_w0.u = 0;
	sg_w0.u = 0;

	if (dev->tx_offload_flags & NIX_TX_NEED_EXT_HDR) {
		/* 2(HDR) + 2(EXT_HDR) + 1(SG) + 1(IOVA) = 6/2 - 1 = 2 */
		send_hdr_w0.sizem1 = 2;

		send_hdr_ext = (struct nix_send_ext_s *)&txq->cmd[0];
		send_hdr_ext->w0.subdc = NIX_SUBDC_EXT;
	} else {
		/* 2(HDR) + 1(SG) + 1(IOVA) = 4/2 - 1 = 1 */
		send_hdr_w0.sizem1 = 1;
	}

	send_hdr_w0.sq = qid;
	sg_w0.subdc = NIX_SUBDC_SG;
	sg_w0.segs = 1;
	sg_w0.ld_type = NIX_SENDLDTYPE_LDD;

	txq->send_hdr_w0 = send_hdr_w0.u;
	txq->sg_w0 = sg_w0.u;

	rte_wmb();
}

static int
cn10k_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			 uint16_t nb_desc, unsigned int socket,
			 const struct rte_eth_txconf *tx_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cn10k_eth_txq *txq;
	struct roc_nix_sq *sq;
	int rc;

	RTE_SET_USED(socket);

	/* Common Tx queue setup */
	rc = cnxk_nix_tx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn10k_eth_txq), tx_conf);
	if (rc)
		return rc;

	sq = &dev->sqs[qid];
	/* Update fast path queue */
	txq = eth_dev->data->tx_queues[qid];
	txq->fc_mem = sq->fc;
	/* Store lmt base in tx queue for easy access */
	txq->lmt_base = dev->nix.lmt_base;
	txq->io_addr = sq->io_addr;
	txq->nb_sqb_bufs_adj = sq->nb_sqb_bufs_adj;
	txq->sqes_per_sqb_log2 = sq->sqes_per_sqb_log2;

	nix_form_default_desc(dev, txq, qid);
	txq->lso_tun_fmt = dev->lso_tun_fmt;
	return 0;
}

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

	/* Lookup mem */
	rxq->lookup_mem = cnxk_nix_fastpath_lookup_mem_get();
	return 0;
}

static int
cn10k_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct cn10k_eth_txq *txq = eth_dev->data->tx_queues[qidx];
	int rc;

	rc = cnxk_nix_tx_queue_stop(eth_dev, qidx);
	if (rc)
		return rc;

	/* Clear fc cache pkts to trigger worker stop */
	txq->fc_cache_pkts = 0;
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
	cnxk_eth_dev_ops.tx_queue_setup = cn10k_nix_tx_queue_setup;
	cnxk_eth_dev_ops.rx_queue_setup = cn10k_nix_rx_queue_setup;
	cnxk_eth_dev_ops.tx_queue_stop = cn10k_nix_tx_queue_stop;
	cnxk_eth_dev_ops.dev_ptypes_set = cn10k_nix_ptypes_set;
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
