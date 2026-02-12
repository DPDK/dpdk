/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <rte_flow.h>

#include "../common.h"
#include "snippet_match_port_affinity.h"

void
snippet_init_match_port_affinity(void)
{
	init_default_snippet();
}

static void
map_tx_queue_to_aggregated_port(uint16_t port_id)
{
	int ret;
	uint16_t queues[] = {0, 1, 2, 3};
	uint16_t affinities[] = {1, 1, 2, 2};
	int i;

	ret = rte_eth_dev_stop(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_stop:err=%d, port=%u\n",
			ret, port_id);

	ret = rte_eth_dev_count_aggr_ports(port_id);
	if (ret < 0) {
		printf("Failed to count the aggregated ports: (%s)\n",
				strerror(-ret));
		return;
	}

	/* Configure TxQ index 0,1 with tx affinity 1 and TxQ index 2,3 with tx affinity 2 */
	for (i = 0; i < NR_QUEUES; i++) {
		ret = rte_eth_dev_map_aggr_tx_affinity(port_id, queues[i], affinities[i]);
		if (ret != 0) {
			printf("Failed to map tx queue with an aggregated port: %s\n",
					rte_strerror(-ret));
			return;
		}
		printf(":: tx queue %d mapped to aggregated port %d with affinity %d\n",
				queues[i], port_id, affinities[i]);
	}

	ret = rte_eth_dev_start(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, port_id);
}

void
snippet_match_port_affinity_create_actions(uint16_t port_id, struct rte_flow_action *action)
{
	/* Configure affinity in TxQ */
	map_tx_queue_to_aggregated_port(port_id);

	struct rte_flow_action_queue *queue = calloc(1, sizeof(struct rte_flow_action_queue));
	if (queue == NULL) {
		printf("Failed to allocate memory for queue\n");
		return;
	}
	queue->index = 1;
	/* Create the Queue action. */
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

void
snippet_match_port_affinity_create_patterns(struct rte_flow_item *pattern)
{
	struct rte_flow_item_aggr_affinity *affinity_spec =
		calloc(1, sizeof(struct rte_flow_item_aggr_affinity));
	if (affinity_spec == NULL) {
		fprintf(stderr, "Failed to allocate memory for affinity_spec\n");
		return;
	}

	struct rte_flow_item_aggr_affinity *affinity_mask =
		calloc(1, sizeof(struct rte_flow_item_aggr_affinity));
	if (affinity_mask == NULL) {
		fprintf(stderr, "Failed to allocate memory for affinity_mask\n");
		return;
	}

	affinity_spec->affinity = 2;	/* Set the request affinity value. */
	affinity_mask->affinity = 0xff;	/* Set the mask for the affinity value. */

	/* Create a rule that matches the port affinity values */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY;
	pattern[1].spec = affinity_spec;
	pattern[1].mask = affinity_mask;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
}

struct rte_flow_template_table *
snippet_match_port_affinity_create_table(__rte_unused uint16_t port_id,
	__rte_unused struct rte_flow_error *error)
{
	return NULL;
}
