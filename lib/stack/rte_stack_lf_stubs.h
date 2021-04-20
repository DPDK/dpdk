/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_STACK_LF_STUBS_H_
#define _RTE_STACK_LF_STUBS_H_

#include <rte_common.h>

static __rte_always_inline unsigned int
__rte_stack_lf_count(struct rte_stack *s)
{
	RTE_SET_USED(s);

	return 0;
}

static __rte_always_inline void
__rte_stack_lf_push_elems(struct rte_stack_lf_list *list,
			  struct rte_stack_lf_elem *first,
			  struct rte_stack_lf_elem *last,
			  unsigned int num)
{
	RTE_SET_USED(first);
	RTE_SET_USED(last);
	RTE_SET_USED(list);
	RTE_SET_USED(num);
}

static __rte_always_inline struct rte_stack_lf_elem *
__rte_stack_lf_pop_elems(struct rte_stack_lf_list *list,
			 unsigned int num,
			 void **obj_table,
			 struct rte_stack_lf_elem **last)
{
	RTE_SET_USED(obj_table);
	RTE_SET_USED(last);
	RTE_SET_USED(list);
	RTE_SET_USED(num);

	return NULL;
}

#endif /* _RTE_STACK_LF_STUBS_H_ */
