/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdbool.h>
#include <stdlib.h>

#include <rte_flow.h>

#include "../common.h"
#include "snippet_re_route_to_kernel.h"

void
snippet_init_re_route_to_kernel(void)
{
	enable_flow_isolation = true; /* enable flow isolation mode */
	enable_promiscuous_mode = false; /* default disable promiscuous mode */
	flow_attr.group = 1;
	flow_attr.ingress = 1;
}

void
snippet_re_route_to_kernel_create_actions(__rte_unused uint16_t port_id,
					struct rte_flow_action *action)
{
	action[0].type = RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_re_route_to_kernel_create_patterns(struct rte_flow_item *pattern)
{
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	/* Fill-in any matching criteria */
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_re_route_to_kernel_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{

	const struct rte_flow_pattern_template_attr attrp = {
		.ingress = 1
	};

	struct rte_flow_item pattern[] = {
		[0] = {.type = RTE_FLOW_ITEM_TYPE_ETH},
		[1] = {.type = RTE_FLOW_ITEM_TYPE_END},
	};

	return rte_flow_pattern_template_create(port_id, &attrp, pattern, error);
}

static struct rte_flow_actions_template *
snippet_re_route_to_kernel_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_action actions[] = {
		[0] = {.type = RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL},
		[1] = {.type = RTE_FLOW_ACTION_TYPE_END},
	};
	struct rte_flow_action masks[] = {
		[0] = {.type = RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL},
		[1] = {.type = RTE_FLOW_ACTION_TYPE_END,},
	};

	const struct rte_flow_actions_template_attr at_attr = {
		.ingress = 1,
	};

	return rte_flow_actions_template_create(port_id, &at_attr, actions, masks, error);
}

struct rte_flow_template_table *
snippet_re_route_to_kernel_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;

	/* Define the template table attributes. */
	const struct rte_flow_template_table_attr table_attr = {
		.flow_attr = {
			.ingress = 1,
			.group = 1,
		},
		.nb_flows = 10000,
	};

	pt = snippet_re_route_to_kernel_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_re_route_to_kernel_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}
	return rte_flow_template_table_create(port_id, &table_attr, &pt, 1, &at, 1, error);
}
