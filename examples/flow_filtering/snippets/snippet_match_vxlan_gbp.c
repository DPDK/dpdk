/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_match_vxlan_gbp.h"

void
snippet_init_vxlan_gbp(void)
{
	flow_attr.ingress = 1;
	flow_attr.group = 0;
}

void
snippet_match_vxlan_gbp_create_actions(__rte_unused uint16_t port_id,
					struct rte_flow_action *action)
{
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");

	queue->index = 1;

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_vxlan_gbp_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_vxlan *vxlan_gbp = calloc(1, sizeof(struct rte_flow_item_vxlan));
	if (vxlan_gbp == NULL)
		fprintf(stderr, "Failed to allocate memory for vxlan_gbp\n");

	struct rte_flow_item_vxlan *vxlan_gbp_mask = calloc(1, sizeof(struct rte_flow_item_vxlan));
	if (vxlan_gbp_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for vxlan_gbp_mask\n");

	uint8_t vni[] = {0x00, 0x00, 0x00};
	uint16_t group_policy_id = 0x200;

	memcpy(vxlan_gbp_mask->hdr.vni, "\xff\xff\xff", 3);
	vxlan_gbp_mask->hdr.flags = 0xff;
	vxlan_gbp_mask->hdr.policy_id = 0xffff;

	vxlan_gbp->flags = 0x88;
	memcpy(vxlan_gbp->vni, vni, 3);
	vxlan_gbp->hdr.rsvd0[1] = (group_policy_id >> 8) & 0xff;
	vxlan_gbp->hdr.rsvd0[2] = group_policy_id & 0xff;
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_VXLAN;
	pattern[3].spec = vxlan_gbp;
	pattern[3].mask = vxlan_gbp_mask;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_END;

}

static struct rte_flow_pattern_template *
snippet_match_vxlan_gbp_create_pattern_template(uint16_t port_id,
	struct rte_flow_error *error)
{
	struct rte_flow_item pattern[MAX_PATTERN_NUM] = {0};

	struct rte_flow_item_vxlan vxlan_gbp_mask = {
		.hdr.vni = {0xff, 0xff, 0xff},
		.hdr.flags = 0xff,
		.hdr.policy_id = 0xffff,
	};

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_VXLAN;
	pattern[3].mask = &vxlan_gbp_mask;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_END;

	const struct rte_flow_pattern_template_attr pt_attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};

	return rte_flow_pattern_template_create(port_id, &pt_attr, pattern, error);
}

static struct rte_flow_actions_template *
snippet_match_vxlan_gbp_create_actions_template(uint16_t port_id,
	struct rte_flow_error *error)
{
	struct rte_flow_action action[MAX_ACTION_NUM] = {0};
	struct rte_flow_action masks[MAX_ACTION_NUM] = {0};

	struct rte_flow_action_queue queue_v = {
		.index = 1,
	};
	struct rte_flow_action_queue queue_m = {
		.index = UINT16_MAX,
	};

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &queue_v;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

	masks[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	masks[0].conf = &queue_m;
	masks[1].type = RTE_FLOW_ACTION_TYPE_END;

	const struct rte_flow_actions_template_attr at_attr = {
		.ingress = 1,
	};

	return rte_flow_actions_template_create(port_id, &at_attr, action, masks, error);
}

struct rte_flow_template_table *
snippet_match_vxlan_gbp_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;
	const struct rte_flow_template_table_attr tbl_attr = {
		.flow_attr = {
			.group = 0,
			.priority = 0,
			.ingress = 1,
		},
		.nb_flows = 1000,
	};

	pt = snippet_match_vxlan_gbp_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_match_vxlan_gbp_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
