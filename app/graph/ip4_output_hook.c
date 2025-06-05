/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_graph_feature_arc_worker.h>

#include "rte_node_ip4_api.h"

#define IP4_OUTPUT_HOOK_FEATURE1_NAME "app_ip4_out_feat1"
#define IP4_OUTPUT_HOOK_FEATURE2_NAME "app_ip4_out_feat2"

struct output_hook_node_ctx {
	rte_graph_feature_arc_t out_arc;
	uint16_t last_index;
};

enum {
	OUTPUT_HOOK_PKT_DROP = 0,
	OUTPUT_HOOK_PKT_CLS,
	OUTPUT_HOOK_MAX_NB_EDGES,
};

#define OUTPUT_HOOK_FEATURE_ARC(ctx) \
	(((struct output_hook_node_ctx *)ctx)->out_arc)

#define OUTPUT_HOOK_LAST_NEXT_INDEX(ctx) \
	(((struct output_hook_node_ctx *)ctx)->last_index)

static int
__app_graph_ip4_output_hook_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	rte_graph_feature_arc_t feature;

	RTE_SET_USED(graph);

	rte_graph_feature_arc_lookup_by_name(RTE_IP4_OUTPUT_FEATURE_ARC_NAME, &feature);

	OUTPUT_HOOK_FEATURE_ARC(node->ctx) = feature;
	/* pkt_drop */
	OUTPUT_HOOK_LAST_NEXT_INDEX(node->ctx) = 0;

	return 0;
}

static __rte_always_inline uint16_t
__app_graph_ip4_output_hook_node_process(struct rte_graph *graph, struct rte_node *node,
					 void **objs, uint16_t nb_objs)
{
	struct rte_graph_feature_arc *arc =
		rte_graph_feature_arc_get(OUTPUT_HOOK_FEATURE_ARC(node->ctx));
	struct rte_graph_feature_arc_mbuf_dynfields *mbfields = NULL;
	uint16_t next = OUTPUT_HOOK_PKT_DROP;
	void **to_next, **from;
	uint16_t last_spec = 0;
	rte_edge_t next_index;
	struct rte_mbuf *mbuf;
	uint16_t held = 0;
	int i;

	/* Speculative next */
	next_index = OUTPUT_HOOK_LAST_NEXT_INDEX(node->ctx);

	from = objs;
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	for (i = 0; i < nb_objs; i++) {

		mbuf = (struct rte_mbuf *)objs[i];

		/* Send mbuf to next enabled feature */
		mbfields = rte_graph_feature_arc_mbuf_dynfields_get(mbuf, arc->mbuf_dyn_offset);
		rte_graph_feature_data_next_feature_get(arc, &mbfields->feature_data, &next);

		if (unlikely(next_index != next)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node, next, from[0]);
			from += 1;
		} else {
			last_spec += 1;
		}
	}
	/* !!! Home run !!! */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}
	held += last_spec;
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);
	OUTPUT_HOOK_LAST_NEXT_INDEX(node->ctx) = next;

	return nb_objs;
}

static int
app_graph_ip4_output_hook_node1_init(const struct rte_graph *graph, struct rte_node *node)
{
	return __app_graph_ip4_output_hook_node_init(graph, node);
}

static __rte_always_inline uint16_t
app_graph_ip4_output_hook_node1_process(struct rte_graph *graph, struct rte_node *node,
					void **objs, uint16_t nb_objs)
{
	return __app_graph_ip4_output_hook_node_process(graph, node, objs, nb_objs);
}

static struct rte_node_register app_graph_ip4_output_hook_node1 = {
	.process = app_graph_ip4_output_hook_node1_process,
	.init = app_graph_ip4_output_hook_node1_init,
	.name = "app_ip4_output_node1",
	.nb_edges = OUTPUT_HOOK_MAX_NB_EDGES,
	.next_nodes = {
		[OUTPUT_HOOK_PKT_DROP] = "pkt_drop",
		[OUTPUT_HOOK_PKT_CLS] = "pkt_cls",
	},
};

RTE_NODE_REGISTER(app_graph_ip4_output_hook_node1);

static int
app_graph_ip4_output_hook_node2_init(const struct rte_graph *graph, struct rte_node *node)
{
	return __app_graph_ip4_output_hook_node_init(graph, node);
}

static __rte_always_inline uint16_t
app_graph_ip4_output_hook_node2_process(struct rte_graph *graph, struct rte_node *node,
					void **objs, uint16_t nb_objs)
{
	return __app_graph_ip4_output_hook_node_process(graph, node, objs, nb_objs);
}

static struct rte_node_register app_graph_ip4_output_hook_node2 = {
	.process = app_graph_ip4_output_hook_node2_process,
	.init = app_graph_ip4_output_hook_node2_init,
	.name = "app_ip4_output_node2",
	.nb_edges = 1,
	.next_nodes = {
		[OUTPUT_HOOK_PKT_DROP] = "pkt_drop",
	},
};

/* Override max_index for arc create */
static uint16_t override_arc_index(void)
{
	return 64;
}

RTE_NODE_REGISTER(app_graph_ip4_output_hook_node2);

/* if feature1 */
struct rte_graph_feature_register app_graph_ip4_output_hook_feature1 = {
	.feature_name = IP4_OUTPUT_HOOK_FEATURE1_NAME,
	.arc_name = RTE_IP4_OUTPUT_FEATURE_ARC_NAME,
	/* Same as regular function */
	.feature_process_fn = app_graph_ip4_output_hook_node1_process,
	.feature_node = &app_graph_ip4_output_hook_node1,
	.runs_before =  IP4_OUTPUT_HOOK_FEATURE2_NAME,
	.override_index_cb = override_arc_index,
};

/* if feature2 (same as f1) */
struct rte_graph_feature_register app_graph_ip4_output_hook_feature2 = {
	.feature_name = IP4_OUTPUT_HOOK_FEATURE2_NAME,
	.arc_name = RTE_IP4_OUTPUT_FEATURE_ARC_NAME,
	/* Same as regular function */
	.feature_node = &app_graph_ip4_output_hook_node2,
	.feature_process_fn = app_graph_ip4_output_hook_node2_process,
};

RTE_GRAPH_FEATURE_REGISTER(app_graph_ip4_output_hook_feature1);
RTE_GRAPH_FEATURE_REGISTER(app_graph_ip4_output_hook_feature2);
