/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>

#include <rte_errno.h>
#include "ethdev_trace.h"
#include "rte_ethdev.h"
#include "rte_tm_driver.h"
#include "rte_tm.h"

/* Get generic traffic manager operations structure from a port. */
const struct rte_tm_ops *
rte_tm_ops_get(uint16_t port_id, struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_tm_ops *ops;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_tm_error_set(error,
			ENODEV,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENODEV));
		return NULL;
	}

	if ((dev->dev_ops->tm_ops_get == NULL) ||
		(dev->dev_ops->tm_ops_get(dev, &ops) != 0) ||
		(ops == NULL)) {
		rte_tm_error_set(error,
			ENOSYS,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENOSYS));
		return NULL;
	}

	return ops;
}

#define RTE_TM_FUNC(port_id, func)				\
({							\
	const struct rte_tm_ops *ops =			\
		rte_tm_ops_get(port_id, error);		\
	if (ops == NULL)					\
		return -rte_errno;			\
							\
	if (ops->func == NULL)				\
		return -rte_tm_error_set(error,		\
			ENOSYS,				\
			RTE_TM_ERROR_TYPE_UNSPECIFIED,	\
			NULL,				\
			rte_strerror(ENOSYS));		\
							\
	ops->func;					\
})

/* Get number of leaf nodes */
int
rte_tm_get_number_of_leaf_nodes(uint16_t port_id,
	uint32_t *n_leaf_nodes,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_tm_ops *ops =
		rte_tm_ops_get(port_id, error);

	if (ops == NULL)
		return -rte_errno;

	if (n_leaf_nodes == NULL) {
		rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));
		return -rte_errno;
	}

	*n_leaf_nodes = dev->data->nb_tx_queues;

	rte_tm_trace_get_number_of_leaf_nodes(port_id, *n_leaf_nodes);

	return 0;
}

/* Check node type (leaf or non-leaf) */
int
rte_tm_node_type_get(uint16_t port_id,
	uint32_t node_id,
	int *is_leaf,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_type_get)(dev,
		node_id, is_leaf, error);

	rte_tm_trace_node_type_get(port_id, node_id, *is_leaf, ret);

	return ret;
}

/* Get capabilities */
int rte_tm_capabilities_get(uint16_t port_id,
	struct rte_tm_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, capabilities_get)(dev,
		cap, error);

	rte_tm_trace_capabilities_get(port_id, cap, ret);

	return ret;
}

/* Get level capabilities */
int rte_tm_level_capabilities_get(uint16_t port_id,
	uint32_t level_id,
	struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, level_capabilities_get)(dev,
		level_id, cap, error);

	rte_tm_trace_level_capabilities_get(port_id, level_id, cap, ret);

	return ret;
}

/* Get node capabilities */
int rte_tm_node_capabilities_get(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_capabilities_get)(dev,
		node_id, cap, error);

	rte_tm_trace_node_capabilities_get(port_id, node_id, cap, ret);

	return ret;
}

/* Add WRED profile */
int rte_tm_wred_profile_add(uint16_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, wred_profile_add)(dev,
		wred_profile_id, profile, error);

	rte_tm_trace_wred_profile_add(port_id, wred_profile_id, profile, ret);

	return ret;
}

/* Delete WRED profile */
int rte_tm_wred_profile_delete(uint16_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, wred_profile_delete)(dev,
		wred_profile_id, error);

	rte_tm_trace_wred_profile_delete(port_id, wred_profile_id, ret);

	return ret;
}

/* Add/update shared WRED context */
int rte_tm_shared_wred_context_add_update(uint16_t port_id,
	uint32_t shared_wred_context_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shared_wred_context_add_update)(dev,
		shared_wred_context_id, wred_profile_id, error);

	rte_tm_trace_shared_wred_context_add_update(port_id,
						    shared_wred_context_id,
						    wred_profile_id, ret);

	return ret;
}

/* Delete shared WRED context */
int rte_tm_shared_wred_context_delete(uint16_t port_id,
	uint32_t shared_wred_context_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shared_wred_context_delete)(dev,
		shared_wred_context_id, error);

	rte_tm_trace_shared_wred_context_delete(port_id,
						shared_wred_context_id, ret);

	return ret;
}

/* Add shaper profile */
int rte_tm_shaper_profile_add(uint16_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shaper_profile_add)(dev,
		shaper_profile_id, profile, error);

	rte_tm_trace_shaper_profile_add(port_id, shaper_profile_id, profile,
					ret);

	return ret;
}

/* Delete WRED profile */
int rte_tm_shaper_profile_delete(uint16_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shaper_profile_delete)(dev,
		shaper_profile_id, error);

	rte_tm_trace_shaper_profile_delete(port_id, shaper_profile_id, ret);

	return ret;
}

/* Add shared shaper */
int rte_tm_shared_shaper_add_update(uint16_t port_id,
	uint32_t shared_shaper_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shared_shaper_add_update)(dev,
		shared_shaper_id, shaper_profile_id, error);

	rte_tm_trace_shared_shaper_add_update(port_id, shared_shaper_id,
					      shaper_profile_id, ret);

	return ret;
}

/* Delete shared shaper */
int rte_tm_shared_shaper_delete(uint16_t port_id,
	uint32_t shared_shaper_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, shared_shaper_delete)(dev,
		shared_shaper_id, error);

	rte_tm_trace_shared_shaper_delete(port_id, shared_shaper_id, ret);

	return ret;
}

/* Add node to port traffic manager hierarchy */
int rte_tm_node_add(uint16_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_add)(dev,
		node_id, parent_node_id, priority, weight, level_id,
		params, error);

	rte_tm_trace_node_add(port_id, node_id, parent_node_id, priority,
			      weight, level_id, params, ret);

	return ret;
}

/* Delete node from traffic manager hierarchy */
int rte_tm_node_delete(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_delete)(dev,
		node_id, error);

	rte_tm_trace_node_delete(port_id, node_id, ret);

	return ret;
}

/* Suspend node */
int rte_tm_node_suspend(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_suspend)(dev,
		node_id, error);

	rte_tm_trace_node_suspend(port_id, node_id, ret);

	return ret;
}

/* Resume node */
int rte_tm_node_resume(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_resume)(dev,
		node_id, error);

	rte_tm_trace_node_resume(port_id, node_id, ret);

	return ret;
}

/* Commit the initial port traffic manager hierarchy */
int rte_tm_hierarchy_commit(uint16_t port_id,
	int clear_on_fail,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, hierarchy_commit)(dev,
		clear_on_fail, error);

	rte_tm_trace_hierarchy_commit(port_id, clear_on_fail, ret);

	return ret;
}

/* Update node parent  */
int rte_tm_node_parent_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_parent_update)(dev,
		node_id, parent_node_id, priority, weight, error);

	rte_tm_trace_node_parent_update(port_id, node_id, parent_node_id,
					priority, weight, ret);

	return ret;
}

/* Update node private shaper */
int rte_tm_node_shaper_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_shaper_update)(dev,
		node_id, shaper_profile_id, error);

	rte_tm_trace_node_shaper_update(port_id, node_id, shaper_profile_id,
					ret);

	return ret;
}

/* Update node shared shapers */
int rte_tm_node_shared_shaper_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shared_shaper_id,
	int add,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_shared_shaper_update)(dev,
		node_id, shared_shaper_id, add, error);

	rte_tm_trace_node_shared_shaper_update(port_id, node_id,
					       shared_shaper_id, add, ret);

	return ret;
}

/* Update node stats */
int rte_tm_node_stats_update(uint16_t port_id,
	uint32_t node_id,
	uint64_t stats_mask,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_stats_update)(dev,
		node_id, stats_mask, error);

	rte_tm_trace_node_stats_update(port_id, node_id, stats_mask, ret);

	return ret;
}

/* Update WFQ weight mode */
int rte_tm_node_wfq_weight_mode_update(uint16_t port_id,
	uint32_t node_id,
	int *wfq_weight_mode,
	uint32_t n_sp_priorities,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_wfq_weight_mode_update)(dev,
		node_id, wfq_weight_mode, n_sp_priorities, error);

	rte_tm_trace_node_wfq_weight_mode_update(port_id, node_id,
						 wfq_weight_mode,
						 n_sp_priorities, ret);

	return ret;
}

/* Update node congestion management mode */
int rte_tm_node_cman_update(uint16_t port_id,
	uint32_t node_id,
	enum rte_tm_cman_mode cman,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_cman_update)(dev,
		node_id, cman, error);

	rte_tm_trace_node_cman_update(port_id, node_id, cman, ret);

	return ret;
}

/* Update node private WRED context */
int rte_tm_node_wred_context_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_wred_context_update)(dev,
		node_id, wred_profile_id, error);

	rte_tm_trace_node_wred_context_update(port_id, node_id, wred_profile_id,
					      ret);

	return ret;
}

/* Update node shared WRED context */
int rte_tm_node_shared_wred_context_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shared_wred_context_id,
	int add,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_shared_wred_context_update)(dev,
		node_id, shared_wred_context_id, add, error);

	rte_tm_trace_node_shared_wred_context_update(port_id, node_id,
						     shared_wred_context_id,
						     add, ret);

	return ret;
}

/* Read and/or clear stats counters for specific node */
int rte_tm_node_stats_read(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, node_stats_read)(dev,
		node_id, stats, stats_mask, clear, error);

	rte_tm_trace_node_stats_read(port_id, node_id, stats, *stats_mask,
				     clear, ret);

	return ret;
}

/* Packet marking - VLAN DEI */
int rte_tm_mark_vlan_dei(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, mark_vlan_dei)(dev,
		mark_green, mark_yellow, mark_red, error);

	rte_tm_trace_mark_vlan_dei(port_id, mark_green, mark_yellow, mark_red,
				   ret);

	return ret;
}

/* Packet marking - IPv4/IPv6 ECN */
int rte_tm_mark_ip_ecn(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, mark_ip_ecn)(dev,
		mark_green, mark_yellow, mark_red, error);

	rte_tm_trace_mark_ip_ecn(port_id, mark_green, mark_yellow, mark_red,
				 ret);

	return ret;
}

/* Packet marking - IPv4/IPv6 DSCP */
int rte_tm_mark_ip_dscp(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_TM_FUNC(port_id, mark_ip_dscp)(dev,
		mark_green, mark_yellow, mark_red, error);

	rte_tm_trace_mark_ip_dscp(port_id, mark_green, mark_yellow, mark_red,
				  ret);

	return ret;
}
