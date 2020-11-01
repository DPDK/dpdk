/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_OSDEP_LIST_H__
#define __DLB_OSDEP_LIST_H__

#include <rte_tailq.h>

struct dlb_list_entry {
	TAILQ_ENTRY(dlb_list_entry) node;
};

/* Dummy - just a struct definition */
TAILQ_HEAD(dlb_list_head, dlb_list_entry);

/* =================
 * TAILQ Supplements
 * =================
 */

#ifndef TAILQ_FOREACH_ENTRY
#define TAILQ_FOREACH_ENTRY(ptr, head, name, iter)		\
	for ((iter) = TAILQ_FIRST(&head);			\
	    (iter)						\
		&& (ptr = container_of(iter, typeof(*(ptr)), name)); \
	    (iter) = TAILQ_NEXT((iter), node))
#endif

#ifndef TAILQ_FOREACH_ENTRY_SAFE
#define TAILQ_FOREACH_ENTRY_SAFE(ptr, head, name, iter, tvar)	\
	for ((iter) = TAILQ_FIRST(&head);			\
	    (iter) &&						\
		(ptr = container_of(iter, typeof(*(ptr)), name)) &&\
		((tvar) = TAILQ_NEXT((iter), node), 1);	\
	    (iter) = (tvar))
#endif

/* =========
 * DLB Lists
 * =========
 */

/**
 * dlb_list_init_head() - initialize the head of a list
 * @head: list head
 */
static inline void dlb_list_init_head(struct dlb_list_head *head)
{
	TAILQ_INIT(head);
}

/**
 * dlb_list_add() - add an entry to a list
 * @head: new entry will be added after this list header
 * @entry: new list entry to be added
 */
static inline void dlb_list_add(struct dlb_list_head *head,
				struct dlb_list_entry *entry)
{
	TAILQ_INSERT_TAIL(head, entry, node);
}

/**
 * @head: list head
 * @entry: list entry to be deleted
 */
static inline void dlb_list_del(struct dlb_list_head *head,
				struct dlb_list_entry *entry)
{
	TAILQ_REMOVE(head, entry, node);
}

/**
 * dlb_list_empty() - check if a list is empty
 * @head: list head
 *
 * Return:
 * Returns 1 if empty, 0 if not.
 */
static inline bool dlb_list_empty(struct dlb_list_head *head)
{
	return TAILQ_EMPTY(head);
}

/**
 * dlb_list_empty() - check if a list is empty
 * @src_head: list to be added
 * @ head: where src_head will be inserted
 */
static inline void dlb_list_splice(struct dlb_list_head *src_head,
				   struct dlb_list_head *head)
{
	TAILQ_CONCAT(head, src_head, node);
}

/**
 * DLB_LIST_HEAD() - retrieve the head of the list
 * @head: list head
 * @type: type of the list variable
 * @name: name of the dlb_list within the struct
 */
#define DLB_LIST_HEAD(head, type, name)				\
	(TAILQ_FIRST(&head) ?					\
		container_of(TAILQ_FIRST(&head), type, name) :	\
		NULL)

/**
 * DLB_LIST_FOR_EACH() - iterate over a list
 * @head: list head
 * @ptr: pointer to struct containing a struct dlb_list_entry
 * @name: name of the dlb_list_entry field within the containing struct
 * @iter: iterator variable
 */
#define DLB_LIST_FOR_EACH(head, ptr, name, tmp_iter) \
	TAILQ_FOREACH_ENTRY(ptr, head, name, tmp_iter)

/**
 * DLB_LIST_FOR_EACH_SAFE() - iterate over a list. This loop works even if
 * an element is removed from the list while processing it.
 * @ptr: pointer to struct containing a struct dlb_list_entry
 * @ptr_tmp: pointer to struct containing a struct dlb_list_entry (temporary)
 * @head: list head
 * @name: name of the dlb_list_entry field within the containing struct
 * @iter: iterator variable
 * @iter_tmp: iterator variable (temporary)
 */
#define DLB_LIST_FOR_EACH_SAFE(head, ptr, ptr_tmp, name, tmp_iter, saf_iter) \
	TAILQ_FOREACH_ENTRY_SAFE(ptr, head, name, tmp_iter, saf_iter)

#endif /*  __DLB_OSDEP_LIST_H__ */
