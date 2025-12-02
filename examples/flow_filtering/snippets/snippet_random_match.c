/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_random_match.h"


static void
snippet_init_random_match(void)
{
	init_default_snippet();
}

static void
snippet_match_random_value_create_actions(__rte_unused uint16_t port_id,
					struct rte_flow_action *action)
{
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");
	queue->index = UINT16_MAX; /* The selected target queue.*/
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static void
snippet_match_random_value_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_random *random_item;
	random_item = calloc(1, sizeof(struct rte_flow_item_random));
	if (random_item == NULL)
		fprintf(stderr, "Failed to allocate memory for port_representor_spec\n");

	random_item->value = 0;

	/* Set the patterns. */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_RANDOM;
	pattern[0].spec = random_item;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_match_random_value_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{

	struct rte_flow_item_random random_item = {
		.value = 0x3,
	};

	/* Define the flow pattern template. */
	struct rte_flow_item pattern[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_RANDOM,
			.mask = &random_item,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};

	/* Set the pattern template attributes */
	const struct rte_flow_pattern_template_attr pt_attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};

	return rte_flow_pattern_template_create(port_id, &pt_attr, pattern, error);
}

static struct rte_flow_actions_template *
snippet_match_random_value_create_actions_template(__rte_unused uint16_t port_id,
						 struct rte_flow_error *error)
{
	struct rte_flow_action_queue queue_v = {
		.index = 0
	};

	struct rte_flow_action_queue queue_m = {
		.index = UINT16_MAX
	};

	/* Define the actions template. */
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &queue_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	/* Define the actions template masks. */
	struct rte_flow_action masks[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &queue_m,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	const struct rte_flow_actions_template_attr at_attr = {
		.ingress = 1,
	};

	return rte_flow_actions_template_create(port_id, &at_attr, actions, masks, error);
}

static struct rte_flow_template_table *
snippet_match_random_value_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;

	/* Define the template table attributes. */
	const struct rte_flow_template_table_attr tbl_attr = {
		.flow_attr = {
			.group = 1,
			.priority = 0,
			.ingress = 1,
		},

		/* set the maximum number of flow rules that this table holds. */
		.nb_flows = 1,
	};

	/* Create the pattern template. */
	pt = snippet_match_random_value_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}

	/* Create the actions template. */
	at = snippet_match_random_value_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}

	/* Create the template table. */
	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
