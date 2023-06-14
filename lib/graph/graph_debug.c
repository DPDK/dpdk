/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */


#include "graph_private.h"

void
graph_dump(FILE *f, struct graph *g)
{
	struct graph_node *graph_node;
	rte_edge_t i = 0;

	fprintf(f, "graph <%s>\n", g->name);
	fprintf(f, "  id=%" PRIu32 "\n", g->id);
	fprintf(f, "  cir_start=%" PRIu32 "\n", g->cir_start);
	fprintf(f, "  cir_mask=%" PRIu32 "\n", g->cir_mask);
	fprintf(f, "  addr=%p\n", g);
	fprintf(f, "  graph=%p\n", g->graph);
	fprintf(f, "  mem_sz=%zu\n", g->mem_sz);
	fprintf(f, "  node_count=%" PRIu32 "\n", g->node_count);
	fprintf(f, "  src_node_count=%" PRIu32 "\n", g->src_node_count);

	STAILQ_FOREACH(graph_node, &g->node_list, next)
		fprintf(f, "     node[%d] <%s>\n", i++, graph_node->node->name);
}

void
node_dump(FILE *f, struct node *n)
{
	rte_edge_t i;

	fprintf(f, "node <%s>\n", n->name);
	fprintf(f, "  id=%" PRIu32 "\n", n->id);
	fprintf(f, "  flags=0x%" PRIx64 "\n", n->flags);
	fprintf(f, "  addr=%p\n", n);
	fprintf(f, "  process=%p\n", n->process);
	fprintf(f, "  nb_edges=%d\n", n->nb_edges);

	for (i = 0; i < n->nb_edges; i++)
		fprintf(f, "     edge[%d] <%s>\n", i, n->next_nodes[i]);
}

void
rte_graph_obj_dump(FILE *f, struct rte_graph *g, bool all)
{
	rte_node_t count;
	rte_graph_off_t off;
	struct rte_node *n;
	rte_edge_t i;

	fprintf(f, "graph <%s> @ %p\n", g->name, g);
	fprintf(f, "  id=%" PRIu32 "\n", g->id);
	fprintf(f, "  head=%" PRId32 "\n", (int32_t)g->head);
	fprintf(f, "  tail=%" PRId32 "\n", (int32_t)g->tail);
	fprintf(f, "  cir_mask=0x%" PRIx32 "\n", g->cir_mask);
	fprintf(f, "  nb_nodes=%" PRId32 "\n", g->nb_nodes);
	fprintf(f, "  socket=%d\n", g->socket);
	fprintf(f, "  fence=0x%" PRIx64 "\n", g->fence);
	fprintf(f, "  nodes_start=0x%" PRIx32 "\n", g->nodes_start);
	fprintf(f, "  cir_start=%p\n", g->cir_start);

	rte_graph_foreach_node(count, off, g, n) {
		if (!all && n->idx == 0)
			continue;
		fprintf(f, "     node[%d] <%s>\n", count, n->name);
		fprintf(f, "       fence=0x%" PRIx64 "\n", n->fence);
		fprintf(f, "       objs=%p\n", n->objs);
		fprintf(f, "       process=%p\n", n->process);
		fprintf(f, "       id=0x%" PRIx32 "\n", n->id);
		fprintf(f, "       offset=0x%" PRIx32 "\n", n->off);
		fprintf(f, "       nb_edges=%" PRId32 "\n", n->nb_edges);
		fprintf(f, "       realloc_count=%d\n", n->realloc_count);
		fprintf(f, "       size=%d\n", n->size);
		fprintf(f, "       idx=%d\n", n->idx);
		fprintf(f, "       total_objs=%" PRId64 "\n", n->total_objs);
		if (rte_graph_worker_model_get(g) == RTE_GRAPH_MODEL_MCORE_DISPATCH) {
			fprintf(f, "       total_sched_objs=%" PRId64 "\n",
				n->dispatch.total_sched_objs);
			fprintf(f, "       total_sched_fail=%" PRId64 "\n",
				n->dispatch.total_sched_fail);
		}
		fprintf(f, "       total_calls=%" PRId64 "\n", n->total_calls);
		for (i = 0; i < n->nb_edges; i++)
			fprintf(f, "          edge[%d] <%s>\n", i,
				n->nodes[i]->name);
	}
}
