/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_SWITCH_FILTER_H_
#define _ICE_SWITCH_FILTER_H_

#include "base/ice_switch.h"
#include "base/ice_type.h"
#include "ice_ethdev.h"

int
ice_create_switch_filter(struct ice_pf *pf,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow *flow,
			struct rte_flow_error *error);
int
ice_destroy_switch_filter(struct ice_pf *pf,
			struct rte_flow *flow,
			struct rte_flow_error *error);
void
ice_free_switch_filter_rule(void *rule);
#endif /* _ICE_SWITCH_FILTER_H_ */
