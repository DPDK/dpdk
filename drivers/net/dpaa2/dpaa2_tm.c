/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_tm_driver.h>

#include "dpaa2_ethdev.h"
#include "dpaa2_pmd_logs.h"
#include <dpaa2_hw_dpio.h>

#define DPAA2_BURST_MAX	(64 * 1024)

#define DPAA2_SHAPER_MIN_RATE 0
#define DPAA2_SHAPER_MAX_RATE 107374182400ull
#define DPAA2_WEIGHT_MAX 24701
#define DPAA2_PKT_ADJUST_LEN_MIN 0
#define DPAA2_PKT_ADJUST_LEN_MAX 0x7ff

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
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Capabilities are NULL\n");

	memset(cap, 0, sizeof(*cap));

	/* root node(port) + channels + txqs number, assuming each TX
	 * Queue is mapped to each TC
	 */
	cap->n_nodes_max = 1 + priv->num_channels + dev->data->nb_tx_queues;
	cap->n_levels_max = MAX_LEVEL;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	cap->shaper_n_max = 1 + priv->num_channels; /* LNI + channels */
	cap->shaper_private_n_max = 1 + priv->num_channels;
	cap->shaper_private_dual_rate_n_max = 1 + priv->num_channels;
	cap->shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
	cap->shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;
	cap->shaper_pkt_length_adjust_min = DPAA2_PKT_ADJUST_LEN_MIN;
	cap->shaper_pkt_length_adjust_max = DPAA2_PKT_ADJUST_LEN_MAX;

	if (priv->num_channels > DPNI_MAX_TC)
		cap->sched_n_children_max = priv->num_channels;
	else
		cap->sched_n_children_max = DPNI_MAX_TC;

	cap->sched_sp_n_priorities_max = DPNI_MAX_TC;
	cap->sched_wfq_n_children_per_group_max = DPNI_MAX_TC;
	cap->sched_wfq_n_groups_max = 2;
	cap->sched_wfq_weight_max = DPAA2_WEIGHT_MAX / 100;
	cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;

	return 0;
}

static int
dpaa2_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	memset(cap, 0, sizeof(*cap));

	if (level_id > QUEUE_LEVEL)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_LEVEL_ID,
					 NULL, "Wrong level id\n");

	if (level_id == LNI_LEVEL) { /* Root node (LNI) */
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = 1;
		cap->nonleaf.shaper_private_dual_rate_supported = 1;
		cap->nonleaf.shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
		cap->nonleaf.shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

		cap->nonleaf.sched_n_children_max = priv->num_channels; /* no. of channels */
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.stats_mask = RTE_TM_STATS_N_PKTS |
					  RTE_TM_STATS_N_BYTES;
	} else if (level_id == CHANNEL_LEVEL) { /* channels */
		cap->n_nodes_max = priv->num_channels;
		cap->n_nodes_nonleaf_max = priv->num_channels;
		cap->n_nodes_leaf_max = 0;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = 1;
		cap->nonleaf.shaper_private_dual_rate_supported = 1;
		cap->nonleaf.shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
		cap->nonleaf.shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

		/* no. of class queues per channel */
		cap->nonleaf.sched_n_children_max = priv->num_tx_tc;
		cap->nonleaf.sched_sp_n_priorities_max = priv->num_tx_tc;
		cap->nonleaf.sched_wfq_n_children_per_group_max = priv->num_tx_tc;
		cap->nonleaf.sched_wfq_n_groups_max = 2;
		cap->nonleaf.sched_wfq_weight_max = DPAA2_WEIGHT_MAX / 100;
	} else { /* leaf nodes */
		/* queues per channels * channel */
		cap->n_nodes_max = priv->num_tx_tc * priv->num_channels;
		cap->n_nodes_leaf_max = priv->num_tx_tc * priv->num_channels;
		cap->leaf_nodes_identical = 1;

		cap->leaf.shaper_private_supported = 0;
		cap->leaf.stats_mask = RTE_TM_STATS_N_PKTS |
				       RTE_TM_STATS_N_BYTES;
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

	if (node->level_id == LNI_LEVEL) {
		cap->shaper_private_supported = 1;
		cap->shaper_private_dual_rate_supported = 1;
		cap->shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
		cap->shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

		cap->nonleaf.sched_n_children_max = priv->num_channels;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->stats_mask = RTE_TM_STATS_N_PKTS |
					  RTE_TM_STATS_N_BYTES;
	} else if (node->level_id == CHANNEL_LEVEL) {
		cap->shaper_private_supported = 1;
		cap->shaper_private_dual_rate_supported = 1;
		cap->shaper_private_rate_min = DPAA2_SHAPER_MIN_RATE;
		cap->shaper_private_rate_max = DPAA2_SHAPER_MAX_RATE;

		cap->nonleaf.sched_n_children_max = priv->num_tx_tc;
		cap->nonleaf.sched_sp_n_priorities_max = priv->num_tx_tc;
		cap->nonleaf.sched_wfq_n_children_per_group_max = priv->num_tx_tc;
		cap->nonleaf.sched_wfq_n_groups_max = 2;
		cap->nonleaf.sched_wfq_weight_max = DPAA2_WEIGHT_MAX / 100;
	} else {
		cap->stats_mask = RTE_TM_STATS_N_PKTS |
				       RTE_TM_STATS_N_BYTES;
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

	*is_leaf = node->type == LEAF_NODE ? 1 : 0;

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

	if (params->pkt_length_adjust > DPAA2_PKT_ADJUST_LEN_MAX ||
			params->pkt_length_adjust < DPAA2_PKT_ADJUST_LEN_MIN)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_CAPABILITIES,
					 NULL,
					 "Not supported pkt adjust length\n");

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

	if (level_id > QUEUE_LEVEL)
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

	/* verify non leaf nodes settings */
	if (node_id >= dev->data->nb_tx_queues) {
		if (params->nonleaf.wfq_weight_mode)
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE,
				NULL, "WFQ weight mode is not supported\n");
	} else {
		if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE)
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
				NULL, "Private shaper not supported on leaf\n");
	}

	/* check leaf node */
	if (level_id == QUEUE_LEVEL) {
		if (params->leaf.cman != RTE_TM_CMAN_TAIL_DROP)
			return -rte_tm_error_set(error, ENODEV,
					RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN,
					NULL, "Only taildrop is supported\n");
		if (params->stats_mask & ~(RTE_TM_STATS_N_PKTS |
					   RTE_TM_STATS_N_BYTES))
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested port stats are not supported\n");
	} else if (level_id == LNI_LEVEL) {
		if (params->stats_mask & ~(RTE_TM_STATS_N_PKTS |
					   RTE_TM_STATS_N_BYTES))
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested port stats are not supported\n");
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
			if (node->level_id != LNI_LEVEL)
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

	if (node_id > dev->data->nb_tx_queues)
		node->type = NON_LEAF_NODE;
	else
		node->type = LEAF_NODE;

	node->level_id = level_id;
	if (node->level_id == CHANNEL_LEVEL) {
		if (priv->channel_inuse < priv->num_channels) {
			node->channel_id = priv->channel_inuse;
			priv->channel_inuse++;
		} else {
			printf("error no channel id available\n");
		}
	}

	if (parent) {
		node->parent = parent;
		parent->refcnt++;
	}

	/* TODO: add check if refcnt is more than supported children */

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

	/* XXX: update it */
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
dpaa2_tm_configure_queue(struct rte_eth_dev *dev, struct dpaa2_tm_node *node)
{
	int ret = 0;
	uint32_t tc_id;
	uint8_t flow_id, options = 0;
	struct dpni_queue tx_flow_cfg;
	struct dpni_queue_id qid;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_queue *dpaa2_q;

	memset(&tx_flow_cfg, 0, sizeof(struct dpni_queue));
	dpaa2_q =  (struct dpaa2_queue *)dev->data->tx_queues[node->id];
	tc_id = node->parent->tc_id;
	node->parent->tc_id++;
	flow_id = 0;

	if (dpaa2_q == NULL) {
		printf("Queue is not configured for node = %d\n", node->id);
		return -1;
	}

	DPAA2_PMD_DEBUG("tc_id = %d, channel = %d\n\n", tc_id,
			node->parent->channel_id);
	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_TX,
			     ((node->parent->channel_id << 8) | tc_id),
			     flow_id, options, &tx_flow_cfg);
	if (ret) {
		printf("Error in setting the tx flow: "
		       "channel id  = %d tc_id= %d, param = 0x%x "
		       "flow=%d err=%d\n", node->parent->channel_id, tc_id,
		       ((node->parent->channel_id << 8) | tc_id), flow_id,
		       ret);
		return -1;
	}

	dpaa2_q->flow_id = flow_id;
	dpaa2_q->tc_index = tc_id;

	ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
		DPNI_QUEUE_TX, ((node->parent->channel_id << 8) | dpaa2_q->tc_index),
		dpaa2_q->flow_id, &tx_flow_cfg, &qid);
	if (ret) {
		printf("Error in getting LFQID err=%d", ret);
		return -1;
	}
	dpaa2_q->fqid = qid.fqid;

	/* setting congestion notification */
	if (!(priv->flags & DPAA2_TX_CGR_OFF)) {
		struct dpni_congestion_notification_cfg cong_notif_cfg = {0};

		cong_notif_cfg.units = DPNI_CONGESTION_UNIT_FRAMES;
		cong_notif_cfg.threshold_entry = dpaa2_q->nb_desc;
		/* Notify that the queue is not congested when the data in
		 * the queue is below this thershold.(90% of value)
		 */
		cong_notif_cfg.threshold_exit = (dpaa2_q->nb_desc * 9) / 10;
		cong_notif_cfg.message_ctx = 0;
		cong_notif_cfg.message_iova =
			(size_t)DPAA2_VADDR_TO_IOVA(dpaa2_q->cscn);
		cong_notif_cfg.dest_cfg.dest_type = DPNI_DEST_NONE;
		cong_notif_cfg.notification_mode =
					DPNI_CONG_OPT_WRITE_MEM_ON_ENTER |
					DPNI_CONG_OPT_WRITE_MEM_ON_EXIT |
					DPNI_CONG_OPT_COHERENT_WRITE;
		cong_notif_cfg.cg_point = DPNI_CP_QUEUE;

		ret = dpni_set_congestion_notification(dpni, CMD_PRI_LOW,
					priv->token,
					DPNI_QUEUE_TX,
					((node->parent->channel_id << 8) | tc_id),
					&cong_notif_cfg);
		if (ret) {
			printf("Error in setting tx congestion notification: "
				"err=%d", ret);
			return -ret;
		}
	}

	return 0;
}

static void
dpaa2_tm_sort_and_configure(struct rte_eth_dev *dev,
			    struct dpaa2_tm_node **nodes, int n)
{
	struct dpaa2_tm_node *temp_node;
	int i;

	if (n == 1) {
		DPAA2_PMD_DEBUG("node id = %d\n, priority = %d, index = %d\n",
				nodes[n - 1]->id, nodes[n - 1]->priority,
				n - 1);
		dpaa2_tm_configure_queue(dev, nodes[n - 1]);
		return;
	}

	for (i = 0; i < n - 1; i++) {
		if (nodes[i]->priority > nodes[i + 1]->priority) {
			temp_node = nodes[i];
			nodes[i] = nodes[i + 1];
			nodes[i + 1] = temp_node;
		}
	}
	dpaa2_tm_sort_and_configure(dev, nodes, n - 1);

	DPAA2_PMD_DEBUG("node id = %d\n, priority = %d, index = %d\n",
			nodes[n - 1]->id, nodes[n - 1]->priority,
			n - 1);
	dpaa2_tm_configure_queue(dev, nodes[n - 1]);
}

static int
dpaa2_hierarchy_commit(struct rte_eth_dev *dev, int clear_on_fail,
		       struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_node *node;
	struct dpaa2_tm_node *leaf_node, *temp_leaf_node, *channel_node;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int ret, t;

	/* Populate TCs */
	LIST_FOREACH(channel_node, &priv->nodes, next) {
		struct dpaa2_tm_node *nodes[DPNI_MAX_TC];
		int i = 0;

		if (channel_node->level_id != CHANNEL_LEVEL)
			continue;

		LIST_FOREACH(leaf_node, &priv->nodes, next) {
			if (leaf_node->level_id == LNI_LEVEL ||
			    leaf_node->level_id == CHANNEL_LEVEL)
				continue;

			if (leaf_node->parent == channel_node) {
				if (i >= DPNI_MAX_TC) {
					ret = -rte_tm_error_set(error, EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
						"More children than supported\n");
					goto out;
				}
				nodes[i++] = leaf_node;
			}
		}
		if (i > 0) {
			DPAA2_PMD_DEBUG("Configure queues\n");
			dpaa2_tm_sort_and_configure(dev, nodes, i);
		}
	}

	/* Shaping */
	LIST_FOREACH(node, &priv->nodes, next) {
		if (node->type == NON_LEAF_NODE) {
			if (!node->profile)
				continue;
			struct dpni_tx_shaping_cfg tx_cr_shaper, tx_er_shaper;
			uint32_t param = 0;

			tx_cr_shaper.max_burst_size =
				node->profile->params.committed.size;
			tx_cr_shaper.rate_limit =
				node->profile->params.committed.rate /
				(1024 * 1024);
			tx_er_shaper.max_burst_size =
				node->profile->params.peak.size;
			tx_er_shaper.rate_limit =
				node->profile->params.peak.rate / (1024 * 1024);
			/* root node */
			if (node->parent == NULL) {
				DPAA2_PMD_DEBUG("LNI S.rate = %u, burst =%u\n",
						tx_cr_shaper.rate_limit,
						tx_cr_shaper.max_burst_size);
				param = 0x2;
				param |= node->profile->params.pkt_length_adjust << 16;
			} else {
				DPAA2_PMD_DEBUG("Channel = %d S.rate = %u\n",
						node->channel_id,
						tx_cr_shaper.rate_limit);
				param = (node->channel_id << 8);
			}
			ret = dpni_set_tx_shaping(dpni, 0, priv->token,
					&tx_cr_shaper, &tx_er_shaper, param);
			if (ret) {
				ret = -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE, NULL,
					"Error in setting Shaping\n");
				goto out;
			}
			continue;
		}
	}

	LIST_FOREACH(channel_node, &priv->nodes, next) {
		int wfq_grp = 0, is_wfq_grp = 0, conf[DPNI_MAX_TC];
		struct dpni_tx_priorities_cfg prio_cfg;

		memset(&prio_cfg, 0, sizeof(prio_cfg));
		memset(conf, 0, sizeof(conf));

		/* Process for each channel */
		if (channel_node->level_id != CHANNEL_LEVEL)
			continue;

		LIST_FOREACH(leaf_node, &priv->nodes, next) {
			struct dpaa2_queue *leaf_dpaa2_q;
			uint8_t leaf_tc_id;

			if (leaf_node->level_id == LNI_LEVEL ||
			    leaf_node->level_id == CHANNEL_LEVEL)
				continue;

			 /* level 2, all leaf nodes */
			if (leaf_node->id >= dev->data->nb_tx_queues) {
				ret = -rte_tm_error_set(error, EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID, NULL,
						"Not enough txqs configured\n");
				goto out;
			}

			if (conf[leaf_node->id])
				continue;

			if (leaf_node->parent != channel_node)
				continue;

			leaf_dpaa2_q =  (struct dpaa2_queue *)dev->data->tx_queues[leaf_node->id];
			leaf_tc_id = leaf_dpaa2_q->tc_index;
			/* Process sibling leaf nodes */
			LIST_FOREACH(temp_leaf_node, &priv->nodes, next) {
				if (temp_leaf_node->id == leaf_node->id ||
					temp_leaf_node->level_id == LNI_LEVEL ||
					temp_leaf_node->level_id == CHANNEL_LEVEL)
					continue;

				if (temp_leaf_node->parent != channel_node)
					continue;

				if (conf[temp_leaf_node->id])
					continue;

				if (leaf_node->priority == temp_leaf_node->priority) {
					struct dpaa2_queue *temp_leaf_dpaa2_q;
					uint8_t temp_leaf_tc_id;

					temp_leaf_dpaa2_q = (struct dpaa2_queue *)
						dev->data->tx_queues[temp_leaf_node->id];
					temp_leaf_tc_id = temp_leaf_dpaa2_q->tc_index;
					if (wfq_grp == 0) {
						prio_cfg.tc_sched[temp_leaf_tc_id].mode =
							DPNI_TX_SCHED_WEIGHTED_A;
						/* DPAA2 support weight in multiple of 100 */
						prio_cfg.tc_sched[temp_leaf_tc_id].delta_bandwidth =
							temp_leaf_node->weight * 100;
					} else if (wfq_grp == 1) {
						prio_cfg.tc_sched[temp_leaf_tc_id].mode =
							DPNI_TX_SCHED_WEIGHTED_B;
						prio_cfg.tc_sched[temp_leaf_tc_id].delta_bandwidth =
							temp_leaf_node->weight * 100;
					} else {
						ret = -rte_tm_error_set(error, EINVAL,
							RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
							"Only 2 WFQ Groups are supported\n");
						goto out;
					}
					is_wfq_grp = 1;
					conf[temp_leaf_node->id] = 1;
				}
			}
			if (is_wfq_grp) {
				if (wfq_grp == 0) {
					prio_cfg.tc_sched[leaf_tc_id].mode =
						DPNI_TX_SCHED_WEIGHTED_A;
					prio_cfg.tc_sched[leaf_tc_id].delta_bandwidth =
						leaf_node->weight * 100;
					prio_cfg.prio_group_A = leaf_node->priority;
				} else if (wfq_grp == 1) {
					prio_cfg.tc_sched[leaf_tc_id].mode =
						DPNI_TX_SCHED_WEIGHTED_B;
					prio_cfg.tc_sched[leaf_tc_id].delta_bandwidth =
						leaf_node->weight * 100;
					prio_cfg.prio_group_B = leaf_node->priority;
				}
				wfq_grp++;
				is_wfq_grp = 0;
			}
			conf[leaf_node->id] = 1;
		}
		if (wfq_grp > 1) {
			prio_cfg.separate_groups = 1;
			if (prio_cfg.prio_group_B < prio_cfg.prio_group_A) {
				prio_cfg.prio_group_A = 0;
				prio_cfg.prio_group_B = 1;
			} else {
				prio_cfg.prio_group_A = 1;
				prio_cfg.prio_group_B = 0;
			}
		}

		prio_cfg.prio_group_A = 1;
		prio_cfg.channel_idx = channel_node->channel_id;
		ret = dpni_set_tx_priorities(dpni, 0, priv->token, &prio_cfg);
		if (ret) {
			ret = -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Scheduling Failed\n");
			goto out;
		}
		DPAA2_PMD_DEBUG("########################################\n");
		DPAA2_PMD_DEBUG("Channel idx = %d\n", prio_cfg.channel_idx);
		for (t = 0; t < DPNI_MAX_TC; t++) {
			DPAA2_PMD_DEBUG("tc = %d mode = %d ", t, prio_cfg.tc_sched[t].mode);
			DPAA2_PMD_DEBUG("delta = %d\n", prio_cfg.tc_sched[t].delta_bandwidth);
		}
		DPAA2_PMD_DEBUG("prioritya = %d\n", prio_cfg.prio_group_A);
		DPAA2_PMD_DEBUG("priorityb = %d\n", prio_cfg.prio_group_B);
		DPAA2_PMD_DEBUG("separate grps = %d\n\n", prio_cfg.separate_groups);
	}
	return 0;

out:
	if (clear_on_fail) {
		dpaa2_tm_deinit(dev);
		dpaa2_tm_init(dev);
	}

	return ret;
}

static int
dpaa2_node_stats_read(struct rte_eth_dev *dev, uint32_t node_id,
		      struct rte_tm_node_stats *stats, uint64_t *stats_mask,
		      int clear, struct rte_tm_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_tm_node *node;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	union dpni_statistics value;
	int ret = 0;

	node = dpaa2_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
				RTE_TM_ERROR_TYPE_NODE_ID,
				NULL, "Node id does not exist\n");

	if (stats_mask)
		*stats_mask = node->stats_mask;

	if (!stats)
		return 0;

	memset(stats, 0, sizeof(*stats));
	memset(&value, 0, sizeof(union dpni_statistics));

	if (node->level_id == LNI_LEVEL) {
		uint8_t page1 = 1;

		ret = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					  page1, 0, &value);
		if (ret)
			return -rte_tm_error_set(error, -ret,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to read port statistics\n");

		if (node->stats_mask & RTE_TM_STATS_N_PKTS)
			stats->n_pkts = value.page_1.egress_all_frames;

		if (node->stats_mask & RTE_TM_STATS_N_BYTES)
			stats->n_bytes = value.page_1.egress_all_bytes;

		if (clear) {
			ret = dpni_reset_statistics(dpni, CMD_PRI_LOW, priv->token);
				return -rte_tm_error_set(error, -ret,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to reset port statistics\n");
		}
	} else if (node->level_id == QUEUE_LEVEL) {
		uint8_t page3 = 3;
		struct dpaa2_queue *dpaa2_q;
		dpaa2_q =  (struct dpaa2_queue *)dev->data->tx_queues[node->id];

		ret = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					  page3,
					  (node->parent->channel_id << 8 |
					   dpaa2_q->tc_index), &value);
		if (ret)
			return -rte_tm_error_set(error, -ret,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to read queue statistics\n");

		if (node->stats_mask & RTE_TM_STATS_N_PKTS)
			stats->n_pkts = value.page_3.ceetm_dequeue_frames;
		if (node->stats_mask & RTE_TM_STATS_N_BYTES)
			stats->n_bytes = value.page_3.ceetm_dequeue_bytes;
	} else {
		return -rte_tm_error_set(error, -1,
				RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to read channel statistics\n");
	}

	return 0;
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
	.node_stats_read = dpaa2_node_stats_read,
};
