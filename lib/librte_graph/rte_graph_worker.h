/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_WORKER_H_
#define _RTE_GRAPH_WORKER_H_

/**
 * @file rte_graph_worker.h
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This API allows a worker thread to walk over a graph and nodes to create,
 * process, enqueue and move streams of objects to the next nodes.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_memcpy.h>
#include <rte_memory.h>

#include "rte_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 *
 * Data structure to hold graph data.
 */
struct rte_graph {
	uint32_t tail;		     /**< Tail of circular buffer. */
	uint32_t head;		     /**< Head of circular buffer. */
	uint32_t cir_mask;	     /**< Circular buffer wrap around mask. */
	rte_node_t nb_nodes;	     /**< Number of nodes in the graph. */
	rte_graph_off_t *cir_start;  /**< Pointer to circular buffer. */
	rte_graph_off_t nodes_start; /**< Offset at which node memory starts. */
	rte_graph_t id;	/**< Graph identifier. */
	int socket;	/**< Socket ID where memory is allocated. */
	char name[RTE_GRAPH_NAMESIZE];	/**< Name of the graph. */
	uint64_t fence;			/**< Fence. */
} __rte_cache_aligned;

/**
 * @internal
 *
 * Data structure to hold node data.
 */
struct rte_node {
	/* Slow path area  */
	uint64_t fence;		/**< Fence. */
	rte_graph_off_t next;	/**< Index to next node. */
	rte_node_t id;		/**< Node identifier. */
	rte_node_t parent_id;	/**< Parent Node identifier. */
	rte_edge_t nb_edges;	/**< Number of edges from this node. */
	uint32_t realloc_count;	/**< Number of times realloced. */

	char parent[RTE_NODE_NAMESIZE];	/**< Parent node name. */
	char name[RTE_NODE_NAMESIZE];	/**< Name of the node. */

	/* Fast path area  */
#define RTE_NODE_CTX_SZ 16
	uint8_t ctx[RTE_NODE_CTX_SZ] __rte_cache_aligned; /**< Node Context. */
	uint16_t size;		/**< Total number of objects available. */
	uint16_t idx;		/**< Number of objects used. */
	rte_graph_off_t off;	/**< Offset of node in the graph reel. */
	uint64_t total_cycles;	/**< Cycles spent in this node. */
	uint64_t total_calls;	/**< Calls done to this node. */
	uint64_t total_objs;	/**< Objects processed by this node. */
	RTE_STD_C11
		union {
			void **objs;	   /**< Array of object pointers. */
			uint64_t objs_u64;
		};
	RTE_STD_C11
		union {
			rte_node_process_t process; /**< Process function. */
			uint64_t process_u64;
		};
	struct rte_node *nodes[] __rte_cache_min_aligned; /**< Next nodes. */
} __rte_cache_aligned;

/**
 * @internal
 *
 * Allocate a stream of objects.
 *
 * If stream already exists then re-allocate it to a larger size.
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 */
__rte_experimental
void __rte_node_stream_alloc(struct rte_graph *graph, struct rte_node *node);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_WORKER_H_ */
