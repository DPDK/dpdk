/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_match_roce_ib_bth.h"

static void
snippet_init_roce_ib_bth(void)
{
	flow_attr.ingress = 1;
	flow_attr.group = 1;
	flow_attr.priority = 1;
}

static void
snippet_match_roce_ib_bth_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	/* jump to group 1 */
	struct rte_flow_error error;
	create_jump_flow(port_id, 1, &error);

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
snippet_match_roce_ib_bth_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_ib_bth *bth;

	bth = calloc(1, sizeof(struct rte_flow_item_ib_bth));
	if (bth == NULL)
		fprintf(stderr, "Failed to allocate memory for bth\n");

	bth->hdr.opcode = 0x81;
	bth->hdr.dst_qp[0] = 0x0;
	bth->hdr.dst_qp[1] = 0xab;
	bth->hdr.dst_qp[2] = 0xd4;

	/* Set the patterns. */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_IB_BTH;
	pattern[3].spec = bth;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_template_table *
snippet_match_roce_ib_bth_create_table(__rte_unused uint16_t port_id,
							__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
