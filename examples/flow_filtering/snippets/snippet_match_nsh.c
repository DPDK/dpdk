/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_match_nsh.h"

static void
snippet_init_nsh(void)
{
	flow_attr.transfer = 1;
	flow_attr.group = 1;
	flow_attr.priority = 0;
}

static void
snippet_match_nsh_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	/* jump to group 1 */
	struct rte_flow_error error;
	create_jump_flow(port_id, 1, &error);

	struct rte_flow_action_port_id *portid = calloc(1, sizeof(struct rte_flow_action_port_id));
	if (portid == NULL)
		fprintf(stderr, "Failed to allocate memory for port_id\n");

	/* To match on NSH to port_id 1. */
	portid->id = 1;

	action[0].type = RTE_FLOW_ACTION_TYPE_PORT_ID;
	action[0].conf = portid;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static void
snippet_match_nsh_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_udp *spec;
	struct rte_flow_item_udp *mask;

	spec = calloc(1, sizeof(struct rte_flow_item_udp));
	if (spec == NULL)
		fprintf(stderr, "Failed to allocate memory for spec\n");

	mask = calloc(1, sizeof(struct rte_flow_item_udp));
	if (mask == NULL)
		fprintf(stderr, "Failed to allocate memory for mask\n");

	/* Set the patterns. */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV6;

	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	spec->hdr.dst_port = RTE_BE16(250);
	mask->hdr.dst_port = RTE_BE16(0xffff);
	pattern[2].spec = spec;
	pattern[2].mask = mask;

	pattern[3].type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_NSH;
	pattern[5].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[6].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_template_table *
snippet_nsh_flow_create_table(__rte_unused uint16_t port_id,
	__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
