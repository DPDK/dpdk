/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <stdint.h>

#include <rte_errno.h>
#include "rte_ethdev.h"
#include "rte_tm_driver.h"
#include "rte_tm.h"

/* Get generic traffic manager operations structure from a port. */
const struct rte_tm_ops *
rte_tm_ops_get(uint8_t port_id, struct rte_tm_error *error)
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
rte_tm_get_number_of_leaf_nodes(uint8_t port_id,
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
	return 0;
}

/* Check node type (leaf or non-leaf) */
int
rte_tm_node_type_get(uint8_t port_id,
	uint32_t node_id,
	int *is_leaf,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_type_get)(dev,
		node_id, is_leaf, error);
}

/* Get capabilities */
int rte_tm_capabilities_get(uint8_t port_id,
	struct rte_tm_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, capabilities_get)(dev,
		cap, error);
}

/* Get level capabilities */
int rte_tm_level_capabilities_get(uint8_t port_id,
	uint32_t level_id,
	struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, level_capabilities_get)(dev,
		level_id, cap, error);
}

/* Get node capabilities */
int rte_tm_node_capabilities_get(uint8_t port_id,
	uint32_t node_id,
	struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_capabilities_get)(dev,
		node_id, cap, error);
}

/* Add WRED profile */
int rte_tm_wred_profile_add(uint8_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, wred_profile_add)(dev,
		wred_profile_id, profile, error);
}

/* Delete WRED profile */
int rte_tm_wred_profile_delete(uint8_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, wred_profile_delete)(dev,
		wred_profile_id, error);
}

/* Add/update shared WRED context */
int rte_tm_shared_wred_context_add_update(uint8_t port_id,
	uint32_t shared_wred_context_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shared_wred_context_add_update)(dev,
		shared_wred_context_id, wred_profile_id, error);
}

/* Delete shared WRED context */
int rte_tm_shared_wred_context_delete(uint8_t port_id,
	uint32_t shared_wred_context_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shared_wred_context_delete)(dev,
		shared_wred_context_id, error);
}

/* Add shaper profile */
int rte_tm_shaper_profile_add(uint8_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shaper_profile_add)(dev,
		shaper_profile_id, profile, error);
}

/* Delete WRED profile */
int rte_tm_shaper_profile_delete(uint8_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shaper_profile_delete)(dev,
		shaper_profile_id, error);
}

/* Add shared shaper */
int rte_tm_shared_shaper_add_update(uint8_t port_id,
	uint32_t shared_shaper_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shared_shaper_add_update)(dev,
		shared_shaper_id, shaper_profile_id, error);
}

/* Delete shared shaper */
int rte_tm_shared_shaper_delete(uint8_t port_id,
	uint32_t shared_shaper_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, shared_shaper_delete)(dev,
		shared_shaper_id, error);
}

/* Add node to port traffic manager hierarchy */
int rte_tm_node_add(uint8_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_add)(dev,
		node_id, parent_node_id, priority, weight, level_id,
		params, error);
}

/* Delete node from traffic manager hierarchy */
int rte_tm_node_delete(uint8_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_delete)(dev,
		node_id, error);
}

/* Suspend node */
int rte_tm_node_suspend(uint8_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_suspend)(dev,
		node_id, error);
}

/* Resume node */
int rte_tm_node_resume(uint8_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_resume)(dev,
		node_id, error);
}

/* Commit the initial port traffic manager hierarchy */
int rte_tm_hierarchy_commit(uint8_t port_id,
	int clear_on_fail,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, hierarchy_commit)(dev,
		clear_on_fail, error);
}

/* Update node parent  */
int rte_tm_node_parent_update(uint8_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_parent_update)(dev,
		node_id, parent_node_id, priority, weight, error);
}

/* Update node private shaper */
int rte_tm_node_shaper_update(uint8_t port_id,
	uint32_t node_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_shaper_update)(dev,
		node_id, shaper_profile_id, error);
}

/* Update node shared shapers */
int rte_tm_node_shared_shaper_update(uint8_t port_id,
	uint32_t node_id,
	uint32_t shared_shaper_id,
	int add,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_shared_shaper_update)(dev,
		node_id, shared_shaper_id, add, error);
}

/* Update node stats */
int rte_tm_node_stats_update(uint8_t port_id,
	uint32_t node_id,
	uint64_t stats_mask,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_stats_update)(dev,
		node_id, stats_mask, error);
}

/* Update WFQ weight mode */
int rte_tm_node_wfq_weight_mode_update(uint8_t port_id,
	uint32_t node_id,
	int *wfq_weight_mode,
	uint32_t n_sp_priorities,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_wfq_weight_mode_update)(dev,
		node_id, wfq_weight_mode, n_sp_priorities, error);
}

/* Update node congestion management mode */
int rte_tm_node_cman_update(uint8_t port_id,
	uint32_t node_id,
	enum rte_tm_cman_mode cman,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_cman_update)(dev,
		node_id, cman, error);
}

/* Update node private WRED context */
int rte_tm_node_wred_context_update(uint8_t port_id,
	uint32_t node_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_wred_context_update)(dev,
		node_id, wred_profile_id, error);
}

/* Update node shared WRED context */
int rte_tm_node_shared_wred_context_update(uint8_t port_id,
	uint32_t node_id,
	uint32_t shared_wred_context_id,
	int add,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_shared_wred_context_update)(dev,
		node_id, shared_wred_context_id, add, error);
}

/* Read and/or clear stats counters for specific node */
int rte_tm_node_stats_read(uint8_t port_id,
	uint32_t node_id,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, node_stats_read)(dev,
		node_id, stats, stats_mask, clear, error);
}

/* Packet marking - VLAN DEI */
int rte_tm_mark_vlan_dei(uint8_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, mark_vlan_dei)(dev,
		mark_green, mark_yellow, mark_red, error);
}

/* Packet marking - IPv4/IPv6 ECN */
int rte_tm_mark_ip_ecn(uint8_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, mark_ip_ecn)(dev,
		mark_green, mark_yellow, mark_red, error);
}

/* Packet marking - IPv4/IPv6 DSCP */
int rte_tm_mark_ip_dscp(uint8_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_TM_FUNC(port_id, mark_ip_dscp)(dev,
		mark_green, mark_yellow, mark_red, error);
}
