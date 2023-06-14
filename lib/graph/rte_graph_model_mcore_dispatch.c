/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#include "graph_private.h"
#include "rte_graph_model_mcore_dispatch.h"

int
graph_sched_wq_create(struct graph *_graph, struct graph *_parent_graph,
		       struct rte_graph_param *prm)
{
	struct rte_graph *parent_graph = _parent_graph->graph;
	struct rte_graph *graph = _graph->graph;
	unsigned int wq_size;
	unsigned int flags = RING_F_SC_DEQ;

	wq_size = RTE_GRAPH_SCHED_WQ_SIZE(graph->nb_nodes);
	wq_size = rte_align32pow2(wq_size + 1);

	if (prm->dispatch.wq_size_max > 0)
		wq_size = wq_size <= (prm->dispatch.wq_size_max) ? wq_size :
			prm->dispatch.wq_size_max;

	if (!rte_is_power_of_2(wq_size))
		flags |= RING_F_EXACT_SZ;

	graph->dispatch.wq = rte_ring_create(graph->name, wq_size, graph->socket,
					     flags);
	if (graph->dispatch.wq == NULL)
		SET_ERR_JMP(EIO, fail, "Failed to allocate graph WQ");

	if (prm->dispatch.mp_capacity > 0)
		wq_size = (wq_size <= prm->dispatch.mp_capacity) ? wq_size :
			prm->dispatch.mp_capacity;

	graph->dispatch.mp = rte_mempool_create(graph->name, wq_size,
						sizeof(struct graph_mcore_dispatch_wq_node),
						0, 0, NULL, NULL, NULL, NULL,
						graph->socket, MEMPOOL_F_SP_PUT);
	if (graph->dispatch.mp == NULL)
		SET_ERR_JMP(EIO, fail_mp,
			    "Failed to allocate graph WQ schedule entry");

	graph->dispatch.lcore_id = _graph->lcore_id;

	if (parent_graph->dispatch.rq == NULL) {
		parent_graph->dispatch.rq = &parent_graph->dispatch.rq_head;
		SLIST_INIT(parent_graph->dispatch.rq);
	}

	graph->dispatch.rq = parent_graph->dispatch.rq;
	SLIST_INSERT_HEAD(graph->dispatch.rq, graph, next);

	return 0;

fail_mp:
	rte_ring_free(graph->dispatch.wq);
	graph->dispatch.wq = NULL;
fail:
	return -rte_errno;
}

void
graph_sched_wq_destroy(struct graph *_graph)
{
	struct rte_graph *graph = _graph->graph;

	if (graph == NULL)
		return;

	rte_ring_free(graph->dispatch.wq);
	graph->dispatch.wq = NULL;

	rte_mempool_free(graph->dispatch.mp);
	graph->dispatch.mp = NULL;
}

static __rte_always_inline bool
__graph_sched_node_enqueue(struct rte_node *node, struct rte_graph *graph)
{
	struct graph_mcore_dispatch_wq_node *wq_node;
	uint16_t off = 0;
	uint16_t size;

submit_again:
	if (rte_mempool_get(graph->dispatch.mp, (void **)&wq_node) < 0)
		goto fallback;

	size = RTE_MIN(node->idx, RTE_DIM(wq_node->objs));
	wq_node->node_off = node->off;
	wq_node->nb_objs = size;
	rte_memcpy(wq_node->objs, &node->objs[off], size * sizeof(void *));

	while (rte_ring_mp_enqueue_bulk_elem(graph->dispatch.wq, (void *)&wq_node,
					     sizeof(wq_node), 1, NULL) == 0)
		rte_pause();

	off += size;
	node->dispatch.total_sched_objs += size;
	node->idx -= size;
	if (node->idx > 0)
		goto submit_again;

	return true;

fallback:
	if (off != 0)
		memmove(&node->objs[0], &node->objs[off],
			node->idx * sizeof(void *));

	node->dispatch.total_sched_fail += node->idx;

	return false;
}

bool __rte_noinline
__rte_graph_mcore_dispatch_sched_node_enqueue(struct rte_node *node,
					      struct rte_graph_rq_head *rq)
{
	const unsigned int lcore_id = node->dispatch.lcore_id;
	struct rte_graph *graph;

	SLIST_FOREACH(graph, rq, next)
		if (graph->dispatch.lcore_id == lcore_id)
			break;

	return graph != NULL ? __graph_sched_node_enqueue(node, graph) : false;
}

void
__rte_graph_mcore_dispatch_sched_wq_process(struct rte_graph *graph)
{
#define WQ_SZ 32
	struct graph_mcore_dispatch_wq_node *wq_node;
	struct rte_mempool *mp = graph->dispatch.mp;
	struct rte_ring *wq = graph->dispatch.wq;
	uint16_t idx, free_space;
	struct rte_node *node;
	unsigned int i, n;
	struct graph_mcore_dispatch_wq_node *wq_nodes[WQ_SZ];

	n = rte_ring_sc_dequeue_burst_elem(wq, wq_nodes, sizeof(wq_nodes[0]),
					   RTE_DIM(wq_nodes), NULL);
	if (n == 0)
		return;

	for (i = 0; i < n; i++) {
		wq_node = wq_nodes[i];
		node = RTE_PTR_ADD(graph, wq_node->node_off);
		RTE_ASSERT(node->fence == RTE_GRAPH_FENCE);
		idx = node->idx;
		free_space = node->size - idx;

		if (unlikely(free_space < wq_node->nb_objs))
			__rte_node_stream_alloc_size(graph, node, node->size + wq_node->nb_objs);

		memmove(&node->objs[idx], wq_node->objs, wq_node->nb_objs * sizeof(void *));
		node->idx = idx + wq_node->nb_objs;

		__rte_node_process(graph, node);

		wq_node->nb_objs = 0;
		node->idx = 0;
	}

	rte_mempool_put_bulk(mp, (void **)wq_nodes, n);
}

int
rte_graph_model_mcore_dispatch_node_lcore_affinity_set(const char *name, unsigned int lcore_id)
{
	struct node *node;
	int ret = -EINVAL;

	if (lcore_id >= RTE_MAX_LCORE)
		return ret;

	graph_spinlock_lock();

	STAILQ_FOREACH(node, node_list_head_get(), next) {
		if (strncmp(node->name, name, RTE_NODE_NAMESIZE) == 0) {
			node->lcore_id = lcore_id;
			ret = 0;
			break;
		}
	}

	graph_spinlock_unlock();

	return ret;
}
