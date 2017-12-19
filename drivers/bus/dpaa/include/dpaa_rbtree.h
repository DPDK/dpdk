/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef __DPAA_RBTREE_H
#define __DPAA_RBTREE_H

#include <rte_common.h>
/************/
/* RB-trees */
/************/

/* Linux has a good RB-tree implementation, that we can't use (GPL). It also has
 * a flat/hooked-in interface that virtually requires license-contamination in
 * order to write a caller-compatible implementation. Instead, I've created an
 * RB-tree encapsulation on top of linux's primitives (it does some of the work
 * the client logic would normally do), and this gives us something we can
 * reimplement on LWE. Unfortunately there's no good+free RB-tree
 * implementations out there that are license-compatible and "flat" (ie. no
 * dynamic allocation). I did find a malloc-based one that I could convert, but
 * that will be a task for later on. For now, LWE's RB-tree is implemented using
 * an ordered linked-list.
 *
 * Note, the only linux-esque type is "struct rb_node", because it's used
 * statically in the exported header, so it can't be opaque. Our version doesn't
 * include a "rb_parent_color" field because we're doing linked-list instead of
 * a true rb-tree.
 */

struct rb_node {
	struct rb_node *prev, *next;
};

struct dpa_rbtree {
	struct rb_node *head, *tail;
};

#define DPAA_RBTREE { NULL, NULL }
static inline void dpa_rbtree_init(struct dpa_rbtree *tree)
{
	tree->head = tree->tail = NULL;
}

#define QMAN_NODE2OBJ(ptr, type, node_field) \
	(type *)((char *)ptr - offsetof(type, node_field))

#define IMPLEMENT_DPAA_RBTREE(name, type, node_field, val_field) \
static inline int name##_push(struct dpa_rbtree *tree, type *obj) \
{ \
	struct rb_node *node = tree->head; \
	if (!node) { \
		tree->head = tree->tail = &obj->node_field; \
		obj->node_field.prev = obj->node_field.next = NULL; \
		return 0; \
	} \
	while (node) { \
		type *item = QMAN_NODE2OBJ(node, type, node_field); \
		if (obj->val_field == item->val_field) \
			return -EBUSY; \
		if (obj->val_field < item->val_field) { \
			if (tree->head == node) \
				tree->head = &obj->node_field; \
			else \
				node->prev->next = &obj->node_field; \
			obj->node_field.prev = node->prev; \
			obj->node_field.next = node; \
			node->prev = &obj->node_field; \
			return 0; \
		} \
		node = node->next; \
	} \
	obj->node_field.prev = tree->tail; \
	obj->node_field.next = NULL; \
	tree->tail->next = &obj->node_field; \
	tree->tail = &obj->node_field; \
	return 0; \
} \
static inline void name##_del(struct dpa_rbtree *tree, type *obj) \
{ \
	if (tree->head == &obj->node_field) { \
		if (tree->tail == &obj->node_field) \
			/* Only item in the list */ \
			tree->head = tree->tail = NULL; \
		else { \
			/* Is the head, next != NULL */ \
			tree->head = tree->head->next; \
			tree->head->prev = NULL; \
		} \
	} else { \
		if (tree->tail == &obj->node_field) { \
			/* Is the tail, prev != NULL */ \
			tree->tail = tree->tail->prev; \
			tree->tail->next = NULL; \
		} else { \
			/* Is neither the head nor the tail */ \
			obj->node_field.prev->next = obj->node_field.next; \
			obj->node_field.next->prev = obj->node_field.prev; \
		} \
	} \
} \
static inline type *name##_find(struct dpa_rbtree *tree, u32 val) \
{ \
	struct rb_node *node = tree->head; \
	while (node) { \
		type *item = QMAN_NODE2OBJ(node, type, node_field); \
		if (val == item->val_field) \
			return item; \
		if (val < item->val_field) \
			return NULL; \
		node = node->next; \
	} \
	return NULL; \
}

#endif /* __DPAA_RBTREE_H */
