/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_match_packet_type.h"

void
snippet_init_packet_type(void)
{
	init_default_snippet();
}

void
snippet_match_packet_type_create_actions(__rte_unused uint16_t port_id,
				       struct rte_flow_action *action)
{
	/* Create one action that moves the packet to the selected queue. */
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	/*
	 * create the action sequence.
	 * one action only, move packet to queue
	 */
	queue->index = 1;
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_packet_type_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_ptype *ptype_spec;
	ptype_spec = calloc(1, sizeof(struct rte_flow_item_ptype));
	if (ptype_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for ptype_spec\n");

	ptype_spec->packet_type = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP;

	struct rte_flow_item_ptype *ptype_mask;
	ptype_mask = calloc(1, sizeof(struct rte_flow_item_ptype));
	if (ptype_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for ptype_mask\n");

	ptype_mask->packet_type = RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK;

	/* match on RTE_PTYPE_L4_TCP / RTE_PTYPE_L4_MASK */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_PTYPE;
	pattern[0].spec = ptype_spec;
	pattern[0].mask = ptype_mask;

	/* the final level must be always type end */
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_match_packet_type_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_item titems[MAX_PATTERN_NUM] = {0};
	struct rte_flow_item_ptype ptype_mask = {0};

	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 1,
		.ingress = 1
	};

	titems[0].type = RTE_FLOW_ITEM_TYPE_PTYPE;
	ptype_mask.packet_type = RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK;
	titems[0].mask = &ptype_mask;

	titems[1].type = RTE_FLOW_ITEM_TYPE_END;

	return rte_flow_pattern_template_create(port_id, &attr, titems, error);
}

static struct rte_flow_actions_template *
snippet_match_packet_type_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_action tactions[MAX_ACTION_NUM] = {0};
	struct rte_flow_action masks[MAX_ACTION_NUM] = {0};
	struct rte_flow_actions_template_attr action_attr = {
		.ingress = 1,
	};

	tactions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	tactions[1].type = RTE_FLOW_ACTION_TYPE_END;

	memcpy(masks, tactions, sizeof(masks));
	return rte_flow_actions_template_create(port_id, &action_attr,
						tactions, masks, error);
}

struct rte_flow_template_table *
snippet_match_packet_type_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;

	struct rte_flow_template_table_attr table_attr = {
			.flow_attr = {
				.group = 0,
				.priority = 0,
				.ingress = 1,
		},
			.nb_flows = 1,
	};

	pt = snippet_match_packet_type_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_match_packet_type_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &table_attr, &pt, 1, &at, 1, error);
}
