/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#ifndef _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_
#define _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_

/**
 * @file rte_graph_model_mcore_dispatch.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * These APIs allow to set core affinity with the node and only used for mcore
 * dispatch model.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_ring.h>

#include "rte_graph_worker_common.h"

#define RTE_GRAPH_SCHED_WQ_SIZE_MULTIPLIER  8
#define RTE_GRAPH_SCHED_WQ_SIZE(nb_nodes)   \
	((typeof(nb_nodes))((nb_nodes) * RTE_GRAPH_SCHED_WQ_SIZE_MULTIPLIER))

/**
 * @internal
 *
 * Schedule the node to the right graph's work queue for mcore dispatch model.
 *
 * @param node
 *   Pointer to the scheduled node object.
 * @param rq
 *   Pointer to the scheduled run-queue for all graphs.
 *
 * @return
 *   True on success, false otherwise.
 *
 * @note
 * This implementation is used by mcore dispatch model only and user application
 * should not call it directly.
 */
__rte_experimental
bool __rte_noinline __rte_graph_mcore_dispatch_sched_node_enqueue(struct rte_node *node,
								  struct rte_graph_rq_head *rq);

/**
 * @internal
 *
 * Process all nodes (streams) in the graph's work queue for mcore dispatch model.
 *
 * @param graph
 *   Pointer to the graph object.
 *
 * @note
 * This implementation is used by mcore dispatch model only and user application
 * should not call it directly.
 */
__rte_experimental
void __rte_graph_mcore_dispatch_sched_wq_process(struct rte_graph *graph);

/**
 * Set lcore affinity with the node used for mcore dispatch model.
 *
 * @param name
 *   Valid node name. In the case of the cloned node, the name will be
 * "parent node name" + "-" + name.
 * @param lcore_id
 *   The lcore ID value.
 *
 * @return
 *   0 on success, error otherwise.
 */
__rte_experimental
int rte_graph_model_mcore_dispatch_node_lcore_affinity_set(const char *name,
							   unsigned int lcore_id);

/**
 * Perform graph walk on the circular buffer and invoke the process function
 * of the nodes and collect the stats.
 *
 * @param graph
 *   Graph pointer returned from rte_graph_lookup function.
 *
 * @see rte_graph_lookup()
 */
__rte_experimental
static inline void
rte_graph_walk_mcore_dispatch(struct rte_graph *graph)
{
	const rte_graph_off_t *cir_start = graph->cir_start;
	const rte_node_t mask = graph->cir_mask;
	uint32_t head = graph->head;
	struct rte_node *node;

	if (graph->dispatch.wq != NULL)
		__rte_graph_mcore_dispatch_sched_wq_process(graph);

	while (likely(head != graph->tail)) {
		node = (struct rte_node *)RTE_PTR_ADD(graph, cir_start[(int32_t)head++]);

		/* skip the src nodes which not bind with current worker */
		if ((int32_t)head < 0 && node->dispatch.lcore_id != graph->dispatch.lcore_id)
			continue;

		/* Schedule the node until all task/objs are done */
		if (node->dispatch.lcore_id != RTE_MAX_LCORE &&
		    graph->dispatch.lcore_id != node->dispatch.lcore_id &&
		    graph->dispatch.rq != NULL &&
		    __rte_graph_mcore_dispatch_sched_node_enqueue(node, graph->dispatch.rq))
			continue;

		__rte_node_process(graph, node);

		head = likely((int32_t)head > 0) ? head & mask : head;
	}

	graph->tail = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_ */
