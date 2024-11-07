/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#define COMP_ID TPM

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cfa_util.h"
#include "cfa_tpm_priv.h"
#include "cfa_tpm.h"
#include "cfa_trace.h"

static uint32_t cfa_tpm_size(uint16_t max_pools)
{
	return sizeof(struct cfa_tpm) + BITALLOC_SIZEOF(max_pools) +
		max_pools * sizeof(uint16_t);
}

int cfa_tpm_query(uint16_t max_pools, uint32_t *tpm_db_size)
{
	if (tpm_db_size == NULL) {
		CFA_LOG_ERR("tpm_db_size = %p\n", tpm_db_size);
		return -EINVAL;
	}

	if (!CFA_CHECK_BOUNDS(max_pools, CFA_TPM_MIN_POOLS,
			      CFA_TPM_MAX_POOLS)) {
		CFA_LOG_ERR("max_pools = %d\n", max_pools);
		return -EINVAL;
	}

	*tpm_db_size = cfa_tpm_size(max_pools);

	return 0;
}

int cfa_tpm_open(void *tpm, uint32_t tpm_db_size, uint16_t max_pools)
{
	int i;
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL) {
		CFA_LOG_ERR("tpm = %p\n", tpm);
		return -EINVAL;
	}

	if (!(CFA_CHECK_BOUNDS(max_pools, CFA_TPM_MIN_POOLS,
			       CFA_TPM_MAX_POOLS) &&
	      tpm_db_size >= cfa_tpm_size(max_pools))) {
		CFA_LOG_ERR("max_pools = %d tpm_db_size = %d\n", max_pools,
			    tpm_db_size);
		return -EINVAL;
	}

	memset(tpm, 0, tpm_db_size);

	ctx->signature = CFA_TPM_SIGNATURE;
	ctx->max_pools = max_pools;
	ctx->pool_ba = (struct bitalloc *)(ctx + 1);
	ctx->fid_tbl = (uint16_t *)((uint8_t *)ctx->pool_ba +
				    BITALLOC_SIZEOF(max_pools));

	if (ba_init(ctx->pool_ba, max_pools, true))
		return -EINVAL;

	for (i = 0; i < max_pools; i++)
		ctx->fid_tbl[i] = CFA_INVALID_FID;

	return 0;
}

int cfa_tpm_close(void *tpm)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p\n", tpm);
		return -EINVAL;
	}

	memset(tpm, 0, cfa_tpm_size(ctx->max_pools));

	return 0;
}

int cfa_tpm_alloc(void *tpm, uint16_t *pool_id)
{
	int rc;
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || pool_id == NULL ||
	    ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p, pool_id = %p\n", tpm, pool_id);
		return -EINVAL;
	}

	rc = ba_alloc(ctx->pool_ba);

	if (rc < 0)
		return -ENOMEM;

	*pool_id = rc;

	ctx->fid_tbl[rc] = CFA_INVALID_FID;

	return 0;
}

int cfa_tpm_free(void *tpm, uint16_t pool_id)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] != CFA_INVALID_FID) {
		CFA_LOG_ERR("A function (%d) is still using the pool (%d)\n",
			    ctx->fid_tbl[pool_id], pool_id);
		return -EINVAL;
	}

	return ba_free(ctx->pool_ba, pool_id);
}

int cfa_tpm_fid_add(void *tpm, uint16_t pool_id, uint16_t fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (!ba_inuse(ctx->pool_ba, pool_id)) {
		CFA_LOG_ERR("Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] != CFA_INVALID_FID &&
	    ctx->fid_tbl[pool_id] != fid) {
		CFA_LOG_ERR("A function id %d was already set to the pool %d\n",
			    fid, ctx->fid_tbl[pool_id]);
		return -EINVAL;
	}

	ctx->fid_tbl[pool_id] = fid;

	return 0;
}

int cfa_tpm_fid_rem(void *tpm, uint16_t pool_id, uint16_t fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p, pool_id = %d\n", tpm, pool_id);
		return -EINVAL;
	}

	if (!ba_inuse(ctx->pool_ba, pool_id)) {
		CFA_LOG_ERR("Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] == CFA_INVALID_FID ||
	    ctx->fid_tbl[pool_id] != fid) {
		CFA_LOG_ERR("The function id %d was not set to the pool %d\n",
			    fid, pool_id);
		return -EINVAL;
	}

	ctx->fid_tbl[pool_id] = CFA_INVALID_FID;

	return 0;
}

int cfa_tpm_srch_by_pool(void *tpm, uint16_t pool_id, uint16_t *fid)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE || fid == NULL ||
	    pool_id >= ctx->max_pools) {
		CFA_LOG_ERR("tpm = %p, pool_id = %d, fid = %p\n", tpm, pool_id,
			    fid);
		return -EINVAL;
	}

	if (!ba_inuse(ctx->pool_ba, pool_id)) {
		CFA_LOG_ERR("Pool id (%d) was not allocated\n", pool_id);
		return -EINVAL;
	}

	if (ctx->fid_tbl[pool_id] == CFA_INVALID_FID) {
		CFA_LOG_ERR("A function id was not set to the pool (%d)\n",
			    pool_id);
		return -EINVAL;
	}

	*fid = ctx->fid_tbl[pool_id];

	return 0;
}

int cfa_tpm_srchm_by_fid(void *tpm, enum cfa_srch_mode srch_mode, uint16_t fid,
			 uint16_t *pool_id)
{
	uint16_t i;
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE ||
	    pool_id == NULL) {
		CFA_LOG_ERR("tpm = %p, pool_id = %p fid = %d\n", tpm, pool_id,
			    fid);
		return -EINVAL;
	}

	if (srch_mode == CFA_SRCH_MODE_FIRST)
		ctx->next_index = 0;

	for (i = ctx->next_index; i < ctx->max_pools; i++) {
		if (ctx->fid_tbl[i] == fid) {
			ctx->next_index = i + 1;
			*pool_id = i;
			return 0;
		}
	}

	ctx->next_index = ctx->max_pools;

	return -ENOENT;
}

int cfa_tpm_pool_size_set(void *tpm, uint8_t pool_sz_exp)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE) {
		CFA_LOG_ERR("tpm = %p\n", tpm);
		return -EINVAL;
	}

	ctx->pool_sz_exp = pool_sz_exp;

	return 0;
}

int cfa_tpm_pool_size_get(void *tpm, uint8_t *pool_sz_exp)
{
	struct cfa_tpm *ctx = (struct cfa_tpm *)tpm;

	if (tpm == NULL || ctx->signature != CFA_TPM_SIGNATURE ||
	    pool_sz_exp == NULL) {
		CFA_LOG_ERR("tpm = %p, pool_sz_exp = %p\n", tpm, pool_sz_exp);
		return -EINVAL;
	}

	*pool_sz_exp = ctx->pool_sz_exp;

	return 0;
}
