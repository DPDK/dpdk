/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline uint32_t
nix_qsize_to_val(enum nix_q_size qsize)
{
	return (16UL << (qsize * 2));
}

static inline enum nix_q_size
nix_qsize_clampup(uint32_t val)
{
	int i = nix_q_size_16;

	for (; i < nix_q_size_max; i++)
		if (val <= nix_qsize_to_val(i))
			break;

	if (i >= nix_q_size_max)
		i = nix_q_size_max - 1;

	return i;
}

int
roc_nix_rq_ena_dis(struct roc_nix_rq *rq, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(rq->roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	int rc;

	/* Pkts will be dropped silently if RQ is disabled */
	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.ena = enable;
		aq->rq_mask.ena = ~(aq->rq_mask.ena);
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.ena = enable;
		aq->rq_mask.ena = ~(aq->rq_mask.ena);
	}

	rc = mbox_process(mbox);

	if (roc_model_is_cn10k())
		plt_write64(rq->qid, nix->base + NIX_LF_OP_VWQE_FLUSH);
	return rc;
}

static int
rq_cn9k_cfg(struct nix *nix, struct roc_nix_rq *rq, bool cfg, bool ena)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = rq->qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = cfg ? NIX_AQ_INSTOP_WRITE : NIX_AQ_INSTOP_INIT;

	if (rq->sso_ena) {
		/* SSO mode */
		aq->rq.sso_ena = 1;
		aq->rq.sso_tt = rq->tt;
		aq->rq.sso_grp = rq->hwgrp;
		aq->rq.ena_wqwd = 1;
		aq->rq.wqe_skip = rq->wqe_skip;
		aq->rq.wqe_caching = 1;

		aq->rq.good_utag = rq->tag_mask >> 24;
		aq->rq.bad_utag = rq->tag_mask >> 24;
		aq->rq.ltag = rq->tag_mask & BITMASK_ULL(24, 0);
	} else {
		/* CQ mode */
		aq->rq.sso_ena = 0;
		aq->rq.good_utag = rq->tag_mask >> 24;
		aq->rq.bad_utag = rq->tag_mask >> 24;
		aq->rq.ltag = rq->tag_mask & BITMASK_ULL(24, 0);
		aq->rq.cq = rq->qid;
	}

	if (rq->ipsech_ena)
		aq->rq.ipsech_ena = 1;

	aq->rq.spb_ena = 0;
	aq->rq.lpb_aura = roc_npa_aura_handle_to_aura(rq->aura_handle);

	/* Sizes must be aligned to 8 bytes */
	if (rq->first_skip & 0x7 || rq->later_skip & 0x7 || rq->lpb_size & 0x7)
		return -EINVAL;

	/* Expressed in number of dwords */
	aq->rq.first_skip = rq->first_skip / 8;
	aq->rq.later_skip = rq->later_skip / 8;
	aq->rq.flow_tagw = rq->flow_tag_width; /* 32-bits */
	aq->rq.lpb_sizem1 = rq->lpb_size / 8;
	aq->rq.lpb_sizem1 -= 1; /* Expressed in size minus one */
	aq->rq.ena = ena;
	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */
	aq->rq.rq_int_ena = 0;
	/* Many to one reduction */
	aq->rq.qint_idx = rq->qid % nix->qints;
	aq->rq.xqe_drop_ena = 1;

	if (cfg) {
		if (rq->sso_ena) {
			/* SSO mode */
			aq->rq_mask.sso_ena = ~aq->rq_mask.sso_ena;
			aq->rq_mask.sso_tt = ~aq->rq_mask.sso_tt;
			aq->rq_mask.sso_grp = ~aq->rq_mask.sso_grp;
			aq->rq_mask.ena_wqwd = ~aq->rq_mask.ena_wqwd;
			aq->rq_mask.wqe_skip = ~aq->rq_mask.wqe_skip;
			aq->rq_mask.wqe_caching = ~aq->rq_mask.wqe_caching;
			aq->rq_mask.good_utag = ~aq->rq_mask.good_utag;
			aq->rq_mask.bad_utag = ~aq->rq_mask.bad_utag;
			aq->rq_mask.ltag = ~aq->rq_mask.ltag;
		} else {
			/* CQ mode */
			aq->rq_mask.sso_ena = ~aq->rq_mask.sso_ena;
			aq->rq_mask.good_utag = ~aq->rq_mask.good_utag;
			aq->rq_mask.bad_utag = ~aq->rq_mask.bad_utag;
			aq->rq_mask.ltag = ~aq->rq_mask.ltag;
			aq->rq_mask.cq = ~aq->rq_mask.cq;
		}

		if (rq->ipsech_ena)
			aq->rq_mask.ipsech_ena = ~aq->rq_mask.ipsech_ena;

		aq->rq_mask.spb_ena = ~aq->rq_mask.spb_ena;
		aq->rq_mask.lpb_aura = ~aq->rq_mask.lpb_aura;
		aq->rq_mask.first_skip = ~aq->rq_mask.first_skip;
		aq->rq_mask.later_skip = ~aq->rq_mask.later_skip;
		aq->rq_mask.flow_tagw = ~aq->rq_mask.flow_tagw;
		aq->rq_mask.lpb_sizem1 = ~aq->rq_mask.lpb_sizem1;
		aq->rq_mask.ena = ~aq->rq_mask.ena;
		aq->rq_mask.pb_caching = ~aq->rq_mask.pb_caching;
		aq->rq_mask.xqe_imm_size = ~aq->rq_mask.xqe_imm_size;
		aq->rq_mask.rq_int_ena = ~aq->rq_mask.rq_int_ena;
		aq->rq_mask.qint_idx = ~aq->rq_mask.qint_idx;
		aq->rq_mask.xqe_drop_ena = ~aq->rq_mask.xqe_drop_ena;
	}

	return 0;
}

static int
rq_cfg(struct nix *nix, struct roc_nix_rq *rq, bool cfg, bool ena)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_cn10k_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	aq->qidx = rq->qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = cfg ? NIX_AQ_INSTOP_WRITE : NIX_AQ_INSTOP_INIT;

	if (rq->sso_ena) {
		/* SSO mode */
		aq->rq.sso_ena = 1;
		aq->rq.sso_tt = rq->tt;
		aq->rq.sso_grp = rq->hwgrp;
		aq->rq.ena_wqwd = 1;
		aq->rq.wqe_skip = rq->wqe_skip;
		aq->rq.wqe_caching = 1;

		aq->rq.good_utag = rq->tag_mask >> 24;
		aq->rq.bad_utag = rq->tag_mask >> 24;
		aq->rq.ltag = rq->tag_mask & BITMASK_ULL(24, 0);

		if (rq->vwqe_ena) {
			aq->rq.vwqe_ena = true;
			aq->rq.vwqe_skip = rq->vwqe_first_skip;
			/* Maximal Vector size is (2^(MAX_VSIZE_EXP+2)) */
			aq->rq.max_vsize_exp = rq->vwqe_max_sz_exp - 2;
			aq->rq.vtime_wait = rq->vwqe_wait_tmo;
			aq->rq.wqe_aura = rq->vwqe_aura_handle;
		}
	} else {
		/* CQ mode */
		aq->rq.sso_ena = 0;
		aq->rq.good_utag = rq->tag_mask >> 24;
		aq->rq.bad_utag = rq->tag_mask >> 24;
		aq->rq.ltag = rq->tag_mask & BITMASK_ULL(24, 0);
		aq->rq.cq = rq->qid;
	}

	if (rq->ipsech_ena)
		aq->rq.ipsech_ena = 1;

	aq->rq.lpb_aura = roc_npa_aura_handle_to_aura(rq->aura_handle);

	/* Sizes must be aligned to 8 bytes */
	if (rq->first_skip & 0x7 || rq->later_skip & 0x7 || rq->lpb_size & 0x7)
		return -EINVAL;

	/* Expressed in number of dwords */
	aq->rq.first_skip = rq->first_skip / 8;
	aq->rq.later_skip = rq->later_skip / 8;
	aq->rq.flow_tagw = rq->flow_tag_width; /* 32-bits */
	aq->rq.lpb_sizem1 = rq->lpb_size / 8;
	aq->rq.lpb_sizem1 -= 1; /* Expressed in size minus one */
	aq->rq.ena = ena;

	if (rq->spb_ena) {
		uint32_t spb_sizem1;

		aq->rq.spb_ena = 1;
		aq->rq.spb_aura =
			roc_npa_aura_handle_to_aura(rq->spb_aura_handle);

		if (rq->spb_size & 0x7 ||
		    rq->spb_size > NIX_RQ_CN10K_SPB_MAX_SIZE)
			return -EINVAL;

		spb_sizem1 = rq->spb_size / 8; /* Expressed in no. of dwords */
		spb_sizem1 -= 1;	       /* Expressed in size minus one */
		aq->rq.spb_sizem1 = spb_sizem1 & 0x3F;
		aq->rq.spb_high_sizem1 = (spb_sizem1 >> 6) & 0x7;
	} else {
		aq->rq.spb_ena = 0;
	}

	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */
	aq->rq.rq_int_ena = 0;
	/* Many to one reduction */
	aq->rq.qint_idx = rq->qid % nix->qints;
	aq->rq.xqe_drop_ena = 1;

	if (cfg) {
		if (rq->sso_ena) {
			/* SSO mode */
			aq->rq_mask.sso_ena = ~aq->rq_mask.sso_ena;
			aq->rq_mask.sso_tt = ~aq->rq_mask.sso_tt;
			aq->rq_mask.sso_grp = ~aq->rq_mask.sso_grp;
			aq->rq_mask.ena_wqwd = ~aq->rq_mask.ena_wqwd;
			aq->rq_mask.wqe_skip = ~aq->rq_mask.wqe_skip;
			aq->rq_mask.wqe_caching = ~aq->rq_mask.wqe_caching;
			aq->rq_mask.good_utag = ~aq->rq_mask.good_utag;
			aq->rq_mask.bad_utag = ~aq->rq_mask.bad_utag;
			aq->rq_mask.ltag = ~aq->rq_mask.ltag;
			if (rq->vwqe_ena) {
				aq->rq_mask.vwqe_ena = ~aq->rq_mask.vwqe_ena;
				aq->rq_mask.vwqe_skip = ~aq->rq_mask.vwqe_skip;
				aq->rq_mask.max_vsize_exp =
					~aq->rq_mask.max_vsize_exp;
				aq->rq_mask.vtime_wait =
					~aq->rq_mask.vtime_wait;
				aq->rq_mask.wqe_aura = ~aq->rq_mask.wqe_aura;
			}
		} else {
			/* CQ mode */
			aq->rq_mask.sso_ena = ~aq->rq_mask.sso_ena;
			aq->rq_mask.good_utag = ~aq->rq_mask.good_utag;
			aq->rq_mask.bad_utag = ~aq->rq_mask.bad_utag;
			aq->rq_mask.ltag = ~aq->rq_mask.ltag;
			aq->rq_mask.cq = ~aq->rq_mask.cq;
		}

		if (rq->ipsech_ena)
			aq->rq_mask.ipsech_ena = ~aq->rq_mask.ipsech_ena;

		if (rq->spb_ena) {
			aq->rq_mask.spb_aura = ~aq->rq_mask.spb_aura;
			aq->rq_mask.spb_sizem1 = ~aq->rq_mask.spb_sizem1;
			aq->rq_mask.spb_high_sizem1 =
				~aq->rq_mask.spb_high_sizem1;
		}

		aq->rq_mask.spb_ena = ~aq->rq_mask.spb_ena;
		aq->rq_mask.lpb_aura = ~aq->rq_mask.lpb_aura;
		aq->rq_mask.first_skip = ~aq->rq_mask.first_skip;
		aq->rq_mask.later_skip = ~aq->rq_mask.later_skip;
		aq->rq_mask.flow_tagw = ~aq->rq_mask.flow_tagw;
		aq->rq_mask.lpb_sizem1 = ~aq->rq_mask.lpb_sizem1;
		aq->rq_mask.ena = ~aq->rq_mask.ena;
		aq->rq_mask.pb_caching = ~aq->rq_mask.pb_caching;
		aq->rq_mask.xqe_imm_size = ~aq->rq_mask.xqe_imm_size;
		aq->rq_mask.rq_int_ena = ~aq->rq_mask.rq_int_ena;
		aq->rq_mask.qint_idx = ~aq->rq_mask.qint_idx;
		aq->rq_mask.xqe_drop_ena = ~aq->rq_mask.xqe_drop_ena;
	}

	return 0;
}

int
roc_nix_rq_init(struct roc_nix *roc_nix, struct roc_nix_rq *rq, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	bool is_cn9k = roc_model_is_cn9k();
	int rc;

	if (roc_nix == NULL || rq == NULL)
		return NIX_ERR_PARAM;

	if (rq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	rq->roc_nix = roc_nix;

	if (is_cn9k)
		rc = rq_cn9k_cfg(nix, rq, false, ena);
	else
		rc = rq_cfg(nix, rq, false, ena);

	if (rc)
		return rc;

	return mbox_process(mbox);
}

int
roc_nix_rq_modify(struct roc_nix *roc_nix, struct roc_nix_rq *rq, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	bool is_cn9k = roc_model_is_cn9k();
	int rc;

	if (roc_nix == NULL || rq == NULL)
		return NIX_ERR_PARAM;

	if (rq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	rq->roc_nix = roc_nix;

	if (is_cn9k)
		rc = rq_cn9k_cfg(nix, rq, true, ena);
	else
		rc = rq_cfg(nix, rq, true, ena);

	if (rc)
		return rc;

	return mbox_process(mbox);
}

int
roc_nix_rq_fini(struct roc_nix_rq *rq)
{
	/* Disabling RQ is sufficient */
	return roc_nix_rq_ena_dis(rq, false);
}

int
roc_nix_cq_init(struct roc_nix *roc_nix, struct roc_nix_cq *cq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	volatile struct nix_cq_ctx_s *cq_ctx;
	enum nix_q_size qsize;
	size_t desc_sz;
	int rc;

	if (cq == NULL)
		return NIX_ERR_PARAM;

	if (cq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	qsize = nix_qsize_clampup(cq->nb_desc);
	cq->nb_desc = nix_qsize_to_val(qsize);
	cq->qmask = cq->nb_desc - 1;
	cq->door = nix->base + NIX_LF_CQ_OP_DOOR;
	cq->status = (int64_t *)(nix->base + NIX_LF_CQ_OP_STATUS);
	cq->wdata = (uint64_t)cq->qid << 32;
	cq->roc_nix = roc_nix;
	cq->drop_thresh = NIX_CQ_THRESH_LEVEL;

	/* CQE of W16 */
	desc_sz = cq->nb_desc * NIX_CQ_ENTRY_SZ;
	cq->desc_base = plt_zmalloc(desc_sz, NIX_CQ_ALIGN);
	if (cq->desc_base == NULL) {
		rc = NIX_ERR_NO_MEM;
		goto fail;
	}

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_INIT;
		cq_ctx = &aq->cq;
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_INIT;
		cq_ctx = &aq->cq;
	}

	cq_ctx->ena = 1;
	cq_ctx->caching = 1;
	cq_ctx->qsize = qsize;
	cq_ctx->base = (uint64_t)cq->desc_base;
	cq_ctx->avg_level = 0xff;
	cq_ctx->cq_err_int_ena = BIT(NIX_CQERRINT_CQE_FAULT);
	cq_ctx->cq_err_int_ena |= BIT(NIX_CQERRINT_DOOR_ERR);

	/* Many to one reduction */
	cq_ctx->qint_idx = cq->qid % nix->qints;
	/* Map CQ0 [RQ0] to CINT0 and so on till max 64 irqs */
	cq_ctx->cint_idx = cq->qid;

	cq_ctx->drop = cq->drop_thresh;
	cq_ctx->drop_ena = 1;

	/* TX pause frames enable flow ctrl on RX side */
	if (nix->tx_pause) {
		/* Single BPID is allocated for all rx channels for now */
		cq_ctx->bpid = nix->bpid[0];
		cq_ctx->bp = cq_ctx->drop;
		cq_ctx->bp_ena = 1;
	}

	rc = mbox_process(mbox);
	if (rc)
		goto free_mem;

	return 0;

free_mem:
	plt_free(cq->desc_base);
fail:
	return rc;
}

int
roc_nix_cq_fini(struct roc_nix_cq *cq)
{
	struct mbox *mbox;
	struct nix *nix;
	int rc;

	if (cq == NULL)
		return NIX_ERR_PARAM;

	nix = roc_nix_to_nix_priv(cq->roc_nix);
	mbox = (&nix->dev)->mbox;

	/* Disable CQ */
	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;
		aq->cq.ena = 0;
		aq->cq.bp_ena = 0;
		aq->cq_mask.ena = ~aq->cq_mask.ena;
		aq->cq_mask.bp_ena = ~aq->cq_mask.bp_ena;
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;
		aq->cq.ena = 0;
		aq->cq.bp_ena = 0;
		aq->cq_mask.ena = ~aq->cq_mask.ena;
		aq->cq_mask.bp_ena = ~aq->cq_mask.bp_ena;
	}

	rc = mbox_process(mbox);
	if (rc)
		return rc;

	plt_free(cq->desc_base);
	return 0;
}

static int
sqb_pool_populate(struct roc_nix *roc_nix, struct roc_nix_sq *sq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint16_t sqes_per_sqb, count, nb_sqb_bufs;
	struct npa_pool_s pool;
	struct npa_aura_s aura;
	uint64_t blk_sz;
	uint64_t iova;
	int rc;

	blk_sz = nix->sqb_size;
	if (sq->max_sqe_sz == roc_nix_maxsqesz_w16)
		sqes_per_sqb = (blk_sz / 8) / 16;
	else
		sqes_per_sqb = (blk_sz / 8) / 8;

	sq->nb_desc = PLT_MAX(256U, sq->nb_desc);
	nb_sqb_bufs = sq->nb_desc / sqes_per_sqb;
	nb_sqb_bufs += NIX_SQB_LIST_SPACE;
	/* Clamp up the SQB count */
	nb_sqb_bufs = PLT_MIN(roc_nix->max_sqb_count,
			      (uint16_t)PLT_MAX(NIX_DEF_SQB, nb_sqb_bufs));

	sq->nb_sqb_bufs = nb_sqb_bufs;
	sq->sqes_per_sqb_log2 = (uint16_t)plt_log2_u32(sqes_per_sqb);
	sq->nb_sqb_bufs_adj =
		nb_sqb_bufs -
		(PLT_ALIGN_MUL_CEIL(nb_sqb_bufs, sqes_per_sqb) / sqes_per_sqb);
	sq->nb_sqb_bufs_adj =
		(sq->nb_sqb_bufs_adj * NIX_SQB_LOWER_THRESH) / 100;

	/* Explicitly set nat_align alone as by default pool is with both
	 * nat_align and buf_offset = 1 which we don't want for SQB.
	 */
	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;

	memset(&aura, 0, sizeof(aura));
	aura.fc_ena = 1;
	aura.fc_addr = (uint64_t)sq->fc;
	aura.fc_hyst_bits = 0; /* Store count on all updates */
	rc = roc_npa_pool_create(&sq->aura_handle, blk_sz, nb_sqb_bufs, &aura,
				 &pool);
	if (rc)
		goto fail;

	sq->sqe_mem = plt_zmalloc(blk_sz * nb_sqb_bufs, blk_sz);
	if (sq->sqe_mem == NULL) {
		rc = NIX_ERR_NO_MEM;
		goto nomem;
	}

	/* Fill the initial buffers */
	iova = (uint64_t)sq->sqe_mem;
	for (count = 0; count < nb_sqb_bufs; count++) {
		roc_npa_aura_op_free(sq->aura_handle, 0, iova);
		iova += blk_sz;
	}
	roc_npa_aura_op_range_set(sq->aura_handle, (uint64_t)sq->sqe_mem, iova);

	return rc;
nomem:
	roc_npa_pool_destroy(sq->aura_handle);
fail:
	return rc;
}

static void
sq_cn9k_init(struct nix *nix, struct roc_nix_sq *sq, uint32_t rr_quantum,
	     uint16_t smq)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_INIT;
	aq->sq.max_sqe_size = sq->max_sqe_sz;

	aq->sq.max_sqe_size = sq->max_sqe_sz;
	aq->sq.smq = smq;
	aq->sq.smq_rr_quantum = rr_quantum;
	aq->sq.default_chan = nix->tx_chan_base;
	aq->sq.sqe_stype = NIX_STYPE_STF;
	aq->sq.ena = 1;
	if (aq->sq.max_sqe_size == NIX_MAXSQESZ_W8)
		aq->sq.sqe_stype = NIX_STYPE_STP;
	aq->sq.sqb_aura = roc_npa_aura_handle_to_aura(sq->aura_handle);
	aq->sq.sq_int_ena = BIT(NIX_SQINT_LMT_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SQB_ALLOC_FAIL);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SEND_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_MNQ_ERR);

	/* Many to one reduction */
	aq->sq.qint_idx = sq->qid % nix->qints;
}

static int
sq_cn9k_fini(struct nix *nix, struct roc_nix_sq *sq)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_aq_enq_rsp *rsp;
	struct nix_aq_enq_req *aq;
	uint16_t sqes_per_sqb;
	void *sqb_buf;
	int rc, count;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Check if sq is already cleaned up */
	if (!rsp->sq.ena)
		return 0;

	/* Disable sq */
	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_WRITE;
	aq->sq_mask.ena = ~aq->sq_mask.ena;
	aq->sq.ena = 0;
	rc = mbox_process(mbox);
	if (rc)
		return rc;

	/* Read SQ and free sqb's */
	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (aq->sq.smq_pend)
		plt_err("SQ has pending SQE's");

	count = aq->sq.sqb_count;
	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	sqb_buf = (void *)rsp->sq.head_sqb;
	while (count) {
		void *next_sqb;

		next_sqb = *(void **)((uintptr_t)sqb_buf +
				      (uint32_t)((sqes_per_sqb - 1) *
						 sq->max_sqe_sz));
		roc_npa_aura_op_free(sq->aura_handle, 1, (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		count--;
	}

	/* Free next to use sqb */
	if (rsp->sq.next_sqb)
		roc_npa_aura_op_free(sq->aura_handle, 1, rsp->sq.next_sqb);
	return 0;
}

static void
sq_init(struct nix *nix, struct roc_nix_sq *sq, uint32_t rr_quantum,
	uint16_t smq)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_cn10k_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_INIT;
	aq->sq.max_sqe_size = sq->max_sqe_sz;

	aq->sq.max_sqe_size = sq->max_sqe_sz;
	aq->sq.smq = smq;
	aq->sq.smq_rr_weight = rr_quantum;
	aq->sq.default_chan = nix->tx_chan_base;
	aq->sq.sqe_stype = NIX_STYPE_STF;
	aq->sq.ena = 1;
	if (aq->sq.max_sqe_size == NIX_MAXSQESZ_W8)
		aq->sq.sqe_stype = NIX_STYPE_STP;
	aq->sq.sqb_aura = roc_npa_aura_handle_to_aura(sq->aura_handle);
	aq->sq.sq_int_ena = BIT(NIX_SQINT_LMT_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SQB_ALLOC_FAIL);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SEND_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_MNQ_ERR);

	/* Many to one reduction */
	aq->sq.qint_idx = sq->qid % nix->qints;
}

static int
sq_fini(struct nix *nix, struct roc_nix_sq *sq)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_cn10k_aq_enq_rsp *rsp;
	struct nix_cn10k_aq_enq_req *aq;
	uint16_t sqes_per_sqb;
	void *sqb_buf;
	int rc, count;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Check if sq is already cleaned up */
	if (!rsp->sq.ena)
		return 0;

	/* Disable sq */
	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_WRITE;
	aq->sq_mask.ena = ~aq->sq_mask.ena;
	aq->sq.ena = 0;
	rc = mbox_process(mbox);
	if (rc)
		return rc;

	/* Read SQ and free sqb's */
	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (aq->sq.smq_pend)
		plt_err("SQ has pending SQE's");

	count = aq->sq.sqb_count;
	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	sqb_buf = (void *)rsp->sq.head_sqb;
	while (count) {
		void *next_sqb;

		next_sqb = *(void **)((uintptr_t)sqb_buf +
				      (uint32_t)((sqes_per_sqb - 1) *
						 sq->max_sqe_sz));
		roc_npa_aura_op_free(sq->aura_handle, 1, (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		count--;
	}

	/* Free next to use sqb */
	if (rsp->sq.next_sqb)
		roc_npa_aura_op_free(sq->aura_handle, 1, rsp->sq.next_sqb);
	return 0;
}

int
roc_nix_sq_init(struct roc_nix *roc_nix, struct roc_nix_sq *sq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	uint16_t qid, smq = UINT16_MAX;
	uint32_t rr_quantum = 0;
	int rc;

	if (sq == NULL)
		return NIX_ERR_PARAM;

	qid = sq->qid;
	if (qid >= nix->nb_tx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	sq->roc_nix = roc_nix;
	/*
	 * Allocate memory for flow control updates from HW.
	 * Alloc one cache line, so that fits all FC_STYPE modes.
	 */
	sq->fc = plt_zmalloc(ROC_ALIGN, ROC_ALIGN);
	if (sq->fc == NULL) {
		rc = NIX_ERR_NO_MEM;
		goto fail;
	}

	rc = sqb_pool_populate(roc_nix, sq);
	if (rc)
		goto nomem;

	rc = nix_tm_leaf_data_get(nix, sq->qid, &rr_quantum, &smq);
	if (rc) {
		rc = NIX_ERR_TM_LEAF_NODE_GET;
		goto nomem;
	}

	/* Init SQ context */
	if (roc_model_is_cn9k())
		sq_cn9k_init(nix, sq, rr_quantum, smq);
	else
		sq_init(nix, sq, rr_quantum, smq);

	rc = mbox_process(mbox);
	if (rc)
		goto nomem;

	nix->sqs[qid] = sq;
	sq->io_addr = nix->base + NIX_LF_OP_SENDX(0);
	/* Evenly distribute LMT slot for each sq */
	if (roc_model_is_cn9k()) {
		/* Multiple cores/SQ's can use same LMTLINE safely in CN9K */
		sq->lmt_addr = (void *)(nix->lmt_base +
					((qid & RVU_CN9K_LMT_SLOT_MASK) << 12));
	}

	return rc;
nomem:
	plt_free(sq->fc);
fail:
	return rc;
}

int
roc_nix_sq_fini(struct roc_nix_sq *sq)
{
	struct nix *nix;
	struct mbox *mbox;
	struct ndc_sync_op *ndc_req;
	uint16_t qid;
	int rc = 0;

	if (sq == NULL)
		return NIX_ERR_PARAM;

	nix = roc_nix_to_nix_priv(sq->roc_nix);
	mbox = (&nix->dev)->mbox;

	qid = sq->qid;

	rc = nix_tm_sq_flush_pre(sq);

	/* Release SQ context */
	if (roc_model_is_cn9k())
		rc |= sq_cn9k_fini(roc_nix_to_nix_priv(sq->roc_nix), sq);
	else
		rc |= sq_fini(roc_nix_to_nix_priv(sq->roc_nix), sq);

	/* Sync NDC-NIX-TX for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox);
	if (ndc_req == NULL)
		return -ENOSPC;
	ndc_req->nix_lf_tx_sync = 1;
	if (mbox_process(mbox))
		rc |= NIX_ERR_NDC_SYNC;

	rc |= nix_tm_sq_flush_post(sq);
	rc |= roc_npa_pool_destroy(sq->aura_handle);
	plt_free(sq->fc);
	plt_free(sq->sqe_mem);
	nix->sqs[qid] = NULL;

	return rc;
}
