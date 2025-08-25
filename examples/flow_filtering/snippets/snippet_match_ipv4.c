/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>

#include <rte_errno.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_match_ipv4.h"

void
snippet_init_ipv4(void)
{
	init_default_snippet();
}

void
snippet_ipv4_flow_create_actions(__rte_unused uint16_t port_id, struct rte_flow_action *action)
{
	/*
	 * create the action sequence.
	 * one action only, move packet to queue
	 */
	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL)
		fprintf(stderr, "Failed to allocate memory for queue\n");
	queue->index = 1; /* The selected target queue.*/
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_ipv4_flow_create_patterns(struct rte_flow_item *patterns)
{
	struct rte_flow_item_ipv4 *ip_spec;
	struct rte_flow_item_ipv4 *ip_mask;

	/*
	 * set the first level of the pattern (ETH).
	 * since in this example we just want to get the
	 * IPV4 we set this level to allow all.
	 */
	patterns[0].type = RTE_FLOW_ITEM_TYPE_ETH;

	/*
	 * setting the second level of the pattern (IP).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */
	patterns[1].type = RTE_FLOW_ITEM_TYPE_IPV4;

	ip_spec = calloc(1, sizeof(struct rte_flow_item_ipv4));
	if (ip_spec == NULL)
		fprintf(stderr, "Failed to allocate memory for ip_spec\n");

	ip_mask = calloc(1, sizeof(struct rte_flow_item_ipv4));
	if (ip_mask == NULL)
		fprintf(stderr, "Failed to allocate memory for ip_mask\n");

	/* Match destination IP 192.168.1.1 with full mask */
	ip_spec->hdr.dst_addr = htonl(((192<<24) + (168<<16) + (1<<8) + 1));
	ip_mask->hdr.dst_addr = 0xffffffff;

	/* Match any source IP by using empty mask */
	ip_spec->hdr.src_addr = htonl(((0<<24) + (0<<16) + (0<<8) + 0));
	ip_mask->hdr.src_addr = 0x0;

	patterns[1].spec = ip_spec;
	patterns[1].mask = ip_mask;

	/* The final level must be always type end. */
	patterns[2].type = RTE_FLOW_ITEM_TYPE_END;

}

/* creates a template that holds a list of action types without any specific values set. */
static struct rte_flow_actions_template *
snippet_ipv4_flow_create_actions_template(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_action tactions[MAX_ACTION_NUM] = {0};
	struct rte_flow_action masks[MAX_ACTION_NUM] = {0};
	struct rte_flow_actions_template_attr action_attr = {
		.ingress = 1,
	};

	tactions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	tactions[1].type = RTE_FLOW_ACTION_TYPE_END;

	/* This sets the masks to match the actions, indicating that all fields of the actions
	 * should be considered as part of the template.
	 */
	memcpy(masks, tactions, sizeof(masks));

	/* Create the flow actions template using the configured attributes, actions, and masks */
	return rte_flow_actions_template_create(port_id, &action_attr,
						tactions, masks, error);
}

static struct rte_flow_pattern_template *
snippet_ipv4_flow_create_pattern_template(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_item titems[MAX_PATTERN_NUM] = {0};
	struct rte_flow_item_ipv4 ip_mask = {0};

	struct rte_flow_pattern_template_attr attr = {
			.relaxed_matching = 1,
			.ingress = 1
	};

	titems[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	titems[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	ip_mask.hdr.src_addr = 0x0;  /* empty mask */
	ip_mask.hdr.dst_addr = 0xffffffff;  /* full mask */
	titems[1].mask = &ip_mask;
	titems[2].type = RTE_FLOW_ITEM_TYPE_END;

	return rte_flow_pattern_template_create(port_id, &attr, titems, error);
}

struct rte_flow_template_table *
snippet_ipv4_flow_create_table(uint16_t port_id, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template *pt;
	struct rte_flow_actions_template *at;

	/* Set the rule attribute, only ingress packets will be checked. */
	struct rte_flow_template_table_attr table_attr = {
			.flow_attr = {
				.group = 0,
				.priority = 0,
				.ingress = 1,
				.egress = 0,
				.transfer = 0,
				.reserved = 0,
		},
			/* Maximum number of flow rules that this table holds. */
			.nb_flows = 1,
	};

	/* The pattern template defines common matching fields without values.
	 * The number and order of items in the template must be the same at the rule creation.
	 */
	pt = snippet_ipv4_flow_create_pattern_template(port_id, error);
	if (pt == NULL) {
		printf("Failed to create pattern template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	/* The actions template holds a list of action types without values.
	 * The number and order of actions in the template must be the same at the rule creation.
	 */
	at = snippet_ipv4_flow_create_actions_template(port_id, error);
	if (at == NULL) {
		printf("Failed to create actions template: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_template_table_create(port_id, &table_attr, &pt, 1, &at, 1, error);
}
