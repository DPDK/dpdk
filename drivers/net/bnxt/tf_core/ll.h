/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Linked List Header File */

#ifndef _LL_H_
#define _LL_H_

/* linked list entry */
struct ll_entry {
	struct ll_entry *prev;
	struct ll_entry *next;
};

/* linked list */
struct ll {
	struct ll_entry *head;
	struct ll_entry *tail;
};

/**
 * Linked list initialization.
 *
 * [in] ll, linked list to be initialized
 */
void ll_init(struct ll *ll);

/**
 * Linked list insert
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

#endif /* _LL_H_ */
