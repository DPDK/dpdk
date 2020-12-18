/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <rte_malloc.h>

#include "txgbe_ethdev.h"

static int txgbe_tm_capabilities_get(struct rte_eth_dev *dev,
				     struct rte_tm_capabilities *cap,
				     struct rte_tm_error *error);
static int txgbe_level_capabilities_get(struct rte_eth_dev *dev,
					uint32_t level_id,
					struct rte_tm_level_capabilities *cap,
					struct rte_tm_error *error);
static int txgbe_node_capabilities_get(struct rte_eth_dev *dev,
				       uint32_t node_id,
				       struct rte_tm_node_capabilities *cap,
				       struct rte_tm_error *error);

const struct rte_tm_ops txgbe_tm_ops = {
	.capabilities_get = txgbe_tm_capabilities_get,
	.level_capabilities_get = txgbe_level_capabilities_get,
	.node_capabilities_get = txgbe_node_capabilities_get,
};

int
txgbe_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		 void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &txgbe_tm_ops;

	return 0;
}

void
txgbe_tm_conf_init(struct rte_eth_dev *dev)
{
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);

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
txgbe_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);
	struct txgbe_tm_shaper_profile *shaper_profile;
	struct txgbe_tm_node *tm_node;

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
txgbe_tc_nb_get(struct rte_eth_dev *dev)
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
txgbe_tm_capabilities_get(struct rte_eth_dev *dev,
			  struct rte_tm_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint8_t tc_nb = txgbe_tc_nb_get(dev);

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
	cap->n_nodes_max = 1 + TXGBE_DCB_TC_MAX +
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
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->sched_n_children_max = hw->mac.max_tx_queues;
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

static inline struct txgbe_tm_node *
txgbe_tm_node_search(struct rte_eth_dev *dev, uint32_t node_id,
		     enum txgbe_tm_node_type *node_type)
{
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);
	struct txgbe_tm_node *tm_node;

	if (tm_conf->root && tm_conf->root->id == node_id) {
		*node_type = TXGBE_TM_NODE_TYPE_PORT;
		return tm_conf->root;
	}

	TAILQ_FOREACH(tm_node, &tm_conf->tc_list, node) {
		if (tm_node->id == node_id) {
			*node_type = TXGBE_TM_NODE_TYPE_TC;
			return tm_node;
		}
	}

	TAILQ_FOREACH(tm_node, &tm_conf->queue_list, node) {
		if (tm_node->id == node_id) {
			*node_type = TXGBE_TM_NODE_TYPE_QUEUE;
			return tm_node;
		}
	}

	return NULL;
}

static int
txgbe_level_capabilities_get(struct rte_eth_dev *dev,
			     uint32_t level_id,
			     struct rte_tm_level_capabilities *cap,
			     struct rte_tm_error *error)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (!cap || !error)
		return -EINVAL;

	if (level_id >= TXGBE_TM_NODE_TYPE_MAX) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		return -EINVAL;
	}

	/* root node */
	if (level_id == TXGBE_TM_NODE_TYPE_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->n_nodes_leaf_max = 0;
	} else if (level_id == TXGBE_TM_NODE_TYPE_TC) {
		/* TC */
		cap->n_nodes_max = TXGBE_DCB_TC_MAX;
		cap->n_nodes_nonleaf_max = TXGBE_DCB_TC_MAX;
		cap->n_nodes_leaf_max = 0;
	} else {
		/* queue */
		cap->n_nodes_max = hw->mac.max_tx_queues;
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = hw->mac.max_tx_queues;
	}

	cap->non_leaf_nodes_identical = true;
	cap->leaf_nodes_identical = true;

	if (level_id != TXGBE_TM_NODE_TYPE_QUEUE) {
		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = false;
		cap->nonleaf.shaper_private_rate_min = 0;
		/* 10Gbps -> 1.25GBps */
		cap->nonleaf.shaper_private_rate_max = 1250000000ull;
		cap->nonleaf.shaper_shared_n_max = 0;
		if (level_id == TXGBE_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				TXGBE_DCB_TC_MAX;
		else
			cap->nonleaf.sched_n_children_max =
				hw->mac.max_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.stats_mask = 0;

		return 0;
	}

	/* queue node */
	cap->leaf.shaper_private_supported = true;
	cap->leaf.shaper_private_dual_rate_supported = false;
	cap->leaf.shaper_private_rate_min = 0;
	/* 10Gbps -> 1.25GBps */
	cap->leaf.shaper_private_rate_max = 1250000000ull;
	cap->leaf.shaper_shared_n_max = 0;
	cap->leaf.cman_head_drop_supported = false;
	cap->leaf.cman_wred_context_private_supported = true;
	cap->leaf.cman_wred_context_shared_n_max = 0;
	cap->leaf.stats_mask = 0;

	return 0;
}

static int
txgbe_node_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t node_id,
			    struct rte_tm_node_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	enum txgbe_tm_node_type node_type = TXGBE_TM_NODE_TYPE_MAX;
	struct txgbe_tm_node *tm_node;

	if (!cap || !error)
		return -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	/* check if the node id exists */
	tm_node = txgbe_tm_node_search(dev, node_id, &node_type);
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
	cap->shaper_shared_n_max = 0;

	if (node_type == TXGBE_TM_NODE_TYPE_QUEUE) {
		cap->leaf.cman_head_drop_supported = false;
		cap->leaf.cman_wred_context_private_supported = true;
		cap->leaf.cman_wred_context_shared_n_max = 0;
	} else {
		if (node_type == TXGBE_TM_NODE_TYPE_PORT)
			cap->nonleaf.sched_n_children_max =
				TXGBE_DCB_TC_MAX;
		else
			cap->nonleaf.sched_n_children_max =
				hw->mac.max_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 1;
	}

	cap->stats_mask = 0;

	return 0;
}

