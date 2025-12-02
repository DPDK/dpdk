/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_match_integrity_flags.h"

void
snippet_init_integrity_flags(void)
{
	init_default_snippet();
}

void
snippet_match_integrity_flags_create_actions(__rte_unused uint16_t port_id,
			struct rte_flow_action *action)
{
	/* Create one action that moves the packet to the selected queue. */
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	queue->index = 1;
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_integrity_flags_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_ipv4 *ip_spec;
	struct rte_flow_item_ipv4 *ip_mask;
	struct rte_flow_item_integrity *integrity_spec;
	struct rte_flow_item_integrity *integrity_mask;

	integrity_spec = calloc(1, sizeof(struct rte_flow_item_integrity));
	if (integrity_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for integrity_spec\n");

	integrity_spec->level = 0;
	integrity_spec->l3_ok = 1;
	integrity_spec->ipv4_csum_ok = 1;

	integrity_mask = calloc(1, sizeof(struct rte_flow_item_integrity));
	if (integrity_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for integrity_mask\n");

	integrity_mask->level = 0;
	integrity_mask->l3_ok = 1;
	integrity_mask->l4_ok = 0;
	integrity_mask->ipv4_csum_ok = 1;

	ip_spec = calloc(1, sizeof(struct rte_flow_item_ipv4));
	if (ip_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for ip_spec\n");

	ip_mask = calloc(1, sizeof(struct rte_flow_item_ipv4));
	if (ip_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for ip_mask\n");

	ip_spec->hdr.dst_addr = htonl(((192<<24) + (168<<16) + (1<<8) + 1));
	ip_mask->hdr.dst_addr = 0xffffffff;
	ip_spec->hdr.src_addr = htonl(((0<<24) + (0<<16) + (0<<8) + 0));
	ip_mask->hdr.src_addr = 0x0;

	/*
	 * set the first level of the pattern (ETH).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

	/*
	 * setting the second level of the pattern (IP).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = ip_spec;
	pattern[1].mask = ip_mask;

	/* check for a valid ipv4 packet. */
	pattern[2].type = RTE_FLOW_ITEM_TYPE_INTEGRITY;
	pattern[2].spec = integrity_spec;
	pattern[2].mask = integrity_mask;

	/* the final level must be always type end */
	pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
}

struct rte_flow_template_table *
snippet_match_integrity_flags_create_table(
	__rte_unused uint16_t port_id,
	__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
