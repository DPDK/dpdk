/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_PRIVATE_H_
#define _RTE_GRAPH_PRIVATE_H_

#include <inttypes.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_spinlock.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "rte_graph.h"
#include "rte_graph_worker.h"

extern int rte_graph_logtype;
#define RTE_LOGTYPE_GRAPH rte_graph_logtype

#define GRAPH_LOG(level, ...)                                                  \
	RTE_LOG_LINE_PREFIX(level, GRAPH,                                      \
		"%s():%u ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#define GRAPH_LOG2(level, _fname, _linenum, ...)                               \
	RTE_LOG_LINE_PREFIX(level, GRAPH,                                      \
		"%s():%u ", _fname RTE_LOG_COMMA _linenum, __VA_ARGS__)

#define graph_err(...) GRAPH_LOG(ERR, __VA_ARGS__)
#define graph_warn(...) GRAPH_LOG(WARNING, __VA_ARGS__)
#define graph_info(...) GRAPH_LOG(INFO, __VA_ARGS__)
#define graph_dbg(...) GRAPH_LOG(DEBUG, __VA_ARGS__)

#define SET_ERR_JMP(err, where, fmt, ...)                                      \
	do {                                                                   \
		graph_err(fmt, ##__VA_ARGS__);                                 \
		rte_errno = err;                                               \
		goto where;                                                    \
	} while (0)

/**
 * @internal
 *
 * Structure that holds node internal data.
 */
struct node {
	STAILQ_ENTRY(node) next;      /**< Next node in the list. */
	char name[RTE_NODE_NAMESIZE]; /**< Name of the node. */
	uint64_t flags;		      /**< Node configuration flag. */
	unsigned int lcore_id;
	/**< Node runs on the Lcore ID used for mcore dispatch model. */
	rte_node_process_t process;   /**< Node process function. */
	rte_node_init_t init;         /**< Node init function. */
	rte_node_fini_t fini;	      /**< Node fini function. */
	rte_node_t id;		      /**< Allocated identifier for the node. */
	rte_node_t parent_id;	      /**< Parent node identifier. */
	rte_edge_t nb_edges;	      /**< Number of edges from this node. */
	struct rte_node_xstats *xstats;	      /**< Node specific xstats. */
	char next_nodes[][RTE_NODE_NAMESIZE]; /**< Names of next nodes. */
};

/**
 * @internal
 *
 * Structure that holds the graph scheduling workqueue node stream.
 * Used for mcore dispatch model.
 */
struct __rte_cache_aligned graph_mcore_dispatch_wq_node {
	rte_graph_off_t node_off;
	uint16_t nb_objs;
	void *objs[RTE_GRAPH_BURST_SIZE];
};

/**
 * @internal
 *
 * Structure that holds the graph node data.
 */
struct graph_node {
	STAILQ_ENTRY(graph_node) next; /**< Next graph node in the list. */
	struct node *node; /**< Pointer to internal node. */
	bool visited;      /**< Flag used in BFS to mark node visited. */
	struct graph_node *adjacency_list[]; /**< Adjacency list of the node. */
};

/**
 * @internal
 *
 * Structure that holds graph internal data.
 */
struct graph {
	STAILQ_ENTRY(graph) next;
	/**< List of graphs. */
	char name[RTE_GRAPH_NAMESIZE];
	/**< Name of the graph. */
	const struct rte_memzone *mz;
	/**< Memzone to store graph data. */
	rte_graph_off_t nodes_start;
	/**< Node memory start offset in graph reel. */
	rte_graph_off_t xstats_start;
	/**< Node xstats memory start offset in graph reel. */
	rte_node_t src_node_count;
	/**< Number of source nodes in a graph. */
	struct rte_graph *graph;
	/**< Pointer to graph data. */
	rte_node_t node_count;
	/**< Total number of nodes. */
	uint32_t cir_start;
	/**< Circular buffer start offset in graph reel. */
	uint32_t cir_mask;
	/**< Circular buffer mask for wrap around. */
	rte_graph_t id;
	/**< Graph identifier. */
	rte_graph_t parent_id;
	/**< Parent graph identifier. */
	unsigned int lcore_id;
	/**< Lcore identifier where the graph prefer to run on. Used for mcore dispatch model. */
	size_t mem_sz;
	/**< Memory size of the graph. */
	int socket;
	/**< Socket identifier where memory is allocated. */
	uint64_t num_pkt_to_capture;
	/**< Number of packets to be captured per core. */
	char pcap_filename[RTE_GRAPH_PCAP_FILE_SZ];
	/**< pcap file name/path. */
	STAILQ_HEAD(gnode_list, graph_node) node_list;
	/**< Nodes in a graph. */
};

/* Node and graph common functions */
/**
 * @internal
 *
 * Naming a cloned graph or node by appending a string to base name.
 *
 * @param new_name
 *   Pointer to the name of the cloned object.
 * @param base_name
 *   Pointer to the name of original object.
 * @param append_str
 *   Pointer to the appended string.
 *
 * @return
 *   0 on success, negative errno value otherwise.
 */
static inline int clone_name(char *new_name, char *base_name, const char *append_str)
{
	ssize_t sz, rc;

#define SZ RTE_MIN(RTE_NODE_NAMESIZE, RTE_GRAPH_NAMESIZE)
	rc = rte_strscpy(new_name, base_name, SZ);
	if (rc < 0)
		goto fail;
	sz = rc;
	rc = rte_strscpy(new_name + sz, "-", RTE_MAX((int16_t)(SZ - sz), 0));
	if (rc < 0)
		goto fail;
	sz += rc;
	sz = rte_strscpy(new_name + sz, append_str, RTE_MAX((int16_t)(SZ - sz), 0));
	if (sz < 0)
		goto fail;

	return 0;
fail:
	rte_errno = E2BIG;
	return -rte_errno;
}

/* Node functions */
STAILQ_HEAD(node_head, node);

/**
 * @internal
 *
 * Get the head of the node list.
 *
 * @return
 *   Pointer to the node head.
 */
struct node_head *node_list_head_get(void);

/**
 * @internal
 *
 * Get node pointer from node name.
 *
 * @param name
 *   Pointer to character string containing the node name.
 *
 * @return
 *   Pointer to the node.
 */
struct node *node_from_name(const char *name);

/**
 * @internal
 *
 * Override process func of a node.
 *
 * @return
 *   - 0: Success.
 *   - <0: Error
 */
int node_override_process_func(rte_node_t id, rte_node_process_t process);

/* Graph list functions */
STAILQ_HEAD(graph_head, graph);

/**
 * @internal
 *
 * Get the head of the graph list.
 *
 * @return
 *   Pointer to the graph head.
 */
struct graph_head *graph_list_head_get(void);

rte_spinlock_t *
graph_spinlock_get(void);

/* Lock functions */
/**
 * @internal
 *
 * Take a lock on the graph internal spin lock.
 */
void graph_spinlock_lock(void)
	__rte_acquire_capability(graph_spinlock_get());

/**
 * @internal
 *
 * Release a lock on the graph internal spin lock.
 */
void graph_spinlock_unlock(void)
	__rte_release_capability(graph_spinlock_get());

/* Graph operations */
/**
 * @internal
 *
 * Run a BFS(Breadth First Search) on the graph marking all
 * the graph nodes as visited.
 *
 * @param graph
 *   Pointer to the internal graph object.
 * @param start
 *   Pointer to the starting graph node.
 *
 * @return
 *   - 0: Success.
 *   - -ENOMEM: Not enough memory for BFS.
 */
int graph_bfs(struct graph *graph, struct graph_node *start);

/**
 * @internal
 *
 * Check if there is an isolated node in the given graph.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   - 0: No isolated node found.
 *   - 1: Isolated node found.
 */
int graph_has_isolated_node(struct graph *graph);

/**
 * @internal
 *
 * Check whether a node in the graph has next_node to a source node.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   - 0: Node has an edge to source node.
 *   - 1: Node doesn't have an edge to source node.
 */
int graph_node_has_edge_to_src_node(struct graph *graph);

/**
 * @internal
 *
 * Checks whether node in the graph has a edge to itself i.e. forms a
 * loop.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   - 0: Node has an edge to itself.
 *   - 1: Node doesn't have an edge to itself.
 */
int graph_node_has_loop_edge(struct graph *graph);

/**
 * @internal
 *
 * Get the count of source nodes in the graph.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   Number of source nodes.
 */
rte_node_t graph_src_nodes_count(struct graph *graph);

/**
 * @internal
 *
 * Get the count of total number of nodes in the graph.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   Number of nodes.
 */
rte_node_t graph_nodes_count(struct graph *graph);

/**
 * @internal
 *
 * Clear the visited flag of all the nodes in the graph.
 *
 * @param graph
 *   Pointer to the internal graph object.
 */
void graph_mark_nodes_as_not_visited(struct graph *graph);

/* Fast path graph memory populate unctions */

/**
 * @internal
 *
 * Create fast-path memory for the graph and nodes.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   - 0: Success.
 *   - -ENOMEM: Not enough for graph and nodes.
 *   - -EINVAL: Graph nodes not found.
 */
int graph_fp_mem_create(struct graph *graph);

/**
 * @internal
 *
 * Free fast-path memory used by graph and nodes.
 *
 * @param graph
 *   Pointer to the internal graph object.
 *
 * @return
 *   - 0: Success.
 *   - <0: Graph memzone related error.
 */
int graph_fp_mem_destroy(struct graph *graph);

/* Lookup functions */
/**
 * @internal
 *
 * Get graph node object from node id.
 *
 * @param graph
 *   Pointer to rte_graph object.
 * @param id
 *   Node Identifier.
 *
 * @return
 *   Pointer to rte_node if identifier is valid else NULL.
 */
struct rte_node *graph_node_id_to_ptr(const struct rte_graph *graph,
				      rte_node_t id);

/**
 * @internal
 *
 * Get graph node object from node name.
 *
 * @param graph
 *   Pointer to rte_graph object.
 * @param node_name
 *   Pointer to character string holding the node name.
 *
 * @return
 *   Pointer to rte_node if identifier is valid else NULL.
 */
struct rte_node *graph_node_name_to_ptr(const struct rte_graph *graph,
					const char *node_name);

/* Debug functions */
/**
 * @internal
 *
 * Dump internal graph object data.
 *
 * @param f
 *   FILE pointer to dump the data.
 * @param g
 *   Pointer to the internal graph object.
 */
void graph_dump(FILE *f, struct graph *g);

/**
 * @internal
 *
 * Dump internal node object data.
 *
 * @param f
 *   FILE pointer to dump the info.
 * @param g
 *   Pointer to the internal node object.
 */
void node_dump(FILE *f, struct node *n);

/**
 * @internal
 *
 * Create the graph schedule work queue for mcore dispatch model.
 * All cloned graphs attached to the parent graph MUST be destroyed together
 * for fast schedule design limitation.
 *
 * @param _graph
 *   The graph object
 * @param _parent_graph
 *   The parent graph object which holds the run-queue head.
 * @param prm
 *   Graph parameter, includes model-specific parameters in this graph.
 *
 * @return
 *   - 0: Success.
 *   - <0: Graph schedule work queue related error.
 */
int graph_sched_wq_create(struct graph *_graph, struct graph *_parent_graph,
			   struct rte_graph_param *prm);

/**
 * @internal
 *
 * Destroy the graph schedule work queue for mcore dispatch model.
 *
 * @param _graph
 *   The graph object
 */
void graph_sched_wq_destroy(struct graph *_graph);

/**
 * @internal
 *
 * Check if given node present in any of the created graphs.
 *
 * @param node
 *   The node object
 *
 * @return
 *   0 if not present in any graph, else return 1.
 */
bool graph_is_node_active_in_graph(struct node *_node);

#endif /* _RTE_GRAPH_PRIVATE_H_ */
