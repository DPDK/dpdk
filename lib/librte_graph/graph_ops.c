/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <stdbool.h>
#include <string.h>

#include <rte_common.h>
#include <rte_errno.h>

#include "graph_private.h"

/* Check whether a node has next_node to itself */
static inline int
node_has_loop_edge(struct node *node)
{
	rte_edge_t i;
	char *name;
	int rc = 0;

	for (i = 0; i < node->nb_edges; i++) {
		if (strncmp(node->name, node->next_nodes[i],
			    RTE_NODE_NAMESIZE) == 0) {
			name = node->name;
			rc = 1;
			SET_ERR_JMP(EINVAL, fail, "Node %s has loop to self",
				    name);
		}
	}
fail:
	return rc;
}

int
graph_node_has_loop_edge(struct graph *graph)
{
	struct graph_node *graph_node;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (node_has_loop_edge(graph_node->node))
			return 1;

	return 0;
}

rte_node_t
graph_src_nodes_count(struct graph *graph)
{
	struct graph_node *graph_node;
	rte_node_t rc = 0;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (graph_node->node->flags & RTE_NODE_SOURCE_F)
			rc++;

	if (rc == 0)
		SET_ERR_JMP(EINVAL, fail, "Graph needs at least a source node");
fail:
	return rc;
}

/* Check whether a node has next_node to a source node */
int
graph_node_has_edge_to_src_node(struct graph *graph)
{
	struct graph_node *graph_node;
	struct node *node;
	rte_edge_t i;

	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		for (i = 0; i < graph_node->node->nb_edges; i++) {
			node = graph_node->adjacency_list[i]->node;
			if (node->flags & RTE_NODE_SOURCE_F)
				SET_ERR_JMP(
					EEXIST, fail,
					"Node %s points to the source node %s",
					graph_node->node->name, node->name);
		}
	}

	return 0;
fail:
	return 1;
}

rte_node_t
graph_nodes_count(struct graph *graph)
{
	struct graph_node *graph_node;
	rte_node_t count = 0;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		count++;

	return count;
}

void
graph_mark_nodes_as_not_visited(struct graph *graph)
{
	struct graph_node *graph_node;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		graph_node->visited = false;
}

int
graph_bfs(struct graph *graph, struct graph_node *start)
{
	struct graph_node **queue, *v, *tmp;
	uint16_t head = 0, tail = 0;
	rte_edge_t i;
	size_t sz;

	sz = sizeof(struct graph_node *) * graph_nodes_count(graph);
	queue = malloc(sz);
	if (queue == NULL)
		SET_ERR_JMP(ENOMEM, fail, "Failed to alloc BFS queue of %zu",
			    sz);

	/* BFS algorithm */
	queue[tail++] = start;
	start->visited = true;
	while (head != tail) {
		v = queue[head++];
		for (i = 0; i < v->node->nb_edges; i++) {
			tmp = v->adjacency_list[i];
			if (tmp->visited == false) {
				queue[tail++] = tmp;
				tmp->visited = true;
			}
		}
	}

	free(queue);
	return 0;

fail:
	return -rte_errno;
}

/* Check whether a node has connected path or parent node */
int
graph_has_isolated_node(struct graph *graph)
{
	struct graph_node *graph_node;

	graph_mark_nodes_as_not_visited(graph);

	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		if (graph_node->node->flags & RTE_NODE_SOURCE_F) {
			if (graph_node->node->nb_edges == 0)
				SET_ERR_JMP(EINVAL, fail,
					    "%s node needs minimum one edge",
					    graph_node->node->name);
			if (graph_bfs(graph, graph_node))
				goto fail;
		}
	}

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (graph_node->visited == false)
			SET_ERR_JMP(EINVAL, fail, "Found isolated node %s",
				    graph_node->node->name);

	return 0;
fail:
	return 1;
}
