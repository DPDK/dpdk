/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "test_ring_stress_impl.h"
#include <rte_ring_elem.h>

static inline uint32_t
_st_ring_dequeue_bulk(struct rte_ring *r, void **obj, uint32_t n,
	uint32_t *avail)
{
	uint32_t m;

	static rte_spinlock_t lck = RTE_SPINLOCK_INITIALIZER;

	rte_spinlock_lock(&lck);

	m = rte_ring_dequeue_bulk_start(r, obj, n, avail);
	n = (m == n) ? n : 0;
	rte_ring_dequeue_finish(r, n);

	rte_spinlock_unlock(&lck);
	return n;
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_ring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	uint32_t m;

	static rte_spinlock_t lck = RTE_SPINLOCK_INITIALIZER;

	rte_spinlock_lock(&lck);

	m = rte_ring_enqueue_bulk_start(r, n, free);
	n = (m == n) ? n : 0;
	rte_ring_enqueue_finish(r, obj, n);

	rte_spinlock_unlock(&lck);
	return n;
}

static int
_st_ring_init(struct rte_ring *r, const char *name, uint32_t num)
{
	return rte_ring_init(r, name, num, RING_F_SP_ENQ | RING_F_SC_DEQ);
}

const struct test test_ring_st_peek_stress = {
	.name = "ST_PEEK",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
