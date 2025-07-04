/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_STACK_LF_C11_H_
#define _RTE_STACK_LF_C11_H_

#include <rte_branch_prediction.h>
#include <rte_prefetch.h>

#ifdef RTE_TOOLCHAIN_MSVC
/**
 * The maximum lock-free data size that can be manipulated atomically using C11
 * standard is limited to 8 bytes.
 *
 * This implementation for rte_atomic128_cmp_exchange operates on 16-byte
 * data types and is made available here so that it can be used without the
 * need to unnecessarily expose other non-C11 atomics present in
 * rte_atomic_64.h when using MSVC.
 */
static inline int
rte_atomic128_cmp_exchange(rte_int128_t *dst,
			   rte_int128_t *exp,
			   const rte_int128_t *src,
			   unsigned int weak,
			   int success,
			   int failure)
{
	return (int)_InterlockedCompareExchange128(
		(int64_t volatile *) dst,
		src->val[1], /* exchange high */
		src->val[0], /* exchange low */
		(int64_t *) exp /* comparand result */
	);
}
#endif /* RTE_TOOLCHAIN_MSVC */

static __rte_always_inline unsigned int
__rte_stack_lf_count(struct rte_stack *s)
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
	return (unsigned int)rte_atomic_load_explicit(&s->stack_lf.used.len,
					     rte_memory_order_relaxed);
}

static __rte_always_inline void
__rte_stack_lf_push_elems(struct rte_stack_lf_list *list,
			  struct rte_stack_lf_elem *first,
			  struct rte_stack_lf_elem *last,
			  unsigned int num)
{
	struct rte_stack_lf_head old_head;
	int success;

	old_head = list->head;

	do {
		struct rte_stack_lf_head new_head;

		/* Swing the top pointer to the first element in the list and
		 * make the last element point to the old top.
		 */
		new_head.top = first;
		new_head.cnt = old_head.cnt + 1;

		last->next = old_head.top;

		/* Use the release memmodel to ensure the writes to the LF LIFO
		 * elements are visible before the head pointer write.
		 */
		success = rte_atomic128_cmp_exchange(
				(rte_int128_t *)&list->head,
				(rte_int128_t *)&old_head,
				(rte_int128_t *)&new_head,
				1, rte_memory_order_release,
				rte_memory_order_relaxed);
	} while (success == 0);

	/* Ensure the stack modifications are not reordered with respect
	 * to the LIFO len update.
	 */
	rte_atomic_fetch_add_explicit(&list->len, num, rte_memory_order_release);
}

static __rte_always_inline struct rte_stack_lf_elem *
__rte_stack_lf_pop_elems(struct rte_stack_lf_list *list,
			 unsigned int num,
			 void **obj_table,
			 struct rte_stack_lf_elem **last)
{
	struct rte_stack_lf_head old_head;
	uint64_t len;
	int success = 0;

	/* Reserve num elements, if available */
	len = rte_atomic_load_explicit(&list->len, rte_memory_order_relaxed);

	while (1) {
		/* Does the list contain enough elements? */
		if (unlikely(len < num))
			return NULL;

		/* len is updated on failure */
		if (rte_atomic_compare_exchange_weak_explicit(&list->len,
						&len, len - num,
						rte_memory_order_acquire,
						rte_memory_order_relaxed))
			break;
	}

	/* If a torn read occurs, the CAS will fail and set old_head to the
	 * correct/latest value.
	 */
	old_head = list->head;

	/* Pop num elements */
	do {
		struct rte_stack_lf_head new_head;
		struct rte_stack_lf_elem *tmp;
		unsigned int i;

		/* Use the acquire memmodel to ensure the reads to the LF LIFO
		 * elements are properly ordered with respect to the head
		 * pointer read.
		 */
		rte_atomic_thread_fence(rte_memory_order_acquire);

		rte_prefetch0(old_head.top);

		tmp = old_head.top;

		/* Traverse the list to find the new head. A next pointer will
		 * either point to another element or NULL; if a thread
		 * encounters a pointer that has already been popped, the CAS
		 * will fail.
		 */
		for (i = 0; i < num && tmp != NULL; i++) {
			rte_prefetch0(tmp->next);
			if (obj_table)
				obj_table[i] = tmp->data;
			if (last)
				*last = tmp;
			tmp = tmp->next;
		}

		/* If NULL was encountered, the list was modified while
		 * traversing it. Retry.
		 */
		if (i != num) {
			old_head = list->head;
			continue;
		}

		new_head.top = tmp;
		new_head.cnt = old_head.cnt + 1;

		/*
		 * The CAS should have release semantics to ensure that
		 * items are read before the head is updated.
		 * But this function is internal, and items are read
		 * only when __rte_stack_lf_pop calls this function to
		 * pop items from used list.
		 * Then, those items are pushed to the free list.
		 * Push uses a CAS store-release on head, which makes
		 * sure that items are read before they are pushed to
		 * the free list, without need for a CAS release here.
		 * This CAS could also be used to ensure that the new
		 * length is visible before the head update, but
		 * acquire semantics on the length update is enough.
		 */
		success = rte_atomic128_cmp_exchange(
				(rte_int128_t *)&list->head,
				(rte_int128_t *)&old_head,
				(rte_int128_t *)&new_head,
				0, rte_memory_order_relaxed,
				rte_memory_order_relaxed);
	} while (success == 0);

	return old_head.top;
}

#endif /* _RTE_STACK_LF_C11_H_ */
