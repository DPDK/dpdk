/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_PRIVATE_H_
#define _RTE_GRAPH_PRIVATE_H_

#include <inttypes.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_eal.h>

#include "rte_graph.h"

/**
 * @internal
 *
 * Structure that holds node internal data.
 */
struct node {
	STAILQ_ENTRY(node) next;      /**< Next node in the list. */
	char name[RTE_NODE_NAMESIZE]; /**< Name of the node. */
	uint64_t flags;		      /**< Node configuration flag. */
	rte_node_process_t process;   /**< Node process function. */
	rte_node_init_t init;         /**< Node init function. */
	rte_node_fini_t fini;	      /**< Node fini function. */
	rte_node_t id;		      /**< Allocated identifier for the node. */
	rte_node_t parent_id;	      /**< Parent node identifier. */
	rte_edge_t nb_edges;	      /**< Number of edges from this node. */
	char next_nodes[][RTE_NODE_NAMESIZE]; /**< Names of next nodes. */
};

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

/* Lock functions */
/**
 * @internal
 *
 * Take a lock on the graph internal spin lock.
 */
void graph_spinlock_lock(void);

/**
 * @internal
 *
 * Release a lock on the graph internal spin lock.
 */
void graph_spinlock_unlock(void);

#endif /* _RTE_GRAPH_PRIVATE_H_ */
