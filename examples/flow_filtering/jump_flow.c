/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "jump_flow.h"

struct rte_flow *
create_jump_flow(uint16_t port_id, uint16_t group_id, struct rte_flow_error *error)
{
	struct rte_flow_action actions[2] = {0};
	struct rte_flow_item patterns[2] = {0};
	struct rte_flow *flow = NULL;

	struct rte_flow_attr flow_attr = {
		.ingress = 1,
		.group = 0,
	};

	struct rte_flow_action_jump jump = {
		.group = group_id,
	};

	/* Set up jump action to target group */
	actions[0].type = RTE_FLOW_ACTION_TYPE_JUMP;
	actions[0].conf = &jump;
	actions[1].type = RTE_FLOW_ACTION_TYPE_END;

	/* match on ethernet */
	patterns[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	patterns[1].type = RTE_FLOW_ITEM_TYPE_END;

	/* Validate the rule and create it. */
	if (rte_flow_validate(port_id, &flow_attr, patterns, actions, error) == 0)
		flow = rte_flow_create(port_id, &flow_attr, patterns, actions, error);
	return flow;
}
