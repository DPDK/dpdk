/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_switch_granularity.h"

static void
snippet_init_switch_granularity(void)
{
	flow_attr.ingress = 0;
	flow_attr.transfer = 1;
	flow_attr.group = 1;
	flow_attr.priority = 1;
}

static void
snippet_match_switch_granularity_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	/* jump to group 1 */
	struct rte_flow_error error;
	create_jump_flow(port_id, 1, &error);

	struct rte_flow_action_ethdev *represented_port = calloc(1,
		sizeof(struct rte_flow_action_ethdev));
	if (represented_port == NULL)
		fprintf(stderr, "Failed to allocate memory for represented_port\n");

	represented_port->port_id = 0;
	action[0].type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT;
	action[0].conf = represented_port;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static void
snippet_match_switch_granularity_create_patterns(struct rte_flow_item *pattern)
{
	/* Set the patterns. */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT;
	pattern[1].spec = NULL;
	pattern[1].mask = NULL;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_template_table *
create_table_switch_granularity(__rte_unused uint16_t port_id,
	__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
