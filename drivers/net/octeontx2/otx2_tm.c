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

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);
	dev->tm_flags = NIX_TM_DEFAULT_TREE;

	rc = nix_tm_prepare_default_tree(eth_dev);
	if (rc != 0)
		return rc;

	dev->tm_leaf_cnt = sq_cnt;

	return 0;
}

int
otx2_nix_tm_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* Clear shaper profiles */
	nix_tm_clear_shaper_profiles(dev);

	dev->tm_flags = 0;
	return 0;
}
