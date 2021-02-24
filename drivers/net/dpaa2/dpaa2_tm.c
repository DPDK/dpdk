/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 NXP
 */

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_tm_driver.h>

#include "dpaa2_ethdev.h"

#define DPAA2_BURST_MAX	(64 * 1024)

#define DPAA2_SHAPER_MIN_RATE 0
#define DPAA2_SHAPER_MAX_RATE 107374182400ull
#define DPAA2_WEIGHT_MAX 24701

int
dpaa2_tm_init(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	LIST_INIT(&priv->shaper_profiles);
	LIST_INIT(&priv->nodes);

	return 0;
}

void dpaa2_tm_deinit(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_shaper_profile *profile =
		LIST_FIRST(&priv->shaper_profiles);
	struct dpaa2_tm_node *node = LIST_FIRST(&priv->nodes);

	while (profile) {
		struct dpaa2_tm_shaper_profile *next = LIST_NEXT(profile, next);

		LIST_REMOVE(profile, next);
		rte_free(profile);
		profile = next;
	}

	while (node) {
		struct dpaa2_tm_node *next = LIST_NEXT(node, next);

		LIST_REMOVE(node, next);
		rte_free(node);
		node = next;
	}
}

static struct dpaa2_tm_node *
dpaa2_node_from_id(struct dpaa2_dev_priv *priv, uint32_t node_id)
{
	struct dpaa2_tm_node *node;

	LIST_FOREACH(node, &priv->nodes, next)
		if (node->id == node_id)
			return node;

	return NULL;
}

static int
dpaa2_capabilities_get(struct rte_eth_dev *dev,
		       struct rte_tm_capabilities *cap,
		      struct rte_tm_error *error)
{
	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Capabilities are NULL\n");

	memset(cap, 0, sizeof(*cap));

	/* root node(port) + txqs number, assuming each TX
	 * Queue is mapped to each TC
	 */
	cap->n_nodes_max = 1 + dev->data->nb_tx_queues;
	cap->n_levels_max = 2; /* port level + txqs level */
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	cap->shaper_n_max = 1;
	cap->shaper_private_n_max = 1;
	cap->shaper_private_dual_rate_n_max = 1;
	cap->shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
	cap->shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

	cap->sched_n_children_max = dev->data->nb_tx_queues;
	cap->sched_sp_n_priorities_max = dev->data->nb_tx_queues;
	cap->sched_wfq_n_children_per_group_max = dev->data->nb_tx_queues;
	cap->sched_wfq_n_groups_max = 2;
	cap->sched_wfq_weight_max = DPAA2_WEIGHT_MAX;

	cap->dynamic_update_mask = RTE_TM_UPDATE_NODE_STATS;
	cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;

	return 0;
}

static int
dpaa2_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error)
{
	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	memset(cap, 0, sizeof(*cap));

	if (level_id > 1)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_LEVEL_ID,
					 NULL, "Wrong level id\n");

	if (level_id == 0) { /* Root node */
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = 1;
		cap->nonleaf.shaper_private_dual_rate_supported = 1;
		cap->nonleaf.shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
		cap->nonleaf.shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

		cap->nonleaf.sched_n_children_max = dev->data->nb_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			dev->data->nb_tx_queues;
		cap->nonleaf.sched_wfq_n_groups_max = 2;
		cap->nonleaf.sched_wfq_weight_max = DPAA2_WEIGHT_MAX;
		cap->nonleaf.stats_mask = RTE_TM_STATS_N_PKTS |
					  RTE_TM_STATS_N_BYTES;
	} else { /* leaf nodes */
		cap->n_nodes_max = dev->data->nb_tx_queues;
		cap->n_nodes_leaf_max = dev->data->nb_tx_queues;
		cap->leaf_nodes_identical = 1;

		cap->leaf.stats_mask = RTE_TM_STATS_N_PKTS;
	}

	return 0;
}

static int
dpaa2_node_capabilities_get(struct rte_eth_dev *dev, uint32_t node_id,
			    struct rte_tm_node_capabilities *cap,
			   struct rte_tm_error *error)
{
	struct dpaa2_tm_node *node;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	memset(cap, 0, sizeof(*cap));

	node = dpaa2_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	if (node->type == 0) {
		cap->shaper_private_supported = 1;

		cap->nonleaf.sched_n_children_max = dev->data->nb_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			dev->data->nb_tx_queues;
		cap->nonleaf.sched_wfq_n_groups_max = 2;
		cap->nonleaf.sched_wfq_weight_max = DPAA2_WEIGHT_MAX;
		cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;
	} else {
		cap->stats_mask = RTE_TM_STATS_N_PKTS;
	}

	return 0;
}

static int
dpaa2_node_type_get(struct rte_eth_dev *dev, uint32_t node_id, int *is_leaf,
		    struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_node *node;

	if (!is_leaf)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	node = dpaa2_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	*is_leaf = node->type == 1/*NODE_QUEUE*/ ? 1 : 0;

	return 0;
}

static struct dpaa2_tm_shaper_profile *
dpaa2_shaper_profile_from_id(struct dpaa2_dev_priv *priv,
				uint32_t shaper_profile_id)
{
	struct dpaa2_tm_shaper_profile *profile;

	LIST_FOREACH(profile, &priv->shaper_profiles, next)
		if (profile->id == shaper_profile_id)
			return profile;

	return NULL;
}

static int
dpaa2_shaper_profile_add(struct rte_eth_dev *dev, uint32_t shaper_profile_id,
			 struct rte_tm_shaper_params *params,
			struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_shaper_profile *profile;

	if (!params)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);
	if (params->committed.rate > DPAA2_SHAPER_MAX_RATE)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
				NULL, "committed rate is out of range\n");

	if (params->committed.size > DPAA2_BURST_MAX)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
				NULL, "committed size is out of range\n");

	if (params->peak.rate > DPAA2_SHAPER_MAX_RATE)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
				NULL, "Peak rate is out of range\n");

	if (params->peak.size > DPAA2_BURST_MAX)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
				NULL, "Peak size is out of range\n");

	if (shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					 NULL, "Wrong shaper profile id\n");

	profile = dpaa2_shaper_profile_from_id(priv, shaper_profile_id);
	if (profile)
		return -rte_tm_error_set(error, EEXIST,
					 RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					 NULL, "Profile id already exists\n");

	profile = rte_zmalloc_socket(NULL, sizeof(*profile), 0,
				     rte_socket_id());
	if (!profile)
		return -rte_tm_error_set(error, ENOMEM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	profile->id = shaper_profile_id;
	rte_memcpy(&profile->params, params, sizeof(profile->params));

	LIST_INSERT_HEAD(&priv->shaper_profiles, profile, next);

	return 0;
}

static int
dpaa2_shaper_profile_delete(struct rte_eth_dev *dev, uint32_t shaper_profile_id,
			    struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_shaper_profile *profile;

	profile = dpaa2_shaper_profile_from_id(priv, shaper_profile_id);
	if (!profile)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					 NULL, "Profile id does not exist\n");

	if (profile->refcnt)
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					 NULL, "Profile is used\n");

	LIST_REMOVE(profile, next);
	rte_free(profile);

	return 0;
}

static int
dpaa2_node_check_params(struct rte_eth_dev *dev, uint32_t node_id,
		__rte_unused uint32_t priority, uint32_t weight,
		       uint32_t level_id,
		       struct rte_tm_node_params *params,
		       struct rte_tm_error *error)
{
	if (node_id == RTE_TM_NODE_ID_NULL)
		return -rte_tm_error_set(error, EINVAL, RTE_TM_NODE_ID_NULL,
					 NULL, "Node id is invalid\n");

	if (weight > DPAA2_WEIGHT_MAX)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_NODE_WEIGHT,
					 NULL, "Weight is out of range\n");

	if (level_id != 0 && level_id != 1)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_LEVEL_ID,
					 NULL, "Wrong level id\n");

	if (!params)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	if (params->shared_shaper_id)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID,
				NULL, "Shared shaper is not supported\n");

	if (params->n_shared_shapers)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
				NULL, "Shared shaper is not supported\n");

	/* verify port (root node) settings */
	if (node_id >= dev->data->nb_tx_queues) {
		if (params->nonleaf.wfq_weight_mode)
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE,
				NULL, "WFQ weight mode is not supported\n");

		if (params->stats_mask & ~(RTE_TM_STATS_N_PKTS |
					   RTE_TM_STATS_N_BYTES))
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested port stats are not supported\n");

		return 0;
	}
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE)
		return -rte_tm_error_set(error, EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL, "Private shaper not supported on leaf\n");

	if (params->stats_mask & ~RTE_TM_STATS_N_PKTS)
		return -rte_tm_error_set(error, EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			"Requested stats are not supported\n");

	/* check leaf node */
	if (level_id == 1) {
		if (params->leaf.cman != RTE_TM_CMAN_TAIL_DROP)
			return -rte_tm_error_set(error, ENODEV,
					RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN,
					NULL, "Only taildrop is supported\n");
	}

	return 0;
}

static int
dpaa2_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority, uint32_t weight,
	      uint32_t level_id, struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_shaper_profile *profile = NULL;
	struct dpaa2_tm_node *node, *parent = NULL;
	int ret;

	if (0/* If device is started*/)
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Port is already started\n");

	ret = dpaa2_node_check_params(dev, node_id, priority, weight, level_id,
				      params, error);
	if (ret)
		return ret;

	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		profile = dpaa2_shaper_profile_from_id(priv,
						     params->shaper_profile_id);
		if (!profile)
			return -rte_tm_error_set(error, ENODEV,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL, "Shaper id does not exist\n");
	}
	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		LIST_FOREACH(node, &priv->nodes, next) {
			if (node->type != 0 /*root node*/)
				continue;

			return -rte_tm_error_set(error, EINVAL,
						 RTE_TM_ERROR_TYPE_UNSPECIFIED,
						 NULL, "Root node exists\n");
		}
	} else {
		parent = dpaa2_node_from_id(priv, parent_node_id);
		if (!parent)
			return -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL, "Parent node id not exist\n");
	}

	node = dpaa2_node_from_id(priv, node_id);
	if (node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id already exists\n");

	node = rte_zmalloc_socket(NULL, sizeof(*node), 0, rte_socket_id());
	if (!node)
		return -rte_tm_error_set(error, ENOMEM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	node->id = node_id;
	node->type = parent_node_id == RTE_TM_NODE_ID_NULL ? 0/*NODE_PORT*/ :
							     1/*NODE_QUEUE*/;

	if (parent) {
		node->parent = parent;
		parent->refcnt++;
	}

	if (profile) {
		node->profile = profile;
		profile->refcnt++;
	}

	node->weight = weight;
	node->priority = priority;
	node->stats_mask = params->stats_mask;

	LIST_INSERT_HEAD(&priv->nodes, node, next);

	return 0;
}

static int
dpaa2_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		  struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_node *node;

	if (0) {
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Port is already started\n");
	}

	node = dpaa2_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	if (node->refcnt)
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id is used\n");

	if (node->parent)
		node->parent->refcnt--;

	if (node->profile)
		node->profile->refcnt--;

	LIST_REMOVE(node, next);
	rte_free(node);

	return 0;
}

static int
dpaa2_hierarchy_commit(struct rte_eth_dev *dev, int clear_on_fail,
		       struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_node *node, *temp_node;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int ret;
	int wfq_grp = 0, is_wfq_grp = 0, conf[DPNI_MAX_TC];
	struct dpni_tx_priorities_cfg prio_cfg;

	memset(&prio_cfg, 0, sizeof(prio_cfg));
	memset(conf, 0, sizeof(conf));

	LIST_FOREACH(node, &priv->nodes, next) {
		if (node->type == 0/*root node*/) {
			if (!node->profile)
				continue;

			struct dpni_tx_shaping_cfg tx_cr_shaper, tx_er_shaper;

			tx_cr_shaper.max_burst_size =
				node->profile->params.committed.size;
			tx_cr_shaper.rate_limit =
				node->profile->params.committed.rate / (1024 * 1024);
			tx_er_shaper.max_burst_size =
				node->profile->params.peak.size;
			tx_er_shaper.rate_limit =
				node->profile->params.peak.rate / (1024 * 1024);
			ret = dpni_set_tx_shaping(dpni, 0, priv->token,
					&tx_cr_shaper, &tx_er_shaper, 0);
			if (ret) {
				ret = -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE, NULL,
					"Error in setting Shaping\n");
				goto out;
			}

			continue;
		} else { /* level 1, all leaf nodes */
			if (node->id >= dev->data->nb_tx_queues) {
				ret = -rte_tm_error_set(error, EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID, NULL,
						"Not enough txqs configured\n");
				goto out;
			}

			if (conf[node->id])
				continue;

			LIST_FOREACH(temp_node, &priv->nodes, next) {
				if (temp_node->id == node->id ||
					temp_node->type == 0)
					continue;
				if (conf[temp_node->id])
					continue;
				if (node->priority == temp_node->priority) {
					if (wfq_grp == 0) {
						prio_cfg.tc_sched[temp_node->id].mode =
								DPNI_TX_SCHED_WEIGHTED_A;
						/* DPDK support lowest weight 1
						 * and DPAA2 platform 100
						 */
						prio_cfg.tc_sched[temp_node->id].delta_bandwidth =
								temp_node->weight + 99;
					} else if (wfq_grp == 1) {
						prio_cfg.tc_sched[temp_node->id].mode =
								DPNI_TX_SCHED_WEIGHTED_B;
						prio_cfg.tc_sched[temp_node->id].delta_bandwidth =
								temp_node->weight + 99;
					} else {
						/*TODO: add one more check for
						 * number of nodes in a group
						 */
						ret = -rte_tm_error_set(error, EINVAL,
							RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
							"Only 2 WFQ Groups are supported\n");
						goto out;
					}
					conf[temp_node->id] = 1;
					is_wfq_grp = 1;
				}
			}
			if (is_wfq_grp) {
				if (wfq_grp == 0) {
					prio_cfg.tc_sched[node->id].mode =
							DPNI_TX_SCHED_WEIGHTED_A;
					prio_cfg.tc_sched[node->id].delta_bandwidth =
							node->weight + 99;
					prio_cfg.prio_group_A = node->priority;
				} else if (wfq_grp == 1) {
					prio_cfg.tc_sched[node->id].mode =
							DPNI_TX_SCHED_WEIGHTED_B;
					prio_cfg.tc_sched[node->id].delta_bandwidth =
							node->weight + 99;
					prio_cfg.prio_group_B = node->priority;
				}
				wfq_grp++;
				is_wfq_grp = 0;
			}
			conf[node->id] = 1;
		}
		if (wfq_grp)
			prio_cfg.separate_groups = 1;
	}
	ret = dpni_set_tx_priorities(dpni, 0, priv->token, &prio_cfg);
	if (ret) {
		ret = -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Scheduling Failed\n");
		goto out;
	}

	return 0;

out:
	if (clear_on_fail) {
		dpaa2_tm_deinit(dev);
		dpaa2_tm_init(dev);
	}

	return ret;
}

const struct rte_tm_ops dpaa2_tm_ops = {
	.node_type_get = dpaa2_node_type_get,
	.capabilities_get = dpaa2_capabilities_get,
	.level_capabilities_get = dpaa2_level_capabilities_get,
	.node_capabilities_get = dpaa2_node_capabilities_get,
	.shaper_profile_add = dpaa2_shaper_profile_add,
	.shaper_profile_delete = dpaa2_shaper_profile_delete,
	.node_add = dpaa2_node_add,
	.node_delete = dpaa2_node_delete,
	.hierarchy_commit = dpaa2_hierarchy_commit,
};
