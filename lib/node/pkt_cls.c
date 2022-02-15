/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell.
 */

#include <rte_graph.h>
#include <rte_graph_worker.h>

#include "pkt_cls_priv.h"
#include "node_private.h"

/* Next node for each ptype, default is '0' is "pkt_drop" */
static const uint8_t p_nxt[256] __rte_cache_aligned = {
	[RTE_PTYPE_L3_IPV4] = PKT_CLS_NEXT_IP4_LOOKUP,

	[RTE_PTYPE_L3_IPV4_EXT] = PKT_CLS_NEXT_IP4_LOOKUP,

	[RTE_PTYPE_L3_IPV4_EXT_UNKNOWN] = PKT_CLS_NEXT_IP4_LOOKUP,

	[RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L2_ETHER] =
		PKT_CLS_NEXT_IP4_LOOKUP,

	[RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L2_ETHER] =
		PKT_CLS_NEXT_IP4_LOOKUP,

	[RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L2_ETHER] =
		PKT_CLS_NEXT_IP4_LOOKUP,
};

static uint16_t
pkt_cls_node_process(struct rte_graph *graph, struct rte_node *node,
		     void **objs, uint16_t nb_objs)
{
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	uint8_t l0, l1, l2, l3, last_type;
	uint16_t next_index, n_left_from;
	uint16_t held = 0, last_spec = 0;
	struct pkt_cls_node_ctx *ctx;
	void **to_next, **from;
	uint32_t i;

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	for (i = OBJS_PER_CLINE; i < RTE_GRAPH_BURST_SIZE; i += OBJS_PER_CLINE)
		rte_prefetch0(&objs[i]);

#if RTE_GRAPH_BURST_SIZE > 64
	for (i = 0; i < 4 && i < n_left_from; i++)
		rte_prefetch0(pkts[i]);
#endif

	ctx = (struct pkt_cls_node_ctx *)node->ctx;
	last_type = ctx->l2l3_type;
	next_index = p_nxt[last_type];

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node,
					   next_index, nb_objs);
	while (n_left_from >= 4) {
#if RTE_GRAPH_BURST_SIZE > 64
		if (likely(n_left_from > 7)) {
			rte_prefetch0(pkts[4]);
			rte_prefetch0(pkts[5]);
			rte_prefetch0(pkts[6]);
			rte_prefetch0(pkts[7]);
		}
#endif

		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];
		pkts += 4;
		n_left_from -= 4;

		l0 = mbuf0->packet_type &
			(RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);
		l1 = mbuf1->packet_type &
			(RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);
		l2 = mbuf2->packet_type &
			(RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);
		l3 = mbuf3->packet_type &
			(RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);

		/* Check if they are destined to same
		 * next node based on l2l3 packet type.
		 */
		uint8_t fix_spec = (last_type ^ l0) | (last_type ^ l1) |
			(last_type ^ l2) | (last_type ^ l3);

		if (unlikely(fix_spec)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from,
				   last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			/* l0 */
			if (p_nxt[l0] == next_index) {
				to_next[0] = from[0];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    p_nxt[l0], from[0]);
			}

			/* l1 */
			if (p_nxt[l1] == next_index) {
				to_next[0] = from[1];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    p_nxt[l1], from[1]);
			}

			/* l2 */
			if (p_nxt[l2] == next_index) {
				to_next[0] = from[2];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    p_nxt[l2], from[2]);
			}

			/* l3 */
			if (p_nxt[l3] == next_index) {
				to_next[0] = from[3];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node,
						    p_nxt[l3], from[3]);
			}

			/* Update speculated ptype */
			if ((last_type != l3) && (l2 == l3) &&
			    (next_index != p_nxt[l3])) {
				/* Put the current stream for
				 * speculated ltype.
				 */
				rte_node_next_stream_put(graph, node,
							 next_index, held);

				held = 0;

				/* Get next stream for new ltype */
				next_index = p_nxt[l3];
				last_type = l3;
				to_next = rte_node_next_stream_get(graph, node,
								   next_index,
								   nb_objs);
			} else if (next_index == p_nxt[l3]) {
				last_type = l3;
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

		l0 = mbuf0->packet_type &
			(RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);
		if (unlikely((l0 != last_type) &&
			     (p_nxt[l0] != next_index))) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from,
				   last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node,
					    p_nxt[l0], from[0]);
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
	/* Copy things successfully speculated till now */
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);

	ctx->l2l3_type = last_type;
	return nb_objs;
}

/* Packet Classification Node */
struct rte_node_register pkt_cls_node = {
	.process = pkt_cls_node_process,
	.name = "pkt_cls",

	.nb_edges = PKT_CLS_NEXT_MAX,
	.next_nodes = {
		/* Pkt drop node starts at '0' */
		[PKT_CLS_NEXT_PKT_DROP] = "pkt_drop",
		[PKT_CLS_NEXT_IP4_LOOKUP] = "ip4_lookup",
	},
};
RTE_NODE_REGISTER(pkt_cls_node);
