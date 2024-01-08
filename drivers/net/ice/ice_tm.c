/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#include <rte_tm_driver.h>

#include "ice_ethdev.h"
#include "ice_rxtx.h"

#define MAX_CHILDREN_PER_SCHED_NODE	8
#define MAX_CHILDREN_PER_TM_NODE	256

static int ice_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 __rte_unused struct rte_tm_error *error);
static int ice_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      struct rte_tm_node_params *params,
	      struct rte_tm_error *error);
static int ice_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
			    struct rte_tm_error *error);
static int ice_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error);
static int ice_shaper_profile_add(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			struct rte_tm_shaper_params *profile,
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
ice_node_param_check(struct ice_pf *pf, uint32_t node_id,
		      uint32_t priority, uint32_t weight,
		      struct rte_tm_node_params *params,
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
	if (node_id >= pf->dev_data->nb_tx_queues) {
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

	if (tm_node->level == ICE_TM_NODE_TYPE_QUEUE)
		*is_leaf = true;
	else
		*is_leaf = false;

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
ice_shaper_profile_param_check(struct rte_tm_shaper_params *profile,
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
			struct rte_tm_shaper_params *profile,
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
	      struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_tm_shaper_profile *shaper_profile = NULL;
	struct ice_tm_node *tm_node;
	struct ice_tm_node *parent_node;
	int ret;

	if (!params || !error)
		return -EINVAL;

	ret = ice_node_param_check(pf, node_id, priority, weight,
				    params, error);
	if (ret)
		return ret;

	/* check if the node is already existed */
	if (find_node(pf->tm_conf.root, node_id)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		return -EINVAL;
	}

	/* check the shaper profile id */
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		shaper_profile = ice_shaper_profile_search(dev,
			params->shaper_profile_id);
		if (!shaper_profile) {
			error->type =
				RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
			error->message = "shaper profile not exist";
			return -EINVAL;
		}
	}

	/* root node if not have a parent */
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		/* check level */
		if (level_id != ICE_TM_NODE_TYPE_PORT) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "Wrong level";
			return -EINVAL;
		}

		/* obviously no more than one root */
		if (pf->tm_conf.root) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "already have a root";
			return -EINVAL;
		}

		/* add the root node */
		tm_node = rte_zmalloc(NULL,
				      sizeof(struct ice_tm_node) +
				      sizeof(struct ice_tm_node *) * MAX_CHILDREN_PER_TM_NODE,
				      0);
		if (!tm_node)
			return -ENOMEM;
		tm_node->id = node_id;
		tm_node->level = ICE_TM_NODE_TYPE_PORT;
		tm_node->parent = NULL;
		tm_node->reference_count = 0;
		tm_node->shaper_profile = shaper_profile;
		tm_node->children =
			(void *)((uint8_t *)tm_node + sizeof(struct ice_tm_node));
		rte_memcpy(&tm_node->params, params,
				 sizeof(struct rte_tm_node_params));
		pf->tm_conf.root = tm_node;
		return 0;
	}

	/* check the parent node */
	parent_node = find_node(pf->tm_conf.root, parent_node_id);
	if (!parent_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}
	if (parent_node->level != ICE_TM_NODE_TYPE_PORT &&
	    parent_node->level != ICE_TM_NODE_TYPE_QGROUP) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not valid";
		return -EINVAL;
	}
	/* check level */
	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != parent_node->level + 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "Wrong level";
		return -EINVAL;
	}

	/* check the node number */
	if (parent_node->level == ICE_TM_NODE_TYPE_PORT) {
		/* check the queue group number */
		if (parent_node->reference_count >= pf->dev_data->nb_tx_queues) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queue groups";
			return -EINVAL;
		}
	} else {
		/* check the queue number */
		if (parent_node->reference_count >=
			MAX_CHILDREN_PER_SCHED_NODE) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queues";
			return -EINVAL;
		}
		if (node_id >= pf->dev_data->nb_tx_queues) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too large queue id";
			return -EINVAL;
		}
	}

	tm_node = rte_zmalloc(NULL,
			      sizeof(struct ice_tm_node) +
			      sizeof(struct ice_tm_node *) * MAX_CHILDREN_PER_TM_NODE,
			      0);
	if (!tm_node)
		return -ENOMEM;
	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	tm_node->level = parent_node->level + 1;
	tm_node->shaper_profile = shaper_profile;
	tm_node->children =
		(void *)((uint8_t *)tm_node + sizeof(struct ice_tm_node));
	tm_node->parent->children[tm_node->parent->reference_count] = tm_node;

	if (tm_node->priority != 0 && level_id != ICE_TM_NODE_TYPE_QUEUE &&
	    level_id != ICE_TM_NODE_TYPE_QGROUP)
		PMD_DRV_LOG(WARNING, "priority != 0 not supported in level %d",
			    level_id);

	if (tm_node->weight != 1 &&
	    level_id != ICE_TM_NODE_TYPE_QUEUE && level_id != ICE_TM_NODE_TYPE_QGROUP)
		PMD_DRV_LOG(WARNING, "weight != 1 not supported in level %d",
			    level_id);

	rte_memcpy(&tm_node->params, params,
			 sizeof(struct rte_tm_node_params));
	tm_node->parent->reference_count++;

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
	if (tm_node->level == ICE_TM_NODE_TYPE_PORT) {
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

static int ice_move_recfg_lan_txq(struct rte_eth_dev *dev,
				  struct ice_sched_node *queue_sched_node,
				  struct ice_sched_node *dst_node,
				  uint16_t queue_id)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_aqc_move_txqs_data *buf;
	struct ice_sched_node *queue_parent_node;
	uint8_t txqs_moved;
	int ret = ICE_SUCCESS;
	uint16_t buf_size = ice_struct_size(buf, txqs, 1);

	buf = (struct ice_aqc_move_txqs_data *)ice_malloc(hw, sizeof(*buf));
	if (buf == NULL)
		return -ENOMEM;

	queue_parent_node = queue_sched_node->parent;
	buf->src_teid = queue_parent_node->info.node_teid;
	buf->dest_teid = dst_node->info.node_teid;
	buf->txqs[0].q_teid = queue_sched_node->info.node_teid;
	buf->txqs[0].txq_id = queue_id;

	ret = ice_aq_move_recfg_lan_txq(hw, 1, true, false, false, false, 50,
					NULL, buf, buf_size, &txqs_moved, NULL);
	if (ret || txqs_moved == 0) {
		PMD_DRV_LOG(ERR, "move lan queue %u failed", queue_id);
		rte_free(buf);
		return ICE_ERR_PARAM;
	}

	if (queue_parent_node->num_children > 0) {
		queue_parent_node->num_children--;
		queue_parent_node->children[queue_parent_node->num_children] = NULL;
	} else {
		PMD_DRV_LOG(ERR, "invalid children number %d for queue %u",
			    queue_parent_node->num_children, queue_id);
		rte_free(buf);
		return ICE_ERR_PARAM;
	}
	dst_node->children[dst_node->num_children++] = queue_sched_node;
	queue_sched_node->parent = dst_node;
	ice_sched_query_elem(hw, queue_sched_node->info.node_teid, &queue_sched_node->info);

	rte_free(buf);
	return ret;
}

static int ice_set_node_rate(struct ice_hw *hw,
			     struct ice_tm_node *tm_node,
			     struct ice_sched_node *sched_node)
{
	enum ice_status status;
	bool reset = false;
	uint32_t peak = 0;
	uint32_t committed = 0;
	uint32_t rate;

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
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to set max bandwidth for node %u", tm_node->id);
		return -EINVAL;
	}

	if (reset || committed == 0)
		rate = ICE_SCHED_DFLT_BW;
	else
		rate = committed / 1000 * BITS_PER_BYTE;

	status = ice_sched_set_node_bw_lmt(hw->port_info,
					   sched_node,
					   ICE_MIN_BW,
					   rate);
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to set min bandwidth for node %u", tm_node->id);
		return -EINVAL;
	}

	return 0;
}

static int ice_cfg_hw_node(struct ice_hw *hw,
			   struct ice_tm_node *tm_node,
			   struct ice_sched_node *sched_node)
{
	enum ice_status status;
	uint8_t priority;
	uint16_t weight;
	int ret;

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

static struct ice_sched_node *ice_get_vsi_node(struct ice_hw *hw)
{
	struct ice_sched_node *node = hw->port_info->root;
	uint32_t vsi_layer = hw->num_tx_sched_layers - ICE_VSI_LAYER_OFFSET;
	uint32_t i;

	for (i = 0; i < vsi_layer; i++)
		node = node->children[0];

	return node;
}

static int ice_reset_noleaf_nodes(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_sched_node *vsi_node = ice_get_vsi_node(hw);
	struct ice_tm_node *root = pf->tm_conf.root;
	uint32_t i;
	int ret;

	/* reset vsi_node */
	ret = ice_set_node_rate(hw, NULL, vsi_node);
	if (ret) {
		PMD_DRV_LOG(ERR, "reset vsi node failed");
		return ret;
	}

	if (root == NULL)
		return 0;

	for (i = 0; i < root->reference_count; i++) {
		struct ice_tm_node *tm_node = root->children[i];

		if (tm_node->sched_node == NULL)
			continue;

		ret = ice_cfg_hw_node(hw, NULL, tm_node->sched_node);
		if (ret) {
			PMD_DRV_LOG(ERR, "reset queue group node %u failed", tm_node->id);
			return ret;
		}
		tm_node->sched_node = NULL;
	}

	return 0;
}

static int ice_remove_leaf_nodes(struct rte_eth_dev *dev)
{
	int ret = 0;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ret = ice_tx_queue_stop(dev, i);
		if (ret) {
			PMD_DRV_LOG(ERR, "stop queue %u failed", i);
			break;
		}
	}

	return ret;
}

static int ice_add_leaf_nodes(struct rte_eth_dev *dev)
{
	int ret = 0;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ret = ice_tx_queue_start(dev, i);
		if (ret) {
			PMD_DRV_LOG(ERR, "start queue %u failed", i);
			break;
		}
	}

	return ret;
}

int ice_do_hierarchy_commit(struct rte_eth_dev *dev,
			    int clear_on_fail,
			    struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_tm_node *root;
	struct ice_sched_node *vsi_node = NULL;
	struct ice_sched_node *queue_node;
	struct ice_tx_queue *txq;
	int ret_val = 0;
	uint32_t i;
	uint32_t idx_vsi_child;
	uint32_t idx_qg;
	uint32_t nb_vsi_child;
	uint32_t nb_qg;
	uint32_t qid;
	uint32_t q_teid;

	/* remove leaf nodes */
	ret_val = ice_remove_leaf_nodes(dev);
	if (ret_val) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		PMD_DRV_LOG(ERR, "reset no-leaf nodes failed");
		goto fail_clear;
	}

	/* reset no-leaf nodes. */
	ret_val = ice_reset_noleaf_nodes(dev);
	if (ret_val) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		PMD_DRV_LOG(ERR, "reset leaf nodes failed");
		goto add_leaf;
	}

	/* config vsi node */
	vsi_node = ice_get_vsi_node(hw);
	root = pf->tm_conf.root;

	ret_val = ice_set_node_rate(hw, root, vsi_node);
	if (ret_val) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		PMD_DRV_LOG(ERR,
			    "configure vsi node %u bandwidth failed",
			    root->id);
		goto add_leaf;
	}

	/* config queue group nodes */
	nb_vsi_child = vsi_node->num_children;
	nb_qg = vsi_node->children[0]->num_children;

	idx_vsi_child = 0;
	idx_qg = 0;

	if (root == NULL)
		goto commit;

	for (i = 0; i < root->reference_count; i++) {
		struct ice_tm_node *tm_node = root->children[i];
		struct ice_tm_node *tm_child_node;
		struct ice_sched_node *qgroup_sched_node =
			vsi_node->children[idx_vsi_child]->children[idx_qg];
		uint32_t j;

		ret_val = ice_cfg_hw_node(hw, tm_node, qgroup_sched_node);
		if (ret_val) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			PMD_DRV_LOG(ERR,
				    "configure queue group node %u failed",
				    tm_node->id);
			goto reset_leaf;
		}

		for (j = 0; j < tm_node->reference_count; j++) {
			tm_child_node = tm_node->children[j];
			qid = tm_child_node->id;
			ret_val = ice_tx_queue_start(dev, qid);
			if (ret_val) {
				error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
				PMD_DRV_LOG(ERR, "start queue %u failed", qid);
				goto reset_leaf;
			}
			txq = dev->data->tx_queues[qid];
			q_teid = txq->q_teid;
			queue_node = ice_sched_get_node(hw->port_info, q_teid);
			if (queue_node == NULL) {
				error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
				PMD_DRV_LOG(ERR, "get queue %u node failed", qid);
				goto reset_leaf;
			}
			if (queue_node->info.parent_teid != qgroup_sched_node->info.node_teid) {
				ret_val = ice_move_recfg_lan_txq(dev, queue_node,
								 qgroup_sched_node, qid);
				if (ret_val) {
					error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
					PMD_DRV_LOG(ERR, "move queue %u failed", qid);
					goto reset_leaf;
				}
			}
			ret_val = ice_cfg_hw_node(hw, tm_child_node, queue_node);
			if (ret_val) {
				error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
				PMD_DRV_LOG(ERR,
					    "configure queue group node %u failed",
					    tm_node->id);
				goto reset_leaf;
			}
		}

		idx_qg++;
		if (idx_qg >= nb_qg) {
			idx_qg = 0;
			idx_vsi_child++;
		}
		if (idx_vsi_child >= nb_vsi_child) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			PMD_DRV_LOG(ERR, "too many queues");
			goto reset_leaf;
		}
	}

commit:
	pf->tm_conf.committed = true;
	pf->tm_conf.clear_on_fail = clear_on_fail;

	return ret_val;

reset_leaf:
	ice_remove_leaf_nodes(dev);
add_leaf:
	ice_add_leaf_nodes(dev);
	ice_reset_noleaf_nodes(dev);
fail_clear:
	/* clear all the traffic manager configuration */
	if (clear_on_fail) {
		ice_tm_conf_uninit(dev);
		ice_tm_conf_init(dev);
	}
	return ret_val;
}

static int ice_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 struct rte_tm_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	/* if device not started, simply set committed flag and return. */
	if (!dev->data->dev_started) {
		pf->tm_conf.committed = true;
		pf->tm_conf.clear_on_fail = clear_on_fail;
		return 0;
	}

	return ice_do_hierarchy_commit(dev, clear_on_fail, error);
}
