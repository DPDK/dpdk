/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_nix_tm_sq_aura_fc(struct roc_nix_sq *sq, bool enable)
{
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	uint64_t aura_handle;
	struct npa_lf *lf;
	struct mbox *mbox;
	int rc = -ENOSPC;

	plt_tm_dbg("Setting SQ %u SQB aura FC to %s", sq->qid,
		   enable ? "enable" : "disable");

	lf = idev_npa_obj_get();
	if (!lf)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	mbox = lf->mbox;
	/* Set/clear sqb aura fc_ena */
	aura_handle = sq->aura_handle;
	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return rc;

	req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_WRITE;
	/* Below is not needed for aura writes but AF driver needs it */
	/* AF will translate to associated poolctx */
	req->aura.pool_addr = req->aura_id;

	req->aura.fc_ena = enable;
	req->aura_mask.fc_ena = 1;
	if (roc_model_is_cn9k() || roc_model_is_cn10ka_a0()) {
		req->aura.fc_stype = 0x0;      /* STF */
		req->aura_mask.fc_stype = 0x0; /* STF */
	} else {
		req->aura.fc_stype = 0x3;      /* STSTP */
		req->aura_mask.fc_stype = 0x3; /* STSTP */
	}

	rc = mbox_process(mbox);
	if (rc)
		return rc;

	/* Read back npa aura ctx */
	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Init when enabled as there might be no triggers */
	if (enable)
		*(volatile uint64_t *)sq->fc = rsp->aura.count;
	else
		*(volatile uint64_t *)sq->fc = sq->nb_sqb_bufs;
	/* Sync write barrier */
	plt_wmb();
	return 0;
}

int
roc_nix_tm_free_resources(struct roc_nix *roc_nix, bool hw_only)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (nix->tm_flags & NIX_TM_HIERARCHY_ENA)
		return -EBUSY;

	return nix_tm_free_resources(roc_nix, BIT(ROC_NIX_TM_USER), hw_only);
}

static int
nix_tm_adjust_shaper_pps_rate(struct nix_tm_shaper_profile *profile)
{
	uint64_t min_rate = profile->commit.rate;

	if (!profile->pkt_mode)
		return 0;

	profile->pkt_mode_adj = 1;

	if (profile->commit.rate &&
	    (profile->commit.rate < NIX_TM_MIN_SHAPER_PPS_RATE ||
	     profile->commit.rate > NIX_TM_MAX_SHAPER_PPS_RATE))
		return NIX_ERR_TM_INVALID_COMMIT_RATE;

	if (profile->peak.rate &&
	    (profile->peak.rate < NIX_TM_MIN_SHAPER_PPS_RATE ||
	     profile->peak.rate > NIX_TM_MAX_SHAPER_PPS_RATE))
		return NIX_ERR_TM_INVALID_PEAK_RATE;

	if (profile->peak.rate && min_rate > profile->peak.rate)
		min_rate = profile->peak.rate;

	/* Each packet accumulate single count, whereas HW
	 * considers each unit as Byte, so we need convert
	 * user pps to bps
	 */
	profile->commit.rate = profile->commit.rate * 8;
	profile->peak.rate = profile->peak.rate * 8;
	min_rate = min_rate * 8;

	if (min_rate && (min_rate < NIX_TM_MIN_SHAPER_RATE)) {
		int adjust = NIX_TM_MIN_SHAPER_RATE / min_rate;

		if (adjust > NIX_TM_LENGTH_ADJUST_MAX)
			return NIX_ERR_TM_SHAPER_PKT_LEN_ADJUST;

		profile->pkt_mode_adj += adjust;
		profile->commit.rate += (adjust * profile->commit.rate);
		profile->peak.rate += (adjust * profile->peak.rate);
		/* Number of tokens freed after scheduling was proportional
		 * to adjust value
		 */
		profile->commit.size *= adjust;
		profile->peak.size *= adjust;
	}

	return 0;
}

static int
nix_tm_shaper_profile_add(struct roc_nix *roc_nix,
			  struct nix_tm_shaper_profile *profile, int skip_ins)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint64_t commit_rate, commit_sz;
	uint64_t min_burst, max_burst;
	uint64_t peak_rate, peak_sz;
	uint32_t id;
	int rc;

	id = profile->id;
	rc = nix_tm_adjust_shaper_pps_rate(profile);
	if (rc)
		return rc;

	commit_rate = profile->commit.rate;
	commit_sz = profile->commit.size;
	peak_rate = profile->peak.rate;
	peak_sz = profile->peak.size;

	min_burst = NIX_TM_MIN_SHAPER_BURST;
	max_burst = roc_nix_tm_max_shaper_burst_get();

	if (nix_tm_shaper_profile_search(nix, id) && !skip_ins)
		return NIX_ERR_TM_SHAPER_PROFILE_EXISTS;

	if (profile->pkt_len_adj < NIX_TM_LENGTH_ADJUST_MIN ||
	    profile->pkt_len_adj > NIX_TM_LENGTH_ADJUST_MAX)
		return NIX_ERR_TM_SHAPER_PKT_LEN_ADJUST;

	/* We cannot support both pkt length adjust and pkt mode */
	if (profile->pkt_mode && profile->pkt_len_adj)
		return NIX_ERR_TM_SHAPER_PKT_LEN_ADJUST;

	/* commit rate and burst size can be enabled/disabled */
	if (commit_rate || commit_sz) {
		if (commit_sz < min_burst || commit_sz > max_burst)
			return NIX_ERR_TM_INVALID_COMMIT_SZ;
		else if (!nix_tm_shaper_rate_conv(commit_rate, NULL, NULL,
						  NULL))
			return NIX_ERR_TM_INVALID_COMMIT_RATE;
	}

	/* Peak rate and burst size can be enabled/disabled */
	if (peak_sz || peak_rate) {
		if (peak_sz < min_burst || peak_sz > max_burst)
			return NIX_ERR_TM_INVALID_PEAK_SZ;
		else if (!nix_tm_shaper_rate_conv(peak_rate, NULL, NULL, NULL))
			return NIX_ERR_TM_INVALID_PEAK_RATE;
	}

	/* If PIR and CIR are requested, PIR should always be larger than CIR */
	if (peak_rate && commit_rate && (commit_rate > peak_rate))
		return NIX_ERR_TM_INVALID_PEAK_RATE;

	if (!skip_ins)
		TAILQ_INSERT_TAIL(&nix->shaper_profile_list, profile, shaper);

	plt_tm_dbg("Added TM shaper profile %u, "
		   " pir %" PRIu64 " , pbs %" PRIu64 ", cir %" PRIu64
		   ", cbs %" PRIu64 " , adj %u, pkt_mode %u",
		   id, profile->peak.rate, profile->peak.size,
		   profile->commit.rate, profile->commit.size,
		   profile->pkt_len_adj, profile->pkt_mode);

	/* Always use PIR for single rate shaping */
	if (!peak_rate && commit_rate) {
		profile->peak.rate = profile->commit.rate;
		profile->peak.size = profile->commit.size;
		profile->commit.rate = 0;
		profile->commit.size = 0;
	}

	/* update min rate */
	nix->tm_rate_min = nix_tm_shaper_profile_rate_min(nix);
	return 0;
}

int
roc_nix_tm_shaper_profile_add(struct roc_nix *roc_nix,
			      struct roc_nix_tm_shaper_profile *roc_profile)
{
	struct nix_tm_shaper_profile *profile;

	profile = (struct nix_tm_shaper_profile *)roc_profile->reserved;

	profile->ref_cnt = 0;
	profile->id = roc_profile->id;
	profile->commit.rate = roc_profile->commit_rate;
	profile->peak.rate = roc_profile->peak_rate;
	profile->commit.size = roc_profile->commit_sz;
	profile->peak.size = roc_profile->peak_sz;
	profile->pkt_len_adj = roc_profile->pkt_len_adj;
	profile->pkt_mode = roc_profile->pkt_mode;
	profile->free_fn = roc_profile->free_fn;

	return nix_tm_shaper_profile_add(roc_nix, profile, 0);
}

int
roc_nix_tm_shaper_profile_update(struct roc_nix *roc_nix,
				 struct roc_nix_tm_shaper_profile *roc_profile)
{
	struct nix_tm_shaper_profile *profile;

	profile = (struct nix_tm_shaper_profile *)roc_profile->reserved;

	profile->commit.rate = roc_profile->commit_rate;
	profile->peak.rate = roc_profile->peak_rate;
	profile->commit.size = roc_profile->commit_sz;
	profile->peak.size = roc_profile->peak_sz;

	return nix_tm_shaper_profile_add(roc_nix, profile, 1);
}

int
roc_nix_tm_shaper_profile_delete(struct roc_nix *roc_nix, uint32_t id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile;

	profile = nix_tm_shaper_profile_search(nix, id);
	if (!profile)
		return NIX_ERR_TM_INVALID_SHAPER_PROFILE;

	if (profile->ref_cnt)
		return NIX_ERR_TM_SHAPER_PROFILE_IN_USE;

	plt_tm_dbg("Removing TM shaper profile %u", id);
	TAILQ_REMOVE(&nix->shaper_profile_list, profile, shaper);
	nix_tm_shaper_profile_free(profile);

	/* update min rate */
	nix->tm_rate_min = nix_tm_shaper_profile_rate_min(nix);
	return 0;
}

int
roc_nix_tm_node_add(struct roc_nix *roc_nix, struct roc_nix_tm_node *roc_node)
{
	struct nix_tm_node *node;

	node = (struct nix_tm_node *)&roc_node->reserved;
	node->id = roc_node->id;
	node->priority = roc_node->priority;
	node->weight = roc_node->weight;
	node->lvl = roc_node->lvl;
	node->parent_id = roc_node->parent_id;
	node->shaper_profile_id = roc_node->shaper_profile_id;
	node->pkt_mode = roc_node->pkt_mode;
	node->pkt_mode_set = roc_node->pkt_mode_set;
	node->free_fn = roc_node->free_fn;
	node->tree = ROC_NIX_TM_USER;

	return nix_tm_node_add(roc_nix, node);
}

int
roc_nix_tm_node_pkt_mode_update(struct roc_nix *roc_nix, uint32_t node_id,
				bool pkt_mode)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node *node, *child;
	struct nix_tm_node_list *list;
	int num_children = 0;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	if (node->pkt_mode == pkt_mode) {
		node->pkt_mode_set = true;
		return 0;
	}

	/* Check for any existing children, if there are any,
	 * then we cannot update the pkt mode as children's quantum
	 * are already taken in.
	 */
	list = nix_tm_node_list(nix, ROC_NIX_TM_USER);
	TAILQ_FOREACH(child, list, node) {
		if (child->parent == node)
			num_children++;
	}

	/* Cannot update mode if it has children or tree is enabled */
	if ((nix->tm_flags & NIX_TM_HIERARCHY_ENA) && num_children)
		return -EBUSY;

	if (node->pkt_mode_set && num_children)
		return NIX_ERR_TM_PKT_MODE_MISMATCH;

	node->pkt_mode = pkt_mode;
	node->pkt_mode_set = true;

	return 0;
}

int
roc_nix_tm_node_name_get(struct roc_nix *roc_nix, uint32_t node_id, char *buf,
			 size_t buflen)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node *node;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node) {
		plt_strlcpy(buf, "???", buflen);
		return NIX_ERR_TM_INVALID_NODE;
	}

	if (node->hw_lvl == NIX_TXSCH_LVL_CNT)
		snprintf(buf, buflen, "SQ_%d", node->id);
	else
		snprintf(buf, buflen, "%s_%d", nix_tm_hwlvl2str(node->hw_lvl),
			 node->hw_id);
	return 0;
}

int
roc_nix_tm_node_delete(struct roc_nix *roc_nix, uint32_t node_id, bool free)
{
	return nix_tm_node_delete(roc_nix, node_id, ROC_NIX_TM_USER, free);
}

int
roc_nix_smq_flush(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
	struct nix_tm_node *node;
	int rc = 0;

	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	tree = nix->tm_tree;
	list = nix_tm_node_list(nix, tree);

	/* XOFF & Flush all SMQ's. HRM mandates
	 * all SQ's empty before SMQ flush is issued.
	 */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_tm_smq_xoff(nix, node, true);
		if (rc) {
			plt_err("Failed to enable smq %u, rc=%d", node->hw_id,
				rc);
			goto exit;
		}
	}

	/* XON all SMQ's */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_tm_smq_xoff(nix, node, false);
		if (rc) {
			plt_err("Failed to enable smq %u, rc=%d", node->hw_id,
				rc);
			goto exit;
		}
	}
exit:
	return rc;
}

int
roc_nix_tm_hierarchy_disable(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint16_t sqb_cnt, head_off, tail_off;
	uint16_t sq_cnt = nix->nb_tx_queues;
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
	struct nix_tm_node *node;
	struct roc_nix_sq *sq;
	uint64_t wdata, val;
	uintptr_t regaddr;
	int rc = -1, i;

	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	plt_tm_dbg("Disabling hierarchy on %s", nix->pci_dev->name);

	tree = nix->tm_tree;
	list = nix_tm_node_list(nix, tree);

	/* Enable CGX RXTX to drain pkts */
	if (!roc_nix->io_enabled) {
		/* Though it enables both RX MCAM Entries and CGX Link
		 * we assume all the rx queues are stopped way back.
		 */
		mbox_alloc_msg_nix_lf_start_rx(mbox);
		rc = mbox_process(mbox);
		if (rc) {
			plt_err("cgx start failed, rc=%d", rc);
			return rc;
		}
	}

	/* XON all SMQ's */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_tm_smq_xoff(nix, node, false);
		if (rc) {
			plt_err("Failed to enable smq %u, rc=%d", node->hw_id,
				rc);
			goto cleanup;
		}
	}

	/* Disable backpressure, it will be enabled back if needed on
	 * hierarchy enable
	 */
	rc = nix_tm_bp_config_set(roc_nix, false);
	if (rc) {
		plt_err("Failed to disable backpressure for flush, rc=%d", rc);
		goto cleanup;
	}

	/* Flush all tx queues */
	for (i = 0; i < sq_cnt; i++) {
		sq = nix->sqs[i];
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
			plt_err("Failed to drain sq, rc=%d\n", rc);
			goto cleanup;
		}
	}

	/* XOFF & Flush all SMQ's. HRM mandates
	 * all SQ's empty before SMQ flush is issued.
	 */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;
		if (!(node->flags & NIX_TM_NODE_HWRES))
			continue;

		rc = nix_tm_smq_xoff(nix, node, true);
		if (rc) {
			plt_err("Failed to enable smq %u, rc=%d", node->hw_id,
				rc);
			goto cleanup;
		}

		node->flags &= ~NIX_TM_NODE_ENABLED;
	}

	/* Verify sanity of all tx queues */
	for (i = 0; i < sq_cnt; i++) {
		sq = nix->sqs[i];
		if (!sq)
			continue;

		wdata = ((uint64_t)sq->qid << 32);
		regaddr = nix->base + NIX_LF_SQ_OP_STATUS;
		val = roc_atomic64_add_nosync(wdata, (int64_t *)regaddr);

		sqb_cnt = val & 0xFFFF;
		head_off = (val >> 20) & 0x3F;
		tail_off = (val >> 28) & 0x3F;

		if (sqb_cnt > 1 || head_off != tail_off ||
		    (*(uint64_t *)sq->fc != sq->nb_sqb_bufs))
			plt_err("Failed to gracefully flush sq %u", sq->qid);
	}

	nix->tm_flags &= ~NIX_TM_HIERARCHY_ENA;
cleanup:
	/* Restore cgx state */
	if (!roc_nix->io_enabled) {
		mbox_alloc_msg_nix_lf_stop_rx(mbox);
		rc |= mbox_process(mbox);
	}
	return rc;
}

int
roc_nix_tm_hierarchy_enable(struct roc_nix *roc_nix, enum roc_nix_tm_tree tree,
			    bool xmit_enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_node_list *list;
	struct nix_tm_node *node;
	struct roc_nix_sq *sq;
	uint32_t tree_mask;
	uint16_t sq_id;
	int rc;

	if (tree >= ROC_NIX_TM_TREE_MAX)
		return NIX_ERR_PARAM;

	if (nix->tm_flags & NIX_TM_HIERARCHY_ENA) {
		if (nix->tm_tree != tree)
			return -EBUSY;
		return 0;
	}

	plt_tm_dbg("Enabling hierarchy on %s, xmit_ena %u, tree %u",
		   nix->pci_dev->name, xmit_enable, tree);

	/* Free hw resources of other trees */
	tree_mask = NIX_TM_TREE_MASK_ALL;
	tree_mask &= ~BIT(tree);

	rc = nix_tm_free_resources(roc_nix, tree_mask, true);
	if (rc) {
		plt_err("failed to free resources of other trees, rc=%d", rc);
		return rc;
	}

	/* Update active tree before starting to do anything */
	nix->tm_tree = tree;

	nix_tm_update_parent_info(nix, tree);

	rc = nix_tm_alloc_txschq(nix, tree);
	if (rc) {
		plt_err("TM failed to alloc tm resources=%d", rc);
		return rc;
	}

	rc = nix_tm_assign_resources(nix, tree);
	if (rc) {
		plt_err("TM failed to assign tm resources=%d", rc);
		return rc;
	}

	rc = nix_tm_txsch_reg_config(nix, tree);
	if (rc) {
		plt_err("TM failed to configure sched registers=%d", rc);
		return rc;
	}

	list = nix_tm_node_list(nix, tree);
	/* Mark all non-leaf's as enabled */
	TAILQ_FOREACH(node, list, node) {
		if (!nix_tm_is_leaf(nix, node->lvl))
			node->flags |= NIX_TM_NODE_ENABLED;
	}

	if (!xmit_enable)
		goto skip_sq_update;

	/* Update SQ Sched Data while SQ is idle */
	TAILQ_FOREACH(node, list, node) {
		if (!nix_tm_is_leaf(nix, node->lvl))
			continue;

		rc = nix_tm_sq_sched_conf(nix, node, false);
		if (rc) {
			plt_err("SQ %u sched update failed, rc=%d", node->id,
				rc);
			return rc;
		}
	}

	/* Finally XON all SMQ's */
	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != NIX_TXSCH_LVL_SMQ)
			continue;

		rc = nix_tm_smq_xoff(nix, node, false);
		if (rc) {
			plt_err("Failed to enable smq %u, rc=%d", node->hw_id,
				rc);
			return rc;
		}
	}

	/* Enable xmit as all the topology is ready */
	TAILQ_FOREACH(node, list, node) {
		if (!nix_tm_is_leaf(nix, node->lvl))
			continue;

		sq_id = node->id;
		sq = nix->sqs[sq_id];

		rc = roc_nix_tm_sq_aura_fc(sq, true);
		if (rc) {
			plt_err("TM sw xon failed on SQ %u, rc=%d", node->id,
				rc);
			return rc;
		}
		node->flags |= NIX_TM_NODE_ENABLED;
	}

skip_sq_update:
	nix->tm_flags |= NIX_TM_HIERARCHY_ENA;
	return 0;
}

int
roc_nix_tm_node_suspend_resume(struct roc_nix *roc_nix, uint32_t node_id,
			       bool suspend)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req;
	struct nix_tm_node *node;
	uint16_t flags;
	int rc;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	flags = node->flags;
	flags = suspend ? (flags & ~NIX_TM_NODE_ENABLED) :
				(flags | NIX_TM_NODE_ENABLED);

	if (node->flags == flags)
		return 0;

	/* send mbox for state change */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);

	req->lvl = node->hw_lvl;
	req->num_regs =
		nix_tm_sw_xoff_prep(node, suspend, req->reg, req->regval);
	rc = mbox_process(mbox);
	if (!rc)
		node->flags = flags;
	return rc;
}

int
roc_nix_tm_prealloc_res(struct roc_nix *roc_nix, uint8_t lvl,
			uint16_t discontig, uint16_t contig)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txsch_alloc_req *req;
	struct nix_txsch_alloc_rsp *rsp;
	uint8_t hw_lvl;
	int rc = -ENOSPC;

	hw_lvl = nix_tm_lvl2nix(nix, lvl);
	if (hw_lvl == NIX_TXSCH_LVL_CNT)
		return -EINVAL;

	/* Preallocate contiguous */
	if (nix->contig_rsvd[hw_lvl] < contig) {
		req = mbox_alloc_msg_nix_txsch_alloc(mbox);
		if (req == NULL)
			return rc;
		req->schq_contig[hw_lvl] = contig - nix->contig_rsvd[hw_lvl];

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			return rc;

		nix_tm_copy_rsp_to_nix(nix, rsp);
	}

	/* Preallocate contiguous */
	if (nix->discontig_rsvd[hw_lvl] < discontig) {
		req = mbox_alloc_msg_nix_txsch_alloc(mbox);
		if (req == NULL)
			return -ENOSPC;
		req->schq[hw_lvl] = discontig - nix->discontig_rsvd[hw_lvl];

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			return rc;

		nix_tm_copy_rsp_to_nix(nix, rsp);
	}

	/* Save thresholds */
	nix->contig_rsvd[hw_lvl] = contig;
	nix->discontig_rsvd[hw_lvl] = discontig;
	/* Release anything present above thresholds */
	nix_tm_release_resources(nix, hw_lvl, true, true);
	nix_tm_release_resources(nix, hw_lvl, false, true);
	return 0;
}

int
roc_nix_tm_node_shaper_update(struct roc_nix *roc_nix, uint32_t node_id,
			      uint32_t profile_id, bool force_update)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile *profile = NULL;
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req;
	struct nix_tm_node *node;
	uint8_t k;
	int rc;

	/* Shaper updates valid only for user nodes */
	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node || nix_tm_is_leaf(nix, node->lvl))
		return NIX_ERR_TM_INVALID_NODE;

	if (profile_id != ROC_NIX_TM_SHAPER_PROFILE_NONE) {
		profile = nix_tm_shaper_profile_search(nix, profile_id);
		if (!profile)
			return NIX_ERR_TM_INVALID_SHAPER_PROFILE;
	}

	/* Pkt mode should match existing node's pkt mode */
	if (profile && profile->pkt_mode != node->pkt_mode)
		return NIX_ERR_TM_PKT_MODE_MISMATCH;

	if ((profile_id == node->shaper_profile_id) && !force_update) {
		return 0;
	} else if (profile_id != node->shaper_profile_id) {
		struct nix_tm_shaper_profile *old;

		/* Find old shaper profile and reduce ref count */
		old = nix_tm_shaper_profile_search(nix,
						   node->shaper_profile_id);
		if (old)
			old->ref_cnt--;

		if (profile)
			profile->ref_cnt++;

		/* Reduce older shaper ref count and increase new one */
		node->shaper_profile_id = profile_id;
	}

	/* Nothing to do if hierarchy not yet enabled */
	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	node->flags &= ~NIX_TM_NODE_ENABLED;

	/* Flush the specific node with SW_XOFF */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = node->hw_lvl;
	k = nix_tm_sw_xoff_prep(node, true, req->reg, req->regval);
	req->num_regs = k;

	rc = mbox_process(mbox);
	if (rc)
		return rc;

	/* Update the PIR/CIR and clear SW XOFF */
	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = node->hw_lvl;

	k = nix_tm_shaper_reg_prep(node, profile, req->reg, req->regval);

	k += nix_tm_sw_xoff_prep(node, false, &req->reg[k], &req->regval[k]);

	req->num_regs = k;
	rc = mbox_process(mbox);
	if (!rc)
		node->flags |= NIX_TM_NODE_ENABLED;
	return rc;
}

int
roc_nix_tm_node_parent_update(struct roc_nix *roc_nix, uint32_t node_id,
			      uint32_t new_parent_id, uint32_t priority,
			      uint32_t weight)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_tm_node *node, *sibling;
	struct nix_tm_node *new_parent;
	struct nix_txschq_config *req;
	struct nix_tm_node_list *list;
	uint8_t k;
	int rc;

	node = nix_tm_node_search(nix, node_id, ROC_NIX_TM_USER);
	if (!node)
		return NIX_ERR_TM_INVALID_NODE;

	/* Parent id valid only for non root nodes */
	if (node->hw_lvl != nix->tm_root_lvl) {
		new_parent =
			nix_tm_node_search(nix, new_parent_id, ROC_NIX_TM_USER);
		if (!new_parent)
			return NIX_ERR_TM_INVALID_PARENT;

		/* Current support is only for dynamic weight update */
		if (node->parent != new_parent || node->priority != priority)
			return NIX_ERR_TM_PARENT_PRIO_UPDATE;
	}

	list = nix_tm_node_list(nix, ROC_NIX_TM_USER);
	/* Skip if no change */
	if (node->weight == weight)
		return 0;

	node->weight = weight;

	/* Nothing to do if hierarchy not yet enabled */
	if (!(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return 0;

	/* For leaf nodes, SQ CTX needs update */
	if (nix_tm_is_leaf(nix, node->lvl)) {
		/* Update SQ quantum data on the fly */
		rc = nix_tm_sq_sched_conf(nix, node, true);
		if (rc)
			return NIX_ERR_TM_SQ_UPDATE_FAIL;
	} else {
		/* XOFF Parent node */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = node->parent->hw_lvl;
		req->num_regs = nix_tm_sw_xoff_prep(node->parent, true,
						    req->reg, req->regval);
		rc = mbox_process(mbox);
		if (rc)
			return rc;

		/* XOFF this node and all other siblings */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = node->hw_lvl;

		k = 0;
		TAILQ_FOREACH(sibling, list, node) {
			if (sibling->parent != node->parent)
				continue;
			k += nix_tm_sw_xoff_prep(sibling, true, &req->reg[k],
						 &req->regval[k]);
		}
		req->num_regs = k;
		rc = mbox_process(mbox);
		if (rc)
			return rc;

		/* Update new weight for current node */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = node->hw_lvl;
		req->num_regs =
			nix_tm_sched_reg_prep(nix, node, req->reg, req->regval);
		rc = mbox_process(mbox);
		if (rc)
			return rc;

		/* XON this node and all other siblings */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = node->hw_lvl;

		k = 0;
		TAILQ_FOREACH(sibling, list, node) {
			if (sibling->parent != node->parent)
				continue;
			k += nix_tm_sw_xoff_prep(sibling, false, &req->reg[k],
						 &req->regval[k]);
		}
		req->num_regs = k;
		rc = mbox_process(mbox);
		if (rc)
			return rc;

		/* XON Parent node */
		req = mbox_alloc_msg_nix_txschq_cfg(mbox);
		req->lvl = node->parent->hw_lvl;
		req->num_regs = nix_tm_sw_xoff_prep(node->parent, false,
						    req->reg, req->regval);
		rc = mbox_process(mbox);
		if (rc)
			return rc;
	}
	return 0;
}

int
roc_nix_tm_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint32_t tree_mask;
	int rc;

	if (nix->tm_flags & NIX_TM_HIERARCHY_ENA) {
		plt_err("Cannot init while existing hierarchy is enabled");
		return -EBUSY;
	}

	/* Free up all user resources already held */
	tree_mask = NIX_TM_TREE_MASK_ALL;
	rc = nix_tm_free_resources(roc_nix, tree_mask, false);
	if (rc) {
		plt_err("Failed to freeup all nodes and resources, rc=%d", rc);
		return rc;
	}

	/* Prepare default tree */
	rc = nix_tm_prepare_default_tree(roc_nix);
	if (rc) {
		plt_err("failed to prepare default tm tree, rc=%d", rc);
		return rc;
	}

	return rc;
}

int
roc_nix_tm_rlimit_sq(struct roc_nix *roc_nix, uint16_t qid, uint64_t rate)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tm_shaper_profile profile;
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_tm_node *node, *parent;

	volatile uint64_t *reg, *regval;
	struct nix_txschq_config *req;
	uint16_t flags;
	uint8_t k = 0;
	int rc;

	if ((nix->tm_tree == ROC_NIX_TM_USER) ||
	    !(nix->tm_flags & NIX_TM_HIERARCHY_ENA))
		return NIX_ERR_TM_INVALID_TREE;

	node = nix_tm_node_search(nix, qid, nix->tm_tree);

	/* check if we found a valid leaf node */
	if (!node || !nix_tm_is_leaf(nix, node->lvl) || !node->parent ||
	    node->parent->hw_id == NIX_TM_HW_ID_INVALID)
		return NIX_ERR_TM_INVALID_NODE;

	parent = node->parent;
	flags = parent->flags;

	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_MDQ;
	reg = req->reg;
	regval = req->regval;

	if (rate == 0) {
		k += nix_tm_sw_xoff_prep(parent, true, &reg[k], &regval[k]);
		flags &= ~NIX_TM_NODE_ENABLED;
		goto exit;
	}

	if (!(flags & NIX_TM_NODE_ENABLED)) {
		k += nix_tm_sw_xoff_prep(parent, false, &reg[k], &regval[k]);
		flags |= NIX_TM_NODE_ENABLED;
	}

	/* Use only PIR for rate limit */
	memset(&profile, 0, sizeof(profile));
	profile.peak.rate = rate;
	/* Minimum burst of ~4us Bytes of Tx */
	profile.peak.size = PLT_MAX((uint64_t)roc_nix_max_pkt_len(roc_nix),
				    (4ul * rate) / ((uint64_t)1E6 * 8));
	if (!nix->tm_rate_min || nix->tm_rate_min > rate)
		nix->tm_rate_min = rate;

	k += nix_tm_shaper_reg_prep(parent, &profile, &reg[k], &regval[k]);
exit:
	req->num_regs = k;
	rc = mbox_process(mbox);
	if (rc)
		return rc;

	parent->flags = flags;
	return 0;
}

void
roc_nix_tm_fini(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txsch_free_req *req;
	uint32_t tree_mask;
	uint8_t hw_lvl;
	int rc;

	/* Xmit is assumed to be disabled */
	/* Free up resources already held */
	tree_mask = NIX_TM_TREE_MASK_ALL;
	rc = nix_tm_free_resources(roc_nix, tree_mask, false);
	if (rc)
		plt_err("Failed to freeup existing nodes or rsrcs, rc=%d", rc);

	/* Free all other hw resources */
	req = mbox_alloc_msg_nix_txsch_free(mbox);
	if (req == NULL)
		return;

	req->flags = TXSCHQ_FREE_ALL;
	rc = mbox_process(mbox);
	if (rc)
		plt_err("Failed to freeup all res, rc=%d", rc);

	for (hw_lvl = 0; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		plt_bitmap_reset(nix->schq_bmp[hw_lvl]);
		plt_bitmap_reset(nix->schq_contig_bmp[hw_lvl]);
		nix->contig_rsvd[hw_lvl] = 0;
		nix->discontig_rsvd[hw_lvl] = 0;
	}

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(nix);
	nix->tm_tree = 0;
	nix->tm_flags &= ~NIX_TM_HIERARCHY_ENA;
}

int
roc_nix_tm_rsrc_count(struct roc_nix *roc_nix, uint16_t schq[ROC_TM_LVL_MAX])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;
	struct free_rsrcs_rsp *rsp;
	uint8_t hw_lvl;
	int rc, i;

	/* Get the current free resources */
	mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	for (i = 0; i < ROC_TM_LVL_MAX; i++) {
		hw_lvl = nix_tm_lvl2nix(nix, i);
		if (hw_lvl == NIX_TXSCH_LVL_CNT)
			continue;

		schq[i] = (nix->is_nix1 ? rsp->schq_nix1[hw_lvl] :
						rsp->schq[hw_lvl]);
	}

	return 0;
}

void
roc_nix_tm_rsrc_max(bool pf, uint16_t schq[ROC_TM_LVL_MAX])
{
	uint8_t hw_lvl, i;
	uint16_t max;

	for (i = 0; i < ROC_TM_LVL_MAX; i++) {
		hw_lvl = pf ? nix_tm_lvl2nix_tl1_root(i) :
				    nix_tm_lvl2nix_tl2_root(i);

		switch (hw_lvl) {
		case NIX_TXSCH_LVL_SMQ:
			max = (roc_model_is_cn9k() ?
					     NIX_CN9K_TXSCH_LVL_SMQ_MAX :
					     NIX_TXSCH_LVL_SMQ_MAX);
			break;
		case NIX_TXSCH_LVL_TL4:
			max = NIX_TXSCH_LVL_TL4_MAX;
			break;
		case NIX_TXSCH_LVL_TL3:
			max = NIX_TXSCH_LVL_TL3_MAX;
			break;
		case NIX_TXSCH_LVL_TL2:
			max = pf ? NIX_TXSCH_LVL_TL2_MAX : 1;
			break;
		case NIX_TXSCH_LVL_TL1:
			max = pf ? 1 : 0;
			break;
		default:
			max = 0;
			break;
		}
		schq[i] = max;
	}
}

bool
roc_nix_tm_root_has_sp(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (nix->tm_flags & NIX_TM_TL1_NO_SP)
		return false;
	return true;
}
