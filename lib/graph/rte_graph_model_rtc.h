/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 * Copyright(C) 2023 Intel Corporation
 */

#include "rte_graph_worker_common.h"

/**
 * Perform graph walk on the circular buffer and invoke the process function
 * of the nodes and collect the stats.
 *
 * @param graph
 *   Graph pointer returned from rte_graph_lookup function.
 *
 * @see rte_graph_lookup()
 */
static inline void
rte_graph_walk_rtc(struct rte_graph *graph)
{
	const rte_graph_off_t *cir_start = graph->cir_start;
	const rte_node_t mask = graph->cir_mask;
	uint32_t head = graph->head;
	struct rte_node *node;

	/*
	 * Walk on the source node(s) ((cir_start - head) -> cir_start) and then
	 * on the pending streams (cir_start -> (cir_start + mask) -> cir_start)
	 * in a circular buffer fashion.
	 *
	 *	+-----+ <= cir_start - head [number of source nodes]
	 *	|     |
	 *	| ... | <= source nodes
	 *	|     |
	 *	+-----+ <= cir_start [head = 0] [tail = 0]
	 *	|     |
	 *	| ... | <= pending streams
	 *	|     |
	 *	+-----+ <= cir_start + mask
	 */
	while (likely(head != graph->tail)) {
		node = (struct rte_node *)RTE_PTR_ADD(graph, cir_start[(int32_t)head++]);
		__rte_node_process(graph, node);
		head = likely((int32_t)head > 0) ? head & mask : head;
	}
	graph->tail = 0;
}
