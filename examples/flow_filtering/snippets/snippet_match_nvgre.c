/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "../jump_flow.h"
#include "snippet_match_nvgre.h"

void
snippet_init_nvgre(void)
{
	flow_attr.ingress = 1;
	flow_attr.group = 1;
}

void
snippet_match_nvgre_create_actions(uint16_t port_id, struct rte_flow_action *action)
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
snippet_match_nvgre_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_nvgre *nvgre = calloc(1, sizeof(struct rte_flow_item_nvgre));
	if (nvgre == NULL)
		fprintf(stderr, "Failed to allocate memory for nvgre\n");

	struct rte_flow_item_udp *udp = calloc(1, sizeof(struct rte_flow_item_udp));
	if (udp == NULL)
		fprintf(stderr, "Failed to allocate memory for udp\n");

	struct rte_flow_item_nvgre *nvgre_mask = calloc(1, sizeof(struct rte_flow_item_nvgre));
	if (nvgre_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for nvgre_mask\n");

	struct rte_flow_item_udp *udp_mask = calloc(1, sizeof(struct rte_flow_item_udp));
	if (udp_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for udp_mask\n");

	/* build rule to match specific NVGRE:
	 * tni = 0x12346, flow_id = 0x78, inner_udp_src = 0x1234
	 */
	nvgre->tni[0] = 0x12;
	nvgre->tni[1] = 0x34;
	nvgre->tni[2] = 0x56;
	nvgre->flow_id = 0x78;
	udp->hdr.src_port = RTE_BE16(0x1234);

	/* define nvgre and udp match mask for all bits */
	nvgre_mask->tni[0] = 0xff;
	nvgre_mask->tni[1] = 0xff;
	nvgre_mask->tni[2] = 0xff;
	nvgre_mask->flow_id = 0xff;
	udp_mask->hdr.src_port = 0xffff;

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_NVGRE;
	pattern[2].spec = nvgre;
	pattern[2].mask = nvgre_mask;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[5].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[5].spec = udp;
	pattern[5].mask = udp_mask;
	pattern[6].type = RTE_FLOW_ITEM_TYPE_END;
}

static struct rte_flow_pattern_template *
snippet_match_nvgre_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{
	/* define nvgre and udp match mask for all bits */
	struct rte_flow_item_nvgre nvgre_mask = {
		.tni = {0xff, 0xff, 0xff},
		.flow_id = 0xff,
	};
	struct rte_flow_item_udp udp_mask = {
		.hdr.src_port = RTE_BE16(0xffff),
	};

	struct rte_flow_item pattern[MAX_PATTERN_NUM] = {{0}};

	/* build the match pattern, match tni + flow_id + inner_udp_src */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_NVGRE;
	pattern[2].mask = &nvgre_mask;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[4].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[5].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[5].mask = &udp_mask;
	pattern[6].type = RTE_FLOW_ITEM_TYPE_END;

	const struct rte_flow_pattern_template_attr pt_attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};

	return rte_flow_pattern_template_create(port_id, &pt_attr, pattern, error);
}

static struct rte_flow_actions_template *
snippet_match_nvgre_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
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
snippet_match_nvgre_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;
	const struct rte_flow_template_table_attr tbl_attr = {
		.flow_attr = {
			.group = 1,
			.priority = 0,
			.ingress = 1,
		},
		.nb_flows = 1,
	};

	pt = snippet_match_nvgre_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	at = snippet_match_nvgre_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &tbl_attr, &pt, 1, &at, 1, error);
}
