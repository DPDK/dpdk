/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_ethdev.h>
#include <rte_tm_driver.h>

#include "sxe2_tm.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"
#include "sxe2_cmd_chnl.h"

static uint16_t sxe2_tm_level_node_num_get(uint8_t level)
{
	uint16_t node_num = 0;

	switch (level) {
	case 0:
		node_num = SXE2_TM_1L_NODE_NUM_MAX;
		break;
	case 1:
		node_num = SXE2_TM_2L_NODE_NUM_MAX;
		break;
	case 2:
		node_num = SXE2_TM_3L_NODE_NUM_MAX;
	break;
	case 3:
		node_num = SXE2_TM_4L_NODE_NUM_MAX;
		break;
	}
	return node_num;
}

static int32_t sxe2_tm_capabilities_get(struct rte_eth_dev *dev,
			  struct rte_tm_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	uint32_t i;

	if (!cap || !error) {
		PMD_LOG_ERR(DRV, "sxe2 get tm cap failed, cap or error is NULL.");
		ret = -EINVAL;
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	error->type = RTE_TM_ERROR_TYPE_NONE;
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	for (i = 0; i < adapter->tm_ctxt.tm_layers; i++)
		cap->n_nodes_max += sxe2_tm_level_node_num_get(i);

	cap->n_levels_max = adapter->tm_ctxt.tm_layers;

	cap->non_leaf_nodes_identical = 1;

	cap->leaf_nodes_identical = 1;

	cap->shaper_n_max = cap->n_nodes_max;

	cap->shaper_private_n_max = cap->n_nodes_max;

	cap->shaper_private_dual_rate_n_max = cap->n_nodes_max;

	cap->shaper_private_rate_min = 0;

	cap->shaper_private_rate_max = 12500000000ull;

	cap->shaper_private_packet_mode_supported = 0;
	cap->shaper_private_byte_mode_supported = 1;

	cap->shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD;

	cap->shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->shaper_shared_packet_mode_supported = 0;
	cap->shaper_shared_byte_mode_supported = 0;

	cap->sched_n_children_max = dev->data->nb_tx_queues;

	cap->sched_sp_n_priorities_max = 7;

	cap->sched_wfq_n_children_per_group_max = 1;
	cap->sched_wfq_n_groups_max = 0;
	cap->sched_wfq_weight_max = 0;
	cap->sched_wfq_packet_mode_supported = 0;
	cap->sched_wfq_byte_mode_supported = 0;

	cap->cman_wred_packet_mode_supported = 0;
	cap->cman_wred_byte_mode_supported = 0;
	cap->cman_head_drop_supported = 0;
	cap->cman_wred_context_n_max = 0;
	cap->cman_wred_context_private_n_max = 0;
	cap->cman_wred_context_shared_n_max = 0;
	cap->cman_wred_context_shared_n_nodes_per_context_max = 0;
	cap->cman_wred_context_shared_n_contexts_per_node_max = 0;

	cap->dynamic_update_mask = 0;

	cap->stats_mask = 0;
l_end:
	return ret;
}

static int32_t sxe2_level_capabilities_get(struct rte_eth_dev *dev,
		uint32_t level_id, struct rte_tm_level_capabilities *cap,
		struct rte_tm_error *error)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (!cap || !error) {
		PMD_LOG_ERR(DRV, "sxe2 get tm cap failed, cap or error is NULL.");
		ret = -EINVAL;
		goto l_end;
	}
	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	if (level_id >= adapter->tm_ctxt.tm_layers) {
		ret = -EINVAL;
		error->type = RTE_TM_ERROR_TYPE_LEVEL_ID;
		error->message = "too deep level";
		goto l_end;
	}

	cap->n_nodes_max = sxe2_tm_level_node_num_get(level_id);

	cap->non_leaf_nodes_identical = true;

	cap->leaf_nodes_identical = true;

	if (level_id != adapter->tm_ctxt.tm_layers - 1) {
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		cap->n_nodes_leaf_max = 0;

		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = true;
		cap->nonleaf.shaper_private_rate_min = 0;

		cap->nonleaf.shaper_private_rate_max = 12500000000ull;
		cap->nonleaf.shaper_private_packet_mode_supported = 0;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;

		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.shaper_shared_packet_mode_supported = 0;
		cap->nonleaf.shaper_shared_byte_mode_supported = 0;

		cap->nonleaf.sched_n_children_max = SXE2_TM_MAX_CHILDREN_COUNT;
		cap->nonleaf.sched_sp_n_priorities_max = 7;

		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 0;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;

		cap->nonleaf.stats_mask = 0;
	} else {
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = cap->n_nodes_max;

		cap->leaf.shaper_private_supported = true;
		cap->leaf.shaper_private_dual_rate_supported = true;
		cap->leaf.shaper_private_rate_min = 0;
		cap->leaf.shaper_private_rate_max = 12500000000ull;
		cap->leaf.shaper_private_packet_mode_supported = 0;
		cap->leaf.shaper_private_byte_mode_supported = 1;

		cap->leaf.shaper_shared_n_max = 0;
		cap->leaf.shaper_shared_packet_mode_supported = 0;
		cap->leaf.shaper_shared_byte_mode_supported = 0;

		cap->leaf.cman_head_drop_supported = false;
		cap->leaf.cman_wred_context_private_supported = false;
		cap->leaf.cman_wred_context_shared_n_max = 0;

		cap->leaf.stats_mask = 0;
	}

l_end:
	return ret;
}

static struct sxe2_tm_node *sxe2_tm_find_node(struct sxe2_tm_node *parent, uint32_t id)
{
	struct sxe2_tm_node *node = NULL;
	uint32_t i;

	if (parent == NULL || parent->id == id) {
		node = parent;
		goto l_end;
	}

	for (i = 0; i < parent->child_cnt; i++) {
		node = sxe2_tm_find_node(parent->children[i], id);
		if (node)
			goto l_end;
	}

l_end:
	return node;
}

static int32_t sxe2_node_capabilities_get(struct rte_eth_dev *dev, uint32_t node_id,
				      struct rte_tm_node_capabilities *cap,
				      struct rte_tm_error *error)
{
	int32_t ret = -EINVAL;
	struct sxe2_tm_node *tm_node;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (!cap || !error) {
		PMD_LOG_ERR(DRV, "sxe2 get tm cap failed, cap or error is NULL.");
		ret = -EINVAL;
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		PMD_LOG_ERR(DRV, "invalid node id");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		goto l_end;
	}

	tm_node = sxe2_tm_find_node(adapter->tm_ctxt.root, node_id);
	if (!tm_node) {
		PMD_LOG_ERR(DRV, "no such node");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		goto l_end;
	}

	cap->shaper_private_supported = true;
	cap->shaper_private_dual_rate_supported = true;
	cap->shaper_private_rate_min = 0;
	cap->shaper_private_rate_max = 12500000000ull;
	cap->shaper_private_packet_mode_supported = 0;
	cap->shaper_private_byte_mode_supported = 1;

	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_packet_mode_supported = 0;
	cap->shaper_shared_byte_mode_supported = 0;

	if (tm_node->level == adapter->tm_ctxt.tm_layers - 1) {
		cap->leaf.cman_head_drop_supported = false;
		cap->leaf.cman_wred_context_private_supported = false;
		cap->leaf.cman_wred_context_shared_n_max = 0;
	} else {
		cap->nonleaf.sched_n_children_max = SXE2_TM_MAX_CHILDREN_COUNT;
		cap->nonleaf.sched_sp_n_priorities_max = 7;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 0;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;
	}
	cap->stats_mask = 0;

	ret = 0;
l_end:
	return ret;
}

static int32_t sxe2_tm_shaper_profile_param_check(const struct rte_tm_shaper_params *profile,
					      struct rte_tm_error *error)
{
	int32_t ret = 0;

	if (profile->committed.size) {
		PMD_LOG_ERR(DRV, "committed bucket size not supported.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
		error->message = "committed bucket size not supported";
		ret = -EINVAL;
		goto l_end;
	}

	if (profile->peak.size) {
		PMD_LOG_ERR(DRV, "peak bucket size not supported.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
		error->message = "peak bucket size not supported";
		ret = -EINVAL;
		goto l_end;
	}

	if (profile->pkt_length_adjust) {
		PMD_LOG_ERR(DRV, "packet length adjustment not supported.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN;
		error->message = "packet length adjustment not supported";
		ret = -EINVAL;
		goto l_end;
	}

	if (profile->committed.rate > SXE2_HW_RATE_MAX ||
		profile->committed.rate < SXE2_HW_RATE_MIN) {
		PMD_LOG_ERR(DRV, "The committed rate limit value is required to be in "
			"the range [%" PRIu64 ", %" PRIu64 "].",
			(uint64_t)SXE2_HW_RATE_MIN, (uint64_t)SXE2_HW_RATE_MAX);
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		error->message = "invalid rate limit: value out of range.";
		ret = -EINVAL;
		goto l_end;
	}

	if (profile->peak.rate > SXE2_HW_RATE_MAX ||
		profile->peak.rate < SXE2_HW_RATE_MIN) {
		PMD_LOG_ERR(DRV, "The peak rate limit value is required to be in "
			"the range [%" PRIu64 ", %" PRIu64 "].",
			(uint64_t)SXE2_HW_RATE_MIN, (uint64_t)SXE2_HW_RATE_MAX);
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE;
		error->message = "invalid rate limit: value out of range.";
		ret = -EINVAL;
		goto l_end;
	}

	if (profile->committed.rate > profile->peak.rate) {
		PMD_LOG_ERR(DRV, "committed rate can't be greater than peak rate.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		error->message = "committed rate can't be greater than peak rate.";
		ret = -EINVAL;
		goto l_end;
	}
l_end:
	return ret;
}

static inline struct sxe2_tm_shaper_profile *
sxe2_tm_shaper_profile_search(struct rte_eth_dev *dev, uint32_t id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_shaper_profile_list *shaper_profile_list =
		&adapter->tm_ctxt.profile_list;
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (id == shaper_profile->id)
			goto l_end;
	}

	shaper_profile = NULL;

l_end:
	return shaper_profile;
}

static int32_t sxe2_tm_shaper_profile_add(struct rte_eth_dev *dev, uint32_t shaper_profile_id,
				      const struct rte_tm_shaper_params *profile,
				      struct rte_tm_error *error)
{
	int32_t ret = -EINVAL;
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (!profile || !error) {
		PMD_LOG_ERR(DRV, "Invalid input: profile:0x%p or error:0x%p is null.",
			profile, error);
		if (error) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "Invalid input: profile or error is null.";
		}
		ret = -EINVAL;
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	ret = sxe2_tm_shaper_profile_param_check(profile, error);
	if (ret)
		goto l_end;

	shaper_profile = sxe2_tm_shaper_profile_search(dev, shaper_profile_id);
	if (shaper_profile) {
		PMD_LOG_ERR(DRV, "profile ID exist.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		ret = -EINVAL;
		goto l_end;
	}

	shaper_profile = rte_zmalloc("sxe2_tm_shaper_profile",
							sizeof(struct sxe2_tm_shaper_profile), 0);
	if (!shaper_profile) {
		PMD_LOG_ERR(DRV, "Alloc shaper_profile memory failed.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "Alloc shaper_profile memory failed";
		ret = -ENOMEM;
		goto l_end;
	}

	rte_memcpy(&shaper_profile->profile, profile,
					sizeof(struct rte_tm_shaper_params));
	shaper_profile->id = shaper_profile_id;

	TAILQ_INSERT_TAIL(&adapter->tm_ctxt.profile_list, shaper_profile, node);
l_end:
	return ret;
}

static int32_t sxe2_tm_shaper_profile_del(struct rte_eth_dev *dev,
		uint32_t id, struct rte_tm_error *error)
{
	int32_t ret = -EINVAL;
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (!error) {
		PMD_LOG_ERR(DRV, "Error param is null.");
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	shaper_profile = sxe2_tm_shaper_profile_search(dev, id);
	if (!shaper_profile) {
		PMD_LOG_ERR(DRV, "profile ID not exist.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID not exist";
		ret = -EINVAL;
		goto l_end;
	}

	if (shaper_profile->ref_cnt) {
		PMD_LOG_ERR(DRV, "profile in use.");
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		error->message = "profile in use";
		ret = -EINVAL;
		goto l_end;
	}

	TAILQ_REMOVE(&adapter->tm_ctxt.profile_list, shaper_profile, node);
	rte_free(shaper_profile);

	ret = 0;
l_end:
	return ret;
}

static int32_t sxe2_tm_node_param_check(struct rte_eth_dev *dev,
			uint32_t parent_node_type,
			uint32_t node_id, uint32_t priority, uint32_t weight,
			const struct rte_tm_node_params *params,
			bool is_leaf, struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	int32_t ret = -EINVAL;

	if (node_id == RTE_TM_NODE_ID_NULL) {
		PMD_LOG_ERR(DRV, "Invalid node id.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		goto l_end;
	}

	if (parent_node_type == SXE2_TM_NODE_TYPE_VSIG &&
			priority >= tm_ctxt->prio_max) {
		PMD_LOG_ERR(DRV, "Priority should be less than %u.", tm_ctxt->prio_max);
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "The priority is too high.";
		goto l_end;
	}

	if (priority > SXE2_TM_PRIO_MAX) {
		PMD_LOG_ERR(DRV, "Priority should be less than %u.", SXE2_TM_PRIO_MAX);
		error->type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		error->message = "The priority is too high.";
		goto l_end;
	}

	if (weight > SXE2_TM_WEIGHT_MAX || weight < SXE2_TM_WEIGHT_MIN) {
		PMD_LOG_ERR(DRV, "Weight must be between 1 and 200.");
		error->type = RTE_TM_ERROR_TYPE_NODE_WEIGHT;
		error->message = "weight must be between 1 and 200";
		goto l_end;
	}

	if (params->shared_shaper_id) {
		PMD_LOG_ERR(DRV, "Shared shaper not supported.");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID;
		error->message = "shared shaper not supported";
		goto l_end;
	}
	if (params->n_shared_shapers) {
		PMD_LOG_ERR(DRV, "Shared shaper not supported..");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS;
		error->message = "shared shaper not supported";
		goto l_end;
	}

	if (!is_leaf) {
		if (node_id <= dev->data->nb_tx_queues) {
			PMD_LOG_ERR(DRV, "no leaf node id must bigger than queue id.");
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "no leaf node id must bigger than queue id.";
			goto l_end;
		}

		if (params->nonleaf.wfq_weight_mode) {
			PMD_LOG_ERR(DRV, "WFQ not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE;
			error->message = "WFQ not supported";
			goto l_end;
		}

		if (params->nonleaf.n_sp_priorities != 1) {
			PMD_LOG_ERR(DRV, "SP priority not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES;
			error->message = "SP priority not supported";
			goto l_end;
		}
	} else {
		if (node_id >= dev->data->nb_tx_queues) {
			PMD_LOG_ERR(DRV, "leaf node id must be queue id.");
			error->type = RTE_TM_ERROR_TYPE_NODE_ID;
			error->message = "leaf node id must be queue id.";
			goto l_end;
		}

		if (params->leaf.cman) {
			PMD_LOG_ERR(DRV, "Congestion management not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN;
			error->message = "Congestion management not supported";
			goto l_end;
		}
		if (params->leaf.wred.wred_profile_id != RTE_TM_WRED_PROFILE_ID_NONE) {
			PMD_LOG_ERR(DRV, "WRED not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID;
			error->message = "WRED not supported";
			goto l_end;
		}
		if (params->leaf.wred.shared_wred_context_id) {
			PMD_LOG_ERR(DRV, "WRED not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_WRED_CONTEXT_ID;
			error->message = "WRED not supported";
			goto l_end;
		}
		if (params->leaf.wred.n_shared_wred_contexts) {
			PMD_LOG_ERR(DRV, "WRED not supported.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS;
			error->message = "WRED not supported";
			goto l_end;
		}
	}
	ret = 0;
l_end:
	return ret;
}

static int32_t sxe2_tm_add_child(struct sxe2_tm_node *parent,
		struct sxe2_tm_node *child)
{
	int32_t ret = -1;
	uint32_t i;
	for (i = 0; i < SXE2_TM_MAX_CHILDREN_COUNT; i++) {
		if (parent->children[i] == NULL) {
			parent->children[i] = child;
			child->index_in_parent = i;
			parent->child_cnt++;
			ret = 0;
			break;
		}
	}
	return ret;
}

static int32_t sxe2_tm_node_add(struct rte_eth_dev *dev, uint32_t node_id, uint32_t parent_node_id,
			    uint32_t priority, uint32_t weight, uint32_t level_id,
			    const struct rte_tm_node_params *params,
			    struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	struct sxe2_tm_node *tm_node = NULL;
	struct sxe2_tm_node *parent_node = NULL;
	int32_t ret = -EINVAL;
	bool is_leaf;

	if (!params || !error) {
		PMD_LOG_ERR(DRV, "Invalid input: params:0x%p or error:0x%p is null.",
			params, error);
		if (error) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "Invalid input: params or error is null.";
		}
		ret = -EINVAL;
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	shaper_profile = sxe2_tm_shaper_profile_search(dev, params->shaper_profile_id);
	if (!shaper_profile) {
		PMD_LOG_ERR(DRV, "Shaper profile does not exist.");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID;
		error->message = "shaper profile does not exist";
		ret = -EINVAL;
		goto l_end;
	}

	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		if (level_id != 0) {
			PMD_LOG_ERR(DRV, "Wrong level, root node (NULL parent) must be at level 0.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
			error->message = "Wrong level, root node (NULL parent) must be at level 0";
			ret = -EINVAL;
			goto l_end;
		}

		if (tm_ctxt->root) {
			PMD_LOG_ERR(DRV, "Already have a root.");
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "already have a root";
			ret = -EINVAL;
			goto l_end;
		}

		ret = sxe2_tm_node_param_check(dev, SXE2_TM_NODE_TYPE_INVALID, node_id, priority,
					       weight, params, false, error);
		if (ret)
			goto l_end;

		tm_node = rte_zmalloc("tm_node_root", sizeof(struct sxe2_tm_node), 0);
		if (!tm_node) {
			PMD_LOG_ERR(DRV, "Alloc tm_node memory failed.");
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "Alloc tm_node memory failed";
			ret = -ENOMEM;
			goto l_end;
		}

		tm_node->id = node_id;
		tm_node->level = 0;
		tm_node->parent = NULL;
		tm_node->child_cnt = 0;
		tm_node->weight = weight;
		tm_node->hw_weight = SXE2_TM_WEIGHT_SUM;
		tm_node->type = SXE2_TM_NODE_TYPE_VSIG;
		tm_node->priority = priority;
		tm_node->shaper_profile = shaper_profile;

		tm_node->teid = tm_ctxt->root_teid;

		shaper_profile->ref_cnt++;
		tm_ctxt->root = tm_node;
		ret = 0;
		goto l_end;
	}

	parent_node = sxe2_tm_find_node(tm_ctxt->root, parent_node_id);
	if (!parent_node) {
		PMD_LOG_ERR(DRV, "Parent not exist.");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		error->message = "parent not exist";
		ret = -EINVAL;
		goto l_end;
	}

	if (parent_node->child_cnt >= SXE2_TM_MAX_CHILDREN_COUNT ||
		(parent_node->type == SXE2_TM_NODE_TYPE_VSIG &&
		 parent_node->child_cnt >= tm_ctxt->root_max_children)) {
		PMD_LOG_ERR(DRV, "Parent node is full.");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "parent node is full";
		ret = -EINVAL;
		goto l_end;
	}

	if (level_id == RTE_TM_NODE_LEVEL_ID_ANY) {
		level_id = parent_node->level + 1;
	} else if (level_id != parent_node->level + 1) {
		PMD_LOG_ERR(DRV, "Wrong level.");
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "Wrong level";
		ret = -EINVAL;
		goto l_end;
	}

	if (level_id >= tm_ctxt->tm_layers) {
		PMD_LOG_ERR(DRV, "The maximum number of TM configuration levels is %d",
				tm_ctxt->tm_layers);
		error->type = RTE_TM_ERROR_TYPE_NODE_PARAMS;
		error->message = "TM level exceeds supported hardware limit";
		ret = -EINVAL;
		goto l_end;
	}

	if (level_id + 1 == tm_ctxt->tm_layers)
		is_leaf = true;
	else
		is_leaf = false;

	ret = sxe2_tm_node_param_check(dev, parent_node->type, node_id, priority, weight,
				       params, is_leaf, error);
	if (ret)
		goto l_end;

	if (sxe2_tm_find_node(tm_ctxt->root, node_id)) {
		PMD_LOG_ERR(DRV, "Node id already used.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "node id already used";
		ret = -EINVAL;
		goto l_end;
	}

	tm_node = rte_zmalloc("tm_node_no_root", sizeof(struct sxe2_tm_node), 0);
	if (!tm_node) {
		PMD_LOG_ERR(DRV, "Alloc tm_node memory failed.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "Alloc tm_node memory failed";
		ret = -ENOMEM;
		goto l_end;
	}

	if (level_id + 1 != tm_ctxt->tm_layers)
		tm_node->type = SXE2_TM_NODE_TYPE_MID;
	else
		tm_node->type = SXE2_TM_NODE_TYPE_QUEUE;
	tm_node->id = node_id;
	tm_node->level = level_id;
	tm_node->parent = parent_node;
	tm_node->child_cnt = 0;
	tm_node->weight = weight;
	tm_node->priority = priority;
	tm_node->shaper_profile = shaper_profile;
	shaper_profile->ref_cnt++;

	ret = sxe2_tm_add_child(parent_node, tm_node);
	if (ret) {
		shaper_profile->ref_cnt--;
		rte_free(tm_node);
	}
l_end:
	return ret;
}

static int32_t sxe2_tm_tree_delete(struct sxe2_tm_node *tm_node)
{
	int32_t ret = 0;
	uint32_t i, j;
	struct sxe2_tm_node *parent = NULL;

	if (!tm_node)
		goto l_end;

	parent = tm_node->parent;

	if (tm_node->child_cnt != 0) {
		for (i = SXE2_TM_MAX_CHILDREN_COUNT; i > 0; i--) {
			if (tm_node->children[i - 1])
				ret = sxe2_tm_tree_delete(tm_node->children[i - 1]);
		}
	}

	if (tm_node->shaper_profile)
		tm_node->shaper_profile->ref_cnt--;

	if (tm_node->type != SXE2_TM_NODE_TYPE_VSIG && parent) {
		for (i = 0; i < parent->child_cnt; i++) {
			if (parent->children[i] == tm_node)
				break;
		}
		for (j = i; j < parent->child_cnt - 1; j++)
			parent->children[j] = parent->children[j + 1];

		parent->children[parent->child_cnt - 1] = NULL;
		parent->child_cnt--;
	}
	rte_free(tm_node);
	tm_node = NULL;
l_end:
	return ret;
}

static int32_t sxe2_tm_node_delete(struct rte_eth_dev *dev, uint32_t node_id,
		struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	struct sxe2_tm_node *tm_node = NULL;
	int32_t ret = -EINVAL;

	if (!error) {
		PMD_LOG_ERR(DRV, "Invalid input: error is null.");
		ret = -EINVAL;
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		PMD_LOG_ERR(DRV, "Invalid node id. node_id = %u", node_id);
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		goto l_end;
	}

	tm_node = sxe2_tm_find_node(tm_ctxt->root, node_id);
	if (!tm_node) {
		PMD_LOG_ERR(DRV, "No such node.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_tm_tree_delete(tm_node);
	if (ret) {
		PMD_LOG_ERR(DRV, "Delete node failed.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "Delete node failed";
		goto l_end;
	}

	if (tm_node == tm_ctxt->root)
		tm_ctxt->root = NULL;

l_end:
	return ret;
}

static int32_t sxe2_tm_node_type_get(struct rte_eth_dev *dev, uint32_t node_id,
		int32_t *is_leaf, struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	struct sxe2_tm_node *tm_node = NULL;
	int32_t ret = -EINVAL;

	if (!is_leaf || !error) {
		PMD_LOG_ERR(DRV, "Invalid input: is_leaf:0x%p or error:0x%p is null.",
			is_leaf, error);
		if (error) {
			error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
			error->message = "Invalid input: is_leaf or error is null";
		}
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_end;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		PMD_LOG_ERR(DRV, "Invalid node id.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		goto l_end;
	}

	tm_node = sxe2_tm_find_node(tm_ctxt->root, node_id);
	if (!tm_node) {
		PMD_LOG_ERR(DRV, "No such node.");
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		goto l_end;
	}

	if (tm_node->level + 1 == tm_ctxt->tm_layers)
		*is_leaf = true;
	else
		*is_leaf = false;
	ret = 0;
l_end:
	return ret;
}

int32_t sxe2_tm_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;
	int32_t ret = 0;

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		ret = 0;
		goto l_end;
	}

	tm_ctxt->tm_layers = 0;
	tm_ctxt->root_max_children = 0;
	tm_ctxt->committed = false;

	(void)sxe2_tm_tree_delete(tm_ctxt->root);

	while ((shaper_profile = TAILQ_FIRST(&tm_ctxt->profile_list))) {
		TAILQ_REMOVE(&tm_ctxt->profile_list, shaper_profile, node);
		rte_free(shaper_profile);
	}
l_end:
	return ret;
}

int32_t sxe2_tm_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_sched_hw_cap *sched_ctxt = &adapter->sched_ctxt;
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	int32_t ret = 0;
	struct sxe2_tm_shaper_profile *shaper_profile = NULL;

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0)
		goto l_end;

	tm_ctxt->tm_layers = sched_ctxt->tm_layers;
	tm_ctxt->root_max_children = sched_ctxt->root_max_children;
	tm_ctxt->prio_max = sched_ctxt->prio_max;
	tm_ctxt->committed = false;
	TAILQ_INIT(&tm_ctxt->profile_list);
	tm_ctxt->root = NULL;

	shaper_profile = rte_zmalloc("sxe2_tm_shaper_profile",
		sizeof(struct sxe2_tm_shaper_profile), 0);
	if (!shaper_profile) {
		PMD_LOG_ERR(DRV, "Alloc shaper_profile memory failed.");
		ret = -ENOMEM;
		goto l_end;
	}
	shaper_profile->id = RTE_TM_SHAPER_PROFILE_ID_NONE;
	shaper_profile->ref_cnt = 1;
	shaper_profile->profile.committed.rate = UINT64_MAX;
	shaper_profile->profile.peak.rate = UINT64_MAX;
	TAILQ_INSERT_TAIL(&tm_ctxt->profile_list, shaper_profile, node);

l_end:
	return ret;
}

static int32_t sxe2_tm_chk_all_leaf(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	uint32_t i = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!sxe2_tm_find_node(adapter->tm_ctxt.root, i)) {
			ret = -1;
			break;
		}
	}
	return ret;
}

static int32_t sxe2_tm_weight_calc(struct sxe2_tm_node *tm_node)
{
	int32_t ret = 0;
	int32_t total_weight = 0;
	int32_t total_weight2 = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t k = 0;
	uint32_t maxindex = 0;
	uint32_t maxweight = 0;
	struct sxe2_tm_node *cacl_node[SXE2_TM_MAX_CHILDREN_COUNT] = {NULL};

	if (!tm_node) {
		PMD_LOG_ERR(DRV, "Invalid input: tm_node is null.");
		ret = -EINVAL;
		goto l_end;
	}

	if (tm_node->child_cnt == 0)
		goto l_end;

	for (j = SXE2_TM_PRIO_MIN; j <= SXE2_TM_PRIO_MAX; j++) {
		k = 0;
		total_weight = 0;
		total_weight2 = 0;
		maxindex = 0;
		maxweight = 0;

		for (i = 0; i < tm_node->child_cnt; i++) {
			if (tm_node->children[i]->priority == j)
				cacl_node[k++] = tm_node->children[i];
		}
		if (k == 0)
			continue;

		for (i = 0; i < k; i++)
			total_weight += cacl_node[i]->weight;

		for (i = 0; i < k; i++) {
			cacl_node[i]->hw_weight = cacl_node[i]->weight *
				SXE2_TM_WEIGHT_SUM / total_weight;
			total_weight2 += cacl_node[i]->hw_weight;
			if (cacl_node[i]->hw_weight > maxweight) {
				maxweight = cacl_node[i]->hw_weight;
				maxindex = i;
			}
		}

		cacl_node[maxindex]->hw_weight += SXE2_TM_WEIGHT_SUM - total_weight2;
	}

	for (i = 0; i < tm_node->child_cnt; i++) {
		ret = sxe2_tm_weight_calc(tm_node->children[i]);
		if (ret)
			goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_tm_hierarchy_commit(struct rte_eth_dev *dev,
				     int32_t clear_on_fail, struct rte_tm_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = -EINVAL;

	if (!error) {
		PMD_LOG_ERR(DRV, "Invalid input: error is null.");
		ret = -EINVAL;
		goto l_clear_on_fail;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM) == 0) {
		PMD_LOG_ERR(DRV, "The TM capability is not supported.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The TM capability is not supported.";
		ret = ENOTSUP;
		goto l_clear_on_fail;
	}

	if (dev->data->dev_started) {
		PMD_LOG_ERR(DRV, "Device failed to Stop.");
		error->message = "Device failed to Stop";
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		ret = -EPERM;
		goto l_clear_on_fail;
	}

	ret = sxe2_tm_chk_all_leaf(dev);
	if (ret) {
		PMD_LOG_ERR(DRV, "All tx queues need config.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "All tx queues need config.";
		goto l_clear_on_fail;
	}

	ret = sxe2_tm_weight_calc(adapter->tm_ctxt.root);
	if (ret) {
		PMD_LOG_ERR(DRV, "The weight in tree is wrong.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "The weight in tree is wrong.";
		goto l_clear_on_fail;
	}

	ret = sxe2_drv_tm_commit(adapter);
	if (ret) {
		PMD_LOG_ERR(DRV, "Commit tree to fw failed.");
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "Commit tree to fw failed.";
		goto l_clear_on_fail;
	}

	adapter->tm_ctxt.committed = true;
	ret = 0;
	goto l_end;

l_clear_on_fail:
	if (clear_on_fail) {
		(void)sxe2_tm_uninit(dev);
		(void)sxe2_tm_init(dev);
	}

l_end:
	return ret;
}

static const struct rte_tm_ops sxe2_tm_ops = {
	.capabilities_get = sxe2_tm_capabilities_get,
	.level_capabilities_get = sxe2_level_capabilities_get,
	.node_capabilities_get = sxe2_node_capabilities_get,
	.shaper_profile_add = sxe2_tm_shaper_profile_add,
	.shaper_profile_delete = sxe2_tm_shaper_profile_del,
	.node_add = sxe2_tm_node_add,
	.node_delete = sxe2_tm_node_delete,
	.node_type_get = sxe2_tm_node_type_get,

	.hierarchy_commit = sxe2_tm_hierarchy_commit,
};

int32_t sxe2_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	int32_t ret = 0;

	if (!arg) {
		ret = -EINVAL;
		PMD_LOG_ERR(DRV, "%s failed because arg is NULL", __func__);
		goto l_end;
	}
	*(const void **)arg = &sxe2_tm_ops;
l_end:
	return ret;
}
