/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "graph_private.h"

static rte_spinlock_t graph_lock = RTE_SPINLOCK_INITIALIZER;
int rte_graph_logtype;

void
graph_spinlock_lock(void)
{
	rte_spinlock_lock(&graph_lock);
}

void
graph_spinlock_unlock(void)
{
	rte_spinlock_unlock(&graph_lock);
}

void __rte_noinline
__rte_node_stream_alloc(struct rte_graph *graph, struct rte_node *node)
{
	uint16_t size = node->size;

	RTE_VERIFY(size != UINT16_MAX);
	/* Allocate double amount of size to avoid immediate realloc */
	size = RTE_MIN(UINT16_MAX, RTE_MAX(RTE_GRAPH_BURST_SIZE, size * 2));
	node->objs = rte_realloc_socket(node->objs, size * sizeof(void *),
					RTE_CACHE_LINE_SIZE, graph->socket);
	RTE_VERIFY(node->objs);
	node->size = size;
	node->realloc_count++;
}
