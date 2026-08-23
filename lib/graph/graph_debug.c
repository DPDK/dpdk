/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */


#include "graph_private.h"

#include <eal_export.h>

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
	if (n->parent_id == RTE_NODE_ID_INVALID)
		fprintf(f, "  parent_id=RTE_NODE_ID_INVALID\n");
	else
		fprintf(f, "  parent_id=%" PRIu32 "\n", n->parent_id);
	fprintf(f, "  init=%p\n", n->init);
	fprintf(f, "  fini=%p\n", n->fini);
	fprintf(f, "  xstats=%p\n", n->xstats);
	fprintf(f, "  next node addr=%p\n", STAILQ_NEXT(n, next));
	if (STAILQ_NEXT(n, next))
		fprintf(f, "  next node name=%s\n", STAILQ_NEXT(n, next)->name);
	fprintf(f, "  nb_edges=%d\n", n->nb_edges);
	for (i = 0; i < n->nb_edges; i++)
		fprintf(f, "     edge[%d] <%s>\n", i, n->next_nodes[i]);
}

RTE_EXPORT_SYMBOL(rte_graph_obj_dump)
void
rte_graph_obj_dump(FILE *f, struct rte_graph *g, bool all)
{
	rte_node_t count;
	rte_graph_off_t off;
	struct rte_node *n;
	rte_edge_t i;
	uint64_t all_total_cycles = 0;

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
		all_total_cycles += n->total_cycles;
	}

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
		if (rte_graph_has_stats_feature())
			fprintf(f, "       total_cycles=%" PRIu64 " (%.1f%% of all nodes), avg cycles/call=%.1f\n",
				n->total_cycles,
				all_total_cycles == 0 ? 0.0 :
				(double)n->total_cycles / (double)all_total_cycles * 100.0,
				n->total_calls == 0 ? 0.0 :
				(double)n->total_cycles / (double)n->total_calls);

#ifdef RTE_GRAPH_PROFILE
		static const uint16_t sample_sizes[] = {
				0, 1, 2, RTE_GRAPH_PROFILE_BURST_SMALL,
				RTE_GRAPH_PROFILE_BURST_MEDIUM, RTE_GRAPH_PROFILE_BURST_LARGE};
		static_assert(RTE_DIM(sample_sizes) == 2U + RTE_DIM(n->usage_stats),
				"usage_stats array size mismatch");
		for (unsigned int idx = 0; idx < RTE_DIM(sample_sizes); idx++) {
			uint64_t calls;
			uint64_t cycles;
			double objs_per_call;
			if (idx < 2) {
				calls = n->usage_stats_01[idx].calls;
				cycles = n->usage_stats_01[idx].cycles;
				objs_per_call = (double)idx;
				fprintf(f, "       for %u objs/call\n",
					idx);
			} else {
				calls = n->usage_stats[idx - 2].calls;
				cycles = n->usage_stats[idx - 2].cycles;
				objs_per_call = calls == 0 ? 0.0 :
						(double)n->usage_stats[idx - 2].objs /
						(double)calls;
				if (idx < RTE_DIM(sample_sizes) - 1)
					fprintf(f, "       for [%u;%u[ objs/call",
						sample_sizes[idx], sample_sizes[idx + 1]);
				else
					fprintf(f, "       for %u+ objs/call",
						sample_sizes[idx]);
				if (calls != 0)
					fprintf(f, ", avg %.1f objs/call\n",
						objs_per_call);
				fprintf(f, "\n");
			}
			fprintf(f, "         calls=%" PRIu64,
				calls);
			if (calls != 0) {
				fprintf(f, " (%.1f%% of this node)\n",
					n->total_calls == 0 ? 0.0 :
					(double)calls / (double)n->total_calls * 100.0);
				fprintf(f, "         cycles=%" PRIu64 " (%.1f%% of this node), avg cycles/call=%.1f",
					cycles,
					n->total_cycles == 0 ? 0.0 :
					(double)cycles / (double)n->total_cycles * 100.0,
					(double)cycles / (double)calls);
				if (objs_per_call != 0.0)
					fprintf(f, ", avg cycles/obj=%.1f",
						(double)cycles / (double)calls / objs_per_call);
			}
			fprintf(f, "\n");
		}
#endif /* RTE_GRAPH_PROFILE */

		for (i = 0; i < n->nb_edges; i++)
			fprintf(f, "          edge[%d] <%s>\n", i,
				n->nodes[i]->name);
	}
}
