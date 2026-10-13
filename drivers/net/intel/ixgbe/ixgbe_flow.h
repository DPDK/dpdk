/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _IXGBE_FLOW_H_
#define _IXGBE_FLOW_H_

#include "../common/flow_check.h"
#include "../common/flow_engine.h"

int
ixgbe_flow_actions_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error);

extern const struct ci_flow_engine_list ixgbe_flow_engine_list;

extern const struct ci_flow_engine ixgbe_ethertype_flow_engine;
extern const struct ci_flow_engine ixgbe_syn_flow_engine;

#endif /*  _IXGBE_FLOW_H_ */
