/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Linked List Functions */

#include <stdio.h>
#include "ll.h"

/* init linked list */
void ll_init(struct ll *ll)
{
	ll->head = NULL;
	ll->tail = NULL;
	ll->cnt = 0;
}

/* insert entry in linked list */
void ll_insert(struct ll *ll,
	       struct ll_entry *entry)
{
	if (ll->head == NULL) {
		ll->head = entry;
		ll->tail = entry;
		entry->next = NULL;
		entry->prev = NULL;
	} else {
		entry->next = ll->head;
		entry->prev = NULL;
		entry->next->prev = entry;
		ll->head = entry->next->prev;
	}
	ll->cnt++;
}

/* delete entry from linked list */
void ll_delete(struct ll *ll,
	       struct ll_entry *entry)
{
	if (ll->head == entry && ll->tail == entry) {
		ll->head = NULL;
		ll->tail = NULL;
	} else if (ll->head == entry) {
		ll->head = entry->next;
		ll->head->prev = NULL;
	} else if (ll->tail == entry) {
		ll->tail = entry->prev;
		ll->tail->next = NULL;
	} else {
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
	}
	ll->cnt--;
}
