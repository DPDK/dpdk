/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline int
bitmap_ctzll(uint64_t slab)
{
	if (slab == 0)
		return 0;

	return plt_ctz64(slab);
}

void
nix_tm_clear_shaper_profiles(struct nix *nix)
{
	struct nix_tm_shaper_profile *shaper_profile, *tmp;
	struct nix_tm_shaper_profile_list *list;

	list = &nix->shaper_profile_list;
	PLT_TAILQ_FOREACH_SAFE(shaper_profile, list, shaper, tmp) {
		if (shaper_profile->ref_cnt)
			plt_warn("Shaper profile %u has non zero references",
				 shaper_profile->id);
		TAILQ_REMOVE(&nix->shaper_profile_list, shaper_profile, shaper);
		nix_tm_shaper_profile_free(shaper_profile);
	}
}

static int
nix_tm_node_reg_conf(struct nix *nix, struct nix_tm_node *node)
{
	uint64_t regval_mask[MAX_REGS_PER_MBOX_MSG];
	uint64_t regval[MAX_REGS_PER_MBOX_MSG];
	struct nix_tm_shaper_profile *profile;
	uint64_t reg[MAX_REGS_PER_MBOX_MSG];
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req;
	int rc = -EFAULT;
	uint32_t hw_lvl;
	uint8_t k = 0;

	memset(regval, 0, sizeof(regval));
	memset(regval_mask, 0, sizeof(regval_mask));

	profile = nix_tm_shaper_profile_search(nix, node->shaper_profile_id);
	hw_lvl = node->hw_lvl;

	/* Need this trigger to configure TL1 */
	if (!nix_tm_have_tl1_access(nix) && hw_lvl == NIX_TXSCH_LVL_TL2) {
		/* Prepare default conf for TL1 */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
		req->lvl = NIX_TXSCH_LVL_TL1;

		k = nix_tm_tl1_default_prep(nix, node->parent_hw_id, req->reg,
					    req->regval);
		req->num_regs = k;
		rc = mbox_process(mbox);
		mbox_put(mbox);
		if (rc)
			goto error;
	}

	/* Prepare topology config */
	k = nix_tm_topology_reg_prep(nix, node, reg, regval, regval_mask);

	/* Prepare schedule config */
	k += nix_tm_sched_reg_prep(nix, node, &reg[k], &regval[k]);

	/* Prepare shaping config */
	k += nix_tm_shaper_reg_prep(node, profile, &reg[k], &regval[k]);

	if (!k)
		return 0;

	/* Copy and send config mbox */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
	req->lvl = hw_lvl;
	req->num_regs = k;

	mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
	mbox_memcpy(req->regval, regval, sizeof(uint64_t) * k);
	mbox_memcpy(req->regval_mask, regval_mask, sizeof(uint64_t) * k);

	rc = mbox_process(mbox);
	mbox_put(mbox);
	if (rc)
		goto error;

	return 0;
error:
	plt_err("Txschq conf failed for node %p, rc=%d", node, rc);
	return rc;
}

int
nix_tm_txsch_reg_config(struct nix *nix, enum roc_nix_tm_tree tree)
{
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	uint32_t hw_lvl;
	int rc = 0;

	list = nix_tm_node_list(nix, tree);

	for (hw_lvl = 0; hw_lvl <= nix->tm_root_lvl; hw_lvl++) {
		TAILQ_FOREACH(node, list, node) {
			if (node->hw_lvl != hw_lvl)
				continue;

			/* Only one TL3/TL2 Link config should have BP enable
			 * set per channel only for PF or lbk vf.
			 */
			node->bp_capa = 0;
			if (!nix->sdp_link && node->hw_lvl == nix->tm_link_cfg_lvl)
				node->bp_capa = 1;

			rc = nix_tm_node_reg_conf(nix, node);
			if (rc)
				goto exit;
		}
	}
exit:
	return rc;
}

int
nix_tm_update_parent_info(struct nix *nix, enum roc_nix_tm_tree tree)
{
	struct nix_tm_node *child, *parent;
	struct nix_tm_node_list *list;
	uint32_t rr_prio, max_prio;
	uint32_t rr_num = 0;

	list = nix_tm_node_list(nix, tree);

	/* Release all the node hw resources locally
	 * if parent marked as dirty and resource exists.
	 */
	TAILQ_FOREACH(child, list, node) {
		/* Release resource only if parent direct hierarchy changed */
		if (child->flags & NIX_TM_NODE_HWRES && child->parent &&
		    child->parent->child_realloc) {
			nix_tm_free_node_resource(nix, child);
		}
		child->max_prio = UINT32_MAX;
	}

	TAILQ_FOREACH(parent, list, node) {
		/* Count group of children of same priority i.e are RR */
		rr_num = nix_tm_check_rr(nix, parent->id, tree, &rr_prio,
					 &max_prio);

		/* Assuming that multiple RR groups are
		 * not configured based on capability.
		 */
		parent->rr_prio = rr_prio;
		parent->rr_num = rr_num;
		parent->max_prio = max_prio;
	}

	return 0;
}

static int
nix_tm_root_node_get(struct nix *nix, int tree)
{
	struct nix_tm_node_list *list = nix_tm_node_list(nix, tree);
	struct nix_tm_node *tm_node;

	TAILQ_FOREACH(tm_node, list, node) {
		if (tm_node->hw_lvl == nix->tm_root_lvl)
			return 1;
	}

	return 0;
}

int
nix_tm_node_add(struct roc_nix *roc_nix, struct nix_tm_node *node)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile;
	uint32_t node_id, parent_id, lvl;
	struct nix_tm_node *parent_node;
	uint32_t priority, profile_id;
	uint8_t hw_lvl, exp_next_lvl;
	enum roc_nix_tm_tree tree;
	int rc;

	node_id = node->id;
	priority = node->priority;
	parent_id = node->parent_id;
	profile_id = node->shaper_profile_id;
	lvl = node->lvl;
	tree = node->tree;

	plt_tm_dbg("Add node %s lvl %u id %u, prio 0x%x weight 0x%x "
		   "parent %u profile 0x%x tree %u",
		   nix_tm_hwlvl2str(nix_tm_lvl2nix(nix, lvl)), lvl, node_id,
		   priority, node->weight, parent_id, profile_id, tree);

	if (tree >= ROC_NIX_TM_TREE_MAX)
		return NIX_ERR_PARAM;

	/* Translate sw level id's to nix hw level id's */
	hw_lvl = nix_tm_lvl2nix(nix, lvl);
	if (hw_lvl == NIX_TXSCH_LVL_CNT && !nix_tm_is_leaf(nix, lvl))
		return NIX_ERR_TM_INVALID_LVL;

	/* Leaf nodes have to be same priority */
	if (nix_tm_is_leaf(nix, lvl) && priority != 0)
		return NIX_ERR_TM_INVALID_PRIO;

	parent_node = nix_tm_node_search(nix, parent_id, tree);

	if (node_id < nix->nb_tx_queues)
		exp_next_lvl = NIX_TXSCH_LVL_SMQ;
	else
		exp_next_lvl = hw_lvl + 1;

	/* Check if there is no parent node yet */
	if (hw_lvl != nix->tm_root_lvl &&
	    (!parent_node || parent_node->hw_lvl != exp_next_lvl))
		return NIX_ERR_TM_INVALID_PARENT;

	/* Check if a node already exists */
	if (nix_tm_node_search(nix, node_id, tree))
		return NIX_ERR_TM_NODE_EXISTS;

	/* Check if root node exists */
	if (hw_lvl == nix->tm_root_lvl && nix_tm_root_node_get(nix, tree))
		return NIX_ERR_TM_NODE_EXISTS;

	profile = nix_tm_shaper_profile_search(nix, profile_id);
	if (!nix_tm_is_leaf(nix, lvl)) {
		/* Check if shaper profile exists for non leaf node */
		if (!profile && profile_id != ROC_NIX_TM_SHAPER_PROFILE_NONE)
			return NIX_ERR_TM_INVALID_SHAPER_PROFILE;

		/* Packet mode in profile should match with that of tm node */
		if (profile && profile->pkt_mode != node->pkt_mode)
			return NIX_ERR_TM_PKT_MODE_MISMATCH;
	}

	/* Check if there is second DWRR already in siblings or holes in prio */
	rc = nix_tm_validate_prio(nix, lvl, parent_id, priority, tree);
	if (rc)
		return rc;

	if (node->weight > roc_nix_tm_max_sched_wt_get())
		return NIX_ERR_TM_WEIGHT_EXCEED;

	node->hw_lvl = nix_tm_lvl2nix(nix, lvl);
	node->rr_prio = 0xF;
	node->max_prio = UINT32_MAX;
	node->hw_id = NIX_TM_HW_ID_INVALID;
	node->flags = 0;

	if (profile)
		profile->ref_cnt++;

	node->parent = parent_node;
	if (parent_node)
		parent_node->child_realloc = true;
	node->parent_hw_id = NIX_TM_HW_ID_INVALID;

	TAILQ_INSERT_TAIL(&nix->trees[tree], node, node);
	plt_tm_dbg("Added node %s lvl %u id %u (%p)",
		   nix_tm_hwlvl2str(node->hw_lvl), lvl, node_id, node);
	return 0;
}

int
nix_tm_clear_path_xoff(struct nix *nix, struct nix_tm_node *node)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_txschq_config *req;
	struct nix_tm_node *p;
	int rc;

	/* Enable nodes in path for flush to succeed */
	if (!nix_tm_is_leaf(nix, node->lvl))
		p = node;
	else
		p = node->parent;
	while (p) {
		if (!(p->flags & NIX_TM_NODE_ENABLED) &&
		    (p->flags & NIX_TM_NODE_HWRES)) {
			req = mbox_alloc_msg_nix_txschq_cfg(mbox);
			req->lvl = p->hw_lvl;
			req->num_regs = nix_tm_sw_xoff_prep(p, false, req->reg,
							    req->regval);
			rc = mbox_process(mbox);
			if (rc)
				goto exit;

			p->flags |= NIX_TM_NODE_ENABLED;
		}
		p = p->parent;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_tm_bp_config_set(struct roc_nix *roc_nix, uint16_t sq, uint16_t tc,
		     bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	enum roc_nix_tm_tree tree = nix->tm_tree;
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req = NULL;
	struct nix_tm_node_list *list;
	uint16_t link = nix->tx_link;
	struct nix_tm_node *sq_node;
	struct nix_tm_node *parent;
	struct nix_tm_node *node;
	struct roc_nix_sq *sq_s;
	uint16_t rel_chan = 0;
	uint8_t parent_lvl;
	uint8_t k = 0;
	int rc = 0, i;

	sq_s = nix->sqs[sq];
	if (!sq_s)
		return -ENOENT;

	sq_node = nix_tm_node_search(nix, sq, nix->tm_tree);
	if (!sq_node)
		return -ENOENT;

	parent_lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH2 :
		      ROC_TM_LVL_SCH1);

	parent = sq_node->parent;
	while (parent) {
		if (parent->lvl == parent_lvl)
			break;

		parent = parent->parent;
	}
	if (!parent)
		return -ENOENT;

	list = nix_tm_node_list(nix, tree);

	/* Get relative channel if loopback */
	if (roc_nix_is_lbk(roc_nix))
		rel_chan = nix_tm_lbk_relchan_get(nix);
	else
		rel_chan = tc;

	/* Enable request, parent rel chan already configured */
	if (enable && parent->rel_chan != NIX_TM_CHAN_INVALID &&
	    parent->rel_chan != rel_chan) {
		plt_err("SQ %d: parent node TL3 id %d already has rel_chan %d set",
			sq, parent->hw_id, parent->rel_chan);
		rc = -EINVAL;
		goto err;
	}

	/* No action if enable request for a non participating SQ. This case is
	 * required to handle post flush where TCs should be reconfigured after
	 * pre flush.
	 */
	if (enable && sq_s->tc == ROC_NIX_PFC_CLASS_INVALID &&
	    tc == ROC_NIX_PFC_CLASS_INVALID)
		return 0;

	/* Find the parent TL3 */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != nix->tm_link_cfg_lvl)
			continue;

		if (!(node->flags & NIX_TM_NODE_HWRES) || !node->bp_capa)
			continue;

		/* Restrict sharing of TL3 across the queues */
		if (enable && node != parent && node->rel_chan == rel_chan) {
			plt_warn("SQ %d: siblng node TL3 %d already has %d(%d) tc value set",
				 sq, node->hw_id, tc, rel_chan);
			return -EEXIST;
		}
	}

	/* Allocating TL3 request */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
	req->lvl = nix->tm_link_cfg_lvl;
	k = 0;

	/* Enable PFC/pause on the identified TL3 */
	req->reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(parent->hw_id, link);
	req->regval[k] = enable ? rel_chan : 0;
	req->regval[k] |= enable ? BIT_ULL(13) : 0;
	req->regval_mask[k] = ~(BIT_ULL(13) | GENMASK_ULL(7, 0));
	k++;

	req->num_regs = k;
	rc = mbox_process(mbox);
	if (rc)
		goto err;

	parent->rel_chan = enable ? rel_chan : NIX_TM_CHAN_INVALID;
	sq_s->tc = enable ? tc : ROC_NIX_PFC_CLASS_INVALID;
	/* Clear other SQ's with same TC i.e same parent node */
	for (i = 0; !enable && i < nix->nb_tx_queues; i++) {
		if (nix->sqs[i] && nix->sqs[i]->tc == tc)
			nix->sqs[i]->tc = ROC_NIX_PFC_CLASS_INVALID;
	}

	rc = 0;
	plt_tm_dbg("SQ %u: TL3 %d TC %u %s",
		   sq, parent->hw_id, tc, enable ? "enabled" : "disabled");
	goto exit;
err:
	plt_err("Failed to %s bp on link %u, rc=%d(%s)",
		enable ? "enable" : "disable", link, rc, roc_error_msg_get(rc));
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_tm_bp_config_get(struct roc_nix *roc_nix, bool *is_enabled)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_txschq_config *req = NULL, *rsp;
	enum roc_nix_tm_tree tree = nix->tm_tree;
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	bool found = false;
	uint8_t enable = 1;
	uint8_t k = 0, i;
	uint16_t link;
	int rc = 0;

	list = nix_tm_node_list(nix, tree);
	link = nix->tx_link;

	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != nix->tm_link_cfg_lvl)
			continue;

		if (!(node->flags & NIX_TM_NODE_HWRES) || !node->bp_capa)
			continue;

		found = true;
		if (!req) {
			req = mbox_alloc_msg_nix_txschq_cfg(mbox);
			req->read = 1;
			req->lvl = nix->tm_link_cfg_lvl;
			k = 0;
		}

		req->reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(node->hw_id, link);
		k++;

		if (k >= MAX_REGS_PER_MBOX_MSG) {
			req->num_regs = k;
			rc = mbox_process_msg(mbox, (void **)&rsp);
			if (rc || rsp->num_regs != k)
				goto err;
			req = NULL;

			/* Report it as enabled only if enabled or all */
			for (i = 0; i < k; i++)
				enable &= !!(rsp->regval[i] & BIT_ULL(13));
		}
	}

	if (req) {
		req->num_regs = k;
		rc = mbox_process_msg(mbox, (void **)&rsp);
		if (rc)
			goto err;
		/* Report it as enabled only if enabled or all */
		for (i = 0; i < k; i++)
			enable &= !!(rsp->regval[i] & BIT_ULL(13));
	}

	*is_enabled = found ? !!enable : false;
	rc = 0;
	goto exit;
err:
	plt_err("Failed to get bp status on link %u, rc=%d(%s)", link, rc,
		roc_error_msg_get(rc));
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_tm_smq_xoff(struct nix *nix, struct nix_tm_node *node, bool enable)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req;
	uint16_t smq;
	int rc;

	smq = node->hw_id;
	plt_tm_dbg("Setting SMQ %u XOFF/FLUSH to %s", smq,
		   enable ? "enable" : "disable");

	rc = nix_tm_clear_path_xoff(nix, node);
	if (rc)
		return rc;

	req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
	req->lvl = NIX_TXSCH_LVL_SMQ;
	req->num_regs = 1;

	req->reg[0] = NIX_AF_SMQX_CFG(smq);
	req->regval[0] = enable ? (BIT_ULL(50) | BIT_ULL(49)) : 0;
	req->regval_mask[0] =
		enable ? ~(BIT_ULL(50) | BIT_ULL(49)) : ~BIT_ULL(50);

	rc = mbox_process(mbox);
	mbox_put(mbox);
	return rc;
}

int
nix_tm_leaf_data_get(struct nix *nix, uint16_t sq, uint32_t *rr_quantum,
		     uint16_t *smq)
{
	struct nix_tm_node *node;
	int rc;

	node = nix_tm_node_search(nix, sq, nix->tm_tree);

	/* Check if we found a valid leaf node */
	if (!node || !nix_tm_is_leaf(nix, node->lvl) || !node->parent ||
	    node->parent->hw_id == NIX_TM_HW_ID_INVALID) {
		return -EIO;
	}

	/* Get SMQ Id of leaf node's parent */
	*smq = node->parent->hw_id;
	*rr_quantum = nix_tm_weight_to_rr_quantum(node->weight);

	rc = nix_tm_smq_xoff(nix, node->parent, false);
	if (rc)
		return rc;
	node->flags |= NIX_TM_NODE_ENABLED;
	return 0;
}

int
roc_nix_tm_sq_flush_spin(struct roc_nix_sq *sq)
{
	struct nix *nix = roc_nix_to_nix_priv(sq->roc_nix);
	uint16_t sqb_cnt, head_off, tail_off;
	uint64_t wdata, val, prev;
	uint16_t qid = sq->qid;
	int64_t *regaddr;
	uint64_t timeout; /* 10's of usec */

	/* Wait for enough time based on shaper min rate */
	timeout = (sq->nb_desc * roc_nix_max_pkt_len(sq->roc_nix) * 8 * 1E5);
	/* Wait for worst case scenario of this SQ being last priority
	 * and so have to wait for all other SQ's drain out by their own.
	 */
	timeout = timeout * nix->nb_tx_queues;
	timeout = timeout / nix->tm_rate_min;
	if (!timeout)
		timeout = 10000;

	wdata = ((uint64_t)qid << 32);
	regaddr = (int64_t *)(nix->base + NIX_LF_SQ_OP_STATUS);
	val = roc_atomic64_add_nosync(wdata, regaddr);

	/* Spin multiple iterations as "sq->fc_cache_pkts" can still
	 * have space to send pkts even though fc_mem is disabled
	 */

	while (true) {
		prev = val;
		plt_delay_us(10);
		val = roc_atomic64_add_nosync(wdata, regaddr);
		/* Continue on error */
		if (val & BIT_ULL(63))
			continue;

		if (prev != val)
			continue;

		sqb_cnt = val & 0xFFFF;
		head_off = (val >> 20) & 0x3F;
		tail_off = (val >> 28) & 0x3F;

		/* SQ reached quiescent state */
		if (sqb_cnt <= 1 && head_off == tail_off &&
		    (*(volatile uint64_t *)sq->fc == sq->aura_sqb_bufs)) {
			break;
		}

		/* Timeout */
		if (!timeout)
			goto exit;
		timeout--;
	}

	return 0;
exit:
	return -EFAULT;
}

void
nix_tm_sq_free_sqe_buffer(uint64_t *sqe, int head_off, int end_off, int instr_sz)
{
	int i, j, inc = (8 * (0x2 >> instr_sz)), segs;
	struct nix_send_hdr_s *send_hdr;
	uint64_t *ptr, aura_handle;
	struct idev_cfg *idev;

	if (!sqe)
		return;

	idev = idev_get_cfg();
	if (idev == NULL)
		return;

	ptr = sqe + (head_off * inc);
	for (i = head_off; i < end_off; i++) {
		ptr = sqe + (i * inc);
		send_hdr = (struct nix_send_hdr_s *)(ptr);
		aura_handle = roc_npa_aura_handle_gen(send_hdr->w0.aura, idev->npa->base);
		ptr += 2;
		if (((*ptr >> 60) & 0xF) == NIX_SUBDC_EXT)
			ptr += 2;
		if (((*ptr >> 60) & 0xF) == NIX_SUBDC_AGE_AND_STATS)
			ptr += 2;
		if (((*ptr >> 60) & 0xF) == NIX_SUBDC_JUMP) {
			ptr += 1;
			ptr = (uint64_t *)*ptr;
		}
		if (((*ptr >> 60) & 0xF) == NIX_SUBDC_CRC)
			ptr += 2;
		/* We are not parsing immediate send descriptor */
		if (((*ptr >> 60) & 0xF) == NIX_SUBDC_IMM)
			continue;
		while (1) {
			if (((*ptr >> 60) & 0xF) == NIX_SUBDC_SG) {
				segs = (*ptr >> 48) & 0x3;
				ptr += 1;
				for (j = 0; j < segs; j++) {
					roc_npa_aura_op_free(aura_handle, 0, *ptr);
					ptr += 1;
				}
				if (segs == 2)
					ptr += 1;
			} else if (((*ptr >> 60) & 0xF) == NIX_SUBDC_SG2) {
				uint64_t aura = (*ptr >> 16) & 0xFFFFF;

				aura = roc_npa_aura_handle_gen(aura, idev->npa->base);
				ptr += 1;
				roc_npa_aura_op_free(aura, 0, *ptr);
				ptr += 1;
			} else
				break;
		}
	}
}

int
roc_nix_tm_sq_free_pending_sqe(struct nix *nix, int q)
{
	int head_off, count, rc = 0, tail_off;
	struct roc_nix_sq *sq = nix->sqs[q];
	void *sqb_buf, *dat, *tail_sqb;
	struct dev *dev = &nix->dev;
	struct ndc_sync_op *ndc_req;
	uint16_t sqes_per_sqb;
	struct mbox *mbox;

	mbox = dev->mbox;
	/* Sync NDC-NIX-TX for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox_get(mbox));
	if (ndc_req == NULL) {
		mbox_put(mbox);
		return -EFAULT;
	}

	ndc_req->nix_lf_tx_sync = 1;
	if (mbox_process(mbox))
		rc |= NIX_ERR_NDC_SYNC;
	mbox_put(mbox);

	if (rc)
		plt_err("NDC_SYNC failed rc %d", rc);

	rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_SQ, q, (void *)&dat);

	if (roc_model_is_cn9k()) {
		volatile struct nix_sq_ctx_s *ctx = (struct nix_sq_ctx_s *)dat;

		/* We will cleanup SQE buffers only when we received MNQ interrupt */
		if (!ctx->mnq_dis)
			return -EFAULT;

		count = ctx->sqb_count;
		sqb_buf = (void *)ctx->head_sqb;
		tail_sqb = (void *)ctx->tail_sqb;
		head_off = ctx->head_offset;
		tail_off = ctx->tail_offset;
	} else {
		volatile struct nix_cn10k_sq_ctx_s *ctx = (struct nix_cn10k_sq_ctx_s *)dat;

		/* We will cleanup SQE buffers only when we received MNQ interrupt */
		if (!ctx->mnq_dis)
			return -EFAULT;

		count = ctx->sqb_count;
		/* Free SQB's that are used */
		sqb_buf = (void *)ctx->head_sqb;
		tail_sqb = (void *)ctx->tail_sqb;
		head_off = ctx->head_offset;
		tail_off = ctx->tail_offset;
	}
	sqes_per_sqb = 1 << sq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	while (count) {
		void *next_sqb;

		if (sqb_buf == tail_sqb)
			nix_tm_sq_free_sqe_buffer(sqb_buf, head_off, tail_off, sq->max_sqe_sz);
		else
			nix_tm_sq_free_sqe_buffer(sqb_buf, head_off, (sqes_per_sqb - 1),
						  sq->max_sqe_sz);
		next_sqb = *(void **)((uint64_t *)sqb_buf +
				      (uint32_t)((sqes_per_sqb - 1) * (0x2 >> sq->max_sqe_sz) * 8));
		roc_npa_aura_op_free(sq->aura_handle, 1, (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		head_off = 0;
		count--;
	}

	return 0;
}

static inline int
nix_tm_sdp_sq_drop_pkts(struct roc_nix *roc_nix, struct roc_nix_sq *sq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_txschq_config *req = NULL, *rsp;
	enum roc_nix_tm_tree tree = nix->tm_tree;
	int rc = 0, qid = sq->qid;
	struct nix_tm_node *node;
	uint64_t regval;

	/* Find the node for this SQ */
	node = nix_tm_node_search(nix, qid, tree);
	while (node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_TL4) {
			node = node->parent;
			continue;
		}
		break;
	}
	if (!node) {
		plt_err("Invalid node/state for sq %u", qid);
		return -EFAULT;
	}

	/* Get present link config */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->read = 1;
	req->lvl = NIX_TXSCH_LVL_TL4;
	req->reg[0] = NIX_AF_TL4X_SDP_LINK_CFG(node->hw_id);
	req->num_regs = 1;
	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc || rsp->num_regs != 1)
		goto err;
	regval = rsp->regval[0];
	/* Disable BP_ENA in SDP link config */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_TL4;
	req->reg[0] = NIX_AF_TL4X_SDP_LINK_CFG(node->hw_id);
	req->regval[0] = 0x0ull;
	req->regval_mask[0] = ~(BIT_ULL(13));
	req->num_regs = 1;
	rc = mbox_process(mbox);
	if (rc)
		goto err;
	mbox_put(mbox);
	/* Flush SQ to drop all packets */
	rc = roc_nix_tm_sq_flush_spin(sq);
	if (rc)
		plt_nix_dbg("SQ flush failed with link reset config rc %d", rc);
	mbox = mbox_get((&nix->dev)->mbox);
	/* Restore link config */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->reg[0] = NIX_AF_TL4X_SDP_LINK_CFG(node->hw_id);
	req->lvl = NIX_TXSCH_LVL_TL4;
	req->regval[0] = regval;
	req->regval_mask[0] = ~(BIT_ULL(13) | BIT_ULL(12) | GENMASK_ULL(7, 0));
	req->num_regs = 1;
	rc = mbox_process(mbox);
err:
	mbox_put(mbox);
	return rc;
}

/* Flush and disable tx queue and its parent SMQ */
int
nix_tm_sq_flush_pre(struct roc_nix_sq *sq)
{
	struct roc_nix *roc_nix = sq->roc_nix;
	struct nix_tm_node *node, *sibling;
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
	struct msg_req *req;
	struct mbox *mbox;
	struct nix *nix;
	uint16_t qid;
	int rc;

	nix = roc_nix_to_nix_priv(roc_nix);

	/* Need not do anything if tree is in disabled state */
	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	mbox = (&nix->dev)->mbox;
	qid = sq->qid;

	tree = nix->tm_tree;
	list = nix_tm_node_list(nix, tree);

	/* Find the node for this SQ */
	node = nix_tm_node_search(nix, qid, tree);
	if (!node) {
		plt_err("Invalid node/state for sq %u", qid);
		return -EFAULT;
	}

	/* Enable CGX RXTX to drain pkts */
	if (!roc_nix->io_enabled) {
		/* Though it enables both RX MCAM Entries and CGX Link
		 * we assume all the rx queues are stopped way back.
		 */
		mbox_alloc_msg_nix_lf_start_rx(mbox_get(mbox));
		rc = mbox_process(mbox);
		if (rc) {
			mbox_put(mbox);
			plt_err("cgx start failed, rc=%d", rc);
			return rc;
		}
		mbox_put(mbox);
	}

	/* Disable backpressure */
	rc = nix_tm_bp_config_set(roc_nix, sq->qid, 0, false);
	if (rc) {
		plt_err("Failed to disable backpressure for flush, rc=%d", rc);
		return rc;
	}

	/* Disable smq xoff for case it was enabled earlier */
	rc = nix_tm_smq_xoff(nix, node->parent, false);
	if (rc) {
		plt_err("Failed to enable smq %u, rc=%d", node->parent->hw_id,
			rc);
		return rc;
	}

	/* As per HRM, to disable an SQ, all other SQ's
	 * that feed to same SMQ must be paused before SMQ flush.
	 */
	TAILQ_FOREACH(sibling, list, node) {
		if (sibling->parent != node->parent)
			continue;
		if (!(sibling->flags & NIX_TM_NODE_ENABLED))
			continue;

		qid = sibling->id;
		sq = nix->sqs[qid];
		if (!sq)
			continue;

		rc = roc_nix_tm_sq_aura_fc(sq, false);
		if (rc) {
			plt_err("Failed to disable sqb aura fc, rc=%d", rc);
			goto cleanup;
		}

		/* Wait for sq entries to be flushed */
		rc = roc_nix_tm_sq_flush_spin(sq);
		if (rc) {
			if (nix->sdp_link)
				rc = nix_tm_sdp_sq_drop_pkts(roc_nix, sq);
			else
				rc = roc_nix_tm_sq_free_pending_sqe(nix, sq->qid);
			if (rc) {
				roc_nix_tm_dump(sq->roc_nix, NULL);
				roc_nix_queues_ctx_dump(sq->roc_nix, NULL);
				plt_err("Failed to drain sq %u, rc=%d\n", sq->qid, rc);
				return rc;
			}
			/* Freed all pending SQEs for this SQ, so disable this node */
			sibling->flags &= ~NIX_TM_NODE_ENABLED;
		}
	}

	node->flags &= ~NIX_TM_NODE_ENABLED;

	/* Disable and flush */
	rc = nix_tm_smq_xoff(nix, node->parent, true);
	if (rc) {
		plt_err("Failed to disable smq %u, rc=%d", node->parent->hw_id,
			rc);
		goto cleanup;
	}

	req = mbox_alloc_msg_nix_rx_sw_sync(mbox_get(mbox));
	if (!req) {
		mbox_put(mbox);
		return -ENOSPC;
	}

	rc = mbox_process(mbox);
	mbox_put(mbox);
cleanup:
	/* Restore cgx state */
	if (!roc_nix->io_enabled) {
		mbox_alloc_msg_nix_lf_stop_rx(mbox_get(mbox));
		rc |= mbox_process(mbox);
		mbox_put(mbox);
	}

	return rc;
}

int
nix_tm_sq_flush_post(struct roc_nix_sq *sq)
{
	struct roc_nix *roc_nix = sq->roc_nix;
	struct nix_tm_node *node, *sibling;
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
	struct roc_nix_sq *s_sq;
	bool once = false;
	uint16_t qid, s_qid;
	struct nix *nix;
	int rc;

	nix = roc_nix_to_nix_priv(roc_nix);

	/* Need not do anything if tree is in disabled state */
	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	qid = sq->qid;
	tree = nix->tm_tree;
	list = nix_tm_node_list(nix, tree);

	/* Find the node for this SQ */
	node = nix_tm_node_search(nix, qid, tree);
	if (!node) {
		plt_err("Invalid node for sq %u", qid);
		return -EFAULT;
	}

	/* Enable all the siblings back */
	TAILQ_FOREACH(sibling, list, node) {
		if (sibling->parent != node->parent)
			continue;

		if (sibling->id == qid)
			continue;

		if (!(sibling->flags & NIX_TM_NODE_ENABLED))
			continue;

		s_qid = sibling->id;
		s_sq = nix->sqs[s_qid];
		if (!s_sq)
			continue;

		if (!once) {
			/* Enable back if any SQ is still present */
			rc = nix_tm_smq_xoff(nix, node->parent, false);
			if (rc) {
				plt_err("Failed to enable smq %u, rc=%d",
					node->parent->hw_id, rc);
				return rc;
			}
			once = true;
		}

		rc = roc_nix_tm_sq_aura_fc(s_sq, true);
		if (rc) {
			plt_err("Failed to enable sqb aura fc, rc=%d", rc);
			return rc;
		}
	}

	return 0;
}

int
nix_tm_sq_sched_conf(struct nix *nix, struct nix_tm_node *node,
		     bool rr_quantum_only)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	uint16_t qid = node->id, smq;
	uint64_t rr_quantum;
	int rc;

	smq = node->parent->hw_id;
	rr_quantum = nix_tm_weight_to_rr_quantum(node->weight);

	if (rr_quantum_only)
		plt_tm_dbg("Update sq(%u) rr_quantum 0x%" PRIx64, qid,
			   rr_quantum);
	else
		plt_tm_dbg("Enabling sq(%u)->smq(%u), rr_quantum 0x%" PRIx64,
			   qid, smq, rr_quantum);

	if (qid > nix->nb_tx_queues) {
		rc = -EFAULT;
		goto exit;
	}

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		/* smq update only when needed */
		if (!rr_quantum_only) {
			aq->sq.smq = smq;
			aq->sq_mask.smq = ~aq->sq_mask.smq;
		}
		aq->sq.smq_rr_quantum = rr_quantum;
		aq->sq_mask.smq_rr_quantum = ~aq->sq_mask.smq_rr_quantum;
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		/* smq update only when needed */
		if (!rr_quantum_only) {
			aq->sq.smq = smq;
			aq->sq_mask.smq = ~aq->sq_mask.smq;
		}
		aq->sq.smq_rr_weight = rr_quantum;
		aq->sq_mask.smq_rr_weight = ~aq->sq_mask.smq_rr_weight;
	}

	rc = mbox_process(mbox);
	if (rc)
		plt_err("Failed to set smq, rc=%d", rc);
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_tm_release_resources(struct nix *nix, uint8_t hw_lvl, bool contig,
			 bool above_thresh)
{
	uint16_t avail, thresh, to_free = 0, schq;
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txsch_free_req *req;
	struct plt_bitmap *bmp;
	uint64_t slab = 0;
	uint32_t pos = 0;
	int rc = -ENOSPC;

	bmp = contig ? nix->schq_contig_bmp[hw_lvl] : nix->schq_bmp[hw_lvl];
	thresh =
		contig ? nix->contig_rsvd[hw_lvl] : nix->discontig_rsvd[hw_lvl];
	plt_bitmap_scan_init(bmp);

	avail = nix_tm_resource_avail(nix, hw_lvl, contig);

	if (above_thresh) {
		/* Release only above threshold */
		if (avail > thresh)
			to_free = avail - thresh;
	} else {
		/* Release everything */
		to_free = avail;
	}

	/* Now release resources to AF */
	while (to_free) {
		if (!slab && !plt_bitmap_scan(bmp, &pos, &slab))
			break;

		schq = bitmap_ctzll(slab);
		slab &= ~(1ULL << schq);
		schq += pos;

		/* Free to AF */
		req = mbox_alloc_msg_nix_txsch_free(mbox_get(mbox));
		if (req == NULL) {
			mbox_put(mbox);
			return rc;
		}
		req->flags = 0;
		req->schq_lvl = hw_lvl;
		req->schq = schq;
		rc = mbox_process(mbox);
		if (rc) {
			plt_err("failed to release hwres %s(%u) rc %d",
				nix_tm_hwlvl2str(hw_lvl), schq, rc);
			mbox_put(mbox);
			return rc;
		}
		mbox_put(mbox);
		plt_tm_dbg("Released hwres %s(%u)", nix_tm_hwlvl2str(hw_lvl),
			   schq);
		plt_bitmap_clear(bmp, schq);
		to_free--;
	}

	if (to_free) {
		plt_err("resource inconsistency for %s(%u)",
			nix_tm_hwlvl2str(hw_lvl), contig);
		return -EFAULT;
	}
	return 0;
}

int
nix_tm_free_node_resource(struct nix *nix, struct nix_tm_node *node)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_txsch_free_req *req;
	struct plt_bitmap *bmp;
	uint16_t avail, hw_id;
	uint8_t hw_lvl;
	int rc = -ENOSPC;

	hw_lvl = node->hw_lvl;
	hw_id = node->hw_id;
	bmp = nix->schq_bmp[hw_lvl];
	/* Free specific HW resource */
	plt_tm_dbg("Free hwres %s(%u) lvl %u id %u (%p)",
		   nix_tm_hwlvl2str(node->hw_lvl), hw_id, node->lvl, node->id,
		   node);

	avail = nix_tm_resource_avail(nix, hw_lvl, false);
	/* Always for now free to discontiguous queue when avail
	 * is not sufficient.
	 */
	if (nix->discontig_rsvd[hw_lvl] &&
	    avail < nix->discontig_rsvd[hw_lvl]) {
		PLT_ASSERT(hw_id < NIX_TM_MAX_HW_TXSCHQ);
		PLT_ASSERT(plt_bitmap_get(bmp, hw_id) == 0);
		plt_bitmap_set(bmp, hw_id);
		node->hw_id = NIX_TM_HW_ID_INVALID;
		node->flags &= ~NIX_TM_NODE_HWRES;
		rc = 0;
		goto exit;
	}

	/* Free to AF */
	req = mbox_alloc_msg_nix_txsch_free(mbox);
	if (req == NULL)
		goto exit;
	req->flags = 0;
	req->schq_lvl = node->hw_lvl;
	req->schq = hw_id;
	rc = mbox_process(mbox);
	if (rc) {
		plt_err("failed to release hwres %s(%u) rc %d",
			nix_tm_hwlvl2str(node->hw_lvl), hw_id, rc);
		goto exit;
	}

	/* Mark parent as dirty for reallocing it's children */
	if (node->parent)
		node->parent->child_realloc = true;

	node->hw_id = NIX_TM_HW_ID_INVALID;
	node->flags &= ~NIX_TM_NODE_HWRES;
	plt_tm_dbg("Released hwres %s(%u) to af",
		   nix_tm_hwlvl2str(node->hw_lvl), hw_id);
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
nix_tm_node_delete(struct roc_nix *roc_nix, uint32_t node_id,
		   enum roc_nix_tm_tree tree, bool free)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile;
	struct nix_tm_node *node, *child;
	struct nix_tm_node_list *list;
	uint32_t profile_id;
	int rc;

	plt_tm_dbg("Delete node id %u tree %u", node_id, tree);

	node = nix_tm_node_search(nix, node_id, tree);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	list = nix_tm_node_list(nix, tree);
	/* Check for any existing children */
	TAILQ_FOREACH(child, list, node) {
		if (child->parent == node)
			return NIX_ERR_TM_CHILD_EXISTS;
	}

	/* Remove shaper profile reference */
	profile_id = node->shaper_profile_id;
	profile = nix_tm_shaper_profile_search(nix, profile_id);

	/* Free hw resource locally */
	if (node->flags & NIX_TM_NODE_HWRES) {
		rc = nix_tm_free_node_resource(nix, node);
		if (rc)
			return rc;
	}

	if (profile)
		profile->ref_cnt--;

	TAILQ_REMOVE(list, node, node);

	plt_tm_dbg("Deleted node %s lvl %u id %u, prio 0x%x weight 0x%x "
		   "parent %u profile 0x%x tree %u (%p)",
		   nix_tm_hwlvl2str(node->hw_lvl), node->lvl, node->id,
		   node->priority, node->weight,
		   node->parent ? node->parent->id : UINT32_MAX,
		   node->shaper_profile_id, tree, node);
	/* Free only if requested */
	if (free)
		nix_tm_node_free(node);
	return 0;
}

static int
nix_tm_assign_hw_id(struct nix *nix, struct nix_tm_node *parent,
		    uint16_t *contig_id, int *contig_cnt,
		    struct nix_tm_node_list *list)
{
	struct nix_tm_node *child;
	struct plt_bitmap *bmp;
	uint8_t child_hw_lvl;
	int spare_schq = -1;
	uint32_t pos = 0;
	uint64_t slab;
	uint16_t schq;

	child_hw_lvl = parent->hw_lvl - 1;
	bmp = nix->schq_bmp[child_hw_lvl];
	plt_bitmap_scan_init(bmp);
	slab = 0;

	/* Save spare schq if it is case of RR + SP */
	if (parent->rr_prio != 0xf && *contig_cnt > 1)
		spare_schq = *contig_id + parent->rr_prio;

	TAILQ_FOREACH(child, list, node) {
		if (!child->parent)
			continue;
		if (child->parent->id != parent->id)
			continue;

		/* Resource never expected to be present */
		if (child->flags & NIX_TM_NODE_HWRES) {
			plt_err("Resource exists for child (%s)%u, id %u (%p)",
				nix_tm_hwlvl2str(child->hw_lvl), child->hw_id,
				child->id, child);
			return -EFAULT;
		}

		if (!slab)
			plt_bitmap_scan(bmp, &pos, &slab);

		if (child->priority == parent->rr_prio && spare_schq != -1) {
			/* Use spare schq first if present */
			schq = spare_schq;
			spare_schq = -1;
			*contig_cnt = *contig_cnt - 1;

		} else if (child->priority == parent->rr_prio) {
			/* Assign a discontiguous queue */
			if (!slab) {
				plt_err("Schq not found for Child %u "
					"lvl %u (%p)",
					child->id, child->lvl, child);
				return -ENOENT;
			}

			schq = bitmap_ctzll(slab);
			slab &= ~(1ULL << schq);
			schq += pos;
			plt_bitmap_clear(bmp, schq);
		} else {
			/* Assign a contiguous queue */
			schq = *contig_id + child->priority;
			*contig_cnt = *contig_cnt - 1;
		}

		plt_tm_dbg("Resource %s(%u), for lvl %u id %u(%p)",
			   nix_tm_hwlvl2str(child->hw_lvl), schq, child->lvl,
			   child->id, child);

		child->hw_id = schq;
		child->parent_hw_id = parent->hw_id;
		child->flags |= NIX_TM_NODE_HWRES;
	}

	return 0;
}

int
nix_tm_assign_resources(struct nix *nix, enum roc_nix_tm_tree tree)
{
	struct nix_tm_node *parent, *root = NULL;
	struct plt_bitmap *bmp, *bmp_contig;
	struct nix_tm_node_list *list;
	uint8_t child_hw_lvl, hw_lvl;
	uint16_t contig_id, j;
	uint64_t slab = 0;
	uint32_t pos = 0;
	int cnt, rc;

	list = nix_tm_node_list(nix, tree);
	/* Walk from TL1 to TL4 parents */
	for (hw_lvl = NIX_TXSCH_LVL_TL1; hw_lvl > 0; hw_lvl--) {
		TAILQ_FOREACH(parent, list, node) {
			child_hw_lvl = parent->hw_lvl - 1;
			if (parent->hw_lvl != hw_lvl)
				continue;

			/* Remember root for future */
			if (parent->hw_lvl == nix->tm_root_lvl)
				root = parent;

			if (!parent->child_realloc) {
				/* Skip when parent is not dirty */
				if (nix_tm_child_res_valid(list, parent))
					continue;
				plt_err("Parent not dirty but invalid "
					"child res parent id %u(lvl %u)",
					parent->id, parent->lvl);
				return -EFAULT;
			}

			bmp_contig = nix->schq_contig_bmp[child_hw_lvl];

			/* Prealloc contiguous indices for a parent */
			contig_id = NIX_TM_MAX_HW_TXSCHQ;
			cnt = (int)parent->max_prio + 1;
			if (cnt > 0) {
				plt_bitmap_scan_init(bmp_contig);
				if (!plt_bitmap_scan(bmp_contig, &pos, &slab)) {
					plt_err("Contig schq not found");
					return -ENOENT;
				}
				contig_id = pos + bitmap_ctzll(slab);

				/* Check if we have enough */
				for (j = contig_id; j < contig_id + cnt; j++) {
					if (!plt_bitmap_get(bmp_contig, j))
						break;
				}

				if (j != contig_id + cnt) {
					plt_err("Contig schq not sufficient");
					return -ENOENT;
				}

				for (j = contig_id; j < contig_id + cnt; j++)
					plt_bitmap_clear(bmp_contig, j);
			}

			/* Assign hw id to all children */
			rc = nix_tm_assign_hw_id(nix, parent, &contig_id, &cnt,
						 list);
			if (cnt || rc) {
				plt_err("Unexpected err, contig res alloc, "
					"parent %u, of %s, rc=%d, cnt=%d",
					parent->id, nix_tm_hwlvl2str(hw_lvl),
					rc, cnt);
				return -EFAULT;
			}

			/* Clear the dirty bit as children's
			 * resources are reallocated.
			 */
			parent->child_realloc = false;
		}
	}

	/* Root is always expected to be there */
	if (!root)
		return -EFAULT;

	if (root->flags & NIX_TM_NODE_HWRES)
		return 0;

	/* Process root node */
	bmp = nix->schq_bmp[nix->tm_root_lvl];
	plt_bitmap_scan_init(bmp);
	if (!plt_bitmap_scan(bmp, &pos, &slab)) {
		plt_err("Resource not allocated for root");
		return -EIO;
	}

	root->hw_id = pos + bitmap_ctzll(slab);
	root->flags |= NIX_TM_NODE_HWRES;
	plt_bitmap_clear(bmp, root->hw_id);

	/* Get TL1 id as well when root is not TL1 */
	if (!nix_tm_have_tl1_access(nix)) {
		bmp = nix->schq_bmp[NIX_TXSCH_LVL_TL1];

		plt_bitmap_scan_init(bmp);
		if (!plt_bitmap_scan(bmp, &pos, &slab)) {
			plt_err("Resource not found for TL1");
			return -EIO;
		}
		root->parent_hw_id = pos + bitmap_ctzll(slab);
		plt_bitmap_clear(bmp, root->parent_hw_id);
	}

	plt_tm_dbg("Resource %s(%u) for root(id %u) (%p)",
		   nix_tm_hwlvl2str(root->hw_lvl), root->hw_id, root->id, root);

	return 0;
}

void
nix_tm_copy_rsp_to_nix(struct nix *nix, struct nix_txsch_alloc_rsp *rsp)
{
	uint8_t lvl;
	uint16_t i;

	for (lvl = 0; lvl < NIX_TXSCH_LVL_CNT; lvl++) {
		for (i = 0; i < rsp->schq[lvl]; i++)
			plt_bitmap_set(nix->schq_bmp[lvl],
				       rsp->schq_list[lvl][i]);

		for (i = 0; i < rsp->schq_contig[lvl]; i++)
			plt_bitmap_set(nix->schq_contig_bmp[lvl],
				       rsp->schq_contig_list[lvl][i]);
	}
}

int
nix_tm_alloc_txschq(struct nix *nix, enum roc_nix_tm_tree tree)
{
	uint16_t schq_contig[NIX_TXSCH_LVL_CNT];
	struct mbox *mbox = (&nix->dev)->mbox;
	uint16_t schq[NIX_TXSCH_LVL_CNT];
	struct nix_txsch_alloc_req *req;
	struct nix_txsch_alloc_rsp *rsp;
	uint8_t hw_lvl, i;
	bool pend;
	int rc;

	memset(schq, 0, sizeof(schq));
	memset(schq_contig, 0, sizeof(schq_contig));

	/* Estimate requirement */
	rc = nix_tm_resource_estimate(nix, schq_contig, schq, tree);
	if (!rc)
		return 0;

	/* Release existing contiguous resources when realloc requested
	 * as there is no way to guarantee continuity of old with new.
	 */
	for (hw_lvl = 0; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		if (schq_contig[hw_lvl])
			nix_tm_release_resources(nix, hw_lvl, true, false);
	}

	/* Alloc as needed */
	do {
		pend = false;
		req = mbox_alloc_msg_nix_txsch_alloc(mbox_get(mbox));
		if (!req) {
			mbox_put(mbox);
			rc = -ENOMEM;
			goto alloc_err;
		}
		mbox_memcpy(req->schq, schq, sizeof(req->schq));
		mbox_memcpy(req->schq_contig, schq_contig,
			    sizeof(req->schq_contig));

		/* Each alloc can be at max of MAX_TXSCHQ_PER_FUNC per level.
		 * So split alloc to multiple requests.
		 */
		for (i = 0; i < NIX_TXSCH_LVL_CNT; i++) {
			if (req->schq[i] > MAX_TXSCHQ_PER_FUNC)
				req->schq[i] = MAX_TXSCHQ_PER_FUNC;
			schq[i] -= req->schq[i];

			if (req->schq_contig[i] > MAX_TXSCHQ_PER_FUNC)
				req->schq_contig[i] = MAX_TXSCHQ_PER_FUNC;
			schq_contig[i] -= req->schq_contig[i];

			if (schq[i] || schq_contig[i])
				pend = true;
		}

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			mbox_put(mbox);
			goto alloc_err;
		}

		nix_tm_copy_rsp_to_nix(nix, rsp);
		mbox_put(mbox);
	} while (pend);

	nix->tm_link_cfg_lvl = rsp->link_cfg_lvl;
	nix->tm_aggr_lvl_rr_prio = rsp->aggr_lvl_rr_prio;
	return 0;
alloc_err:
	for (i = 0; i < NIX_TXSCH_LVL_CNT; i++) {
		if (nix_tm_release_resources(nix, i, true, false))
			plt_err("Failed to release contig resources of "
				"lvl %d on error",
				i);
		if (nix_tm_release_resources(nix, i, false, false))
			plt_err("Failed to release discontig resources of "
				"lvl %d on error",
				i);
	}
	return rc;
}

int
nix_tm_prepare_default_tree(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint32_t nonleaf_id = nix->nb_tx_queues;
	struct nix_tm_node *node = NULL;
	uint8_t leaf_lvl, lvl, lvl_end;
	uint32_t parent, i;
	int rc = 0;

	/* Add ROOT, SCH1, SCH2, SCH3, [SCH4]  nodes */
	parent = ROC_NIX_TM_NODE_ID_INVALID;
	/* With TL1 access we have an extra level */
	lvl_end = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH4 :
						       ROC_TM_LVL_SCH3);

	for (lvl = ROC_TM_LVL_ROOT; lvl <= lvl_end; lvl++) {
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = nonleaf_id;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = lvl;
		node->tree = ROC_NIX_TM_DEFAULT;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
		parent = nonleaf_id;
		nonleaf_id++;
	}

	parent = nonleaf_id - 1;
	leaf_lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_QUEUE :
							ROC_TM_LVL_SCH4);

	/* Add leaf nodes */
	for (i = 0; i < nix->nb_tx_queues; i++) {
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = i;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = leaf_lvl;
		node->tree = ROC_NIX_TM_DEFAULT;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
	}

	return 0;
error:
	nix_tm_node_free(node);
	return rc;
}

int
roc_nix_tm_prepare_rate_limited_tree(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint32_t nonleaf_id = nix->nb_tx_queues;
	struct nix_tm_node *node = NULL;
	uint8_t leaf_lvl, lvl, lvl_end;
	uint32_t parent, i;
	int rc = 0;

	/* Add ROOT, SCH1, SCH2 nodes */
	parent = ROC_NIX_TM_NODE_ID_INVALID;
	lvl_end = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH3 :
						       ROC_TM_LVL_SCH2);

	for (lvl = ROC_TM_LVL_ROOT; lvl <= lvl_end; lvl++) {
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = nonleaf_id;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = lvl;
		node->tree = ROC_NIX_TM_RLIMIT;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
		parent = nonleaf_id;
		nonleaf_id++;
	}

	/* SMQ is mapped to SCH4 when we have TL1 access and SCH3 otherwise */
	lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH4 : ROC_TM_LVL_SCH3);

	/* Add per queue SMQ nodes i.e SCH4 / SCH3 */
	for (i = 0; i < nix->nb_tx_queues; i++) {
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = nonleaf_id + i;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = lvl;
		node->tree = ROC_NIX_TM_RLIMIT;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
	}

	parent = nonleaf_id;
	leaf_lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_QUEUE :
							ROC_TM_LVL_SCH4);

	/* Add leaf nodes */
	for (i = 0; i < nix->nb_tx_queues; i++) {
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = i;
		node->parent_id = parent + i;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = leaf_lvl;
		node->tree = ROC_NIX_TM_RLIMIT;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
	}

	return 0;
error:
	nix_tm_node_free(node);
	return rc;
}

int
roc_nix_tm_pfc_prepare_tree(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint8_t leaf_lvl, lvl, lvl_start, lvl_end;
	uint32_t nonleaf_id = nix->nb_tx_queues;
	struct nix_tm_node *node = NULL;
	uint32_t tl2_node_id;
	uint32_t parent, i;
	int rc = -ENOMEM;

	parent = ROC_NIX_TM_NODE_ID_INVALID;
	lvl_end = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH3 :
		   ROC_TM_LVL_SCH2);
	leaf_lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_QUEUE :
		    ROC_TM_LVL_SCH4);

	/* TL1 node */
	node = nix_tm_node_alloc();
	if (!node)
		goto error;

	node->id = nonleaf_id;
	node->parent_id = parent;
	node->priority = 0;
	node->weight = NIX_TM_DFLT_RR_WT;
	node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
	node->lvl = ROC_TM_LVL_ROOT;
	node->tree = ROC_NIX_TM_PFC;
	node->rel_chan = NIX_TM_CHAN_INVALID;

	rc = nix_tm_node_add(roc_nix, node);
	if (rc)
		goto error;

	parent = nonleaf_id;
	nonleaf_id++;

	lvl_start = ROC_TM_LVL_SCH1;
	if (roc_nix_is_pf(roc_nix)) {
		/* TL2 node */
		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = nonleaf_id;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = ROC_TM_LVL_SCH1;
		node->tree = ROC_NIX_TM_PFC;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;

		lvl_start = ROC_TM_LVL_SCH2;
		tl2_node_id = nonleaf_id;
		nonleaf_id++;
	} else {
		tl2_node_id = parent;
	}

	for (i = 0; i < nix->nb_tx_queues; i++) {
		parent = tl2_node_id;
		for (lvl = lvl_start; lvl <= lvl_end; lvl++) {
			rc = -ENOMEM;
			node = nix_tm_node_alloc();
			if (!node)
				goto error;

			node->id = nonleaf_id;
			node->parent_id = parent;
			node->priority = 0;
			node->weight = NIX_TM_DFLT_RR_WT;
			node->shaper_profile_id =
				ROC_NIX_TM_SHAPER_PROFILE_NONE;
			node->lvl = lvl;
			node->tree = ROC_NIX_TM_PFC;
			node->rel_chan = NIX_TM_CHAN_INVALID;

			rc = nix_tm_node_add(roc_nix, node);
			if (rc)
				goto error;

			parent = nonleaf_id;
			nonleaf_id++;
		}

		lvl = (nix_tm_have_tl1_access(nix) ? ROC_TM_LVL_SCH4 :
		       ROC_TM_LVL_SCH3);

		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = nonleaf_id;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = lvl;
		node->tree = ROC_NIX_TM_PFC;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;

		parent = nonleaf_id;
		nonleaf_id++;

		rc = -ENOMEM;
		node = nix_tm_node_alloc();
		if (!node)
			goto error;

		node->id = i;
		node->parent_id = parent;
		node->priority = 0;
		node->weight = NIX_TM_DFLT_RR_WT;
		node->shaper_profile_id = ROC_NIX_TM_SHAPER_PROFILE_NONE;
		node->lvl = leaf_lvl;
		node->tree = ROC_NIX_TM_PFC;
		node->rel_chan = NIX_TM_CHAN_INVALID;

		rc = nix_tm_node_add(roc_nix, node);
		if (rc)
			goto error;
	}

	return 0;
error:
	nix_tm_node_free(node);
	return rc;
}

int
nix_tm_free_resources(struct roc_nix *roc_nix, uint32_t tree_mask, bool hw_only)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile;
	struct nix_tm_node *node, *next_node;
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
	uint32_t profile_id;
	int rc = 0;
	int hw_lvl;

	for (tree = 0; tree < ROC_NIX_TM_TREE_MAX; tree++) {
		if (!(tree_mask & BIT(tree)))
			continue;

		plt_tm_dbg("Freeing resources of tree %u", tree);

		list = nix_tm_node_list(nix, tree);
		/* Flush and free resources from leaf */
		for (hw_lvl = NIX_TXSCH_LVL_SMQ; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
			next_node = TAILQ_FIRST(list);
			while (next_node) {
				node = next_node;
				next_node = TAILQ_NEXT(node, node);
				if (node->hw_lvl != hw_lvl)
					continue;

				if (!nix_tm_is_leaf(nix, node->lvl) &&
				    node->flags & NIX_TM_NODE_HWRES) {
					/* Clear xoff in path for flush to succeed */
					rc = nix_tm_clear_path_xoff(nix, node);
					if (rc)
						return rc;
					rc = nix_tm_free_node_resource(nix, node);
					if (rc)
						return rc;
				}
			}
		}

		/* Leave software elements if needed */
		if (hw_only)
			continue;

		next_node = TAILQ_FIRST(list);
		while (next_node) {
			node = next_node;
			next_node = TAILQ_NEXT(node, node);

			plt_tm_dbg("Free node lvl %u id %u (%p)", node->lvl,
				   node->id, node);

			profile_id = node->shaper_profile_id;
			profile = nix_tm_shaper_profile_search(nix, profile_id);
			if (profile)
				profile->ref_cnt--;

			TAILQ_REMOVE(list, node, node);
			nix_tm_node_free(node);
		}
	}
	return rc;
}

int
nix_tm_conf_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint32_t bmp_sz, hw_lvl;
	void *bmp_mem;
	int rc, i;

	PLT_STATIC_ASSERT(sizeof(struct nix_tm_node) <= ROC_NIX_TM_NODE_SZ);
	PLT_STATIC_ASSERT(sizeof(struct nix_tm_shaper_profile) <=
			  ROC_NIX_TM_SHAPER_PROFILE_SZ);

	nix->tm_flags = 0;
	for (i = 0; i < ROC_NIX_TM_TREE_MAX; i++)
		TAILQ_INIT(&nix->trees[i]);

	TAILQ_INIT(&nix->shaper_profile_list);
	nix->tm_rate_min = 1E9; /* 1Gbps */

	rc = -ENOMEM;
	bmp_sz = plt_bitmap_get_memory_footprint(NIX_TM_MAX_HW_TXSCHQ);
	bmp_mem = plt_zmalloc(bmp_sz * NIX_TXSCH_LVL_CNT * 2, 0);
	if (!bmp_mem)
		return rc;
	nix->schq_bmp_mem = bmp_mem;

	/* Init contiguous and discontiguous bitmap per lvl */
	rc = -EIO;
	for (hw_lvl = 0; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		/* Bitmap for discontiguous resource */
		nix->schq_bmp[hw_lvl] =
			plt_bitmap_init(NIX_TM_MAX_HW_TXSCHQ, bmp_mem, bmp_sz);
		if (!nix->schq_bmp[hw_lvl])
			goto exit;

		bmp_mem = PLT_PTR_ADD(bmp_mem, bmp_sz);

		/* Bitmap for contiguous resource */
		nix->schq_contig_bmp[hw_lvl] =
			plt_bitmap_init(NIX_TM_MAX_HW_TXSCHQ, bmp_mem, bmp_sz);
		if (!nix->schq_contig_bmp[hw_lvl])
			goto exit;

		bmp_mem = PLT_PTR_ADD(bmp_mem, bmp_sz);
	}

	rc = nix_tm_mark_init(nix);
	if (rc)
		goto exit;

	/* Disable TL1 Static Priority when VF's are enabled
	 * as otherwise VF's TL2 reallocation will be needed
	 * runtime to support a specific topology of PF.
	 */
	if (nix->pci_dev->max_vfs)
		nix->tm_flags |= NIX_TM_TL1_NO_SP;

	/* TL1 access is only for PF's */
	if (roc_nix_is_pf(roc_nix)) {
		nix->tm_flags |= NIX_TM_TL1_ACCESS;
		nix->tm_root_lvl = NIX_TXSCH_LVL_TL1;
	} else {
		nix->tm_root_lvl = NIX_TXSCH_LVL_TL2;
	}

	return 0;
exit:
	nix_tm_conf_fini(roc_nix);
	return rc;
}

void
nix_tm_conf_fini(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint16_t hw_lvl;

	for (hw_lvl = 0; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		plt_bitmap_free(nix->schq_bmp[hw_lvl]);
		plt_bitmap_free(nix->schq_contig_bmp[hw_lvl]);
	}
	plt_free(nix->schq_bmp_mem);
}
