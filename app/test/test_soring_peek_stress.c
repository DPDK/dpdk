/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#include "test_soring_stress_impl.h"

static inline uint32_t
_st_ring_dequeue_burst(struct rte_soring *r, void **obj, uint32_t n,
	uint32_t *avail)
{
	uint32_t m;

	m = rte_soring_dequeue_burst_start(r, obj, n, avail);
	if (m != 0)
		rte_soring_dequeue_finish(r, m);
	return m;
}

static inline uint32_t
_st_ring_enqueue_bulk(struct rte_soring *r, void * const *obj, uint32_t n,
	uint32_t *free)
{
	uint32_t m;

	m = rte_soring_enqueue_bulk_start(r, n, free);
	if (m != 0)
		rte_soring_enqueue_finish(r, obj, m);
	return m;
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

static const struct test_case tests[] = {

	{
		.name = "MTHTS_DEQENQ-MT_STG4-PRCS",
		.func = test_sym_mt_hts4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MTHTS_DEQENQ-MT_STG4-AVG",
		.func = test_sym_mt_hts4,
		.wfunc = test_worker_avg,
	},
	{
		.name = "ST_DEQENQ-MT_STG4-PRCS",
		.func = test_stdenq_stage4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "ST_DEQENQ-MT_STG4-AVG",
		.func = test_stdenq_stage4,
		.wfunc = test_worker_avg,
	},
};

const struct test test_soring_peek_stress = {
	.name = "PEEK",
	.nb_case = RTE_DIM(tests),
	.cases = tests,
};
