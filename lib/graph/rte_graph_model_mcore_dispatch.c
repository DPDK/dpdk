/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#include "graph_private.h"
#include "rte_graph_model_mcore_dispatch.h"

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
