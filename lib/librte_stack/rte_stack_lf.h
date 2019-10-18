/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_STACK_LF_H_
#define _RTE_STACK_LF_H_

#if !(defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_ARM64))
#include "rte_stack_lf_stubs.h"
#else
#ifdef RTE_USE_C11_MEM_MODEL
#include "rte_stack_lf_c11.h"
#else
#include "rte_stack_lf_generic.h"
#endif
#endif

/**
 * @internal Push several objects on the lock-free stack (MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to push on the stack from the obj_table.
 * @return
 *   Actual number of objects enqueued.
 */
__rte_experimental
static __rte_always_inline unsigned int
__rte_stack_lf_push(struct rte_stack *s,
		    void * const *obj_table,
		    unsigned int n)
{
	struct rte_stack_lf_elem *tmp, *first, *last = NULL;
	unsigned int i;

	if (unlikely(n == 0))
		return 0;

	/* Pop n free elements */
	first = __rte_stack_lf_pop_elems(&s->stack_lf.free, n, NULL, &last);
	if (unlikely(first == NULL))
		return 0;

	/* Construct the list elements */
	for (tmp = first, i = 0; i < n; i++, tmp = tmp->next)
		tmp->data = obj_table[n - i - 1];

	/* Push them to the used list */
	__rte_stack_lf_push_elems(&s->stack_lf.used, first, last, n);

	return n;
}

/**
 * @internal Pop several objects from the lock-free stack (MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to pull from the stack.
 * @return
 *   - Actual number of objects popped.
 */
__rte_experimental
static __rte_always_inline unsigned int
__rte_stack_lf_pop(struct rte_stack *s, void **obj_table, unsigned int n)
{
	struct rte_stack_lf_elem *first, *last = NULL;

	if (unlikely(n == 0))
		return 0;

	/* Pop n used elements */
	first = __rte_stack_lf_pop_elems(&s->stack_lf.used,
					 n, obj_table, &last);
	if (unlikely(first == NULL))
		return 0;

	/* Push the list elements to the free list */
	__rte_stack_lf_push_elems(&s->stack_lf.free, first, last, n);

	return n;
}

/**
 * @internal Initialize a lock-free stack.
 *
 * @param s
 *   A pointer to the stack structure.
 * @param count
 *   The size of the stack.
 */
void
rte_stack_lf_init(struct rte_stack *s, unsigned int count);

/**
 * @internal Return the memory required for a lock-free stack.
 *
 * @param count
 *   The size of the stack.
 * @return
 *   The bytes to allocate for a lock-free stack.
 */
ssize_t
rte_stack_lf_get_memsize(unsigned int count);

#endif /* _RTE_STACK_LF_H_ */
