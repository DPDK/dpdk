/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <rte_tm_driver.h>

#include <rte_mbuf.h>
#include <rte_sched.h>
#include <ethdev_driver.h>

#include <rte_io.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_bus_ifpga.h>
#include <ifpga_logs.h>

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

#define BYTES_IN_MBPS     (1000 * 1000 / 8)
#define SUBPORT_TC_PERIOD 10
#define PIPE_TC_PERIOD    40

struct ipn3ke_tm_shaper_params_range_type {
	uint32_t m1;
	uint32_t m2;
	uint32_t exp;
	uint32_t exp2;
	uint32_t low;
	uint32_t high;
};
struct ipn3ke_tm_shaper_params_range_type ipn3ke_tm_shaper_params_rang[] = {
	{  0,       1,     0,        1,           0,            4},
	{  2,       3,     0,        1,           8,           12},
	{  4,       7,     0,        1,          16,           28},
	{  8,      15,     0,        1,          32,           60},
	{ 16,      31,     0,        1,          64,          124},
	{ 32,      63,     0,        1,         128,          252},
	{ 64,     127,     0,        1,         256,          508},
	{128,     255,     0,        1,         512,         1020},
	{256,     511,     0,        1,        1024,         2044},
	{512,    1023,     0,        1,        2048,         4092},
	{512,    1023,     1,        2,        4096,         8184},
	{512,    1023,     2,        4,        8192,        16368},
	{512,    1023,     3,        8,       16384,        32736},
	{512,    1023,     4,       16,       32768,        65472},
	{512,    1023,     5,       32,       65536,       130944},
	{512,    1023,     6,       64,      131072,       261888},
	{512,    1023,     7,      128,      262144,       523776},
	{512,    1023,     8,      256,      524288,      1047552},
	{512,    1023,     9,      512,     1048576,      2095104},
	{512,    1023,    10,     1024,     2097152,      4190208},
	{512,    1023,    11,     2048,     4194304,      8380416},
	{512,    1023,    12,     4096,     8388608,     16760832},
	{512,    1023,    13,     8192,    16777216,     33521664},
	{512,    1023,    14,    16384,    33554432,     67043328},
	{512,    1023,    15,    32768,    67108864,    134086656},
};

#define IPN3KE_TM_SHAPER_RANGE_NUM (sizeof(ipn3ke_tm_shaper_params_rang) / \
	sizeof(struct ipn3ke_tm_shaper_params_range_type))

#define IPN3KE_TM_SHAPER_COMMITTED_RATE_MAX \
	(ipn3ke_tm_shaper_params_rang[IPN3KE_TM_SHAPER_RANGE_NUM - 1].high)

#define IPN3KE_TM_SHAPER_PEAK_RATE_MAX \
	(ipn3ke_tm_shaper_params_rang[IPN3KE_TM_SHAPER_RANGE_NUM - 1].high)

int
ipn3ke_hw_tm_init(struct ipn3ke_hw *hw)
{
#define SCRATCH_DATA 0xABCDEF
	struct ipn3ke_tm_node *nodes;
	struct ipn3ke_tm_tdrop_profile *tdrop_profile;
	int node_num;
	int i;

	if (hw == NULL)
		return -EINVAL;
#if IPN3KE_TM_SCRATCH_RW
	uint32_t scratch_data;
	IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_TM_SCRATCH,
					0,
					SCRATCH_DATA,
					0xFFFFFFFF);
	scratch_data = IPN3KE_MASK_READ_REG(hw,
					IPN3KE_TM_SCRATCH,
					0,
					0xFFFFFFFF);
	if (scratch_data != SCRATCH_DATA)
		return -EINVAL;
#endif
	/* alloc memory for all hierarchy nodes */
	node_num = hw->port_num +
		IPN3KE_TM_VT_NODE_NUM +
		IPN3KE_TM_COS_NODE_NUM;

	nodes = rte_zmalloc("ipn3ke_tm_nodes",
			sizeof(struct ipn3ke_tm_node) * node_num,
			0);
	if (!nodes)
		return -ENOMEM;

	/* alloc memory for Tail Drop Profile */
	tdrop_profile = rte_zmalloc("ipn3ke_tm_tdrop_profile",
				sizeof(struct ipn3ke_tm_tdrop_profile) *
				IPN3KE_TM_TDROP_PROFILE_NUM,
				0);
	if (!tdrop_profile) {
		rte_free(nodes);
		return -ENOMEM;
	}

	hw->nodes = nodes;
	hw->port_nodes = nodes;
	hw->vt_nodes = hw->port_nodes + hw->port_num;
	hw->cos_nodes = hw->vt_nodes + IPN3KE_TM_VT_NODE_NUM;
	hw->tdrop_profile = tdrop_profile;
	hw->tdrop_profile_num = IPN3KE_TM_TDROP_PROFILE_NUM;

	for (i = 0, nodes = hw->port_nodes;
		i < hw->port_num;
		i++, nodes++) {
		nodes->node_index = i;
		nodes->level = IPN3KE_TM_NODE_LEVEL_PORT;
		nodes->tm_id = RTE_TM_NODE_ID_NULL;
		nodes->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		nodes->parent_node_id = RTE_TM_NODE_ID_NULL;
		nodes->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		nodes->weight = 0;
		nodes->parent_node = NULL;
		nodes->shaper_profile.valid = 0;
		nodes->tdrop_profile = NULL;
		nodes->n_children = 0;
		TAILQ_INIT(&nodes->children_node_list);
	}

	for (i = 0, nodes = hw->vt_nodes;
		i < IPN3KE_TM_VT_NODE_NUM;
		i++, nodes++) {
		nodes->node_index = i;
		nodes->level = IPN3KE_TM_NODE_LEVEL_VT;
		nodes->tm_id = RTE_TM_NODE_ID_NULL;
		nodes->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		nodes->parent_node_id = RTE_TM_NODE_ID_NULL;
		nodes->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		nodes->weight = 0;
		nodes->parent_node = NULL;
		nodes->shaper_profile.valid = 0;
		nodes->tdrop_profile = NULL;
		nodes->n_children = 0;
		TAILQ_INIT(&nodes->children_node_list);
	}

	for (i = 0, nodes = hw->cos_nodes;
		i < IPN3KE_TM_COS_NODE_NUM;
		i++, nodes++) {
		nodes->node_index = i;
		nodes->level = IPN3KE_TM_NODE_LEVEL_COS;
		nodes->tm_id = RTE_TM_NODE_ID_NULL;
		nodes->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		nodes->parent_node_id = RTE_TM_NODE_ID_NULL;
		nodes->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		nodes->weight = 0;
		nodes->parent_node = NULL;
		nodes->shaper_profile.valid = 0;
		nodes->tdrop_profile = NULL;
		nodes->n_children = 0;
		TAILQ_INIT(&nodes->children_node_list);
	}

	for (i = 0, tdrop_profile = hw->tdrop_profile;
		i < IPN3KE_TM_TDROP_PROFILE_NUM;
		i++, tdrop_profile++) {
		tdrop_profile->tdrop_profile_id = i;
		tdrop_profile->n_users = 0;
		tdrop_profile->valid = 0;
	}

	return 0;
}

void
ipn3ke_tm_init(struct ipn3ke_rpst *rpst)
{
	struct ipn3ke_tm_internals *tm;
	struct ipn3ke_tm_node *port_node;

	tm = &rpst->tm;

	port_node = &rpst->hw->port_nodes[rpst->port_id];
	tm->h.port_node = port_node;

	tm->h.n_shaper_profiles = 0;
	tm->h.n_tdrop_profiles = 0;
	tm->h.n_vt_nodes = 0;
	tm->h.n_cos_nodes = 0;

	tm->h.port_commit_node = NULL;
	TAILQ_INIT(&tm->h.vt_commit_node_list);
	TAILQ_INIT(&tm->h.cos_commit_node_list);

	tm->hierarchy_frozen = 0;
	tm->tm_started = 1;
	tm->tm_id = rpst->port_id;
}

static struct ipn3ke_tm_shaper_profile *
ipn3ke_hw_tm_shaper_profile_search(struct ipn3ke_hw *hw,
	uint32_t shaper_profile_id, struct rte_tm_error *error)
{
	struct ipn3ke_tm_shaper_profile *sp = NULL;
	uint32_t level_of_node_id;
	uint32_t node_index;

	/* Shaper profile ID must not be NONE. */
	if (shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE) {
		rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
				NULL,
				rte_strerror(EINVAL));

		return NULL;
	}

	level_of_node_id = shaper_profile_id / IPN3KE_TM_NODE_LEVEL_MOD;
	node_index = shaper_profile_id % IPN3KE_TM_NODE_LEVEL_MOD;

	switch (level_of_node_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		if (node_index >= hw->port_num)
			rte_tm_error_set(error,
					EEXIST,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EEXIST));
		else
			sp = &hw->port_nodes[node_index].shaper_profile;

		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_index >= IPN3KE_TM_VT_NODE_NUM)
			rte_tm_error_set(error,
					EEXIST,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EEXIST));
		else
			sp = &hw->vt_nodes[node_index].shaper_profile;

		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_index >= IPN3KE_TM_COS_NODE_NUM)
			rte_tm_error_set(error,
					EEXIST,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EEXIST));
		else
			sp = &hw->cos_nodes[node_index].shaper_profile;

		break;
	default:
		rte_tm_error_set(error,
				EEXIST,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
				NULL,
				rte_strerror(EEXIST));
	}

	return sp;
}

static struct ipn3ke_tm_tdrop_profile *
ipn3ke_hw_tm_tdrop_profile_search(struct ipn3ke_hw *hw,
	uint32_t tdrop_profile_id)
{
	struct ipn3ke_tm_tdrop_profile *tdrop_profile;

	if (tdrop_profile_id >= hw->tdrop_profile_num)
		return NULL;

	tdrop_profile = &hw->tdrop_profile[tdrop_profile_id];
	if (tdrop_profile->valid)
		return tdrop_profile;

	return NULL;
}

static struct ipn3ke_tm_node *
ipn3ke_hw_tm_node_search(struct ipn3ke_hw *hw, uint32_t tm_id,
	uint32_t node_id, uint32_t state_mask)
{
	uint32_t level_of_node_id;
	uint32_t node_index;
	struct ipn3ke_tm_node *n;

	level_of_node_id = node_id / IPN3KE_TM_NODE_LEVEL_MOD;
	node_index = node_id % IPN3KE_TM_NODE_LEVEL_MOD;

	switch (level_of_node_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		if (node_index >= hw->port_num)
			return NULL;
		n = &hw->port_nodes[node_index];

		break;
	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_index >= IPN3KE_TM_VT_NODE_NUM)
			return NULL;
		n = &hw->vt_nodes[node_index];

		break;
	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_index >= IPN3KE_TM_COS_NODE_NUM)
			return NULL;
		n = &hw->cos_nodes[node_index];

		break;
	default:
		return NULL;
	}

	/* Check tm node status */
	if (n->node_state == IPN3KE_TM_NODE_STATE_IDLE) {
		if (n->tm_id != RTE_TM_NODE_ID_NULL ||
		n->parent_node_id != RTE_TM_NODE_ID_NULL ||
		n->parent_node != NULL ||
		n->n_children > 0) {
			IPN3KE_AFU_PMD_ERR("tm node check error %d", 1);
		}
	} else if (n->node_state < IPN3KE_TM_NODE_STATE_MAX) {
		if (n->tm_id == RTE_TM_NODE_ID_NULL ||
		(level_of_node_id != IPN3KE_TM_NODE_LEVEL_PORT &&
			n->parent_node_id == RTE_TM_NODE_ID_NULL) ||
		(level_of_node_id != IPN3KE_TM_NODE_LEVEL_PORT &&
			n->parent_node == NULL)) {
			IPN3KE_AFU_PMD_ERR("tm node check error %d", 1);
		}
	} else {
		IPN3KE_AFU_PMD_ERR("tm node check error %d", 1);
	}

	if (IPN3KE_BIT_ISSET(state_mask, n->node_state)) {
		if (n->node_state == IPN3KE_TM_NODE_STATE_IDLE)
			return n;
		else if (n->tm_id == tm_id)
			return n;
		else
			return NULL;
	} else {
		return NULL;
	}
}

/* Traffic manager node type get */
static int
ipn3ke_pmd_tm_node_type_get(struct rte_eth_dev *dev,
	uint32_t node_id, int *is_leaf, struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node *node;
	uint32_t state_mask;

	if (is_leaf == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));

	tm_id = tm->tm_id;

	state_mask = 0;
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_COMMITTED);
	node = ipn3ke_hw_tm_node_search(hw, tm_id, node_id, state_mask);
	if (node_id == RTE_TM_NODE_ID_NULL ||
		node == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	*is_leaf = (node->level == IPN3KE_TM_NODE_LEVEL_COS) ? 1 : 0;

	return 0;
}

#define WRED_SUPPORTED    0

#define STATS_MASK_DEFAULT \
	(RTE_TM_STATS_N_PKTS | \
	RTE_TM_STATS_N_BYTES | \
	RTE_TM_STATS_N_PKTS_GREEN_DROPPED | \
	RTE_TM_STATS_N_BYTES_GREEN_DROPPED)

#define STATS_MASK_QUEUE \
	(STATS_MASK_DEFAULT | RTE_TM_STATS_N_PKTS_QUEUED)

/* Traffic manager capabilities get */
static int
ipn3ke_tm_capabilities_get(__rte_unused struct rte_eth_dev *dev,
	struct rte_tm_capabilities *cap, struct rte_tm_error *error)
{
	if (cap == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_CAPABILITIES,
					NULL,
					rte_strerror(EINVAL));

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(*cap));

	cap->n_nodes_max = 1 + IPN3KE_TM_COS_NODE_NUM + IPN3KE_TM_VT_NODE_NUM;
	cap->n_levels_max = IPN3KE_TM_NODE_LEVEL_MAX;

	cap->non_leaf_nodes_identical = 0;
	cap->leaf_nodes_identical = 1;

	cap->shaper_n_max = 1 + IPN3KE_TM_VT_NODE_NUM;
	cap->shaper_private_n_max = 1 + IPN3KE_TM_VT_NODE_NUM;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 1;
	cap->shaper_private_rate_max = 1 + IPN3KE_TM_VT_NODE_NUM;
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

	cap->shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;
	cap->shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	cap->sched_n_children_max = IPN3KE_TM_COS_NODE_NUM;
	cap->sched_sp_n_priorities_max = 3;
	cap->sched_wfq_n_children_per_group_max = UINT32_MAX;
	cap->sched_wfq_n_groups_max = 1;
	cap->sched_wfq_weight_max = UINT32_MAX;
	cap->sched_wfq_packet_mode_supported = 0;
	cap->sched_wfq_byte_mode_supported = 1;

	cap->cman_wred_packet_mode_supported = 0;
	cap->cman_wred_byte_mode_supported = 0;
	cap->cman_head_drop_supported = 0;
	cap->cman_wred_context_n_max = 0;
	cap->cman_wred_context_private_n_max = 0;
	cap->cman_wred_context_shared_n_max = 0;
	cap->cman_wred_context_shared_n_nodes_per_context_max = 0;
	cap->cman_wred_context_shared_n_contexts_per_node_max = 0;

	/**
	 * cap->mark_vlan_dei_supported = {0, 0, 0};
	 * cap->mark_ip_ecn_tcp_supported = {0, 0, 0};
	 * cap->mark_ip_ecn_sctp_supported = {0, 0, 0};
	 * cap->mark_ip_dscp_supported = {0, 0, 0};
	 */

	cap->dynamic_update_mask = 0;

	cap->stats_mask = 0;

	return 0;
}

/* Traffic manager level capabilities get */
static int
ipn3ke_tm_level_capabilities_get(struct rte_eth_dev *dev,
	uint32_t level_id, struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);

	if (cap == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_CAPABILITIES,
					NULL,
					rte_strerror(EINVAL));

	if (level_id >= IPN3KE_TM_NODE_LEVEL_MAX)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(*cap));

	switch (level_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		cap->n_nodes_max = hw->port_num;
		cap->n_nodes_nonleaf_max = IPN3KE_TM_VT_NODE_NUM;
		cap->n_nodes_leaf_max = 0;
		cap->non_leaf_nodes_identical = 0;
		cap->leaf_nodes_identical = 0;

		cap->nonleaf.shaper_private_supported = 0;
		cap->nonleaf.shaper_private_dual_rate_supported = 0;
		cap->nonleaf.shaper_private_rate_min = 1;
		cap->nonleaf.shaper_private_rate_max = UINT32_MAX;
		cap->nonleaf.shaper_private_packet_mode_supported = 0;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;
		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.shaper_shared_packet_mode_supported = 0;
		cap->nonleaf.shaper_shared_byte_mode_supported = 0;

		cap->nonleaf.sched_n_children_max = IPN3KE_TM_VT_NODE_NUM;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 0;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;

		cap->nonleaf.stats_mask = STATS_MASK_DEFAULT;
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		cap->n_nodes_max = IPN3KE_TM_VT_NODE_NUM;
		cap->n_nodes_nonleaf_max = IPN3KE_TM_COS_NODE_NUM;
		cap->n_nodes_leaf_max = 0;
		cap->non_leaf_nodes_identical = 0;
		cap->leaf_nodes_identical = 0;

		cap->nonleaf.shaper_private_supported = 0;
		cap->nonleaf.shaper_private_dual_rate_supported = 0;
		cap->nonleaf.shaper_private_rate_min = 1;
		cap->nonleaf.shaper_private_rate_max = UINT32_MAX;
		cap->nonleaf.shaper_private_packet_mode_supported = 0;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;
		cap->nonleaf.shaper_shared_n_max = 0;
		cap->nonleaf.shaper_shared_packet_mode_supported = 0;
		cap->nonleaf.shaper_shared_byte_mode_supported = 0;

		cap->nonleaf.sched_n_children_max = IPN3KE_TM_COS_NODE_NUM;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max = 0;
		cap->nonleaf.sched_wfq_n_groups_max = 0;
		cap->nonleaf.sched_wfq_weight_max = 0;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;

		cap->nonleaf.stats_mask = STATS_MASK_DEFAULT;
		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		cap->n_nodes_max = IPN3KE_TM_COS_NODE_NUM;
		cap->n_nodes_nonleaf_max = 0;
		cap->n_nodes_leaf_max = IPN3KE_TM_COS_NODE_NUM;
		cap->non_leaf_nodes_identical = 0;
		cap->leaf_nodes_identical = 0;

		cap->leaf.shaper_private_supported = 0;
		cap->leaf.shaper_private_dual_rate_supported = 0;
		cap->leaf.shaper_private_rate_min = 0;
		cap->leaf.shaper_private_rate_max = 0;
		cap->leaf.shaper_private_packet_mode_supported = 0;
		cap->leaf.shaper_private_byte_mode_supported = 1;
		cap->leaf.shaper_shared_n_max = 0;
		cap->leaf.shaper_shared_packet_mode_supported = 0;
		cap->leaf.shaper_shared_byte_mode_supported = 0;

		cap->leaf.cman_head_drop_supported = 0;
		cap->leaf.cman_wred_packet_mode_supported = WRED_SUPPORTED;
		cap->leaf.cman_wred_byte_mode_supported = 0;
		cap->leaf.cman_wred_context_private_supported = WRED_SUPPORTED;
		cap->leaf.cman_wred_context_shared_n_max = 0;

		cap->leaf.stats_mask = STATS_MASK_QUEUE;
		break;

	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
		break;
	}

	return 0;
}

/* Traffic manager node capabilities get */
static int
ipn3ke_tm_node_capabilities_get(struct rte_eth_dev *dev,
	uint32_t node_id, struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error)
{
	struct ipn3ke_rpst *representor = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node *tm_node;
	uint32_t state_mask;

	if (cap == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_CAPABILITIES,
					NULL,
					rte_strerror(EINVAL));

	tm_id = tm->tm_id;

	state_mask = 0;
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_COMMITTED);
	tm_node = ipn3ke_hw_tm_node_search(hw, tm_id, node_id, state_mask);
	if (tm_node == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	if (tm_node->tm_id != representor->port_id)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(*cap));

	switch (tm_node->level) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		cap->shaper_private_supported = 1;
		cap->shaper_private_dual_rate_supported = 0;
		cap->shaper_private_rate_min = 1;
		cap->shaper_private_rate_max = UINT32_MAX;
		cap->shaper_private_packet_mode_supported = 0;
		cap->shaper_private_byte_mode_supported = 1;
		cap->shaper_shared_n_max = 0;
		cap->shaper_shared_packet_mode_supported = 0;
		cap->shaper_shared_byte_mode_supported = 0;

		cap->nonleaf.sched_n_children_max = IPN3KE_TM_VT_NODE_NUM;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			IPN3KE_TM_VT_NODE_NUM;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;

		cap->stats_mask = STATS_MASK_DEFAULT;
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		cap->shaper_private_supported = 1;
		cap->shaper_private_dual_rate_supported = 0;
		cap->shaper_private_rate_min = 1;
		cap->shaper_private_rate_max = UINT32_MAX;
		cap->shaper_private_packet_mode_supported = 0;
		cap->shaper_private_byte_mode_supported = 1;
		cap->shaper_shared_n_max = 0;
		cap->shaper_shared_packet_mode_supported = 0;
		cap->shaper_shared_byte_mode_supported = 0;

		cap->nonleaf.sched_n_children_max = IPN3KE_TM_COS_NODE_NUM;
		cap->nonleaf.sched_sp_n_priorities_max = 1;
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			IPN3KE_TM_COS_NODE_NUM;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max = 1;
		cap->nonleaf.sched_wfq_packet_mode_supported = 0;
		cap->nonleaf.sched_wfq_byte_mode_supported = 0;

		cap->stats_mask = STATS_MASK_DEFAULT;
		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		cap->shaper_private_supported = 0;
		cap->shaper_private_dual_rate_supported = 0;
		cap->shaper_private_rate_min = 0;
		cap->shaper_private_rate_max = 0;
		cap->shaper_private_packet_mode_supported = 0;
		cap->shaper_private_byte_mode_supported = 0;
		cap->shaper_shared_n_max = 0;
		cap->shaper_shared_packet_mode_supported = 0;
		cap->shaper_shared_byte_mode_supported = 0;

		cap->leaf.cman_head_drop_supported = 0;
		cap->leaf.cman_wred_packet_mode_supported = WRED_SUPPORTED;
		cap->leaf.cman_wred_byte_mode_supported = 0;
		cap->leaf.cman_wred_context_private_supported = WRED_SUPPORTED;
		cap->leaf.cman_wred_context_shared_n_max = 0;

		cap->stats_mask = STATS_MASK_QUEUE;
		break;
	default:
		break;
	}

	return 0;
}

static int
ipn3ke_tm_shaper_parame_trans(struct rte_tm_shaper_params *profile,
	struct ipn3ke_tm_shaper_profile *local_profile,
	const struct ipn3ke_tm_shaper_params_range_type *ref_data)
{
	uint32_t i;
	const struct ipn3ke_tm_shaper_params_range_type *r;
	uint64_t rate;

	rate = profile->peak.rate;
	for (i = 0, r = ref_data; i < IPN3KE_TM_SHAPER_RANGE_NUM; i++, r++) {
		if (rate >= r->low &&
		rate <= r->high) {
			local_profile->m = (rate / 4) / r->exp2;
			local_profile->e = r->exp;
			local_profile->rate = rate;

			return 0;
		}
	}

	return -1;
}

static int
ipn3ke_tm_shaper_profile_add(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id, struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_shaper_profile *sp;

	/* Shaper profile must not exist. */
	sp = ipn3ke_hw_tm_shaper_profile_search(hw, shaper_profile_id, error);
	if (!sp || (sp && sp->valid))
		return -rte_tm_error_set(error,
					EEXIST,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EEXIST));

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE,
					NULL,
					rte_strerror(EINVAL));

	/* Peak rate: non-zero, 32-bit */
	if (profile->peak.rate == 0 ||
		profile->peak.rate > IPN3KE_TM_SHAPER_PEAK_RATE_MAX)
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
				NULL,
				rte_strerror(EINVAL));

	/* Peak size: non-zero, 32-bit */
	if (profile->peak.size != 0)
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
				NULL,
				rte_strerror(EINVAL));

	/* Dual-rate profiles are not supported. */
	if (profile->committed.rate > IPN3KE_TM_SHAPER_COMMITTED_RATE_MAX)
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE,
				NULL,
				rte_strerror(EINVAL));

	/* Packet length adjust: 24 bytes */
	if (profile->pkt_length_adjust != 0)
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN,
				NULL,
				rte_strerror(EINVAL));

	if (ipn3ke_tm_shaper_parame_trans(profile,
					sp,
					ipn3ke_tm_shaper_params_rang)) {
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
				NULL,
				rte_strerror(EINVAL));
	} else {
		sp->valid = 1;
		rte_memcpy(&sp->params, profile, sizeof(sp->params));
	}

	tm->h.n_shaper_profiles++;

	return 0;
}

/* Traffic manager shaper profile delete */
static int
ipn3ke_tm_shaper_profile_delete(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id, struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_shaper_profile *sp;

	/* Check existing */
	sp = ipn3ke_hw_tm_shaper_profile_search(hw, shaper_profile_id, error);
	if (!sp || (sp && !sp->valid))
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EINVAL));

	sp->valid = 0;
	tm->h.n_shaper_profiles--;

	return 0;
}

static int
ipn3ke_tm_tdrop_profile_check(__rte_unused struct rte_eth_dev *dev,
	uint32_t tdrop_profile_id, struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	enum rte_color color;

	/* TDROP profile ID must not be NONE. */
	if (tdrop_profile_id == RTE_TM_WRED_PROFILE_ID_NONE)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_WRED_PROFILE,
					NULL,
					rte_strerror(EINVAL));

	/* TDROP profile should be in packet mode */
	if (profile->packet_mode != 0)
		return -rte_tm_error_set(error,
					ENOTSUP,
					RTE_TM_ERROR_TYPE_WRED_PROFILE,
					NULL,
					rte_strerror(ENOTSUP));

	/* min_th <= max_th, max_th > 0  */
	for (color = RTE_COLOR_GREEN; color <= RTE_COLOR_GREEN; color++) {
		uint64_t min_th = profile->red_params[color].min_th;
		uint64_t max_th = profile->red_params[color].max_th;

		if (((min_th >> IPN3KE_TDROP_TH1_SHIFT) >>
				IPN3KE_TDROP_TH1_SHIFT) ||
			max_th != 0)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_WRED_PROFILE,
						NULL,
						rte_strerror(EINVAL));
	}

	return 0;
}

static int
ipn3ke_hw_tm_tdrop_wr(struct ipn3ke_hw *hw,
				struct ipn3ke_tm_tdrop_profile *tp)
{
	if (tp->valid) {
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CCB_PROFILE_MS,
				0,
				tp->th2,
				IPN3KE_CCB_PROFILE_MS_MASK);

		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CCB_PROFILE_P,
				tp->tdrop_profile_id,
				tp->th1,
				IPN3KE_CCB_PROFILE_MASK);
	} else {
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CCB_PROFILE_MS,
				0,
				0,
				IPN3KE_CCB_PROFILE_MS_MASK);

		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CCB_PROFILE_P,
				tp->tdrop_profile_id,
				0,
				IPN3KE_CCB_PROFILE_MASK);
	}

	return 0;
}

/* Traffic manager TDROP profile add */
static int
ipn3ke_tm_tdrop_profile_add(struct rte_eth_dev *dev,
	uint32_t tdrop_profile_id, struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_tdrop_profile *tp;
	int status;
	uint64_t min_th;
	uint32_t th1, th2;

	/* Check input params */
	status = ipn3ke_tm_tdrop_profile_check(dev,
					tdrop_profile_id,
					profile,
					error);
	if (status)
		return status;

	/* Memory allocation */
	tp = &hw->tdrop_profile[tdrop_profile_id];

	/* Fill in */
	tp->valid = 1;
	min_th = profile->red_params[RTE_COLOR_GREEN].min_th;
	th1 = (uint32_t)(min_th & IPN3KE_TDROP_TH1_MASK);
	th2 = (uint32_t)((min_th >> IPN3KE_TDROP_TH1_SHIFT) &
			IPN3KE_TDROP_TH2_MASK);
	tp->th1 = th1;
	tp->th2 = th2;
	rte_memcpy(&tp->params, profile, sizeof(tp->params));

	/* Add to list */
	tm->h.n_tdrop_profiles++;

	/* Write FPGA */
	ipn3ke_hw_tm_tdrop_wr(hw, tp);

	return 0;
}

/* Traffic manager TDROP profile delete */
static int
ipn3ke_tm_tdrop_profile_delete(struct rte_eth_dev *dev,
	uint32_t tdrop_profile_id, struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_tdrop_profile *tp;

	/* Check existing */
	tp = ipn3ke_hw_tm_tdrop_profile_search(hw, tdrop_profile_id);
	if (tp == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* Check unused */
	if (tp->n_users)
		return -rte_tm_error_set(error,
					EBUSY,
					RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
					NULL,
					rte_strerror(EBUSY));

	/* Set free */
	tp->valid = 0;
	tm->h.n_tdrop_profiles--;

	/* Write FPGA */
	ipn3ke_hw_tm_tdrop_wr(hw, tp);

	return 0;
}

static int
ipn3ke_tm_node_add_check_parameter(uint32_t tm_id,
	uint32_t node_id, uint32_t parent_node_id, uint32_t priority,
	uint32_t weight, uint32_t level_id, struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	uint32_t level_of_node_id;
	uint32_t node_index;
	uint32_t parent_level_id;

	if (node_id == RTE_TM_NODE_ID_NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* priority: must be 0, 1, 2, 3 */
	if (priority > IPN3KE_TM_NODE_PRIORITY_HIGHEST)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PRIORITY,
					NULL,
					rte_strerror(EINVAL));

	/* weight: must be 1 .. 255 */
	if (weight > IPN3KE_TM_NODE_WEIGHT_MAX)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_WEIGHT,
					NULL,
					rte_strerror(EINVAL));

	/* check node id and parent id*/
	level_of_node_id = node_id / IPN3KE_TM_NODE_LEVEL_MOD;
	if (level_of_node_id != level_id)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
	node_index = node_id % IPN3KE_TM_NODE_LEVEL_MOD;
	parent_level_id = parent_node_id / IPN3KE_TM_NODE_LEVEL_MOD;
	switch (level_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		if (node_index != tm_id)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		if (parent_node_id != RTE_TM_NODE_ID_NULL)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_index >= IPN3KE_TM_VT_NODE_NUM)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		if (parent_level_id != IPN3KE_TM_NODE_LEVEL_PORT)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_index >= IPN3KE_TM_COS_NODE_NUM)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		if (parent_level_id != IPN3KE_TM_NODE_LEVEL_VT)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		break;
	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
	}

	/* params: must not be NULL */
	if (params == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARAMS,
					NULL,
					rte_strerror(EINVAL));
	/* No shared shapers */
	if (params->n_shared_shapers != 0)
		return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
				NULL,
				rte_strerror(EINVAL));
	return 0;
}

static int
ipn3ke_tm_node_add_check_mount(uint32_t tm_id,
	uint32_t node_id, uint32_t parent_node_id, uint32_t level_id,
	struct rte_tm_error *error)
{
	uint32_t node_index;
	uint32_t parent_index;
	uint32_t parent_index1;

	node_index = node_id % IPN3KE_TM_NODE_LEVEL_MOD;
	parent_index = parent_node_id % IPN3KE_TM_NODE_LEVEL_MOD;
	parent_index1 = node_index / IPN3KE_TM_NODE_MOUNT_MAX;
	switch (level_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (parent_index != tm_id)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (parent_index != parent_index1)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		break;
	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
	}

	return 0;
}

/* Traffic manager node add */
static int
ipn3ke_tm_node_add(struct rte_eth_dev *dev,
	uint32_t node_id, uint32_t parent_node_id, uint32_t priority,
	uint32_t weight, uint32_t level_id, struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node *n, *parent_node;
	uint32_t node_state, state_mask;
	int status;

	/* Checks */
	if (tm->hierarchy_frozen)
		return -rte_tm_error_set(error,
					EBUSY,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EBUSY));

	tm_id = tm->tm_id;

	status = ipn3ke_tm_node_add_check_parameter(tm_id,
						node_id,
						parent_node_id,
						priority,
						weight,
						level_id,
						params,
						error);
	if (status)
		return status;

	status = ipn3ke_tm_node_add_check_mount(tm_id,
						node_id,
						parent_node_id,
						level_id,
						error);
	if (status)
		return status;

	/* Shaper profile ID must not be NONE. */
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE &&
		params->shaper_profile_id != node_id)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* Memory allocation */
	state_mask = 0;
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_IDLE);
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_CONFIGURED_DEL);
	n = ipn3ke_hw_tm_node_search(hw, tm_id, node_id, state_mask);
	if (!n)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));
	node_state = n->node_state;

	/* Check parent node */
	state_mask = 0;
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_CONFIGURED_ADD);
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_COMMITTED);
	if (parent_node_id != RTE_TM_NODE_ID_NULL) {
		parent_node = ipn3ke_hw_tm_node_search(hw,
							tm_id,
							parent_node_id,
							state_mask);
		if (!parent_node)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
	} else {
		parent_node = NULL;
	}

	switch (level_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_ADD;
		n->tm_id = tm_id;
		tm->h.port_commit_node = n;
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_state == IPN3KE_TM_NODE_STATE_IDLE) {
			TAILQ_INSERT_TAIL(&tm->h.vt_commit_node_list, n, node);
			if (parent_node)
				parent_node->n_children++;
			tm->h.n_vt_nodes++;
		} else if (node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			if (parent_node)
				parent_node->n_children++;
			tm->h.n_vt_nodes++;
		}
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_ADD;
		n->parent_node_id = parent_node_id;
		n->tm_id = tm_id;
		n->parent_node = parent_node;

		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_state == IPN3KE_TM_NODE_STATE_IDLE) {
			TAILQ_INSERT_TAIL(&tm->h.cos_commit_node_list,
				n, node);
			if (parent_node)
				parent_node->n_children++;
			tm->h.n_cos_nodes++;
		} else if (node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			if (parent_node)
				parent_node->n_children++;
			tm->h.n_cos_nodes++;
		}
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_ADD;
		n->parent_node_id = parent_node_id;
		n->tm_id = tm_id;
		n->parent_node = parent_node;

		break;
	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
	}

	/* Fill in */
	n->priority = priority;
	n->weight = weight;

	if (n->level == IPN3KE_TM_NODE_LEVEL_COS &&
		params->leaf.cman == RTE_TM_CMAN_TAIL_DROP)
		n->tdrop_profile = ipn3ke_hw_tm_tdrop_profile_search(hw,
			params->leaf.wred.wred_profile_id);

	rte_memcpy(&n->params, params, sizeof(n->params));

	return 0;
}

static int
ipn3ke_tm_node_del_check_parameter(uint32_t tm_id,
	uint32_t node_id, struct rte_tm_error *error)
{
	uint32_t level_of_node_id;
	uint32_t node_index;

	if (node_id == RTE_TM_NODE_ID_NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	/* check node id and parent id*/
	level_of_node_id = node_id / IPN3KE_TM_NODE_LEVEL_MOD;
	node_index = node_id % IPN3KE_TM_NODE_LEVEL_MOD;
	switch (level_of_node_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		if (node_index != tm_id)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_index >= IPN3KE_TM_VT_NODE_NUM)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_index >= IPN3KE_TM_COS_NODE_NUM)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		break;
	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
	}

	return 0;
}

/* Traffic manager node delete */
static int
ipn3ke_pmd_tm_node_delete(struct rte_eth_dev *dev,
	uint32_t node_id, struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_node *n, *parent_node;
	uint32_t tm_id;
	int status;
	uint32_t level_of_node_id;
	uint32_t node_state;
	uint32_t state_mask;

	/* Check hierarchy changes are currently allowed */
	if (tm->hierarchy_frozen)
		return -rte_tm_error_set(error,
					EBUSY,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EBUSY));

	tm_id = tm->tm_id;

	status = ipn3ke_tm_node_del_check_parameter(tm_id,
						node_id,
						error);
	if (status)
		return status;

	/* Check existing */
	state_mask = 0;
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_CONFIGURED_ADD);
	IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_COMMITTED);
	n = ipn3ke_hw_tm_node_search(hw, tm_id, node_id, state_mask);
	if (n == NULL)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	if (n->n_children > 0)
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_ID,
					NULL,
					rte_strerror(EINVAL));

	node_state = n->node_state;

	level_of_node_id = node_id / IPN3KE_TM_NODE_LEVEL_MOD;

	/* Check parent node */
	if (n->parent_node_id != RTE_TM_NODE_ID_NULL) {
		state_mask = 0;
		IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_CONFIGURED_ADD);
		IPN3KE_BIT_SET(state_mask, IPN3KE_TM_NODE_STATE_COMMITTED);
		parent_node = ipn3ke_hw_tm_node_search(hw,
						tm_id,
						n->parent_node_id,
						state_mask);
		if (!parent_node)
			return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
					NULL,
					rte_strerror(EINVAL));
		if (n->parent_node != parent_node)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
	} else {
		parent_node = NULL;
	}

	switch (level_of_node_id) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		if (tm->h.port_node != n)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_NODE_ID,
						NULL,
						rte_strerror(EINVAL));
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_DEL;
		tm->h.port_commit_node = n;

		break;

	case IPN3KE_TM_NODE_LEVEL_VT:
		if (node_state == IPN3KE_TM_NODE_STATE_COMMITTED) {
			if (parent_node)
				TAILQ_REMOVE(&parent_node->children_node_list,
					n, node);
			TAILQ_INSERT_TAIL(&tm->h.vt_commit_node_list, n, node);
			if (parent_node)
				parent_node->n_children--;
			tm->h.n_vt_nodes--;
		} else if (node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			if (parent_node)
				parent_node->n_children--;
			tm->h.n_vt_nodes--;
		}
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_DEL;

		break;

	case IPN3KE_TM_NODE_LEVEL_COS:
		if (node_state == IPN3KE_TM_NODE_STATE_COMMITTED) {
			if (parent_node)
				TAILQ_REMOVE(&parent_node->children_node_list,
					n, node);
			TAILQ_INSERT_TAIL(&tm->h.cos_commit_node_list,
				n, node);
			if (parent_node)
				parent_node->n_children--;
			tm->h.n_cos_nodes--;
		} else if (node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			if (parent_node)
				parent_node->n_children--;
			tm->h.n_cos_nodes--;
		}
		n->node_state = IPN3KE_TM_NODE_STATE_CONFIGURED_DEL;

		break;
	default:
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_LEVEL_ID,
					NULL,
					rte_strerror(EINVAL));
	}

	return 0;
}

static int
ipn3ke_tm_hierarchy_commit_check(struct rte_eth_dev *dev,
						struct rte_tm_error *error)
{
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node_list *nl;
	struct ipn3ke_tm_node *n, *parent_node;

	tm_id = tm->tm_id;

	nl = &tm->h.cos_commit_node_list;
	TAILQ_FOREACH(n, nl, node) {
		parent_node = n->parent_node;
		if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			if (n->parent_node_id == RTE_TM_NODE_ID_NULL ||
				n->level != IPN3KE_TM_NODE_LEVEL_COS ||
				n->tm_id != tm_id ||
				parent_node == NULL ||
				(parent_node &&
					parent_node->node_state ==
					IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) ||
				(parent_node &&
					parent_node->node_state ==
						IPN3KE_TM_NODE_STATE_IDLE) ||
				n->shaper_profile.valid == 0) {
				return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
			}
		} else if (n->node_state ==
				IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			if (n->level != IPN3KE_TM_NODE_LEVEL_COS ||
				n->n_children != 0) {
				return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
			}
		}
	}

	nl = &tm->h.vt_commit_node_list;
	TAILQ_FOREACH(n, nl, node) {
		parent_node = n->parent_node;
		if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			if (n->parent_node_id == RTE_TM_NODE_ID_NULL ||
				n->level != IPN3KE_TM_NODE_LEVEL_VT ||
				n->tm_id != tm_id ||
				parent_node == NULL ||
				(parent_node &&
					parent_node->node_state ==
					IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) ||
				(parent_node &&
					parent_node->node_state ==
						IPN3KE_TM_NODE_STATE_IDLE) ||
				n->shaper_profile.valid == 0) {
				return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
			}
		} else if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_DEL)
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
	}

	n = tm->h.port_commit_node;
	if (n &&
		(n->parent_node_id != RTE_TM_NODE_ID_NULL ||
		n->level != IPN3KE_TM_NODE_LEVEL_PORT ||
		n->tm_id != tm_id ||
		n->parent_node != NULL ||
		n->shaper_profile.valid == 0)) {
		return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));
	}

	return 0;
}

static int
ipn3ke_hw_tm_node_wr(struct ipn3ke_hw *hw,
	struct ipn3ke_tm_node *n,
	struct ipn3ke_tm_node *parent_node)
{
	uint32_t level;

	level = n->level;

	switch (level) {
	case IPN3KE_TM_NODE_LEVEL_PORT:
		/**
		 * Configure Type
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_TYPE_L3_X,
				n->node_index,
				n->priority,
				IPN3KE_QOS_TYPE_MASK);

		/**
		 * Configure Sch_wt
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_SCH_WT_L3_X,
				n->node_index,
				n->weight,
				IPN3KE_QOS_SCH_WT_MASK);

		/**
		 * Configure Shap_wt
		 */
		if (n->shaper_profile.valid)
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_QOS_SHAP_WT_L3_X,
					n->node_index,
					((n->shaper_profile.e << 10) |
						n->shaper_profile.m),
					IPN3KE_QOS_SHAP_WT_MASK);

		break;
	case IPN3KE_TM_NODE_LEVEL_VT:
		/**
		 * Configure Type
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_TYPE_L2_X,
				n->node_index,
				n->priority,
				IPN3KE_QOS_TYPE_MASK);

		/**
		 * Configure Sch_wt
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_SCH_WT_L2_X,
				n->node_index,
				n->weight,
				IPN3KE_QOS_SCH_WT_MASK);

		/**
		 * Configure Shap_wt
		 */
		if (n->shaper_profile.valid)
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_QOS_SHAP_WT_L2_X,
					n->node_index,
					((n->shaper_profile.e << 10) |
						n->shaper_profile.m),
					IPN3KE_QOS_SHAP_WT_MASK);

		/**
		 * Configure Map
		 */
		if (parent_node)
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_QOS_MAP_L2_X,
					n->node_index,
					parent_node->node_index,
					IPN3KE_QOS_MAP_L2_MASK);

		break;
	case IPN3KE_TM_NODE_LEVEL_COS:
		/**
		 * Configure Tail Drop mapping
		 */
		if (n->tdrop_profile && n->tdrop_profile->valid) {
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_CCB_QPROFILE_Q,
					n->node_index,
					n->tdrop_profile->tdrop_profile_id,
					IPN3KE_CCB_QPROFILE_MASK);
		}

		/**
		 * Configure Type
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_TYPE_L1_X,
				n->node_index,
				n->priority,
				IPN3KE_QOS_TYPE_MASK);

		/**
		 * Configure Sch_wt
		 */
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QOS_SCH_WT_L1_X,
				n->node_index,
				n->weight,
				IPN3KE_QOS_SCH_WT_MASK);

		/**
		 * Configure Shap_wt
		 */
		if (n->shaper_profile.valid)
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_QOS_SHAP_WT_L1_X,
					n->node_index,
					((n->shaper_profile.e << 10) |
						n->shaper_profile.m),
					IPN3KE_QOS_SHAP_WT_MASK);

		/**
		 * Configure COS queue to port
		 */
		while (IPN3KE_MASK_READ_REG(hw,
					IPN3KE_QM_UID_CONFIG_CTRL,
					0,
					0x80000000))
			;

		if (parent_node && parent_node->parent_node)
			IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QM_UID_CONFIG_DATA,
				0,
				(1 << 8 | parent_node->parent_node->node_index),
				0x1FF);

		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_QM_UID_CONFIG_CTRL,
				0,
				n->node_index,
				0xFFFFF);

		while (IPN3KE_MASK_READ_REG(hw,
					IPN3KE_QM_UID_CONFIG_CTRL,
					0,
					0x80000000))
			;

		/**
		 * Configure Map
		 */
		if (parent_node)
			IPN3KE_MASK_WRITE_REG(hw,
					IPN3KE_QOS_MAP_L1_X,
					n->node_index,
					parent_node->node_index,
					IPN3KE_QOS_MAP_L1_MASK);

		break;
	default:
		return -1;
	}

	return 0;
}

static int
ipn3ke_tm_hierarchy_hw_commit(struct rte_eth_dev *dev,
					struct rte_tm_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_node_list *nl;
	struct ipn3ke_tm_node *n, *nn, *parent_node;

	n = tm->h.port_commit_node;
	if (n) {
		if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			tm->h.port_commit_node = NULL;

			n->node_state = IPN3KE_TM_NODE_STATE_COMMITTED;
		} else if (n->node_state ==
					IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			tm->h.port_commit_node = NULL;

			n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
			n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
			n->weight = 0;
			n->tm_id = RTE_TM_NODE_ID_NULL;
		} else {
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
		}
		parent_node = n->parent_node;
		ipn3ke_hw_tm_node_wr(hw, n, parent_node);
	}

	nl = &tm->h.vt_commit_node_list;
	for (n = TAILQ_FIRST(nl); n != NULL; n = nn) {
		nn = TAILQ_NEXT(n, node);
		if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			n->node_state = IPN3KE_TM_NODE_STATE_COMMITTED;
			parent_node = n->parent_node;
			TAILQ_REMOVE(nl, n, node);
			TAILQ_INSERT_TAIL(&parent_node->children_node_list,
						n, node);
		} else if (n->node_state ==
					IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			parent_node = n->parent_node;
			TAILQ_REMOVE(nl, n, node);

			n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
			n->parent_node_id = RTE_TM_NODE_ID_NULL;
			n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
			n->weight = 0;
			n->tm_id = RTE_TM_NODE_ID_NULL;
			n->parent_node = NULL;
		} else {
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
		}
		ipn3ke_hw_tm_node_wr(hw, n, parent_node);
	}

	nl = &tm->h.cos_commit_node_list;
	for (n = TAILQ_FIRST(nl); n != NULL; n = nn) {
		nn = TAILQ_NEXT(n, node);
		if (n->node_state == IPN3KE_TM_NODE_STATE_CONFIGURED_ADD) {
			n->node_state = IPN3KE_TM_NODE_STATE_COMMITTED;
			parent_node = n->parent_node;
			TAILQ_REMOVE(nl, n, node);
			TAILQ_INSERT_TAIL(&parent_node->children_node_list,
					n, node);
		} else if (n->node_state ==
					IPN3KE_TM_NODE_STATE_CONFIGURED_DEL) {
			n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
			parent_node = n->parent_node;
			TAILQ_REMOVE(nl, n, node);

			n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
			n->parent_node_id = RTE_TM_NODE_ID_NULL;
			n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
			n->weight = 0;
			n->tm_id = RTE_TM_NODE_ID_NULL;
			n->parent_node = NULL;

			if (n->tdrop_profile)
				n->tdrop_profile->n_users--;
		} else {
			return -rte_tm_error_set(error,
						EINVAL,
						RTE_TM_ERROR_TYPE_UNSPECIFIED,
						NULL,
						rte_strerror(EINVAL));
		}
		ipn3ke_hw_tm_node_wr(hw, n, parent_node);
	}

	return 0;
}

static int
ipn3ke_tm_hierarchy_commit_clear(struct rte_eth_dev *dev)
{
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	struct ipn3ke_tm_node_list *nl;
	struct ipn3ke_tm_node *n;
	struct ipn3ke_tm_node *nn;

	n = tm->h.port_commit_node;
	if (n) {
		n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		n->weight = 0;
		n->tm_id = RTE_TM_NODE_ID_NULL;
		n->n_children = 0;

		tm->h.port_commit_node = NULL;
	}

	nl = &tm->h.vt_commit_node_list;
	for (n = TAILQ_FIRST(nl); n != NULL; n = nn) {
		nn = TAILQ_NEXT(n, node);

		n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		n->parent_node_id = RTE_TM_NODE_ID_NULL;
		n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		n->weight = 0;
		n->tm_id = RTE_TM_NODE_ID_NULL;
		n->parent_node = NULL;
		n->n_children = 0;
		tm->h.n_vt_nodes--;

		TAILQ_REMOVE(nl, n, node);
	}

	nl = &tm->h.cos_commit_node_list;
	for (n = TAILQ_FIRST(nl); n != NULL; n = nn) {
		nn = TAILQ_NEXT(n, node);

		n->node_state = IPN3KE_TM_NODE_STATE_IDLE;
		n->parent_node_id = RTE_TM_NODE_ID_NULL;
		n->priority = IPN3KE_TM_NODE_PRIORITY_NORMAL0;
		n->weight = 0;
		n->tm_id = RTE_TM_NODE_ID_NULL;
		n->parent_node = NULL;
		tm->h.n_cos_nodes--;

		TAILQ_REMOVE(nl, n, node);
	}

	return 0;
}

static void
ipn3ke_tm_show(struct rte_eth_dev *dev)
{
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node_list *vt_nl, *cos_nl;
	struct ipn3ke_tm_node *port_n, *vt_n, *cos_n;
	const char *str_state[IPN3KE_TM_NODE_STATE_MAX] = {"Idle",
						"CfgAdd",
						"CfgDel",
						"Committed"};

	tm_id = tm->tm_id;

	IPN3KE_AFU_PMD_DEBUG("***HQoS Tree(%d)***\n", tm_id);

	port_n = tm->h.port_node;
	IPN3KE_AFU_PMD_DEBUG("Port: (%d|%s)\n", port_n->node_index,
				str_state[port_n->node_state]);

	vt_nl = &tm->h.port_node->children_node_list;
	TAILQ_FOREACH(vt_n, vt_nl, node) {
		cos_nl = &vt_n->children_node_list;
		IPN3KE_AFU_PMD_DEBUG("    VT%d: ", vt_n->node_index);
		TAILQ_FOREACH(cos_n, cos_nl, node) {
			if (cos_n->parent_node_id !=
				(vt_n->node_index + IPN3KE_TM_NODE_LEVEL_MOD))
				IPN3KE_AFU_PMD_ERR("(%d|%s), ",
					cos_n->node_index,
					str_state[cos_n->node_state]);
		}
		IPN3KE_AFU_PMD_DEBUG("\n");
	}
}

static void
ipn3ke_tm_show_commmit(struct rte_eth_dev *dev)
{
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	uint32_t tm_id;
	struct ipn3ke_tm_node_list *nl;
	struct ipn3ke_tm_node *n;
	const char *str_state[IPN3KE_TM_NODE_STATE_MAX] = {"Idle",
						"CfgAdd",
						"CfgDel",
						"Committed"};

	tm_id = tm->tm_id;

	IPN3KE_AFU_PMD_DEBUG("***Commit Tree(%d)***\n", tm_id);
	n = tm->h.port_commit_node;
	IPN3KE_AFU_PMD_DEBUG("Port: ");
	if (n)
		IPN3KE_AFU_PMD_DEBUG("(%d|%s)",
			n->node_index,
			str_state[n->node_state]);
	IPN3KE_AFU_PMD_DEBUG("\n");

	nl = &tm->h.vt_commit_node_list;
	IPN3KE_AFU_PMD_DEBUG("VT  : ");
	TAILQ_FOREACH(n, nl, node) {
		IPN3KE_AFU_PMD_DEBUG("(%d|%s), ",
				n->node_index,
				str_state[n->node_state]);
	}
	IPN3KE_AFU_PMD_DEBUG("\n");

	nl = &tm->h.cos_commit_node_list;
	IPN3KE_AFU_PMD_DEBUG("COS : ");
	TAILQ_FOREACH(n, nl, node) {
		IPN3KE_AFU_PMD_DEBUG("(%d|%s), ",
				n->node_index,
				str_state[n->node_state]);
	}
	IPN3KE_AFU_PMD_DEBUG("\n");
}

/* Traffic manager hierarchy commit */
static int
ipn3ke_tm_hierarchy_commit(struct rte_eth_dev *dev,
	int clear_on_fail, struct rte_tm_error *error)
{
	struct ipn3ke_tm_internals *tm = IPN3KE_DEV_PRIVATE_TO_TM(dev);
	int status;

	/* Checks */
	if (tm->hierarchy_frozen)
		return -rte_tm_error_set(error,
					EBUSY,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EBUSY));

	ipn3ke_tm_show_commmit(dev);

	status = ipn3ke_tm_hierarchy_commit_check(dev, error);
	if (status) {
		if (clear_on_fail)
			ipn3ke_tm_hierarchy_commit_clear(dev);
		return status;
	}

	ipn3ke_tm_hierarchy_hw_commit(dev, error);
	ipn3ke_tm_show(dev);

	return 0;
}

const struct rte_tm_ops ipn3ke_tm_ops = {
	.node_type_get = ipn3ke_pmd_tm_node_type_get,
	.capabilities_get = ipn3ke_tm_capabilities_get,
	.level_capabilities_get = ipn3ke_tm_level_capabilities_get,
	.node_capabilities_get = ipn3ke_tm_node_capabilities_get,

	.wred_profile_add = ipn3ke_tm_tdrop_profile_add,
	.wred_profile_delete = ipn3ke_tm_tdrop_profile_delete,
	.shared_wred_context_add_update = NULL,
	.shared_wred_context_delete = NULL,

	.shaper_profile_add = ipn3ke_tm_shaper_profile_add,
	.shaper_profile_delete = ipn3ke_tm_shaper_profile_delete,
	.shared_shaper_add_update = NULL,
	.shared_shaper_delete = NULL,

	.node_add = ipn3ke_tm_node_add,
	.node_delete = ipn3ke_pmd_tm_node_delete,
	.node_suspend = NULL,
	.node_resume = NULL,
	.hierarchy_commit = ipn3ke_tm_hierarchy_commit,

	.node_parent_update = NULL,
	.node_shaper_update = NULL,
	.node_shared_shaper_update = NULL,
	.node_stats_update = NULL,
	.node_wfq_weight_mode_update = NULL,
	.node_cman_update = NULL,
	.node_wred_context_update = NULL,
	.node_shared_wred_context_update = NULL,

	.node_stats_read = NULL,
};

int
ipn3ke_tm_ops_get(struct rte_eth_dev *ethdev,
		void *arg)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct rte_eth_dev *i40e_pf_eth;
	const struct rte_tm_ops *ops;

	if (!arg)
		return -EINVAL;

	if (hw->acc_tm) {
		*(const void **)arg = &ipn3ke_tm_ops;
	} else if (rpst->i40e_pf_eth) {
		i40e_pf_eth = rpst->i40e_pf_eth;
		if (i40e_pf_eth->dev_ops->tm_ops_get == NULL ||
			i40e_pf_eth->dev_ops->tm_ops_get(i40e_pf_eth,
			&ops) != 0 ||
			ops == NULL) {
			return -EINVAL;
		}
		*(const void **)arg = ops;
	} else {
		return -EINVAL;
	}

	return 0;
}
