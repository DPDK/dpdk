/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */


#include "acl_run_avx2.h"

/*
 * Note, that to be able to use AVX2 classify method,
 * both compiler and target cpu have to support AVX2 instructions.
 */
int
rte_acl_classify_avx2(const struct rte_acl_build *build, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	if (likely(num >= MAX_SEARCHES_AVX16))
		return search_avx2x16(build, data, results, num, categories);
	else if (num >= MAX_SEARCHES_SSE8)
		return search_sse_8(build, data, results, num, categories);
	else if (num >= MAX_SEARCHES_SSE4)
		return search_sse_4(build, data, results, num, categories);
	else
		return rte_acl_classify_scalar(build, data, results, num,
			categories);
}
