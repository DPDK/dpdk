/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2019 Intel Corporation
 */

#include <stdio.h>
#include <rte_mempool.h>
#include <rte_stack.h>

static int
stack_alloc(struct rte_mempool *mp)
{
	char name[RTE_STACK_NAMESIZE];
	struct rte_stack *s;
	int ret;

	ret = snprintf(name, sizeof(name),
		       RTE_MEMPOOL_MZ_FORMAT, mp->name);
	if (ret < 0 || ret >= (int)sizeof(name)) {
		rte_errno = ENAMETOOLONG;
		return -rte_errno;
	}

	s = rte_stack_create(name, mp->size, mp->socket_id, 0);
	if (s == NULL)
		return -rte_errno;

	mp->pool_data = s;

	return 0;
}

static int
stack_enqueue(struct rte_mempool *mp, void * const *obj_table,
	      unsigned int n)
{
	struct rte_stack *s = mp->pool_data;

	return rte_stack_push(s, obj_table, n) == 0 ? -ENOBUFS : 0;
}

static int
stack_dequeue(struct rte_mempool *mp, void **obj_table,
	      unsigned int n)
{
	struct rte_stack *s = mp->pool_data;

	return rte_stack_pop(s, obj_table, n) == 0 ? -ENOBUFS : 0;
}

static unsigned
stack_get_count(const struct rte_mempool *mp)
{
	struct rte_stack *s = mp->pool_data;

	return rte_stack_count(s);
}

static void
stack_free(struct rte_mempool *mp)
{
	struct rte_stack *s = mp->pool_data;

	rte_stack_free(s);
}

static struct rte_mempool_ops ops_stack = {
	.name = "stack",
	.alloc = stack_alloc,
	.free = stack_free,
	.enqueue = stack_enqueue,
	.dequeue = stack_dequeue,
	.get_count = stack_get_count
};

MEMPOOL_REGISTER_OPS(ops_stack);
