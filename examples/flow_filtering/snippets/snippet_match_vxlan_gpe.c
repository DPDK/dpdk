/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_match_vxlan_gpe.h"

void
snippet_init_vxlan_gpe(void)
{
	flow_attr.ingress = 1;
	flow_attr.group = 1;
}

void
snippet_match_vxlan_gpe_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	struct rte_flow_error error;
	create_jump_flow(port_id, 1, &error);

	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");
	queue->index = 1;

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_vxlan_gpe_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_vxlan_gpe *vxlan_gpe_mask = calloc(1,
		sizeof(struct rte_flow_item_vxlan_gpe));
	if (vxlan_gpe_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for vxlan_gpe_mask\n");
	memset(vxlan_gpe_mask->hdr.vni, 0xff, 3);

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE;
	pattern[3].spec = vxlan_gpe_mask;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_match_vxlan_gpe_create_pattern_template(uint16_t port_id,
	struct rte_flow_error *error)
{
	struct rte_flow_item_vxlan_gpe vxlan_gpe_mask = {
		.hdr.vni = {0xff, 0xff, 0xff},
	};
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
			.type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
			.mask = &vxlan_gpe_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	const struct rte_flow_pattern_template_attr pt_attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};

	return rte_flow_pattern_template_create(port_id, &pt_attr, pattern, error);
}

static struct rte_flow_actions_template *
snippet_match_vxlan_gpe_create_actions_template(uint16_t port_id,
	struct rte_flow_error *error)
{
	struct rte_flow_action_queue queue_v = {
		.index = 1
	};
	struct rte_flow_action_queue queue_m = {
		.index = UINT16_MAX
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &queue_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
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

struct rte_flow_template_table *
snippet_match_vxlan_gpe_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;
	const struct rte_flow_template_table_attr tbl_attr = {
		.flow_attr = {
			.group = 1,
			.priority = 0,
			.ingress = 1,
		},
		.nb_flows = 1000,
	};

	pt = snippet_match_vxlan_gpe_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_match_vxlan_gpe_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
