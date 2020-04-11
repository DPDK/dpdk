/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_graph.h>

static uint16_t
null(struct rte_graph *graph, struct rte_node *node, void **objs,
	uint16_t nb_objs)
{
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(graph);

	return nb_objs;
}

static struct rte_node_register null_node = {
	.name = "null",
	.process = null,
};

RTE_NODE_REGISTER(null_node);
