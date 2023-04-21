/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

/* Linked List Header File */

#ifndef _LL_H_
#define _LL_H_

#include <stdint.h>

/* linked list entry */
struct ll_entry {
	struct ll_entry *prev;
	struct ll_entry *next;
};

/* linked list */
struct ll {
	struct ll_entry *head;
	struct ll_entry *tail;
	uint32_t cnt;
};

/**
 * Linked list initialization.
 *
 * [in] ll, linked list to be initialized
 */
void ll_init(struct ll *ll);

/**
 * Linked list insert head
 *
 * [in] ll, linked list where element is inserted
 * [in] entry, entry to be added
 */
void ll_insert(struct ll *ll, struct ll_entry *entry);

/**
 * Linked list delete
 *
 * [in] ll, linked list where element is removed
 * [in] entry, entry to be deleted
 */
void ll_delete(struct ll *ll, struct ll_entry *entry);

/**
 * Linked list return next entry without deleting it
 *
 * Useful in performing search
 *
 * [in] Entry in the list
 */
static inline struct ll_entry *ll_next(struct ll_entry *entry)
{
	return entry->next;
}

/**
 * Linked list return the head of the list without removing it
 *
 * Useful in performing search
 *
 * [in] ll, linked list
 */
static inline struct ll_entry *ll_head(struct ll *ll)
{
	return ll->head;
}

/**
 * Linked list return the tail of the list without removing it
 *
 * Useful in performing search
 *
 * [in] ll, linked list
 */
static inline struct ll_entry *ll_tail(struct ll *ll)
{
	return ll->tail;
}

/**
 * Linked list return the number of entries in the list
 *
 * [in] ll, linked list
 */
static inline uint32_t ll_cnt(struct ll *ll)
{
	return ll->cnt;
}
#endif /* _LL_H_ */
