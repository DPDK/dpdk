/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_LIST_H
#define __DLB2_OSDEP_LIST_H

#include <rte_tailq.h>

struct dlb2_list_entry {
	TAILQ_ENTRY(dlb2_list_entry) node;
};

/* Dummy - just a struct definition */
TAILQ_HEAD(dlb2_list_head, dlb2_list_entry);

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

/***********************/
/*** List operations ***/
/***********************/

/**
 * dlb2_list_init_head() - initialize the head of a list
 * @head: list head
 */
static inline void dlb2_list_init_head(struct dlb2_list_head *head)
{
	TAILQ_INIT(head);
}

/**
 * dlb2_list_add() - add an entry to a list
 * @head: list head
 * @entry: new list entry
 */
static inline void
dlb2_list_add(struct dlb2_list_head *head, struct dlb2_list_entry *entry)
{
	TAILQ_INSERT_TAIL(head, entry, node);
}

/**
 * dlb2_list_del() - delete an entry from a list
 * @entry: list entry
 * @head: list head
 */
static inline void dlb2_list_del(struct dlb2_list_head *head,
				 struct dlb2_list_entry *entry)
{
	TAILQ_REMOVE(head, entry, node);
}

/**
 * dlb2_list_empty() - check if a list is empty
 * @head: list head
 *
 * Return:
 * Returns 1 if empty, 0 if not.
 */
static inline int dlb2_list_empty(struct dlb2_list_head *head)
{
	return TAILQ_EMPTY(head);
}

/**
 * dlb2_list_splice() - splice a list
 * @src_head: list to be added
 * @ head: where src_head will be inserted
 */
static inline void dlb2_list_splice(struct dlb2_list_head *src_head,
				    struct dlb2_list_head *head)
{
	TAILQ_CONCAT(head, src_head, node);
}

/**
 * DLB2_LIST_HEAD() - retrieve the head of the list
 * @head: list head
 * @type: type of the list variable
 * @name: name of the list field within the containing struct
 */
#define DLB2_LIST_HEAD(head, type, name)                       \
	(TAILQ_FIRST(&head) ?					\
		container_of(TAILQ_FIRST(&head), type, name) :	\
		NULL)

/**
 * DLB2_LIST_FOR_EACH() - iterate over a list
 * @head: list head
 * @ptr: pointer to struct containing a struct list
 * @name: name of the list field within the containing struct
 * @iter: iterator variable
 */
#define DLB2_LIST_FOR_EACH(head, ptr, name, tmp_iter) \
	TAILQ_FOREACH_ENTRY(ptr, head, name, tmp_iter)

/**
 * DLB2_LIST_FOR_EACH_SAFE() - iterate over a list. This loop works even if
 * an element is removed from the list while processing it.
 * @ptr: pointer to struct containing a struct list
 * @ptr_tmp: pointer to struct containing a struct list (temporary)
 * @head: list head
 * @name: name of the list field within the containing struct
 * @iter: iterator variable
 * @iter_tmp: iterator variable (temporary)
 */
#define DLB2_LIST_FOR_EACH_SAFE(head, ptr, ptr_tmp, name, tmp_iter, saf_itr) \
	TAILQ_FOREACH_ENTRY_SAFE(ptr, head, name, tmp_iter, saf_itr)

#endif /*  __DLB2_OSDEP_LIST_H */
