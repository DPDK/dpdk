/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <stdio.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

struct rte_mempool_stack {
	rte_spinlock_t sl;

	uint32_t size;
	uint32_t len;
	void *objs[];
};

static int
stack_alloc(struct rte_mempool *mp)
{
	struct rte_mempool_stack *s;
	unsigned n = mp->size;
	int size = sizeof(*s) + (n+16)*sizeof(void *);

	/* Allocate our local memory structure */
	s = rte_zmalloc_socket("mempool-stack",
			size,
			RTE_CACHE_LINE_SIZE,
			mp->socket_id);
	if (s == NULL) {
		RTE_LOG(ERR, MEMPOOL, "Cannot allocate stack!\n");
		return -ENOMEM;
	}

	rte_spinlock_init(&s->sl);

	s->size = n;
	mp->pool_data = s;

	return 0;
}

static int
stack_enqueue(struct rte_mempool *mp, void * const *obj_table,
		unsigned n)
{
	struct rte_mempool_stack *s = mp->pool_data;
	void **cache_objs;
	unsigned index;

	rte_spinlock_lock(&s->sl);
	cache_objs = &s->objs[s->len];

	/* Is there sufficient space in the stack ? */
	if ((s->len + n) > s->size) {
		rte_spinlock_unlock(&s->sl);
		return -ENOBUFS;
	}

	/* Add elements back into the cache */
	for (index = 0; index < n; ++index, obj_table++)
		cache_objs[index] = *obj_table;

	s->len += n;

	rte_spinlock_unlock(&s->sl);
	return 0;
}

static int
stack_dequeue(struct rte_mempool *mp, void **obj_table,
		unsigned n)
{
	struct rte_mempool_stack *s = mp->pool_data;
	void **cache_objs;
	unsigned index, len;

	rte_spinlock_lock(&s->sl);

	if (unlikely(n > s->len)) {
		rte_spinlock_unlock(&s->sl);
		return -ENOENT;
	}

	cache_objs = s->objs;

	for (index = 0, len = s->len - 1; index < n;
			++index, len--, obj_table++)
		*obj_table = cache_objs[len];

	s->len -= n;
	rte_spinlock_unlock(&s->sl);
	return 0;
}

static unsigned
stack_get_count(const struct rte_mempool *mp)
{
	struct rte_mempool_stack *s = mp->pool_data;

	return s->len;
}

static void
stack_free(struct rte_mempool *mp)
{
	rte_free((void *)(mp->pool_data));
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
