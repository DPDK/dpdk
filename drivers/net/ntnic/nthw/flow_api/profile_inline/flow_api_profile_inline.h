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

int flow_nic_set_hasher_fields_inline(struct flow_nic_dev *ndev,
	int hsh_idx,
	struct nt_eth_rss_conf rss_conf);

#endif	/* _FLOW_API_PROFILE_INLINE_H_ */
