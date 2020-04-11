/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "graph_private.h"

static struct node_head node_list = STAILQ_HEAD_INITIALIZER(node_list);
static rte_node_t node_id;

/* Private functions */
struct node_head *
node_list_head_get(void)
{
	return &node_list;
}

struct node *
node_from_name(const char *name)
{
	struct node *node;

	STAILQ_FOREACH(node, &node_list, next)
		if (strncmp(node->name, name, RTE_NODE_NAMESIZE) == 0)
			return node;

	return NULL;
}

static bool
node_has_duplicate_entry(const char *name)
{
	struct node *node;

	/* Is duplicate name registered */
	STAILQ_FOREACH(node, &node_list, next) {
		if (strncmp(node->name, name, RTE_NODE_NAMESIZE) == 0) {
			rte_errno = EEXIST;
			return 1;
		}
	}
	return 0;
}

/* Public functions */
rte_node_t
__rte_node_register(const struct rte_node_register *reg)
{
	struct node *node;
	rte_edge_t i;
	size_t sz;

	graph_spinlock_lock();

	/* Check sanity */
	if (reg == NULL || reg->process == NULL) {
		rte_errno = EINVAL;
		goto fail;
	}

	/* Check for duplicate name */
	if (node_has_duplicate_entry(reg->name))
		goto fail;

	sz = sizeof(struct node) + (reg->nb_edges * RTE_NODE_NAMESIZE);
	node = calloc(1, sz);
	if (node == NULL) {
		rte_errno = ENOMEM;
		goto fail;
	}

	/* Initialize the node */
	if (rte_strscpy(node->name, reg->name, RTE_NODE_NAMESIZE) < 0) {
		rte_errno = E2BIG;
		goto free;
	}
	node->flags = reg->flags;
	node->process = reg->process;
	node->init = reg->init;
	node->fini = reg->fini;
	node->nb_edges = reg->nb_edges;
	node->parent_id = reg->parent_id;
	for (i = 0; i < reg->nb_edges; i++) {
		if (rte_strscpy(node->next_nodes[i], reg->next_nodes[i],
				RTE_NODE_NAMESIZE) < 0) {
			rte_errno = E2BIG;
			goto free;
		}
	}

	node->id = node_id++;

	/* Add the node at tail */
	STAILQ_INSERT_TAIL(&node_list, node, next);
	graph_spinlock_unlock();

	return node->id;
free:
	free(node);
fail:
	graph_spinlock_unlock();
	return RTE_NODE_ID_INVALID;
}

