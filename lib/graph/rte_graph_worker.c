/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#include <eal_export.h>
#include "rte_graph_worker_common.h"
#include "graph_private.h"

RTE_EXPORT_SYMBOL(rte_graph_model_is_valid)
bool
rte_graph_model_is_valid(uint8_t model)
{
	if (model > RTE_GRAPH_MODEL_MCORE_DISPATCH)
		return false;

	return true;
}

RTE_EXPORT_SYMBOL(rte_graph_worker_model_set)
int
rte_graph_worker_model_set(uint8_t model)
{
	struct graph_head *graph_head = graph_list_head_get();
	struct graph *graph;

	if (!rte_graph_model_is_valid(model))
		return -EINVAL;

	STAILQ_FOREACH(graph, graph_head, next)
			graph->graph->model = model;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_graph_worker_model_get)
uint8_t
rte_graph_worker_model_get(struct rte_graph *graph)
{
	if (!rte_graph_model_is_valid(graph->model))
		return -EINVAL;

	return graph->model;
}
