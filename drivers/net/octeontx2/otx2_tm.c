/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_malloc.h>

#include "otx2_ethdev.h"
#include "otx2_tm.h"

/* Use last LVL_CNT nodes as default nodes */
#define NIX_DEFAULT_NODE_ID_START (RTE_TM_NODE_ID_NULL - NIX_TXSCH_LVL_CNT)

enum otx2_tm_node_level {
	OTX2_TM_LVL_ROOT = 0,
	OTX2_TM_LVL_SCH1,
	OTX2_TM_LVL_SCH2,
	OTX2_TM_LVL_SCH3,
	OTX2_TM_LVL_SCH4,
	OTX2_TM_LVL_QUEUE,
	OTX2_TM_LVL_MAX,
};

static bool
nix_tm_have_tl1_access(struct otx2_eth_dev *dev)
{
	bool is_lbk = otx2_dev_is_lbk(dev);
	return otx2_dev_is_pf(dev) && !otx2_dev_is_A0(dev) &&
		!is_lbk && !dev->maxvf;
}

static struct otx2_nix_tm_shaper_profile *
nix_tm_shaper_profile_search(struct otx2_eth_dev *dev, uint32_t shaper_id)
{
	struct otx2_nix_tm_shaper_profile *tm_shaper_profile;

	TAILQ_FOREACH(tm_shaper_profile, &dev->shaper_profile_list, shaper) {
		if (tm_shaper_profile->shaper_profile_id == shaper_id)
			return tm_shaper_profile;
	}
	return NULL;
}

static struct otx2_nix_tm_node *
nix_tm_node_search(struct otx2_eth_dev *dev,
		   uint32_t node_id, bool user)
{
	struct otx2_nix_tm_node *tm_node;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (tm_node->id == node_id &&
		    (user == !!(tm_node->flags & NIX_TM_NODE_USER)))
			return tm_node;
	}
	return NULL;
}

static uint32_t
check_rr(struct otx2_eth_dev *dev, uint32_t priority, uint32_t parent_id)
{
	struct otx2_nix_tm_node *tm_node;
	uint32_t rr_num = 0;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!tm_node->parent)
			continue;

		if (!(tm_node->parent->id == parent_id))
			continue;

		if (tm_node->priority == priority)
			rr_num++;
	}
	return rr_num;
}

static int
nix_tm_update_parent_info(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *tm_node_child;
	struct otx2_nix_tm_node *tm_node;
	struct otx2_nix_tm_node *parent;
	uint32_t rr_num = 0;
	uint32_t priority;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (!tm_node->parent)
			continue;
		/* Count group of children of same priority i.e are RR */
		parent = tm_node->parent;
		priority = tm_node->priority;
		rr_num = check_rr(dev, priority, parent->id);

		/* Assuming that multiple RR groups are
		 * not configured based on capability.
		 */
		if (rr_num > 1) {
			parent->rr_prio = priority;
			parent->rr_num = rr_num;
		}

		/* Find out static priority children that are not in RR */
		TAILQ_FOREACH(tm_node_child, &dev->node_list, node) {
			if (!tm_node_child->parent)
				continue;
			if (parent->id != tm_node_child->parent->id)
				continue;
			if (parent->max_prio == UINT32_MAX &&
			    tm_node_child->priority != parent->rr_prio)
				parent->max_prio = 0;

			if (parent->max_prio < tm_node_child->priority &&
			    parent->rr_prio != tm_node_child->priority)
				parent->max_prio = tm_node_child->priority;
		}
	}

	return 0;
}

static int
nix_tm_node_add_to_list(struct otx2_eth_dev *dev, uint32_t node_id,
			uint32_t parent_node_id, uint32_t priority,
			uint32_t weight, uint16_t hw_lvl_id,
			uint16_t level_id, bool user,
			struct rte_tm_node_params *params)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;
	struct otx2_nix_tm_node *tm_node, *parent_node;
	uint32_t shaper_profile_id;

	shaper_profile_id = params->shaper_profile_id;
	shaper_profile = nix_tm_shaper_profile_search(dev, shaper_profile_id);

	parent_node = nix_tm_node_search(dev, parent_node_id, user);

	tm_node = rte_zmalloc("otx2_nix_tm_node",
			      sizeof(struct otx2_nix_tm_node), 0);
	if (!tm_node)
		return -ENOMEM;

	tm_node->level_id = level_id;
	tm_node->hw_lvl_id = hw_lvl_id;

	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->rr_prio = 0xf;
	tm_node->max_prio = UINT32_MAX;
	tm_node->hw_id = UINT32_MAX;
	tm_node->flags = 0;
	if (user)
		tm_node->flags = NIX_TM_NODE_USER;
	rte_memcpy(&tm_node->params, params, sizeof(struct rte_tm_node_params));

	if (shaper_profile)
		shaper_profile->reference_count++;
	tm_node->parent = parent_node;
	tm_node->parent_hw_id = UINT32_MAX;

	TAILQ_INSERT_TAIL(&dev->node_list, tm_node, node);

	return 0;
}

static int
nix_tm_clear_shaper_profiles(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;

	while ((shaper_profile = TAILQ_FIRST(&dev->shaper_profile_list))) {
		if (shaper_profile->reference_count)
			otx2_tm_dbg("Shaper profile %u has non zero references",
				    shaper_profile->shaper_profile_id);
		TAILQ_REMOVE(&dev->shaper_profile_list, shaper_profile, shaper);
		rte_free(shaper_profile);
	}

	return 0;
}

static int
nix_tm_free_resources(struct otx2_eth_dev *dev, uint32_t flags_mask,
		      uint32_t flags, bool hw_only)
{
	struct otx2_nix_tm_shaper_profile *shaper_profile;
	struct otx2_nix_tm_node *tm_node, *next_node;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txsch_free_req *req;
	uint32_t shaper_profile_id;
	bool skip_node = false;
	int rc = 0;

	next_node = TAILQ_FIRST(&dev->node_list);
	while (next_node) {
		tm_node = next_node;
		next_node = TAILQ_NEXT(tm_node, node);

		/* Check for only requested nodes */
		if ((tm_node->flags & flags_mask) != flags)
			continue;

		if (nix_tm_have_tl1_access(dev) &&
		    tm_node->hw_lvl_id ==  NIX_TXSCH_LVL_TL1)
			skip_node = true;

		otx2_tm_dbg("Free hwres for node %u, hwlvl %u, hw_id %u (%p)",
			    tm_node->id,  tm_node->hw_lvl_id,
			    tm_node->hw_id, tm_node);
		/* Free specific HW resource if requested */
		if (!skip_node && flags_mask &&
		    tm_node->flags & NIX_TM_NODE_HWRES) {
			req = otx2_mbox_alloc_msg_nix_txsch_free(mbox);
			req->flags = 0;
			req->schq_lvl = tm_node->hw_lvl_id;
			req->schq = tm_node->hw_id;
			rc = otx2_mbox_process(mbox);
			if (rc)
				break;
		} else {
			skip_node = false;
		}
		tm_node->flags &= ~NIX_TM_NODE_HWRES;

		/* Leave software elements if needed */
		if (hw_only)
			continue;

		shaper_profile_id = tm_node->params.shaper_profile_id;
		shaper_profile =
			nix_tm_shaper_profile_search(dev, shaper_profile_id);
		if (shaper_profile)
			shaper_profile->reference_count--;

		TAILQ_REMOVE(&dev->node_list, tm_node, node);
		rte_free(tm_node);
	}

	if (!flags_mask) {
		/* Free all hw resources */
		req = otx2_mbox_alloc_msg_nix_txsch_free(mbox);
		req->flags = TXSCHQ_FREE_ALL;

		return otx2_mbox_process(mbox);
	}

	return rc;
}

static uint8_t
nix_tm_copy_rsp_to_dev(struct otx2_eth_dev *dev,
		       struct nix_txsch_alloc_rsp *rsp)
{
	uint16_t schq;
	uint8_t lvl;

	for (lvl = 0; lvl < NIX_TXSCH_LVL_CNT; lvl++) {
		for (schq = 0; schq < MAX_TXSCHQ_PER_FUNC; schq++) {
			dev->txschq_list[lvl][schq] = rsp->schq_list[lvl][schq];
			dev->txschq_contig_list[lvl][schq] =
				rsp->schq_contig_list[lvl][schq];
		}

		dev->txschq[lvl] = rsp->schq[lvl];
		dev->txschq_contig[lvl] = rsp->schq_contig[lvl];
	}
	return 0;
}

static int
nix_tm_assign_id_to_node(struct otx2_eth_dev *dev,
			 struct otx2_nix_tm_node *child,
			 struct otx2_nix_tm_node *parent)
{
	uint32_t hw_id, schq_con_index, prio_offset;
	uint32_t l_id, schq_index;

	otx2_tm_dbg("Assign hw id for child node %u, lvl %u, hw_lvl %u (%p)",
		    child->id, child->level_id, child->hw_lvl_id, child);

	child->flags |= NIX_TM_NODE_HWRES;

	/* Process root nodes */
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL2 &&
	    child->hw_lvl_id == dev->otx2_tm_root_lvl && !parent) {
		int idx = 0;
		uint32_t tschq_con_index;

		l_id = child->hw_lvl_id;
		tschq_con_index = dev->txschq_contig_index[l_id];
		hw_id = dev->txschq_contig_list[l_id][tschq_con_index];
		child->hw_id = hw_id;
		dev->txschq_contig_index[l_id]++;
		/* Update TL1 hw_id for its parent for config purpose */
		idx = dev->txschq_index[NIX_TXSCH_LVL_TL1]++;
		hw_id = dev->txschq_list[NIX_TXSCH_LVL_TL1][idx];
		child->parent_hw_id = hw_id;
		return 0;
	}
	if (dev->otx2_tm_root_lvl == NIX_TXSCH_LVL_TL1 &&
	    child->hw_lvl_id == dev->otx2_tm_root_lvl && !parent) {
		uint32_t tschq_con_index;

		l_id = child->hw_lvl_id;
		tschq_con_index = dev->txschq_index[l_id];
		hw_id = dev->txschq_list[l_id][tschq_con_index];
		child->hw_id = hw_id;
		dev->txschq_index[l_id]++;
		return 0;
	}

	/* Process children with parents */
	l_id = child->hw_lvl_id;
	schq_index = dev->txschq_index[l_id];
	schq_con_index = dev->txschq_contig_index[l_id];

	if (child->priority == parent->rr_prio) {
		hw_id = dev->txschq_list[l_id][schq_index];
		child->hw_id = hw_id;
		child->parent_hw_id = parent->hw_id;
		dev->txschq_index[l_id]++;
	} else {
		prio_offset = schq_con_index + child->priority;
		hw_id = dev->txschq_contig_list[l_id][prio_offset];
		child->hw_id = hw_id;
	}
	return 0;
}

static int
nix_tm_assign_hw_id(struct otx2_eth_dev *dev)
{
	struct otx2_nix_tm_node *parent, *child;
	uint32_t child_hw_lvl, con_index_inc, i;

	for (i = NIX_TXSCH_LVL_TL1; i > 0; i--) {
		TAILQ_FOREACH(parent, &dev->node_list, node) {
			child_hw_lvl = parent->hw_lvl_id - 1;
			if (parent->hw_lvl_id != i)
				continue;
			TAILQ_FOREACH(child, &dev->node_list, node) {
				if (!child->parent)
					continue;
				if (child->parent->id != parent->id)
					continue;
				nix_tm_assign_id_to_node(dev, child, parent);
			}

			con_index_inc = parent->max_prio + 1;
			dev->txschq_contig_index[child_hw_lvl] += con_index_inc;

			/*
			 * Explicitly assign id to parent node if it
			 * doesn't have a parent
			 */
			if (parent->hw_lvl_id == dev->otx2_tm_root_lvl)
				nix_tm_assign_id_to_node(dev, parent, NULL);
		}
	}
	return 0;
}

static uint8_t
nix_tm_count_req_schq(struct otx2_eth_dev *dev,
		      struct nix_txsch_alloc_req *req, uint8_t lvl)
{
	struct otx2_nix_tm_node *tm_node;
	uint8_t contig_count;

	TAILQ_FOREACH(tm_node, &dev->node_list, node) {
		if (lvl == tm_node->hw_lvl_id) {
			req->schq[lvl - 1] += tm_node->rr_num;
			if (tm_node->max_prio != UINT32_MAX) {
				contig_count = tm_node->max_prio + 1;
				req->schq_contig[lvl - 1] += contig_count;
			}
		}
		if (lvl == dev->otx2_tm_root_lvl &&
		    dev->otx2_tm_root_lvl && lvl == NIX_TXSCH_LVL_TL2 &&
		    tm_node->hw_lvl_id == dev->otx2_tm_root_lvl) {
			req->schq_contig[dev->otx2_tm_root_lvl]++;
		}
	}

	req->schq[NIX_TXSCH_LVL_TL1] = 1;
	req->schq_contig[NIX_TXSCH_LVL_TL1] = 0;

	return 0;
}

static int
nix_tm_prepare_txschq_req(struct otx2_eth_dev *dev,
			  struct nix_txsch_alloc_req *req)
{
	uint8_t i;

	for (i = NIX_TXSCH_LVL_TL1; i > 0; i--)
		nix_tm_count_req_schq(dev, req, i);

	for (i = 0; i < NIX_TXSCH_LVL_CNT; i++) {
		dev->txschq_index[i] = 0;
		dev->txschq_contig_index[i] = 0;
	}
	return 0;
}

static int
nix_tm_send_txsch_alloc_msg(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_txsch_alloc_req *req;
	struct nix_txsch_alloc_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_nix_txsch_alloc(mbox);

	rc = nix_tm_prepare_txschq_req(dev, req);
	if (rc)
		return rc;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix_tm_copy_rsp_to_dev(dev, rsp);

	nix_tm_assign_hw_id(dev);
	return 0;
}

static int
nix_tm_alloc_resources(struct rte_eth_dev *eth_dev, bool xmit_enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc;

	RTE_SET_USED(xmit_enable);

	nix_tm_update_parent_info(dev);

	rc = nix_tm_send_txsch_alloc_msg(dev);
	if (rc) {
		otx2_err("TM failed to alloc tm resources=%d", rc);
		return rc;
	}

	return 0;
}

static int
nix_tm_prepare_default_tree(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint32_t def = eth_dev->data->nb_tx_queues;
	struct rte_tm_node_params params;
	uint32_t leaf_parent, i;
	int rc = 0;

	/* Default params */
	memset(&params, 0, sizeof(params));
	params.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;

	if (nix_tm_have_tl1_access(dev)) {
		dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL1;
		rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL1,
					     OTX2_TM_LVL_ROOT, false, &params);
		if (rc)
			goto exit;
		rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL2,
					     OTX2_TM_LVL_SCH1, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL3,
					     OTX2_TM_LVL_SCH2, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 3, def + 2, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL4,
					     OTX2_TM_LVL_SCH3, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 4, def + 3, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_SMQ,
					     OTX2_TM_LVL_SCH4, false, &params);
		if (rc)
			goto exit;

		leaf_parent = def + 4;
	} else {
		dev->otx2_tm_root_lvl = NIX_TXSCH_LVL_TL2;
		rc = nix_tm_node_add_to_list(dev, def, RTE_TM_NODE_ID_NULL, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL2,
					     OTX2_TM_LVL_ROOT, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 1, def, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL3,
					     OTX2_TM_LVL_SCH1, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 2, def + 1, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_TL4,
					     OTX2_TM_LVL_SCH2, false, &params);
		if (rc)
			goto exit;

		rc = nix_tm_node_add_to_list(dev, def + 3, def + 2, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_SMQ,
					     OTX2_TM_LVL_SCH3, false, &params);
		if (rc)
			goto exit;

		leaf_parent = def + 3;
	}

	/* Add leaf nodes */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = nix_tm_node_add_to_list(dev, i, leaf_parent, 0,
					     DEFAULT_RR_WEIGHT,
					     NIX_TXSCH_LVL_CNT,
					     OTX2_TM_LVL_QUEUE, false, &params);
		if (rc)
			break;
	}

exit:
	return rc;
}

void otx2_nix_tm_conf_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	TAILQ_INIT(&dev->node_list);
	TAILQ_INIT(&dev->shaper_profile_list);
}

int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev  *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t sq_cnt = eth_dev->data->nb_tx_queues;
	int rc;

	/* Free up all resources already held */
	rc = nix_tm_free_resources(dev, 0, 0, false);
	if (rc) {
		otx2_err("Failed to freeup existing resources,rc=%d", rc);
		return rc;
	}

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);
	dev->tm_flags = NIX_TM_DEFAULT_TREE;

	rc = nix_tm_prepare_default_tree(eth_dev);
	if (rc != 0)
		return rc;

	rc = nix_tm_alloc_resources(eth_dev, false);
	if (rc != 0)
		return rc;
	dev->tm_leaf_cnt = sq_cnt;

	return 0;
}

int
otx2_nix_tm_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc;

	/* Xmit is assumed to be disabled */
	/* Free up resources already held */
	rc = nix_tm_free_resources(dev, 0, 0, false);
	if (rc) {
		otx2_err("Failed to freeup existing resources,rc=%d", rc);
		return rc;
	}

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);

	dev->tm_flags = 0;
	return 0;
}
