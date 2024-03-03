/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_eswitch.h>

#define CNXK_NIX_DEF_SQ_COUNT 512

static int
cnxk_eswitch_dev_remove(struct rte_pci_device *pci_dev)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	int rc = 0;

	PLT_SET_USED(pci_dev);
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		rc = -EINVAL;
		goto exit;
	}

	rte_free(eswitch_dev);
exit:
	return rc;
}

int
cnxk_eswitch_nix_rsrc_start(struct cnxk_eswitch_dev *eswitch_dev)
{
	int rc;

	/* Enable Rx in NPC */
	rc = roc_nix_npc_rx_ena_dis(&eswitch_dev->nix, true);
	if (rc) {
		plt_err("Failed to enable NPC rx %d", rc);
		goto done;
	}

	rc = roc_npc_mcam_enable_all_entries(&eswitch_dev->npc, 1);
	if (rc) {
		plt_err("Failed to enable NPC entries %d", rc);
		goto done;
	}

done:
	return 0;
}

int
cnxk_eswitch_txq_start(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_sq *sq = &eswitch_dev->txq[qid].sqs;
	int rc = -EINVAL;

	if (eswitch_dev->txq[qid].state == CNXK_ESWITCH_QUEUE_STATE_STARTED)
		return 0;

	if (eswitch_dev->txq[qid].state != CNXK_ESWITCH_QUEUE_STATE_CONFIGURED) {
		plt_err("Eswitch txq %d not configured yet", qid);
		goto done;
	}

	rc = roc_nix_sq_ena_dis(sq, true);
	if (rc) {
		plt_err("Failed to enable sq aura fc, txq=%u, rc=%d", qid, rc);
		goto done;
	}

	eswitch_dev->txq[qid].state = CNXK_ESWITCH_QUEUE_STATE_STARTED;
done:
	return rc;
}

int
cnxk_eswitch_txq_stop(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_sq *sq = &eswitch_dev->txq[qid].sqs;
	int rc = -EINVAL;

	if (eswitch_dev->txq[qid].state == CNXK_ESWITCH_QUEUE_STATE_STOPPED ||
	    eswitch_dev->txq[qid].state == CNXK_ESWITCH_QUEUE_STATE_RELEASED)
		return 0;

	if (eswitch_dev->txq[qid].state != CNXK_ESWITCH_QUEUE_STATE_STARTED) {
		plt_err("Eswitch txq %d not started", qid);
		goto done;
	}

	rc = roc_nix_sq_ena_dis(sq, false);
	if (rc) {
		plt_err("Failed to disable sqb aura fc, txq=%u, rc=%d", qid, rc);
		goto done;
	}

	eswitch_dev->txq[qid].state = CNXK_ESWITCH_QUEUE_STATE_STOPPED;
done:
	return rc;
}

int
cnxk_eswitch_rxq_start(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_rq *rq = &eswitch_dev->rxq[qid].rqs;
	int rc = -EINVAL;

	if (eswitch_dev->rxq[qid].state == CNXK_ESWITCH_QUEUE_STATE_STARTED)
		return 0;

	if (eswitch_dev->rxq[qid].state != CNXK_ESWITCH_QUEUE_STATE_CONFIGURED) {
		plt_err("Eswitch rxq %d not configured yet", qid);
		goto done;
	}

	rc = roc_nix_rq_ena_dis(rq, true);
	if (rc) {
		plt_err("Failed to enable rxq=%u, rc=%d", qid, rc);
		goto done;
	}

	eswitch_dev->rxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_STARTED;
done:
	return rc;
}

int
cnxk_eswitch_rxq_stop(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_rq *rq = &eswitch_dev->rxq[qid].rqs;
	int rc = -EINVAL;

	if (eswitch_dev->rxq[qid].state == CNXK_ESWITCH_QUEUE_STATE_STOPPED ||
	    eswitch_dev->rxq[qid].state == CNXK_ESWITCH_QUEUE_STATE_RELEASED)
		return 0;

	if (eswitch_dev->rxq[qid].state != CNXK_ESWITCH_QUEUE_STATE_STARTED) {
		plt_err("Eswitch rxq %d not started", qid);
		goto done;
	}

	rc = roc_nix_rq_ena_dis(rq, false);
	if (rc) {
		plt_err("Failed to disable rxq=%u, rc=%d", qid, rc);
		goto done;
	}

	eswitch_dev->rxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_STOPPED;
done:
	return rc;
}

int
cnxk_eswitch_rxq_release(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc;

	if (eswitch_dev->rxq[qid].state == CNXK_ESWITCH_QUEUE_STATE_RELEASED)
		return 0;

	/* Cleanup ROC SQ */
	rq = &eswitch_dev->rxq[qid].rqs;
	rc = roc_nix_rq_fini(rq);
	if (rc) {
		plt_err("Failed to cleanup sq, rc=%d", rc);
		goto fail;
	}

	eswitch_dev->rxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_RELEASED;

	/* Cleanup ROC CQ */
	cq = &eswitch_dev->cxq[qid].cqs;
	rc = roc_nix_cq_fini(cq);
	if (rc) {
		plt_err("Failed to cleanup cq, rc=%d", rc);
		goto fail;
	}

	eswitch_dev->cxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_RELEASED;
fail:
	return rc;
}

int
cnxk_eswitch_rxq_setup(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid, uint16_t nb_desc,
		       const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp)
{
	struct roc_nix *nix = &eswitch_dev->nix;
	struct rte_mempool *lpb_pool = mp;
	struct rte_mempool_ops *ops;
	const char *platform_ops;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	uint16_t first_skip;
	int rc = -EINVAL;

	if (eswitch_dev->rxq[qid].state != CNXK_ESWITCH_QUEUE_STATE_RELEASED ||
	    eswitch_dev->cxq[qid].state != CNXK_ESWITCH_QUEUE_STATE_RELEASED) {
		plt_err("Queue %d is in invalid state %d, cannot be setup", qid,
			eswitch_dev->txq[qid].state);
		goto fail;
	}

	RTE_SET_USED(rx_conf);
	platform_ops = rte_mbuf_platform_mempool_ops();
	/* This driver needs cnxk_npa mempool ops to work */
	ops = rte_mempool_get_ops(lpb_pool->ops_index);
	if (strncmp(ops->name, platform_ops, RTE_MEMPOOL_OPS_NAMESIZE)) {
		plt_err("mempool ops should be of cnxk_npa type");
		goto fail;
	}

	if (lpb_pool->pool_id == 0) {
		plt_err("Invalid pool_id");
		goto fail;
	}

	/* Setup ROC CQ */
	cq = &eswitch_dev->cxq[qid].cqs;
	memset(cq, 0, sizeof(struct roc_nix_cq));
	cq->qid = qid;
	cq->nb_desc = nb_desc;
	rc = roc_nix_cq_init(nix, cq);
	if (rc) {
		plt_err("Failed to init roc cq for rq=%d, rc=%d", qid, rc);
		goto fail;
	}
	eswitch_dev->cxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_CONFIGURED;

	/* Setup ROC RQ */
	rq = &eswitch_dev->rxq[qid].rqs;
	memset(rq, 0, sizeof(struct roc_nix_rq));
	rq->qid = qid;
	rq->cqid = cq->qid;
	rq->aura_handle = lpb_pool->pool_id;
	rq->flow_tag_width = 32;
	rq->sso_ena = false;

	/* Calculate first mbuf skip */
	first_skip = (sizeof(struct rte_mbuf));
	first_skip += RTE_PKTMBUF_HEADROOM;
	first_skip += rte_pktmbuf_priv_size(lpb_pool);
	rq->first_skip = first_skip;
	rq->later_skip = sizeof(struct rte_mbuf) + rte_pktmbuf_priv_size(lpb_pool);
	rq->lpb_size = lpb_pool->elt_size;
	if (roc_errata_nix_no_meta_aura())
		rq->lpb_drop_ena = true;

	rc = roc_nix_rq_init(nix, rq, true);
	if (rc) {
		plt_err("Failed to init roc rq for rq=%d, rc=%d", qid, rc);
		goto cq_fini;
	}
	eswitch_dev->rxq[qid].state = CNXK_ESWITCH_QUEUE_STATE_CONFIGURED;

	return 0;
cq_fini:
	rc |= roc_nix_cq_fini(cq);
fail:
	return rc;
}

int
cnxk_eswitch_txq_release(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid)
{
	struct roc_nix_sq *sq;
	int rc = 0;

	if (eswitch_dev->txq[qid].state == CNXK_ESWITCH_QUEUE_STATE_RELEASED)
		return 0;

	/* Cleanup ROC SQ */
	sq = &eswitch_dev->txq[qid].sqs;
	rc = roc_nix_sq_fini(sq);
	if (rc) {
		plt_err("Failed to cleanup sq, rc=%d", rc);
		goto fail;
	}

	eswitch_dev->txq[qid].state = CNXK_ESWITCH_QUEUE_STATE_RELEASED;
fail:
	return rc;
}

int
cnxk_eswitch_txq_setup(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid, uint16_t nb_desc,
		       const struct rte_eth_txconf *tx_conf)
{
	struct roc_nix_sq *sq;
	int rc = 0;

	if (eswitch_dev->txq[qid].state != CNXK_ESWITCH_QUEUE_STATE_RELEASED) {
		plt_err("Queue %d is in invalid state %d, cannot be setup", qid,
			eswitch_dev->txq[qid].state);
		rc = -EINVAL;
		goto fail;
	}
	RTE_SET_USED(tx_conf);
	/* Setup ROC SQ */
	sq = &eswitch_dev->txq[qid].sqs;
	memset(sq, 0, sizeof(struct roc_nix_sq));
	sq->qid = qid;
	sq->nb_desc = nb_desc;
	sq->max_sqe_sz = NIX_MAXSQESZ_W8;
	if (sq->nb_desc >= CNXK_NIX_DEF_SQ_COUNT)
		sq->fc_hyst_bits = 0x1;

	rc = roc_nix_sq_init(&eswitch_dev->nix, sq);
	if (rc)
		plt_err("Failed to init sq=%d, rc=%d", qid, rc);

	eswitch_dev->txq[qid].state = CNXK_ESWITCH_QUEUE_STATE_CONFIGURED;

fail:
	return rc;
}

static int
cnxk_eswitch_dev_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	const struct rte_memzone *mz = NULL;
	int rc = -ENOMEM;

	RTE_SET_USED(pci_drv);

	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		rc = roc_plt_init();
		if (rc) {
			plt_err("Failed to initialize platform model, rc=%d", rc);
			return rc;
		}

		if (rte_eal_process_type() != RTE_PROC_PRIMARY)
			return 0;

		mz = rte_memzone_reserve_aligned(CNXK_REP_ESWITCH_DEV_MZ, sizeof(*eswitch_dev),
						 SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);
		if (mz == NULL) {
			plt_err("Failed to reserve a memzone");
			goto fail;
		}

		eswitch_dev = mz->addr;
		eswitch_dev->pci_dev = pci_dev;
	}

	/* Spinlock for synchronization between representors traffic and control
	 * messages
	 */
	rte_spinlock_init(&eswitch_dev->rep_lock);

	return rc;
fail:
	return rc;
}

static const struct rte_pci_id cnxk_eswitch_pci_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_RVU_ESWITCH_PF)},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_eswitch_pci = {
	.id_table = cnxk_eswitch_pci_map,
	.drv_flags =
		RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA | RTE_PCI_DRV_PROBE_AGAIN,
	.probe = cnxk_eswitch_dev_probe,
	.remove = cnxk_eswitch_dev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_eswitch, cnxk_eswitch_pci);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_eswitch, cnxk_eswitch_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_eswitch, "vfio-pci");
