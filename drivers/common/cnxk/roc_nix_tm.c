/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"


int
nix_tm_clear_path_xoff(struct nix *nix, struct nix_tm_node *node)
{
	struct mbox *mbox = (&nix->dev)->mbox;
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
				return rc;

			p->flags |= NIX_TM_NODE_ENABLED;
		}
		p = p->parent;
	}

	return 0;
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

	req = mbox_alloc_msg_nix_txschq_cfg(mbox);
	req->lvl = NIX_TXSCH_LVL_SMQ;
	req->num_regs = 1;

	req->reg[0] = NIX_AF_SMQX_CFG(smq);
	req->regval[0] = enable ? (BIT_ULL(50) | BIT_ULL(49)) : 0;
	req->regval_mask[0] =
		enable ? ~(BIT_ULL(50) | BIT_ULL(49)) : ~BIT_ULL(50);

	return mbox_process(mbox);
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
		    (*(volatile uint64_t *)sq->fc == sq->nb_sqb_bufs)) {
			break;
		}

		/* Timeout */
		if (!timeout)
			goto exit;
		timeout--;
	}

	return 0;
exit:
	roc_nix_queues_ctx_dump(sq->roc_nix);
	return -EFAULT;
}

/* Flush and disable tx queue and its parent SMQ */
int
nix_tm_sq_flush_pre(struct roc_nix_sq *sq)
{
	struct roc_nix *roc_nix = sq->roc_nix;
	struct nix_tm_node *node, *sibling;
	struct nix_tm_node_list *list;
	enum roc_nix_tm_tree tree;
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
	if (!node || !(node->flags & NIX_TM_NODE_ENABLED)) {
		plt_err("Invalid node/state for sq %u", qid);
		return -EFAULT;
	}

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
			plt_err("Failed to drain sq %u, rc=%d\n", sq->qid, rc);
			return rc;
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
cleanup:
	/* Restore cgx state */
	if (!roc_nix->io_enabled) {
		mbox_alloc_msg_nix_lf_stop_rx(mbox);
		rc |= mbox_process(mbox);
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
nix_tm_conf_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint32_t bmp_sz, hw_lvl;
	void *bmp_mem;
	int rc, i;

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
