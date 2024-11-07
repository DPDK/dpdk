/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#define COMP_ID TIM

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cfa_util.h"
#include "cfa_types.h"
#include "cfa_tim.h"
#include "cfa_tim_priv.h"
#include "cfa_trace.h"

static uint32_t cfa_tim_size(uint8_t max_tbl_scopes, uint8_t max_regions)
{
	return (sizeof(struct cfa_tim) +
		(max_tbl_scopes * max_regions * CFA_DIR_MAX) * sizeof(void *));
}

int cfa_tim_query(uint8_t max_tbl_scopes, uint8_t max_regions,
		  uint32_t *tim_db_size)
{
	if (tim_db_size == NULL) {
		CFA_LOG_ERR("tim_db_size = %p\n", tim_db_size);
		return -EINVAL;
	}

	*tim_db_size = cfa_tim_size(max_tbl_scopes, max_regions);

	return 0;
}

int cfa_tim_open(void *tim, uint32_t tim_db_size, uint8_t max_tbl_scopes,
		 uint8_t max_regions)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (tim == NULL) {
		CFA_LOG_ERR("tim = %p\n", tim);
		return -EINVAL;
	}
	if (tim_db_size < cfa_tim_size(max_tbl_scopes, max_regions)) {
		CFA_LOG_ERR("max_tbl_scopes = %d, max_regions = %d\n",
			    max_tbl_scopes, max_regions);
		return -EINVAL;
	}

	memset(tim, 0, tim_db_size);

	ctx->signature = CFA_TIM_SIGNATURE;
	ctx->max_tsid = max_tbl_scopes;
	ctx->max_regions = max_regions;
	ctx->tpm_tbl = (void **)(ctx + 1);

	return 0;
}

int cfa_tim_close(void *tim)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (tim == NULL || ctx->signature != CFA_TIM_SIGNATURE) {
		CFA_LOG_ERR("tim = %p\n", tim);
		return -EINVAL;
	}

	memset(tim, 0, cfa_tim_size(ctx->max_tsid, ctx->max_regions));

	return 0;
}

int cfa_tim_tpm_inst_set(void *tim, uint8_t tsid, uint8_t region_id,
			 int dir, void *tpm_inst)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (tim == NULL || ctx->signature != CFA_TIM_SIGNATURE) {
		CFA_LOG_ERR("tim = %p\n", tim);
		return -EINVAL;
	}

	if (!(CFA_CHECK_UPPER_BOUNDS(tsid, ctx->max_tsid - 1) &&
	      CFA_CHECK_UPPER_BOUNDS(region_id, ctx->max_regions - 1))) {
		CFA_LOG_ERR("tsid = %d, region_id = %d\n", tsid, region_id);
		return -EINVAL;
	}

	ctx->tpm_tbl[CFA_TIM_MAKE_INDEX(tsid, region_id, dir, ctx->max_regions, ctx->max_tsid)] =
	    tpm_inst;
	return 0;
}

int cfa_tim_tpm_inst_get(void *tim, uint8_t tsid, uint8_t region_id,
			 int dir, void **tpm_inst)
{
	struct cfa_tim *ctx = (struct cfa_tim *)tim;

	if (tim == NULL || tpm_inst == NULL ||
	    ctx->signature != CFA_TIM_SIGNATURE) {
		CFA_LOG_ERR("tim = %p\n", tim);
		return -EINVAL;
	}

	if (!(CFA_CHECK_UPPER_BOUNDS(tsid, ctx->max_tsid - 1) &&
	      CFA_CHECK_UPPER_BOUNDS(region_id, ctx->max_regions - 1))) {
		CFA_LOG_ERR("tsid = %d, region_id = %d\n", tsid, region_id);
		return -EINVAL;
	}

	*tpm_inst = ctx->tpm_tbl[CFA_TIM_MAKE_INDEX(tsid, region_id, dir,
						    ctx->max_regions, ctx->max_tsid)];
	return 0;
}
