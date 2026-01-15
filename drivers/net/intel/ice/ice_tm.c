/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#include <rte_ethdev.h>
#include <rte_tm_driver.h>

#include "ice_ethdev.h"
#include "ice_rxtx.h"

static int ice_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 struct rte_tm_error *error);
static int ice_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      const struct rte_tm_node_params *params,
	      struct rte_tm_error *error);
static int ice_node_query(const struct rte_eth_dev *dev, uint32_t node_id,
		uint32_t *parent_node_id, uint32_t *priority,
		uint32_t *weight, uint32_t *level_id,
		struct rte_tm_node_params *params,
		struct rte_tm_error *error);
static int ice_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
			    struct rte_tm_error *error);
static int ice_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error);
static int ice_shaper_profile_add(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			const struct rte_tm_shaper_params *profile,
			struct rte_tm_error *error);
static int ice_shaper_profile_del(struct rte_eth_dev *dev,
				   uint32_t shaper_profile_id,
				   struct rte_tm_error *error);

const struct rte_tm_ops ice_tm_ops = {
	.shaper_profile_add = ice_shaper_profile_add,
	.shaper_profile_delete = ice_shaper_profile_del,
	.node_add = ice_tm_node_add,
	.node_delete = ice_tm_node_delete,
	.node_type_get = ice_node_type_get,
	.node_query = ice_node_query,
	.hierarchy_commit = ice_hierarchy_commit,
};

void
ice_tm_conf_init(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	/* initialize node configuration */
	TAILQ_INIT(&pf->tm_conf.shaper_profile_list);
	pf->tm_conf.root = NULL;
	pf->tm_conf.committed = false;
	pf->tm_conf.clear_on_fail = false;
}

static void free_node(struct ice_tm_node *root)
{
	uint32_t i;

	if (root == NULL)
		return;

	for (i = 0; i < root->reference_count; i++)
		free_node(root->children[i]);

	rte_free(root);
}

void
ice_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_shaper_profile *shaper_profile;

	/* clear profile */
	while ((shaper_profile = TAILQ_FIRST(&pf->tm_conf.shaper_profile_list))) {
		TAILQ_REMOVE(&pf->tm_conf.shaper_profile_list, shaper_profile, node);
		rte_free(shaper_profile);
	}

	free_node(pf->tm_conf.root);
	pf->tm_conf.root = NULL;
}

static int
ice_node_param_check(uint32_t node_id,
		      uint32_t priority, uint32_t weight,
		      const struct rte_tm_node_params *params,
		      bool is_leaf,
		      struct rte_tm_error *error)
{
	/* checked all the unsupported parameter */
	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	if (priority >= 8) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "priority should be less than 8";
		return -EINVAL;
	}

	if (weight > 200 || weight < 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_WEIGHT;
		error->message = "weight must be between 1 and 200";
		return -EINVAL;
	}

	/* not support shared shaper */
	if (params->shared_shaper_id) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID;
		error->message = "shared shaper not supported";
		return -EINVAL;
	}
	if (params->n_shared_shapers) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS;
		error->message = "shared shaper not supported";
		return -EINVAL;
	}

	/* for non-leaf node */
	if (!is_leaf) {
		if (params->nonleaf.wfq_weight_mode) {
			error->type =
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE;
			error->message = "WFQ not supported";
			return -EINVAL;
		}
		if (params->nonleaf.n_sp_priorities != 1) {
			error->type =
				RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES;
			error->message = "SP priority not supported";
			return -EINVAL;
		} else if (params->nonleaf.wfq_weight_mode &&
			   !(*params->nonleaf.wfq_weight_mode)) {
			error->type =
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE;
			error->message = "WFP should be byte mode";
			return -EINVAL;
		}

		return 0;
	}

	/* for leaf node */
	if (node_id >= RTE_MAX_QUEUES_PER_PORT) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "Node ID out of range for a leaf node.";
		return -EINVAL;
	}
	if (params->leaf.cman) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN;
		error->message = "Congestion management not supported";
		return -EINVAL;
	}
	if (params->leaf.wred.wred_profile_id !=
	    RTE_TM_WRED_PROFILE_ID_NONE) {
		error->type =
			RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID;
		error->message = "WRED not supported";
		return -EINVAL;
	}
	if (params->leaf.wred.shared_wred_context_id) {
		error->type =
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_WRED_CONTEXT_ID;
		error->message = "WRED not supported";
		return -EINVAL;
	}
	if (params->leaf.wred.n_shared_wred_contexts) {
		error->type =
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS;
		error->message = "WRED not supported";
		return -EINVAL;
	}

	return 0;
}

static struct ice_tm_node *
find_node(struct ice_tm_node *root, uint32_t id)
{
	uint32_t i;

	if (root == NULL || root->id == id)
		return root;

	for (i = 0; i < root->reference_count; i++) {
		struct ice_tm_node *node = find_node(root->children[i], id);

		if (node)
			return node;
	}

	return NULL;
}

static inline uint8_t
ice_get_leaf_level(const struct ice_pf *pf)
{
	const struct ice_hw *hw = ICE_PF_TO_HW(pf);
	return hw->num_tx_sched_layers - pf->tm_conf.hidden_layers - 1;
}

static int
ice_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_node *tm_node;

	if (!is_leaf || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = find_node(pf->tm_conf.root, node_id);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (tm_node->level == ice_get_leaf_level(pf))
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static int
ice_node_query(const struct rte_eth_dev *dev, uint32_t node_id,
		uint32_t *parent_node_id, uint32_t *priority,
		uint32_t *weight, uint32_t *level_id,
		struct rte_tm_node_params *params,
		struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_node *tm_node;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = find_node(pf->tm_conf.root, node_id);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EEXIST;
	}

	if (parent_node_id != NULL) {
		if (tm_node->parent != NULL)
			*parent_node_id = tm_node->parent->id;
		else
			*parent_node_id = RTE_TM_NODE_ID_NULL;
	}

	if (priority != NULL)
		*priority = tm_node->priority;

	if (weight != NULL)
		*weight = tm_node->weight;

	if (level_id != NULL)
		*level_id = tm_node->level;

	if (params != NULL)
		*params = tm_node->params;

	return 0;
}

static inline struct ice_tm_shaper_profile *
ice_shaper_profile_search(struct rte_eth_dev *dev,
			   uint32_t shaper_profile_id)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_shaper_profile_list *shaper_profile_list =
		&pf->tm_conf.shaper_profile_list;
	struct ice_tm_shaper_profile *shaper_profile;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (shaper_profile_id == shaper_profile->shaper_profile_id)
			return shaper_profile;
	}

	return NULL;
}

static int
ice_shaper_profile_param_check(const struct rte_tm_shaper_params *profile,
				struct rte_tm_error *error)
{
	/* min bucket size not supported */
	if (profile->committed.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
		error->message = "committed bucket size not supported";
		return -EINVAL;
	}
	/* max bucket size not supported */
	if (profile->peak.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
		error->message = "peak bucket size not supported";
		return -EINVAL;
	}
	/* length adjustment not supported */
	if (profile->pkt_length_adjust) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN;
		error->message = "packet length adjustment not supported";
		return -EINVAL;
	}

	return 0;
}

static int
ice_shaper_profile_add(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			const struct rte_tm_shaper_params *profile,
			struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_shaper_profile *shaper_profile;
	int ret;

	if (!profile || !error)
		return -EINVAL;

	ret = ice_shaper_profile_param_check(profile, error);
	if (ret)
		return ret;

	shaper_profile = ice_shaper_profile_search(dev, shaper_profile_id);

	if (shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		return -EINVAL;
	}

	shaper_profile = rte_zmalloc("ice_tm_shaper_profile",
				     sizeof(struct ice_tm_shaper_profile),
				     0);
	if (!shaper_profile)
		return -ENOMEM;
	shaper_profile->shaper_profile_id = shaper_profile_id;
	rte_memcpy(&shaper_profile->profile, profile,
			 sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&pf->tm_conf.shaper_profile_list,
			  shaper_profile, node);

	return 0;
}

static int
ice_shaper_profile_del(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_shaper_profile *shaper_profile;

	if (!error)
		return -EINVAL;

	shaper_profile = ice_shaper_profile_search(dev, shaper_profile_id);

	if (!shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID not exist";
		return -EINVAL;
	}

	/* don't delete a profile if it's used by one or several nodes */
	if (shaper_profile->reference_count) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "profile in use";
		return -EINVAL;
	}

	TAILQ_REMOVE(&pf->tm_conf.shaper_profile_list, shaper_profile, node);
	rte_free(shaper_profile);

	return 0;
}

static int
ice_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      const struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_tm_shaper_profile *shaper_profile = NULL;
	struct ice_tm_node *tm_node;
	struct ice_tm_node *parent_node = NULL;
	uint8_t layer_offset = pf->tm_conf.hidden_layers;
	int ret;

	if (!params || !error)
		return -EINVAL;

	/* check the shaper profile id */
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		shaper_profile = ice_shaper_profile_search(dev, params->shaper_profile_id);
		if (!shaper_profile) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
			error->message = "shaper profile does not exist";
			return -EINVAL;
		}
	}

	/* root node if not have a parent */
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		/* check level */
		if (level_id != 0) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "Wrong level, root node (NULL parent) must be at level 0";
			return -EINVAL;
		}

		/* obviously no more than one root */
		if (pf->tm_conf.root) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "already have a root";
			return -EINVAL;
		}

		ret = ice_node_param_check(node_id, priority, weight, params, false, error);
		if (ret)
			return ret;

		/* add the root node */
		tm_node = rte_zmalloc(NULL,
				sizeof(struct ice_tm_node) +
				sizeof(struct ice_tm_node *) * hw->max_children[layer_offset],
				0);
		if (!tm_node)
			return -ENOMEM;
		tm_node->id = node_id;
		tm_node->level = 0;
		tm_node->parent = NULL;
		tm_node->reference_count = 0;
		tm_node->shaper_profile = shaper_profile;
		tm_node->children = RTE_PTR_ADD(tm_node, sizeof(struct ice_tm_node));
		tm_node->params = *params;
		pf->tm_conf.root = tm_node;
		return 0;
	}

	parent_node = find_node(pf->tm_conf.root, parent_node_id);
	if (!parent_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}

	/* check level */
	if (level_id == RTE_TM_NODE_LEVEL_ID_ANY)
		level_id = parent_node->level + 1;
	else if (level_id != parent_node->level + 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "Wrong level";
		return -EINVAL;
	}

	ret = ice_node_param_check(node_id, priority, weight,
			params, level_id == ice_get_leaf_level(pf), error);
	if (ret)
		return ret;

	/* check if the node is already existed */
	if (find_node(pf->tm_conf.root, node_id)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		return -EINVAL;
	}

	/* check the parent node */
	/* for n-level hierarchy, level n-1 is leaf, so last level with children is n-2 */
	if ((int)parent_node->level > hw->num_tx_sched_layers - 2) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not valid";
		return -EINVAL;
	}

	/* check the max children allowed at this level */
	if (parent_node->reference_count >= hw->max_children[parent_node->level]) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "insufficient number of child nodes supported";
		return -EINVAL;
	}

	tm_node = rte_zmalloc(NULL,
			sizeof(struct ice_tm_node) +
			sizeof(struct ice_tm_node *) * hw->max_children[level_id + layer_offset],
			0);
	if (!tm_node)
		return -ENOMEM;
	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	tm_node->level = level_id;
	tm_node->shaper_profile = shaper_profile;
	tm_node->children = RTE_PTR_ADD(tm_node, sizeof(struct ice_tm_node));
	tm_node->parent->children[tm_node->parent->reference_count++] = tm_node;
	tm_node->params = *params;

	if (tm_node->priority != 0)
		PMD_DRV_LOG(WARNING, "priority != 0 not supported in level %d", level_id);

	if (tm_node->weight != 1 && level_id == 0)
		PMD_DRV_LOG(WARNING, "weight != 1 not supported in level %d", level_id);


	return 0;
}

static int
ice_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		 struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_node *tm_node;
	uint32_t i, j;

	if (!error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = find_node(pf->tm_conf.root, node_id);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	/* the node should have no child */
	if (tm_node->reference_count) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message =
			"cannot delete a node which has children";
		return -EINVAL;
	}

	/* root node */
	if (tm_node->level == 0) {
		rte_free(tm_node);
		pf->tm_conf.root = NULL;
		return 0;
	}

	/* queue group or queue node */
	for (i = 0; i < tm_node->parent->reference_count; i++)
		if (tm_node->parent->children[i] == tm_node)
			break;

	for (j = i ; j < tm_node->parent->reference_count - 1; j++)
		tm_node->parent->children[j] = tm_node->parent->children[j + 1];

	tm_node->parent->reference_count--;
	rte_free(tm_node);

	return 0;
}

static int ice_set_node_rate(struct ice_hw *hw,
			     struct ice_tm_node *tm_node,
			     struct ice_sched_node *sched_node)
{
	bool reset = false;
	uint32_t peak = 0;
	uint32_t committed = 0;
	uint32_t rate;
	int status;

	if (tm_node == NULL || tm_node->shaper_profile == NULL) {
		reset = true;
	} else {
		peak = (uint32_t)tm_node->shaper_profile->profile.peak.rate;
		committed = (uint32_t)tm_node->shaper_profile->profile.committed.rate;
	}

	if (reset || peak == 0)
		rate = ICE_SCHED_DFLT_BW;
	else
		rate = peak / 1000 * BITS_PER_BYTE;


	status = ice_sched_set_node_bw_lmt(hw->port_info,
					   sched_node,
					   ICE_MAX_BW,
					   rate);
	if (status)
		return -EINVAL;

	if (reset || committed == 0)
		rate = ICE_SCHED_DFLT_BW;
	else
		rate = committed / 1000 * BITS_PER_BYTE;

	status = ice_sched_set_node_bw_lmt(hw->port_info,
					   sched_node,
					   ICE_MIN_BW,
					   rate);
	if (status)
		return -EINVAL;

	return 0;
}

static int ice_cfg_hw_node(struct ice_hw *hw,
			   struct ice_tm_node *tm_node,
			   struct ice_sched_node *sched_node)
{
	uint8_t priority;
	uint16_t weight;
	int status, ret;

	ret = ice_set_node_rate(hw, tm_node, sched_node);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "configure queue group %u bandwidth failed",
			    sched_node->info.node_teid);
		return ret;
	}

	priority = tm_node ? (7 - tm_node->priority) : 0;
	status = ice_sched_cfg_sibl_node_prio(hw->port_info,
					      sched_node,
					      priority);
	if (status) {
		PMD_DRV_LOG(ERR, "configure node %u priority %u failed",
			    sched_node->info.node_teid,
			    priority);
		return -EINVAL;
	}

	weight = tm_node ? (uint16_t)tm_node->weight : 4;

	status = ice_sched_cfg_node_bw_alloc(hw, sched_node,
					     ICE_MAX_BW,
					     weight);
	if (status) {
		PMD_DRV_LOG(ERR, "configure node %u weight %u failed",
			    sched_node->info.node_teid,
			    weight);
		return -EINVAL;
	}

	return 0;
}

int
ice_tm_setup_txq_node(struct ice_pf *pf, struct ice_hw *hw, uint16_t qid, uint32_t teid)
{
	struct ice_sched_node *hw_node = ice_sched_find_node_by_teid(hw->port_info->root, teid);
	struct ice_tm_node *sw_node = find_node(pf->tm_conf.root, qid);

	/* bad node teid passed */
	if (hw_node == NULL)
		return -ENOENT;

	/* not configured in hierarchy */
	if (sw_node == NULL)
		return 0;

	sw_node->sched_node = hw_node;

	/* if the queue node has been put in the wrong place in hierarchy */
	if (hw_node->parent != sw_node->parent->sched_node) {
		struct ice_aqc_move_txqs_data *buf;
		uint8_t txqs_moved = 0;
		uint16_t buf_size = ice_struct_size(buf, txqs, 1);

		buf = ice_malloc(hw, buf_size);
		if (buf == NULL)
			return -ENOMEM;

		struct ice_sched_node *parent = hw_node->parent;
		struct ice_sched_node *new_parent = sw_node->parent->sched_node;
		buf->src_teid = parent->info.node_teid;
		buf->dest_teid = new_parent->info.node_teid;
		buf->txqs[0].q_teid = hw_node->info.node_teid;
		buf->txqs[0].txq_id = qid;

		int ret = ice_aq_move_recfg_lan_txq(hw, 1, true, false, false, false, 50,
						NULL, buf, buf_size, &txqs_moved, NULL);
		if (ret || txqs_moved == 0) {
			PMD_DRV_LOG(ERR, "move lan queue %u failed", qid);
			ice_free(hw, buf);
			return ICE_ERR_PARAM;
		}

		/* now update the ice_sched_nodes to match physical layout */
		new_parent->children[new_parent->num_children++] = hw_node;
		hw_node->parent = new_parent;
		ice_sched_query_elem(hw, hw_node->info.node_teid, &hw_node->info);
		for (uint16_t i = 0; i < parent->num_children; i++)
			if (parent->children[i] == hw_node) {
				/* to remove, just overwrite the old node slot with the last ptr */
				parent->children[i] = parent->children[--parent->num_children];
				break;
			}
	}

	return ice_cfg_hw_node(hw, sw_node, hw_node);
}

/* from a given node, recursively deletes all the nodes that belong to that vsi.
 * Any nodes which can't be deleted because they have children belonging to a different
 * VSI, are now also adjusted to belong to that VSI also
 */
static int
free_sched_node_recursive(struct ice_port_info *pi, const struct ice_sched_node *root,
		struct ice_sched_node *node, uint8_t vsi_id)
{
	uint16_t i = 0;

	while (i < node->num_children) {
		if (node->children[i]->vsi_handle != vsi_id) {
			i++;
			continue;
		}
		free_sched_node_recursive(pi, root, node->children[i], vsi_id);
	}

	if (node != root) {
		if (node->num_children == 0)
			ice_free_sched_node(pi, node);
		else
			node->vsi_handle = node->children[0]->vsi_handle;
	}

	return 0;
}

static int
create_sched_node_recursive(struct ice_pf *pf, struct ice_port_info *pi,
		 struct ice_tm_node *sw_node, struct ice_sched_node *hw_root, uint16_t *created)
{
	struct ice_sched_node *parent = sw_node->sched_node;
	uint32_t teid;
	uint16_t added;

	/* first create all child nodes */
	for (uint16_t i = 0; i < sw_node->reference_count; i++) {
		struct ice_tm_node *tm_node = sw_node->children[i];
		int res = ice_sched_add_elems(pi, hw_root,
				parent, parent->tx_sched_layer + 1,
				1 /* num nodes */, &added, &teid,
				NULL /* no pre-alloc */);
		if (res != 0) {
			PMD_DRV_LOG(ERR, "Error with ice_sched_add_elems, adding child node to teid %u",
					parent->info.node_teid);
			return -1;
		}
		struct ice_sched_node *hw_node = ice_sched_find_node_by_teid(parent, teid);
		if (ice_cfg_hw_node(pi->hw, tm_node, hw_node) != 0) {
			PMD_DRV_LOG(ERR, "Error configuring node %u at layer %u",
					teid, parent->tx_sched_layer + 1);
			return -1;
		}
		tm_node->sched_node = hw_node;
		created[hw_node->tx_sched_layer]++;
	}

	/* if we have just created the child nodes in the q-group, i.e. last non-leaf layer,
	 * then just return, rather than trying to create leaf nodes.
	 * That is done later at queue start.
	 */
	if (sw_node->level + 2 == ice_get_leaf_level(pf))
		return 0;

	for (uint16_t i = 0; i < sw_node->reference_count; i++) {
		if (sw_node->children[i]->reference_count == 0)
			continue;

		if (create_sched_node_recursive(pf, pi, sw_node->children[i], hw_root, created) < 0)
			return -1;
	}
	return 0;
}

static int
commit_new_hierarchy(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_port_info *pi = hw->port_info;
	struct ice_tm_node *sw_root = pf->tm_conf.root;
	const uint16_t new_root_level = pf->tm_conf.hidden_layers;
	/* count nodes per hw level, not per logical */
	uint16_t nodes_created_per_level[ICE_TM_MAX_LAYERS] = {0};
	uint8_t q_lvl = ice_get_leaf_level(pf);
	uint8_t qg_lvl = q_lvl - 1;
	struct ice_sched_node *new_vsi_root = hw->vsi_ctx[pf->main_vsi->idx]->sched.vsi_node[0];

	if (sw_root == NULL) {
		PMD_DRV_LOG(ERR, "No root node defined in TM hierarchy");
		return -1;
	}

	/* handle case where VSI node needs to move DOWN the hierarchy */
	while (new_vsi_root->tx_sched_layer < new_root_level) {
		if (new_vsi_root->num_children == 0)
			return -1;
		/* remove all child nodes but the first */
		while (new_vsi_root->num_children > 1)
			free_sched_node_recursive(pi, new_vsi_root,
					new_vsi_root->children[1],
					new_vsi_root->vsi_handle);
		new_vsi_root = new_vsi_root->children[0];
	}
	/* handle case where VSI node needs to move UP the hierarchy */
	while (new_vsi_root->tx_sched_layer > new_root_level)
		new_vsi_root = new_vsi_root->parent;

	free_sched_node_recursive(pi, new_vsi_root, new_vsi_root, new_vsi_root->vsi_handle);

	sw_root->sched_node = new_vsi_root;
	if (create_sched_node_recursive(pf, pi, sw_root, new_vsi_root, nodes_created_per_level) < 0)
		return -1;
	for (uint16_t i = 0; i < RTE_DIM(nodes_created_per_level); i++)
		PMD_DRV_LOG(DEBUG, "Created %u nodes at level %u",
				nodes_created_per_level[i], i);
	hw->vsi_ctx[pf->main_vsi->idx]->sched.vsi_node[0] = new_vsi_root;

	pf->main_vsi->nb_qps =
			RTE_MIN(nodes_created_per_level[qg_lvl] * hw->max_children[qg_lvl],
				hw->layer_info[q_lvl].max_device_nodes);

	pf->tm_conf.committed = true; /* set flag to be checks on queue start */

	return ice_alloc_lan_q_ctx(hw, 0, 0, pf->main_vsi->nb_qps);
}

static int
ice_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 struct rte_tm_error *error)
{
	bool restart = false;

	/* commit should only be done to topology before start
	 * If port is already started, stop it and then restart when done.
	 */
	if (dev->data->dev_started) {
		if (rte_eth_dev_stop(dev->data->port_id) != 0) {
			error->message = "Device failed to Stop";
			return -1;
		}
		restart = true;
	}

	int ret = commit_new_hierarchy(dev);
	if (ret < 0 && clear_on_fail) {
		ice_tm_conf_uninit(dev);
		ice_tm_conf_init(dev);
	}

	if (restart) {
		if (rte_eth_dev_start(dev->data->port_id) != 0) {
			error->message = "Device failed to Start";
			return -1;
		}
	}
	return ret;
}
