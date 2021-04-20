/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include "rte_stack.h"

void
rte_stack_std_init(struct rte_stack *s)
{
	rte_spinlock_init(&s->stack_std.lock);
}

ssize_t
rte_stack_std_get_memsize(unsigned int count)
{
	ssize_t sz = sizeof(struct rte_stack);

	sz += RTE_CACHE_LINE_ROUNDUP(count * sizeof(void *));

	/* Add padding to avoid false sharing conflicts caused by
	 * next-line hardware prefetchers.
	 */
	sz += 2 * RTE_CACHE_LINE_SIZE;

	return sz;
}
