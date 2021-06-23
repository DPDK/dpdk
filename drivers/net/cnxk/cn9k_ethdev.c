/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "cn9k_ethdev.h"
#include "cn9k_tx.h"

static void
nix_form_default_desc(struct cnxk_eth_dev *dev, struct cn9k_eth_txq *txq,
		      uint16_t qid)
{
	struct nix_send_ext_s *send_hdr_ext;
	struct nix_send_hdr_s *send_hdr;
	union nix_send_sg_s *sg;

	RTE_SET_USED(dev);

	/* Initialize the fields based on basic single segment packet */
	memset(&txq->cmd, 0, sizeof(txq->cmd));

	if (dev->tx_offload_flags & NIX_TX_NEED_EXT_HDR) {
		send_hdr = (struct nix_send_hdr_s *)&txq->cmd[0];
		/* 2(HDR) + 2(EXT_HDR) + 1(SG) + 1(IOVA) = 6/2 - 1 = 2 */
		send_hdr->w0.sizem1 = 2;

		send_hdr_ext = (struct nix_send_ext_s *)&txq->cmd[2];
		send_hdr_ext->w0.subdc = NIX_SUBDC_EXT;
		sg = (union nix_send_sg_s *)&txq->cmd[4];
	} else {
		send_hdr = (struct nix_send_hdr_s *)&txq->cmd[0];
		/* 2(HDR) + 1(SG) + 1(IOVA) = 4/2 - 1 = 1 */
		send_hdr->w0.sizem1 = 1;
		sg = (union nix_send_sg_s *)&txq->cmd[2];
	}

	send_hdr->w0.sq = qid;
	sg->subdc = NIX_SUBDC_SG;
	sg->segs = 1;
	sg->ld_type = NIX_SENDLDTYPE_LDD;

	rte_wmb();
}

static int
cn9k_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			uint16_t nb_desc, unsigned int socket,
			const struct rte_eth_txconf *tx_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cn9k_eth_txq *txq;
	struct roc_nix_sq *sq;
	int rc;

	RTE_SET_USED(socket);

	/* Common Tx queue setup */
	rc = cnxk_nix_tx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn9k_eth_txq), tx_conf);
	if (rc)
		return rc;

	sq = &dev->sqs[qid];
	/* Update fast path queue */
	txq = eth_dev->data->tx_queues[qid];
	txq->fc_mem = sq->fc;
	txq->lmt_addr = sq->lmt_addr;
	txq->io_addr = sq->io_addr;
	txq->nb_sqb_bufs_adj = sq->nb_sqb_bufs_adj;
	txq->sqes_per_sqb_log2 = sq->sqes_per_sqb_log2;

	nix_form_default_desc(dev, txq, qid);
	txq->lso_tun_fmt = dev->lso_tun_fmt;
	return 0;
}

static int
cn9k_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			uint16_t nb_desc, unsigned int socket,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cn9k_eth_rxq *rxq;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc;

	RTE_SET_USED(socket);

	/* CQ Errata needs min 4K ring */
	if (dev->cq_min_4k && nb_desc < 4096)
		nb_desc = 4096;

	/* Common Rx queue setup */
	rc = cnxk_nix_rx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn9k_eth_rxq), rx_conf, mp);
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
cn9k_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_conf *conf = &eth_dev->data->dev_conf;
	struct rte_eth_txmode *txmode = &conf->txmode;
	int rc;

	/* Platform specific checks */
	if ((roc_model_is_cn96_a0() || roc_model_is_cn95_a0()) &&
	    (txmode->offloads & DEV_TX_OFFLOAD_SCTP_CKSUM) &&
	    ((txmode->offloads & DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) ||
	     (txmode->offloads & DEV_TX_OFFLOAD_OUTER_UDP_CKSUM))) {
		plt_err("Outer IP and SCTP checksum unsupported");
		return -EINVAL;
	}

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
	cnxk_eth_dev_ops.dev_configure = cn9k_nix_configure;
	cnxk_eth_dev_ops.tx_queue_setup = cn9k_nix_tx_queue_setup;
	cnxk_eth_dev_ops.rx_queue_setup = cn9k_nix_rx_queue_setup;
}

static int
cn9k_nix_remove(struct rte_pci_device *pci_dev)
{
	return cnxk_nix_remove(pci_dev);
}

static int
cn9k_nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	int rc;

	if (RTE_CACHE_LINE_SIZE != 128) {
		plt_err("Driver not compiled for CN9K");
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

	/* Find eth dev allocated */
	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!eth_dev)
		return -ENOENT;

	dev = cnxk_eth_pmd_priv(eth_dev);
	/* Update capabilities already set for TSO.
	 * TSO not supported for earlier chip revisions
	 */
	if (roc_model_is_cn96_a0() || roc_model_is_cn95_a0())
		dev->tx_offload_capa &= ~(DEV_TX_OFFLOAD_TCP_TSO |
					  DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
					  DEV_TX_OFFLOAD_GENEVE_TNL_TSO |
					  DEV_TX_OFFLOAD_GRE_TNL_TSO);

	/* 50G and 100G to be supported for board version C0
	 * and above of CN9K.
	 */
	if (roc_model_is_cn96_a0() || roc_model_is_cn95_a0()) {
		dev->speed_capa &= ~(uint64_t)ETH_LINK_SPEED_50G;
		dev->speed_capa &= ~(uint64_t)ETH_LINK_SPEED_100G;
	}

	dev->hwcap = 0;

	/* Update HW erratas */
	if (roc_model_is_cn96_a0() || roc_model_is_cn95_a0())
		dev->cq_min_4k = 1;
	return 0;
}

static const struct rte_pci_id cn9k_pci_nix_map[] = {
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn9k_pci_nix = {
	.id_table = cn9k_pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = cn9k_nix_probe,
	.remove = cn9k_nix_remove,
};

RTE_PMD_REGISTER_PCI(net_cn9k, cn9k_pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_cn9k, cn9k_pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_cn9k, "vfio-pci");
