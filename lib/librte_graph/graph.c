/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <fnmatch.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "graph_private.h"

static struct graph_head graph_list = STAILQ_HEAD_INITIALIZER(graph_list);
static rte_spinlock_t graph_lock = RTE_SPINLOCK_INITIALIZER;
static rte_graph_t graph_id;
int rte_graph_logtype;

#define GRAPH_ID_CHECK(id) ID_CHECK(id, graph_id)

/* Private functions */
struct graph_head *
graph_list_head_get(void)
{
	return &graph_list;
}

void
graph_spinlock_lock(void)
{
	rte_spinlock_lock(&graph_lock);
}

void
graph_spinlock_unlock(void)
{
	rte_spinlock_unlock(&graph_lock);
}

static int
graph_node_add(struct graph *graph, struct node *node)
{
	struct graph_node *graph_node;
	size_t sz;

	/* Skip the duplicate nodes */
	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (strncmp(node->name, graph_node->node->name,
			    RTE_NODE_NAMESIZE) == 0)
			return 0;

	/* Allocate new graph node object */
	sz = sizeof(*graph_node) + node->nb_edges * sizeof(struct node *);
	graph_node = calloc(1, sz);

	if (graph_node == NULL)
		SET_ERR_JMP(ENOMEM, free, "Failed to calloc %s object",
			    node->name);

	/* Initialize the graph node */
	graph_node->node = node;

	/* Add to graph node list */
	STAILQ_INSERT_TAIL(&graph->node_list, graph_node, next);
	return 0;

free:
	free(graph_node);
	return -rte_errno;
}

static struct graph_node *
node_to_graph_node(struct graph *graph, struct node *node)
{
	struct graph_node *graph_node;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (graph_node->node == node)
			return graph_node;

	SET_ERR_JMP(ENODEV, fail, "Found isolated node %s", node->name);
fail:
	return NULL;
}

static int
graph_node_edges_add(struct graph *graph)
{
	struct graph_node *graph_node;
	struct node *adjacency;
	const char *next;
	rte_edge_t i;

	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		for (i = 0; i < graph_node->node->nb_edges; i++) {
			next = graph_node->node->next_nodes[i];
			adjacency = node_from_name(next);
			if (adjacency == NULL)
				SET_ERR_JMP(EINVAL, fail,
					    "Node %s not registered", next);
			if (graph_node_add(graph, adjacency))
				goto fail;
		}
	}
	return 0;
fail:
	return -rte_errno;
}

static int
graph_adjacency_list_update(struct graph *graph)
{
	struct graph_node *graph_node, *tmp;
	struct node *adjacency;
	const char *next;
	rte_edge_t i;

	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		for (i = 0; i < graph_node->node->nb_edges; i++) {
			next = graph_node->node->next_nodes[i];
			adjacency = node_from_name(next);
			if (adjacency == NULL)
				SET_ERR_JMP(EINVAL, fail,
					    "Node %s not registered", next);
			tmp = node_to_graph_node(graph, adjacency);
			if (tmp == NULL)
				goto fail;
			graph_node->adjacency_list[i] = tmp;
		}
	}

	return 0;
fail:
	return -rte_errno;
}

static int
expand_pattern_to_node(struct graph *graph, const char *pattern)
{
	struct node_head *node_head = node_list_head_get();
	bool found = false;
	struct node *node;

	/* Check for pattern match */
	STAILQ_FOREACH(node, node_head, next) {
		if (fnmatch(pattern, node->name, 0) == 0) {
			if (graph_node_add(graph, node))
				goto fail;
			found = true;
		}
	}
	if (found == false)
		SET_ERR_JMP(EFAULT, fail, "Pattern %s node not found", pattern);

	return 0;
fail:
	return -rte_errno;
}

static void
graph_cleanup(struct graph *graph)
{
	struct graph_node *graph_node;

	while (!STAILQ_EMPTY(&graph->node_list)) {
		graph_node = STAILQ_FIRST(&graph->node_list);
		STAILQ_REMOVE_HEAD(&graph->node_list, next);
		free(graph_node);
	}
}

static int
graph_node_init(struct graph *graph)
{
	struct graph_node *graph_node;
	const char *name;
	int rc;

	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		if (graph_node->node->init) {
			name = graph_node->node->name;
			rc = graph_node->node->init(
				graph->graph,
				graph_node_name_to_ptr(graph->graph, name));
			if (rc)
				SET_ERR_JMP(rc, err, "Node %s init() failed",
					    name);
		}
	}

	return 0;
err:
	return -rte_errno;
}

static void
graph_node_fini(struct graph *graph)
{
	struct graph_node *graph_node;

	STAILQ_FOREACH(graph_node, &graph->node_list, next)
		if (graph_node->node->fini)
			graph_node->node->fini(
				graph->graph,
				graph_node_name_to_ptr(graph->graph,
						       graph_node->node->name));
}

rte_graph_t
rte_graph_create(const char *name, struct rte_graph_param *prm)
{
	rte_node_t src_node_count;
	struct graph *graph;
	const char *pattern;
	uint16_t i;

	graph_spinlock_lock();

	/* Check arguments sanity */
	if (prm == NULL)
		SET_ERR_JMP(EINVAL, fail, "Param should not be NULL");

	if (name == NULL)
		SET_ERR_JMP(EINVAL, fail, "Graph name should not be NULL");

	/* Check for existence of duplicate graph */
	STAILQ_FOREACH(graph, &graph_list, next)
		if (strncmp(name, graph->name, RTE_GRAPH_NAMESIZE) == 0)
			SET_ERR_JMP(EEXIST, fail, "Found duplicate graph %s",
				    name);

	/* Create graph object */
	graph = calloc(1, sizeof(*graph));
	if (graph == NULL)
		SET_ERR_JMP(ENOMEM, fail, "Failed to calloc graph object");

	/* Initialize the graph object */
	STAILQ_INIT(&graph->node_list);
	if (rte_strscpy(graph->name, name, RTE_GRAPH_NAMESIZE) < 0)
		SET_ERR_JMP(E2BIG, free, "Too big name=%s", name);

	/* Expand node pattern and add the nodes to the graph */
	for (i = 0; i < prm->nb_node_patterns; i++) {
		pattern = prm->node_patterns[i];
		if (expand_pattern_to_node(graph, pattern))
			goto graph_cleanup;
	}

	/* Go over all the nodes edges and add them to the graph */
	if (graph_node_edges_add(graph))
		goto graph_cleanup;

	/* Update adjacency list of all nodes in the graph */
	if (graph_adjacency_list_update(graph))
		goto graph_cleanup;

	/* Make sure at least a source node present in the graph */
	src_node_count = graph_src_nodes_count(graph);
	if (src_node_count == 0)
		goto graph_cleanup;

	/* Make sure no node is pointing to source node */
	if (graph_node_has_edge_to_src_node(graph))
		goto graph_cleanup;

	/* Don't allow node has loop to self */
	if (graph_node_has_loop_edge(graph))
		goto graph_cleanup;

	/* Do BFS from src nodes on the graph to find isolated nodes */
	if (graph_has_isolated_node(graph))
		goto graph_cleanup;

	/* Initialize graph object */
	graph->socket = prm->socket_id;
	graph->src_node_count = src_node_count;
	graph->node_count = graph_nodes_count(graph);
	graph->id = graph_id;

	/* Allocate the Graph fast path memory and populate the data */
	if (graph_fp_mem_create(graph))
		goto graph_cleanup;

	/* Call init() of the all the nodes in the graph */
	if (graph_node_init(graph))
		goto graph_mem_destroy;

	/* All good, Lets add the graph to the list */
	graph_id++;
	STAILQ_INSERT_TAIL(&graph_list, graph, next);

	graph_spinlock_unlock();
	return graph->id;

graph_mem_destroy:
	graph_fp_mem_destroy(graph);
graph_cleanup:
	graph_cleanup(graph);
free:
	free(graph);
fail:
	graph_spinlock_unlock();
	return RTE_GRAPH_ID_INVALID;
}

int
rte_graph_destroy(rte_graph_t id)
{
	struct graph *graph, *tmp;
	int rc = -ENOENT;

	graph_spinlock_lock();

	graph = STAILQ_FIRST(&graph_list);
	while (graph != NULL) {
		tmp = STAILQ_NEXT(graph, next);
		if (graph->id == id) {
			/* Call fini() of the all the nodes in the graph */
			graph_node_fini(graph);
			/* Destroy graph fast path memory */
			rc = graph_fp_mem_destroy(graph);
			if (rc)
				SET_ERR_JMP(rc, done, "Graph %s destroy failed",
					    graph->name);

			graph_cleanup(graph);
			STAILQ_REMOVE(&graph_list, graph, graph, next);
			free(graph);
			graph_id--;
			goto done;
		}
		graph = tmp;
	}
done:
	graph_spinlock_unlock();
	return rc;
}

void __rte_noinline
__rte_node_stream_alloc(struct rte_graph *graph, struct rte_node *node)
{
	uint16_t size = node->size;

	RTE_VERIFY(size != UINT16_MAX);
	/* Allocate double amount of size to avoid immediate realloc */
	size = RTE_MIN(UINT16_MAX, RTE_MAX(RTE_GRAPH_BURST_SIZE, size * 2));
	node->objs = rte_realloc_socket(node->objs, size * sizeof(void *),
					RTE_CACHE_LINE_SIZE, graph->socket);
	RTE_VERIFY(node->objs);
	node->size = size;
	node->realloc_count++;
}
