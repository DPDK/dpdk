/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include <rte_malloc.h>

#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "mrvl_tm.h"

/** Minimum rate value in Bytes/s */
#define MRVL_RATE_MIN (PP2_PPIO_MIN_CIR * 1000 / 8)

/** Minimum burst size in Bytes */
#define MRVL_BURST_MIN (PP2_PPIO_MIN_CBS * 1000)

/** Maximum burst size in Bytes */
#define MRVL_BURST_MAX 256000000

/** Maximum WRR weight */
#define MRVL_WEIGHT_MAX 255

/**
 * Get maximum port rate in Bytes/s.
 *
 * @param dev Pointer to the device.
 * @param rate Pointer to the rate.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_get_max_rate(struct rte_eth_dev *dev, uint64_t *rate)
{
	struct ethtool_cmd edata;
	struct ifreq req;
	int ret, fd;

	memset(&edata, 0, sizeof(edata));
	memset(&req, 0, sizeof(req));
	edata.cmd = ETHTOOL_GSET;
	strcpy(req.ifr_name, dev->data->name);
	req.ifr_data = (void *)&edata;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		return -1;

	ret = ioctl(fd, SIOCETHTOOL, &req);
	if (ret == -1) {
		close(fd);
		return -1;
	}

	close(fd);

	*rate = ethtool_cmd_speed(&edata) * 1000 * 1000 / 8;

	return 0;
}

/**
 * Initialize traffic manager related data.
 *
 * @param dev Pointer to the device.
 * @returns 0 on success, failure otherwise.
 */
int
mrvl_tm_init(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	LIST_INIT(&priv->shaper_profiles);
	LIST_INIT(&priv->nodes);

	if (priv->rate_max)
		return 0;

	return mrvl_get_max_rate(dev, &priv->rate_max);
}

/**
 * Cleanup traffic manager related data.
 *
 * @param dev Pointer to the device.
 */
void mrvl_tm_deinit(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_shaper_profile *profile =
		LIST_FIRST(&priv->shaper_profiles);
	struct mrvl_tm_node *node = LIST_FIRST(&priv->nodes);

	while (profile) {
		struct mrvl_tm_shaper_profile *next = LIST_NEXT(profile, next);

		LIST_REMOVE(profile, next);
		rte_free(profile);
		profile = next;
	}

	while (node) {
		struct mrvl_tm_node *next = LIST_NEXT(node, next);

		LIST_REMOVE(node, next);
		rte_free(node);
		node = next;
	}
}

/**
 * Get node using its id.
 *
 * @param priv Pointer to the port's private data.
 * @param node_id Id used by this node.
 * @returns Pointer to the node if exists, NULL otherwise.
 */
static struct mrvl_tm_node *
mrvl_node_from_id(struct mrvl_priv *priv, uint32_t node_id)
{
	struct mrvl_tm_node *node;

	LIST_FOREACH(node, &priv->nodes, next)
		if (node->id == node_id)
			return node;

	return NULL;
}

/**
 * Check whether node is leaf or root.
 *
 * @param dev Pointer to the device.
 * @param node_id Id used by this node.
 * @param is_leaf Pointer to flag indicating whether node is a leaf.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_type_get(struct rte_eth_dev *dev, uint32_t node_id, int *is_leaf,
		   struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;

	if (!is_leaf)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	*is_leaf = node->type == MRVL_NODE_QUEUE ? 1 : 0;

	return 0;
}

/**
 * Get traffic manager capabilities.
 *
 * @param dev Pointer to the device (unused).
 * @param cap Pointer to the capabilities.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_capabilities_get(struct rte_eth_dev *dev,
		      struct rte_tm_capabilities *cap,
		      struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Capabilities are missing\n");

	memset(cap, 0, sizeof(*cap));

	cap->n_nodes_max = 1 + dev->data->nb_tx_queues; /* port + txqs number */
	cap->n_levels_max = 2; /* port level + txqs level */
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->shaper_n_max;
	cap->shaper_private_rate_min = MRVL_RATE_MIN;
	cap->shaper_private_rate_max = priv->rate_max;

	cap->sched_n_children_max = dev->data->nb_tx_queues;
	cap->sched_sp_n_priorities_max = dev->data->nb_tx_queues;
	cap->sched_wfq_n_children_per_group_max = dev->data->nb_tx_queues;
	cap->sched_wfq_n_groups_max = 1;
	cap->sched_wfq_weight_max = MRVL_WEIGHT_MAX;

	cap->dynamic_update_mask = RTE_TM_UPDATE_NODE_SUSPEND_RESUME |
				   RTE_TM_UPDATE_NODE_STATS;
	cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;

	return 0;
}

/**
 * Get traffic manager hierarchy level capabilities.
 *
 * @param dev Pointer to the device.
 * @param level_id Id of the level.
 * @param cap Pointer to the level capabilities.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_level_capabilities_get(struct rte_eth_dev *dev,
			    uint32_t level_id,
			    struct rte_tm_level_capabilities *cap,
			    struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	memset(cap, 0, sizeof(*cap));

	if (level_id != MRVL_NODE_PORT && level_id != MRVL_NODE_QUEUE)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_LEVEL_ID,
					 NULL, "Wrong level id\n");

	if (level_id == MRVL_NODE_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = 1;
		cap->nonleaf.shaper_private_rate_min = MRVL_RATE_MIN;
		cap->nonleaf.shaper_private_rate_max = priv->rate_max;

		cap->nonleaf.sched_n_children_max = dev->data->nb_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			dev->data->nb_tx_queues;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = MRVL_WEIGHT_MAX;
		cap->nonleaf.stats_mask = RTE_TM_STATS_N_PKTS |
					  RTE_TM_STATS_N_BYTES;
	} else { /* level_id == MRVL_NODE_QUEUE */
		cap->n_nodes_max = dev->data->nb_tx_queues;
		cap->n_nodes_leaf_max = dev->data->nb_tx_queues;
		cap->leaf_nodes_identical = 1;

		cap->leaf.shaper_private_supported = 1;
		cap->leaf.shaper_private_rate_min = MRVL_RATE_MIN;
		cap->leaf.shaper_private_rate_max = priv->rate_max;
		cap->leaf.stats_mask = RTE_TM_STATS_N_PKTS;
	}

	return 0;
}

/**
 * Get node capabilities.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param cap Pointer to the capabilities.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_capabilities_get(struct rte_eth_dev *dev, uint32_t node_id,
			   struct rte_tm_node_capabilities *cap,
			   struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;

	if (!cap)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	memset(cap, 0, sizeof(*cap));

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	cap->shaper_private_supported = 1;
	cap->shaper_private_rate_min = MRVL_RATE_MIN;
	cap->shaper_private_rate_max = priv->rate_max;

	if (node->type == MRVL_NODE_PORT) {
		cap->nonleaf.sched_n_children_max = dev->data->nb_tx_queues;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			dev->data->nb_tx_queues;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = MRVL_WEIGHT_MAX;
		cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;
	} else {
		cap->stats_mask = RTE_TM_STATS_N_PKTS;
	}

	return 0;
}

/**
 * Get shaper profile using its id.
 *
 * @param priv Pointer to the port's private data.
 * @param shaper_profile_id Id used by the shaper.
 * @returns Pointer to the shaper profile if exists, NULL otherwise.
 */
static struct mrvl_tm_shaper_profile *
mrvl_shaper_profile_from_id(struct mrvl_priv *priv, uint32_t shaper_profile_id)
{
	struct mrvl_tm_shaper_profile *profile;

	LIST_FOREACH(profile, &priv->shaper_profiles, next)
		if (profile->id == shaper_profile_id)
			return profile;

	return NULL;
}

/**
 * Add a new shaper profile.
 *
 * @param dev Pointer to the device.
 * @param shaper_profile_id Id of the new profile.
 * @param params Pointer to the shaper profile parameters.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_shaper_profile_add(struct rte_eth_dev *dev, uint32_t shaper_profile_id,
			struct rte_tm_shaper_params *params,
			struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_shaper_profile *profile;

	if (!params)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, NULL);

	if (params->committed.rate)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE,
				NULL, "Committed rate not supported\n");

	if (params->committed.size)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE,
				NULL, "Committed bucket size not supported\n");

	if (params->peak.rate < MRVL_RATE_MIN ||
	    params->peak.rate > priv->rate_max)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
				NULL, "Peak rate is out of range\n");

	if (params->peak.size < MRVL_BURST_MIN ||
	    params->peak.size > MRVL_BURST_MAX)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
				NULL, "Peak size is out of range\n");

	if (shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					 NULL, "Wrong shaper profile id\n");

	profile = mrvl_shaper_profile_from_id(priv, shaper_profile_id);
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

/**
 * Remove a shaper profile.
 *
 * @param dev Pointer to the device.
 * @param shaper_profile_id Id of the shaper profile.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_shaper_profile_delete(struct rte_eth_dev *dev, uint32_t shaper_profile_id,
			   struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_shaper_profile *profile;

	profile = mrvl_shaper_profile_from_id(priv, shaper_profile_id);
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

/**
 * Check node parameters.
 *
 * @param dev Pointer to the device.
 * @param node_id Id used by the node.
 * @param priority Priority value.
 * @param weight Weight value.
 * @param level_id Id of the level.
 * @param params Pointer to the node parameters.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_check_params(struct rte_eth_dev *dev, uint32_t node_id,
		       uint32_t priority, uint32_t weight, uint32_t level_id,
		       struct rte_tm_node_params *params,
		       struct rte_tm_error *error)
{
	if (node_id == RTE_TM_NODE_ID_NULL)
		return -rte_tm_error_set(error, EINVAL, RTE_TM_NODE_ID_NULL,
					 NULL, "Node id is invalid\n");

	if (priority)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_NODE_PRIORITY,
					 NULL, "Priority should be 0\n");

	if (weight > MRVL_WEIGHT_MAX)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_NODE_WEIGHT,
					 NULL, "Weight is out of range\n");

	if (level_id != MRVL_NODE_PORT && level_id != MRVL_NODE_QUEUE)
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
				NULL, "WFQ is not supported\n");

		if (params->nonleaf.n_sp_priorities != 1)
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
				NULL, "SP is not supported\n");

		if (params->stats_mask & ~(RTE_TM_STATS_N_PKTS |
					   RTE_TM_STATS_N_BYTES))
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested port stats are not supported\n");

		return 0;
	}

	/* verify txq (leaf node) settings */
	if (params->leaf.cman)
		return -rte_tm_error_set(error, EINVAL,
					 RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN,
					 NULL,
					 "Congestion mngmt is not supported\n");

	if (params->leaf.wred.wred_profile_id)
		return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID,
				NULL, "WRED is not supported\n");

	if (params->leaf.wred.shared_wred_context_id)
		return -rte_tm_error_set(error, EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_WRED_CONTEXT_ID,
			NULL, "WRED is not supported\n");

	if (params->leaf.wred.n_shared_wred_contexts)
		return -rte_tm_error_set(error, EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS,
			NULL, "WRED is not supported\n");

	if (params->stats_mask & ~RTE_TM_STATS_N_PKTS)
		return -rte_tm_error_set(error, EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			"Requested txq stats are not supported\n");

	return 0;
}

/**
 * Add a new node.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param parent_node_id Id of the parent node.
 * @param priority Priority value.
 * @param weight Weight value.
 * @param level_id Id of the level.
 * @param params Pointer to the node parameters.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_add(struct rte_eth_dev *dev, uint32_t node_id,
	      uint32_t parent_node_id, uint32_t priority, uint32_t weight,
	      uint32_t level_id, struct rte_tm_node_params *params,
	      struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_shaper_profile *profile = NULL;
	struct mrvl_tm_node *node, *parent = NULL;
	int ret;

	if (priv->ppio)
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Port is already started\n");

	ret = mrvl_node_check_params(dev, node_id, priority, weight, level_id,
				     params, error);
	if (ret)
		return ret;

	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		profile = mrvl_shaper_profile_from_id(priv,
						 params->shaper_profile_id);
		if (!profile)
			return -rte_tm_error_set(error, ENODEV,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL, "Shaper id does not exist\n");
	}

	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		LIST_FOREACH(node, &priv->nodes, next) {
			if (node->type != MRVL_NODE_PORT)
				continue;

			return -rte_tm_error_set(error, EINVAL,
						 RTE_TM_ERROR_TYPE_UNSPECIFIED,
						 NULL, "Root node exists\n");
		}
	} else {
		parent = mrvl_node_from_id(priv, parent_node_id);
		if (!parent)
			return -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL, "Node id does not exist\n");
	}

	node = mrvl_node_from_id(priv, node_id);
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
	node->type = parent_node_id == RTE_TM_NODE_ID_NULL ? MRVL_NODE_PORT :
							     MRVL_NODE_QUEUE;

	if (parent) {
		node->parent = parent;
		parent->refcnt++;
	}

	if (profile) {
		node->profile = profile;
		profile->refcnt++;
	}

	node->weight = weight;
	node->stats_mask = params->stats_mask;

	LIST_INSERT_HEAD(&priv->nodes, node, next);

	return 0;
}

/**
 * Delete a node.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		 struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;

	if (priv->ppio) {
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Port is already started\n");
	}

	node = mrvl_node_from_id(priv, node_id);
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

/**
 * Helper for suspending specific tx queue.
 *
 * @param dev Pointer to the device.
 * @param node_id Id used by this node.
 * @returns 0 on success, negative value otherwise.
 */
static int mrvl_node_suspend_one(struct rte_eth_dev *dev, uint32_t node_id,
				 struct rte_tm_error *error)
{
	int ret = dev->dev_ops->tx_queue_stop(dev, node_id);
	if (ret)
		return -rte_tm_error_set(error, ret,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Failed to suspend a txq\n");

	return 0;
}

/**
 * Suspend a node.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param error Pointer to the error.
 * returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_suspend(struct rte_eth_dev *dev, uint32_t node_id,
		  struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node, *tmp;
	int ret;

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	if (!node->parent) {
		LIST_FOREACH(tmp, &priv->nodes, next) {
			if (!tmp->parent)
				continue;

			if (node != tmp->parent)
				continue;

			ret = mrvl_node_suspend_one(dev, tmp->id, error);
			if (ret)
				return ret;
		}

		return 0;
	}

	return mrvl_node_suspend_one(dev, node_id, error);
}

/**
 * Resume a node.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param error Pointer to the error.
 * returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_resume(struct rte_eth_dev *dev, uint32_t node_id,
		 struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;
	int ret;

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");


	if (!node->parent)
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Cannot suspend a port\n");

	ret = dev->dev_ops->tx_queue_start(dev, node_id);
	if (ret)
		return -rte_tm_error_set(error, ret,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Failed to resume a txq\n");
	return 0;
}

/**
 * Apply traffic manager hierarchy.
 *
 * @param dev Pointer to the device.
 * @param clear_on_fail Flag indicating whether to do cleanup on the failure.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_hierarchy_commit(struct rte_eth_dev *dev, int clear_on_fail,
		      struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;
	int ret;

	if (priv->ppio) {
		ret = -rte_tm_error_set(error, EPERM,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL, "Port is already started\n");
		goto out;
	}

	LIST_FOREACH(node, &priv->nodes, next) {
		struct pp2_ppio_outq_params *p;

		if (node->type == MRVL_NODE_PORT) {
			if (!node->profile)
				continue;

			priv->ppio_params.rate_limit_enable = 1;
			priv->ppio_params.rate_limit_params.cir =
				node->profile->params.peak.rate * 8 / 1000;
			priv->ppio_params.rate_limit_params.cbs =
				node->profile->params.peak.size / 1000;

			MRVL_LOG(INFO,
				"Port rate limit overrides txqs rate limit");

			continue;
		}

		if (node->id >= dev->data->nb_tx_queues) {
			ret = -rte_tm_error_set(error, EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID, NULL,
					"Not enough txqs are configured\n");
			goto out;
		}

		p = &priv->ppio_params.outqs_params.outqs_params[node->id];

		if (node->weight) {
			p->sched_mode = PP2_PPIO_SCHED_M_WRR;
			p->weight = node->weight;
		} else {
			p->sched_mode = PP2_PPIO_SCHED_M_SP;
			p->weight = 0;
		}

		if (node->profile) {
			p->rate_limit_enable = 1;
			/* convert Bytes/s to kilo bits/s */
			p->rate_limit_params.cir =
				node->profile->params.peak.rate * 8 / 1000;
			/* convert bits to kilo bits */
			p->rate_limit_params.cbs =
				node->profile->params.peak.size / 1000;
		} else {
			p->rate_limit_enable = 0;
			p->rate_limit_params.cir = 0;
			p->rate_limit_params.cbs = 0;
		}
	}

	/* reset to defaults in case applied tm hierarchy is empty */
	if (LIST_EMPTY(&priv->nodes)) {
		int i;

		for (i = 0; i < priv->ppio_params.outqs_params.num_outqs; i++) {
			struct pp2_ppio_outq_params *p =
				&priv->ppio_params.outqs_params.outqs_params[i];

			p->sched_mode = PP2_PPIO_SCHED_M_WRR;
			p->weight = 0;
			p->rate_limit_enable = 0;
			p->rate_limit_params.cir = 0;
			p->rate_limit_params.cbs = 0;
		}
	}

	return 0;
out:
	if (clear_on_fail) {
		mrvl_tm_deinit(dev);
		mrvl_tm_init(dev);
	}

	return ret;
}

/**
 * Read statistics counters for current node.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param stats Pointer to the statistics counters.
 * @param stats_mask Pointer to mask of enabled statistics counters
 *                   that are retrieved.
 * @param clear Flag indicating whether to clear statistics.
 *              Non-zero value clears statistics.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_stats_read(struct rte_eth_dev *dev, uint32_t node_id,
		     struct rte_tm_node_stats *stats, uint64_t *stats_mask,
		     int clear, struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;
	int ret;

	if (!priv->ppio) {
		return -rte_tm_error_set(error, EPERM,
					 RTE_TM_ERROR_TYPE_UNSPECIFIED,
					 NULL, "Port is not started\n");
	}

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	if (stats_mask)
		*stats_mask = node->stats_mask;

	if (!stats)
		return 0;

	memset(stats, 0, sizeof(*stats));

	if (!node->parent) {
		struct pp2_ppio_statistics s;

		memset(&s, 0, sizeof(s));
		ret = pp2_ppio_get_statistics(priv->ppio, &s, clear);
		if (ret)
			return -rte_tm_error_set(error, -ret,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to read port statistics\n");

		if (node->stats_mask & RTE_TM_STATS_N_PKTS)
			stats->n_pkts = s.tx_packets;

		if (node->stats_mask & RTE_TM_STATS_N_BYTES)
			stats->n_bytes = s.tx_bytes;
	} else {
		struct pp2_ppio_outq_statistics s;

		memset(&s, 0, sizeof(s));
		ret = pp2_ppio_outq_get_statistics(priv->ppio, node_id, &s,
						   clear);
		if (ret)
			return -rte_tm_error_set(error, -ret,
					RTE_TM_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to read txq statistics\n");

		if (node->stats_mask & RTE_TM_STATS_N_PKTS)
			stats->n_pkts = s.deq_desc;
	}

	return 0;
}

/**
 * Update node statistics.
 *
 * @param dev Pointer to the device.
 * @param node_id Id of the node.
 * @param stats_mask Bitmask of statistics counters to be enabled.
 * @param error Pointer to the error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_node_stats_update(struct rte_eth_dev *dev, uint32_t node_id,
		       uint64_t stats_mask, struct rte_tm_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_tm_node *node;

	node = mrvl_node_from_id(priv, node_id);
	if (!node)
		return -rte_tm_error_set(error, ENODEV,
					 RTE_TM_ERROR_TYPE_NODE_ID,
					 NULL, "Node id does not exist\n");

	if (!node->parent) {
		if (stats_mask & ~(RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES))
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested port stats are not supported\n");
	} else {
		if (stats_mask & ~RTE_TM_STATS_N_PKTS)
			return -rte_tm_error_set(error, EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
				NULL,
				"Requested txq stats are not supported\n");
	}

	node->stats_mask = stats_mask;

	return 0;
}

const struct rte_tm_ops mrvl_tm_ops = {
	.node_type_get = mrvl_node_type_get,
	.capabilities_get = mrvl_capabilities_get,
	.level_capabilities_get = mrvl_level_capabilities_get,
	.node_capabilities_get = mrvl_node_capabilities_get,
	.shaper_profile_add = mrvl_shaper_profile_add,
	.shaper_profile_delete = mrvl_shaper_profile_delete,
	.node_add = mrvl_node_add,
	.node_delete = mrvl_node_delete,
	.node_suspend = mrvl_node_suspend,
	.node_resume = mrvl_node_resume,
	.hierarchy_commit = mrvl_hierarchy_commit,
	.node_stats_update = mrvl_node_stats_update,
	.node_stats_read = mrvl_node_stats_read,
};
