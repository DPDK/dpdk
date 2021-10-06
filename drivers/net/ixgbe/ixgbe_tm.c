/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <rte_malloc.h>

#include "ixgbe_ethdev.h"

static int ixgbe_tm_capabilities_get(struct rte_eth_dev *dev,
				     struct rte_tm_capabilities *cap,
				     struct rte_tm_error *error);
static int ixgbe_shaper_profile_add(struct rte_eth_dev *dev,
				    uint32_t shaper_profile_id,
				    struct rte_tm_shaper_params *profile,
				    struct rte_tm_error *error);
static int ixgbe_shaper_profile_del(struct rte_eth_dev *dev,
				    uint32_t shaper_profile_id,
				    struct rte_tm_error *error);
static int ixgbe_node_add(struct rte_eth_dev *dev, uint32_t node_id,
			  uint32_t parent_node_id, uint32_t priority,
			  uint32_t weight, uint32_t level_id,
			  struct rte_tm_node_params *params,
			  struct rte_tm_error *error);
static int ixgbe_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
			     struct rte_tm_error *error);
static int ixgbe_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
			       int *is_leaf, struct rte_tm_error *error);
static int ixgbe_level_capabilities_get(struct rte_eth_dev *dev,
					uint32_t level_id,
					struct rte_tm_level_capabilities *cap,
					struct rte_tm_error *error);
static int ixgbe_node_capabilities_get(struct rte_eth_dev *dev,
				       uint32_t node_id,
				       struct rte_tm_node_capabilities *cap,
				       struct rte_tm_error *error);
static int ixgbe_hierarchy_commit(struct rte_eth_dev *dev,
				  int clear_on_fail,
				  struct rte_tm_error *error);

const struct rte_tm_ops ixgbe_tm_ops = {
	.capabilities_get = ixgbe_tm_capabilities_get,
	.shaper_profile_add = ixgbe_shaper_profile_add,
	.shaper_profile_delete = ixgbe_shaper_profile_del,
	.node_add = ixgbe_node_add,
	.node_delete = ixgbe_node_delete,
	.node_type_get = ixgbe_node_type_get,
	.level_capabilities_get = ixgbe_level_capabilities_get,
	.node_capabilities_get = ixgbe_node_capabilities_get,
	.hierarchy_commit = ixgbe_hierarchy_commit,
};

int
ixgbe_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		 void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &ixgbe_tm_ops;

	return 0;
}

void
ixgbe_tm_conf_init(struct rte_eth_dev *dev)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);

	/* initialize shaper profile list */
	TAILQ_INIT(&tm_conf->shaper_profile_list);

	/* initialize node configuration */
	tm_conf->root = NULL;
	TAILQ_INIT(&tm_conf->queue_list);
	TAILQ_INIT(&tm_conf->tc_list);
	tm_conf->nb_tc_node = 0;
	tm_conf->nb_queue_node = 0;
	tm_conf->committed = false;
}

void
ixgbe_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_shaper_profile *shaper_profile;
	struct ixgbe_tm_node *tm_node;

	/* clear node configuration */
	while ((tm_node = TAILQ_FIRST(&tm_conf->queue_list))) {
		TAILQ_REMOVE(&tm_conf->queue_list, tm_node, node);
		rte_free(tm_node);
	}
	tm_conf->nb_queue_node = 0;
	while ((tm_node = TAILQ_FIRST(&tm_conf->tc_list))) {
		TAILQ_REMOVE(&tm_conf->tc_list, tm_node, node);
		rte_free(tm_node);
	}
	tm_conf->nb_tc_node = 0;
	if (tm_conf->root) {
		rte_free(tm_conf->root);
		tm_conf->root = NULL;
	}

	/* Remove all shaper profiles */
	while ((shaper_profile =
	       TAILQ_FIRST(&tm_conf->shaper_profile_list))) {
		TAILQ_REMOVE(&tm_conf->shaper_profile_list,
			     shaper_profile, node);
		rte_free(shaper_profile);
	}
}

static inline uint8_t
ixgbe_tc_nb_get(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *eth_conf;
	uint8_t nb_tcs = 0;

	eth_conf = &dev->data->dev_conf;
	if (eth_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
		nb_tcs = eth_conf->tx_adv_conf.dcb_tx_conf.nb_tcs;
	} else if (eth_conf->txmode.mq_mode == ETH_MQ_TX_VMDQ_DCB) {
		if (eth_conf->tx_adv_conf.vmdq_dcb_tx_conf.nb_queue_pools ==
		    ETH_32_POOLS)
			nb_tcs = ETH_4_TCS;
		else
			nb_tcs = ETH_8_TCS;
	} else {
		nb_tcs = 1;
	}

	return nb_tcs;
}

static int
ixgbe_tm_capabilities_get(struct rte_eth_dev *dev,
			  struct rte_tm_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint8_t tc_nb = ixgbe_tc_nb_get(dev);

	if (!cap || !error)
		return -EINVAL;

	if (tc_nb > hw->mac.max_tx_queues)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	/**
	 * here is the max capability not the current configuration.
	 */
	/* port + TCs + queues */
	cap->n_nodes_max = 1 + IXGBE_DCB_MAX_TRAFFIC_CLASS +
			   hw->mac.max_tx_queues;
	cap->n_levels_max = 3;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->n_nodes_max;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 0;
	/* 10Gbps -> 1.25GBps */
	cap->shaper_private_rate_max = 1250000000ull;
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
	cap->sched_n_children_max = hw->mac.max_tx_queues;
	/**
	 * HW supports SP. But no plan to support it now.
	 * So, all the nodes should have the same priority.
	 */
	cap->sched_sp_n_priorities_max = 1;
	cap->sched_wfq_n_children_per_group_max = 0;
	cap->sched_wfq_n_groups_max = 0;
	cap->sched_wfq_packet_mode_supported = 0;
	cap->sched_wfq_byte_mode_supported = 0;
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

static inline struct ixgbe_tm_shaper_profile *
ixgbe_shaper_profile_search(struct rte_eth_dev *dev,
			    uint32_t shaper_profile_id)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_shaper_profile_list *shaper_profile_list =
		&tm_conf->shaper_profile_list;
	struct ixgbe_tm_shaper_profile *shaper_profile;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (shaper_profile_id == shaper_profile->shaper_profile_id)
			return shaper_profile;
	}

	return NULL;
}

static int
ixgbe_shaper_profile_param_check(struct rte_tm_shaper_params *profile,
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
ixgbe_shaper_profile_add(struct rte_eth_dev *dev,
			 uint32_t shaper_profile_id,
			 struct rte_tm_shaper_params *profile,
			 struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_shaper_profile *shaper_profile;
	int ret;

	if (!profile || !error)
		return -EINVAL;

	ret = ixgbe_shaper_profile_param_check(profile, error);
	if (ret)
		return ret;

	shaper_profile = ixgbe_shaper_profile_search(dev, shaper_profile_id);

	if (shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		return -EINVAL;
	}

	shaper_profile = rte_zmalloc("ixgbe_tm_shaper_profile",
				     sizeof(struct ixgbe_tm_shaper_profile),
				     0);
	if (!shaper_profile)
		return -ENOMEM;
	shaper_profile->shaper_profile_id = shaper_profile_id;
	rte_memcpy(&shaper_profile->profile, profile,
			 sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&tm_conf->shaper_profile_list,
			  shaper_profile, node);

	return 0;
}

static int
ixgbe_shaper_profile_del(struct rte_eth_dev *dev,
			 uint32_t shaper_profile_id,
			 struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_shaper_profile *shaper_profile;

	if (!error)
		return -EINVAL;

	shaper_profile = ixgbe_shaper_profile_search(dev, shaper_profile_id);

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

	TAILQ_REMOVE(&tm_conf->shaper_profile_list, shaper_profile, node);
	rte_free(shaper_profile);

	return 0;
}

static inline struct ixgbe_tm_node *
ixgbe_tm_node_search(struct rte_eth_dev *dev, uint32_t node_id,
		     enum ixgbe_tm_node_type *node_type)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_node *tm_node;

	if (tm_conf->root && tm_conf->root->id == node_id) {
		*node_type = IXGBE_TM_NODE_TYPE_PORT;
		return tm_conf->root;
	}

	TAILQ_FOREACH(tm_node, &tm_conf->tc_list, node) {
		if (tm_node->id == node_id) {
			*node_type = IXGBE_TM_NODE_TYPE_TC;
			return tm_node;
		}
	}

	TAILQ_FOREACH(tm_node, &tm_conf->queue_list, node) {
		if (tm_node->id == node_id) {
			*node_type = IXGBE_TM_NODE_TYPE_QUEUE;
			return tm_node;
		}
	}

	return NULL;
}

static void
ixgbe_queue_base_nb_get(struct rte_eth_dev *dev, uint16_t tc_node_no,
			uint16_t *base, uint16_t *nb)
{
	uint8_t nb_tcs = ixgbe_tc_nb_get(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	uint16_t vf_num = pci_dev->max_vfs;

	*base = 0;
	*nb = 0;

	/* VT on */
	if (vf_num) {
		/* no DCB */
		if (nb_tcs == 1) {
			if (vf_num >= ETH_32_POOLS) {
				*nb = 2;
				*base = vf_num * 2;
			} else if (vf_num >= ETH_16_POOLS) {
				*nb = 4;
				*base = vf_num * 4;
			} else {
				*nb = 8;
				*base = vf_num * 8;
			}
		} else {
			/* DCB */
			*nb = 1;
			*base = vf_num * nb_tcs + tc_node_no;
		}
	} else {
		/* VT off */
		if (nb_tcs == ETH_8_TCS) {
			switch (tc_node_no) {
			case 0:
				*base = 0;
				*nb = 32;
				break;
			case 1:
				*base = 32;
				*nb = 32;
				break;
			case 2:
				*base = 64;
				*nb = 16;
				break;
			case 3:
				*base = 80;
				*nb = 16;
				break;
			case 4:
				*base = 96;
				*nb = 8;
				break;
			case 5:
				*base = 104;
				*nb = 8;
				break;
			case 6:
				*base = 112;
				*nb = 8;
				break;
			case 7:
				*base = 120;
				*nb = 8;
				break;
			default:
				return;
			}
		} else {
			switch (tc_node_no) {
			/**
			 * If no VF and no DCB, only 64 queues can be used.
			 * This case also be covered by this "case 0".
			 */
			case 0:
				*base = 0;
				*nb = 64;
				break;
			case 1:
				*base = 64;
				*nb = 32;
				break;
			case 2:
				*base = 96;
				*nb = 16;
				break;
			case 3:
				*base = 112;
				*nb = 16;
				break;
			default:
				return;
			}
		}
	}
}

static int
ixgbe_node_param_check(struct rte_eth_dev *dev, uint32_t node_id,
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

	/* for non-leaf node */
	if (node_id >= dev->data->nb_tx_queues) {
		/* check the unsupported parameters */
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
	/* check the unsupported parameters */
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
ixgbe_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	       uint32_t parent_node_id, uint32_t priority,
	       uint32_t weight, uint32_t level_id,
	       struct rte_tm_node_params *params,
	       struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	enum ixgbe_tm_node_type node_type = IXGBE_TM_NODE_TYPE_MAX;
	enum ixgbe_tm_node_type parent_node_type = IXGBE_TM_NODE_TYPE_MAX;
	struct ixgbe_tm_shaper_profile *shaper_profile = NULL;
	struct ixgbe_tm_node *tm_node;
	struct ixgbe_tm_node *parent_node;
	uint8_t nb_tcs;
	uint16_t q_base = 0;
	uint16_t q_nb = 0;
	int ret;

	if (!params || !error)
		return -EINVAL;

	/* if already committed */
	if (tm_conf->committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	ret = ixgbe_node_param_check(dev, node_id, priority, weight,
				     params, error);
	if (ret)
		return ret;

	/* check if the node ID is already used */
	if (ixgbe_tm_node_search(dev, node_id, &node_type)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		return -EINVAL;
	}

	/* check the shaper profile id */
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		shaper_profile = ixgbe_shaper_profile_search(
					dev, params->shaper_profile_id);
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
		if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
		    level_id > IXGBE_TM_NODE_TYPE_PORT) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "Wrong level";
			return -EINVAL;
		}

		/* obviously no more than one root */
		if (tm_conf->root) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "already have a root";
			return -EINVAL;
		}

		/* add the root node */
		tm_node = rte_zmalloc("ixgbe_tm_node",
				      sizeof(struct ixgbe_tm_node),
				      0);
		if (!tm_node)
			return -ENOMEM;
		tm_node->id = node_id;
		tm_node->priority = priority;
		tm_node->weight = weight;
		tm_node->reference_count = 0;
		tm_node->no = 0;
		tm_node->parent = NULL;
		tm_node->shaper_profile = shaper_profile;
		rte_memcpy(&tm_node->params, params,
				 sizeof(struct rte_tm_node_params));
		tm_conf->root = tm_node;

		/* increase the reference counter of the shaper profile */
		if (shaper_profile)
			shaper_profile->reference_count++;

		return 0;
	}

	/* TC or queue node */
	/* check the parent node */
	parent_node = ixgbe_tm_node_search(dev, parent_node_id,
					   &parent_node_type);
	if (!parent_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}
	if (parent_node_type != IXGBE_TM_NODE_TYPE_PORT &&
	    parent_node_type != IXGBE_TM_NODE_TYPE_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not port or TC";
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
	if (parent_node_type == IXGBE_TM_NODE_TYPE_PORT) {
		/* check TC number */
		nb_tcs = ixgbe_tc_nb_get(dev);
		if (tm_conf->nb_tc_node >= nb_tcs) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many TCs";
			return -EINVAL;
		}
	} else {
		/* check queue number */
		if (tm_conf->nb_queue_node >= dev->data->nb_tx_queues) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queues";
			return -EINVAL;
		}

		ixgbe_queue_base_nb_get(dev, parent_node->no, &q_base, &q_nb);
		if (parent_node->reference_count >= q_nb) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too many queues than TC supported";
			return -EINVAL;
		}

		/**
		 * check the node id.
		 * For queue, the node id means queue id.
		 */
		if (node_id >= dev->data->nb_tx_queues) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "too large queue id";
			return -EINVAL;
		}
	}

	/* add the TC or queue node */
	tm_node = rte_zmalloc("ixgbe_tm_node",
			      sizeof(struct ixgbe_tm_node),
			      0);
	if (!tm_node)
		return -ENOMEM;
	tm_node->id = node_id;
	tm_node->priority = priority;
	tm_node->weight = weight;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	tm_node->shaper_profile = shaper_profile;
	rte_memcpy(&tm_node->params, params,
			 sizeof(struct rte_tm_node_params));
	if (parent_node_type == IXGBE_TM_NODE_TYPE_PORT) {
		tm_node->no = parent_node->reference_count;
		TAILQ_INSERT_TAIL(&tm_conf->tc_list,
				  tm_node, node);
		tm_conf->nb_tc_node++;
	} else {
		tm_node->no = q_base + parent_node->reference_count;
		TAILQ_INSERT_TAIL(&tm_conf->queue_list,
				  tm_node, node);
		tm_conf->nb_queue_node++;
	}
	tm_node->parent->reference_count++;

	/* increase the reference counter of the shaper profile */
	if (shaper_profile)
		shaper_profile->reference_count++;

	return 0;
}

static int
ixgbe_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		  struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	enum ixgbe_tm_node_type node_type = IXGBE_TM_NODE_TYPE_MAX;
	struct ixgbe_tm_node *tm_node;

	if (!error)
		return -EINVAL;

	/* if already committed */
	if (tm_conf->committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check the if the node id exists */
	tm_node = ixgbe_tm_node_search(dev, node_id, &node_type);
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
	if (node_type == IXGBE_TM_NODE_TYPE_PORT) {
		if (tm_node->shaper_profile)
			tm_node->shaper_profile->reference_count--;
		rte_free(tm_node);
		tm_conf->root = NULL;
		return 0;
	}

	/* TC or queue node */
	if (tm_node->shaper_profile)
		tm_node->shaper_profile->reference_count--;
	tm_node->parent->reference_count--;
	if (node_type == IXGBE_TM_NODE_TYPE_TC) {
		TAILQ_REMOVE(&tm_conf->tc_list, tm_node, node);
		tm_conf->nb_tc_node--;
	} else {
		TAILQ_REMOVE(&tm_conf->queue_list, tm_node, node);
		tm_conf->nb_queue_node--;
	}
	rte_free(tm_node);

	return 0;
}

static int
ixgbe_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		    int *is_leaf, struct rte_tm_error *error)
{
	enum ixgbe_tm_node_type node_type = IXGBE_TM_NODE_TYPE_MAX;
	struct ixgbe_tm_node *tm_node;

	if (!is_leaf || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = ixgbe_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (node_type == IXGBE_TM_NODE_TYPE_QUEUE)
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static int
ixgbe_level_capabilities_get(struct rte_eth_dev *dev,
			     uint32_t level_id,
			     struct rte_tm_level_capabilities *cap,
			     struct rte_tm_error *error)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!cap || !error)
		return -EINVAL;

	if (level_id >= IXGBE_TM_NODE_TYPE_MAX) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		return -EINVAL;
	}

	/* root node */
	if (level_id == IXGBE_TM_NODE_TYPE_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->n_nodes_leaf_max = 0;
	} else if (level_id == IXGBE_TM_NODE_TYPE_TC) {
		/* TC */
		cap->n_nodes_max = IXGBE_DCB_MAX_TRAFFIC_CLASS;
		cap->n_nodes_nonleaf_max = IXGBE_DCB_MAX_TRAFFIC_CLASS;
		cap->n_nodes_leaf_max = 0;
	} else {
		/* queue */
		cap->n_nodes_max = hw->mac.max_tx_queues;
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = hw->mac.max_tx_queues;
	}

	cap->non_leaf_nodes_identical = true;
	cap->leaf_nodes_identical = true;

	if (level_id != IXGBE_TM_NODE_TYPE_QUEUE) {
		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = false;
		cap->nonleaf.shaper_private_rate_min = 0;
		/* 10Gbps -> 1.25GBps */
		cap->nonleaf.shaper_private_rate_max = 1250000000ull;
		cap->nonleaf.shaper_private_packet_mode_supported = 0;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;
		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.shaper_shared_packet_mode_supported = 0;
		cap->nonleaf.shaper_shared_byte_mode_supported = 0;
		if (level_id == IXGBE_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				IXGBE_DCB_MAX_TRAFFIC_CLASS;
		else
			cap->nonleaf.sched_n_children_max =
				hw->mac.max_tx_queues;
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
	cap->leaf.shaper_private_supported = true;
	cap->leaf.shaper_private_dual_rate_supported = false;
	cap->leaf.shaper_private_rate_min = 0;
	/* 10Gbps -> 1.25GBps */
	cap->leaf.shaper_private_rate_max = 1250000000ull;
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
ixgbe_node_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t node_id,
			    struct rte_tm_node_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	enum ixgbe_tm_node_type node_type = IXGBE_TM_NODE_TYPE_MAX;
	struct ixgbe_tm_node *tm_node;

	if (!cap || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = ixgbe_tm_node_search(dev, node_id, &node_type);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported = false;
	cap->shaper_private_rate_min = 0;
	/* 10Gbps -> 1.25GBps */
	cap->shaper_private_rate_max = 1250000000ull;
	cap->shaper_private_packet_mode_supported = 0;
	cap->shaper_private_byte_mode_supported = 1;
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_packet_mode_supported = 0;
	cap->shaper_shared_byte_mode_supported = 0;

	if (node_type == IXGBE_TM_NODE_TYPE_QUEUE) {
		cap->leaf.cman_head_drop_supported = false;
		cap->leaf.cman_wred_context_private_supported = true;
		cap->leaf.cman_wred_context_shared_n_max = 0;
	} else {
		if (node_type == IXGBE_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				IXGBE_DCB_MAX_TRAFFIC_CLASS;
		else
			cap->nonleaf.sched_n_children_max =
				hw->mac.max_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;
	}

	cap->stats_mask = 0;

	return 0;
}

static int
ixgbe_hierarchy_commit(struct rte_eth_dev *dev,
		       int clear_on_fail,
		       struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_node *tm_node;
	uint64_t bw;
	int ret;

	if (!error)
		return -EINVAL;

	/* check the setting */
	if (!tm_conf->root)
		goto done;

	/* not support port max bandwidth yet */
	if (tm_conf->root->shaper_profile &&
	    tm_conf->root->shaper_profile->profile.peak.rate) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "no port max bandwidth";
		goto fail_clear;
	}

	/* HW not support TC max bandwidth */
	TAILQ_FOREACH(tm_node, &tm_conf->tc_list, node) {
		if (tm_node->shaper_profile &&
		    tm_node->shaper_profile->profile.peak.rate) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
			error->message = "no TC max bandwidth";
			goto fail_clear;
		}
	}

	/* queue max bandwidth */
	TAILQ_FOREACH(tm_node, &tm_conf->queue_list, node) {
		if (tm_node->shaper_profile)
			bw = tm_node->shaper_profile->profile.peak.rate;
		else
			bw = 0;
		if (bw) {
			/* interpret Bps to Mbps */
			bw = bw * 8 / 1000 / 1000;
			ret = ixgbe_set_queue_rate_limit(dev, tm_node->no, bw);
			if (ret) {
				error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
				error->message =
					"failed to set queue max bandwidth";
				goto fail_clear;
			}
		}
	}

done:
	tm_conf->committed = true;
	return 0;

fail_clear:
	/* clear all the traffic manager configuration */
	if (clear_on_fail) {
		ixgbe_tm_conf_uninit(dev);
		ixgbe_tm_conf_init(dev);
	}
	return -EINVAL;
}
