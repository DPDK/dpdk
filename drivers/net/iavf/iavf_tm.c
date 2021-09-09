/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */
#include <rte_tm_driver.h>

#include "iavf.h"

static int iavf_hierarchy_commit(struct rte_eth_dev *dev,
				 __rte_unused int clear_on_fail,
				 __rte_unused struct rte_tm_error *error);
static int iavf_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      struct rte_tm_node_params *params,
	      struct rte_tm_error *error);
static int iavf_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
			    struct rte_tm_error *error);
static int iavf_tm_capabilities_get(struct rte_eth_dev *dev,
			 struct rte_tm_capabilities *cap,
			 struct rte_tm_error *error);
static int iavf_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error);
static int iavf_node_capabilities_get(struct rte_eth_dev *dev,
				      uint32_t node_id,
				      struct rte_tm_node_capabilities *cap,
				      struct rte_tm_error *error);
static int iavf_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error);

const struct rte_tm_ops iavf_tm_ops = {
	.node_add = iavf_tm_node_add,
	.node_delete = iavf_tm_node_delete,
	.capabilities_get = iavf_tm_capabilities_get,
	.level_capabilities_get = iavf_level_capabilities_get,
	.node_capabilities_get = iavf_node_capabilities_get,
	.node_type_get = iavf_node_type_get,
	.hierarchy_commit = iavf_hierarchy_commit,
};

void
iavf_tm_conf_init(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	/* initialize node configuration */
	vf->tm_conf.root = NULL;
	TAILQ_INIT(&vf->tm_conf.tc_list);
	TAILQ_INIT(&vf->tm_conf.queue_list);
	vf->tm_conf.nb_tc_node = 0;
	vf->tm_conf.nb_queue_node = 0;
	vf->tm_conf.committed = false;
}

void
iavf_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_tm_node *tm_node;

	/* clear node configuration */
	while ((tm_node = TAILQ_FIRST(&vf->tm_conf.queue_list))) {
		TAILQ_REMOVE(&vf->tm_conf.queue_list, tm_node, node);
		rte_free(tm_node);
	}
	vf->tm_conf.nb_queue_node = 0;
	while ((tm_node = TAILQ_FIRST(&vf->tm_conf.tc_list))) {
		TAILQ_REMOVE(&vf->tm_conf.tc_list, tm_node, node);
		rte_free(tm_node);
	}
	vf->tm_conf.nb_tc_node = 0;
	if (vf->tm_conf.root) {
		rte_free(vf->tm_conf.root);
		vf->tm_conf.root = NULL;
	}
}

static inline struct iavf_tm_node *
iavf_tm_node_search(struct rte_eth_dev *dev,
		    uint32_t node_id, enum iavf_tm_node_type *node_type)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_tm_node_list *tc_list = &vf->tm_conf.tc_list;
	struct iavf_tm_node_list *queue_list = &vf->tm_conf.queue_list;
	struct iavf_tm_node *tm_node;

	if (vf->tm_conf.root && vf->tm_conf.root->id == node_id) {
		*node_type = IAVF_TM_NODE_TYPE_PORT;
		return vf->tm_conf.root;
	}

	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (tm_node->id == node_id) {
			*node_type = IAVF_TM_NODE_TYPE_TC;
			return tm_node;
		}
	}

	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->id == node_id) {
			*node_type = IAVF_TM_NODE_TYPE_QUEUE;
			return tm_node;
		}
	}

	return NULL;
}

static int
iavf_node_param_check(struct iavf_info *vf, uint32_t node_id,
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

	if (priority) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "priority should be 0";
		return -EINVAL;
	}

	if (weight != 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_WEIGHT;
		error->message = "weight must be 1";
		return -EINVAL;
	}

	/* not support shaper profile */
	if (params->shaper_profile_id) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
		error->message = "shaper profile not supported";
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
	if (node_id >= vf->num_queue_pairs) {
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

static int
iavf_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error)
{
	enum iavf_tm_node_type node_type = IAVF_TM_NODE_TYPE_MAX;
	struct iavf_tm_node *tm_node;

	if (!is_leaf || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = iavf_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (node_type == IAVF_TM_NODE_TYPE_QUEUE)
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static int
iavf_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	enum iavf_tm_node_type node_type = IAVF_TM_NODE_TYPE_MAX;
	enum iavf_tm_node_type parent_node_type = IAVF_TM_NODE_TYPE_MAX;
	struct iavf_tm_node *tm_node;
	struct iavf_tm_node *parent_node;
	uint16_t tc_nb = vf->qos_cap->num_elem;
	int ret;

	if (!params || !error)
		return -EINVAL;

	/* if already committed */
	if (vf->tm_conf.committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	ret = iavf_node_param_check(vf, node_id, priority, weight,
				    params, error);
	if (ret)
		return ret;

	/* check if the node is already existed */
	if (iavf_tm_node_search(dev, node_id, &node_type)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		return -EINVAL;
	}

	/* root node if not have a parent */
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		/* check level */
		if (level_id != IAVF_TM_NODE_TYPE_PORT) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "Wrong level";
			return -EINVAL;
		}

		/* obviously no more than one root */
		if (vf->tm_conf.root) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "already have a root";
			return -EINVAL;
		}

		/* add the root node */
		tm_node = rte_zmalloc("iavf_tm_node",
				      sizeof(struct iavf_tm_node),
				      0);
		if (!tm_node)
			return -ENOMEM;
		tm_node->id = node_id;
		tm_node->parent = NULL;
		tm_node->reference_count = 0;
		rte_memcpy(&tm_node->params, params,
				 sizeof(struct rte_tm_node_params));
		vf->tm_conf.root = tm_node;
		return 0;
	}

	/* TC or queue node */
	/* check the parent node */
	parent_node = iavf_tm_node_search(dev, parent_node_id,
					  &parent_node_type);
	if (!parent_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}
	if (parent_node_type != IAVF_TM_NODE_TYPE_PORT &&
	    parent_node_type != IAVF_TM_NODE_TYPE_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not root or TC";
		return -EINVAL;
	}
	/* check level */
	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != (uint32_t)parent_node_type + 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "Wrong level";
		return -EINVAL;
	}

	/* check the node number */
	if (parent_node_type == IAVF_TM_NODE_TYPE_PORT) {
		/* check the TC number */
		if (vf->tm_conf.nb_tc_node >= tc_nb) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many TCs";
			return -EINVAL;
		}
	} else {
		/* check the queue number */
		if (parent_node->reference_count >= vf->num_queue_pairs) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queues";
			return -EINVAL;
		}
		if (node_id >= vf->num_queue_pairs) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too large queue id";
			return -EINVAL;
		}
	}

	/* add the TC or queue node */
	tm_node = rte_zmalloc("iavf_tm_node",
			      sizeof(struct iavf_tm_node),
			      0);
	if (!tm_node)
		return -ENOMEM;
	tm_node->id = node_id;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	rte_memcpy(&tm_node->params, params,
			 sizeof(struct rte_tm_node_params));
	if (parent_node_type == IAVF_TM_NODE_TYPE_PORT) {
		TAILQ_INSERT_TAIL(&vf->tm_conf.tc_list,
				  tm_node, node);
		tm_node->tc = vf->tm_conf.nb_tc_node;
		vf->tm_conf.nb_tc_node++;
	} else {
		TAILQ_INSERT_TAIL(&vf->tm_conf.queue_list,
				  tm_node, node);
		tm_node->tc = parent_node->tc;
		vf->tm_conf.nb_queue_node++;
	}
	tm_node->parent->reference_count++;

	return 0;
}

static int
iavf_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		 struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	enum iavf_tm_node_type node_type = IAVF_TM_NODE_TYPE_MAX;
	struct iavf_tm_node *tm_node;

	if (!error)
		return -EINVAL;

	/* if already committed */
	if (vf->tm_conf.committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = iavf_tm_node_search(dev, node_id, &node_type);
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
	if (node_type == IAVF_TM_NODE_TYPE_PORT) {
		rte_free(tm_node);
		vf->tm_conf.root = NULL;
		return 0;
	}

	/* TC or queue node */
	tm_node->parent->reference_count--;
	if (node_type == IAVF_TM_NODE_TYPE_TC) {
		TAILQ_REMOVE(&vf->tm_conf.tc_list, tm_node, node);
		vf->tm_conf.nb_tc_node--;
	} else {
		TAILQ_REMOVE(&vf->tm_conf.queue_list, tm_node, node);
		vf->tm_conf.nb_queue_node--;
	}
	rte_free(tm_node);

	return 0;
}

static int
iavf_tm_capabilities_get(struct rte_eth_dev *dev,
			 struct rte_tm_capabilities *cap,
			 struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	uint16_t tc_nb = vf->qos_cap->num_elem;

	if (!cap || !error)
		return -EINVAL;

	if (tc_nb > vf->vf_res->num_queue_pairs)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	/**
	 * support port + TCs + queues
	 * here shows the max capability not the current configuration.
	 */
	cap->n_nodes_max = 1 + IAVF_MAX_TRAFFIC_CLASS
		+ vf->num_queue_pairs;
	cap->n_levels_max = 3; /* port, TC, queue */
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->n_nodes_max;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 0;
	/* Bytes per second */
	cap->shaper_private_rate_max =
		(uint64_t)vf->link_speed * 1000000 / IAVF_BITS_PER_BYTE;
	cap->shaper_private_packet_mode_supported = 0;
	cap->shaper_private_byte_mode_supported = 1;
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->shaper_shared_packet_mode_supported = 0;
	cap->shaper_shared_byte_mode_supported = 0;
	cap->sched_n_children_max = vf->num_queue_pairs;
	cap->sched_sp_n_priorities_max = 1;
	cap->sched_wfq_n_children_per_group_max = 0;
	cap->sched_wfq_n_groups_max = 0;
	cap->sched_wfq_weight_max = 1;
	cap->sched_wfq_packet_mode_supported = 0;
	cap->sched_wfq_byte_mode_supported = 0;
	cap->cman_head_drop_supported = 0;
	cap->dynamic_update_mask = 0;
	cap->shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD;
	cap->shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;
	cap->cman_wred_context_n_max = 0;
	cap->cman_wred_context_private_n_max = 0;
	cap->cman_wred_context_shared_n_max = 0;
	cap->cman_wred_context_shared_n_nodes_per_context_max = 0;
	cap->cman_wred_context_shared_n_contexts_per_node_max = 0;
	cap->stats_mask = 0;

	return 0;
}

static int
iavf_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	if (!cap || !error)
		return -EINVAL;

	if (level_id >= IAVF_TM_NODE_TYPE_MAX) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		return -EINVAL;
	}

	/* root node */
	if (level_id == IAVF_TM_NODE_TYPE_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->n_nodes_leaf_max = 0;
	} else if (level_id == IAVF_TM_NODE_TYPE_TC) {
		/* TC */
		cap->n_nodes_max = IAVF_MAX_TRAFFIC_CLASS;
		cap->n_nodes_nonleaf_max = IAVF_MAX_TRAFFIC_CLASS;
		cap->n_nodes_leaf_max = 0;
	} else {
		/* queue */
		cap->n_nodes_max = vf->num_queue_pairs;
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = vf->num_queue_pairs;
	}

	cap->non_leaf_nodes_identical = true;
	cap->leaf_nodes_identical = true;

	if (level_id != IAVF_TM_NODE_TYPE_QUEUE) {
		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = false;
		cap->nonleaf.shaper_private_rate_min = 0;
		/* Bytes per second */
		cap->nonleaf.shaper_private_rate_max =
			(uint64_t)vf->link_speed * 1000000 / IAVF_BITS_PER_BYTE;
		cap->nonleaf.shaper_private_packet_mode_supported = 0;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;
		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.shaper_shared_packet_mode_supported = 0;
		cap->nonleaf.shaper_shared_byte_mode_supported = 0;
		if (level_id == IAVF_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				IAVF_MAX_TRAFFIC_CLASS;
		else
			cap->nonleaf.sched_n_children_max =
				vf->num_queue_pairs;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;
		cap->nonleaf.stats_mask = 0;

		return 0;
	}

	/* queue node */
	cap->leaf.shaper_private_supported = false;
	cap->leaf.shaper_private_dual_rate_supported = false;
	cap->leaf.shaper_private_rate_min = 0;
	/* Bytes per second */
	cap->leaf.shaper_private_rate_max =
		(uint64_t)vf->link_speed * 1000000 / IAVF_BITS_PER_BYTE;
	cap->leaf.shaper_private_packet_mode_supported = 0;
	cap->leaf.shaper_private_byte_mode_supported = 1;
	cap->leaf.shaper_shared_n_max = 0;
	cap->leaf.shaper_shared_packet_mode_supported = 0;
	cap->leaf.shaper_shared_byte_mode_supported = 0;
	cap->leaf.cman_head_drop_supported = false;
	cap->leaf.cman_wred_context_private_supported = true;
	cap->leaf.cman_wred_context_shared_n_max = 0;
	cap->leaf.stats_mask = 0;

	return 0;
}

static int
iavf_node_capabilities_get(struct rte_eth_dev *dev,
			   uint32_t node_id,
			   struct rte_tm_node_capabilities *cap,
			   struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	enum iavf_tm_node_type node_type;
	struct virtchnl_qos_cap_elem tc_cap;
	struct iavf_tm_node *tm_node;

	if (!cap || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = iavf_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (node_type != IAVF_TM_NODE_TYPE_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "not support capability get";
		return -EINVAL;
	}

	tc_cap = vf->qos_cap->cap[tm_node->tc];
	if (tc_cap.tc_num != tm_node->tc) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "tc not match";
		return -EINVAL;
	}

	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported = false;
	/* Bytes per second */
	cap->shaper_private_rate_min =
		(uint64_t)tc_cap.shaper.committed * 1000 / IAVF_BITS_PER_BYTE;
	cap->shaper_private_rate_max =
		(uint64_t)tc_cap.shaper.peak * 1000 / IAVF_BITS_PER_BYTE;
	cap->shaper_shared_n_max = 0;
	cap->nonleaf.sched_n_children_max = vf->num_queue_pairs;
	cap->nonleaf.sched_sp_n_priorities_max = 1;
	cap->nonleaf.sched_wfq_n_children_per_group_max = 1;
	cap->nonleaf.sched_wfq_n_groups_max = 0;
	cap->nonleaf.sched_wfq_weight_max = tc_cap.weight;
	cap->stats_mask = 0;

	return 0;
}

static int iavf_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 __rte_unused struct rte_tm_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct virtchnl_queue_tc_mapping *q_tc_mapping;
	struct iavf_tm_node_list *queue_list = &vf->tm_conf.queue_list;
	struct iavf_tm_node *tm_node;
	struct iavf_qtc_map *qtc_map;
	uint16_t size;
	int index = 0, node_committed = 0;
	int i, ret_val = IAVF_SUCCESS;

	/* check if port is stopped */
	if (adapter->stopped != 1) {
		PMD_DRV_LOG(ERR, "Please stop port first");
		ret_val = IAVF_ERR_NOT_READY;
		goto err;
	}

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS)) {
		PMD_DRV_LOG(ERR, "VF queue tc mapping is not supported");
		ret_val = IAVF_NOT_SUPPORTED;
		goto fail_clear;
	}

	/* check if all TC nodes are set with VF vsi */
	if (vf->tm_conf.nb_tc_node != vf->qos_cap->num_elem) {
		PMD_DRV_LOG(ERR, "Does not set VF vsi nodes to all TCs");
		ret_val = IAVF_ERR_PARAM;
		goto fail_clear;
	}

	size = sizeof(*q_tc_mapping) + sizeof(q_tc_mapping->tc[0]) *
		(vf->qos_cap->num_elem - 1);
	q_tc_mapping = rte_zmalloc("q_tc", size, 0);
	if (!q_tc_mapping) {
		ret_val = IAVF_ERR_NO_MEMORY;
		goto fail_clear;
	}

	q_tc_mapping->vsi_id = vf->vsi.vsi_id;
	q_tc_mapping->num_tc = vf->qos_cap->num_elem;
	q_tc_mapping->num_queue_pairs = vf->num_queue_pairs;

	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->tc >= q_tc_mapping->num_tc) {
			PMD_DRV_LOG(ERR, "TC%d is not enabled", tm_node->tc);
			ret_val = IAVF_ERR_PARAM;
			goto fail_clear;
		}
		q_tc_mapping->tc[tm_node->tc].req.queue_count++;
		node_committed++;
	}

	/* All queues allocated to this VF should be mapped */
	if (node_committed < vf->num_queue_pairs) {
		PMD_DRV_LOG(ERR, "queue node is less than allocated queue pairs");
		ret_val = IAVF_ERR_PARAM;
		goto fail_clear;
	}

	/* store the queue TC mapping info */
	qtc_map = rte_zmalloc("qtc_map",
		sizeof(struct iavf_qtc_map) * q_tc_mapping->num_tc, 0);
	if (!qtc_map)
		return IAVF_ERR_NO_MEMORY;

	for (i = 0; i < q_tc_mapping->num_tc; i++) {
		q_tc_mapping->tc[i].req.start_queue_id = index;
		index += q_tc_mapping->tc[i].req.queue_count;
		qtc_map[i].tc = i;
		qtc_map[i].start_queue_id =
			q_tc_mapping->tc[i].req.start_queue_id;
		qtc_map[i].queue_count = q_tc_mapping->tc[i].req.queue_count;
	}

	ret_val = iavf_set_q_tc_map(dev, q_tc_mapping, size);
	if (ret_val)
		goto fail_clear;

	vf->qtc_map = qtc_map;
	vf->tm_conf.committed = true;
	return ret_val;

fail_clear:
	/* clear all the traffic manager configuration */
	if (clear_on_fail) {
		iavf_tm_conf_uninit(dev);
		iavf_tm_conf_init(dev);
	}
err:
	return ret_val;
}
