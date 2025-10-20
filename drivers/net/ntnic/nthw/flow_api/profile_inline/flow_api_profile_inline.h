/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_PROFILE_INLINE_H_
#define _FLOW_API_PROFILE_INLINE_H_

#include <stdint.h>

#include "flow_api.h"
#include "stream_binary_flow_api.h"

#define ENABLE_FRAGMENTATION 1
#define DISABLE_FRAGMENTATION 0
#define IPV4_DF_DROP 1
#define IPV4_DF_FORWARD 0
#define IPV6_DROP 1
#define IPV6_FRAGMENT 0

/*
 * Management
 */

int nthw_done_flow_mgmnt_of_ndev_profile_inline(struct flow_nic_dev *ndev);

int nthw_init_flow_mgmnt_of_ndev_profile_inline(struct flow_nic_dev *ndev);

/*
 * Flow functionality
 */
int nthw_flow_destroy_locked_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *fh,
	struct rte_flow_error *error);

struct flow_handle *nthw_flow_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_attr *attr,
	uint16_t forced_vlan_vid,
	uint16_t caller_id,
	const struct rte_flow_item elem[],
	const struct rte_flow_action action[],
	struct rte_flow_error *error);

int nthw_flow_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	struct rte_flow_error *error);

int nthw_flow_flush_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	struct rte_flow_error *error);

int nthw_flow_actions_update_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	const struct rte_flow_action action[],
	struct rte_flow_error *error);

int nthw_flow_dev_dump_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	uint16_t caller_id,
	FILE *file,
	struct rte_flow_error *error);

int nthw_flow_get_aged_flows_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	void **context,
	uint32_t nb_contexts,
	struct rte_flow_error *error);

/*
 * Stats
 */

int nthw_flow_get_flm_stats_profile_inline(struct flow_nic_dev *ndev,
		uint64_t *data,
		uint64_t size);

int nthw_flow_get_ifr_stats_profile_inline(struct flow_nic_dev *ndev,
	uint64_t *data,
	uint8_t port_count);

/*
 * RTE flow asynchronous operations functions
 */

struct flow_pattern_template *
nthw_flow_pattern_template_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_pattern_template_attr *template_attr, uint16_t caller_id,
	const struct rte_flow_item pattern[], struct rte_flow_error *error);

int nthw_flow_pattern_template_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_pattern_template *pattern_template,
	struct rte_flow_error *error);

struct flow_actions_template *
nthw_flow_actions_template_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_actions_template_attr *template_attr, uint16_t caller_id,
	const struct rte_flow_action actions[], const struct rte_flow_action masks[],
	struct rte_flow_error *error);

int nthw_flow_actions_template_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_actions_template *actions_template,
	struct rte_flow_error *error);

struct flow_template_table *nthw_flow_template_table_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_template_table_attr *table_attr, uint16_t forced_vlan_vid,
	uint16_t caller_id,
	struct flow_pattern_template *pattern_templates[], uint8_t nb_pattern_templates,
	struct flow_actions_template *actions_templates[], uint8_t nb_actions_templates,
	struct rte_flow_error *error);

int nthw_flow_template_table_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_template_table *template_table,
	struct rte_flow_error *error);

struct flow_handle *nthw_flow_async_create_profile_inline(struct flow_eth_dev *dev,
	uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr,
	struct flow_template_table *template_table, const struct rte_flow_item pattern[],
	uint8_t pattern_template_index, const struct rte_flow_action actions[],
	uint8_t actions_template_index, void *user_data, struct rte_flow_error *error);

int nthw_flow_async_destroy_profile_inline(struct flow_eth_dev *dev, uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr, struct flow_handle *flow,
	void *user_data, struct rte_flow_error *error);

int nthw_flow_info_get_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	struct rte_flow_port_info *port_info,
	struct rte_flow_queue_info *queue_info, struct rte_flow_error *error);

int nthw_flow_configure_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	const struct rte_flow_port_attr *port_attr, uint16_t nb_queue,
	const struct rte_flow_queue_attr *queue_attr[],
	struct rte_flow_error *error);

/*
 * Config API
 */
int nthw_flow_set_mtu_inline(struct flow_eth_dev *dev, uint32_t port, uint16_t mtu);

#endif	/* _FLOW_API_PROFILE_INLINE_H_ */
