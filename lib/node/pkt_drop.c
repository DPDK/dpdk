/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_graph.h>
#include <rte_mbuf.h>

static uint16_t
pkt_drop_process(struct rte_graph *graph, struct rte_node *node, void **objs,
		 uint16_t nb_objs)
{
	RTE_SET_USED(node);
	RTE_SET_USED(graph);

	rte_pktmbuf_free_bulk((struct rte_mbuf **)objs, nb_objs);

	return nb_objs;
}

static struct rte_node_register pkt_drop_node = {
	.process = pkt_drop_process,
	.name = "pkt_drop",
};

RTE_NODE_REGISTER(pkt_drop_node);
