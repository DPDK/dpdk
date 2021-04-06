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
