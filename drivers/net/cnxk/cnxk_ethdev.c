/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_ethdev.h>

#include <rte_eventdev.h>

#define CNXK_NIX_CQ_INL_CLAMP_MAX (64UL * 1024UL)

static inline uint64_t
nix_get_rx_offload_capa(struct cnxk_eth_dev *dev)
{
	uint64_t capa = CNXK_NIX_RX_OFFLOAD_CAPA;

	if (roc_nix_is_vf_or_sdp(&dev->nix) ||
	    dev->npc.switch_header_type == ROC_PRIV_FLAGS_HIGIG)
		capa &= ~RTE_ETH_RX_OFFLOAD_TIMESTAMP;

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
	speed_capa = RTE_ETH_LINK_SPEED_FIXED;
	if (!roc_nix_is_vf_or_sdp(&dev->nix) && !roc_nix_is_lbk(&dev->nix)) {
		speed_capa |= RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_10G |
			      RTE_ETH_LINK_SPEED_25G | RTE_ETH_LINK_SPEED_40G |
			      RTE_ETH_LINK_SPEED_50G | RTE_ETH_LINK_SPEED_100G;
	}

	return speed_capa;
}

static uint32_t
nix_inl_cq_sz_clamp_up(struct roc_nix *nix, struct rte_mempool *mp,
		       uint32_t nb_desc)
{
	struct roc_nix_rq *inl_rq;
	uint64_t limit;

	/* For CN10KB and above, LBP needs minimum CQ size */
	if (!roc_errata_cpt_hang_on_x2p_bp())
		return RTE_MAX(nb_desc, (uint32_t)4096);

	/* CQ should be able to hold all buffers in first pass RQ's aura
	 * this RQ's aura.
	 */
	inl_rq = roc_nix_inl_dev_rq(nix);
	if (!inl_rq) {
		/* This itself is going to be inline RQ's aura */
		limit = roc_npa_aura_op_limit_get(mp->pool_id);
	} else {
		limit = roc_npa_aura_op_limit_get(inl_rq->aura_handle);
		/* Also add this RQ's aura if it is different */
		if (inl_rq->aura_handle != mp->pool_id)
			limit += roc_npa_aura_op_limit_get(mp->pool_id);
	}
	nb_desc = PLT_MAX(limit + 1, nb_desc);
	if (nb_desc > CNXK_NIX_CQ_INL_CLAMP_MAX) {
		plt_warn("Could not setup CQ size to accommodate"
			 " all buffers in related auras (%" PRIu64 ")",
			 limit);
		nb_desc = CNXK_NIX_CQ_INL_CLAMP_MAX;
	}
	return nb_desc;
}

int
cnxk_nix_inb_mode_set(struct cnxk_eth_dev *dev, bool use_inl_dev)
{
	struct roc_nix *nix = &dev->nix;

	if (dev->inb.inl_dev == use_inl_dev)
		return 0;

	plt_nix_dbg("Security sessions(%u) still active, inl=%u!!!",
		    dev->inb.nb_sess, !!dev->inb.inl_dev);

	/* Change the mode */
	dev->inb.inl_dev = use_inl_dev;

	/* Update RoC for NPC rule insertion */
	roc_nix_inb_mode_set(nix, use_inl_dev);

	/* Setup lookup mem */
	return cnxk_nix_lookup_mem_sa_base_set(dev);
}

static int
nix_security_setup(struct cnxk_eth_dev *dev)
{
	struct roc_nix *nix = &dev->nix;
	int i, rc = 0;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		/* Setup minimum SA table when inline device is used */
		nix->ipsec_in_min_spi = dev->inb.no_inl_dev ? dev->inb.min_spi : 0;
		nix->ipsec_in_max_spi = dev->inb.no_inl_dev ? dev->inb.max_spi : 1;

		/* Setup Inline Inbound */
		rc = roc_nix_inl_inb_init(nix);
		if (rc) {
			plt_err("Failed to initialize nix inline inb, rc=%d",
				rc);
			return rc;
		}

		/* By default pick using inline device for poll mode.
		 * Will be overridden when event mode rq's are setup.
		 */
		cnxk_nix_inb_mode_set(dev, true);

		/* Allocate memory to be used as dptr for CPT ucode
		 * WRITE_SA op.
		 */
		dev->inb.sa_dptr =
			plt_zmalloc(ROC_NIX_INL_OT_IPSEC_INB_HW_SZ, 0);
		if (!dev->inb.sa_dptr) {
			plt_err("Couldn't allocate memory for SA dptr");
			rc = -ENOMEM;
			goto cleanup;
		}
	}

	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY ||
	    dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		struct plt_bitmap *bmap;
		size_t bmap_sz;
		void *mem;

		/* Setup enough descriptors for all tx queues */
		nix->outb_nb_desc = dev->outb.nb_desc;
		nix->outb_nb_crypto_qs = dev->outb.nb_crypto_qs;

		/* Setup Inline Outbound */
		rc = roc_nix_inl_outb_init(nix);
		if (rc) {
			plt_err("Failed to initialize nix inline outb, rc=%d",
				rc);
			goto sa_dptr_free;
		}

		dev->outb.lf_base = roc_nix_inl_outb_lf_base_get(nix);

		/* Skip the rest if DEV_TX_OFFLOAD_SECURITY is not enabled */
		if (!(dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY))
			return 0;

		/* Allocate memory to be used as dptr for CPT ucode
		 * WRITE_SA op.
		 */
		dev->outb.sa_dptr =
			plt_zmalloc(ROC_NIX_INL_OT_IPSEC_OUTB_HW_SZ, 0);
		if (!dev->outb.sa_dptr) {
			plt_err("Couldn't allocate memory for SA dptr");
			rc = -ENOMEM;
			goto sa_dptr_free;
		}

		rc = -ENOMEM;
		/* Allocate a bitmap to alloc and free sa indexes */
		bmap_sz = plt_bitmap_get_memory_footprint(dev->outb.max_sa);
		mem = plt_zmalloc(bmap_sz, PLT_CACHE_LINE_SIZE);
		if (mem == NULL) {
			plt_err("Outbound SA bmap alloc failed");

			rc |= roc_nix_inl_outb_fini(nix);
			goto sa_dptr_free;
		}

		rc = -EIO;
		bmap = plt_bitmap_init(dev->outb.max_sa, mem, bmap_sz);
		if (!bmap) {
			plt_err("Outbound SA bmap init failed");

			rc |= roc_nix_inl_outb_fini(nix);
			plt_free(mem);
			goto sa_dptr_free;
		}

		for (i = 0; i < dev->outb.max_sa; i++)
			plt_bitmap_set(bmap, i);

		dev->outb.sa_base = roc_nix_inl_outb_sa_base_get(nix);
		dev->outb.sa_bmap_mem = mem;
		dev->outb.sa_bmap = bmap;

		dev->outb.fc_sw_mem = plt_zmalloc(dev->outb.nb_crypto_qs *
							  RTE_CACHE_LINE_SIZE,
						  RTE_CACHE_LINE_SIZE);
		if (!dev->outb.fc_sw_mem) {
			plt_err("Outbound fc sw mem alloc failed");
			goto sa_bmap_free;
		}
	}
	return 0;

sa_bmap_free:
	plt_free(dev->outb.sa_bmap_mem);
sa_dptr_free:
	if (dev->inb.sa_dptr)
		plt_free(dev->inb.sa_dptr);
	if (dev->outb.sa_dptr)
		plt_free(dev->outb.sa_dptr);
cleanup:
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		rc |= roc_nix_inl_inb_fini(nix);
	return rc;
}

static int
nix_meter_fini(struct cnxk_eth_dev *dev)
{
	struct cnxk_meter_node *next_mtr = NULL;
	struct roc_nix_bpf_objs profs = {0};
	struct cnxk_meter_node *mtr = NULL;
	struct cnxk_mtr *fms = &dev->mtr;
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_rq *rq;
	uint32_t i;
	int rc = 0;

	RTE_TAILQ_FOREACH_SAFE(mtr, fms, next, next_mtr) {
		for (i = 0; i < mtr->rq_num; i++) {
			rq = &dev->rqs[mtr->rq_id[i]];
			rc |= roc_nix_bpf_ena_dis(nix, mtr->bpf_id, rq, false);
		}

		profs.level = mtr->level;
		profs.count = 1;
		profs.ids[0] = mtr->bpf_id;
		rc = roc_nix_bpf_free(nix, &profs, 1);

		if (rc)
			return rc;

		TAILQ_REMOVE(fms, mtr, next);
		plt_free(mtr);
	}
	return 0;
}

static int
nix_security_release(struct cnxk_eth_dev *dev)
{
	struct rte_eth_dev *eth_dev = dev->eth_dev;
	struct cnxk_eth_sec_sess *eth_sec, *tvar;
	struct roc_nix *nix = &dev->nix;
	int rc, ret = 0;

	/* Cleanup Inline inbound */
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		/* Destroy inbound sessions */
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->inb.list, entry, tvar)
			cnxk_eth_sec_ops.session_destroy(eth_dev,
							 eth_sec->sess);

		/* Clear lookup mem */
		cnxk_nix_lookup_mem_sa_base_clear(dev);

		rc = roc_nix_inl_inb_fini(nix);
		if (rc)
			plt_err("Failed to cleanup nix inline inb, rc=%d", rc);
		ret |= rc;

		cnxk_nix_lookup_mem_metapool_clear(dev);

		if (dev->inb.sa_dptr) {
			plt_free(dev->inb.sa_dptr);
			dev->inb.sa_dptr = NULL;
		}
	}

	/* Cleanup Inline outbound */
	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY ||
	    dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		/* Destroy outbound sessions */
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->outb.list, entry, tvar)
			cnxk_eth_sec_ops.session_destroy(eth_dev,
							 eth_sec->sess);

		rc = roc_nix_inl_outb_fini(nix);
		if (rc)
			plt_err("Failed to cleanup nix inline outb, rc=%d", rc);
		ret |= rc;

		plt_bitmap_free(dev->outb.sa_bmap);
		plt_free(dev->outb.sa_bmap_mem);
		dev->outb.sa_bmap = NULL;
		dev->outb.sa_bmap_mem = NULL;
		if (dev->outb.sa_dptr) {
			plt_free(dev->outb.sa_dptr);
			dev->outb.sa_dptr = NULL;
		}

		plt_free(dev->outb.fc_sw_mem);
		dev->outb.fc_sw_mem = NULL;
	}

	dev->inb.inl_dev = false;
	roc_nix_inb_mode_set(nix, false);
	dev->nb_rxq_sso = 0;
	dev->inb.nb_sess = 0;
	dev->outb.nb_sess = 0;
	return ret;
}

static void
nix_enable_mseg_on_jumbo(struct cnxk_eth_rxq_sp *rxq)
{
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	uint32_t buffsz;

	dev = rxq->dev;
	eth_dev = dev->eth_dev;

	/* Get rx buffer size */
	mbp_priv = rte_mempool_get_priv(rxq->qconf.mp);
	buffsz = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;

	if (eth_dev->data->mtu + (uint32_t)CNXK_NIX_L2_OVERHEAD > buffsz) {
		dev->rx_offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
		dev->tx_offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
	}
}

int
nix_recalc_mtu(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_dev_data *data = eth_dev->data;
	struct cnxk_eth_rxq_sp *rxq;
	int rc;

	rxq = ((struct cnxk_eth_rxq_sp *)data->rx_queues[0]) - 1;
	/* Setup scatter mode if needed by jumbo */
	nix_enable_mseg_on_jumbo(rxq);

	rc = cnxk_nix_mtu_set(eth_dev, data->mtu);
	if (rc)
		plt_err("Failed to set default MTU size, rc=%d", rc);

	return rc;
}

static int
nix_init_flow_ctrl_config(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	enum roc_nix_fc_mode fc_mode = ROC_NIX_FC_FULL;
	struct cnxk_fc_cfg *fc = &dev->fc_cfg;
	int rc;

	if (roc_nix_is_vf_or_sdp(&dev->nix) && !roc_nix_is_lbk(&dev->nix))
		return 0;

	/* To avoid Link credit deadlock on Ax, disable Tx FC if it's enabled */
	if (roc_model_is_cn96_ax() &&
	    dev->npc.switch_header_type != ROC_PRIV_FLAGS_HIGIG)
		fc_mode = ROC_NIX_FC_TX;

	/* By default enable flow control */
	rc = roc_nix_fc_mode_set(&dev->nix, fc_mode);
	if (rc)
		return rc;

	fc->mode = (fc_mode == ROC_NIX_FC_FULL) ? RTE_ETH_FC_FULL :
						  RTE_ETH_FC_TX_PAUSE;
	return rc;
}

static int
nix_update_flow_ctrl_config(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_fc_cfg *fc = &dev->fc_cfg;
	struct rte_eth_fc_conf fc_cfg = {0};

	if (roc_nix_is_sdp(&dev->nix))
		return 0;

	/* Don't do anything if PFC is enabled */
	if (dev->pfc_cfg.rx_pause_en || dev->pfc_cfg.tx_pause_en)
		return 0;

	fc_cfg.mode = fc->mode;

	/* To avoid Link credit deadlock on Ax, disable Tx FC if it's enabled */
	if (roc_model_is_cn96_ax() &&
	    dev->npc.switch_header_type != ROC_PRIV_FLAGS_HIGIG &&
	    (fc_cfg.mode == RTE_ETH_FC_FULL || fc_cfg.mode == RTE_ETH_FC_RX_PAUSE)) {
		fc_cfg.mode =
				(fc_cfg.mode == RTE_ETH_FC_FULL ||
				fc_cfg.mode == RTE_ETH_FC_TX_PAUSE) ?
				RTE_ETH_FC_TX_PAUSE : RTE_ETH_FC_NONE;
	}

	return cnxk_nix_flow_ctrl_set(eth_dev, &fc_cfg);
}

uint64_t
cnxk_nix_rxq_mbuf_setup(struct cnxk_eth_dev *dev)
{
	uint16_t port_id = dev->eth_dev->data->port_id;
	struct rte_mbuf mb_def;
	uint64_t *tmp;

	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) % 8 != 0);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, refcnt) -
				 offsetof(struct rte_mbuf, data_off) !=
			 2);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, nb_segs) -
				 offsetof(struct rte_mbuf, data_off) !=
			 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, port) -
				 offsetof(struct rte_mbuf, data_off) !=
			 6);
	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM +
			  (dev->ptp_en * CNXK_NIX_TIMESYNC_RX_OFFSET);
	mb_def.port = port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* Prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	tmp = (uint64_t *)&mb_def.rearm_data;

	return *tmp;
}

static inline uint8_t
nix_sq_max_sqe_sz(struct cnxk_eth_dev *dev)
{
	/*
	 * Maximum three segments can be supported with W8, Choose
	 * NIX_MAXSQESZ_W16 for multi segment offload.
	 */
	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		return NIX_MAXSQESZ_W16;
	else
		return NIX_MAXSQESZ_W8;
}

int
cnxk_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			uint16_t nb_desc, uint16_t fp_tx_q_sz,
			const struct rte_eth_txconf *tx_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct roc_nix *nix = &dev->nix;
	struct cnxk_eth_txq_sp *txq_sp;
	struct roc_nix_cq *cq;
	struct roc_nix_sq *sq;
	size_t txq_sz;
	int rc;

	/* Free memory prior to re-allocation if needed. */
	if (eth_dev->data->tx_queues[qid] != NULL) {
		plt_nix_dbg("Freeing memory prior to re-allocation %d", qid);
		dev_ops->tx_queue_release(eth_dev, qid);
		eth_dev->data->tx_queues[qid] = NULL;
	}

	/* When Tx Security offload is enabled, increase tx desc count by
	 * max possible outbound desc count.
	 */
	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY)
		nb_desc += dev->outb.nb_desc;

	/* Setup ROC SQ */
	sq = &dev->sqs[qid];
	sq->qid = qid;
	sq->nb_desc = nb_desc;
	sq->max_sqe_sz = nix_sq_max_sqe_sz(dev);

	if (nix->tx_compl_ena) {
		sq->cqid = sq->qid + dev->nb_rxq;
		sq->cq_ena = 1;
		cq = &dev->cqs[sq->cqid];
		cq->qid = sq->cqid;
		cq->nb_desc = nb_desc;
		rc = roc_nix_cq_init(&dev->nix, cq);
		if (rc) {
			plt_err("Failed to init cq=%d, rc=%d", cq->qid, rc);
			return rc;
		}
	}

	rc = roc_nix_sq_init(&dev->nix, sq);
	if (rc) {
		plt_err("Failed to init sq=%d, rc=%d", qid, rc);
		return rc;
	}

	rc = -ENOMEM;
	txq_sz = sizeof(struct cnxk_eth_txq_sp) + fp_tx_q_sz;
	txq_sp = plt_zmalloc(txq_sz, PLT_CACHE_LINE_SIZE);
	if (!txq_sp) {
		plt_err("Failed to alloc tx queue mem");
		rc |= roc_nix_sq_fini(sq);
		return rc;
	}

	txq_sp->dev = dev;
	txq_sp->qid = qid;
	txq_sp->qconf.conf.tx = *tx_conf;
	/* Queue config should reflect global offloads */
	txq_sp->qconf.conf.tx.offloads = dev->tx_offloads;
	txq_sp->qconf.nb_desc = nb_desc;

	plt_nix_dbg("sq=%d fc=%p offload=0x%" PRIx64 " lmt_addr=%p"
		    " nb_sqb_bufs=%d sqes_per_sqb_log2=%d",
		    qid, sq->fc, dev->tx_offloads, sq->lmt_addr,
		    sq->nb_sqb_bufs, sq->sqes_per_sqb_log2);

	/* Store start of fast path area */
	eth_dev->data->tx_queues[qid] = txq_sp + 1;
	eth_dev->data->tx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

void
cnxk_nix_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	void *txq = eth_dev->data->tx_queues[qid];
	struct cnxk_eth_txq_sp *txq_sp;
	struct cnxk_eth_dev *dev;
	struct roc_nix_sq *sq;
	int rc;

	if (!txq)
		return;

	txq_sp = cnxk_eth_txq_to_sp(txq);

	dev = txq_sp->dev;

	plt_nix_dbg("Releasing txq %u", qid);

	/* Cleanup ROC SQ */
	sq = &dev->sqs[qid];
	rc = roc_nix_sq_fini(sq);
	if (rc)
		plt_err("Failed to cleanup sq, rc=%d", rc);

	/* Finally free */
	plt_free(txq_sp);
}

static int
cnxk_nix_process_rx_conf(const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool **lpb_pool,
			 struct rte_mempool **spb_pool)
{
	struct rte_mempool *pool0;
	struct rte_mempool *pool1;
	struct rte_mempool **mp = rx_conf->rx_mempools;
	const char *platform_ops;
	struct rte_mempool_ops *ops;

	if (*lpb_pool ||
	    rx_conf->rx_nmempool != CNXK_NIX_NUM_POOLS_MAX) {
		plt_err("invalid arguments");
		return -EINVAL;
	}

	if (mp == NULL || mp[0] == NULL || mp[1] == NULL) {
		plt_err("invalid memory pools\n");
		return -EINVAL;
	}

	pool0 = mp[0];
	pool1 = mp[1];

	if (pool0->elt_size > pool1->elt_size) {
		*lpb_pool = pool0;
		*spb_pool = pool1;

	} else {
		*lpb_pool = pool1;
		*spb_pool = pool0;
	}

	if ((*spb_pool)->pool_id == 0) {
		plt_err("Invalid pool_id");
		return -EINVAL;
	}

	platform_ops = rte_mbuf_platform_mempool_ops();
	ops = rte_mempool_get_ops((*spb_pool)->ops_index);
	if (strncmp(ops->name, platform_ops, RTE_MEMPOOL_OPS_NAMESIZE)) {
		plt_err("mempool ops should be of cnxk_npa type");
		return -EINVAL;
	}

	plt_info("spb_pool:%s lpb_pool:%s lpb_len:%u spb_len:%u\n", (*spb_pool)->name,
		 (*lpb_pool)->name, (*lpb_pool)->elt_size, (*spb_pool)->elt_size);

	return 0;
}

int
cnxk_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			uint32_t nb_desc, uint16_t fp_rx_q_sz,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct cnxk_eth_rxq_sp *rxq_sp;
	struct rte_mempool_ops *ops;
	const char *platform_ops;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	uint16_t first_skip;
	int rc = -EINVAL;
	size_t rxq_sz;
	struct rte_mempool *lpb_pool = mp;
	struct rte_mempool *spb_pool = NULL;

	/* Sanity checks */
	if (rx_conf->rx_deferred_start == 1) {
		plt_err("Deferred Rx start is not supported");
		goto fail;
	}

	if (rx_conf->rx_nmempool > 0) {
		rc = cnxk_nix_process_rx_conf(rx_conf, &lpb_pool, &spb_pool);
		if (rc)
			goto fail;
	}

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

	/* Free memory prior to re-allocation if needed */
	if (eth_dev->data->rx_queues[qid] != NULL) {
		const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;

		plt_nix_dbg("Freeing memory prior to re-allocation %d", qid);
		dev_ops->rx_queue_release(eth_dev, qid);
		eth_dev->data->rx_queues[qid] = NULL;
	}

	/* Its a no-op when inline device is not used */
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY ||
	    dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY)
		roc_nix_inl_dev_xaq_realloc(lpb_pool->pool_id);

	/* Increase CQ size to Aura size to avoid CQ overflow and
	 * then CPT buffer leak.
	 */
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		nb_desc = nix_inl_cq_sz_clamp_up(nix, lpb_pool, nb_desc);

	/* Setup ROC CQ */
	cq = &dev->cqs[qid];
	cq->qid = qid;
	cq->nb_desc = nb_desc;
	rc = roc_nix_cq_init(&dev->nix, cq);
	if (rc) {
		plt_err("Failed to init roc cq for rq=%d, rc=%d", qid, rc);
		goto fail;
	}

	/* Setup ROC RQ */
	rq = &dev->rqs[qid];
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
		rq->lpb_drop_ena = !(dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY);

	/* Enable Inline IPSec on RQ, will not be used for Poll mode */
	if (roc_nix_inl_inb_is_enabled(nix))
		rq->ipsech_ena = true;

	if (spb_pool) {
		rq->spb_ena = 1;
		rq->spb_aura_handle = spb_pool->pool_id;
		rq->spb_size = spb_pool->elt_size;
	}

	rc = roc_nix_rq_init(&dev->nix, rq, !!eth_dev->data->dev_started);
	if (rc) {
		plt_err("Failed to init roc rq for rq=%d, rc=%d", qid, rc);
		goto cq_fini;
	}

	/* Allocate and setup fast path rx queue */
	rc = -ENOMEM;
	rxq_sz = sizeof(struct cnxk_eth_rxq_sp) + fp_rx_q_sz;
	rxq_sp = plt_zmalloc(rxq_sz, PLT_CACHE_LINE_SIZE);
	if (!rxq_sp) {
		plt_err("Failed to alloc rx queue for rq=%d", qid);
		goto rq_fini;
	}

	/* Setup slow path fields */
	rxq_sp->dev = dev;
	rxq_sp->qid = qid;
	rxq_sp->qconf.conf.rx = *rx_conf;
	/* Queue config should reflect global offloads */
	rxq_sp->qconf.conf.rx.offloads = dev->rx_offloads;
	rxq_sp->qconf.nb_desc = nb_desc;
	rxq_sp->qconf.mp = lpb_pool;
	rxq_sp->tc = 0;
	rxq_sp->tx_pause = (dev->fc_cfg.mode == RTE_ETH_FC_FULL ||
			    dev->fc_cfg.mode == RTE_ETH_FC_TX_PAUSE);

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		/* Pass a tagmask used to handle error packets in inline device.
		 * Ethdev rq's tag_mask field will be overwritten later
		 * when sso is setup.
		 */
		rq->tag_mask =
			0x0FF00000 | ((uint32_t)RTE_EVENT_TYPE_ETHDEV << 28);

		/* Setup rq reference for inline dev if present */
		rc = roc_nix_inl_dev_rq_get(rq, !!eth_dev->data->dev_started);
		if (rc)
			goto free_mem;
	}

	plt_nix_dbg("rq=%d pool=%s nb_desc=%d->%d", qid, lpb_pool->name, nb_desc,
		    cq->nb_desc);

	/* Store start of fast path area */
	eth_dev->data->rx_queues[qid] = rxq_sp + 1;
	eth_dev->data->rx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STOPPED;

	/* Calculating delta and freq mult between PTP HI clock and tsc.
	 * These are needed in deriving raw clock value from tsc counter.
	 * read_clock eth op returns raw clock value.
	 */
	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) || dev->ptp_en) {
		rc = cnxk_nix_tsc_convert(dev);
		if (rc) {
			plt_err("Failed to calculate delta and freq mult");
			goto rq_fini;
		}
	}

	return 0;
free_mem:
	plt_free(rxq_sp);
rq_fini:
	rc |= roc_nix_rq_fini(rq);
cq_fini:
	rc |= roc_nix_cq_fini(cq);
fail:
	return rc;
}

static void
cnxk_nix_rx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	void *rxq = eth_dev->data->rx_queues[qid];
	struct cnxk_eth_rxq_sp *rxq_sp;
	struct cnxk_eth_dev *dev;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc;

	if (!rxq)
		return;

	rxq_sp = cnxk_eth_rxq_to_sp(rxq);
	dev = rxq_sp->dev;
	rq = &dev->rqs[qid];

	plt_nix_dbg("Releasing rxq %u", qid);

	/* Release rq reference for inline dev if present */
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		roc_nix_inl_dev_rq_put(rq);

	/* Cleanup ROC RQ */
	rc = roc_nix_rq_fini(rq);
	if (rc)
		plt_err("Failed to cleanup rq, rc=%d", rc);

	/* Cleanup ROC CQ */
	cq = &dev->cqs[qid];
	rc = roc_nix_cq_fini(cq);
	if (rc)
		plt_err("Failed to cleanup cq, rc=%d", rc);

	/* Finally free fast path area */
	plt_free(rxq_sp);
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

	if (ethdev_rss & RTE_ETH_RSS_L2_PAYLOAD &&
	    dev->npc.switch_header_type == ROC_PRIV_FLAGS_LEN_90B) {
		flowkey_cfg |= FLOW_KEY_TYPE_CH_LEN_90B;
	}

	if (ethdev_rss & RTE_ETH_RSS_C_VLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VLAN;

	if (ethdev_rss & RTE_ETH_RSS_L3_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_SRC;

	if (ethdev_rss & RTE_ETH_RSS_L3_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_DST;

	if (ethdev_rss & RTE_ETH_RSS_L4_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_SRC;

	if (ethdev_rss & RTE_ETH_RSS_L4_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_DST;

	if (ethdev_rss & RSS_IPV4_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV4_INDEX];

	if (ethdev_rss & RSS_IPV6_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV6_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_TCP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_TCP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_UDP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_UDP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_SCTP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_SCTP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_L2_PAYLOAD)
		flowkey_cfg |= flow_key_type[rss_level][RSS_DMAC_INDEX];

	if (ethdev_rss & RSS_IPV6_EX_ENABLE)
		flowkey_cfg |= FLOW_KEY_TYPE_IPV6_EXT;

	if (ethdev_rss & RTE_ETH_RSS_PORT)
		flowkey_cfg |= FLOW_KEY_TYPE_PORT;

	if (ethdev_rss & RTE_ETH_RSS_NVGRE)
		flowkey_cfg |= FLOW_KEY_TYPE_NVGRE;

	if (ethdev_rss & RTE_ETH_RSS_VXLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VXLAN;

	if (ethdev_rss & RTE_ETH_RSS_GENEVE)
		flowkey_cfg |= FLOW_KEY_TYPE_GENEVE;

	if (ethdev_rss & RTE_ETH_RSS_GTPU)
		flowkey_cfg |= FLOW_KEY_TYPE_GTPU;

	return flowkey_cfg;
}

static int
nix_rxchan_cfg_disable(struct cnxk_eth_dev *dev)
{
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_fc_cfg fc_cfg;
	int rc;

	if (!roc_nix_is_lbk(nix))
		return 0;

	memset(&fc_cfg, 0, sizeof(struct roc_nix_fc_cfg));
	fc_cfg.type = ROC_NIX_FC_RXCHAN_CFG;
	fc_cfg.rxchan_cfg.enable = false;
	rc = roc_nix_fc_config_set(nix, &fc_cfg);
	if (rc) {
		plt_err("Failed to setup flow control, rc=%d(%s)", rc, roc_error_msg_get(rc));
		return rc;
	}
	return 0;
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
nix_ingress_policer_setup(struct cnxk_eth_dev *dev)
{
	struct rte_eth_dev *eth_dev = dev->eth_dev;
	int rc = 0;

	TAILQ_INIT(&dev->mtr_profiles);
	TAILQ_INIT(&dev->mtr_policy);
	TAILQ_INIT(&dev->mtr);

	if (eth_dev->dev_ops->mtr_ops_get == NULL)
		return rc;

	return nix_mtr_capabilities_init(eth_dev);
}

static int
nix_rss_default_setup(struct cnxk_eth_dev *dev)
{
	struct rte_eth_dev *eth_dev = dev->eth_dev;
	uint8_t rss_hash_level;
	uint32_t flowkey_cfg;
	uint64_t rss_hf;

	rss_hf = eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	rss_hash_level = RTE_ETH_RSS_LEVEL(rss_hf);
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
		dev_ops->tx_queue_release(eth_dev, i);
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
		dev_ops->rx_queue_release(eth_dev, i);
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
			for (i -= 1; i >= 0; i--)
				dev_ops->tx_queue_release(eth_dev, i);
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
			for (i -= 1; i >= 0; i--)
				dev_ops->rx_queue_release(eth_dev, i);
			goto tx_queue_release;
		}
	}

	free(rx_qconf);
	rx_qconf = NULL;

	return 0;

tx_queue_release:
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		dev_ops->tx_queue_release(eth_dev, i);
fail:
	free(tx_qconf);
	free(rx_qconf);

	return rc;
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
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
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
	      (RTE_ETH_TX_OFFLOAD_TCP_TSO | RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
	       RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO | RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO)))
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
	struct roc_nix_fc_cfg fc_cfg = {0};
	struct roc_nix *nix = &dev->nix;
	struct rte_ether_addr *ea;
	uint16_t nb_rxq, nb_txq;
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

	if (rxmode->mq_mode != RTE_ETH_MQ_RX_NONE &&
	    rxmode->mq_mode != RTE_ETH_MQ_RX_RSS) {
		plt_err("Unsupported mq rx mode %d", rxmode->mq_mode);
		goto fail_configure;
	}

	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
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

		/* Disable and free rte_meter entries */
		rc = nix_meter_fini(dev);
		if (rc)
			goto fail_configure;

		/* Cleanup security support */
		rc = nix_security_release(dev);
		if (rc)
			goto fail_configure;

		roc_nix_tm_fini(nix);
		nix_rxchan_cfg_disable(dev);
		roc_nix_lf_free(nix);
	}

	dev->rx_offloads = rxmode->offloads;
	dev->tx_offloads = txmode->offloads;

	/* Prepare rx cfg */
	rx_cfg = ROC_NIX_LF_RX_CFG_DIS_APAD;
	if (dev->rx_offloads &
	    (RTE_ETH_RX_OFFLOAD_TCP_CKSUM | RTE_ETH_RX_OFFLOAD_UDP_CKSUM)) {
		rx_cfg |= ROC_NIX_LF_RX_CFG_CSUM_OL4;
		rx_cfg |= ROC_NIX_LF_RX_CFG_CSUM_IL4;
	}
	rx_cfg |= (ROC_NIX_LF_RX_CFG_DROP_RE | ROC_NIX_LF_RX_CFG_L2_LEN_ERR |
		   ROC_NIX_LF_RX_CFG_LEN_IL4 | ROC_NIX_LF_RX_CFG_LEN_IL3 |
		   ROC_NIX_LF_RX_CFG_LEN_OL4 | ROC_NIX_LF_RX_CFG_LEN_OL3);

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		rx_cfg |= ROC_NIX_LF_RX_CFG_IP6_UDP_OPT;
		/* Disable drop re if rx offload security is enabled and
		 * platform does not support it.
		 */
		if (dev->ipsecd_drop_re_dis)
			rx_cfg &= ~(ROC_NIX_LF_RX_CFG_DROP_RE);
	}

	nb_rxq = RTE_MAX(data->nb_rx_queues, 1);
	nb_txq = RTE_MAX(data->nb_tx_queues, 1);

	if (roc_nix_is_lbk(nix))
		nix->enable_loop = eth_dev->data->dev_conf.lpbk_mode;

	nix->tx_compl_ena = dev->tx_compl_ena;

	/* Alloc a nix lf */
	rc = roc_nix_lf_alloc(nix, nb_rxq, nb_txq, rx_cfg);
	if (rc) {
		plt_err("Failed to init nix_lf rc=%d", rc);
		goto fail_configure;
	}

	/* Check if ptp is enable in PF owning this VF*/
	if (!roc_nix_is_pf(nix) && (!roc_nix_is_sdp(nix)))
		dev->ptp_en = roc_nix_ptp_is_enable(nix);

	dev->npc.channel = roc_nix_get_base_chan(nix);

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

		if (nix->tx_compl_ena) {
			qs = plt_zmalloc(sizeof(struct roc_nix_cq) * nb_txq, 0);
			if (!qs) {
				plt_err("Failed to alloc cqs");
				goto free_nix_lf;
			}
			dev->cqs = qs;
		}
	}

	/* Re-enable NIX LF error interrupts */
	roc_nix_err_intr_ena_dis(nix, true);
	roc_nix_ras_intr_ena_dis(nix, true);

	if (nix->rx_ptp_ena &&
	    dev->npc.switch_header_type == ROC_PRIV_FLAGS_HIGIG) {
		plt_err("Both PTP and switch header enabled");
		goto free_nix_lf;
	}

	rc = roc_nix_switch_hdr_set(nix, dev->npc.switch_header_type,
				    dev->npc.pre_l2_size_offset,
				    dev->npc.pre_l2_size_offset_mask,
				    dev->npc.pre_l2_size_shift_dir);
	if (rc) {
		plt_err("Failed to enable switch type nix_lf rc=%d", rc);
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

	rc = nix_ingress_policer_setup(dev);
	if (rc) {
		plt_err("Failed to setup ingress policer rc=%d", rc);
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

	if (roc_nix_is_lbk(nix))
		goto skip_lbk_setup;

	/* Configure loop back mode */
	rc = roc_nix_mac_loopback_enable(nix,
					 eth_dev->data->dev_conf.lpbk_mode);
	if (rc) {
		plt_err("Failed to configure cgx loop back mode rc=%d", rc);
		goto cq_fini;
	}

skip_lbk_setup:
	/* Setup Inline security support */
	rc = nix_security_setup(dev);
	if (rc)
		goto cq_fini;

	/* Init flow control configuration */
	fc_cfg.type = ROC_NIX_FC_RXCHAN_CFG;
	fc_cfg.rxchan_cfg.enable = true;
	rc = roc_nix_fc_config_set(nix, &fc_cfg);
	if (rc) {
		plt_err("Failed to initialize flow control rc=%d", rc);
		goto cq_fini;
	}

	/* Update flow control configuration to PMD */
	rc = nix_init_flow_ctrl_config(eth_dev);
	if (rc) {
		plt_err("Failed to initialize flow control rc=%d", rc);
		goto cq_fini;
	}

	/*
	 * Restore queue config when reconfigure followed by
	 * reconfigure and no queue configure invoked from application case.
	 */
	if (dev->configured == 1) {
		rc = nix_restore_queue_cfg(eth_dev);
		if (rc)
			goto sec_release;
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

sec_release:
	rc |= nix_security_release(dev);
cq_fini:
	roc_nix_unregister_cq_irqs(nix);
q_irq_fini:
	roc_nix_unregister_queue_irqs(nix);
tm_fini:
	roc_nix_tm_fini(nix);
free_nix_lf:
	nix_free_queue_mem(dev);
	rc |= nix_rxchan_cfg_disable(dev);
	rc |= roc_nix_lf_free(nix);
fail_configure:
	dev->configured = 0;
	return rc;
}

int
cnxk_nix_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix_sq *sq = &dev->sqs[qid];
	int rc = -EINVAL;

	if (data->tx_queue_state[qid] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	rc = roc_nix_tm_sq_aura_fc(sq, true);
	if (rc) {
		plt_err("Failed to enable sq aura fc, txq=%u, rc=%d", qid, rc);
		goto done;
	}

	data->tx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STARTED;
done:
	return rc;
}

int
cnxk_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix_sq *sq = &dev->sqs[qid];
	int rc;

	if (data->tx_queue_state[qid] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	rc = roc_nix_tm_sq_aura_fc(sq, false);
	if (rc) {
		plt_err("Failed to disable sqb aura fc, txq=%u, rc=%d", qid,
			rc);
		goto done;
	}

	data->tx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STOPPED;
done:
	return rc;
}

static int
cnxk_nix_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix_rq *rq = &dev->rqs[qid];
	int rc;

	if (data->rx_queue_state[qid] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	rc = roc_nix_rq_ena_dis(rq, true);
	if (rc) {
		plt_err("Failed to enable rxq=%u, rc=%d", qid, rc);
		goto done;
	}

	data->rx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STARTED;
done:
	return rc;
}

static int
cnxk_nix_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct roc_nix_rq *rq = &dev->rqs[qid];
	int rc;

	if (data->rx_queue_state[qid] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	rc = roc_nix_rq_ena_dis(rq, false);
	if (rc) {
		plt_err("Failed to disable rxq=%u, rc=%d", qid, rc);
		goto done;
	}

	data->rx_queue_state[qid] = RTE_ETH_QUEUE_STATE_STOPPED;
done:
	return rc;
}

static int
cnxk_nix_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct rte_mbuf *rx_pkts[32];
	struct rte_eth_link link;
	int count, i, j, rc;
	void *rxq;

	/* Disable all the NPC entries */
	rc = roc_npc_mcam_enable_all_entries(&dev->npc, 0);
	if (rc)
		return rc;

	/* Stop link change events */
	if (!roc_nix_is_vf_or_sdp(&dev->nix))
		roc_nix_mac_link_event_start_stop(&dev->nix, false);

	/* Disable Rx via NPC */
	roc_nix_npc_rx_ena_dis(&dev->nix, false);

	roc_nix_inl_outb_soft_exp_poll_switch(&dev->nix, false);

	/* Stop inline device RQ first */
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		roc_nix_inl_rq_ena_dis(&dev->nix, false);

	/* Stop rx queues and free up pkts pending */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rc = dev_ops->rx_queue_stop(eth_dev, i);
		if (rc)
			continue;

		rxq = eth_dev->data->rx_queues[i];
		count = dev->rx_pkt_burst_no_offload(rxq, rx_pkts, 32);
		while (count) {
			for (j = 0; j < count; j++)
				rte_pktmbuf_free(rx_pkts[j]);
			count = dev->rx_pkt_burst_no_offload(rxq, rx_pkts, 32);
		}
	}

	/* Stop tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		dev_ops->tx_queue_stop(eth_dev, i);

	/* Bring down link status internally */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(eth_dev, &link);

	return 0;
}

int
cnxk_nix_dev_start(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc, i;

	if (eth_dev->data->nb_rx_queues != 0 && !dev->ptp_en) {
		rc = nix_recalc_mtu(eth_dev);
		if (rc)
			return rc;
	}

	/* Start rx queues */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rc = cnxk_nix_rx_queue_start(eth_dev, i);
		if (rc)
			return rc;
	}

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		rc = roc_nix_inl_rq_ena_dis(&dev->nix, true);
		if (rc) {
			plt_err("Failed to enable Inline device RQ, rc=%d", rc);
			return rc;
		}
	}

	/* Start tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = cnxk_nix_tx_queue_start(eth_dev, i);
		if (rc)
			return rc;
	}

	/* Update Flow control configuration */
	rc = nix_update_flow_ctrl_config(eth_dev);
	if (rc) {
		plt_err("Failed to enable flow control. error code(%d)", rc);
		return rc;
	}

	/* Enable Rx in NPC */
	rc = roc_nix_npc_rx_ena_dis(&dev->nix, true);
	if (rc) {
		plt_err("Failed to enable NPC rx %d", rc);
		return rc;
	}

	rc = roc_npc_mcam_enable_all_entries(&dev->npc, 1);
	if (rc) {
		plt_err("Failed to enable NPC entries %d", rc);
		return rc;
	}

	cnxk_nix_toggle_flag_link_cfg(dev, true);

	/* Start link change events */
	if (!roc_nix_is_vf_or_sdp(&dev->nix)) {
		rc = roc_nix_mac_link_event_start_stop(&dev->nix, true);
		if (rc) {
			plt_err("Failed to start cgx link event %d", rc);
			goto rx_disable;
		}
	}

	/* Enable PTP if it is requested by the user or already
	 * enabled on PF owning this VF
	 */
	memset(&dev->tstamp, 0, sizeof(struct cnxk_timesync_info));
	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) || dev->ptp_en)
		cnxk_eth_dev_ops.timesync_enable(eth_dev);
	else
		cnxk_eth_dev_ops.timesync_disable(eth_dev);

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		rc = rte_mbuf_dyn_rx_timestamp_register
			(&dev->tstamp.tstamp_dynfield_offset,
			 &dev->tstamp.rx_tstamp_dynflag);
		if (rc != 0) {
			plt_err("Failed to register Rx timestamp field/flag");
			goto rx_disable;
		}
	}

	cnxk_nix_toggle_flag_link_cfg(dev, false);

	roc_nix_inl_outb_soft_exp_poll_switch(&dev->nix, true);

	return 0;

rx_disable:
	roc_nix_npc_rx_ena_dis(&dev->nix, false);
	cnxk_nix_toggle_flag_link_cfg(dev, false);
	return rc;
}

static int cnxk_nix_dev_reset(struct rte_eth_dev *eth_dev);
static int cnxk_nix_dev_close(struct rte_eth_dev *eth_dev);

/* CNXK platform independent eth dev ops */
struct eth_dev_ops cnxk_eth_dev_ops = {
	.mtu_set = cnxk_nix_mtu_set,
	.mac_addr_add = cnxk_nix_mac_addr_add,
	.mac_addr_remove = cnxk_nix_mac_addr_del,
	.mac_addr_set = cnxk_nix_mac_addr_set,
	.dev_infos_get = cnxk_nix_info_get,
	.link_update = cnxk_nix_link_update,
	.tx_queue_release = cnxk_nix_tx_queue_release,
	.rx_queue_release = cnxk_nix_rx_queue_release,
	.dev_stop = cnxk_nix_dev_stop,
	.dev_close = cnxk_nix_dev_close,
	.dev_reset = cnxk_nix_dev_reset,
	.tx_queue_start = cnxk_nix_tx_queue_start,
	.rx_queue_start = cnxk_nix_rx_queue_start,
	.rx_queue_stop = cnxk_nix_rx_queue_stop,
	.dev_supported_ptypes_get = cnxk_nix_supported_ptypes_get,
	.promiscuous_enable = cnxk_nix_promisc_enable,
	.promiscuous_disable = cnxk_nix_promisc_disable,
	.allmulticast_enable = cnxk_nix_allmulticast_enable,
	.allmulticast_disable = cnxk_nix_allmulticast_disable,
	.rx_burst_mode_get = cnxk_nix_rx_burst_mode_get,
	.tx_burst_mode_get = cnxk_nix_tx_burst_mode_get,
	.flow_ctrl_get = cnxk_nix_flow_ctrl_get,
	.flow_ctrl_set = cnxk_nix_flow_ctrl_set,
	.priority_flow_ctrl_queue_config =
				cnxk_nix_priority_flow_ctrl_queue_config,
	.priority_flow_ctrl_queue_info_get =
				cnxk_nix_priority_flow_ctrl_queue_info_get,
	.dev_set_link_up = cnxk_nix_set_link_up,
	.dev_set_link_down = cnxk_nix_set_link_down,
	.get_module_info = cnxk_nix_get_module_info,
	.get_module_eeprom = cnxk_nix_get_module_eeprom,
	.rx_queue_intr_enable = cnxk_nix_rx_queue_intr_enable,
	.rx_queue_intr_disable = cnxk_nix_rx_queue_intr_disable,
	.pool_ops_supported = cnxk_nix_pool_ops_supported,
	.queue_stats_mapping_set = cnxk_nix_queue_stats_mapping,
	.stats_get = cnxk_nix_stats_get,
	.stats_reset = cnxk_nix_stats_reset,
	.xstats_get = cnxk_nix_xstats_get,
	.xstats_get_names = cnxk_nix_xstats_get_names,
	.xstats_reset = cnxk_nix_xstats_reset,
	.xstats_get_by_id = cnxk_nix_xstats_get_by_id,
	.xstats_get_names_by_id = cnxk_nix_xstats_get_names_by_id,
	.fw_version_get = cnxk_nix_fw_version_get,
	.rxq_info_get = cnxk_nix_rxq_info_get,
	.txq_info_get = cnxk_nix_txq_info_get,
	.tx_done_cleanup = cnxk_nix_tx_done_cleanup,
	.flow_ops_get = cnxk_nix_flow_ops_get,
	.get_reg = cnxk_nix_dev_get_reg,
	.timesync_read_rx_timestamp = cnxk_nix_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = cnxk_nix_timesync_read_tx_timestamp,
	.timesync_read_time = cnxk_nix_timesync_read_time,
	.timesync_write_time = cnxk_nix_timesync_write_time,
	.timesync_adjust_time = cnxk_nix_timesync_adjust_time,
	.read_clock = cnxk_nix_read_clock,
	.reta_update = cnxk_nix_reta_update,
	.reta_query = cnxk_nix_reta_query,
	.rss_hash_update = cnxk_nix_rss_hash_update,
	.rss_hash_conf_get = cnxk_nix_rss_hash_conf_get,
	.set_mc_addr_list = cnxk_nix_mc_addr_list_configure,
	.set_queue_rate_limit = cnxk_nix_tm_set_queue_rate_limit,
	.tm_ops_get = cnxk_nix_tm_ops_get,
	.mtr_ops_get = cnxk_nix_mtr_ops_get,
	.eth_dev_priv_dump  = cnxk_nix_eth_dev_priv_dump,
	.cman_info_get = cnxk_nix_cman_info_get,
	.cman_config_init = cnxk_nix_cman_config_init,
	.cman_config_set = cnxk_nix_cman_config_set,
	.cman_config_get = cnxk_nix_cman_config_get,
};

void
cnxk_eth_dev_q_err_cb(struct roc_nix *nix, void *data)
{
	struct cnxk_eth_dev *dev = (struct cnxk_eth_dev *)nix;
	struct rte_eth_dev *eth_dev = dev->eth_dev;

	/* Set the flag and execute application callbacks */
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_RESET, data);
}

static int
cnxk_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_security_ctx *sec_ctx;
	struct roc_nix *nix = &dev->nix;
	struct rte_pci_device *pci_dev;
	int rc, max_entries;

	eth_dev->dev_ops = &cnxk_eth_dev_ops;
	eth_dev->rx_queue_count = cnxk_nix_rx_queue_count;
	eth_dev->rx_descriptor_status = cnxk_nix_rx_descriptor_status;
	eth_dev->tx_descriptor_status = cnxk_nix_tx_descriptor_status;

	/* Alloc security context */
	sec_ctx = plt_zmalloc(sizeof(struct rte_security_ctx), 0);
	if (!sec_ctx)
		return -ENOMEM;
	sec_ctx->device = eth_dev;
	sec_ctx->ops = &cnxk_eth_sec_ops;
	sec_ctx->flags = RTE_SEC_CTX_F_FAST_SET_MDATA;
	eth_dev->security_ctx = sec_ctx;

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
	nix->hw_vlan_ins = true;
	nix->port_id = eth_dev->data->port_id;
	if (roc_feature_nix_has_own_meta_aura())
		nix->local_meta_aura_ena = true;
	rc = roc_nix_dev_init(nix);
	if (rc) {
		plt_err("Failed to initialize roc nix rc=%d", rc);
		goto error;
	}

	/* Register up msg callbacks */
	roc_nix_mac_link_cb_register(nix, cnxk_eth_dev_link_status_cb);

	/* Register up msg callbacks */
	roc_nix_mac_link_info_get_cb_register(nix,
					      cnxk_eth_dev_link_status_get_cb);

	/* Register up msg callbacks */
	roc_nix_q_err_cb_register(nix, cnxk_eth_dev_q_err_cb);

	/* Register callback for inline meta pool create */
	roc_nix_inl_meta_pool_cb_register(cnxk_nix_inl_meta_pool_cb);

	dev->eth_dev = eth_dev;
	dev->configured = 0;
	dev->ptype_disable = 0;
	dev->proto = RTE_MTR_COLOR_IN_PROTO_OUTER_VLAN;

	TAILQ_INIT(&dev->inb.list);
	TAILQ_INIT(&dev->outb.list);
	rte_spinlock_init(&dev->inb.lock);
	rte_spinlock_init(&dev->outb.lock);

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
	dev->dmac_filter_count = 1;

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
	dev->npc.roc_nix = nix;
	rc = roc_npc_init(&dev->npc);
	if (rc)
		goto free_mac_addrs;

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
cnxk_eth_dev_uninit(struct rte_eth_dev *eth_dev, bool reset)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct eth_dev_ops *dev_ops = eth_dev->dev_ops;
	struct cnxk_pfc_cfg *pfc_cfg = &dev->pfc_cfg;
	struct cnxk_fc_cfg *fc_cfg = &dev->fc_cfg;
	struct rte_eth_pfc_queue_conf pfc_conf;
	struct roc_nix *nix = &dev->nix;
	struct rte_eth_fc_conf fc_conf;
	int rc, i;

	plt_free(eth_dev->security_ctx);
	eth_dev->security_ctx = NULL;

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Disable switch hdr pkind */
	roc_nix_switch_hdr_set(&dev->nix, 0, 0, 0, 0);

	/* Clear the flag since we are closing down */
	dev->configured = 0;

	roc_nix_npc_rx_ena_dis(nix, false);

	/* Restore 802.3 Flow control configuration */
	memset(&pfc_conf, 0, sizeof(struct rte_eth_pfc_queue_conf));
	memset(&fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	if (fc_cfg->rx_pause || fc_cfg->tx_pause) {
		fc_conf.mode = RTE_ETH_FC_NONE;
		rc = cnxk_nix_flow_ctrl_set(eth_dev, &fc_conf);
		if (rc < 0)
			plt_err("Failed to reset control flow. error code(%d)",
					rc);
	}
	if (pfc_cfg->rx_pause_en || pfc_cfg->tx_pause_en) {
		for (i = 0; i < RTE_MAX(eth_dev->data->nb_rx_queues,
					eth_dev->data->nb_tx_queues);
		     i++) {
			pfc_conf.mode = RTE_ETH_FC_NONE;
			pfc_conf.rx_pause.tc = ROC_NIX_PFC_CLASS_INVALID;
			pfc_conf.rx_pause.tx_qid = i;
			pfc_conf.tx_pause.tc = ROC_NIX_PFC_CLASS_INVALID;
			pfc_conf.tx_pause.rx_qid = i;
			rc = cnxk_nix_priority_flow_ctrl_queue_config(eth_dev,
					&pfc_conf);
			if (rc && rc != -ENOTSUP)
				plt_err("Failed to reset PFC. error code(%d)", rc);
		}
	}

	/* Disable and free rte_meter entries */
	nix_meter_fini(dev);

	/* Disable and free rte_flow entries */
	roc_npc_fini(&dev->npc);

	/* Disable link status events */
	roc_nix_mac_link_event_start_stop(nix, false);

	/* Unregister the link update op, this is required to stop VFs from
	 * receiving link status updates on exit path.
	 */
	roc_nix_mac_link_cb_unregister(nix);

	/* Free up SQs */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		dev_ops->tx_queue_release(eth_dev, i);
		eth_dev->data->tx_queues[i] = NULL;
	}
	eth_dev->data->nb_tx_queues = 0;

	/* Free up RQ's and CQ's */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		dev_ops->rx_queue_release(eth_dev, i);
		eth_dev->data->rx_queues[i] = NULL;
	}
	eth_dev->data->nb_rx_queues = 0;

	/* Free security resources */
	nix_security_release(dev);

	/* Free tm resources */
	roc_nix_tm_fini(nix);

	/* Unregister queue irqs */
	roc_nix_unregister_queue_irqs(nix);

	/* Unregister cq irqs */
	if (eth_dev->data->dev_conf.intr_conf.rxq)
		roc_nix_unregister_cq_irqs(nix);

	/* Free ROC RQ's, SQ's and CQ's memory */
	nix_free_queue_mem(dev);

	/* free nix bpid */
	rc = nix_rxchan_cfg_disable(dev);
	if (rc)
		plt_err("Failed to free nix bpid, rc=%d", rc);

	/* Free nix lf resources */
	rc = roc_nix_lf_free(nix);
	if (rc)
		plt_err("Failed to free nix lf, rc=%d", rc);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	rc = roc_nix_dev_fini(nix);
	/* Can be freed later by PMD if NPA LF is in use */
	if (rc == -EAGAIN) {
		if (!reset)
			eth_dev->data->dev_private = NULL;
		return 0;
	} else if (rc) {
		plt_err("Failed in nix dev fini, rc=%d", rc);
	}

	return rc;
}

static int
cnxk_nix_dev_close(struct rte_eth_dev *eth_dev)
{
	cnxk_eth_dev_uninit(eth_dev, false);
	return 0;
}

static int
cnxk_nix_dev_reset(struct rte_eth_dev *eth_dev)
{
	int rc;

	rc = cnxk_eth_dev_uninit(eth_dev, true);
	if (rc)
		return rc;

	return cnxk_eth_dev_init(eth_dev);
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
		rc = cnxk_eth_dev_uninit(eth_dev, false);
		if (rc)
			return rc;

		rte_eth_dev_release_port(eth_dev);
	}

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Check if this device is hosting common resource */
	nix = roc_idev_npa_nix_get();
	if (!nix || nix->pci_dev != pci_dev)
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
