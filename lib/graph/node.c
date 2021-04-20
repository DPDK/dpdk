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

#define NODE_ID_CHECK(id) ID_CHECK(id, node_id)

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

	/* Limit Node specific metadata to one cacheline on 64B CL machine */
	RTE_BUILD_BUG_ON((offsetof(struct rte_node, nodes) -
			  offsetof(struct rte_node, ctx)) !=
			 RTE_CACHE_LINE_MIN_SIZE);

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

static int
clone_name(struct rte_node_register *reg, struct node *node, const char *name)
{
	ssize_t sz, rc;

#define SZ RTE_NODE_NAMESIZE
	rc = rte_strscpy(reg->name, node->name, SZ);
	if (rc < 0)
		goto fail;
	sz = rc;
	rc = rte_strscpy(reg->name + sz, "-", RTE_MAX((int16_t)(SZ - sz), 0));
	if (rc < 0)
		goto fail;
	sz += rc;
	sz = rte_strscpy(reg->name + sz, name, RTE_MAX((int16_t)(SZ - sz), 0));
	if (sz < 0)
		goto fail;

	return 0;
fail:
	rte_errno = E2BIG;
	return -rte_errno;
}

static rte_node_t
node_clone(struct node *node, const char *name)
{
	rte_node_t rc = RTE_NODE_ID_INVALID;
	struct rte_node_register *reg;
	rte_edge_t i;

	/* Don't allow to clone a node from a cloned node */
	if (node->parent_id != RTE_NODE_ID_INVALID) {
		rte_errno = EEXIST;
		goto fail;
	}

	/* Check for duplicate name */
	if (node_has_duplicate_entry(name))
		goto fail;

	reg = calloc(1, sizeof(*reg) + (sizeof(char *) * node->nb_edges));
	if (reg == NULL) {
		rte_errno = ENOMEM;
		goto fail;
	}

	/* Clone the source node */
	reg->flags = node->flags;
	reg->process = node->process;
	reg->init = node->init;
	reg->fini = node->fini;
	reg->nb_edges = node->nb_edges;
	reg->parent_id = node->id;

	for (i = 0; i < node->nb_edges; i++)
		reg->next_nodes[i] = node->next_nodes[i];

	/* Naming ceremony of the new node. name is node->name + "-" + name */
	if (clone_name(reg, node, name))
		goto free;

	rc = __rte_node_register(reg);
free:
	free(reg);
fail:
	return rc;
}

rte_node_t
rte_node_clone(rte_node_t id, const char *name)
{
	struct node *node;

	NODE_ID_CHECK(id);
	STAILQ_FOREACH(node, &node_list, next)
		if (node->id == id)
			return node_clone(node, name);

fail:
	return RTE_NODE_ID_INVALID;
}

rte_node_t
rte_node_from_name(const char *name)
{
	struct node *node;

	STAILQ_FOREACH(node, &node_list, next)
		if (strncmp(node->name, name, RTE_NODE_NAMESIZE) == 0)
			return node->id;

	return RTE_NODE_ID_INVALID;
}

char *
rte_node_id_to_name(rte_node_t id)
{
	struct node *node;

	NODE_ID_CHECK(id);
	STAILQ_FOREACH(node, &node_list, next)
		if (node->id == id)
			return node->name;

fail:
	return NULL;
}

rte_edge_t
rte_node_edge_count(rte_node_t id)
{
	struct node *node;

	NODE_ID_CHECK(id);
	STAILQ_FOREACH(node, &node_list, next)
		if (node->id == id)
			return node->nb_edges;
fail:
	return RTE_EDGE_ID_INVALID;
}

static rte_edge_t
edge_update(struct node *node, struct node *prev, rte_edge_t from,
	    const char **next_nodes, rte_edge_t nb_edges)
{
	rte_edge_t i, max_edges, count = 0;
	struct node *new_node;
	bool need_realloc;
	size_t sz;

	if (from == RTE_EDGE_ID_INVALID)
		from = node->nb_edges;

	/* Don't create hole in next_nodes[] list */
	if (from > node->nb_edges) {
		rte_errno = ENOMEM;
		goto fail;
	}

	/* Remove me from list */
	STAILQ_REMOVE(&node_list, node, node, next);

	/* Allocate the storage space for new node if required */
	max_edges = from + nb_edges;
	need_realloc = max_edges > node->nb_edges;
	if (need_realloc) {
		sz = sizeof(struct node) + (max_edges * RTE_NODE_NAMESIZE);
		new_node = realloc(node, sz);
		if (new_node == NULL) {
			rte_errno = ENOMEM;
			goto restore;
		} else {
			node = new_node;
		}
	}

	/* Update the new nodes name */
	for (i = from; i < max_edges; i++, count++) {
		if (rte_strscpy(node->next_nodes[i], next_nodes[count],
				RTE_NODE_NAMESIZE) < 0) {
			rte_errno = E2BIG;
			goto restore;
		}
	}
restore:
	/* Update the linked list to point new node address in prev node */
	if (prev)
		STAILQ_INSERT_AFTER(&node_list, prev, node, next);
	else
		STAILQ_INSERT_HEAD(&node_list, node, next);

	if (need_realloc)
		node->nb_edges = max_edges;

fail:
	return count;
}

rte_edge_t
rte_node_edge_shrink(rte_node_t id, rte_edge_t size)
{
	rte_edge_t rc = RTE_EDGE_ID_INVALID;
	struct node *node;

	NODE_ID_CHECK(id);
	graph_spinlock_lock();

	STAILQ_FOREACH(node, &node_list, next) {
		if (node->id == id) {
			if (node->nb_edges < size) {
				rte_errno = E2BIG;
				goto fail;
			}
			node->nb_edges = size;
			rc = size;
			break;
		}
	}

fail:
	graph_spinlock_unlock();
	return rc;
}

rte_edge_t
rte_node_edge_update(rte_node_t id, rte_edge_t from, const char **next_nodes,
		     uint16_t nb_edges)
{
	rte_edge_t rc = RTE_EDGE_ID_INVALID;
	struct node *n, *prev;

	NODE_ID_CHECK(id);
	graph_spinlock_lock();

	prev = NULL;
	STAILQ_FOREACH(n, &node_list, next) {
		if (n->id == id) {
			rc = edge_update(n, prev, from, next_nodes, nb_edges);
			break;
		}
		prev = n;
	}

	graph_spinlock_unlock();
fail:
	return rc;
}

static rte_node_t
node_copy_edges(struct node *node, char *next_nodes[])
{
	rte_edge_t i;

	for (i = 0; i < node->nb_edges; i++)
		next_nodes[i] = node->next_nodes[i];

	return i;
}

rte_node_t
rte_node_edge_get(rte_node_t id, char *next_nodes[])
{
	rte_node_t rc = RTE_NODE_ID_INVALID;
	struct node *node;

	NODE_ID_CHECK(id);
	graph_spinlock_lock();

	STAILQ_FOREACH(node, &node_list, next) {
		if (node->id == id) {
			if (next_nodes == NULL)
				rc = sizeof(char *) * node->nb_edges;
			else
				rc = node_copy_edges(node, next_nodes);
			break;
		}
	}

	graph_spinlock_unlock();
fail:
	return rc;
}

static void
node_scan_dump(FILE *f, rte_node_t id, bool all)
{
	struct node *node;

	RTE_ASSERT(f != NULL);
	NODE_ID_CHECK(id);

	STAILQ_FOREACH(node, &node_list, next) {
		if (all == true) {
			node_dump(f, node);
		} else if (node->id == id) {
			node_dump(f, node);
			return;
		}
	}
fail:
	return;
}

void
rte_node_dump(FILE *f, rte_node_t id)
{
	node_scan_dump(f, id, false);
}

void
rte_node_list_dump(FILE *f)
{
	node_scan_dump(f, 0, true);
}

rte_node_t
rte_node_max_count(void)
{
	return node_id;
}
