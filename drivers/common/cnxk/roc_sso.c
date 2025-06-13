/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define SSO_XAQ_CACHE_CNT (0x3)
#define SSO_XAQ_RSVD_CNT  (0x4)
#define SSO_XAQ_SLACK	  (16)

/* Private functions. */
int
sso_lf_alloc(struct dev *dev, enum sso_lf_type lf_type, uint16_t nb_lf,
	     void **rsp)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	if (!nb_lf) {
		mbox_put(mbox);
		return 0;
	}

	switch (lf_type) {
	case SSO_LF_TYPE_HWS: {
		struct ssow_lf_alloc_req *req;

		req = mbox_alloc_msg_ssow_lf_alloc(mbox);
		if (req == NULL)
			goto exit;
		req->hws = nb_lf;
	} break;
	case SSO_LF_TYPE_HWGRP: {
		struct sso_lf_alloc_req *req;

		req = mbox_alloc_msg_sso_lf_alloc(mbox);
		if (req == NULL)
			goto exit;
		req->hwgrps = nb_lf;
	} break;
	default:
		break;
	}

	rc = mbox_process_msg(mbox, rsp);
	if (rc) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
sso_lf_free(struct dev *dev, enum sso_lf_type lf_type, uint16_t nb_lf)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	if (!nb_lf) {
		mbox_put(mbox);
		return 0;
	}

	switch (lf_type) {
	case SSO_LF_TYPE_HWS: {
		struct ssow_lf_free_req *req;

		req = mbox_alloc_msg_ssow_lf_free(mbox);
		if (req == NULL)
			goto exit;
		req->hws = nb_lf;
	} break;
	case SSO_LF_TYPE_HWGRP: {
		struct sso_lf_free_req *req;

		req = mbox_alloc_msg_sso_lf_free(mbox);
		if (req == NULL)
			goto exit;
		req->hwgrps = nb_lf;
	} break;
	default:
		break;
	}

	rc = mbox_process(mbox);
	if (rc) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
sso_rsrc_attach(struct roc_sso *roc_sso, enum sso_lf_type lf_type,
		uint16_t nb_lf)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct rsrc_attach_req *req;
	int rc = -ENOSPC;

	if (!nb_lf) {
		mbox_put(mbox);
		return 0;
	}

	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		goto exit;
	switch (lf_type) {
	case SSO_LF_TYPE_HWS:
		req->ssow = nb_lf;
		break;
	case SSO_LF_TYPE_HWGRP:
		req->sso = nb_lf;
		break;
	default:
		rc = SSO_ERR_PARAM;
		goto exit;
	}

	req->modify = true;
	if (mbox_process(mbox)) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
sso_rsrc_detach(struct roc_sso *roc_sso, enum sso_lf_type lf_type)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct rsrc_detach_req *req;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		goto exit;
	switch (lf_type) {
	case SSO_LF_TYPE_HWS:
		req->ssow = true;
		break;
	case SSO_LF_TYPE_HWGRP:
		req->sso = true;
		break;
	default:
		rc = SSO_ERR_PARAM;
		goto exit;
	}

	req->partial = true;
	if (mbox_process(mbox)) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
sso_rsrc_get(struct roc_sso *roc_sso)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct free_rsrcs_rsp *rsrc_cnt;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = mbox_process_msg(mbox, (void **)&rsrc_cnt);
	if (rc) {
		plt_err("Failed to get free resource count");
		rc = -EIO;
		goto exit;
	}

	roc_sso->max_hwgrp = PLT_MIN(rsrc_cnt->sso, roc_sso->feat.hwgrps_per_pf);
	roc_sso->max_hws = rsrc_cnt->ssow;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
sso_hw_info_get(struct roc_sso *roc_sso)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct sso_hw_info *rsp;
	int rc;

	mbox_alloc_msg_sso_get_hw_info(mbox);
	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc && rc != MBOX_MSG_INVALID) {
		plt_err("Failed to get SSO HW info");
		rc = -EIO;
		goto exit;
	}

	if (rc == MBOX_MSG_INVALID) {
		roc_sso->feat.hwgrps_per_pf = ROC_SSO_MAX_HWGRP_PER_PF;
	} else {
		mbox_memcpy(&roc_sso->feat, &rsp->feat, sizeof(roc_sso->feat));

		if (!roc_sso->feat.hwgrps_per_pf)
			roc_sso->feat.hwgrps_per_pf = ROC_SSO_MAX_HWGRP_PER_PF;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

void
sso_hws_link_modify(uint8_t hws, uintptr_t base, struct plt_bitmap *bmp, uint16_t hwgrp[],
		    uint16_t n, uint8_t set, uint16_t enable)
{
	uint64_t reg;
	int i, j, k;

	i = 0;
	while (n) {
		uint64_t mask[4] = {
			0x8000,
			0x8000,
			0x8000,
			0x8000,
		};

		k = n % 4;
		k = k ? k : 4;
		for (j = 0; j < k; j++) {
			mask[j] = hwgrp[i + j] | (uint32_t)set << 12 | enable << 14;
			if (bmp) {
				enable ? plt_bitmap_set(bmp, hwgrp[i + j]) :
					 plt_bitmap_clear(bmp, hwgrp[i + j]);
			}
			plt_sso_dbg("HWS %d Linked to HWGRP %d", hws,
				    hwgrp[i + j]);
		}

		n -= j;
		i += j;
		reg = mask[0] | mask[1] << 16 | mask[2] << 32 | mask[3] << 48;
		plt_write64(reg, base + SSOW_LF_GWS_GRPMSK_CHG);
	}
}

static int
sso_hws_link_modify_af(struct dev *dev, uint8_t hws, struct plt_bitmap *bmp, uint16_t hwgrp[],
		       uint16_t n, uint8_t set, uint16_t enable)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct ssow_chng_mship *req;
	int rc, i;

	req = mbox_alloc_msg_ssow_chng_mship(mbox);
	if (req == NULL) {
		rc = mbox_process(mbox);
		if (rc) {
			mbox_put(mbox);
			return -EIO;
		}
		req = mbox_alloc_msg_ssow_chng_mship(mbox);
		if (req == NULL) {
			mbox_put(mbox);
			return -ENOSPC;
		}
	}
	req->enable = enable;
	req->set = set;
	req->hws = hws;
	req->nb_hwgrps = n;
	for (i = 0; i < n; i++)
		req->hwgrps[i] = hwgrp[i];
	rc = mbox_process(mbox);
	mbox_put(mbox);
	if (rc == MBOX_MSG_INVALID)
		return rc;
	if (rc)
		return -EIO;

	for (i = 0; i < n; i++)
		enable ? plt_bitmap_set(bmp, hwgrp[i]) :
			 plt_bitmap_clear(bmp, hwgrp[i]);

	return 0;
}

static int
sso_msix_fill(struct roc_sso *roc_sso, uint16_t nb_hws, uint16_t nb_hwgrp)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct msix_offset_rsp *rsp;
	struct dev *dev = &sso->dev;
	int i, rc;

	mbox_alloc_msg_msix_offset(mbox_get(dev->mbox));
	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc) {
		rc = -EIO;
		goto exit;
	}

	for (i = 0; i < nb_hws; i++)
		sso->hws_msix_offset[i] = rsp->ssow_msixoff[i];
	for (i = 0; i < nb_hwgrp; i++)
		sso->hwgrp_msix_offset[i] = rsp->sso_msixoff[i];

	rc = 0;
exit:
	mbox_put(dev->mbox);
	return rc;
}

/* Public Functions. */
uintptr_t
roc_sso_hws_base_get(struct roc_sso *roc_sso, uint8_t hws)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;

	return dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | hws << 12);
}

uintptr_t
roc_sso_hwgrp_base_get(struct roc_sso *roc_sso, uint16_t hwgrp)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;

	return dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 | hwgrp << 12);
}

uint16_t
roc_sso_pf_func_get(void)
{
	return idev_sso_pffunc_get();
}

uint64_t
roc_sso_ns_to_gw(uint64_t base, uint64_t ns)
{
	uint64_t current_us;

	current_us = plt_read64(base + SSOW_LF_GWS_NW_TIM);
	/* From HRM, table 14-19:
	 * The SSOW_LF_GWS_NW_TIM[NW_TIM] period is specified in n-1 notation.
	 */
	current_us += 1;

	/* From HRM, table 14-1:
	 * SSOW_LF_GWS_NW_TIM[NW_TIM] specifies the minimum timeout. The SSO
	 * hardware times out a GET_WORK request within 1 usec of the minimum
	 * timeout specified by SSOW_LF_GWS_NW_TIM[NW_TIM].
	 */
	current_us += 1;
	return PLT_MAX(1UL, (uint64_t)PLT_DIV_CEIL(ns, (current_us * 1E3)));
}

int
roc_sso_hws_link(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[], uint16_t nb_hwgrp,
		 uint8_t set, bool use_mbox)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	uintptr_t base;
	int rc;

	if (!nb_hwgrp)
		return 0;

	if (use_mbox && roc_model_is_cn10k()) {
		rc = sso_hws_link_modify_af(dev, hws, sso->link_map[hws], hwgrp, nb_hwgrp, set, 1);
		if (rc == MBOX_MSG_INVALID)
			goto lf_access;
		if (rc < 0)
			return 0;
		goto done;
	}
lf_access:
	base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | hws << 12);
	sso_hws_link_modify(hws, base, sso->link_map[hws], hwgrp, nb_hwgrp, set, 1);
done:
	return nb_hwgrp;
}

int
roc_sso_hws_unlink(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[],
		   uint16_t nb_hwgrp, uint8_t set, bool use_mbox)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	uintptr_t base;
	int rc;

	if (!nb_hwgrp)
		return 0;

	if (use_mbox && roc_model_is_cn10k()) {
		rc = sso_hws_link_modify_af(dev, hws, sso->link_map[hws], hwgrp, nb_hwgrp, set, 0);
		if (rc == MBOX_MSG_INVALID)
			goto lf_access;
		if (rc < 0)
			return 0;
		goto done;
	}
lf_access:
	base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | hws << 12);
	sso_hws_link_modify(hws, base, sso->link_map[hws], hwgrp, nb_hwgrp, set, 0);
done:
	return nb_hwgrp;
}

int
roc_sso_hws_stats_get(struct roc_sso *roc_sso, uint8_t hws,
		      struct roc_sso_hws_stats *stats)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_hws_stats *req_rsp;
	struct dev *dev = &sso->dev;
	struct mbox *mbox;
	int rc;

	mbox = mbox_get(dev->mbox);
	req_rsp = (struct sso_hws_stats *)mbox_alloc_msg_sso_hws_get_stats(
		mbox);
	if (req_rsp == NULL) {
		rc = mbox_process(mbox);
		if (rc) {
			rc = -EIO;
			goto fail;
		}
		req_rsp = (struct sso_hws_stats *)
			mbox_alloc_msg_sso_hws_get_stats(mbox);
		if (req_rsp == NULL) {
			rc = -ENOSPC;
			goto fail;
		}
	}
	req_rsp->hws = hws;
	rc = mbox_process_msg(mbox, (void **)&req_rsp);
	if (rc) {
		rc = -EIO;
		goto fail;
	}

	stats->arbitration = req_rsp->arbitration;
fail:
	mbox_put(mbox);
	return rc;
}

void
roc_sso_hws_gwc_invalidate(struct roc_sso *roc_sso, uint8_t *hws,
			   uint8_t nb_hws)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct ssow_lf_inv_req *req;
	struct dev *dev = &sso->dev;
	struct mbox *mbox;
	int i;

	if (!nb_hws)
		return;

	mbox = mbox_get(dev->mbox);
	req = mbox_alloc_msg_sso_ws_cache_inv(mbox);
	if (req == NULL) {
		mbox_process(mbox);
		req = mbox_alloc_msg_sso_ws_cache_inv(mbox);
		if (req == NULL) {
			mbox_put(mbox);
			return;
		}
	}
	req->hdr.ver = SSOW_INVAL_SELECTIVE_VER;
	req->nb_hws = nb_hws;
	for (i = 0; i < nb_hws; i++)
		req->hws[i] = hws[i];
	mbox_process(mbox);
	mbox_put(mbox);
}

static void
sso_agq_op_wait(struct roc_sso *roc_sso, uint16_t hwgrp)
{
	uint64_t reg;

	reg = plt_read64(roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_INSTOP);
	while (reg & BIT_ULL(2)) {
		plt_delay_us(100);
		reg = plt_read64(roc_sso_hwgrp_base_get(roc_sso, hwgrp) +
				 SSO_LF_GGRP_AGGR_CTX_INSTOP);
	}
}

int
roc_sso_hwgrp_agq_alloc(struct roc_sso *roc_sso, uint16_t hwgrp, struct roc_sso_agq_data *data,
			uint32_t *agq_id)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_aggr_setconfig *req;
	struct sso_agq_ctx *ctx;
	uint32_t cnt, off;
	struct mbox *mbox;
	uintptr_t ptr;
	uint64_t reg;
	int rc;

	if (sso->agg_mem[hwgrp] == 0) {
		mbox = mbox_get(sso->dev.mbox);
		req = mbox_alloc_msg_sso_aggr_setconfig(mbox);
		if (req == NULL) {
			mbox_process(mbox);
			req = mbox_alloc_msg_sso_aggr_setconfig(mbox);
			if (req == NULL) {
				plt_err("Failed to allocate AGQ config mbox.");
				mbox_put(mbox);
				return -EIO;
			}
		}

		req->hwgrp = hwgrp;
		req->npa_pf_func = idev_npa_pffunc_get();
		rc = mbox_process(mbox);
		if (rc < 0) {
			plt_err("Failed to set HWGRP AGQ config rc=%d", rc);
			mbox_put(mbox);
			return rc;
		}

		mbox_put(mbox);

		sso->agg_mem[hwgrp] =
			(uintptr_t)plt_zmalloc(SSO_AGGR_MIN_CTX * sizeof(struct sso_agq_ctx),
					       roc_model_optimal_align_sz());
		if (sso->agg_mem[hwgrp] == 0)
			return -ENOMEM;
		sso->agg_cnt[hwgrp] = SSO_AGGR_MIN_CTX;
		sso->agg_used[hwgrp] = 0;
		plt_wmb();
		plt_write64(sso->agg_mem[hwgrp],
			    roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_BASE);
		reg = (plt_log2_u32(SSO_AGGR_MIN_CTX) - 6) << 16;
		reg |= (SSO_AGGR_DEF_TMO << 4) | 1;
		plt_write64(reg, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CFG);
	}

	if (sso->agg_cnt[hwgrp] >= SSO_AGGR_MAX_CTX)
		return -ENOSPC;

	if (sso->agg_cnt[hwgrp] == sso->agg_used[hwgrp]) {
		ptr = sso->agg_mem[hwgrp];
		cnt = sso->agg_cnt[hwgrp] << 1;
		sso->agg_mem[hwgrp] = (uintptr_t)plt_zmalloc(cnt * sizeof(struct sso_agq_ctx),
							     roc_model_optimal_align_sz());
		if (sso->agg_mem[hwgrp] == 0) {
			sso->agg_mem[hwgrp] = ptr;
			return -ENOMEM;
		}

		memcpy((void *)sso->agg_mem[hwgrp], (void *)ptr,
		       sso->agg_cnt[hwgrp] * sizeof(struct sso_agq_ctx));
		plt_wmb();
		sso_agq_op_wait(roc_sso, hwgrp);
		/* Base address has changed, evict old entries. */
		plt_write64(sso->agg_mem[hwgrp],
			    roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_BASE);
		reg = plt_read64(roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CFG);
		reg &= ~GENMASK_ULL(19, 16);
		reg |= (uint64_t)(plt_log2_u32(cnt) - 6) << 16;
		plt_write64(reg, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CFG);
		reg = SSO_LF_AGGR_INSTOP_GLOBAL_EVICT << 4;
		plt_write64(reg,
			    roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_INSTOP);
		sso_agq_op_wait(roc_sso, hwgrp);
		plt_free((void *)ptr);

		sso->agg_cnt[hwgrp] = cnt;
		off = sso->agg_used[hwgrp];
	} else {
		ctx = (struct sso_agq_ctx *)sso->agg_mem[hwgrp];
		for (cnt = 0; cnt < sso->agg_cnt[hwgrp]; cnt++) {
			if (!ctx[cnt].ena)
				break;
		}
		if (cnt == sso->agg_cnt[hwgrp])
			return -EINVAL;
		off = cnt;
	}

	ctx = (struct sso_agq_ctx *)sso->agg_mem[hwgrp];
	ctx += off;
	ctx->ena = 1;
	ctx->tt = data->tt;
	ctx->tag = data->tag;
	ctx->swqe_tag = data->stag;
	ctx->cnt_ena = data->cnt_ena;
	ctx->xqe_type = data->xqe_type;
	ctx->vtimewait = data->vwqe_wait_tmo;
	ctx->vwqe_aura = data->vwqe_aura;
	ctx->max_vsize_exp = data->vwqe_max_sz_exp - 2;

	plt_wmb();
	sso->agg_used[hwgrp]++;
	*agq_id = off;

	return 0;
}

void
roc_sso_hwgrp_agq_free(struct roc_sso *roc_sso, uint16_t hwgrp, uint32_t agq_id)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_agq_ctx *ctx;
	uint64_t reg;

	ctx = (struct sso_agq_ctx *)sso->agg_mem[hwgrp];
	ctx += agq_id;

	if (!ctx->ena)
		return;

	reg = SSO_LF_AGGR_INSTOP_FLUSH << 4;
	reg |= (uint64_t)(agq_id << 8);

	plt_write64(reg, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_INSTOP);
	sso_agq_op_wait(roc_sso, hwgrp);

	memset(ctx, 0, sizeof(struct sso_agq_ctx));
	plt_wmb();
	sso->agg_used[hwgrp]--;

	/* Flush the context from CTX Cache */
	reg = SSO_LF_AGGR_INSTOP_EVICT << 4;
	reg |= (uint64_t)(agq_id << 8);

	plt_write64(reg, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_INSTOP);
	sso_agq_op_wait(roc_sso, hwgrp);
}

void
roc_sso_hwgrp_agq_release(struct roc_sso *roc_sso, uint16_t hwgrp)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_aggr_setconfig *req;
	struct sso_agq_ctx *ctx;
	struct mbox *mbox;
	uint32_t cnt;
	int rc;

	if (!roc_sso->feat.eva_present)
		return;

	plt_write64(0, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CFG);
	ctx = (struct sso_agq_ctx *)sso->agg_mem[hwgrp];
	for (cnt = 0; cnt < sso->agg_cnt[hwgrp]; cnt++) {
		if (!ctx[cnt].ena)
			continue;
		roc_sso_hwgrp_agq_free(roc_sso, hwgrp, cnt);
	}

	plt_write64(0, roc_sso_hwgrp_base_get(roc_sso, hwgrp) + SSO_LF_GGRP_AGGR_CTX_BASE);
	plt_free((void *)sso->agg_mem[hwgrp]);
	sso->agg_mem[hwgrp] = 0;
	sso->agg_cnt[hwgrp] = 0;
	sso->agg_used[hwgrp] = 0;

	mbox = mbox_get(sso->dev.mbox);
	req = mbox_alloc_msg_sso_aggr_setconfig(mbox);
	if (req == NULL) {
		mbox_process(mbox);
		req = mbox_alloc_msg_sso_aggr_setconfig(mbox);
		if (req == NULL) {
			plt_err("Failed to allocate AGQ config mbox.");
			mbox_put(mbox);
			return;
		}
	}

	req->hwgrp = hwgrp;
	req->npa_pf_func = 0;
	rc = mbox_process(mbox);
	if (rc < 0)
		plt_err("Failed to set HWGRP AGQ config rc=%d", rc);
	mbox_put(mbox);
}

uint32_t
roc_sso_hwgrp_agq_from_tag(struct roc_sso *roc_sso, uint16_t hwgrp, uint32_t tag_mask,
			   uint8_t xqe_type)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_agq_ctx *ctx;
	uint32_t i;

	plt_rmb();
	ctx = (struct sso_agq_ctx *)sso->agg_mem[hwgrp];
	for (i = 0; i < sso->agg_used[hwgrp]; i++) {
		if (!ctx[i].ena)
			continue;
		if (ctx[i].tag == tag_mask && ctx[i].xqe_type == xqe_type)
			return i;
	}

	return UINT32_MAX;
}

int
roc_sso_hwgrp_stats_get(struct roc_sso *roc_sso, uint16_t hwgrp, struct roc_sso_hwgrp_stats *stats)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_grp_stats *req_rsp;
	struct dev *dev = &sso->dev;
	struct mbox *mbox;
	int rc;

	mbox = mbox_get(dev->mbox);
	req_rsp = (struct sso_grp_stats *)mbox_alloc_msg_sso_grp_get_stats(
		mbox);
	if (req_rsp == NULL) {
		rc = mbox_process(mbox);
		if (rc) {
			rc = -EIO;
			goto fail;
		}
		req_rsp = (struct sso_grp_stats *)
			mbox_alloc_msg_sso_grp_get_stats(mbox);
		if (req_rsp == NULL) {
			rc = -ENOSPC;
			goto fail;
		}
	}
	req_rsp->grp = hwgrp;
	rc = mbox_process_msg(mbox, (void **)&req_rsp);
	if (rc) {
		rc = -EIO;
		goto fail;
	}

	stats->aw_status = req_rsp->aw_status;
	stats->dq_pc = req_rsp->dq_pc;
	stats->ds_pc = req_rsp->ds_pc;
	stats->ext_pc = req_rsp->ext_pc;
	stats->page_cnt = req_rsp->page_cnt;
	stats->ts_pc = req_rsp->ts_pc;
	stats->wa_pc = req_rsp->wa_pc;
	stats->ws_pc = req_rsp->ws_pc;

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_sso_hwgrp_hws_link_status(struct roc_sso *roc_sso, uint8_t hws,
			      uint16_t hwgrp)
{
	struct sso *sso;

	sso = roc_sso_to_sso_priv(roc_sso);
	return plt_bitmap_get(sso->link_map[hws], hwgrp);
}

int
roc_sso_hwgrp_qos_config(struct roc_sso *roc_sso, struct roc_sso_hwgrp_qos *qos, uint16_t nb_qos)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	struct sso_grp_qos_cfg *req;
	struct mbox *mbox;
	int i, rc;

	if (!nb_qos)
		return 0;

	mbox = mbox_get(dev->mbox);
	for (i = 0; i < nb_qos; i++) {
		uint8_t iaq_prcnt = qos[i].iaq_prcnt;
		uint8_t taq_prcnt = qos[i].taq_prcnt;

		req = mbox_alloc_msg_sso_grp_qos_config(mbox);
		if (req == NULL) {
			rc = mbox_process(mbox);
			if (rc) {
				rc = -EIO;
				goto fail;
			}

			req = mbox_alloc_msg_sso_grp_qos_config(mbox);
			if (req == NULL) {
				rc = -ENOSPC;
				goto fail;
			}
		}
		req->grp = qos[i].hwgrp;
		req->iaq_thr = (SSO_HWGRP_IAQ_MAX_THR_MASK *
				(iaq_prcnt ? iaq_prcnt : 100)) /
			       100;
		req->taq_thr = (SSO_HWGRP_TAQ_MAX_THR_MASK *
				(taq_prcnt ? taq_prcnt : 100)) /
			       100;
	}

	rc = mbox_process(mbox);
	if (rc)
		rc = -EIO;
fail:
	mbox_put(mbox);
	return rc;
}

int
sso_hwgrp_init_xaq_aura(struct dev *dev, struct roc_sso_xaq_data *xaq,
			uint32_t nb_xae, uint32_t xae_waes,
			uint32_t xaq_buf_size, uint16_t nb_hwgrp)
{
	struct npa_pool_s pool;
	struct npa_aura_s aura;
	plt_iova_t iova;
	uint32_t i;
	int rc;

	if (xaq->mem != NULL) {
		rc = sso_hwgrp_release_xaq(dev, nb_hwgrp);
		if (rc < 0) {
			plt_err("Failed to release XAQ %d", rc);
			return rc;
		}
		roc_npa_pool_destroy(xaq->aura_handle);
		plt_free(xaq->fc);
		plt_free(xaq->mem);
		memset(xaq, 0, sizeof(struct roc_sso_xaq_data));
	}

	xaq->fc = plt_zmalloc(ROC_ALIGN, ROC_ALIGN);
	if (xaq->fc == NULL) {
		plt_err("Failed to allocate XAQ FC");
		rc = -ENOMEM;
		goto fail;
	}

	xaq->nb_xae = nb_xae;

	/** SSO will reserve up to 0x4 XAQ buffers per group when GetWork engine
	 * is inactive and it might prefetch an additional 0x3 buffers due to
	 * pipelining.
	 */
	xaq->nb_xaq = (SSO_XAQ_CACHE_CNT * nb_hwgrp);
	xaq->nb_xaq += (SSO_XAQ_RSVD_CNT * nb_hwgrp);
	xaq->nb_xaq += PLT_MAX(1 + ((xaq->nb_xae - 1) / xae_waes), xaq->nb_xaq);
	xaq->nb_xaq += SSO_XAQ_SLACK;

	xaq->mem = plt_zmalloc(xaq_buf_size * xaq->nb_xaq, xaq_buf_size);
	if (xaq->mem == NULL) {
		plt_err("Failed to allocate XAQ mem");
		rc = -ENOMEM;
		goto free_fc;
	}

	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;

	memset(&aura, 0, sizeof(aura));
	aura.fc_ena = 1;
	aura.fc_addr = (uint64_t)xaq->fc;
	aura.fc_hyst_bits = 0; /* Store count on all updates */
	rc = roc_npa_pool_create(&xaq->aura_handle, xaq_buf_size, xaq->nb_xaq,
				 &aura, &pool, 0);
	if (rc) {
		plt_err("Failed to create XAQ pool");
		goto npa_fail;
	}

	iova = (uint64_t)xaq->mem;
	for (i = 0; i < xaq->nb_xaq; i++) {
		roc_npa_aura_op_free(xaq->aura_handle, 0, iova);
		iova += xaq_buf_size;
	}
	roc_npa_pool_op_range_set(xaq->aura_handle, (uint64_t)xaq->mem, iova);

	if (roc_npa_aura_op_available_wait(xaq->aura_handle, xaq->nb_xaq, 0) !=
	    xaq->nb_xaq) {
		plt_err("Failed to free all pointers to the pool");
		rc = -ENOMEM;
		goto npa_fill_fail;
	}

	/* When SW does addwork (enqueue) check if there is space in XAQ by
	 * comparing fc_addr above against the xaq_lmt calculated below.
	 * There should be a minimum headroom of 7 XAQs per HWGRP for SSO
	 * to request XAQ to cache them even before enqueue is called.
	 */
	xaq->xaq_lmt = xaq->nb_xaq - (nb_hwgrp * SSO_XAQ_CACHE_CNT) - SSO_XAQ_SLACK;

	return 0;
npa_fill_fail:
	roc_npa_pool_destroy(xaq->aura_handle);
npa_fail:
	plt_free(xaq->mem);
free_fc:
	plt_free(xaq->fc);
fail:
	memset(xaq, 0, sizeof(struct roc_sso_xaq_data));
	return rc;
}

int
roc_sso_hwgrp_init_xaq_aura(struct roc_sso *roc_sso, uint32_t nb_xae)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	int rc;

	rc = sso_hwgrp_init_xaq_aura(dev, &roc_sso->xaq, nb_xae, roc_sso->feat.xaq_wq_entries,
				     roc_sso->feat.xaq_buf_size, roc_sso->nb_hwgrp);
	return rc;
}

int
sso_hwgrp_free_xaq_aura(struct dev *dev, struct roc_sso_xaq_data *xaq,
			uint16_t nb_hwgrp)
{
	int rc;

	if (xaq->mem != NULL) {
		if (nb_hwgrp) {
			rc = sso_hwgrp_release_xaq(dev, nb_hwgrp);
			if (rc < 0) {
				plt_err("Failed to release XAQ %d", rc);
				return rc;
			}
		}
		roc_npa_pool_destroy(xaq->aura_handle);
		plt_free(xaq->fc);
		plt_free(xaq->mem);
	}
	memset(xaq, 0, sizeof(struct roc_sso_xaq_data));

	return 0;
}

int
roc_sso_hwgrp_free_xaq_aura(struct roc_sso *roc_sso, uint16_t nb_hwgrp)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	int rc;

	rc = sso_hwgrp_free_xaq_aura(dev, &roc_sso->xaq, nb_hwgrp);
	return rc;
}

int
sso_hwgrp_alloc_xaq(struct dev *dev, uint32_t npa_aura_id, uint16_t hwgrps)
{
	struct sso_hw_setconfig *req;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	req = mbox_alloc_msg_sso_hw_setconfig(mbox);
	if (req == NULL)
		goto exit;
	req->npa_pf_func = idev_npa_pffunc_get();
	req->npa_aura_id = npa_aura_id;
	req->hwgrps = hwgrps;

	if (mbox_process(dev->mbox)) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_sso_hwgrp_alloc_xaq(struct roc_sso *roc_sso, uint32_t npa_aura_id,
			uint16_t hwgrps)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	int rc;

	rc = sso_hwgrp_alloc_xaq(dev, npa_aura_id, hwgrps);
	return rc;
}

int
sso_hwgrp_release_xaq(struct dev *dev, uint16_t hwgrps)
{
	struct sso_hw_xaq_release *req;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	req = mbox_alloc_msg_sso_hw_release_xaq_aura(mbox);
	if (req == NULL) {
		rc =  -EINVAL;
		goto exit;
	}
	req->hwgrps = hwgrps;

	if (mbox_process(mbox)) {
		rc = -EIO;
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_sso_hwgrp_release_xaq(struct roc_sso *roc_sso, uint16_t hwgrps)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	int rc;

	if (!hwgrps)
		return 0;

	rc = sso_hwgrp_release_xaq(dev, hwgrps);
	return rc;
}

int
roc_sso_hwgrp_set_priority(struct roc_sso *roc_sso, uint16_t hwgrp,
			   uint8_t weight, uint8_t affinity, uint8_t priority)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	struct sso_grp_priority *req;
	struct mbox *mbox;
	int rc = -ENOSPC;

	mbox = mbox_get(dev->mbox);
	req = mbox_alloc_msg_sso_grp_set_priority(mbox);
	if (req == NULL)
		goto fail;
	req->grp = hwgrp;
	req->weight = weight;
	req->affinity = affinity;
	req->priority = priority;

	rc = mbox_process(mbox);
	if (rc) {
		rc = -EIO;
		goto fail;
	}
	mbox_put(mbox);
	plt_sso_dbg("HWGRP %d weight %d affinity %d priority %d", hwgrp, weight,
		    affinity, priority);

	return 0;
fail:
	mbox_put(mbox);
	return rc;
}

static int
sso_update_msix_vec_count(struct roc_sso *roc_sso, uint16_t sso_vec_cnt)
{
	struct plt_pci_device *pci_dev = roc_sso->pci_dev;
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	uint16_t mbox_vec_cnt, npa_vec_cnt;
	struct dev *dev = &sso->dev;
	struct idev_cfg *idev;
	int rc;

	idev = idev_get_cfg();
	if (idev == NULL)
		return -ENODEV;

	if (roc_model_is_cn20k())
		mbox_vec_cnt = RVU_MBOX_PF_INT_VEC_AFPF_MBOX + 1;
	else
		mbox_vec_cnt = RVU_PF_INT_VEC_AFPF_MBOX + 1;

	/* Allocating vectors for the first time */
	if (plt_intr_max_intr_get(pci_dev->intr_handle) == 0) {
		npa_vec_cnt = idev->npa_refcnt ? 0 : NPA_LF_INT_VEC_POISON + 1;
		return dev_irq_reconfigure(pci_dev->intr_handle, mbox_vec_cnt + npa_vec_cnt);
	}

	/* Before re-configuring unregister irqs */
	npa_vec_cnt = (dev->npa.pci_dev == pci_dev) ? NPA_LF_INT_VEC_POISON + 1 : 0;
	if (npa_vec_cnt)
		npa_unregister_irqs(&dev->npa);

	dev_mbox_unregister_irq(pci_dev, dev);
	if (!dev_is_vf(dev))
		dev_vf_flr_unregister_irqs(pci_dev, dev);

	/* Re-configure to include SSO vectors */
	rc = dev_irq_reconfigure(pci_dev->intr_handle, mbox_vec_cnt + npa_vec_cnt + sso_vec_cnt);
	if (rc)
		return rc;

	rc = dev_mbox_register_irq(pci_dev, dev);
	if (rc)
		return rc;

	if (!dev_is_vf(dev)) {
		rc = dev_vf_flr_register_irqs(pci_dev, dev);
		if (rc)
			return rc;
	}

	if (npa_vec_cnt)
		rc = npa_register_irqs(&dev->npa);

	return rc;
}

int
roc_sso_hwgrp_stash_config(struct roc_sso *roc_sso, struct roc_sso_hwgrp_stash *stash,
			   uint16_t nb_stash)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_grp_stash_cfg *req;
	struct dev *dev = &sso->dev;
	struct mbox *mbox;
	int i, rc;

	if (!nb_stash)
		return 0;

	mbox = mbox_get(dev->mbox);
	for (i = 0; i < nb_stash; i++) {
		req = mbox_alloc_msg_sso_grp_stash_config(mbox);
		if (req == NULL) {
			rc = mbox_process(mbox);
			if (rc) {
				rc = -EIO;
				goto fail;
			}

			req = mbox_alloc_msg_sso_grp_stash_config(mbox);
			if (req == NULL) {
				rc = -ENOSPC;
				goto fail;
			}
		}
		req->ena = true;
		req->grp = stash[i].hwgrp;
		req->offset = stash[i].stash_offset;
		req->num_linesm1 = stash[i].stash_count - 1;
	}

	rc = mbox_process(mbox);
	if (rc)
		rc = -EIO;
fail:
	mbox_put(mbox);
	return rc;
}

int
roc_sso_rsrc_init(struct roc_sso *roc_sso, uint8_t nb_hws, uint16_t nb_hwgrp, uint16_t nb_tim_lfs)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct sso_lf_alloc_rsp *rsp_hwgrp;
	uint16_t sso_vec_cnt, free_tim_lfs;
	int rc;

	if (!nb_hwgrp || roc_sso->max_hwgrp < nb_hwgrp)
		return -ENOENT;
	if (!nb_hws || roc_sso->max_hws < nb_hws)
		return -ENOENT;

	rc = sso_rsrc_attach(roc_sso, SSO_LF_TYPE_HWS, nb_hws);
	if (rc < 0) {
		plt_err("Unable to attach SSO HWS LFs");
		goto fail;
	}

	rc = sso_rsrc_attach(roc_sso, SSO_LF_TYPE_HWGRP, nb_hwgrp);
	if (rc < 0) {
		plt_err("Unable to attach SSO HWGRP LFs");
		goto hwgrp_atch_fail;
	}

	rc = sso_lf_alloc(&sso->dev, SSO_LF_TYPE_HWS, nb_hws, NULL);
	if (rc < 0) {
		plt_err("Unable to alloc SSO HWS LFs");
		goto hws_alloc_fail;
	}

	rc = sso_lf_alloc(&sso->dev, SSO_LF_TYPE_HWGRP, nb_hwgrp,
			  (void **)&rsp_hwgrp);
	if (rc < 0) {
		plt_err("Unable to alloc SSO HWGRP Lfs");
		goto hwgrp_alloc_fail;
	}

	if (!roc_sso->feat.xaq_buf_size || !roc_sso->feat.xaq_wq_entries || !roc_sso->feat.iue) {
		roc_sso->feat.xaq_buf_size = rsp_hwgrp->xaq_buf_size;
		roc_sso->feat.xaq_wq_entries = rsp_hwgrp->xaq_wq_entries;
		roc_sso->feat.iue = rsp_hwgrp->in_unit_entries;
	}

	rc = sso_msix_fill(roc_sso, nb_hws, nb_hwgrp);
	if (rc < 0) {
		plt_err("Unable to get MSIX offsets for SSO LFs");
		goto sso_msix_fail;
	}

	/* 1 error interrupt per SSO HWS/HWGRP */
	sso_vec_cnt = nb_hws + nb_hwgrp;

	if (sso->dev.roc_tim) {
		nb_tim_lfs = ((struct roc_tim *)sso->dev.roc_tim)->nb_lfs;
	} else {
		rc = tim_free_lf_count_get(&sso->dev, &free_tim_lfs);
		if (rc < 0) {
			plt_err("Failed to get TIM resource count");
			goto sso_msix_fail;
		}

		nb_tim_lfs = PLT_MIN(nb_tim_lfs, free_tim_lfs);
	}

	/* 2 error interrupt per TIM LF */
	if (roc_model_is_cn20k())
		sso_vec_cnt += 3 * nb_tim_lfs;
	else
		sso_vec_cnt += 2 * nb_tim_lfs;

	rc = sso_update_msix_vec_count(roc_sso, sso_vec_cnt);
	if (rc < 0) {
		plt_err("Failed to update SSO MSIX vector count");
		goto sso_msix_fail;
	}

	rc = sso_register_irqs_priv(roc_sso, sso->pci_dev->intr_handle, nb_hws,
				    nb_hwgrp);
	if (rc < 0) {
		plt_err("Failed to register SSO LF IRQs");
		goto sso_msix_fail;
	}

	roc_sso->nb_hwgrp = nb_hwgrp;
	roc_sso->nb_hws = nb_hws;

	return 0;
sso_msix_fail:
	sso_lf_free(&sso->dev, SSO_LF_TYPE_HWGRP, nb_hwgrp);
hwgrp_alloc_fail:
	sso_lf_free(&sso->dev, SSO_LF_TYPE_HWS, nb_hws);
hws_alloc_fail:
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWGRP);
hwgrp_atch_fail:
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWS);
fail:
	return rc;
}

void
roc_sso_rsrc_fini(struct roc_sso *roc_sso)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	uint32_t cnt;

	if (!roc_sso->nb_hws && !roc_sso->nb_hwgrp)
		return;

	for (cnt = 0; cnt < roc_sso->nb_hwgrp; cnt++)
		roc_sso_hwgrp_agq_release(roc_sso, cnt);

	sso_unregister_irqs_priv(roc_sso, sso->pci_dev->intr_handle,
				 roc_sso->nb_hws, roc_sso->nb_hwgrp);
	sso_lf_free(&sso->dev, SSO_LF_TYPE_HWS, roc_sso->nb_hws);
	sso_lf_free(&sso->dev, SSO_LF_TYPE_HWGRP, roc_sso->nb_hwgrp);

	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWS);
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWGRP);

	roc_sso->nb_hwgrp = 0;
	roc_sso->nb_hws = 0;
}

int
roc_sso_dev_init(struct roc_sso *roc_sso)
{
	struct plt_pci_device *pci_dev;
	uint32_t link_map_sz;
	struct sso *sso;
	void *link_mem;
	int i, rc;

	if (roc_sso == NULL || roc_sso->pci_dev == NULL)
		return SSO_ERR_PARAM;

	PLT_STATIC_ASSERT(sizeof(struct sso) <= ROC_SSO_MEM_SZ);
	sso = roc_sso_to_sso_priv(roc_sso);
	memset(sso, 0, sizeof(*sso));
	pci_dev = roc_sso->pci_dev;

	rc = sso_update_msix_vec_count(roc_sso, 0);
	if (rc < 0) {
		plt_err("Failed to set SSO MSIX vector count");
		return rc;
	}

	rc = dev_init(&sso->dev, pci_dev);
	if (rc < 0) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	rc = sso_hw_info_get(roc_sso);
	if (rc < 0) {
		plt_err("Failed to get SSO HW info");
		goto fail;
	}

	rc = sso_rsrc_get(roc_sso);
	if (rc < 0) {
		plt_err("Failed to get SSO resources");
		goto rsrc_fail;
	}
	rc = -ENOMEM;

	if (roc_sso->max_hws) {
		sso->link_map = plt_zmalloc(
			sizeof(struct plt_bitmap *) * roc_sso->max_hws, 0);
		if (sso->link_map == NULL) {
			plt_err("Failed to allocate memory for link_map array");
			goto rsrc_fail;
		}

		link_map_sz =
			plt_bitmap_get_memory_footprint(roc_sso->max_hwgrp);
		sso->link_map_mem =
			plt_zmalloc(link_map_sz * roc_sso->max_hws, 0);
		if (sso->link_map_mem == NULL) {
			plt_err("Failed to get link_map memory");
			goto rsrc_fail;
		}

		link_mem = sso->link_map_mem;

		for (i = 0; i < roc_sso->max_hws; i++) {
			sso->link_map[i] = plt_bitmap_init(
				roc_sso->max_hwgrp, link_mem, link_map_sz);
			if (sso->link_map[i] == NULL) {
				plt_err("Failed to allocate link map");
				goto link_mem_free;
			}
			link_mem = PLT_PTR_ADD(link_mem, link_map_sz);
		}
	}
	idev_sso_pffunc_set(sso->dev.pf_func);
	idev_sso_set(roc_sso);
	sso->pci_dev = pci_dev;
	sso->dev.drv_inited = true;
	roc_sso->lmt_base = sso->dev.lmt_base;

	return 0;
link_mem_free:
	plt_free(sso->link_map_mem);
rsrc_fail:
	rc |= dev_fini(&sso->dev, pci_dev);
fail:
	return rc;
}

int
roc_sso_dev_fini(struct roc_sso *roc_sso)
{
	struct sso *sso;

	sso = roc_sso_to_sso_priv(roc_sso);
	sso->dev.drv_inited = false;

	return dev_fini(&sso->dev, sso->pci_dev);
}
