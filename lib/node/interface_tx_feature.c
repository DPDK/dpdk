/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph_feature_arc_worker.h>
#include <rte_malloc.h>

#include "rte_node_ip4_api.h"
#include "node_private.h"
#include "interface_tx_feature_priv.h"

#define IF_TX_FEATURE_LAST_NEXT_INDEX(ctx) \
	(((struct if_tx_feature_node_ctx *)ctx)->last_index)
/*
 * @internal array for mapping port to next node index
 */
struct if_tx_feature_node_main  {
	uint16_t next_index[RTE_MAX_ETHPORTS];
};

struct if_tx_feature_node_ctx {
	uint16_t last_index;
};

static struct if_tx_feature_node_main *if_tx_feature_nm;

int
if_tx_feature_node_set_next(uint16_t port_id, uint16_t next_index)
{
	if (if_tx_feature_nm == NULL) {
		if_tx_feature_nm = rte_zmalloc(
			"if_tx_feature_nm", sizeof(struct if_tx_feature_node_main),
			RTE_CACHE_LINE_SIZE);
		if (if_tx_feature_nm == NULL)
			return -ENOMEM;
	}
	if_tx_feature_nm->next_index[port_id] = next_index;

	return 0;
}

static int
if_tx_feature_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	RTE_SET_USED(graph);

	/* pkt_drop */
	IF_TX_FEATURE_LAST_NEXT_INDEX(node->ctx) = 0;

	return 0;
}

static uint16_t
if_tx_feature_node_process(struct rte_graph *graph, struct rte_node *node,
			   void **objs, uint16_t nb_objs)
{
	uint16_t held = 0, next0 = 0, next1 = 0, next2 = 0, next3 = 0;
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	uint16_t last_spec = 0, fix_spec = 0;
	void **to_next, **from;
	rte_edge_t next_index;
	uint16_t n_left_from;

	/* Speculative next */
	next_index = IF_TX_FEATURE_LAST_NEXT_INDEX(node->ctx);

	from = objs;
	n_left_from = nb_objs;
	pkts = (struct rte_mbuf **)objs;

	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	while (n_left_from > 4) {
		if (likely(n_left_from > 7)) {
			/* Prefetch next mbuf */
			rte_prefetch0(objs[4]);
			rte_prefetch0(objs[5]);
			rte_prefetch0(objs[6]);
			rte_prefetch0(objs[7]);
		}
		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];
		pkts += 4;
		n_left_from -= 4;

		/* port-tx node starts from next edge 1*/
		next0 = if_tx_feature_nm->next_index[mbuf0->port];
		next1 = if_tx_feature_nm->next_index[mbuf1->port];
		next2 = if_tx_feature_nm->next_index[mbuf2->port];
		next3 = if_tx_feature_nm->next_index[mbuf3->port];

		fix_spec = (next_index ^ next0) | (next_index ^ next1) |
			(next_index ^ next2) | (next_index ^ next3);

		if (unlikely(fix_spec)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from,
				   last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			if (next0 == next_index) {
				to_next[0] = from[0];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    next0, from[0]);
			}

			if (next1 == next_index) {
				to_next[0] = from[1];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    next1, from[1]);
			}

			if (next2 == next_index) {
				to_next[0] = from[2];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    next2, from[2]);
			}

			if (next3 == next_index) {
				to_next[0] = from[3];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    next3, from[3]);
			}
			from += 4;
		} else {
			last_spec += 4;
		}
	}

	while (n_left_from > 0) {
		mbuf0 = pkts[0];

		pkts += 1;
		n_left_from -= 1;

		next0 = if_tx_feature_nm->next_index[mbuf0->port];
		if (unlikely(next0 != next_index)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from,
				   last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node,
					    next0, from[0]);
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

	IF_TX_FEATURE_LAST_NEXT_INDEX(node->ctx) = next0;

	return nb_objs;
}

static struct rte_node_register if_tx_feature_node = {
	.process = if_tx_feature_node_process,
	.init = if_tx_feature_node_init,
	.name = "interface_tx",
	.nb_edges = 1,
	.next_nodes = {
		[0] = "pkt_drop",
	},
};

struct rte_node_register *
if_tx_feature_node_get(void)
{
	return &if_tx_feature_node;
}

RTE_NODE_REGISTER(if_tx_feature_node);

/* if_tx feature node */
struct rte_graph_feature_register if_tx_feature = {
	.feature_name = RTE_IP4_OUTPUT_END_FEATURE_NAME,
	.arc_name = RTE_IP4_OUTPUT_FEATURE_ARC_NAME,
	.feature_process_fn = if_tx_feature_node_process,
	.feature_node = &if_tx_feature_node,
};
