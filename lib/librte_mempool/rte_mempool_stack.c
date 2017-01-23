/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
