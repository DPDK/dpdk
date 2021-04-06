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
