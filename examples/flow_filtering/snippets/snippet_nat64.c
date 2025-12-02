/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_nat64.h"

void
snippet_init_nat64(void)
{
	flow_attr.egress = 1;
	flow_attr.group = 1;
}

void
snippet_match_nat64_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	struct rte_flow_error error;
	create_jump_flow(port_id, 1, &error);

	struct rte_flow_action_nat64 *nat64_v = calloc(1, sizeof(struct rte_flow_action_nat64));
	struct rte_flow_action_jump *jump_v = calloc(1, sizeof(struct rte_flow_action_jump));

	nat64_v->type = RTE_FLOW_NAT64_4TO6;
	jump_v->group = 2;

	action[0].type = RTE_FLOW_ACTION_TYPE_NAT64;
	action[0].conf = nat64_v;
	action[1].type = RTE_FLOW_ACTION_TYPE_JUMP;
	action[1].conf = jump_v;
	action[2].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_nat64_create_patterns(struct rte_flow_item *pattern)
{
pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
pattern[1].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_match_nat64_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{
const struct rte_flow_pattern_template_attr pt_attr = { .egress = 1 };
const struct rte_flow_item pattern[2] = {
	[0] = {
		.type = RTE_FLOW_ITEM_TYPE_ETH,
	},
	[1] = {
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
};

return rte_flow_pattern_template_create(port_id, &pt_attr, pattern, error);
}

static struct rte_flow_actions_template *
snippet_match_nat64_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
{
	const struct rte_flow_action_nat64 nat64_v = { .type = RTE_FLOW_NAT64_4TO6 };
	const struct rte_flow_action_nat64 nat64_m = { .type = UINT8_MAX };
	const struct rte_flow_action_jump jump_v = { .group = 2 };
	const struct rte_flow_action_jump jump_m = { .group = UINT32_MAX };

	const struct rte_flow_action actions[3] = {
		[0] = {
			.type = RTE_FLOW_ACTION_TYPE_NAT64,
			.conf = &nat64_v,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_v,
		},
		[2] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	const struct rte_flow_action masks[3] = {
		[0] = {
			.type = RTE_FLOW_ACTION_TYPE_NAT64,
			.conf = &nat64_m,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_m,
		},
		[2] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	const struct rte_flow_actions_template_attr at_attr = { .egress = 1 };
	return rte_flow_actions_template_create(port_id, &at_attr, actions, masks, error);
}

struct rte_flow_template_table *
snippet_match_nat64_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;
	const struct rte_flow_template_table_attr tbl_attr = {
		.flow_attr = {
			.group = 1,
			.priority = 0,
			.egress = 1,
		},
		.nb_flows = 8,
	};

	pt = snippet_match_nat64_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_match_nat64_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
