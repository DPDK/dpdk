/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_STACK_STD_H_
#define _RTE_STACK_STD_H_

#include <rte_branch_prediction.h>

/**
 * @internal Push several objects on the stack (MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to push on the stack from the obj_table.
 * @return
 *   Actual number of objects pushed (either 0 or *n*).
 */
static __rte_always_inline unsigned int
__rte_stack_std_push(struct rte_stack *s, void * const *obj_table,
		     unsigned int n)
{
	struct rte_stack_std *stack = &s->stack_std;
	unsigned int index;
	void **stack_objs;

	rte_spinlock_lock(&stack->lock);
	stack_objs = &stack->objs[stack->len];

	if (unlikely((stack->len + n) > s->capacity)) {
		/* Insufficient space in the stack. */
		rte_spinlock_unlock(&stack->lock);
		return 0;
	}

	/* Push objects to the stack */
	for (index = 0; index < n; ++index, obj_table++)
		stack_objs[index] = *obj_table;

	stack->len += n;

	rte_spinlock_unlock(&stack->lock);
	return n;
}

/**
 * @internal Pop several objects from the stack (MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to pull from the stack.
 * @return
 *   Actual number of objects popped (either 0 or *n*).
 */
static __rte_always_inline unsigned int
__rte_stack_std_pop(struct rte_stack *s, void **obj_table, unsigned int n)
{
	struct rte_stack_std *stack = &s->stack_std;
	unsigned int index, len;
	void **stack_objs;

	rte_spinlock_lock(&stack->lock);

	if (unlikely(n > stack->len)) {
		/* Insufficient objects in the stack. */
		rte_spinlock_unlock(&stack->lock);
		return 0;
	}

	stack_objs = stack->objs;

	/* Pop objects from the stack */
	for (index = 0, len = stack->len - 1; index < n;
			++index, len--, obj_table++)
		*obj_table = stack_objs[len];

	stack->len -= n;
	rte_spinlock_unlock(&stack->lock);

	return n;
}

/**
 * @internal Return the number of used entries in a stack.
 *
 * @param s
 *   A pointer to the stack structure.
 * @return
 *   The number of used entries in the stack.
 */
static __rte_always_inline unsigned int
__rte_stack_std_count(struct rte_stack *s)
{
	return (unsigned int)s->stack_std.len;
}

/**
 * @internal Initialize a standard stack.
 *
 * @param s
 *   A pointer to the stack structure.
 */
void
rte_stack_std_init(struct rte_stack *s);

/**
 * @internal Return the memory required for a standard stack.
 *
 * @param count
 *   The size of the stack.
 * @return
 *   The bytes to allocate for a standard stack.
 */
ssize_t
rte_stack_std_get_memsize(unsigned int count);

#endif /* _RTE_STACK_STD_H_ */
