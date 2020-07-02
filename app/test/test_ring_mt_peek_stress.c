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

	m = rte_ring_dequeue_bulk_start(r, obj, n, avail);
	n = (m == n) ? n : 0;
	rte_ring_dequeue_finish(r, n);
	return n;
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_ring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	uint32_t m;

	m = rte_ring_enqueue_bulk_start(r, n, free);
	n = (m == n) ? n : 0;
	rte_ring_enqueue_finish(r, obj, n);
	return n;
}

static int
_st_ring_init(struct rte_ring *r, const char *name, uint32_t num)
{
	return rte_ring_init(r, name, num,
		RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ);
}

const struct test test_ring_mt_peek_stress = {
	.name = "MT_PEEK",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
