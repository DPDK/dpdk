/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_modify_ecn.h"

void
snippet_init_modify_ecn(void)
{
	flow_attr.ingress = 1;
	flow_attr.group = 1;
	flow_attr.priority = 1;
}

void
snippet_match_modify_ecn_create_actions(__rte_unused uint16_t port_id,
				      struct rte_flow_action *action)
{
	/* Create one action that moves the packet to the selected queue. */
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	struct rte_flow_action_modify_field *modify_field =
					calloc(1, sizeof(struct rte_flow_action_modify_field));
	if (modify_field == NULL)
		fprintf(stderr, "Failed to allocate memory for modify_field\n");

	queue->index = 1;
	modify_field->operation = RTE_FLOW_MODIFY_SET;
	modify_field->dst.field = RTE_FLOW_FIELD_IPV4_ECN;
	modify_field->src.field = RTE_FLOW_FIELD_VALUE;
	modify_field->src.value[0] = 3;
	modify_field->width = 2;

	action[0].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	action[0].conf = modify_field;
	action[1].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[1].conf = queue;
	action[2].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_modify_ecn_create_patterns(struct rte_flow_item *pattern)
{
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
}

struct rte_flow_template_table *
snippet_match_modify_ecn_create_table(__rte_unused uint16_t port_id,
							__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
