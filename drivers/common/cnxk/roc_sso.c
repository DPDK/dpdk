/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define SSO_XAQ_CACHE_CNT (0x7)
#define SSO_XAQ_SLACK	  (16)

/* Private functions. */
int
sso_lf_alloc(struct dev *dev, enum sso_lf_type lf_type, uint16_t nb_lf,
	     void **rsp)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

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
		plt_err("Failed to get free resource count\n");
		rc = -EIO;
		goto exit;
	}

	roc_sso->max_hwgrp = rsrc_cnt->sso;
	roc_sso->max_hws = rsrc_cnt->ssow;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

void
sso_hws_link_modify(uint8_t hws, uintptr_t base, struct plt_bitmap *bmp,
		    uint16_t hwgrp[], uint16_t n, uint16_t enable)
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
			mask[j] = hwgrp[i + j] | enable << 14;
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

uint64_t
roc_sso_ns_to_gw(struct roc_sso *roc_sso, uint64_t ns)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	uint64_t current_us, current_ns, new_ns;
	uintptr_t base;

	base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20);
	current_us = plt_read64(base + SSOW_LF_GWS_NW_TIM);
	/* From HRM, table 14-19:
	 * The SSOW_LF_GWS_NW_TIM[NW_TIM] period is specified in n-1 notation.
	 */
	current_us += 1;

	/* From HRM, table 14-1:
	 * SSOW_LF_GWS_NW_TIM[NW_TIM] specifies the minimum timeout. The SSO
	 * hardware times out a GET_WORK request within 2 usec of the minimum
	 * timeout specified by SSOW_LF_GWS_NW_TIM[NW_TIM].
	 */
	current_us += 2;
	current_ns = current_us * 1E3;
	new_ns = (ns - PLT_MIN(ns, current_ns));
	new_ns = !new_ns ? 1 : new_ns;
	return (new_ns * plt_tsc_hz()) / 1E9;
}

int
roc_sso_hws_link(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[],
		 uint16_t nb_hwgrp)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct sso *sso;
	uintptr_t base;

	sso = roc_sso_to_sso_priv(roc_sso);
	base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | hws << 12);
	sso_hws_link_modify(hws, base, sso->link_map[hws], hwgrp, nb_hwgrp, 1);

	return nb_hwgrp;
}

int
roc_sso_hws_unlink(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[],
		   uint16_t nb_hwgrp)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct sso *sso;
	uintptr_t base;

	sso = roc_sso_to_sso_priv(roc_sso);
	base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | hws << 12);
	sso_hws_link_modify(hws, base, sso->link_map[hws], hwgrp, nb_hwgrp, 0);

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

int
roc_sso_hwgrp_stats_get(struct roc_sso *roc_sso, uint8_t hwgrp,
			struct roc_sso_hwgrp_stats *stats)
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
	xaq->xaq_lmt =
		xaq->nb_xaq - (nb_hwgrp * SSO_XAQ_CACHE_CNT) - SSO_XAQ_SLACK;

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

	rc = sso_hwgrp_init_xaq_aura(dev, &roc_sso->xaq, nb_xae,
				     roc_sso->xae_waes, roc_sso->xaq_buf_size,
				     roc_sso->nb_hwgrp);
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

	mbox_vec_cnt = RVU_PF_INT_VEC_AFPF_MBOX + 1;

	/* Allocating vectors for the first time */
	if (plt_intr_max_intr_get(pci_dev->intr_handle) == 0) {
		npa_vec_cnt = idev->npa_refcnt ? 0 : NPA_LF_INT_VEC_POISON + 1;
		return dev_irq_reconfigure(pci_dev->intr_handle, mbox_vec_cnt + npa_vec_cnt);
	}

	npa_vec_cnt = (dev->npa.pci_dev == pci_dev) ? NPA_LF_INT_VEC_POISON + 1 : 0;

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

	roc_sso->xaq_buf_size = rsp_hwgrp->xaq_buf_size;
	roc_sso->xae_waes = rsp_hwgrp->xaq_wq_entries;
	roc_sso->iue = rsp_hwgrp->in_unit_entries;

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

		nb_tim_lfs = nb_tim_lfs ? PLT_MIN(nb_tim_lfs, free_tim_lfs) : free_tim_lfs;
	}

	/* 2 error interrupt per TIM LF */
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

	if (!roc_sso->nb_hws && !roc_sso->nb_hwgrp)
		return;

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
