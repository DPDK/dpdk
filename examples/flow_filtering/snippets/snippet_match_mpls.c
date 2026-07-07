/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>

#include <rte_errno.h>
#include <rte_flow.h>
#include <rte_common.h>

#include "../common.h"
#include "snippet_match_mpls.h"


static void
snippet_init_mpls(void)
{
	init_default_snippet();
}

static void
snippet_mpls_create_actions(__rte_unused uint16_t port_id, struct rte_flow_action *actions)
{
	/* Create one action that moves the packet to the selected queue. */
	struct rte_flow_action_queue *queue;

	queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	/* Set the selected queue. */
	queue->index = UINT16_MAX;

	/* Set the action move packet to the selected queue. */
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = queue;
	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static void
snippet_mpls_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_mpls *mpls_item;

	mpls_item = calloc(1, sizeof(struct rte_flow_item_ipv4));
	if (mpls_item == NULL)
		fprintf(stderr, "Failed to allocate memory for mpls_item\n");

	memcpy(mpls_item->label_tc_s, "\xab\xcd\xe1", sizeof(mpls_item->label_tc_s));

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_MPLS;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_MPLS;
	pattern[5].type = RTE_FLOW_ITEM_TYPE_MPLS;
	pattern[5].spec = mpls_item;
}

static struct rte_flow_pattern_template *
snippet_mpls_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{
	/* Initialize the MPLS flow item with specific values. */
	struct rte_flow_item_mpls mpls_item = {
		/* The MPLS label set to the maximum value (0xfffff),
		 * the Traffic Class set to 0,
		 * and the Bottom of Stack bit set to 1.
		 */
		.label_tc_s = { 0xff, 0xff, 0xf1 },
	};

	/* Define the flow pattern template. */
	struct rte_flow_item pattern[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_UDP,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_MPLS,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_MPLS,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_MPLS,
			.mask = &mpls_item,
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
snippet_mpls_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
{
	/* Define the queue action value. */
	struct rte_flow_action_queue queue_v = {
		.index = 0	/* The queue index is set to 0. */
	};

	/* Define the queue action mask. */
	struct rte_flow_action_queue queue_m = {
		/* The queue index mask is set to the maximum value (0xFFFF, the mask) */
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
snippet_mpls_create_table(uint16_t port_id, struct rte_flow_error *error)
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
		.nb_flows = 1000,
	};

	/* Create the pattern template. */
	pt = snippet_mpls_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}

	/* Create the actions template. */
	at = snippet_mpls_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
				error->message, rte_strerror(rte_errno));
		return NULL;
	}

	/* Create the template table. */
	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
