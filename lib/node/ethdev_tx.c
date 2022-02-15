/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#include "ethdev_tx_priv.h"

static struct ethdev_tx_node_main ethdev_tx_main;

static uint16_t
ethdev_tx_node_process(struct rte_graph *graph, struct rte_node *node,
		       void **objs, uint16_t nb_objs)
{
	ethdev_tx_node_ctx_t *ctx = (ethdev_tx_node_ctx_t *)node->ctx;
	uint16_t port, queue;
	uint16_t count;

	/* Get Tx port id */
	port = ctx->port;
	queue = ctx->queue;

	count = rte_eth_tx_burst(port, queue, (struct rte_mbuf **)objs,
				 nb_objs);

	/* Redirect unsent pkts to drop node */
	if (count != nb_objs) {
		rte_node_enqueue(graph, node, ETHDEV_TX_NEXT_PKT_DROP,
				 &objs[count], nb_objs - count);
	}

	return count;
}

static int
ethdev_tx_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	ethdev_tx_node_ctx_t *ctx = (ethdev_tx_node_ctx_t *)node->ctx;
	uint64_t port_id = RTE_MAX_ETHPORTS;
	int i;

	/* Find our port id */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ethdev_tx_main.nodes[i] == node->id) {
			port_id = i;
			break;
		}
	}
	RTE_VERIFY(port_id < RTE_MAX_ETHPORTS);

	/* Update port and queue */
	ctx->port = port_id;
	ctx->queue = graph->id;

	return 0;
}

struct ethdev_tx_node_main *
ethdev_tx_node_data_get(void)
{
	return &ethdev_tx_main;
}

static struct rte_node_register ethdev_tx_node_base = {
	.process = ethdev_tx_node_process,
	.name = "ethdev_tx",

	.init = ethdev_tx_node_init,

	.nb_edges = ETHDEV_TX_NEXT_MAX,
	.next_nodes = {
		[ETHDEV_TX_NEXT_PKT_DROP] = "pkt_drop",
	},
};

struct rte_node_register *
ethdev_tx_node_get(void)
{
	return &ethdev_tx_node_base;
}

RTE_NODE_REGISTER(ethdev_tx_node_base);
