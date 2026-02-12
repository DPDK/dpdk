/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_thash.h>

#include <cnxk_eswitch.h>
#include <cnxk_rep.h>

#define CNXK_NIX_DEF_SQ_COUNT 512

int
cnxk_eswitch_representor_id(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func,
			    uint16_t *rep_id)
{
	struct cnxk_esw_repr_hw_info *repr_info;
	int rc = 0;

	repr_info = cnxk_eswitch_representor_hw_info(eswitch_dev, hw_func);
	if (!repr_info) {
		plt_warn("Failed to get representor group for %x", hw_func);
		rc = -ENOENT;
		goto fail;
	}

	*rep_id = repr_info->rep_id;

	return 0;
fail:
	return rc;
}

struct cnxk_esw_repr_hw_info *
cnxk_eswitch_representor_hw_info(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func)
{
	struct cnxk_eswitch_devargs *esw_da;
	int i, j;

	if (!eswitch_dev)
		return NULL;

	/* Traversing the initialized represented list */
	for (i = 0; i < eswitch_dev->nb_esw_da; i++) {
		esw_da = &eswitch_dev->esw_da[i];
		for (j = 0; j < esw_da->nb_repr_ports; j++) {
			if (esw_da->repr_hw_info[j].hw_func == hw_func)
				return &esw_da->repr_hw_info[j];
		}
	}
	return NULL;
}

static int
eswitch_hw_rsrc_cleanup(struct cnxk_eswitch_dev *eswitch_dev, struct rte_pci_device *pci_dev)
{
	struct roc_nix *nix;
	int rc = 0;

	nix = &eswitch_dev->nix;

	roc_nix_unregister_queue_irqs(nix);
	roc_nix_tm_fini(nix);
	rc = roc_nix_lf_free(nix);
	if (rc) {
		plt_err("Failed to cleanup sq, rc %d", rc);
		goto exit;
	}

	/* Check if this device is hosting common resource */
	nix = roc_idev_npa_nix_get();
	if (!nix || nix->pci_dev != pci_dev) {
		rc = 0;
		goto exit;
	}

	/* Try nix fini now */
	rc = roc_nix_dev_fini(nix);
	if (rc == -EAGAIN) {
		plt_info("Common resource in use by other devices %s", pci_dev->name);
		goto exit;
	} else if (rc) {
		plt_err("Failed in nix dev fini, rc=%d", rc);
		goto exit;
	}

	rte_free(eswitch_dev->txq);
	rte_free(eswitch_dev->rxq);
	rte_free(eswitch_dev->cxq);

exit:
	return rc;
}

static int
cnxk_eswitch_dev_remove(struct rte_pci_device *pci_dev)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	int rc = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		rc = -EINVAL;
		goto exit;
	}

	/* Remove representor devices associated with PF */
	if (eswitch_dev->repr_cnt.nb_repr_created) {
		/* Exiting the rep msg ctrl thread */
		if (eswitch_dev->start_ctrl_msg_thrd) {
			uint32_t sunlen;
			struct sockaddr_un sun = {0};
			int sock_fd = 0;

			eswitch_dev->start_ctrl_msg_thrd = false;
			if (!eswitch_dev->client_connected) {
				plt_esw_dbg("Establishing connection for teardown");
				sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
				if (sock_fd == -1) {
					plt_err("Failed to open socket. err %d", -errno);
					return -errno;
				}
				sun.sun_family = AF_UNIX;
				sunlen = sizeof(struct sockaddr_un);
				strncpy(sun.sun_path, CNXK_ESWITCH_CTRL_MSG_SOCK_PATH,
					sizeof(sun.sun_path) - 1);

				if (connect(sock_fd, (struct sockaddr *)&sun, sunlen) < 0) {
					plt_err("Failed to connect socket: %s, err %d",
						CNXK_ESWITCH_CTRL_MSG_SOCK_PATH, errno);
					close(sock_fd);
					return -errno;
				}
			}
			rte_thread_join(eswitch_dev->rep_ctrl_msg_thread, NULL);
			if (!eswitch_dev->client_connected)
				close(sock_fd);
		}

		if (eswitch_dev->repte_msg_proc.start_thread) {
			eswitch_dev->repte_msg_proc.start_thread = false;
			pthread_cond_signal(&eswitch_dev->repte_msg_proc.repte_msg_cond);
			rte_thread_join(eswitch_dev->repte_msg_proc.repte_msg_thread, NULL);
			pthread_mutex_destroy(&eswitch_dev->repte_msg_proc.mutex);
			pthread_cond_destroy(&eswitch_dev->repte_msg_proc.repte_msg_cond);
		}

		/* Remove representor devices associated with PF */
		cnxk_rep_dev_remove(eswitch_dev);
	}

	/* Cleanup NPC rxtx flow rules */
	cnxk_eswitch_flow_rules_remove_list(eswitch_dev, &eswitch_dev->esw_flow_list,
					    eswitch_dev->npc.pf_func);

	/* Cleanup HW resources */
	eswitch_hw_rsrc_cleanup(eswitch_dev, pci_dev);

	rte_free(eswitch_dev);
exit:
	return rc;
}

int
cnxk_eswitch_nix_rsrc_start(struct cnxk_eswitch_dev *eswitch_dev)
{
	int rc;

	/* Install eswitch PF mcam rules */
	rc = cnxk_eswitch_pfvf_flow_rules_install(eswitch_dev, false);
	if (rc) {
		plt_err("Failed to install rxtx rules, rc %d", rc);
		goto done;
	}

	/* Configure TPID for Eswitch PF LFs */
	rc = roc_eswitch_nix_vlan_tpid_set(&eswitch_dev->nix, ROC_NIX_VLAN_TYPE_OUTER,
					   CNXK_ESWITCH_VLAN_TPID, false);
	if (rc) {
		plt_err("Failed to configure tpid, rc %d", rc);
		goto done;
	}

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
	sq->max_sqe_sz = NIX_MAXSQESZ_W16;
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
nix_lf_setup(struct cnxk_eswitch_dev *eswitch_dev)
{
	uint16_t nb_rxq, nb_txq, nb_cq;
	struct roc_nix_fc_cfg fc_cfg;
	struct roc_nix *nix;
	uint64_t rx_cfg;
	void *qs;
	int rc;

	/* Initialize base roc nix */
	nix = &eswitch_dev->nix;
	nix->pci_dev = eswitch_dev->pci_dev;
	nix->hw_vlan_ins = true;
	nix->reta_sz = ROC_NIX_RSS_RETA_SZ_256;
	rc = roc_nix_dev_init(nix);
	if (rc) {
		plt_err("Failed to init nix eswitch device, rc=%d(%s)", rc, roc_error_msg_get(rc));
		goto fail;
	}

	/* Get the representors count */
	rc = roc_nix_max_rep_count(&eswitch_dev->nix);
	if (rc) {
		plt_err("Failed to get rep cnt, rc=%d(%s)", rc, roc_error_msg_get(rc));
		goto free_cqs;
	}
	eswitch_dev->repr_cnt.max_repr = eswitch_dev->nix.rep_cnt;

	/* Allocating an NIX LF */
	nb_rxq = CNXK_ESWITCH_MAX_RXQ;
	nb_txq = CNXK_ESWITCH_MAX_TXQ;
	nb_cq = CNXK_ESWITCH_MAX_RXQ;
	rx_cfg = ROC_NIX_LF_RX_CFG_DIS_APAD;
	rc = roc_nix_lf_alloc(nix, nb_rxq, nb_txq, rx_cfg);
	if (rc) {
		plt_err("lf alloc failed = %s(%d)", roc_error_msg_get(rc), rc);
		goto dev_fini;
	}

	if (nb_rxq) {
		/* Allocate memory for eswitch rq's and cq's */
		qs = plt_zmalloc(sizeof(struct cnxk_eswitch_rxq) * nb_rxq, 0);
		if (!qs) {
			plt_err("Failed to alloc eswitch rxq");
			goto lf_free;
		}
		eswitch_dev->rxq = qs;
	}

	if (nb_txq) {
		/* Allocate memory for roc sq's */
		qs = plt_zmalloc(sizeof(struct cnxk_eswitch_txq) * nb_txq, 0);
		if (!qs) {
			plt_err("Failed to alloc eswitch txq");
			goto free_rqs;
		}
		eswitch_dev->txq = qs;
	}

	if (nb_cq) {
		qs = plt_zmalloc(sizeof(struct cnxk_eswitch_cxq) * nb_cq, 0);
		if (!qs) {
			plt_err("Failed to alloc eswitch cxq");
			goto free_sqs;
		}
		eswitch_dev->cxq = qs;
	}

	eswitch_dev->nb_rxq = nb_rxq;
	eswitch_dev->nb_txq = nb_txq;

	/* Re-enable NIX LF error interrupts */
	roc_nix_err_intr_ena_dis(nix, true);
	roc_nix_ras_intr_ena_dis(nix, true);

	rc = roc_nix_lso_fmt_setup(nix);
	if (rc) {
		plt_err("lso setup failed = %s(%d)", roc_error_msg_get(rc), rc);
		goto free_cqs;
	}

	rc = roc_nix_switch_hdr_set(nix, 0, 0, 0, 0);
	if (rc) {
		plt_err("switch hdr set failed = %s(%d)", roc_error_msg_get(rc), rc);
		goto free_cqs;
	}

	rc = roc_nix_tm_init(nix);
	if (rc) {
		plt_err("tm failed = %s(%d)", roc_error_msg_get(rc), rc);
		goto free_cqs;
	}

	/* Register queue IRQs */
	rc = roc_nix_register_queue_irqs(nix);
	if (rc) {
		plt_err("Failed to register queue interrupts rc=%d", rc);
		goto tm_fini;
	}

	/* Enable default tree */
	rc = roc_nix_tm_hierarchy_enable(nix, ROC_NIX_TM_DEFAULT, false);
	if (rc) {
		plt_err("tm default hierarchy enable failed = %s(%d)", roc_error_msg_get(rc), rc);
		goto q_irq_fini;
	}

	memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
	fc_cfg.rxchan_cfg.enable = false;
	rc = roc_nix_fc_config_set(nix, &fc_cfg);
	if (rc) {
		plt_err("Failed to setup flow control, rc=%d(%s)", rc, roc_error_msg_get(rc));
		goto q_irq_fini;
	}

	roc_nix_fc_mode_get(nix);

	return rc;
q_irq_fini:
	roc_nix_unregister_queue_irqs(nix);
tm_fini:
	roc_nix_tm_fini(nix);
free_cqs:
	rte_free(eswitch_dev->cxq);
free_sqs:
	rte_free(eswitch_dev->txq);
free_rqs:
	rte_free(eswitch_dev->rxq);
lf_free:
	roc_nix_lf_free(nix);
dev_fini:
	roc_nix_dev_fini(nix);
fail:
	return rc;
}

static int
eswitch_hw_rsrc_setup(struct cnxk_eswitch_dev *eswitch_dev, struct rte_pci_device *pci_dev)
{
	struct roc_nix *nix;
	int rc;

	nix = &eswitch_dev->nix;
	rc = nix_lf_setup(eswitch_dev);
	if (rc) {
		plt_err("Failed to setup hw rsrc, rc=%d(%s)", rc, roc_error_msg_get(rc));
		goto fail;
	}

	/* Initialize roc npc */
	eswitch_dev->npc.roc_nix = nix;
	eswitch_dev->npc.flow_max_priority = 3;
	eswitch_dev->npc.flow_prealloc_size = 1;
	rc = roc_npc_init(&eswitch_dev->npc);
	if (rc)
		goto rsrc_cleanup;

	/* List for eswitch default flows */
	TAILQ_INIT(&eswitch_dev->esw_flow_list);

	return rc;
rsrc_cleanup:
	eswitch_hw_rsrc_cleanup(eswitch_dev, pci_dev);
fail:
	return rc;
}

int
cnxk_eswitch_representor_info_get(struct cnxk_eswitch_dev *eswitch_dev,
				  struct rte_eth_representor_info *info)
{
	struct cnxk_eswitch_devargs *esw_da;
	int rc = 0, n_entries, i, j = 0, k = 0;

	for (i = 0; i < eswitch_dev->nb_esw_da; i++) {
		for (j = 0; j < eswitch_dev->esw_da[i].nb_repr_ports; j++)
			k++;
	}
	n_entries = k;

	if (info == NULL)
		goto out;

	if ((uint32_t)n_entries > info->nb_ranges_alloc)
		n_entries = info->nb_ranges_alloc;

	k = 0;
	info->controller = 0;
	info->pf = 0;
	for (i = 0; i < eswitch_dev->nb_esw_da; i++) {
		esw_da = &eswitch_dev->esw_da[i];
		info->ranges[k].type = esw_da->da.type;
		switch (esw_da->da.type) {
		case RTE_ETH_REPRESENTOR_PF:
			info->ranges[k].controller = 0;
			info->ranges[k].pf = esw_da->repr_hw_info[0].pfvf;
			info->ranges[k].vf = 0;
			info->ranges[k].id_base = info->ranges[i].pf;
			info->ranges[k].id_end = info->ranges[i].pf;
			snprintf(info->ranges[k].name, sizeof(info->ranges[k].name), "pf%d",
				 info->ranges[k].pf);
			k++;
			break;
		case RTE_ETH_REPRESENTOR_VF:
			for (j = 0; j < esw_da->nb_repr_ports; j++) {
				info->ranges[k].controller = 0;
				info->ranges[k].pf = esw_da->da.ports[0];
				info->ranges[k].vf = esw_da->repr_hw_info[j].pfvf;
				info->ranges[k].id_base = esw_da->repr_hw_info[j].port_id;
				info->ranges[k].id_end = esw_da->repr_hw_info[j].port_id;
				snprintf(info->ranges[k].name, sizeof(info->ranges[k].name),
					 "pf%dvf%d", info->ranges[k].pf, info->ranges[k].vf);
				k++;
			}
			break;
		default:
			plt_err("Invalid type %d", esw_da->da.type);
			rc = 0;
			goto fail;
		};
	}
	info->nb_ranges = k;
fail:
	return rc;
out:
	return n_entries;
}

static int
cnxk_eswitch_dev_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	const struct rte_memzone *mz = NULL;
	uint16_t num_reps;
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

		rc = eswitch_hw_rsrc_setup(eswitch_dev, pci_dev);
		if (rc) {
			plt_err("Failed to setup hw rsrc, rc=%d(%s)", rc, roc_error_msg_get(rc));
			goto free_mem;
		}
	}

	if (pci_dev->device.devargs) {
		rc = cnxk_eswitch_repr_devargs(pci_dev, eswitch_dev);
		if (rc)
			goto rsrc_cleanup;
	}

	if (eswitch_dev->repr_cnt.nb_repr_created > eswitch_dev->repr_cnt.max_repr) {
		plt_err("Representors to be created %d can be greater than max allowed %d",
			eswitch_dev->repr_cnt.nb_repr_created, eswitch_dev->repr_cnt.max_repr);
		rc = -EINVAL;
		goto rsrc_cleanup;
	}

	num_reps = eswitch_dev->repr_cnt.nb_repr_created;
	if (!num_reps) {
		plt_err("No representors enabled");
		goto fail;
	}

	plt_esw_dbg("Max no of reps %d reps to be created %d Eswtch pfunc %x",
		    eswitch_dev->repr_cnt.max_repr, eswitch_dev->repr_cnt.nb_repr_created,
		    roc_nix_get_pf_func(&eswitch_dev->nix));

	/* Probe representor ports */
	rc = cnxk_rep_dev_probe(pci_dev, eswitch_dev);
	if (rc) {
		plt_err("Failed to probe representor ports");
		goto rsrc_cleanup;
	}

	/* Spinlock for synchronization between representors traffic and control
	 * messages
	 */
	rte_spinlock_init(&eswitch_dev->rep_lock);

	return rc;
rsrc_cleanup:
	eswitch_hw_rsrc_cleanup(eswitch_dev, pci_dev);
free_mem:
	rte_memzone_free(mz);
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
