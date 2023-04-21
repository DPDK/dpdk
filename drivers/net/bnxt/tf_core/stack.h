/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */
#ifndef _STACK_H_
#define _STACK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/** Stack data structure
 */
struct stack {
	int max;         /**< Maximum number of entries */
	int top;         /**< maximum value in stack */
	uint32_t *items; /**< items in the stack */
};

/** Initialize stack of uint32_t elements
 *
 *  [in] num_entries
 *    maximum number of elements in the stack
 *
 *  [in] items
 *    pointer to items (must be sized to (uint32_t * num_entries)
 *
 *  s[in] st
 *    pointer to the stack structure
 *
 *  return
 *    0 for success
 */
int stack_init(int num_entries,
	       uint32_t *items,
	       struct stack *st);

/** Return the address of the stack contents
 *
 *  [in] st
 *    pointer to the stack
 *
 *  return
 *    pointer to the stack contents
 */
uint32_t *stack_items(struct stack *st);

/** Return the size of the stack
 *
 *  [in] st
 *    pointer to the stack
 *
 *  return
 *    number of elements
 */
int32_t stack_size(struct stack *st);

/** Check if the stack is empty
 *
 * [in] st
 *   pointer to the stack
 *
 * return
 *   true or false
 */
bool stack_is_empty(struct stack *st);

/** Check if the stack is full
 *
 * [in] st
 *   pointer to the stack
 *
 * return
 *   true or false
 */
bool stack_is_full(struct stack *st);

/** Add  element x to  the stack
 *
 * [in] st
 *   pointer to the stack
 *
 * [in] x
 *   value to push on the stack
 * return
 *  0 for success
 */
int stack_push(struct stack *st, uint32_t x);

/** Pop top element x from the stack and return
 * in user provided location.
 *
 * [in] st
 *   pointer to the stack
 *
 * [in, out] x
 *  pointer to where the value popped will be written
 *
 * return
 *  0 for success
 */
int stack_pop(struct stack *st, uint32_t *x);

/** Dump stack information
 *
 * Warning: Don't use for large stacks due to prints
 *
 * [in] st
 *   pointer to the stack
 *
 * return
 *    none
 */
void stack_dump(struct stack *st);
#endif /* _STACK_H_ */
