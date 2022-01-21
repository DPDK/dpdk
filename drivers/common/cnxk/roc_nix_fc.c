/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

static int
nix_fc_rxchan_bpid_get(struct roc_nix *roc_nix, struct roc_nix_fc_cfg *fc_cfg)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (nix->chan_cnt != 0)
		fc_cfg->rxchan_cfg.enable = true;
	else
		fc_cfg->rxchan_cfg.enable = false;

	fc_cfg->type = ROC_NIX_FC_RXCHAN_CFG;

	return 0;
}

static int
nix_fc_rxchan_bpid_set(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_bp_cfg_req *req;
	struct nix_bp_cfg_rsp *rsp;
	int rc = -ENOSPC;

	if (roc_nix_is_sdp(roc_nix))
		return 0;

	if (enable) {
		req = mbox_alloc_msg_nix_bp_enable(mbox);
		if (req == NULL)
			return rc;
		req->chan_base = 0;
		req->chan_cnt = 1;
		req->bpid_per_chan = 0;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc || (req->chan_cnt != rsp->chan_cnt))
			goto exit;

		nix->bpid[0] = rsp->chan_bpid[0];
		nix->chan_cnt = rsp->chan_cnt;
	} else {
		req = mbox_alloc_msg_nix_bp_disable(mbox);
		if (req == NULL)
			return rc;
		req->chan_base = 0;
		req->chan_cnt = 1;

		rc = mbox_process(mbox);
		if (rc)
			goto exit;

		memset(nix->bpid, 0, sizeof(uint16_t) * NIX_MAX_CHAN);
		nix->chan_cnt = 0;
	}

	if (roc_model_is_cn9k())
		goto exit;

	/* Enable backpressure on CPT if inline inb is enabled */
	if (enable && roc_nix_inl_inb_is_enabled(roc_nix)) {
		req = mbox_alloc_msg_nix_cpt_bp_enable(mbox);
		if (req == NULL)
			return rc;
		req->chan_base = 0;
		req->chan_cnt = 1;
		req->bpid_per_chan = 0;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;
	} else {
		req = mbox_alloc_msg_nix_cpt_bp_disable(mbox);
		if (req == NULL)
			return rc;
		req->chan_base = 0;
		req->chan_cnt = 1;
		req->bpid_per_chan = 0;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;
	}

exit:
	return rc;
}

static int
nix_fc_cq_config_get(struct roc_nix *roc_nix, struct roc_nix_fc_cfg *fc_cfg)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_aq_enq_rsp *rsp;
	int rc;

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq)
			return -ENOSPC;

		aq->qidx = fc_cfg->cq_cfg.rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_READ;
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq)
			return -ENOSPC;

		aq->qidx = fc_cfg->cq_cfg.rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_READ;
	}

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	fc_cfg->cq_cfg.cq_drop = rsp->cq.bp;
	fc_cfg->cq_cfg.enable = rsp->cq.bp_ena;
	fc_cfg->type = ROC_NIX_FC_CQ_CFG;

exit:
	return rc;
}

static int
nix_fc_cq_config_set(struct roc_nix *roc_nix, struct roc_nix_fc_cfg *fc_cfg)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq)
			return -ENOSPC;

		aq->qidx = fc_cfg->cq_cfg.rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		if (fc_cfg->cq_cfg.enable) {
			aq->cq.bpid = nix->bpid[0];
			aq->cq_mask.bpid = ~(aq->cq_mask.bpid);
			aq->cq.bp = fc_cfg->cq_cfg.cq_drop;
			aq->cq_mask.bp = ~(aq->cq_mask.bp);
		}

		aq->cq.bp_ena = !!(fc_cfg->cq_cfg.enable);
		aq->cq_mask.bp_ena = ~(aq->cq_mask.bp_ena);
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq)
			return -ENOSPC;

		aq->qidx = fc_cfg->cq_cfg.rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		if (fc_cfg->cq_cfg.enable) {
			aq->cq.bpid = nix->bpid[0];
			aq->cq_mask.bpid = ~(aq->cq_mask.bpid);
			aq->cq.bp = fc_cfg->cq_cfg.cq_drop;
			aq->cq_mask.bp = ~(aq->cq_mask.bp);
		}

		aq->cq.bp_ena = !!(fc_cfg->cq_cfg.enable);
		aq->cq_mask.bp_ena = ~(aq->cq_mask.bp_ena);
	}

	return mbox_process(mbox);
}

int
roc_nix_fc_config_get(struct roc_nix *roc_nix, struct roc_nix_fc_cfg *fc_cfg)
{
	if (roc_nix_is_vf_or_sdp(roc_nix) && !roc_nix_is_lbk(roc_nix))
		return 0;

	if (fc_cfg->type == ROC_NIX_FC_CQ_CFG)
		return nix_fc_cq_config_get(roc_nix, fc_cfg);
	else if (fc_cfg->type == ROC_NIX_FC_RXCHAN_CFG)
		return nix_fc_rxchan_bpid_get(roc_nix, fc_cfg);
	else if (fc_cfg->type == ROC_NIX_FC_TM_CFG)
		return nix_tm_bp_config_get(roc_nix, &fc_cfg->tm_cfg.enable);

	return -EINVAL;
}

int
roc_nix_fc_config_set(struct roc_nix *roc_nix, struct roc_nix_fc_cfg *fc_cfg)
{
	if (roc_nix_is_vf_or_sdp(roc_nix) && !roc_nix_is_lbk(roc_nix))
		return 0;

	if (fc_cfg->type == ROC_NIX_FC_CQ_CFG)
		return nix_fc_cq_config_set(roc_nix, fc_cfg);
	else if (fc_cfg->type == ROC_NIX_FC_RXCHAN_CFG)
		return nix_fc_rxchan_bpid_set(roc_nix,
					      fc_cfg->rxchan_cfg.enable);
	else if (fc_cfg->type == ROC_NIX_FC_TM_CFG)
		return nix_tm_bp_config_set(roc_nix, fc_cfg->tm_cfg.enable);

	return -EINVAL;
}

enum roc_nix_fc_mode
roc_nix_fc_mode_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct cgx_pause_frm_cfg *req, *rsp;
	enum roc_nix_fc_mode mode;
	int rc = -ENOSPC;

	/* Flow control on LBK link is always available */
	if (roc_nix_is_lbk(roc_nix)) {
		if (nix->tx_pause && nix->rx_pause)
			return ROC_NIX_FC_FULL;
		else if (nix->rx_pause)
			return ROC_NIX_FC_RX;
		else if (nix->tx_pause)
			return ROC_NIX_FC_TX;
		else
			return ROC_NIX_FC_NONE;
	}

	req = mbox_alloc_msg_cgx_cfg_pause_frm(mbox);
	if (req == NULL)
		return rc;
	req->set = 0;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	if (rsp->rx_pause && rsp->tx_pause)
		mode = ROC_NIX_FC_FULL;
	else if (rsp->rx_pause)
		mode = ROC_NIX_FC_RX;
	else if (rsp->tx_pause)
		mode = ROC_NIX_FC_TX;
	else
		mode = ROC_NIX_FC_NONE;

	nix->rx_pause = rsp->rx_pause;
	nix->tx_pause = rsp->tx_pause;
	return mode;

exit:
	return ROC_NIX_FC_NONE;
}

int
roc_nix_fc_mode_set(struct roc_nix *roc_nix, enum roc_nix_fc_mode mode)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct cgx_pause_frm_cfg *req;
	uint8_t tx_pause, rx_pause;
	int rc = -ENOSPC;

	rx_pause = (mode == ROC_NIX_FC_FULL) || (mode == ROC_NIX_FC_RX);
	tx_pause = (mode == ROC_NIX_FC_FULL) || (mode == ROC_NIX_FC_TX);

	/* Nothing much to do for LBK links */
	if (roc_nix_is_lbk(roc_nix)) {
		nix->rx_pause = rx_pause;
		nix->tx_pause = tx_pause;
		return 0;
	}

	req = mbox_alloc_msg_cgx_cfg_pause_frm(mbox);
	if (req == NULL)
		return rc;
	req->set = 1;
	req->rx_pause = rx_pause;
	req->tx_pause = tx_pause;

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	nix->rx_pause = rx_pause;
	nix->tx_pause = tx_pause;

exit:
	return rc;
}

void
rox_nix_fc_npa_bp_cfg(struct roc_nix *roc_nix, uint64_t pool_id, uint8_t ena,
		      uint8_t force)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct npa_lf *lf = idev_npa_obj_get();
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	struct mbox *mbox;
	uint32_t limit;
	int rc;

	if (roc_nix_is_sdp(roc_nix))
		return;

	if (!lf)
		return;
	mbox = lf->mbox;

	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return;

	req->aura_id = roc_npa_aura_handle_to_aura(pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return;

	limit = rsp->aura.limit;
	/* BP is already enabled. */
	if (rsp->aura.bp_ena) {
		uint16_t bpid;
		bool nix1;

		nix1 = !!(rsp->aura.bp_ena & 0x2);
		if (nix1)
			bpid = rsp->aura.nix1_bpid;
		else
			bpid = rsp->aura.nix0_bpid;

		/* If BP ids don't match disable BP. */
		if (((nix1 != nix->is_nix1) || (bpid != nix->bpid[0])) &&
		    !force) {
			req = mbox_alloc_msg_npa_aq_enq(mbox);
			if (req == NULL)
				return;

			req->aura_id = roc_npa_aura_handle_to_aura(pool_id);
			req->ctype = NPA_AQ_CTYPE_AURA;
			req->op = NPA_AQ_INSTOP_WRITE;

			req->aura.bp_ena = 0;
			req->aura_mask.bp_ena = ~(req->aura_mask.bp_ena);

			mbox_process(mbox);
		}
		return;
	}

	/* BP was previously enabled but now disabled skip. */
	if (rsp->aura.bp)
		return;

	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return;

	req->aura_id = roc_npa_aura_handle_to_aura(pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_WRITE;

	if (ena) {
		if (nix->is_nix1) {
			req->aura.nix1_bpid = nix->bpid[0];
			req->aura_mask.nix1_bpid = ~(req->aura_mask.nix1_bpid);
		} else {
			req->aura.nix0_bpid = nix->bpid[0];
			req->aura_mask.nix0_bpid = ~(req->aura_mask.nix0_bpid);
		}
		req->aura.bp = NIX_RQ_AURA_THRESH(
			limit > 128 ? 256 : limit); /* 95% of size*/
		req->aura_mask.bp = ~(req->aura_mask.bp);
	}

	req->aura.bp_ena = (!!ena << nix->is_nix1);
	req->aura_mask.bp_ena = ~(req->aura_mask.bp_ena);

	mbox_process(mbox);
}
