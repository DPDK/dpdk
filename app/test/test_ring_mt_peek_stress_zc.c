/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 */

#include "test_ring.h"
#include "test_ring_stress_impl.h"
#include <rte_ring_elem.h>

static inline uint32_t
_st_ring_dequeue_bulk(struct rte_ring *r, void **obj, uint32_t n,
	uint32_t *avail)
{
	uint32_t m;
	struct rte_ring_zc_data zcd;

	m = rte_ring_dequeue_zc_bulk_start(r, n, &zcd, avail);
	if (m != 0) {
		/* Copy the data from the ring */
		test_ring_copy_from(&zcd, obj, -1, n);
		rte_ring_dequeue_zc_finish(r, n);
	}

	return n;
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_ring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	uint32_t m;
	struct rte_ring_zc_data zcd;

	m = rte_ring_enqueue_zc_bulk_start(r, n, &zcd, free);
	if (m != 0) {
		/* Copy the data from the ring */
		test_ring_copy_to(&zcd, obj, -1, n);
		rte_ring_enqueue_zc_finish(r, n);
	}

	return n;
}

static int
_st_ring_init(struct rte_ring *r, const char *name, uint32_t num)
{
	return rte_ring_init(r, name, num,
		RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ);
}

const struct test test_ring_mt_peek_stress_zc = {
	.name = "MT_PEEK_ZC",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
