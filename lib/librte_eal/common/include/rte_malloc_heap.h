/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_MALLOC_HEAP_H_
#define _RTE_MALLOC_HEAP_H_

#include <stddef.h>
#include <sys/queue.h>
#include <rte_spinlock.h>
#include <rte_memory.h>

/* Number of free lists per heap, grouped by size. */
#define RTE_HEAP_NUM_FREELISTS  13

/* dummy definition, for pointers */
struct malloc_elem;

/**
 * Structure to hold malloc heap
 */
struct malloc_heap {
	rte_spinlock_t lock;
	LIST_HEAD(, malloc_elem) free_head[RTE_HEAP_NUM_FREELISTS];
	struct malloc_elem *volatile first;
	struct malloc_elem *volatile last;

	unsigned alloc_count;
	size_t total_size;
} __rte_cache_aligned;

#endif /* _RTE_MALLOC_HEAP_H_ */
