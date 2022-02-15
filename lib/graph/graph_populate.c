/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */


#include <rte_common.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memzone.h>

#include "graph_private.h"

static size_t
graph_fp_mem_calc_size(struct graph *graph)
{
	struct graph_node *graph_node;
	rte_node_t val;
	size_t sz;

	/* Graph header */
	sz = sizeof(struct rte_graph);
	/* Source nodes list */
	sz += sizeof(rte_graph_off_t) * graph->src_node_count;
	/* Circular buffer for pending streams of size number of nodes */
	val = rte_align32pow2(graph->node_count * sizeof(rte_graph_off_t));
	sz = RTE_ALIGN(sz, val);
	graph->cir_start = sz;
	graph->cir_mask = rte_align32pow2(graph->node_count) - 1;
	sz += val;
	/* Fence */
	sz += sizeof(RTE_GRAPH_FENCE);
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);
	graph->nodes_start = sz;
	/* For 0..N node objects with fence */
	STAILQ_FOREACH(graph_node, &graph->node_list, next) {
		sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);
		sz += sizeof(struct rte_node);
		/* Pointer to next nodes(edges) */
		sz += sizeof(struct rte_node *) * graph_node->node->nb_edges;
	}

	graph->mem_sz = sz;
	return sz;
}

static void
graph_header_popluate(struct graph *_graph)
{
	struct rte_graph *graph = _graph->graph;

	graph->tail = 0;
	graph->head = (int32_t)-_graph->src_node_count;
	graph->cir_mask = _graph->cir_mask;
	graph->nb_nodes = _graph->node_count;
	graph->cir_start = RTE_PTR_ADD(graph, _graph->cir_start);
	graph->nodes_start = _graph->nodes_start;
	graph->socket = _graph->socket;
	graph->id = _graph->id;
	memcpy(graph->name, _graph->name, RTE_GRAPH_NAMESIZE);
	graph->fence = RTE_GRAPH_FENCE;
}

static void
graph_nodes_populate(struct graph *_graph)
{
	rte_graph_off_t off = _graph->nodes_start;
	struct rte_graph *graph = _graph->graph;
	struct graph_node *graph_node;
	rte_edge_t count, nb_edges;
	const char *parent;
	rte_node_t pid;

	STAILQ_FOREACH(graph_node, &_graph->node_list, next) {
		struct rte_node *node = RTE_PTR_ADD(graph, off);
		memset(node, 0, sizeof(*node));
		node->fence = RTE_GRAPH_FENCE;
		node->off = off;
		node->process = graph_node->node->process;
		memcpy(node->name, graph_node->node->name, RTE_GRAPH_NAMESIZE);
		pid = graph_node->node->parent_id;
		if (pid != RTE_NODE_ID_INVALID) { /* Cloned node */
			parent = rte_node_id_to_name(pid);
			memcpy(node->parent, parent, RTE_GRAPH_NAMESIZE);
		}
		node->id = graph_node->node->id;
		node->parent_id = pid;
		nb_edges = graph_node->node->nb_edges;
		node->nb_edges = nb_edges;
		off += sizeof(struct rte_node);
		/* Copy the name in first pass to replace with rte_node* later*/
		for (count = 0; count < nb_edges; count++)
			node->nodes[count] = (struct rte_node *)&graph_node
						     ->adjacency_list[count]
						     ->node->name[0];

		off += sizeof(struct rte_node *) * nb_edges;
		off = RTE_ALIGN(off, RTE_CACHE_LINE_SIZE);
		node->next = off;
		__rte_node_stream_alloc(graph, node);
	}
}

struct rte_node *
graph_node_id_to_ptr(const struct rte_graph *graph, rte_node_t id)
{
	rte_node_t count;
	rte_graph_off_t off;
	struct rte_node *node;

	rte_graph_foreach_node(count, off, graph, node)
		if (unlikely(node->id == id))
			return node;

	return NULL;
}

struct rte_node *
graph_node_name_to_ptr(const struct rte_graph *graph, const char *name)
{
	rte_node_t count;
	rte_graph_off_t off;
	struct rte_node *node;

	rte_graph_foreach_node(count, off, graph, node)
		if (strncmp(name, node->name, RTE_NODE_NAMESIZE) == 0)
			return node;

	return NULL;
}

static int
graph_node_nexts_populate(struct graph *_graph)
{
	rte_node_t count, val;
	rte_graph_off_t off;
	struct rte_node *node;
	const struct rte_graph *graph = _graph->graph;
	const char *name;

	rte_graph_foreach_node(count, off, graph, node) {
		for (val = 0; val < node->nb_edges; val++) {
			name = (const char *)node->nodes[val];
			node->nodes[val] = graph_node_name_to_ptr(graph, name);
			if (node->nodes[val] == NULL)
				SET_ERR_JMP(EINVAL, fail, "%s not found", name);
		}
	}

	return 0;
fail:
	return -rte_errno;
}

static int
graph_src_nodes_populate(struct graph *_graph)
{
	struct rte_graph *graph = _graph->graph;
	struct graph_node *graph_node;
	struct rte_node *node;
	int32_t head = -1;
	const char *name;

	STAILQ_FOREACH(graph_node, &_graph->node_list, next) {
		if (graph_node->node->flags & RTE_NODE_SOURCE_F) {
			name = graph_node->node->name;
			node = graph_node_name_to_ptr(graph, name);
			if (node == NULL)
				SET_ERR_JMP(EINVAL, fail, "%s not found", name);

			__rte_node_stream_alloc(graph, node);
			graph->cir_start[head--] = node->off;
		}
	}

	return 0;
fail:
	return -rte_errno;
}

static int
graph_fp_mem_populate(struct graph *graph)
{
	int rc;

	graph_header_popluate(graph);
	graph_nodes_populate(graph);
	rc = graph_node_nexts_populate(graph);
	rc |= graph_src_nodes_populate(graph);

	return rc;
}

int
graph_fp_mem_create(struct graph *graph)
{
	const struct rte_memzone *mz;
	size_t sz;

	sz = graph_fp_mem_calc_size(graph);
	mz = rte_memzone_reserve(graph->name, sz, graph->socket, 0);
	if (mz == NULL)
		SET_ERR_JMP(ENOMEM, fail, "Memzone %s reserve failed",
			    graph->name);

	graph->graph = mz->addr;
	graph->mz = mz;

	return graph_fp_mem_populate(graph);
fail:
	return -rte_errno;
}

static void
graph_nodes_mem_destroy(struct rte_graph *graph)
{
	rte_node_t count;
	rte_graph_off_t off;
	struct rte_node *node;

	if (graph == NULL)
		return;

	rte_graph_foreach_node(count, off, graph, node)
		rte_free(node->objs);
}

int
graph_fp_mem_destroy(struct graph *graph)
{
	graph_nodes_mem_destroy(graph->graph);
	return rte_memzone_free(graph->mz);
}
