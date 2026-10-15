/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#ifndef _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_
#define _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_

/**
 * @file rte_graph_model_mcore_dispatch.h
 *
 * These APIs allow setting core affinity with the node and only used for mcore
 * dispatch model.
 */

#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_ring.h>

#include "rte_graph_worker_common.h"

#ifdef __cplusplus
extern "C" {
#endif

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
int rte_graph_model_mcore_dispatch_node_lcore_affinity_set(const char *name,
							   unsigned int lcore_id);

/**
 * Perform graph walk on the pending bitmap and invoke the process function
 * of the nodes and collect the stats.
 *
 * Nodes are visited in topological order (upstream before downstream).
 * Source nodes are seeded into the pending bitmap at the start of each walk.
 * Nodes with different lcore affinity are dispatched to their target lcore.
 *
 * @param graph
 *   Graph pointer returned from rte_graph_lookup function.
 *
 * @see rte_graph_lookup()
 */
static inline void
rte_graph_walk_mcore_dispatch(struct rte_graph *graph)
{
	const uint16_t nwords = graph->nb_sched_words;
	struct rte_node *node;
	uint16_t word, bit;

	if (graph->dispatch.wq != NULL)
		__rte_graph_mcore_dispatch_sched_wq_process(graph);

	/* Seed pending bitmap with source nodes bound to this lcore */
	for (word = 0; word < nwords; word++)
		graph->pending[word] |= graph->src_pending[word];

	for (;;) {
		/* find first word with any pending bit */
		for (word = 0; word < nwords; word++)
			if (graph->pending[word])
				break;
		if (word == nwords)
			break; /* no more pending nodes */

		bit = rte_ctz64(graph->pending[word]);
		graph->pending[word] &= ~(1ULL << bit);
		node = __rte_graph_pending_node(graph, word, bit);

		/* Schedule the node until all task/objs are done */
		if (node->dispatch.lcore_id != RTE_MAX_LCORE &&
		    graph->dispatch.lcore_id != node->dispatch.lcore_id &&
		    graph->dispatch.rq != NULL &&
		    __rte_graph_mcore_dispatch_sched_node_enqueue(node, graph->dispatch.rq))
			continue;

		__rte_node_process(graph, node);
	}
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_ */
