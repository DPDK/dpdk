/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2016.
 */

#include "acl_run_altivec.h"

int
rte_acl_classify_altivec(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	if (likely(num >= MAX_SEARCHES_ALTIVEC8))
		return search_altivec_8(ctx, data, results, num, categories);
	else if (num >= MAX_SEARCHES_ALTIVEC4)
		return search_altivec_4(ctx, data, results, num, categories);
	else
		return rte_acl_classify_scalar(ctx, data, results, num,
			categories);
}
