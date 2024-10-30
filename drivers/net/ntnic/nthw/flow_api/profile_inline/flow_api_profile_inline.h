/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_PROFILE_INLINE_H_
#define _FLOW_API_PROFILE_INLINE_H_

#include <stdint.h>

#include "flow_api.h"
#include "stream_binary_flow_api.h"

/*
 * Management
 */

int done_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev);

int initialize_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev);

/*
 * Flow functionality
 */
int flow_destroy_locked_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *fh,
	struct rte_flow_error *error);

struct flow_handle *flow_create_profile_inline(struct flow_eth_dev *dev,
	const struct rte_flow_attr *attr,
	uint16_t forced_vlan_vid,
	uint16_t caller_id,
	const struct rte_flow_item elem[],
	const struct rte_flow_action action[],
	struct rte_flow_error *error);

int flow_destroy_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	struct rte_flow_error *error);

int flow_flush_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	struct rte_flow_error *error);

int flow_actions_update_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	const struct rte_flow_action action[],
	struct rte_flow_error *error);

int flow_dev_dump_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *flow,
	uint16_t caller_id,
	FILE *file,
	struct rte_flow_error *error);

int flow_get_aged_flows_profile_inline(struct flow_eth_dev *dev,
	uint16_t caller_id,
	void **context,
	uint32_t nb_contexts,
	struct rte_flow_error *error);

int flow_nic_set_hasher_fields_inline(struct flow_nic_dev *ndev,
	int hsh_idx,
	struct nt_eth_rss_conf rss_conf);

/*
 * Stats
 */

int flow_get_flm_stats_profile_inline(struct flow_nic_dev *ndev, uint64_t *data, uint64_t size);

int flow_info_get_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	struct rte_flow_port_info *port_info,
	struct rte_flow_queue_info *queue_info, struct rte_flow_error *error);

int flow_configure_profile_inline(struct flow_eth_dev *dev, uint8_t caller_id,
	const struct rte_flow_port_attr *port_attr, uint16_t nb_queue,
	const struct rte_flow_queue_attr *queue_attr[],
	struct rte_flow_error *error);

#endif	/* _FLOW_API_PROFILE_INLINE_H_ */
