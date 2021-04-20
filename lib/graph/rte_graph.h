/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_H_
#define _RTE_GRAPH_H_

/**
 * @file rte_graph.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * Graph architecture abstracts the data processing functions as
 * "node" and "link" them together to create a complex "graph" to enable
 * reusable/modular data processing functions.
 *
 * This API enables graph framework operations such as create, lookup,
 * dump and destroy on graph and node operations such as clone,
 * edge update, and edge shrink, etc. The API also allows to create the stats
 * cluster to monitor per graph and per node stats.
 *
 */

#include <stdbool.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_GRAPH_NAMESIZE 64 /**< Max length of graph name. */
#define RTE_NODE_NAMESIZE 64  /**< Max length of node name. */
#define RTE_GRAPH_OFF_INVALID UINT32_MAX /**< Invalid graph offset. */
#define RTE_NODE_ID_INVALID UINT32_MAX   /**< Invalid node id. */
#define RTE_EDGE_ID_INVALID UINT16_MAX   /**< Invalid edge id. */
#define RTE_GRAPH_ID_INVALID UINT16_MAX  /**< Invalid graph id. */
#define RTE_GRAPH_FENCE 0xdeadbeef12345678ULL /**< Graph fence data. */

typedef uint32_t rte_graph_off_t;  /**< Graph offset type. */
typedef uint32_t rte_node_t;       /**< Node id type. */
typedef uint16_t rte_edge_t;       /**< Edge id type. */
typedef uint16_t rte_graph_t;      /**< Graph id type. */

/** Burst size in terms of log2 */
#if RTE_GRAPH_BURST_SIZE == 1
#define RTE_GRAPH_BURST_SIZE_LOG2 0  /**< Object burst size of 1. */
#elif RTE_GRAPH_BURST_SIZE == 2
#define RTE_GRAPH_BURST_SIZE_LOG2 1  /**< Object burst size of 2. */
#elif RTE_GRAPH_BURST_SIZE == 4
#define RTE_GRAPH_BURST_SIZE_LOG2 2  /**< Object burst size of 4. */
#elif RTE_GRAPH_BURST_SIZE == 8
#define RTE_GRAPH_BURST_SIZE_LOG2 3  /**< Object burst size of 8. */
#elif RTE_GRAPH_BURST_SIZE == 16
#define RTE_GRAPH_BURST_SIZE_LOG2 4  /**< Object burst size of 16. */
#elif RTE_GRAPH_BURST_SIZE == 32
#define RTE_GRAPH_BURST_SIZE_LOG2 5  /**< Object burst size of 32. */
#elif RTE_GRAPH_BURST_SIZE == 64
#define RTE_GRAPH_BURST_SIZE_LOG2 6  /**< Object burst size of 64. */
#elif RTE_GRAPH_BURST_SIZE == 128
#define RTE_GRAPH_BURST_SIZE_LOG2 7  /**< Object burst size of 128. */
#elif RTE_GRAPH_BURST_SIZE == 256
#define RTE_GRAPH_BURST_SIZE_LOG2 8  /**< Object burst size of 256. */
#else
#error "Unsupported burst size"
#endif

/* Forward declaration */
struct rte_node;  /**< Node object */
struct rte_graph; /**< Graph object */
struct rte_graph_cluster_stats;      /**< Stats for Cluster of graphs */
struct rte_graph_cluster_node_stats; /**< Node stats within cluster of graphs */

/**
 * Node process function.
 *
 * The function invoked when the worker thread walks on nodes using
 * rte_graph_walk().
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 * @param objs
 *   Pointer to an array of objects to be processed.
 * @param nb_objs
 *   Number of objects in the array.
 *
 * @return
 *   Number of objects processed.
 *
 * @see rte_graph_walk()
 *
 */
typedef uint16_t (*rte_node_process_t)(struct rte_graph *graph,
				       struct rte_node *node, void **objs,
				       uint16_t nb_objs);

/**
 * Node initialization function.
 *
 * The function invoked when the user creates the graph using rte_graph_create()
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 *
 * @return
 *   - 0: Success.
 *   -<0: Failure.
 *
 * @see rte_graph_create()
 */
typedef int (*rte_node_init_t)(const struct rte_graph *graph,
			       struct rte_node *node);

/**
 * Node finalization function.
 *
 * The function invoked when the user destroys the graph using
 * rte_graph_destroy().
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 *
 * @see rte_graph_destroy()
 */
typedef void (*rte_node_fini_t)(const struct rte_graph *graph,
				struct rte_node *node);

/**
 * Graph cluster stats callback.
 *
 * @param is_first
 *   Flag to denote that stats are of the first node.
 * @param is_last
 *   Flag to denote that stats are of the last node.
 * @param cookie
 *   Cookie supplied during stats creation.
 * @param stats
 *   Node cluster stats data.
 *
 * @return
 *   - 0: Success.
 *   -<0: Failure.
 */
typedef int (*rte_graph_cluster_stats_cb_t)(bool is_first, bool is_last,
	     void *cookie, const struct rte_graph_cluster_node_stats *stats);

/**
 * Structure to hold configuration parameters for creating the graph.
 *
 * @see rte_graph_create()
 */
struct rte_graph_param {
	int socket_id; /**< Socket id where memory is allocated. */
	uint16_t nb_node_patterns;  /**< Number of node patterns. */
	const char **node_patterns;
	/**< Array of node patterns based on shell pattern. */
};

/**
 * Structure to hold configuration parameters for graph cluster stats create.
 *
 * @see rte_graph_cluster_stats_create()
 */
struct rte_graph_cluster_stats_param {
	int socket_id;
	/**< Socket id where memory is allocated */
	rte_graph_cluster_stats_cb_t fn;
	/**< Stats print callback function. NULL value allowed, in that case,
	 *   default print stat function used.
	 */
	RTE_STD_C11
	union {
		void *cookie;
		FILE *f; /**< File pointer to dump the stats when fn == NULL. */
	};
	uint16_t nb_graph_patterns;  /**< Number of graph patterns. */
	const char **graph_patterns;
	/**< Array of graph patterns based on shell pattern. */
};

/**
 * Node cluster stats data structure.
 *
 * @see struct rte_graph_cluster_stats_param::fn
 */
struct rte_graph_cluster_node_stats {
	uint64_t ts;	    /**< Current timestamp. */
	uint64_t calls;	    /**< Current number of calls made. */
	uint64_t objs;      /**< Current number of objs processed. */
	uint64_t cycles;    /**< Current number of cycles. */

	uint64_t prev_ts;	/**< Previous call timestamp. */
	uint64_t prev_calls;	/**< Previous number of calls. */
	uint64_t prev_objs;	/**< Previous number of processed objs. */
	uint64_t prev_cycles;	/**< Previous number of cycles. */

	uint64_t realloc_count; /**< Realloc count. */

	rte_node_t id;	/**< Node identifier of stats. */
	uint64_t hz;	/**< Cycles per seconds. */
	char name[RTE_NODE_NAMESIZE];	/**< Name of the node. */
} __rte_cache_aligned;

/**
 * Create Graph.
 *
 * Create memory reel, detect loops and find isolated nodes.
 *
 * @param name
 *   Unique name for this graph.
 * @param prm
 *   Graph parameter, includes node names and count to be included
 *   in this graph.
 *
 * @return
 *   Unique graph id on success, RTE_GRAPH_ID_INVALID otherwise.
 */
__rte_experimental
rte_graph_t rte_graph_create(const char *name, struct rte_graph_param *prm);

/**
 * Destroy Graph.
 *
 * Free Graph memory reel.
 *
 * @param id
 *   id of the graph to destroy.
 *
 * @return
 *   0 on success, error otherwise.
 */
__rte_experimental
int rte_graph_destroy(rte_graph_t id);

/**
 * Get graph id from graph name.
 *
 * @param name
 *   Name of the graph to get id.
 *
 * @return
 *   Graph id on success, RTE_GRAPH_ID_INVALID otherwise.
 */
__rte_experimental
rte_graph_t rte_graph_from_name(const char *name);

/**
 * Get graph name from graph id.
 *
 * @param id
 *   id of the graph to get name.
 *
 * @return
 *   Graph name on success, NULL otherwise.
 */
__rte_experimental
char *rte_graph_id_to_name(rte_graph_t id);

/**
 * Export the graph as graph viz dot file
 *
 * @param name
 *   Name of the graph to export.
 * @param f
 *   File pointer to export the graph.
 *
 * @return
 *   0 on success, error otherwise.
 */
__rte_experimental
int rte_graph_export(const char *name, FILE *f);

/**
 * Get graph object from its name.
 *
 * Typical usage of this API to get graph objects in the worker thread and
 * followed calling rte_graph_walk() in a loop.
 *
 * @param name
 *   Name of the graph.
 *
 * @return
 *   Graph pointer on success, NULL otherwise.
 *
 * @see rte_graph_walk()
 */
__rte_experimental
struct rte_graph *rte_graph_lookup(const char *name);

/**
 * Get maximum number of graph available.
 *
 * @return
 *   Maximum graph count.
 */
__rte_experimental
rte_graph_t rte_graph_max_count(void);

/**
 * Dump the graph information to file.
 *
 * @param f
 *   File pointer to dump graph info.
 * @param id
 *   Graph id to get graph info.
 */
__rte_experimental
void rte_graph_dump(FILE *f, rte_graph_t id);

/**
 * Dump all graphs information to file
 *
 * @param f
 *   File pointer to dump graph info.
 */
__rte_experimental
void rte_graph_list_dump(FILE *f);

/**
 * Dump graph information along with node info to file
 *
 * @param f
 *   File pointer to dump graph info.
 * @param graph
 *   Graph pointer to get graph info.
 * @param all
 *   true to dump nodes in the graph.
 */
__rte_experimental
void rte_graph_obj_dump(FILE *f, struct rte_graph *graph, bool all);

/** Macro to browse rte_node object after the graph creation */
#define rte_graph_foreach_node(count, off, graph, node)                        \
	for (count = 0, off = graph->nodes_start,                              \
	     node = RTE_PTR_ADD(graph, off);                                   \
	     count < graph->nb_nodes;                                          \
	     off = node->next, node = RTE_PTR_ADD(graph, off), count++)

/**
 * Get node object with in graph from id.
 *
 * @param graph_id
 *   Graph id to get node pointer from.
 * @param node_id
 *   Node id to get node pointer.
 *
 * @return
 *   Node pointer on success, NULL otherwise.
 */
__rte_experimental
struct rte_node *rte_graph_node_get(rte_graph_t graph_id, rte_node_t node_id);

/**
 * Get node pointer with in graph from name.
 *
 * @param graph
 *   Graph name to get node pointer from.
 * @param name
 *   Node name to get the node pointer.
 *
 * @return
 *   Node pointer on success, NULL otherwise.
 */
__rte_experimental
struct rte_node *rte_graph_node_get_by_name(const char *graph,
					    const char *name);

/**
 * Create graph stats cluster to aggregate runtime node stats.
 *
 * @param prm
 *   Parameters including file pointer to dump stats,
 *   Graph pattern to create cluster and callback function.
 *
 * @return
 *   Valid pointer on success, NULL otherwise.
 */
__rte_experimental
struct rte_graph_cluster_stats *rte_graph_cluster_stats_create(
			const struct rte_graph_cluster_stats_param *prm);

/**
 * Destroy cluster stats.
 *
 * @param stat
 *    Valid cluster pointer to destroy.
 */
__rte_experimental
void rte_graph_cluster_stats_destroy(struct rte_graph_cluster_stats *stat);

/**
 * Get stats to application.
 *
 * @param[out] stat
 *   Cluster status.
 * @param skip_cb
 *   true to skip callback function invocation.
 */
__rte_experimental
void rte_graph_cluster_stats_get(struct rte_graph_cluster_stats *stat,
				 bool skip_cb);

/**
 * Reset cluster stats to zero.
 *
 * @param stat
 *   Valid cluster stats pointer.
 */
__rte_experimental
void rte_graph_cluster_stats_reset(struct rte_graph_cluster_stats *stat);

/**
 * Structure defines the node registration parameters.
 *
 * @see __rte_node_register(), RTE_NODE_REGISTER()
 */
struct rte_node_register {
	char name[RTE_NODE_NAMESIZE]; /**< Name of the node. */
	uint64_t flags;		      /**< Node configuration flag. */
#define RTE_NODE_SOURCE_F (1ULL << 0) /**< Node type is source. */
	rte_node_process_t process; /**< Node process function. */
	rte_node_init_t init;       /**< Node init function. */
	rte_node_fini_t fini;       /**< Node fini function. */
	rte_node_t id;		    /**< Node Identifier. */
	rte_node_t parent_id;       /**< Identifier of parent node. */
	rte_edge_t nb_edges;        /**< Number of edges from this node. */
	const char *next_nodes[];   /**< Names of next nodes. */
};

/**
 * Register new packet processing node. Nodes can be registered
 * dynamically via this call or statically via the RTE_NODE_REGISTER
 * macro.
 *
 * @param node
 *   Valid node pointer with name, process function and next_nodes.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 *
 * @see RTE_NODE_REGISTER()
 */
__rte_experimental
rte_node_t __rte_node_register(const struct rte_node_register *node);

/**
 * Register a static node.
 *
 * The static node is registered through the constructor scheme, thereby, it can
 * be used in a multi-process scenario.
 *
 * @param node
 *   Valid node pointer with name, process function, and next_nodes.
 */
#define RTE_NODE_REGISTER(node)                                                \
	RTE_INIT(rte_node_register_##node)                                     \
	{                                                                      \
		node.parent_id = RTE_NODE_ID_INVALID;                          \
		node.id = __rte_node_register(&node);                          \
	}

/**
 * Clone a node from static node(node created from RTE_NODE_REGISTER).
 *
 * @param id
 *   Static node id to clone from.
 * @param name
 *   Name of the new node. The library prepends the parent node name to the
 * user-specified name. The final node name will be,
 * "parent node name" + "-" + name.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 */
__rte_experimental
rte_node_t rte_node_clone(rte_node_t id, const char *name);

/**
 * Get node id from node name.
 *
 * @param name
 *   Valid node name. In the case of the cloned node, the name will be
 * "parent node name" + "-" + name.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 */
__rte_experimental
rte_node_t rte_node_from_name(const char *name);

/**
 * Get node name from node id.
 *
 * @param id
 *   Valid node id.
 *
 * @return
 *   Valid node name on success, NULL otherwise.
 */
__rte_experimental
char *rte_node_id_to_name(rte_node_t id);

/**
 * Get the number of edges(next-nodes) for a node from node id.
 *
 * @param id
 *   Valid node id.
 *
 * @return
 *   Valid edge count on success, RTE_EDGE_ID_INVALID otherwise.
 */
__rte_experimental
rte_edge_t rte_node_edge_count(rte_node_t id);

/**
 * Update the edges for a node from node id.
 *
 * @param id
 *   Valid node id.
 * @param from
 *   Index to update the edges from. RTE_EDGE_ID_INVALID is valid,
 * in that case, it will be added to the end of the list.
 * @param next_nodes
 *   Name of the edges to update.
 * @param nb_edges
 *   Number of edges to update.
 *
 * @return
 *   Valid edge count on success, 0 otherwise.
 */
__rte_experimental
rte_edge_t rte_node_edge_update(rte_node_t id, rte_edge_t from,
				const char **next_nodes, uint16_t nb_edges);

/**
 * Shrink the edges to a given size.
 *
 * @param id
 *   Valid node id.
 * @param size
 *   New size to shrink the edges.
 *
 * @return
 *   New size on success, RTE_EDGE_ID_INVALID otherwise.
 */
__rte_experimental
rte_edge_t rte_node_edge_shrink(rte_node_t id, rte_edge_t size);

/**
 * Get the edge names from a given node.
 *
 * @param id
 *   Valid node id.
 * @param[out] next_nodes
 *   Buffer to copy the edge names. The NULL value is allowed in that case,
 * the function returns the size of the array that needs to be allocated.
 *
 * @return
 *   When next_nodes == NULL, it returns the size of the array else
 *  number of item copied.
 */
__rte_experimental
rte_node_t rte_node_edge_get(rte_node_t id, char *next_nodes[]);

/**
 * Get maximum nodes available.
 *
 * @return
 *   Maximum nodes count.
 */
__rte_experimental
rte_node_t rte_node_max_count(void);

/**
 * Dump node info to file.
 *
 * @param f
 *   File pointer to dump the node info.
 * @param id
 *   Node id to get the info.
 */
__rte_experimental
void rte_node_dump(FILE *f, rte_node_t id);

/**
 * Dump all node info to file.
 *
 * @param f
 *   File pointer to dump the node info.
 */
__rte_experimental
void rte_node_list_dump(FILE *f);

/**
 * Test the validity of node id.
 *
 * @param id
 *   Node id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_node_is_invalid(rte_node_t id)
{
	return (id == RTE_NODE_ID_INVALID);
}

/**
 * Test the validity of edge id.
 *
 * @param id
 *   Edge node id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_edge_is_invalid(rte_edge_t id)
{
	return (id == RTE_EDGE_ID_INVALID);
}

/**
 * Test the validity of graph id.
 *
 * @param id
 *   Graph id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_graph_is_invalid(rte_graph_t id)
{
	return (id == RTE_GRAPH_ID_INVALID);
}

/**
 * Test stats feature support.
 *
 * @return
 *   1 if stats enabled, 0 otherwise.
 */
static __rte_always_inline int
rte_graph_has_stats_feature(void)
{
#ifdef RTE_LIBRTE_GRAPH_STATS
	return RTE_LIBRTE_GRAPH_STATS;
#else
	return 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_H_ */
