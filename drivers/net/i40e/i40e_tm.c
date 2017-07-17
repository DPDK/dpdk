/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_malloc.h>

#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"

static int i40e_tm_capabilities_get(struct rte_eth_dev *dev,
				    struct rte_tm_capabilities *cap,
				    struct rte_tm_error *error);
static int i40e_shaper_profile_add(struct rte_eth_dev *dev,
				   uint32_t shaper_profile_id,
				   struct rte_tm_shaper_params *profile,
				   struct rte_tm_error *error);
static int i40e_shaper_profile_del(struct rte_eth_dev *dev,
				   uint32_t shaper_profile_id,
				   struct rte_tm_error *error);
static int i40e_node_add(struct rte_eth_dev *dev, uint32_t node_id,
			 uint32_t parent_node_id, uint32_t priority,
			 uint32_t weight, uint32_t level_id,
			 struct rte_tm_node_params *params,
			 struct rte_tm_error *error);
static int i40e_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
			    struct rte_tm_error *error);
static int i40e_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
			      int *is_leaf, struct rte_tm_error *error);
static int i40e_level_capabilities_get(struct rte_eth_dev *dev,
				       uint32_t level_id,
				       struct rte_tm_level_capabilities *cap,
				       struct rte_tm_error *error);
static int i40e_node_capabilities_get(struct rte_eth_dev *dev,
				      uint32_t node_id,
				      struct rte_tm_node_capabilities *cap,
				      struct rte_tm_error *error);
static int i40e_hierarchy_commit(struct rte_eth_dev *dev,
				 int clear_on_fail,
				 struct rte_tm_error *error);

const struct rte_tm_ops i40e_tm_ops = {
	.capabilities_get = i40e_tm_capabilities_get,
	.shaper_profile_add = i40e_shaper_profile_add,
	.shaper_profile_delete = i40e_shaper_profile_del,
	.node_add = i40e_node_add,
	.node_delete = i40e_node_delete,
	.node_type_get = i40e_node_type_get,
	.level_capabilities_get = i40e_level_capabilities_get,
	.node_capabilities_get = i40e_node_capabilities_get,
	.hierarchy_commit = i40e_hierarchy_commit,
};

int
i40e_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &i40e_tm_ops;

	return 0;
}

void
i40e_tm_conf_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	/* initialize shaper profile list */
	TAILQ_INIT(&pf->tm_conf.shaper_profile_list);

	/* initialize node configuration */
	pf->tm_conf.root = NULL;
	TAILQ_INIT(&pf->tm_conf.tc_list);
	TAILQ_INIT(&pf->tm_conf.queue_list);
	pf->tm_conf.nb_tc_node = 0;
	pf->tm_conf.nb_queue_node = 0;
	pf->tm_conf.committed = false;
}

void
i40e_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tm_shaper_profile *shaper_profile;
	struct i40e_tm_node *tm_node;

	/* clear node configuration */
	while ((tm_node = TAILQ_FIRST(&pf->tm_conf.queue_list))) {
		TAILQ_REMOVE(&pf->tm_conf.queue_list, tm_node, node);
		rte_free(tm_node);
	}
	pf->tm_conf.nb_queue_node = 0;
	while ((tm_node = TAILQ_FIRST(&pf->tm_conf.tc_list))) {
		TAILQ_REMOVE(&pf->tm_conf.tc_list, tm_node, node);
		rte_free(tm_node);
	}
	pf->tm_conf.nb_tc_node = 0;
	if (pf->tm_conf.root) {
		rte_free(pf->tm_conf.root);
		pf->tm_conf.root = NULL;
	}

	/* Remove all shaper profiles */
	while ((shaper_profile =
	       TAILQ_FIRST(&pf->tm_conf.shaper_profile_list))) {
		TAILQ_REMOVE(&pf->tm_conf.shaper_profile_list,
			     shaper_profile, node);
		rte_free(shaper_profile);
	}
}

static inline uint16_t
i40e_tc_nb_get(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	uint16_t sum = 0;
	int i;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (main_vsi->enabled_tc & BIT_ULL(i))
			sum++;
	}

	return sum;
}

static int
i40e_tm_capabilities_get(struct rte_eth_dev *dev,
			 struct rte_tm_capabilities *cap,
			 struct rte_tm_error *error)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t tc_nb = i40e_tc_nb_get(dev);

	if (!cap || !error)
		return -EINVAL;

	if (tc_nb > hw->func_caps.num_tx_qp)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	/**
	 * support port + TCs + queues
	 * here shows the max capability not the current configuration.
	 */
	cap->n_nodes_max = 1 + I40E_MAX_TRAFFIC_CLASS + hw->func_caps.num_tx_qp;
	cap->n_levels_max = 3; /* port, TC, queue */
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->n_nodes_max;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 0;
	/* 40Gbps -> 5GBps */
	cap->shaper_private_rate_max = 5000000000ull;
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->sched_n_children_max = hw->func_caps.num_tx_qp;
	/**
	 * HW supports SP. But no plan to support it now.
	 * So, all the nodes should have the same priority.
	 */
	cap->sched_sp_n_priorities_max = 1;
	cap->sched_wfq_n_children_per_group_max = 0;
	cap->sched_wfq_n_groups_max = 0;
	/**
	 * SW only supports fair round robin now.
	 * So, all the nodes should have the same weight.
	 */
	cap->sched_wfq_weight_max = 1;
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

static inline struct i40e_tm_shaper_profile *
i40e_shaper_profile_search(struct rte_eth_dev *dev,
			   uint32_t shaper_profile_id)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_shaper_profile_list *shaper_profile_list =
		&pf->tm_conf.shaper_profile_list;
	struct i40e_tm_shaper_profile *shaper_profile;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (shaper_profile_id == shaper_profile->shaper_profile_id)
			return shaper_profile;
	}

	return NULL;
}

static int
i40e_shaper_profile_param_check(struct rte_tm_shaper_params *profile,
				struct rte_tm_error *error)
{
	/* min rate not supported */
	if (profile->committed.rate) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		error->message = "committed rate not supported";
		return -EINVAL;
	}
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
i40e_shaper_profile_add(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			struct rte_tm_shaper_params *profile,
			struct rte_tm_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tm_shaper_profile *shaper_profile;
	int ret;

	if (!profile || !error)
		return -EINVAL;

	ret = i40e_shaper_profile_param_check(profile, error);
	if (ret)
		return ret;

	shaper_profile = i40e_shaper_profile_search(dev, shaper_profile_id);

	if (shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		return -EINVAL;
	}

	shaper_profile = rte_zmalloc("i40e_tm_shaper_profile",
				     sizeof(struct i40e_tm_shaper_profile),
				     0);
	if (!shaper_profile)
		return -ENOMEM;
	shaper_profile->shaper_profile_id = shaper_profile_id;
	(void)rte_memcpy(&shaper_profile->profile, profile,
			 sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&pf->tm_conf.shaper_profile_list,
			  shaper_profile, node);

	return 0;
}

static int
i40e_shaper_profile_del(struct rte_eth_dev *dev,
			uint32_t shaper_profile_id,
			struct rte_tm_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tm_shaper_profile *shaper_profile;

	if (!error)
		return -EINVAL;

	shaper_profile = i40e_shaper_profile_search(dev, shaper_profile_id);

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

static inline struct i40e_tm_node *
i40e_tm_node_search(struct rte_eth_dev *dev,
		    uint32_t node_id, enum i40e_tm_node_type *node_type)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tm_node_list *queue_list = &pf->tm_conf.queue_list;
	struct i40e_tm_node_list *tc_list = &pf->tm_conf.tc_list;
	struct i40e_tm_node *tm_node;

	if (pf->tm_conf.root && pf->tm_conf.root->id == node_id) {
		*node_type = I40E_TM_NODE_TYPE_PORT;
		return pf->tm_conf.root;
	}

	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (tm_node->id == node_id) {
			*node_type = I40E_TM_NODE_TYPE_TC;
			return tm_node;
		}
	}

	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->id == node_id) {
			*node_type = I40E_TM_NODE_TYPE_QUEUE;
			return tm_node;
		}
	}

	return NULL;
}

static int
i40e_node_param_check(uint32_t node_id, uint32_t parent_node_id,
		      uint32_t priority, uint32_t weight,
		      struct rte_tm_node_params *params,
		      struct rte_tm_error *error)
{
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

	/* for root node */
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
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

	/* for TC or queue node */
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

/**
 * Now the TC and queue configuration is controlled by DCB.
 * We need check if the node configuration follows the DCB configuration.
 * In the future, we may use TM to cover DCB.
 */
static int
i40e_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority,
	      uint32_t weight, uint32_t level_id,
	      struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum i40e_tm_node_type node_type = I40E_TM_NODE_TYPE_MAX;
	enum i40e_tm_node_type parent_node_type = I40E_TM_NODE_TYPE_MAX;
	struct i40e_tm_shaper_profile *shaper_profile;
	struct i40e_tm_node *tm_node;
	struct i40e_tm_node *parent_node;
	uint16_t tc_nb = 0;
	int ret;

	if (!params || !error)
		return -EINVAL;

	/* if already committed */
	if (pf->tm_conf.committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	ret = i40e_node_param_check(node_id, parent_node_id, priority, weight,
				    params, error);
	if (ret)
		return ret;

	/* check if the node ID is already used */
	if (i40e_tm_node_search(dev, node_id, &node_type)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		return -EINVAL;
	}

	/* check the shaper profile id */
	shaper_profile = i40e_shaper_profile_search(dev,
						    params->shaper_profile_id);
	if (!shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
		error->message = "shaper profile not exist";
		return -EINVAL;
	}

	/* root node if not have a parent */
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		/* check level */
		if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
		    level_id > I40E_TM_NODE_TYPE_PORT) {
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
		tm_node = rte_zmalloc("i40e_tm_node",
				      sizeof(struct i40e_tm_node),
				      0);
		if (!tm_node)
			return -ENOMEM;
		tm_node->id = node_id;
		tm_node->priority = priority;
		tm_node->weight = weight;
		tm_node->reference_count = 0;
		tm_node->parent = NULL;
		tm_node->shaper_profile = shaper_profile;
		(void)rte_memcpy(&tm_node->params, params,
				 sizeof(struct rte_tm_node_params));
		pf->tm_conf.root = tm_node;

		/* increase the reference counter of the shaper profile */
		shaper_profile->reference_count++;

		return 0;
	}

	/* TC or queue node */
	/* check the parent node */
	parent_node = i40e_tm_node_search(dev, parent_node_id,
					  &parent_node_type);
	if (!parent_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}
	if (parent_node_type != I40E_TM_NODE_TYPE_PORT &&
	    parent_node_type != I40E_TM_NODE_TYPE_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not port or TC";
		return -EINVAL;
	}
	/* check level */
	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != parent_node_type + 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "Wrong level";
		return -EINVAL;
	}

	/* check the node number */
	if (parent_node_type == I40E_TM_NODE_TYPE_PORT) {
		/* check the TC number */
		tc_nb = i40e_tc_nb_get(dev);
		if (pf->tm_conf.nb_tc_node >= tc_nb) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many TCs";
			return -EINVAL;
		}
	} else {
		/* check the queue number */
		if (pf->tm_conf.nb_queue_node >= hw->func_caps.num_tx_qp) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queues";
			return -EINVAL;
		}

		/**
		 * check the node id.
		 * For queue, the node id means queue id.
		 */
		if (node_id >= hw->func_caps.num_tx_qp) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too large queue id";
			return -EINVAL;
		}
	}

	/* add the TC or queue node */
	tm_node = rte_zmalloc("i40e_tm_node",
			      sizeof(struct i40e_tm_node),
			      0);
	if (!tm_node)
		return -ENOMEM;
	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->reference_count = 0;
	tm_node->parent = pf->tm_conf.root;
	tm_node->shaper_profile = shaper_profile;
	(void)rte_memcpy(&tm_node->params, params,
			 sizeof(struct rte_tm_node_params));
	if (parent_node_type == I40E_TM_NODE_TYPE_PORT) {
		TAILQ_INSERT_TAIL(&pf->tm_conf.tc_list,
				  tm_node, node);
		pf->tm_conf.nb_tc_node++;
	} else {
		TAILQ_INSERT_TAIL(&pf->tm_conf.queue_list,
				  tm_node, node);
		pf->tm_conf.nb_queue_node++;
	}
	tm_node->parent->reference_count++;

	/* increase the reference counter of the shaper profile */
	shaper_profile->reference_count++;

	return 0;
}

static int
i40e_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		 struct rte_tm_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum i40e_tm_node_type node_type = I40E_TM_NODE_TYPE_MAX;
	struct i40e_tm_node *tm_node;

	if (!error)
		return -EINVAL;

	/* if already committed */
	if (pf->tm_conf.committed) {
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
	tm_node = i40e_tm_node_search(dev, node_id, &node_type);
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
	if (node_type == I40E_TM_NODE_TYPE_PORT) {
		tm_node->shaper_profile->reference_count--;
		rte_free(tm_node);
		pf->tm_conf.root = NULL;
		return 0;
	}

	/* TC or queue node */
	tm_node->shaper_profile->reference_count--;
	tm_node->parent->reference_count--;
	if (node_type == I40E_TM_NODE_TYPE_TC) {
		TAILQ_REMOVE(&pf->tm_conf.tc_list, tm_node, node);
		pf->tm_conf.nb_tc_node--;
	} else {
		TAILQ_REMOVE(&pf->tm_conf.queue_list, tm_node, node);
		pf->tm_conf.nb_queue_node--;
	}
	rte_free(tm_node);

	return 0;
}

static int
i40e_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		   int *is_leaf, struct rte_tm_error *error)
{
	enum i40e_tm_node_type node_type = I40E_TM_NODE_TYPE_MAX;
	struct i40e_tm_node *tm_node;

	if (!is_leaf || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = i40e_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (node_type == I40E_TM_NODE_TYPE_QUEUE)
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static int
i40e_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!cap || !error)
		return -EINVAL;

	if (level_id >= I40E_TM_NODE_TYPE_MAX) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		return -EINVAL;
	}

	/* root node */
	if (level_id == I40E_TM_NODE_TYPE_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->n_nodes_leaf_max = 0;
		cap->non_leaf_nodes_identical = true;
		cap->leaf_nodes_identical = true;
		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = false;
		cap->nonleaf.shaper_private_rate_min = 0;
		/* 40Gbps -> 5GBps */
		cap->nonleaf.shaper_private_rate_max = 5000000000ull;
		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.sched_n_children_max = I40E_MAX_TRAFFIC_CLASS;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.stats_mask = 0;

		return 0;
	}

	/* TC or queue node */
	if (level_id == I40E_TM_NODE_TYPE_TC) {
		/* TC */
		cap->n_nodes_max = I40E_MAX_TRAFFIC_CLASS;
		cap->n_nodes_nonleaf_max = I40E_MAX_TRAFFIC_CLASS;
		cap->n_nodes_leaf_max = 0;
		cap->non_leaf_nodes_identical = true;
	} else {
		/* queue */
		cap->n_nodes_max = hw->func_caps.num_tx_qp;
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = hw->func_caps.num_tx_qp;
		cap->non_leaf_nodes_identical = true;
	}
	cap->leaf_nodes_identical = true;
	cap->leaf.shaper_private_supported = true;
	cap->leaf.shaper_private_dual_rate_supported = false;
	cap->leaf.shaper_private_rate_min = 0;
	/* 40Gbps -> 5GBps */
	cap->leaf.shaper_private_rate_max = 5000000000ull;
	cap->leaf.shaper_shared_n_max = 0;
	cap->leaf.cman_head_drop_supported = false;
	cap->leaf.cman_wred_context_private_supported = true;
	cap->leaf.cman_wred_context_shared_n_max = 0;
	cap->leaf.stats_mask = 0;

	return 0;
}

static int
i40e_node_capabilities_get(struct rte_eth_dev *dev,
			   uint32_t node_id,
			   struct rte_tm_node_capabilities *cap,
			   struct rte_tm_error *error)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	enum i40e_tm_node_type node_type;
	struct i40e_tm_node *tm_node;

	if (!cap || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = i40e_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported = false;
	cap->shaper_private_rate_min = 0;
	/* 40Gbps -> 5GBps */
	cap->shaper_private_rate_max = 5000000000ull;
	cap->shaper_shared_n_max = 0;

	if (node_type == I40E_TM_NODE_TYPE_QUEUE) {
		cap->leaf.cman_head_drop_supported = false;
		cap->leaf.cman_wred_context_private_supported = true;
		cap->leaf.cman_wred_context_shared_n_max = 0;
	} else {
		if (node_type == I40E_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				I40E_MAX_TRAFFIC_CLASS;
		else
			cap->nonleaf.sched_n_children_max =
				hw->func_caps.num_tx_qp;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
	}

	cap->stats_mask = 0;

	return 0;
}

static int
i40e_hierarchy_commit(struct rte_eth_dev *dev,
		      int clear_on_fail,
		      struct rte_tm_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tm_node_list *tc_list = &pf->tm_conf.tc_list;
	struct i40e_tm_node_list *queue_list = &pf->tm_conf.queue_list;
	struct i40e_tm_node *tm_node;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_aqc_configure_vsi_ets_sla_bw_data tc_bw;
	uint64_t bw;
	uint8_t tc_map;
	int ret;
	int i;

	if (!error)
		return -EINVAL;

	/* check the setting */
	if (!pf->tm_conf.root)
		goto done;

	vsi = pf->main_vsi;
	hw = I40E_VSI_TO_HW(vsi);

	/**
	 * Don't support bandwidth control for port and TCs in parallel.
	 * If the port has a max bandwidth, the TCs should have none.
	 */
	/* port */
	bw = pf->tm_conf.root->shaper_profile->profile.peak.rate;
	if (bw) {
		/* check if any TC has a max bandwidth */
		TAILQ_FOREACH(tm_node, tc_list, node) {
			if (tm_node->shaper_profile->profile.peak.rate) {
				error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
				error->message = "no port and TC max bandwidth"
						 " in parallel";
				goto fail_clear;
			}
		}

		/* interpret Bps to 50Mbps */
		bw = bw * 8 / 1000 / 1000 / I40E_QOS_BW_GRANULARITY;

		/* set the max bandwidth */
		ret = i40e_aq_config_vsi_bw_limit(hw, vsi->seid,
						  (uint16_t)bw, 0, NULL);
		if (ret) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
			error->message = "fail to set port max bandwidth";
			goto fail_clear;
		}

		goto done;
	}

	/* TC */
	memset(&tc_bw, 0, sizeof(tc_bw));
	tc_bw.tc_valid_bits = vsi->enabled_tc;
	tc_map = vsi->enabled_tc;
	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (!tm_node->reference_count) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "TC without queue assigned";
			goto fail_clear;
		}

		i = 0;
		while (i < I40E_MAX_TRAFFIC_CLASS && !(tc_map & BIT_ULL(i)))
			i++;
		if (i >= I40E_MAX_TRAFFIC_CLASS) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "cannot find the TC";
			goto fail_clear;
		}
		tc_map &= ~BIT_ULL(i);

		bw = tm_node->shaper_profile->profile.peak.rate;
		if (!bw)
			continue;

		/* interpret Bps to 50Mbps */
		bw = bw * 8 / 1000 / 1000 / I40E_QOS_BW_GRANULARITY;

		tc_bw.tc_bw_credits[i] = rte_cpu_to_le_16((uint16_t)bw);
	}

	TAILQ_FOREACH(tm_node, queue_list, node) {
		bw = tm_node->shaper_profile->profile.peak.rate;
		if (bw) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "not support queue QoS";
			goto fail_clear;
		}
	}

	ret = i40e_aq_config_vsi_ets_sla_bw_limit(hw, vsi->seid, &tc_bw, NULL);
	if (ret) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "fail to set TC max bandwidth";
		goto fail_clear;
	}

done:
	pf->tm_conf.committed = true;
	return 0;

fail_clear:
	/* clear all the traffic manager configuration */
	if (clear_on_fail) {
		i40e_tm_conf_uninit(dev);
		i40e_tm_conf_init(dev);
	}
	return -EINVAL;
}
