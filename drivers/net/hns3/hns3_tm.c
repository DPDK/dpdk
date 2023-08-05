/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#include <rte_malloc.h>

#include "hns3_common.h"
#include "hns3_dcb.h"
#include "hns3_logs.h"
#include "hns3_tm.h"

static inline uint32_t
hns3_tm_max_tx_queues_get(struct rte_eth_dev *dev)
{
	/*
	 * This API will called in pci device probe stage, we can't call
	 * rte_eth_dev_info_get to get max_tx_queues (due to rte_eth_devices
	 * not setup), so we call the hns3_dev_infos_get.
	 */
	struct rte_eth_dev_info dev_info;

	memset(&dev_info, 0, sizeof(dev_info));
	(void)hns3_dev_infos_get(dev, &dev_info);
	return RTE_MIN(dev_info.max_tx_queues, RTE_MAX_QUEUES_PER_PORT);
}

void
hns3_tm_conf_init(struct rte_eth_dev *dev)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t max_tx_queues = hns3_tm_max_tx_queues_get(dev);

	if (!hns3_dev_get_support(hw, TM))
		return;

	pf->tm_conf.nb_leaf_nodes_max = max_tx_queues;
	pf->tm_conf.nb_nodes_max = 1 + HNS3_MAX_TC_NUM + max_tx_queues;
	pf->tm_conf.nb_shaper_profile_max = 1 + HNS3_MAX_TC_NUM;

	TAILQ_INIT(&pf->tm_conf.shaper_profile_list);
	pf->tm_conf.nb_shaper_profile = 0;

	pf->tm_conf.root = NULL;
	TAILQ_INIT(&pf->tm_conf.tc_list);
	TAILQ_INIT(&pf->tm_conf.queue_list);
	pf->tm_conf.nb_tc_node = 0;
	pf->tm_conf.nb_queue_node = 0;

	pf->tm_conf.committed = false;
}

void
hns3_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tm_shaper_profile *shaper_profile;
	struct hns3_tm_node *tm_node;

	if (!hns3_dev_get_support(hw, TM))
		return;

	if (pf->tm_conf.nb_queue_node > 0) {
		while ((tm_node = TAILQ_FIRST(&pf->tm_conf.queue_list))) {
			TAILQ_REMOVE(&pf->tm_conf.queue_list, tm_node, node);
			rte_free(tm_node);
		}
		pf->tm_conf.nb_queue_node = 0;
	}

	if (pf->tm_conf.nb_tc_node > 0) {
		while ((tm_node = TAILQ_FIRST(&pf->tm_conf.tc_list))) {
			TAILQ_REMOVE(&pf->tm_conf.tc_list, tm_node, node);
			rte_free(tm_node);
		}
		pf->tm_conf.nb_tc_node = 0;
	}

	if (pf->tm_conf.root != NULL) {
		rte_free(pf->tm_conf.root);
		pf->tm_conf.root = NULL;
	}

	if (pf->tm_conf.nb_shaper_profile > 0) {
		while ((shaper_profile =
		       TAILQ_FIRST(&pf->tm_conf.shaper_profile_list))) {
			TAILQ_REMOVE(&pf->tm_conf.shaper_profile_list,
				     shaper_profile, node);
			rte_free(shaper_profile);
		}
		pf->tm_conf.nb_shaper_profile = 0;
	}

	pf->tm_conf.nb_leaf_nodes_max = 0;
	pf->tm_conf.nb_nodes_max = 0;
	pf->tm_conf.nb_shaper_profile_max = 0;
}

static inline uint64_t
hns3_tm_rate_convert_firmware2tm(uint32_t firmware_rate)
{
#define FIRMWARE_TO_TM_RATE_SCALE	125000
	/* tm rate unit is Bps, firmware rate is Mbps */
	return ((uint64_t)firmware_rate) * FIRMWARE_TO_TM_RATE_SCALE;
}

static inline uint32_t
hns3_tm_rate_convert_tm2firmware(uint64_t tm_rate)
{
#define TM_TO_FIRMWARE_RATE_SCALE	125000
	/* tm rate unit is Bps, firmware rate is Mbps */
	return (uint32_t)(tm_rate / TM_TO_FIRMWARE_RATE_SCALE);
}

static int
hns3_tm_capabilities_get(struct rte_eth_dev *dev,
			 struct rte_tm_capabilities *cap,
			 struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t max_tx_queues = hns3_tm_max_tx_queues_get(dev);

	if (cap == NULL || error == NULL)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	cap->n_nodes_max = 1 + HNS3_MAX_TC_NUM + max_tx_queues;
	cap->n_levels_max = HNS3_TM_NODE_LEVEL_MAX;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = 1 + HNS3_MAX_TC_NUM;
	cap->shaper_private_n_max = 1 + HNS3_MAX_TC_NUM;
	cap->shaper_private_rate_max =
		hns3_tm_rate_convert_firmware2tm(hw->max_tm_rate);

	cap->sched_n_children_max = max_tx_queues;
	cap->sched_sp_n_priorities_max = 1;
	cap->sched_wfq_weight_max = 1;

	cap->shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD;
	cap->shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	return 0;
}

static struct hns3_tm_shaper_profile *
hns3_tm_shaper_profile_search(struct rte_eth_dev *dev,
			      uint32_t shaper_profile_id)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_shaper_profile_list *shaper_profile_list =
		&pf->tm_conf.shaper_profile_list;
	struct hns3_tm_shaper_profile *shaper_profile;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (shaper_profile_id == shaper_profile->shaper_profile_id)
			return shaper_profile;
	}

	return NULL;
}

static int
hns3_tm_shaper_profile_param_check(struct rte_eth_dev *dev,
				   struct rte_tm_shaper_params *profile,
				   struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (profile->committed.rate) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		error->message = "committed rate not supported";
		return -EINVAL;
	}

	if (profile->committed.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
		error->message = "committed bucket size not supported";
		return -EINVAL;
	}

	if (profile->peak.rate >
	    hns3_tm_rate_convert_firmware2tm(hw->max_tm_rate)) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE;
		error->message = "peak rate too large";
		return -EINVAL;
	}

	if (profile->peak.rate < hns3_tm_rate_convert_firmware2tm(1)) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE;
		error->message = "peak rate must be at least 1Mbps";
		return -EINVAL;
	}

	if (profile->peak.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
		error->message = "peak bucket size not supported";
		return -EINVAL;
	}

	if (profile->pkt_length_adjust) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN;
		error->message = "packet length adjustment not supported";
		return -EINVAL;
	}

	if (profile->packet_mode) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PACKET_MODE;
		error->message = "packet mode not supported";
		return -EINVAL;
	}

	return 0;
}

static int
hns3_tm_shaper_profile_add(struct rte_eth_dev *dev,
			   uint32_t shaper_profile_id,
			   struct rte_tm_shaper_params *profile,
			   struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_shaper_profile *shaper_profile;
	int ret;

	if (profile == NULL || error == NULL)
		return -EINVAL;

	if (pf->tm_conf.nb_shaper_profile >=
	    pf->tm_conf.nb_shaper_profile_max) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "too much profiles";
		return -EINVAL;
	}

	ret = hns3_tm_shaper_profile_param_check(dev, profile, error);
	if (ret)
		return ret;

	shaper_profile = hns3_tm_shaper_profile_search(dev, shaper_profile_id);
	if (shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		return -EINVAL;
	}

	shaper_profile = rte_zmalloc("hns3_tm_shaper_profile",
				     sizeof(struct hns3_tm_shaper_profile),
				     0);
	if (shaper_profile == NULL)
		return -ENOMEM;

	shaper_profile->shaper_profile_id = shaper_profile_id;
	memcpy(&shaper_profile->profile, profile,
	       sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&pf->tm_conf.shaper_profile_list,
			  shaper_profile, node);
	pf->tm_conf.nb_shaper_profile++;

	return 0;
}

static int
hns3_tm_shaper_profile_del(struct rte_eth_dev *dev,
			   uint32_t shaper_profile_id,
			   struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_shaper_profile *shaper_profile;

	if (error == NULL)
		return -EINVAL;

	shaper_profile = hns3_tm_shaper_profile_search(dev, shaper_profile_id);
	if (shaper_profile == NULL) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID not exist";
		return -EINVAL;
	}

	if (shaper_profile->reference_count) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "profile in use";
		return -EINVAL;
	}

	TAILQ_REMOVE(&pf->tm_conf.shaper_profile_list, shaper_profile, node);
	rte_free(shaper_profile);
	pf->tm_conf.nb_shaper_profile--;

	return 0;
}

static struct hns3_tm_node *
hns3_tm_node_search(struct rte_eth_dev *dev,
		    uint32_t node_id,
		    enum hns3_tm_node_type *node_type)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_node_list *queue_list = &pf->tm_conf.queue_list;
	struct hns3_tm_node_list *tc_list = &pf->tm_conf.tc_list;
	struct hns3_tm_node *tm_node;

	if (pf->tm_conf.root && pf->tm_conf.root->id == node_id) {
		*node_type = HNS3_TM_NODE_TYPE_PORT;
		return pf->tm_conf.root;
	}

	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (tm_node->id == node_id) {
			*node_type = HNS3_TM_NODE_TYPE_TC;
			return tm_node;
		}
	}

	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->id == node_id) {
			*node_type = HNS3_TM_NODE_TYPE_QUEUE;
			return tm_node;
		}
	}

	return NULL;
}

static int
hns3_tm_nonleaf_node_param_check(struct rte_eth_dev *dev,
				 struct rte_tm_node_params *params,
				 struct rte_tm_error *error)
{
	struct hns3_tm_shaper_profile *shaper_profile;

	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		shaper_profile = hns3_tm_shaper_profile_search(dev,
				 params->shaper_profile_id);
		if (shaper_profile == NULL) {
			error->type =
				RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
			error->message = "shaper profile not exist";
			return -EINVAL;
		}
	}

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
	}

	return 0;
}

static int
hns3_tm_leaf_node_param_check(struct rte_eth_dev *dev __rte_unused,
			      struct rte_tm_node_params *params,
			      struct rte_tm_error *error)

{
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		error->type =
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
		error->message = "shaper not supported";
		return -EINVAL;
	}

	if (params->leaf.cman != RTE_TM_CMAN_TAIL_DROP) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN;
		error->message = "congestion management not supported";
		return -EINVAL;
	}

	if (params->leaf.wred.wred_profile_id != RTE_TM_WRED_PROFILE_ID_NONE) {
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
hns3_tm_node_param_check(struct rte_eth_dev *dev, uint32_t node_id,
			 uint32_t priority, uint32_t weight,
			 struct rte_tm_node_params *params,
			 struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_tm_node_type node_type = HNS3_TM_NODE_TYPE_MAX;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	if (hns3_tm_node_search(dev, node_id, &node_type)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
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

	if (node_id >= pf->tm_conf.nb_leaf_nodes_max)
		return hns3_tm_nonleaf_node_param_check(dev, params, error);
	else
		return hns3_tm_leaf_node_param_check(dev, params, error);
}

static int
hns3_tm_port_node_add(struct rte_eth_dev *dev, uint32_t node_id,
		      uint32_t level_id, struct rte_tm_node_params *params,
		      struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_node *tm_node;

	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != HNS3_TM_NODE_LEVEL_PORT) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "wrong level";
		return -EINVAL;
	}

	if (node_id != pf->tm_conf.nb_nodes_max - 1) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid port node ID";
		return -EINVAL;
	}

	if (pf->tm_conf.root) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "already have a root";
		return -EINVAL;
	}

	tm_node = rte_zmalloc("hns3_tm_node", sizeof(struct hns3_tm_node), 0);
	if (tm_node == NULL)
		return -ENOMEM;

	tm_node->id = node_id;
	tm_node->reference_count = 0;
	tm_node->parent = NULL;
	tm_node->shaper_profile = hns3_tm_shaper_profile_search(dev,
				  params->shaper_profile_id);
	memcpy(&tm_node->params, params, sizeof(struct rte_tm_node_params));
	pf->tm_conf.root = tm_node;

	if (tm_node->shaper_profile)
		tm_node->shaper_profile->reference_count++;

	return 0;
}

static int
hns3_tm_tc_node_add(struct rte_eth_dev *dev, uint32_t node_id,
		    uint32_t level_id, struct hns3_tm_node *parent_node,
		    struct rte_tm_node_params *params,
		    struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_node *tm_node;

	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != HNS3_TM_NODE_LEVEL_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "wrong level";
		return -EINVAL;
	}

	if (node_id >= pf->tm_conf.nb_nodes_max - 1 ||
	    node_id < pf->tm_conf.nb_leaf_nodes_max ||
	    hns3_tm_calc_node_tc_no(&pf->tm_conf, node_id) >= hw->num_tc) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid tc node ID";
		return -EINVAL;
	}

	if (pf->tm_conf.nb_tc_node >= hw->num_tc) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "too many TCs";
		return -EINVAL;
	}

	tm_node = rte_zmalloc("hns3_tm_node", sizeof(struct hns3_tm_node), 0);
	if (tm_node == NULL)
		return -ENOMEM;

	tm_node->id = node_id;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	tm_node->shaper_profile = hns3_tm_shaper_profile_search(dev,
					params->shaper_profile_id);
	memcpy(&tm_node->params, params, sizeof(struct rte_tm_node_params));
	TAILQ_INSERT_TAIL(&pf->tm_conf.tc_list, tm_node, node);
	pf->tm_conf.nb_tc_node++;
	tm_node->parent->reference_count++;

	if (tm_node->shaper_profile)
		tm_node->shaper_profile->reference_count++;

	return 0;
}

static int
hns3_tm_queue_node_add(struct rte_eth_dev *dev, uint32_t node_id,
		       uint32_t level_id, struct hns3_tm_node *parent_node,
		       struct rte_tm_node_params *params,
		       struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_node *tm_node;

	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY &&
	    level_id != HNS3_TM_NODE_LEVEL_QUEUE) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "wrong level";
		return -EINVAL;
	}

	/* note: dev->data->nb_tx_queues <= max_tx_queues */
	if (node_id >= dev->data->nb_tx_queues) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid queue node ID";
		return -EINVAL;
	}

	if (hns3_txq_mapped_tc_get(hw, node_id) !=
	    hns3_tm_calc_node_tc_no(&pf->tm_conf, parent_node->id)) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "queue's TC not match parent's TC";
		return -EINVAL;
	}

	tm_node = rte_zmalloc("hns3_tm_node", sizeof(struct hns3_tm_node), 0);
	if (tm_node == NULL)
		return -ENOMEM;

	tm_node->id = node_id;
	tm_node->reference_count = 0;
	tm_node->parent = parent_node;
	memcpy(&tm_node->params, params, sizeof(struct rte_tm_node_params));
	TAILQ_INSERT_TAIL(&pf->tm_conf.queue_list, tm_node, node);
	pf->tm_conf.nb_queue_node++;
	tm_node->parent->reference_count++;

	return 0;
}

static int
hns3_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id,
		 uint32_t parent_node_id, uint32_t priority,
		 uint32_t weight, uint32_t level_id,
		 struct rte_tm_node_params *params,
		 struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_tm_node_type parent_node_type = HNS3_TM_NODE_TYPE_MAX;
	struct hns3_tm_node *parent_node;
	int ret;

	if (params == NULL || error == NULL)
		return -EINVAL;

	if (pf->tm_conf.committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	ret = hns3_tm_node_param_check(dev, node_id, priority, weight,
				       params, error);
	if (ret)
		return ret;

	/* root node who don't have a parent */
	if (parent_node_id == RTE_TM_NODE_ID_NULL)
		return hns3_tm_port_node_add(dev, node_id, level_id,
					     params, error);

	parent_node = hns3_tm_node_search(dev, parent_node_id,
					  &parent_node_type);
	if (parent_node == NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		return -EINVAL;
	}

	if (parent_node_type != HNS3_TM_NODE_TYPE_PORT &&
	    parent_node_type != HNS3_TM_NODE_TYPE_TC) {
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent is not port or TC";
		return -EINVAL;
	}

	if (parent_node_type == HNS3_TM_NODE_TYPE_PORT)
		return hns3_tm_tc_node_add(dev, node_id, level_id,
					   parent_node, params, error);
	else
		return hns3_tm_queue_node_add(dev, node_id, level_id,
					      parent_node, params, error);
}

static void
hns3_tm_node_do_delete(struct hns3_pf *pf,
		       enum hns3_tm_node_type node_type,
		       struct hns3_tm_node *tm_node)
{
	if (node_type == HNS3_TM_NODE_TYPE_PORT) {
		if (tm_node->shaper_profile)
			tm_node->shaper_profile->reference_count--;
		rte_free(tm_node);
		pf->tm_conf.root = NULL;
		return;
	}

	if (tm_node->shaper_profile)
		tm_node->shaper_profile->reference_count--;
	tm_node->parent->reference_count--;
	if (node_type == HNS3_TM_NODE_TYPE_TC) {
		TAILQ_REMOVE(&pf->tm_conf.tc_list, tm_node, node);
		pf->tm_conf.nb_tc_node--;
	} else {
		TAILQ_REMOVE(&pf->tm_conf.queue_list, tm_node, node);
		pf->tm_conf.nb_queue_node--;
	}
	rte_free(tm_node);
}

static int
hns3_tm_node_delete(struct rte_eth_dev *dev,
		    uint32_t node_id,
		    struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_tm_node_type node_type = HNS3_TM_NODE_TYPE_MAX;
	struct hns3_tm_node *tm_node;

	if (error == NULL)
		return -EINVAL;

	if (pf->tm_conf.committed) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "already committed";
		return -EINVAL;
	}

	tm_node = hns3_tm_node_search(dev, node_id, &node_type);
	if (tm_node == NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (tm_node->reference_count) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "cannot delete a node which has children";
		return -EINVAL;
	}

	hns3_tm_node_do_delete(pf, node_type, tm_node);

	return 0;
}

static int
hns3_tm_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		      int *is_leaf, struct rte_tm_error *error)
{
	enum hns3_tm_node_type node_type = HNS3_TM_NODE_TYPE_MAX;
	struct hns3_tm_node *tm_node;

	if (is_leaf == NULL || error == NULL)
		return -EINVAL;

	tm_node = hns3_tm_node_search(dev, node_id, &node_type);
	if (tm_node == NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (node_type == HNS3_TM_NODE_TYPE_QUEUE)
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static void
hns3_tm_nonleaf_level_capsbilities_get(struct rte_eth_dev *dev,
				       uint32_t level_id,
				       struct rte_tm_level_capabilities *cap)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t max_tx_queues = hns3_tm_max_tx_queues_get(dev);

	if (level_id == HNS3_TM_NODE_LEVEL_PORT) {
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->n_nodes_leaf_max = 0;
	} else {
		cap->n_nodes_max = HNS3_MAX_TC_NUM;
		cap->n_nodes_nonleaf_max = HNS3_MAX_TC_NUM;
		cap->n_nodes_leaf_max = 0;
	}

	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	cap->nonleaf.shaper_private_supported = true;
	cap->nonleaf.shaper_private_dual_rate_supported = false;
	cap->nonleaf.shaper_private_rate_min = 0;
	cap->nonleaf.shaper_private_rate_max =
		hns3_tm_rate_convert_firmware2tm(hw->max_tm_rate);
	cap->nonleaf.shaper_shared_n_max = 0;
	if (level_id == HNS3_TM_NODE_LEVEL_PORT)
		cap->nonleaf.sched_n_children_max = HNS3_MAX_TC_NUM;
	else
		cap->nonleaf.sched_n_children_max = max_tx_queues;
	cap->nonleaf.sched_sp_n_priorities_max = 1;
	cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
	cap->nonleaf.sched_wfq_n_groups_max = 0;
	cap->nonleaf.sched_wfq_weight_max = 1;
	cap->nonleaf.stats_mask = 0;
}

static void
hns3_tm_leaf_level_capabilities_get(struct rte_eth_dev *dev,
				    struct rte_tm_level_capabilities *cap)
{
	uint32_t max_tx_queues = hns3_tm_max_tx_queues_get(dev);

	cap->n_nodes_max = max_tx_queues;
	cap->n_nodes_nonleaf_max = 0;
	cap->n_nodes_leaf_max = max_tx_queues;

	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	cap->leaf.shaper_private_supported = false;
	cap->leaf.shaper_private_dual_rate_supported = false;
	cap->leaf.shaper_private_rate_min = 0;
	cap->leaf.shaper_private_rate_max = 0;
	cap->leaf.shaper_shared_n_max = 0;
	cap->leaf.cman_head_drop_supported = false;
	cap->leaf.cman_wred_context_private_supported = false;
	cap->leaf.cman_wred_context_shared_n_max = 0;
	cap->leaf.stats_mask = 0;
}

static int
hns3_tm_level_capabilities_get(struct rte_eth_dev *dev,
			       uint32_t level_id,
			       struct rte_tm_level_capabilities *cap,
			       struct rte_tm_error *error)
{
	if (cap == NULL || error == NULL)
		return -EINVAL;

	if (level_id >= HNS3_TM_NODE_LEVEL_MAX) {
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		return -EINVAL;
	}

	memset(cap, 0, sizeof(struct rte_tm_level_capabilities));

	if (level_id != HNS3_TM_NODE_LEVEL_QUEUE)
		hns3_tm_nonleaf_level_capsbilities_get(dev, level_id, cap);
	else
		hns3_tm_leaf_level_capabilities_get(dev, cap);

	return 0;
}

static void
hns3_tm_nonleaf_node_capabilities_get(struct rte_eth_dev *dev,
				      enum hns3_tm_node_type node_type,
				      struct rte_tm_node_capabilities *cap)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t max_tx_queues = hns3_tm_max_tx_queues_get(dev);

	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported = false;
	cap->shaper_private_rate_min = 0;
	cap->shaper_private_rate_max =
		hns3_tm_rate_convert_firmware2tm(hw->max_tm_rate);
	cap->shaper_shared_n_max = 0;

	if (node_type == HNS3_TM_NODE_TYPE_PORT)
		cap->nonleaf.sched_n_children_max = HNS3_MAX_TC_NUM;
	else
		cap->nonleaf.sched_n_children_max = max_tx_queues;
	cap->nonleaf.sched_sp_n_priorities_max = 1;
	cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
	cap->nonleaf.sched_wfq_n_groups_max = 0;
	cap->nonleaf.sched_wfq_weight_max = 1;

	cap->stats_mask = 0;
}

static void
hns3_tm_leaf_node_capabilities_get(struct rte_eth_dev *dev __rte_unused,
				   struct rte_tm_node_capabilities *cap)
{
	cap->shaper_private_supported = false;
	cap->shaper_private_dual_rate_supported = false;
	cap->shaper_private_rate_min = 0;
	cap->shaper_private_rate_max = 0;
	cap->shaper_shared_n_max = 0;

	cap->leaf.cman_head_drop_supported = false;
	cap->leaf.cman_wred_context_private_supported = false;
	cap->leaf.cman_wred_context_shared_n_max = 0;

	cap->stats_mask = 0;
}

static int
hns3_tm_node_capabilities_get(struct rte_eth_dev *dev,
			      uint32_t node_id,
			      struct rte_tm_node_capabilities *cap,
			      struct rte_tm_error *error)
{
	enum hns3_tm_node_type node_type;
	struct hns3_tm_node *tm_node;

	if (cap == NULL || error == NULL)
		return -EINVAL;

	tm_node = hns3_tm_node_search(dev, node_id, &node_type);
	if (tm_node == NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	memset(cap, 0, sizeof(struct rte_tm_node_capabilities));

	if (node_type != HNS3_TM_NODE_TYPE_QUEUE)
		hns3_tm_nonleaf_node_capabilities_get(dev, node_type, cap);
	else
		hns3_tm_leaf_node_capabilities_get(dev, cap);

	return 0;
}

static int
hns3_tm_config_port_rate(struct hns3_hw *hw,
			 struct hns3_tm_shaper_profile *shaper_profile)
{
	struct hns3_port_limit_rate_cmd *cfg;
	struct hns3_cmd_desc desc;
	uint32_t firmware_rate;
	uint64_t rate;
	int ret;

	if (shaper_profile) {
		rate = shaper_profile->profile.peak.rate;
		firmware_rate = hns3_tm_rate_convert_tm2firmware(rate);
	} else {
		firmware_rate = hw->max_tm_rate;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PORT_LIMIT_RATE, false);
	cfg = (struct hns3_port_limit_rate_cmd *)desc.data;
	cfg->speed = rte_cpu_to_le_32(firmware_rate);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "failed to config port rate, ret = %d", ret);

	return ret;
}

static int
hns3_tm_config_tc_rate(struct hns3_hw *hw, uint8_t tc_no,
		       struct hns3_tm_shaper_profile *shaper_profile)
{
	struct hns3_tc_limit_rate_cmd *cfg;
	struct hns3_cmd_desc desc;
	uint32_t firmware_rate;
	uint64_t rate;
	int ret;

	if (shaper_profile) {
		rate = shaper_profile->profile.peak.rate;
		firmware_rate = hns3_tm_rate_convert_tm2firmware(rate);
	} else {
		firmware_rate = hw->dcb_info.tc_info[tc_no].bw_limit;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_TC_LIMIT_RATE, false);
	cfg = (struct hns3_tc_limit_rate_cmd *)desc.data;
	cfg->speed = rte_cpu_to_le_32(firmware_rate);
	cfg->tc_id = tc_no;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "failed to config tc (%u) rate, ret = %d",
			 tc_no, ret);

	return ret;
}

static bool
hns3_tm_configure_check(struct hns3_hw *hw, struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	struct hns3_tm_conf *tm_conf = &pf->tm_conf;
	struct hns3_tm_node_list *tc_list = &tm_conf->tc_list;
	struct hns3_tm_node_list *queue_list = &tm_conf->queue_list;
	struct hns3_tm_node *tm_node;

	/* TC */
	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (!tm_node->reference_count) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "TC without queue assigned";
			return false;
		}

		if (hns3_tm_calc_node_tc_no(tm_conf, tm_node->id) >=
			hw->num_tc) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "node's TC not exist";
			return false;
		}
	}

	/* Queue */
	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->id >= hw->data->nb_tx_queues) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "node's queue invalid";
			return false;
		}

		if (hns3_txq_mapped_tc_get(hw, tm_node->id) !=
		    hns3_tm_calc_node_tc_no(tm_conf, tm_node->parent->id)) {
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "queue's TC not match parent's TC";
			return false;
		}
	}

	return true;
}

static int
hns3_tm_hierarchy_do_commit(struct hns3_hw *hw,
			    struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	struct hns3_tm_node_list *tc_list = &pf->tm_conf.tc_list;
	struct hns3_tm_node *tm_node;
	uint8_t tc_no;
	int ret;

	/* port */
	tm_node = pf->tm_conf.root;
	if (tm_node->shaper_profile) {
		ret = hns3_tm_config_port_rate(hw, tm_node->shaper_profile);
		if (ret) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
			error->message = "fail to set port peak rate";
			return -EIO;
		}
	}

	/* TC */
	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (tm_node->shaper_profile == NULL)
			continue;

		tc_no = hns3_tm_calc_node_tc_no(&pf->tm_conf, tm_node->id);
		ret = hns3_tm_config_tc_rate(hw, tc_no,
					     tm_node->shaper_profile);
		if (ret) {
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "fail to set TC peak rate";
			return -EIO;
		}
	}

	return 0;
}

static int
hns3_tm_hierarchy_commit(struct rte_eth_dev *dev,
			 int clear_on_fail,
			 struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	if (error == NULL)
		return -EINVAL;

	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED)) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "device is resetting";
		/* don't goto fail_clear, user may try later */
		return -EBUSY;
	}

	if (pf->tm_conf.root == NULL)
		goto done;

	/* check configure before commit make sure key configure not violated */
	if (!hns3_tm_configure_check(hw, error))
		goto fail_clear;

	ret = hns3_tm_hierarchy_do_commit(hw, error);
	if (ret)
		goto fail_clear;

done:
	pf->tm_conf.committed = true;
	return 0;

fail_clear:
	if (clear_on_fail) {
		hns3_tm_conf_uninit(dev);
		hns3_tm_conf_init(dev);
	}
	return -EINVAL;
}

static int
hns3_tm_node_shaper_do_update(struct hns3_hw *hw,
			      uint32_t node_id,
			      enum hns3_tm_node_type node_type,
			      struct hns3_tm_shaper_profile *shaper_profile,
			      struct rte_tm_error *error)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	uint8_t tc_no;
	int ret;

	if (node_type == HNS3_TM_NODE_TYPE_QUEUE) {
		if (shaper_profile != NULL) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
			error->message = "queue node shaper not supported";
			return -EINVAL;
		}
		return 0;
	}

	if (!pf->tm_conf.committed)
		return 0;

	if (node_type == HNS3_TM_NODE_TYPE_PORT) {
		ret = hns3_tm_config_port_rate(hw, shaper_profile);
		if (ret) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
			error->message = "fail to update port peak rate";
		}

		return ret;
	}

	/*
	 * update TC's shaper
	 */
	tc_no = hns3_tm_calc_node_tc_no(&pf->tm_conf, node_id);
	ret = hns3_tm_config_tc_rate(hw, tc_no, shaper_profile);
	if (ret) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "fail to update TC peak rate";
	}

	return ret;
}

static int
hns3_tm_node_shaper_update(struct rte_eth_dev *dev,
			   uint32_t node_id,
			   uint32_t shaper_profile_id,
			   struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	enum hns3_tm_node_type node_type = HNS3_TM_NODE_TYPE_MAX;
	struct hns3_tm_shaper_profile *profile = NULL;
	struct hns3_tm_node *tm_node;

	if (error == NULL)
		return -EINVAL;

	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED)) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "device is resetting";
		return -EBUSY;
	}

	tm_node = hns3_tm_node_search(dev, node_id, &node_type);
	if (tm_node == NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (shaper_profile_id == tm_node->params.shaper_profile_id)
		return 0;

	if (shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE) {
		profile = hns3_tm_shaper_profile_search(dev, shaper_profile_id);
		if (profile == NULL) {
			error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
			error->message = "profile ID not exist";
			return -EINVAL;
		}
	}

	if (hns3_tm_node_shaper_do_update(hw, node_id, node_type,
					  profile, error))
		return -EINVAL;

	if (tm_node->shaper_profile)
		tm_node->shaper_profile->reference_count--;
	tm_node->shaper_profile = profile;
	tm_node->params.shaper_profile_id = shaper_profile_id;
	if (profile != NULL)
		profile->reference_count++;

	return 0;
}

static int
hns3_tm_capabilities_get_wrap(struct rte_eth_dev *dev,
			      struct rte_tm_capabilities *cap,
			      struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_capabilities_get(dev, cap, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_shaper_profile_add_wrap(struct rte_eth_dev *dev,
				uint32_t shaper_profile_id,
				struct rte_tm_shaper_params *profile,
				struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_shaper_profile_add(dev, shaper_profile_id, profile, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_shaper_profile_del_wrap(struct rte_eth_dev *dev,
				uint32_t shaper_profile_id,
				struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_shaper_profile_del(dev, shaper_profile_id, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_node_add_wrap(struct rte_eth_dev *dev, uint32_t node_id,
		      uint32_t parent_node_id, uint32_t priority,
		      uint32_t weight, uint32_t level_id,
		      struct rte_tm_node_params *params,
		      struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_node_add(dev, node_id, parent_node_id, priority,
			       weight, level_id, params, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_node_delete_wrap(struct rte_eth_dev *dev,
			 uint32_t node_id,
			 struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_node_delete(dev, node_id, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_node_type_get_wrap(struct rte_eth_dev *dev,
			   uint32_t node_id,
			   int *is_leaf,
			   struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_node_type_get(dev, node_id, is_leaf, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_level_capabilities_get_wrap(struct rte_eth_dev *dev,
				    uint32_t level_id,
				    struct rte_tm_level_capabilities *cap,
				    struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_level_capabilities_get(dev, level_id, cap, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_node_capabilities_get_wrap(struct rte_eth_dev *dev,
				   uint32_t node_id,
				   struct rte_tm_node_capabilities *cap,
				   struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_node_capabilities_get(dev, node_id, cap, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_hierarchy_commit_wrap(struct rte_eth_dev *dev,
			      int clear_on_fail,
			      struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_hierarchy_commit(dev, clear_on_fail, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_tm_node_shaper_update_wrap(struct rte_eth_dev *dev,
				uint32_t node_id,
				uint32_t shaper_profile_id,
				struct rte_tm_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_tm_node_shaper_update(dev, node_id,
					 shaper_profile_id, error);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static const struct rte_tm_ops hns3_tm_ops = {
	.capabilities_get       = hns3_tm_capabilities_get_wrap,
	.shaper_profile_add     = hns3_tm_shaper_profile_add_wrap,
	.shaper_profile_delete  = hns3_tm_shaper_profile_del_wrap,
	.node_add               = hns3_tm_node_add_wrap,
	.node_delete            = hns3_tm_node_delete_wrap,
	.node_type_get          = hns3_tm_node_type_get_wrap,
	.level_capabilities_get = hns3_tm_level_capabilities_get_wrap,
	.node_capabilities_get  = hns3_tm_node_capabilities_get_wrap,
	.hierarchy_commit       = hns3_tm_hierarchy_commit_wrap,
	.node_shaper_update     = hns3_tm_node_shaper_update_wrap,
};

int
hns3_tm_ops_get(struct rte_eth_dev *dev, void *arg)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (arg == NULL)
		return -EINVAL;

	if (!hns3_dev_get_support(hw, TM))
		return -EOPNOTSUPP;

	*(const void **)arg = &hns3_tm_ops;

	return 0;
}

void
hns3_tm_dev_start_proc(struct hns3_hw *hw)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);

	if (!hns3_dev_get_support(hw, TM))
		return;

	if (pf->tm_conf.root && !pf->tm_conf.committed)
		hns3_warn(hw,
		    "please call hierarchy_commit() before starting the port.");
}

/*
 * We need clear tm_conf committed flag when device stop so that user can modify
 * tm configuration (e.g. add or delete node).
 *
 * If user don't call hierarchy commit when device start later, the Port/TC's
 * shaper rate still the same as previous committed.
 *
 * To avoid the above problem, we need recover Port/TC shaper rate when device
 * stop.
 */
void
hns3_tm_dev_stop_proc(struct hns3_hw *hw)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	struct hns3_tm_node_list *tc_list = &pf->tm_conf.tc_list;
	struct hns3_tm_node *tm_node;
	uint8_t tc_no;

	if (!pf->tm_conf.committed)
		return;

	tm_node = pf->tm_conf.root;
	if (tm_node != NULL && tm_node->shaper_profile)
		(void)hns3_tm_config_port_rate(hw, NULL);

	TAILQ_FOREACH(tm_node, tc_list, node) {
		if (tm_node->shaper_profile == NULL)
			continue;
		tc_no = hns3_tm_calc_node_tc_no(&pf->tm_conf, tm_node->id);
		(void)hns3_tm_config_tc_rate(hw, tc_no, NULL);
	}

	pf->tm_conf.committed = false;
}

int
hns3_tm_conf_update(struct hns3_hw *hw)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	struct rte_tm_error error;

	if (!hns3_dev_get_support(hw, TM))
		return 0;

	if (pf->tm_conf.root == NULL || !pf->tm_conf.committed)
		return 0;

	memset(&error, 0, sizeof(struct rte_tm_error));
	return hns3_tm_hierarchy_do_commit(hw, &error);
}
