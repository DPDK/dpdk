/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "stack.h"

#define STACK_EMPTY -1

/* Initialize stack
 */
int
stack_init(int num_entries, uint32_t *items, struct stack *st)
{
	if (items == NULL || st == NULL)
		return -EINVAL;

	st->max = num_entries;
	st->top = STACK_EMPTY;
	st->items = items;

	return 0;
}

/*
 * Return the address of the items
 */
uint32_t *stack_items(struct stack *st)
{
	return st->items;
}

/* Return the size of the stack
 */
int32_t
stack_size(struct stack *st)
{
	return st->top + 1;
}

/* Check if the stack is empty
 */
bool
stack_is_empty(struct stack *st)
{
	return st->top == STACK_EMPTY;
}

/* Check if the stack is full
 */
bool
stack_is_full(struct stack *st)
{
	return st->top == st->max - 1;
}

/* Add  element x to  the stack
 */
int
stack_push(struct stack *st, uint32_t x)
{
	if (stack_is_full(st))
		return -EOVERFLOW;

	/* add an element and increments the top index
	 */
	st->items[++st->top] = x;

	return 0;
}

/* Pop top element x from the stack and return
 * in user provided location.
 */
int
stack_pop(struct stack *st, uint32_t *x)
{
	if (stack_is_empty(st))
		return -ENOENT;

	*x = st->items[st->top];
	st->top--;

	return 0;
}

/* Dump the stack
 */
void stack_dump(struct stack *st)
{
	int i, j;

	printf("top=%d\n", st->top);
	printf("max=%d\n", st->max);

	if (st->top == -1) {
		printf("stack is empty\n");
		return;
	}

	for (i = 0; i < st->max + 7 / 8; i++) {
		printf("item[%d] 0x%08x", i, st->items[i]);

		for (j = 0; j < 7; j++) {
			if (i++ < st->max - 1)
				printf(" 0x%08x", st->items[i]);
		}
		printf("\n");
	}
}
