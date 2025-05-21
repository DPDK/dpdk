/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include <rte_common.h>

#include "../common.h"
#include "snippet_match_gre.h"

void
snippet_init_gre(void)
{
	init_default_snippet();
}

static void
snippet_match_gre_create_actions(struct rte_flow_action *action)
{
	/* Create one action that moves the packet to the selected queue. */
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	/* Set the selected queue. */
	queue->index = 1;

	/* Set the action move packet to the selected queue. */
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static void
snippet_match_gre_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_gre *gre_spec;
	struct rte_flow_item_gre_opt *gre_opt_spec;

	gre_spec = calloc(1, sizeof(struct rte_flow_item_gre));
	if (gre_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for gre_spec\n");

	gre_opt_spec = calloc(1, sizeof(struct rte_flow_item_gre_opt));
	if (gre_opt_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for gre_opt_spec\n");

	/* Set the Checksum GRE option. */
	gre_spec->c_rsvd0_ver = RTE_BE16(0x8000);
	gre_opt_spec->checksum_rsvd.checksum = RTE_BE16(0x11);

	/* Set the patterns. */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_GRE;
	pattern[2].spec = gre_spec;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_GRE_OPTION;
	pattern[3].spec = gre_opt_spec;
	pattern[3].mask = gre_opt_spec;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_template_table *
snippet_gre_flow_create_table(
	__rte_unused uint16_t port_id,
	__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
