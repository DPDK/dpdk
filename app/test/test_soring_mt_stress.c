/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
 */

#include "test_soring_stress_impl.h"

static inline uint32_t
_st_ring_dequeue_burst(struct rte_soring *r, void **obj, uint32_t n,
	uint32_t *avail)
{
	return rte_soring_dequeue_burst(r, obj, n, avail);
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_soring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	return rte_soring_enqueue_bulk(r, obj, n, free);
}

static inline uint32_t
_st_ring_acquire_burst(struct rte_soring *r, uint32_t stage, void **obj,
	uint32_t num, uint32_t *token, uint32_t *avail)
{
	return rte_soring_acquire_burst(r, obj, stage, num, token, avail);
}

static inline void
_st_ring_release(struct rte_soring *r, uint32_t stage, uint32_t token,
	void * const *obj, uint32_t num)
{
	RTE_SET_USED(obj);
	rte_soring_release(r, NULL, stage, num, token);
}

const struct test test_soring_mt_stress = {
	.name = "MT",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
