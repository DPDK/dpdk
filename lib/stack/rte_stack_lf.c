/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include "rte_stack.h"

void
rte_stack_lf_init(struct rte_stack *s, unsigned int count)
{
	struct rte_stack_lf_elem *elems = s->stack_lf.elems;
	unsigned int i;

	for (i = 0; i < count; i++)
		__rte_stack_lf_push_elems(&s->stack_lf.free,
					  &elems[i], &elems[i], 1);
}

ssize_t
rte_stack_lf_get_memsize(unsigned int count)
{
	ssize_t sz = sizeof(struct rte_stack);

	sz += RTE_CACHE_LINE_ROUNDUP(count * sizeof(struct rte_stack_lf_elem));

	/* Add padding to avoid false sharing conflicts caused by
	 * next-line hardware prefetchers.
	 */
	sz += 2 * RTE_CACHE_LINE_SIZE;

	return sz;
}
