/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "test_ring_stress_impl.h"

static inline uint32_t
_st_ring_dequeue_bulk(struct rte_ring *r, void **obj, uint32_t n,
	uint32_t *avail)
{
	return rte_ring_mc_dequeue_bulk(r, obj, n, avail);
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_ring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	return rte_ring_mp_enqueue_bulk(r, obj, n, free);
}

static int
_st_ring_init(struct rte_ring *r, const char *name, uint32_t num)
{
	return rte_ring_init(r, name, num, 0);
}

const struct test test_ring_mpmc_stress = {
	.name = "MP/MC",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
