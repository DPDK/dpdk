/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 SmartShare Systems
 */

#ifndef _RTE_STACK_PILE_H_
#define _RTE_STACK_PILE_H_

#include <rte_memcpy.h>

#include "rte_stack_lf.h"
#ifdef RTE_STACK_LF_SUPPORTED
/**
 * Indicates that RTE_STACK_F_PILE is supported.
 */
#define RTE_STACK_PILE_SUPPORTED
#endif

static __rte_always_inline unsigned int
__rte_stack_pile_count(struct rte_stack *s)
{
	/* stack_lf_push() and stack_lf_pop() do not update the list's contents
	 * and stack_lf->len atomically, which can cause the list to appear
	 * shorter than it actually is if this function is called while other
	 * threads are modifying the list.
	 *
	 * However, given the inherently approximate nature of the get_count
	 * callback -- even if the list and its size were updated atomically,
	 * the size could change between when get_count executes and when the
	 * value is returned to the caller -- this is acceptable.
	 *
	 * The stack_lf->len updates are placed such that the list may appear to
	 * have fewer elements than it does, but will never appear to have more
	 * elements. If the mempool is near-empty to the point that this is a
	 * concern, the user should consider increasing the mempool size.
	 */
	return RTE_MIN((unsigned int)s->capacity,
			__rte_stack_lf_elems_count(&s->stack_pile.bulk) * RTE_STACK_PILE_BULK_SIZE +
			__rte_stack_lf_elems_count(&s->stack_pile.solo));
}

static __rte_always_inline void
__rte_stack_pile_bulk_push_elems(struct rte_stack_lf_list *list,
		struct rte_stack_pile_bulk_elem *first,
		struct rte_stack_pile_bulk_elem *last,
		unsigned int num)
{
	__rte_stack_lf_push_elems(list,
		(struct rte_stack_lf_elem *)first,
		(struct rte_stack_lf_elem *)last,
		num);
}

static __rte_always_inline struct rte_stack_pile_bulk_elem *
__rte_stack_pile_bulk_pop_elems(struct rte_stack_lf_list *list,
		unsigned int num,
		void **obj_table,
		struct rte_stack_pile_bulk_elem **last)
{
	struct rte_stack_lf_elem *first =
			__rte_stack_lf_pop_elems(list, num, NULL,
			(struct rte_stack_lf_elem **)last);
	if (first == NULL)
		return NULL;

	if (obj_table != NULL) {
		/*
		 * Traverse the list to copy the bulks.
		 * Note:
		 * Done here to minimize the time spent in the retry loop in
		 * __rte_stack_lf_pop_elems(),
		 * and to avoid modifying __rte_stack_lf_pop_elems().
		 */
		struct rte_stack_lf_elem *tmp = first;
		for (unsigned int i = 0; i < num; i++, tmp = tmp->next)
			rte_memcpy(&obj_table[i * RTE_STACK_PILE_BULK_SIZE],
					((struct rte_stack_pile_bulk_elem *)tmp)->objs,
					sizeof(void *) * RTE_STACK_PILE_BULK_SIZE);
	}

	return (struct rte_stack_pile_bulk_elem *)first;
}

/**
 * Push several objects on the pile (lock-free, MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to push on the pile from the obj_table.
 * @return
 *   Actual number of objects pushed (either 0 or *n*).
 */
static __rte_always_inline unsigned int
__rte_stack_pile_push(struct rte_stack *s,
		void * const *obj_table,
		unsigned int n)
{
	RTE_ASSERT(s != NULL);
	RTE_ASSERT(obj_table != NULL);

	struct rte_stack_pile *pile = &s->stack_pile;
	struct rte_stack_pile_bulk_elem *bulk_first = NULL, *bulk_last = NULL;
	struct rte_stack_lf_elem *solo_first = NULL, *solo_last = NULL;
	struct rte_stack_lf_elem *tmp;
	unsigned int n_bulk = n / RTE_STACK_PILE_BULK_SIZE;
	unsigned int n_solo = n & (RTE_STACK_PILE_BULK_SIZE - 1);
	unsigned int i;

	if (unlikely(n_bulk == 0)) {
		if (unlikely(n_solo == 0))
			return 0;
		goto solo;
	}

	/* Allocate n_bulk elements from the free list. */
	bulk_first = __rte_stack_pile_bulk_pop_elems(&pile->free_bulk, n_bulk, NULL, &bulk_last);
	if (unlikely(bulk_first == NULL))
		return 0; /* Failed. */

	if (likely(n_solo == 0))
		goto bulk;

solo:
	/* Allocate n_solo elements from the free list. */
	solo_first = __rte_stack_lf_pop_elems(&pile->free_solo, n_solo, NULL, &solo_last);
	if (unlikely(solo_first == NULL)) {
		/* Failed. Roll back. */
		if (n_bulk > 0)
			__rte_stack_pile_bulk_push_elems(&pile->free_bulk,
					bulk_first, bulk_last, n_bulk);
		return 0;
	}

	/*
	 * Construct the solo elements.
	 * Copy objects in reverse order.
	 */
	tmp = solo_first;
	__rte_assume(n_solo > 0);
	__rte_assume(n_solo < RTE_STACK_PILE_BULK_SIZE);
	for (i = 0; i < n_solo; i++, tmp = tmp->next)
		tmp->data = obj_table[n_bulk * RTE_STACK_PILE_BULK_SIZE + n_solo - i - 1];

	/* Push them to the solo list. */
	__rte_stack_lf_push_elems(&pile->solo, solo_first, solo_last, n_solo);

	if (unlikely(n_bulk == 0))
		return n; /* Done. */

bulk:
	/*
	 * Construct the bulk elements.
	 * Copy bulks in reverse order, but ignore the object order within each bulk.
	 */
	tmp = (struct rte_stack_lf_elem *)bulk_first;
	__rte_assume(n_bulk > 0);
	for (i = 0; i < n_bulk; i++, tmp = tmp->next)
		rte_memcpy(((struct rte_stack_pile_bulk_elem *)tmp)->objs,
				&obj_table[(n_bulk - i - 1) * RTE_STACK_PILE_BULK_SIZE],
				sizeof(void *) * RTE_STACK_PILE_BULK_SIZE);

	/* Push them to the bulk list. */
	__rte_stack_pile_bulk_push_elems(&pile->bulk, bulk_first, bulk_last, n_bulk);

	return n;
}

/**
 * @internal Pop objects from the pile using fragmentation of a bulk element.
 *
 * @param pile
 *   A pointer to the pile structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to pull.
 *   Must be non-zero and less than RTE_STACK_PILE_BULK_SIZE.
 * @return
 *   Actual number of objects popped (either 0 or *n*).
 */
static __rte_noinline unsigned int
__rte_stack_pile_pop_frag(struct rte_stack_pile * const pile,
		void **obj_table,
		unsigned int n)
{
	alignas(RTE_CACHE_LINE_SIZE) void *obj_frag[RTE_STACK_PILE_BULK_SIZE];
	struct rte_stack_pile_bulk_elem *frag = NULL;
	struct rte_stack_lf_elem *solo_first = NULL, *solo_last = NULL, *tmp;
	unsigned int i;

	RTE_ASSERT(n > 0);
	RTE_ASSERT(n < RTE_STACK_PILE_BULK_SIZE);

	/* Fetch a bulk element for fragmentation. */
	frag = __rte_stack_pile_bulk_pop_elems(&pile->bulk, 1, obj_frag, NULL);
	if (unlikely(frag == NULL))
		return 0; /* Not available. */

	/* Get n objects from the bulk element. */
	__rte_assume(n > 0);
	__rte_assume(n < RTE_STACK_PILE_BULK_SIZE);
	for (i = 0; i < n; i++)
		obj_table[i] = obj_frag[i];

	/* Fetch free solo elements for the excess objects. */
	__rte_assume(RTE_STACK_PILE_BULK_SIZE - n > 0);
	__rte_assume(RTE_STACK_PILE_BULK_SIZE - n < RTE_STACK_PILE_BULK_SIZE);
	solo_first = __rte_stack_lf_pop_elems(&pile->free_solo,
			RTE_STACK_PILE_BULK_SIZE - n, NULL, &solo_last);
	if (unlikely(solo_first == NULL)) {
		/*
		 * Failed. Roll back.
		 * No further action is required to roll the bulk element
		 * back into the pile of bulk elements, as the objects in
		 * the bulk element are intact.
		 */
		__rte_stack_pile_bulk_push_elems(&pile->bulk, frag, frag, 1);
		return 0;
	}

	/* Construct the solo elements from the excess objects. */
	tmp = solo_first;
	__rte_assume(n > 0);
	__rte_assume(n < RTE_STACK_PILE_BULK_SIZE);
	for (i = n; i < RTE_STACK_PILE_BULK_SIZE; i++, tmp = tmp->next)
		tmp->data = obj_frag[i];

	/* Push the excess objects as solo elements. */
	__rte_stack_lf_push_elems(&pile->solo, solo_first, solo_last,
			RTE_STACK_PILE_BULK_SIZE - n);

	/* Free the bulk element. */
	__rte_stack_pile_bulk_push_elems(&pile->free_bulk, frag, frag, 1);

	return n;
}

/**
 * Pop several objects from the pile (lock-free, MT-safe).
 *
 * @param s
 *   A pointer to the stack structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to pull from the pile.
 * @return
 *   Actual number of objects popped (either 0 or *n*).
 */
static __rte_always_inline unsigned int
__rte_stack_pile_pop(struct rte_stack *s,
		void **obj_table,
		unsigned int n)
{
	RTE_ASSERT(s != NULL);
	RTE_ASSERT(obj_table != NULL);

	struct rte_stack_pile *pile = &s->stack_pile;
	struct rte_stack_pile_bulk_elem *bulk_first = NULL, *bulk_last = NULL;
	struct rte_stack_lf_elem *solo_first = NULL, *solo_last = NULL;
	unsigned int n_bulk = n / RTE_STACK_PILE_BULK_SIZE;
	unsigned int n_solo = n & (RTE_STACK_PILE_BULK_SIZE - 1);

	if (unlikely(n_bulk == 0)) {
		if (unlikely(n_solo == 0))
			return 0;
		goto solo;
	}

bulk:
	/* Fetch n_bulk * RTE_STACK_PILE_BULK_SIZE objects as bulk elements. */
	bulk_first = __rte_stack_pile_bulk_pop_elems(&pile->bulk, n_bulk, obj_table, &bulk_last);
	if (unlikely(bulk_first == NULL)) {
		/*
		 * Not available.
		 * Determine how many are available, and retry with fewer than before.
		 * Remaining objects are to be fetched as solo elements instead.
		 * TODO: Retry could be avoided if pop_elems() had a burst variant.
		 */
		unsigned int delta_bulk = n_bulk - __rte_stack_lf_elems_count(&pile->bulk);

		if (unlikely((int)delta_bulk <= 0)) {
			/*
			 * Another thread raced to add bulk elements; more are available now.
			 * Retry with one less than before.
			 * Note: By only retrying with fewer, progress is guaranteed.
			 */
			delta_bulk = 1;
		}
		n_bulk -= delta_bulk;
		n_solo += RTE_STACK_PILE_BULK_SIZE * delta_bulk;

		if (n_bulk == 0)
			goto solo; /* No bulk elements available. Stop retrying. */

		goto bulk;
	}

	obj_table += n_bulk * RTE_STACK_PILE_BULK_SIZE;

	if (likely(n_solo == 0))
		goto done;

solo:
	/* Fetch n_solo objects as solo elements. */
	solo_first = __rte_stack_lf_pop_elems(&pile->solo, n_solo,
			obj_table, &solo_last);
	if (solo_first != NULL)
		goto done;

	/* Solo elements not available. Try fetching objects by fragmentation of a bulk element. */
	if (unlikely(n_solo >= RTE_STACK_PILE_BULK_SIZE))
		goto fail; /* Ran out of bulk elements above. Don't try to fetch one more. */

	if (unlikely(__rte_stack_pile_pop_frag(pile, obj_table, n_solo) == 0))
		goto fail; /* Fragmentation failed. */

done:
	/* Success. Free the elements. */
	if (bulk_first != NULL)
		__rte_stack_pile_bulk_push_elems(&pile->free_bulk, bulk_first, bulk_last, n_bulk);
	if (solo_first != NULL)
		__rte_stack_lf_push_elems(&pile->free_solo, solo_first, solo_last, n_solo);

	return n;

fail:
	/* Failed. Roll back. */
	if (bulk_first != NULL)
		__rte_stack_pile_bulk_push_elems(&pile->bulk, bulk_first, bulk_last, n_bulk);

	return 0;
}

/**
 * @internal Initialize a pile stack.
 *
 * @param s
 *   A pointer to the stack structure.
 * @param count
 *   The size of the stack.
 */
void
rte_stack_pile_init(struct rte_stack *s, unsigned int count);

/**
 * @internal Return the memory required for a pile stack.
 *
 * @param count
 *   The size of the stack.
 * @return
 *   The bytes to allocate for a pile stack.
 */
ssize_t
rte_stack_pile_get_memsize(unsigned int count);

#endif /* _RTE_STACK_PILE_H_ */
