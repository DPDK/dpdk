/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <math.h>

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

void
nix_rq_vwqe_flush(struct roc_nix_rq *rq, uint16_t vwqe_interval)
{
	uint64_t wait_ns;

	if (!roc_model_is_cn10k())
		return;
	/* Due to HW errata writes to VWQE_FLUSH might hang, so instead
	 * wait for max vwqe timeout interval.
	 */
	if (rq->vwqe_ena) {
		wait_ns = rq->vwqe_wait_tmo * (vwqe_interval + 1) * 100;
		plt_delay_us((wait_ns / 1E3) + 1);
	}
}

int
nix_rq_ena_dis(struct dev *dev, struct roc_nix_rq *rq, bool enable)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	/* Pkts will be dropped silently if RQ is disabled */
	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.ena = enable;
		aq->rq_mask.ena = ~(aq->rq_mask.ena);
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.ena = enable;
		aq->rq_mask.ena = ~(aq->rq_mask.ena);
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_rq_ena_dis(struct roc_nix_rq *rq, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(rq->roc_nix);
	int rc;

	rc = nix_rq_ena_dis(&nix->dev, rq, enable);
	nix_rq_vwqe_flush(rq, nix->vwqe_interval);
	if (rc)
		return rc;

	/* Check for meta aura if RQ is enabled */
	if (enable && nix->need_meta_aura)
		rc = roc_nix_inl_meta_aura_check(rq->roc_nix, rq);
	return rc;
}

int
roc_nix_rq_is_sso_enable(struct roc_nix *roc_nix, uint32_t qid)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	bool sso_enable;
	int rc;

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_rsp *rsp;
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_READ;
		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;

		sso_enable = rsp->rq.sso_ena;
	} else {
		struct nix_cn10k_aq_enq_rsp *rsp;
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;

		sso_enable = rsp->rq.sso_ena;
	}

	rc = sso_enable ? true : false;
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_rq_aura_buf_type_update(struct roc_nix_rq *rq, bool set)
{
	struct roc_nix *roc_nix = rq->roc_nix;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	bool inl_inb_ena = roc_nix_inl_inb_is_enabled(roc_nix);
	uint64_t lpb_aura = 0, vwqe_aura = 0, spb_aura = 0;
	struct mbox *mbox = nix->dev.mbox;
	uint64_t aura_base;
	int rc, count;

	count = set ? 1 : -1;
	/* For buf type set, use info from RQ context */
	if (set) {
		lpb_aura = rq->aura_handle;
		spb_aura = rq->spb_ena ? rq->spb_aura_handle : 0;
		vwqe_aura = rq->vwqe_ena ? rq->vwqe_aura_handle : 0;
		goto skip_ctx_read;
	}

	aura_base = roc_npa_aura_handle_to_base(rq->aura_handle);
	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_rsp *rsp;
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox_get(mbox));
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_READ;
		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			mbox_put(mbox);
			return rc;
		}

		/* Get aura handle from aura */
		lpb_aura = roc_npa_aura_handle_gen(rsp->rq.lpb_aura, aura_base);
		if (rsp->rq.spb_ena)
			spb_aura = roc_npa_aura_handle_gen(rsp->rq.spb_aura, aura_base);
		mbox_put(mbox);
	} else {
		struct nix_cn10k_aq_enq_rsp *rsp;
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox_get(mbox));
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

		aq->qidx = rq->qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			mbox_put(mbox);
			return rc;
		}

		/* Get aura handle from aura */
		lpb_aura = roc_npa_aura_handle_gen(rsp->rq.lpb_aura, aura_base);
		if (rsp->rq.spb_ena)
			spb_aura = roc_npa_aura_handle_gen(rsp->rq.spb_aura, aura_base);
		if (rsp->rq.vwqe_ena)
			vwqe_aura = roc_npa_aura_handle_gen(rsp->rq.wqe_aura, aura_base);

		mbox_put(mbox);
	}

skip_ctx_read:
	/* Update attributes for LPB aura */
	if (inl_inb_ena)
		roc_npa_buf_type_update(lpb_aura, ROC_NPA_BUF_TYPE_PACKET_IPSEC, count);
	else
		roc_npa_buf_type_update(lpb_aura, ROC_NPA_BUF_TYPE_PACKET, count);

	/* Update attributes for SPB aura */
	if (spb_aura) {
		if (inl_inb_ena)
			roc_npa_buf_type_update(spb_aura, ROC_NPA_BUF_TYPE_PACKET_IPSEC, count);
		else
			roc_npa_buf_type_update(spb_aura, ROC_NPA_BUF_TYPE_PACKET, count);
	}

	/* Update attributes for VWQE aura */
	if (vwqe_aura) {
		if (inl_inb_ena)
			roc_npa_buf_type_update(vwqe_aura, ROC_NPA_BUF_TYPE_VWQE_IPSEC, count);
		else
			roc_npa_buf_type_update(vwqe_aura, ROC_NPA_BUF_TYPE_VWQE, count);
	}

	return 0;
}

static int
nix_rq_cn9k_cman_cfg(struct dev *dev, struct roc_nix_rq *rq)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_aq_enq_req *aq;
	int rc;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq) {
		rc = -ENOSPC;
		goto exit;
	}

	aq->qidx = rq->qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
		aq->rq.lpb_pool_pass = rq->red_pass;
		aq->rq.lpb_pool_drop = rq->red_drop;
		aq->rq_mask.lpb_pool_pass = ~(aq->rq_mask.lpb_pool_pass);
		aq->rq_mask.lpb_pool_drop = ~(aq->rq_mask.lpb_pool_drop);

	}

	if (rq->spb_red_pass && (rq->spb_red_pass >= rq->spb_red_drop)) {
		aq->rq.spb_pool_pass = rq->spb_red_pass;
		aq->rq.spb_pool_drop = rq->spb_red_drop;
		aq->rq_mask.spb_pool_pass = ~(aq->rq_mask.spb_pool_pass);
		aq->rq_mask.spb_pool_drop = ~(aq->rq_mask.spb_pool_drop);

	}

	if (rq->xqe_red_pass && (rq->xqe_red_pass >= rq->xqe_red_drop)) {
		aq->rq.xqe_pass = rq->xqe_red_pass;
		aq->rq.xqe_drop = rq->xqe_red_drop;
		aq->rq_mask.xqe_drop = ~(aq->rq_mask.xqe_drop);
		aq->rq_mask.xqe_pass = ~(aq->rq_mask.xqe_pass);
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_rq_cn9k_cfg(struct dev *dev, struct roc_nix_rq *rq, uint16_t qints,
		bool cfg, bool ena)
{
	struct mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq)
		return -ENOSPC;

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
		aq->rq.cq = rq->cqid;
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
	aq->rq.qint_idx = rq->qid % qints;
	aq->rq.xqe_drop_ena = 1;

	/* If RED enabled, then fill enable for all cases */
	if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
		aq->rq.spb_pool_pass = rq->spb_red_pass;
		aq->rq.lpb_pool_pass = rq->red_pass;

		aq->rq.spb_pool_drop = rq->spb_red_drop;
		aq->rq.lpb_pool_drop = rq->red_drop;
	}

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

		if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
			aq->rq_mask.spb_pool_pass = ~aq->rq_mask.spb_pool_pass;
			aq->rq_mask.lpb_pool_pass = ~aq->rq_mask.lpb_pool_pass;

			aq->rq_mask.spb_pool_drop = ~aq->rq_mask.spb_pool_drop;
			aq->rq_mask.lpb_pool_drop = ~aq->rq_mask.lpb_pool_drop;
		}
	}

	return 0;
}

int
nix_rq_cfg(struct dev *dev, struct roc_nix_rq *rq, uint16_t qints, bool cfg,
	   bool ena)
{
	struct nix_cn10k_aq_enq_req *aq;
	struct mbox *mbox = dev->mbox;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq)
		return -ENOSPC;

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
			aq->rq.wqe_aura = roc_npa_aura_handle_to_aura(rq->vwqe_aura_handle);
		}
	} else {
		/* CQ mode */
		aq->rq.sso_ena = 0;
		aq->rq.good_utag = rq->tag_mask >> 24;
		aq->rq.bad_utag = rq->tag_mask >> 24;
		aq->rq.ltag = rq->tag_mask & BITMASK_ULL(24, 0);
		aq->rq.cq = rq->cqid;
	}

	if (rq->ipsech_ena) {
		aq->rq.ipsech_ena = 1;
		aq->rq.ipsecd_drop_en = 1;
	}

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
	aq->rq.qint_idx = rq->qid % qints;
	aq->rq.xqe_drop_ena = 0;
	aq->rq.lpb_drop_ena = rq->lpb_drop_ena;
	aq->rq.spb_drop_ena = rq->spb_drop_ena;

	/* If RED enabled, then fill enable for all cases */
	if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
		aq->rq.spb_pool_pass = rq->spb_red_pass;
		aq->rq.lpb_pool_pass = rq->red_pass;
		aq->rq.wqe_pool_pass = rq->red_pass;
		aq->rq.xqe_pass = rq->red_pass;

		aq->rq.spb_pool_drop = rq->spb_red_drop;
		aq->rq.lpb_pool_drop = rq->red_drop;
		aq->rq.wqe_pool_drop = rq->red_drop;
		aq->rq.xqe_drop = rq->red_drop;
	}

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
		aq->rq_mask.lpb_drop_ena = ~aq->rq_mask.lpb_drop_ena;
		aq->rq_mask.spb_drop_ena = ~aq->rq_mask.spb_drop_ena;

		if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
			aq->rq_mask.spb_pool_pass = ~aq->rq_mask.spb_pool_pass;
			aq->rq_mask.lpb_pool_pass = ~aq->rq_mask.lpb_pool_pass;
			aq->rq_mask.wqe_pool_pass = ~aq->rq_mask.wqe_pool_pass;
			aq->rq_mask.xqe_pass = ~aq->rq_mask.xqe_pass;

			aq->rq_mask.spb_pool_drop = ~aq->rq_mask.spb_pool_drop;
			aq->rq_mask.lpb_pool_drop = ~aq->rq_mask.lpb_pool_drop;
			aq->rq_mask.wqe_pool_drop = ~aq->rq_mask.wqe_pool_drop;
			aq->rq_mask.xqe_drop = ~aq->rq_mask.xqe_drop;
		}
	}

	return 0;
}

static int
nix_rq_cman_cfg(struct dev *dev, struct roc_nix_rq *rq)
{
	struct nix_cn10k_aq_enq_req *aq;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq) {
		rc = -ENOSPC;
		goto exit;
	}

	aq->qidx = rq->qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	if (rq->red_pass && (rq->red_pass >= rq->red_drop)) {
		aq->rq.lpb_pool_pass = rq->red_pass;
		aq->rq.lpb_pool_drop = rq->red_drop;
		aq->rq_mask.lpb_pool_pass = ~(aq->rq_mask.lpb_pool_pass);
		aq->rq_mask.lpb_pool_drop = ~(aq->rq_mask.lpb_pool_drop);

	}

	if (rq->spb_red_pass && (rq->spb_red_pass >= rq->spb_red_drop)) {
		aq->rq.spb_pool_pass = rq->spb_red_pass;
		aq->rq.spb_pool_drop = rq->spb_red_drop;
		aq->rq_mask.spb_pool_pass = ~(aq->rq_mask.spb_pool_pass);
		aq->rq_mask.spb_pool_drop = ~(aq->rq_mask.spb_pool_drop);

	}

	if (rq->xqe_red_pass && (rq->xqe_red_pass >= rq->xqe_red_drop)) {
		aq->rq.xqe_pass = rq->xqe_red_pass;
		aq->rq.xqe_drop = rq->xqe_red_drop;
		aq->rq_mask.xqe_drop = ~(aq->rq_mask.xqe_drop);
		aq->rq_mask.xqe_pass = ~(aq->rq_mask.xqe_pass);
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_rq_init(struct roc_nix *roc_nix, struct roc_nix_rq *rq, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	bool is_cn9k = roc_model_is_cn9k();
	struct dev *dev = &nix->dev;
	int rc;

	if (roc_nix == NULL || rq == NULL) {
		mbox_put(mbox);
		return NIX_ERR_PARAM;
	}

	if (rq->qid >= nix->nb_rx_queues) {
		mbox_put(mbox);
		return NIX_ERR_QUEUE_INVALID_RANGE;
	}

	rq->roc_nix = roc_nix;
	rq->tc = ROC_NIX_PFC_CLASS_INVALID;

	if (is_cn9k)
		rc = nix_rq_cn9k_cfg(dev, rq, nix->qints, false, ena);
	else
		rc = nix_rq_cfg(dev, rq, nix->qints, false, ena);

	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}
	mbox_put(mbox);

	/* Update aura buf type to indicate its use */
	nix_rq_aura_buf_type_update(rq, true);

	/* Check for meta aura if RQ is enabled */
	if (ena && nix->need_meta_aura) {
		rc = roc_nix_inl_meta_aura_check(roc_nix, rq);
		if (rc)
			return rc;
	}

	nix->rqs[rq->qid] = rq;
	return nix_tel_node_add_rq(rq);
}

int
roc_nix_rq_modify(struct roc_nix *roc_nix, struct roc_nix_rq *rq, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *m_box = (&nix->dev)->mbox;
	bool is_cn9k = roc_model_is_cn9k();
	struct dev *dev = &nix->dev;
	struct mbox *mbox;
	int rc;

	if (roc_nix == NULL || rq == NULL)
		return NIX_ERR_PARAM;

	if (rq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	/* Clear attributes for existing aura's */
	nix_rq_aura_buf_type_update(rq, false);

	rq->roc_nix = roc_nix;
	rq->tc = ROC_NIX_PFC_CLASS_INVALID;

	mbox = mbox_get(m_box);
	if (is_cn9k)
		rc = nix_rq_cn9k_cfg(dev, rq, nix->qints, true, ena);
	else
		rc = nix_rq_cfg(dev, rq, nix->qints, true, ena);

	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}
	mbox_put(mbox);

	/* Update aura attribute to indicate its use */
	nix_rq_aura_buf_type_update(rq, true);

	/* Check for meta aura if RQ is enabled */
	if (ena && nix->need_meta_aura) {
		rc = roc_nix_inl_meta_aura_check(roc_nix, rq);
		if (rc)
			return rc;
	}

	return nix_tel_node_add_rq(rq);
}

int
roc_nix_rq_cman_config(struct roc_nix *roc_nix, struct roc_nix_rq *rq)
{
	bool is_cn9k = roc_model_is_cn9k();
	struct nix *nix;
	struct dev *dev;
	int rc;

	if (roc_nix == NULL || rq == NULL)
		return NIX_ERR_PARAM;

	nix = roc_nix_to_nix_priv(roc_nix);

	if (rq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	dev = &nix->dev;

	if (is_cn9k)
		rc = nix_rq_cn9k_cman_cfg(dev, rq);
	else
		rc = nix_rq_cman_cfg(dev, rq);

	return rc;
}

int
roc_nix_rq_fini(struct roc_nix_rq *rq)
{
	struct nix *nix = roc_nix_to_nix_priv(rq->roc_nix);
	int rc;

	/* Disabling RQ is sufficient */
	rc = roc_nix_rq_ena_dis(rq, false);
	if (rc)
		return rc;

	/* Update aura attribute to indicate its use for */
	nix_rq_aura_buf_type_update(rq, false);

	nix->rqs[rq->qid] = NULL;
	return 0;
}

int
roc_nix_cq_init(struct roc_nix *roc_nix, struct roc_nix_cq *cq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	volatile struct nix_cq_ctx_s *cq_ctx;
	uint16_t drop_thresh = NIX_CQ_THRESH_LEVEL;
	uint16_t cpt_lbpid = nix->cpt_lbpid;
	enum nix_q_size qsize;
	size_t desc_sz;
	int rc;

	if (cq == NULL)
		return NIX_ERR_PARAM;

	qsize = nix_qsize_clampup(cq->nb_desc);
	cq->nb_desc = nix_qsize_to_val(qsize);
	cq->qmask = cq->nb_desc - 1;
	cq->door = nix->base + NIX_LF_CQ_OP_DOOR;
	cq->status = (int64_t *)(nix->base + NIX_LF_CQ_OP_STATUS);
	cq->wdata = (uint64_t)cq->qid << 32;
	cq->roc_nix = roc_nix;

	/* CQE of W16 */
	desc_sz = cq->nb_desc * NIX_CQ_ENTRY_SZ;
	cq->desc_base = plt_zmalloc(desc_sz, NIX_CQ_ALIGN);
	if (cq->desc_base == NULL) {
		rc = NIX_ERR_NO_MEM;
		goto fail;
	}

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox_get(mbox));
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_INIT;
		cq_ctx = &aq->cq;
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox_get(mbox));
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

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
	if (roc_feature_nix_has_late_bp() && roc_nix_inl_inb_is_enabled(roc_nix)) {
		cq_ctx->cq_err_int_ena |= BIT(NIX_CQERRINT_CPT_DROP);
		cq_ctx->cpt_drop_err_en = 1;
		/* Enable Late BP only when non zero CPT BPID */
		if (cpt_lbpid) {
			cq_ctx->lbp_ena = 1;
			cq_ctx->lbpid_low = cpt_lbpid & 0x7;
			cq_ctx->lbpid_med = (cpt_lbpid >> 3) & 0x7;
			cq_ctx->lbpid_high = (cpt_lbpid >> 6) & 0x7;
			cq_ctx->lbp_frac = NIX_CQ_LPB_THRESH_FRAC;
		}
		drop_thresh = NIX_CQ_SEC_THRESH_LEVEL;
	}

	/* Many to one reduction */
	cq_ctx->qint_idx = cq->qid % nix->qints;
	/* Map CQ0 [RQ0] to CINT0 and so on till max 64 irqs */
	cq_ctx->cint_idx = cq->qid;

	if (roc_errata_nix_has_cq_min_size_4k()) {
		const float rx_cq_skid = NIX_CQ_FULL_ERRATA_SKID;
		uint16_t min_rx_drop;

		min_rx_drop = ceil(rx_cq_skid / (float)cq->nb_desc);
		cq_ctx->drop = min_rx_drop;
		cq_ctx->drop_ena = 1;
		cq->drop_thresh = min_rx_drop;
	} else {
		cq->drop_thresh = drop_thresh;
		/* Drop processing or red drop cannot be enabled due to
		 * due to packets coming for second pass from CPT.
		 */
		if (!roc_nix_inl_inb_is_enabled(roc_nix)) {
			cq_ctx->drop = cq->drop_thresh;
			cq_ctx->drop_ena = 1;
		}
	}
	cq_ctx->bp = cq->drop_thresh;

	rc = mbox_process(mbox);
	mbox_put(mbox);
	if (rc)
		goto free_mem;

	return nix_tel_node_add_cq(cq);

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
	mbox = mbox_get((&nix->dev)->mbox);

	/* Disable CQ */
	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

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
		if (!aq) {
			mbox_put(mbox);
			return -ENOSPC;
		}

		aq->qidx = cq->qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;
		aq->cq.ena = 0;
		aq->cq.bp_ena = 0;
		aq->cq_mask.ena = ~aq->cq_mask.ena;
		aq->cq_mask.bp_ena = ~aq->cq_mask.bp_ena;
		if (roc_feature_nix_has_late_bp() && roc_nix_inl_inb_is_enabled(cq->roc_nix)) {
			aq->cq.lbp_ena = 0;
			aq->cq_mask.lbp_ena = ~aq->cq_mask.lbp_ena;
		}
	}

	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	mbox_put(mbox);
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

	sq->nb_desc = PLT_MAX(512U, sq->nb_desc);
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
		(sq->nb_sqb_bufs_adj * ROC_NIX_SQB_LOWER_THRESH) / 100;

	nb_sqb_bufs += roc_nix->sqb_slack;
	/* Explicitly set nat_align alone as by default pool is with both
	 * nat_align and buf_offset = 1 which we don't want for SQB.
	 */
	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;

	memset(&aura, 0, sizeof(aura));
	aura.fc_ena = 1;
	if (roc_model_is_cn9k() || roc_errata_npa_has_no_fc_stype_ststp())
		aura.fc_stype = 0x0; /* STF */
	else
		aura.fc_stype = 0x3; /* STSTP */
	aura.fc_addr = (uint64_t)sq->fc;
	aura.fc_hyst_bits = sq->fc_hyst_bits & 0xF;
	rc = roc_npa_pool_create(&sq->aura_handle, blk_sz, nb_sqb_bufs, &aura, &pool, 0);
	if (rc)
		goto fail;

	roc_npa_buf_type_update(sq->aura_handle, ROC_NPA_BUF_TYPE_SQB, 1);
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

	if (roc_npa_aura_op_available_wait(sq->aura_handle, nb_sqb_bufs, 0) !=
	    nb_sqb_bufs) {
		plt_err("Failed to free all pointers to the pool");
		rc = NIX_ERR_NO_MEM;
		goto npa_fail;
	}

	roc_npa_aura_op_range_set(sq->aura_handle, (uint64_t)sq->sqe_mem, iova);
	roc_npa_aura_limit_modify(sq->aura_handle, nb_sqb_bufs);
	sq->aura_sqb_bufs = nb_sqb_bufs;

	return rc;
npa_fail:
	plt_free(sq->sqe_mem);
nomem:
	roc_npa_pool_destroy(sq->aura_handle);
fail:
	return rc;
}

static int
sq_cn9k_init(struct nix *nix, struct roc_nix_sq *sq, uint32_t rr_quantum,
	     uint16_t smq)
{
	struct roc_nix *roc_nix = nix_priv_to_roc_nix(nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq)
		return -ENOSPC;

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_INIT;
	aq->sq.max_sqe_size = sq->max_sqe_sz;

	aq->sq.max_sqe_size = sq->max_sqe_sz;
	aq->sq.smq = smq;
	aq->sq.smq_rr_quantum = rr_quantum;
	if (roc_nix_is_sdp(roc_nix))
		aq->sq.default_chan =
			nix->tx_chan_base + (sq->qid % nix->tx_chan_cnt);
	else
		aq->sq.default_chan = nix->tx_chan_base;
	aq->sq.sqe_stype = NIX_STYPE_STF;
	aq->sq.ena = 1;
	aq->sq.sso_ena = !!sq->sso_ena;
	aq->sq.cq_ena = !!sq->cq_ena;
	aq->sq.cq = sq->cqid;
	aq->sq.cq_limit = sq->cq_drop_thresh;
	if (aq->sq.max_sqe_size == NIX_MAXSQESZ_W8)
		aq->sq.sqe_stype = NIX_STYPE_STP;
	aq->sq.sqb_aura = roc_npa_aura_handle_to_aura(sq->aura_handle);
	aq->sq.sq_int_ena = BIT(NIX_SQINT_LMT_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SQB_ALLOC_FAIL);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SEND_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_MNQ_ERR);

	/* Many to one reduction */
	aq->sq.qint_idx = sq->qid % nix->qints;

	return 0;
}

static int
sq_cn9k_fini(struct nix *nix, struct roc_nix_sq *sq)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_aq_enq_rsp *rsp;
	struct nix_aq_enq_req *aq;
	uint16_t sqes_per_sqb;
	void *sqb_buf;
	int rc, count;

	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	/* Check if sq is already cleaned up */
	if (!rsp->sq.ena) {
		mbox_put(mbox);
		return 0;
	}

	/* Disable sq */
	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_WRITE;
	aq->sq_mask.ena = ~aq->sq_mask.ena;
	aq->sq.ena = 0;
	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	/* Read SQ and free sqb's */
	aq = mbox_alloc_msg_nix_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	if (aq->sq.smq_pend)
		plt_err("SQ has pending SQE's");

	count = aq->sq.sqb_count;
	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	sqb_buf = (void *)rsp->sq.head_sqb;
	while (count) {
		void *next_sqb;

		next_sqb = *(void **)((uint64_t *)sqb_buf +
				      (uint32_t)((sqes_per_sqb - 1) * (0x2 >> sq->max_sqe_sz) * 8));
		roc_npa_aura_op_free(sq->aura_handle, 1, (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		count--;
	}

	/* Free next to use sqb */
	if (rsp->sq.next_sqb)
		roc_npa_aura_op_free(sq->aura_handle, 1, rsp->sq.next_sqb);
	mbox_put(mbox);
	return 0;
}

static int
sq_init(struct nix *nix, struct roc_nix_sq *sq, uint32_t rr_quantum, uint16_t smq)
{
	struct roc_nix *roc_nix = nix_priv_to_roc_nix(nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_cn10k_aq_enq_req *aq;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq)
		return -ENOSPC;

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_INIT;
	aq->sq.max_sqe_size = sq->max_sqe_sz;

	aq->sq.max_sqe_size = sq->max_sqe_sz;
	aq->sq.smq = smq;
	aq->sq.smq_rr_weight = rr_quantum;
	if (roc_nix_is_sdp(roc_nix))
		aq->sq.default_chan = nix->tx_chan_base + (sq->qid % nix->tx_chan_cnt);
	else
		aq->sq.default_chan = nix->tx_chan_base;
	aq->sq.sqe_stype = NIX_STYPE_STF;
	aq->sq.ena = 1;
	aq->sq.sso_ena = !!sq->sso_ena;
	aq->sq.cq_ena = !!sq->cq_ena;
	aq->sq.cq = sq->cqid;
	aq->sq.cq_limit = sq->cq_drop_thresh;
	if (aq->sq.max_sqe_size == NIX_MAXSQESZ_W8)
		aq->sq.sqe_stype = NIX_STYPE_STP;
	aq->sq.sqb_aura = roc_npa_aura_handle_to_aura(sq->aura_handle);
	aq->sq.sq_int_ena = BIT(NIX_SQINT_LMT_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SQB_ALLOC_FAIL);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_SEND_ERR);
	aq->sq.sq_int_ena |= BIT(NIX_SQINT_MNQ_ERR);

	/* Many to one reduction */
	aq->sq.qint_idx = sq->qid % nix->qints;
	if (roc_errata_nix_assign_incorrect_qint()) {
		/* Assigning QINT 0 to all the SQs, an errata exists where NIXTX can
		 * send incorrect QINT_IDX when reporting queue interrupt (QINT). This
		 * might result in software missing the interrupt.
		 */
		aq->sq.qint_idx = 0;
	}
	return 0;
}

static int
sq_fini(struct nix *nix, struct roc_nix_sq *sq)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_cn10k_aq_enq_rsp *rsp;
	struct nix_cn10k_aq_enq_req *aq;
	uint16_t sqes_per_sqb;
	void *sqb_buf;
	int rc, count;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	/* Check if sq is already cleaned up */
	if (!rsp->sq.ena) {
		mbox_put(mbox);
		return 0;
	}

	/* Disable sq */
	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_WRITE;
	aq->sq_mask.ena = ~aq->sq_mask.ena;
	aq->sq.ena = 0;
	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	/* Read SQ and free sqb's */
	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (!aq) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	aq->qidx = sq->qid;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}

	if (aq->sq.smq_pend)
		plt_err("SQ has pending SQE's");

	count = aq->sq.sqb_count;
	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	sqb_buf = (void *)rsp->sq.head_sqb;
	while (count) {
		void *next_sqb;

		next_sqb = *(void **)((uint64_t *)sqb_buf +
				      (uint32_t)((sqes_per_sqb - 1) * (0x2 >> sq->max_sqe_sz) * 8));
		roc_npa_aura_op_free(sq->aura_handle, 1, (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		count--;
	}

	/* Free next to use sqb */
	if (rsp->sq.next_sqb)
		roc_npa_aura_op_free(sq->aura_handle, 1, rsp->sq.next_sqb);
	mbox_put(mbox);
	return 0;
}

int
roc_nix_sq_init(struct roc_nix *roc_nix, struct roc_nix_sq *sq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *m_box = (&nix->dev)->mbox;
	uint16_t qid, smq = UINT16_MAX;
	uint32_t rr_quantum = 0;
	struct mbox *mbox;
	int rc;

	if (sq == NULL)
		return NIX_ERR_PARAM;

	qid = sq->qid;
	if (qid >= nix->nb_tx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	sq->roc_nix = roc_nix;
	sq->tc = ROC_NIX_PFC_CLASS_INVALID;
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

	mbox = mbox_get(m_box);
	/* Init SQ context */
	if (roc_model_is_cn9k())
		rc = sq_cn9k_init(nix, sq, rr_quantum, smq);
	else
		rc = sq_init(nix, sq, rr_quantum, smq);

	if (rc) {
		mbox_put(mbox);
		goto nomem;
	}


	rc = mbox_process(mbox);
	if (rc) {
		mbox_put(mbox);
		goto nomem;
	}
	mbox_put(mbox);

	nix->sqs[qid] = sq;
	sq->io_addr = nix->base + NIX_LF_OP_SENDX(0);
	/* Evenly distribute LMT slot for each sq */
	if (roc_model_is_cn9k()) {
		/* Multiple cores/SQ's can use same LMTLINE safely in CN9K */
		sq->lmt_addr = (void *)(nix->lmt_base +
					((qid & RVU_CN9K_LMT_SLOT_MASK) << 12));
	}

	rc = nix_tel_node_add_sq(sq);
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
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox_get(mbox));
	if (ndc_req == NULL) {
		mbox_put(mbox);
		return -ENOSPC;
	}
	ndc_req->nix_lf_tx_sync = 1;
	if (mbox_process(mbox))
		rc |= NIX_ERR_NDC_SYNC;
	mbox_put(mbox);

	rc |= nix_tm_sq_flush_post(sq);

	/* Restore limit to max SQB count that the pool was created
	 * for aura drain to succeed.
	 */
	roc_npa_aura_limit_modify(sq->aura_handle, NIX_MAX_SQB);
	rc |= roc_npa_pool_destroy(sq->aura_handle);
	plt_free(sq->fc);
	plt_free(sq->sqe_mem);
	nix->sqs[qid] = NULL;

	return rc;
}

void
roc_nix_cq_head_tail_get(struct roc_nix *roc_nix, uint16_t qid, uint32_t *head,
			 uint32_t *tail)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint64_t reg, val;
	int64_t *addr;

	if (head == NULL || tail == NULL)
		return;

	reg = (((uint64_t)qid) << 32);
	addr = (int64_t *)(nix->base + NIX_LF_CQ_OP_STATUS);
	val = roc_atomic64_add_nosync(reg, addr);
	if (val &
	    (BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) | BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR)))
		val = 0;

	*tail = (uint32_t)(val & 0xFFFFF);
	*head = (uint32_t)((val >> 20) & 0xFFFFF);
}

void
roc_nix_sq_head_tail_get(struct roc_nix *roc_nix, uint16_t qid, uint32_t *head,
			 uint32_t *tail)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_nix_sq *sq = nix->sqs[qid];
	uint16_t sqes_per_sqb, sqb_cnt;
	uint64_t reg, val;
	int64_t *addr;

	if (head == NULL || tail == NULL)
		return;

	reg = (((uint64_t)qid) << 32);
	addr = (int64_t *)(nix->base + NIX_LF_SQ_OP_STATUS);
	val = roc_atomic64_add_nosync(reg, addr);
	if (val & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR)) {
		val = 0;
		return;
	}

	*tail = (uint32_t)((val >> 28) & 0x3F);
	*head = (uint32_t)((val >> 20) & 0x3F);
	sqb_cnt = (uint16_t)(val & 0xFFFF);

	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;

	/* Update tail index as per used sqb count */
	*tail += (sqes_per_sqb * (sqb_cnt - 1));
}

int
roc_nix_q_err_cb_register(struct roc_nix *roc_nix, q_err_get_t sq_err_handle)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	if (sq_err_handle == NULL)
		return NIX_ERR_PARAM;

	dev->ops->q_err_cb = (q_err_cb_t)sq_err_handle;
	return 0;
}

void
roc_nix_q_err_cb_unregister(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	dev->ops->q_err_cb = NULL;
}
