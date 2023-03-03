/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

uint32_t soft_exp_consumer_cnt;
roc_nix_inl_meta_pool_cb_t meta_pool_cb;

PLT_STATIC_ASSERT(ROC_NIX_INL_ON_IPSEC_INB_SA_SZ ==
		  1UL << ROC_NIX_INL_ON_IPSEC_INB_SA_SZ_LOG2);
PLT_STATIC_ASSERT(ROC_NIX_INL_ON_IPSEC_INB_SA_SZ == 1024);
PLT_STATIC_ASSERT(ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ ==
		  1UL << ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ_LOG2);
PLT_STATIC_ASSERT(ROC_NIX_INL_OT_IPSEC_INB_SA_SZ ==
		  1UL << ROC_NIX_INL_OT_IPSEC_INB_SA_SZ_LOG2);
PLT_STATIC_ASSERT(ROC_NIX_INL_OT_IPSEC_INB_SA_SZ == 1024);
PLT_STATIC_ASSERT(ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ ==
		  1UL << ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ_LOG2);

static int
nix_inl_meta_aura_destroy(struct roc_nix *roc_nix)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct idev_nix_inl_cfg *inl_cfg;
	char mempool_name[24] = {'\0'};
	char *mp_name = NULL;
	uint64_t *meta_aura;
	int rc;

	if (!idev)
		return -EINVAL;

	inl_cfg = &idev->inl_cfg;
	if (roc_nix->local_meta_aura_ena) {
		meta_aura = &roc_nix->meta_aura_handle;
		snprintf(mempool_name, sizeof(mempool_name), "NIX_INL_META_POOL_%d",
			 roc_nix->port_id + 1);
		mp_name = mempool_name;
	} else {
		meta_aura = &inl_cfg->meta_aura;
	}

	/* Destroy existing Meta aura */
	if (*meta_aura) {
		uint64_t avail, limit;

		/* Check if all buffers are back to pool */
		avail = roc_npa_aura_op_available(*meta_aura);
		limit = roc_npa_aura_op_limit_get(*meta_aura);
		if (avail != limit)
			plt_warn("Not all buffers are back to meta pool,"
				 " %" PRIu64 " != %" PRIu64, avail, limit);

		rc = meta_pool_cb(meta_aura, &roc_nix->meta_mempool, 0, 0, true, mp_name);
		if (rc) {
			plt_err("Failed to destroy meta aura, rc=%d", rc);
			return rc;
		}

		if (!roc_nix->local_meta_aura_ena) {
			inl_cfg->meta_aura = 0;
			inl_cfg->buf_sz = 0;
			inl_cfg->nb_bufs = 0;
		} else
			roc_nix->buf_sz = 0;
	}

	return 0;
}

static int
nix_inl_meta_aura_create(struct idev_cfg *idev, struct roc_nix *roc_nix, uint16_t first_skip,
			 uint64_t *meta_aura)
{
	uint64_t mask = BIT_ULL(ROC_NPA_BUF_TYPE_PACKET_IPSEC);
	struct idev_nix_inl_cfg *inl_cfg;
	struct nix_inl_dev *nix_inl_dev;
	int port_id = roc_nix->port_id;
	char mempool_name[24] = {'\0'};
	struct roc_nix_rq *inl_rq;
	uint32_t nb_bufs, buf_sz;
	char *mp_name = NULL;
	uint16_t inl_rq_id;
	uintptr_t mp;
	int rc;

	inl_cfg = &idev->inl_cfg;
	nix_inl_dev = idev->nix_inl_dev;

	if (roc_nix->local_meta_aura_ena) {
		/* Per LF Meta Aura */
		inl_rq_id = nix_inl_dev->nb_rqs > 1 ? port_id : 0;
		inl_rq = &nix_inl_dev->rqs[inl_rq_id];

		nb_bufs = roc_npa_aura_op_limit_get(inl_rq->aura_handle);
		if (inl_rq->spb_ena)
			nb_bufs += roc_npa_aura_op_limit_get(inl_rq->spb_aura_handle);

		/* Override meta buf size from NIX devargs if present */
		if (roc_nix->meta_buf_sz)
			buf_sz = roc_nix->meta_buf_sz;
		else
			buf_sz = first_skip + NIX_INL_META_SIZE;

		/* Create Metapool name */
		snprintf(mempool_name, sizeof(mempool_name), "NIX_INL_META_POOL_%d",
			 roc_nix->port_id + 1);
		mp_name = mempool_name;
	} else {
		/* Global Meta Aura (Aura 0) */
		/* Override meta buf count from devargs if present */
		if (nix_inl_dev && nix_inl_dev->nb_meta_bufs)
			nb_bufs = nix_inl_dev->nb_meta_bufs;
		else
			nb_bufs = roc_npa_buf_type_limit_get(mask);

		/* Override meta buf size from devargs if present */
		if (nix_inl_dev && nix_inl_dev->meta_buf_sz)
			buf_sz = nix_inl_dev->meta_buf_sz;
		else
			buf_sz = first_skip + NIX_INL_META_SIZE;
	}

	/* Allocate meta aura */
	rc = meta_pool_cb(meta_aura, &mp, buf_sz, nb_bufs, false, mp_name);
	if (rc) {
		plt_err("Failed to allocate meta aura, rc=%d", rc);
		return rc;
	}
	roc_nix->meta_mempool = mp;

	if (!roc_nix->local_meta_aura_ena) {
		inl_cfg->buf_sz = buf_sz;
		inl_cfg->nb_bufs = nb_bufs;
	} else
		roc_nix->buf_sz = buf_sz;

	return 0;
}

static int
nix_inl_global_meta_buffer_validate(struct idev_cfg *idev, struct roc_nix_rq *rq)
{
	struct idev_nix_inl_cfg *inl_cfg;
	uint32_t actual, expected;
	uint64_t mask, type_mask;

	inl_cfg = &idev->inl_cfg;
	/* Validate if we have enough meta buffers */
	mask = BIT_ULL(ROC_NPA_BUF_TYPE_PACKET_IPSEC);
	expected = roc_npa_buf_type_limit_get(mask);
	actual = inl_cfg->nb_bufs;

	if (actual < expected) {
		plt_err("Insufficient buffers in meta aura %u < %u (expected)",
			actual, expected);
		return -EIO;
	}

	/* Validate if we have enough space for meta buffer */
	if (rq->first_skip + NIX_INL_META_SIZE > inl_cfg->buf_sz) {
		plt_err("Meta buffer size %u not sufficient to meet RQ first skip %u",
			inl_cfg->buf_sz, rq->first_skip);
		return -EIO;
	}

	/* Validate if we have enough VWQE buffers */
	if (rq->vwqe_ena) {
		actual = roc_npa_aura_op_limit_get(rq->vwqe_aura_handle);

		type_mask = roc_npa_buf_type_mask(rq->vwqe_aura_handle);
		if (type_mask & BIT_ULL(ROC_NPA_BUF_TYPE_VWQE_IPSEC) &&
		    type_mask & BIT_ULL(ROC_NPA_BUF_TYPE_VWQE)) {
			/* VWQE aura shared b/w Inline enabled and non Inline
			 * enabled ports needs enough buffers to store all the
			 * packet buffers, one per vwqe.
			 */
			mask = (BIT_ULL(ROC_NPA_BUF_TYPE_PACKET_IPSEC) |
				BIT_ULL(ROC_NPA_BUF_TYPE_PACKET));
			expected = roc_npa_buf_type_limit_get(mask);

			if (actual < expected) {
				plt_err("VWQE aura shared b/w Inline inbound and non-Inline "
					"ports needs vwqe bufs(%u) minimum of all pkt bufs (%u)",
					actual, expected);
				return -EIO;
			}
		} else {
			/* VWQE aura not shared b/w Inline and non Inline ports have relaxed
			 * requirement of match all the meta buffers.
			 */
			expected = inl_cfg->nb_bufs;

			if (actual < expected) {
				plt_err("VWQE aura not shared b/w Inline inbound and non-Inline "
					"ports needs vwqe bufs(%u) minimum of all meta bufs (%u)",
					actual, expected);
				return -EIO;
			}
		}
	}
	return 0;
}

static int
nix_inl_local_meta_buffer_validate(struct roc_nix *roc_nix, struct roc_nix_rq *rq)
{
	/* Validate if we have enough space for meta buffer */
	if (roc_nix->buf_sz && (rq->first_skip + NIX_INL_META_SIZE > roc_nix->buf_sz)) {
		plt_err("Meta buffer size %u not sufficient to meet RQ first skip %u",
			roc_nix->buf_sz, rq->first_skip);
		return -EIO;
	}

	/* TODO: Validate VWQE buffers */

	return 0;
}

int
roc_nix_inl_meta_aura_check(struct roc_nix *roc_nix, struct roc_nix_rq *rq)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct idev_nix_inl_cfg *inl_cfg;
	bool aura_setup = false;
	uint64_t *meta_aura;
	int rc;

	if (!idev || !meta_pool_cb)
		return -EFAULT;

	inl_cfg = &idev->inl_cfg;

	/* Create meta aura if not present */
	if (roc_nix->local_meta_aura_ena)
		meta_aura = &roc_nix->meta_aura_handle;
	else
		meta_aura = &inl_cfg->meta_aura;

	if (!(*meta_aura)) {
		rc = nix_inl_meta_aura_create(idev, roc_nix, rq->first_skip, meta_aura);
		if (rc)
			return rc;

		aura_setup = true;
	}
	/* Update rq meta aura handle */
	rq->meta_aura_handle = *meta_aura;

	if (roc_nix->local_meta_aura_ena) {
		rc = nix_inl_local_meta_buffer_validate(roc_nix, rq);
		if (rc)
			return rc;

		/* Check for TC config on RQ 0 when local meta aura is used as
		 * inline meta aura creation is delayed.
		 */
		if (aura_setup && nix->rqs[0] && nix->rqs[0]->tc != ROC_NIX_PFC_CLASS_INVALID)
			roc_nix_fc_npa_bp_cfg(roc_nix, roc_nix->meta_aura_handle,
					      true, true, nix->rqs[0]->tc);
	} else {
		rc = nix_inl_global_meta_buffer_validate(idev, rq);
		if (rc)
			return rc;
	}

	return 0;
}

static int
nix_inl_inb_sa_tbl_setup(struct roc_nix *roc_nix)
{
	uint32_t ipsec_in_min_spi = roc_nix->ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi = roc_nix->ipsec_in_max_spi;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_nix_ipsec_cfg cfg;
	uint64_t max_sa, i;
	size_t inb_sa_sz;
	void *sa;
	int rc;

	max_sa = plt_align32pow2(ipsec_in_max_spi - ipsec_in_min_spi + 1);

	/* CN9K SA size is different */
	if (roc_model_is_cn9k())
		inb_sa_sz = ROC_NIX_INL_ON_IPSEC_INB_SA_SZ;
	else
		inb_sa_sz = ROC_NIX_INL_OT_IPSEC_INB_SA_SZ;

	/* Alloc contiguous memory for Inbound SA's */
	nix->inb_sa_sz = inb_sa_sz;
	nix->inb_spi_mask = max_sa - 1;
	nix->inb_sa_base = plt_zmalloc(inb_sa_sz * max_sa,
				       ROC_NIX_INL_SA_BASE_ALIGN);
	if (!nix->inb_sa_base) {
		plt_err("Failed to allocate memory for Inbound SA");
		return -ENOMEM;
	}
	if (roc_model_is_cn10k()) {
		for (i = 0; i < max_sa; i++) {
			sa = ((uint8_t *)nix->inb_sa_base) + (i * inb_sa_sz);
			roc_ot_ipsec_inb_sa_init(sa, true);
		}
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.sa_size = inb_sa_sz;
	cfg.iova = (uintptr_t)nix->inb_sa_base;
	cfg.max_sa = max_sa;
	cfg.tt = SSO_TT_ORDERED;

	/* Setup device specific inb SA table */
	rc = roc_nix_lf_inl_ipsec_cfg(roc_nix, &cfg, true);
	if (rc) {
		plt_err("Failed to setup NIX Inbound SA conf, rc=%d", rc);
		goto free_mem;
	}

	return 0;
free_mem:
	plt_free(nix->inb_sa_base);
	nix->inb_sa_base = NULL;
	return rc;
}

static int
nix_inl_sa_tbl_release(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	rc = roc_nix_lf_inl_ipsec_cfg(roc_nix, NULL, false);
	if (rc) {
		plt_err("Failed to disable Inbound inline ipsec, rc=%d", rc);
		return rc;
	}

	plt_free(nix->inb_sa_base);
	nix->inb_sa_base = NULL;
	return 0;
}

struct roc_cpt_lf *
roc_nix_inl_outb_lf_base_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	/* NIX Inline config needs to be done */
	if (!nix->inl_outb_ena || !nix->cpt_lf_base)
		return NULL;

	return (struct roc_cpt_lf *)nix->cpt_lf_base;
}

uintptr_t
roc_nix_inl_outb_sa_base_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return (uintptr_t)nix->outb_sa_base;
}

uintptr_t
roc_nix_inl_inb_sa_base_get(struct roc_nix *roc_nix, bool inb_inl_dev)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	struct nix *nix = NULL;

	if (idev == NULL)
		return 0;

	if (!inb_inl_dev && roc_nix == NULL)
		return -EINVAL;

	if (roc_nix) {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!nix->inl_inb_ena)
			return 0;
	}

	if (inb_inl_dev) {
		inl_dev = idev->nix_inl_dev;
		/* Return inline dev sa base */
		if (inl_dev)
			return (uintptr_t)inl_dev->inb_sa_base;
		return 0;
	}

	return (uintptr_t)nix->inb_sa_base;
}

uint32_t
roc_nix_inl_inb_spi_range(struct roc_nix *roc_nix, bool inb_inl_dev,
			  uint32_t *min_spi, uint32_t *max_spi)
{
	struct idev_cfg *idev = idev_get_cfg();
	uint32_t min = 0, max = 0, mask = 0;
	struct nix_inl_dev *inl_dev;
	struct nix *nix = NULL;

	if (idev == NULL)
		return 0;

	if (!inb_inl_dev && roc_nix == NULL)
		return -EINVAL;

	inl_dev = idev->nix_inl_dev;
	if (inb_inl_dev) {
		if (inl_dev == NULL)
			goto exit;
		min = inl_dev->ipsec_in_min_spi;
		max = inl_dev->ipsec_in_max_spi;
		mask = inl_dev->inb_spi_mask;
	} else {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!nix->inl_inb_ena)
			goto exit;
		min = roc_nix->ipsec_in_min_spi;
		max = roc_nix->ipsec_in_max_spi;
		mask = nix->inb_spi_mask;
	}
exit:
	if (min_spi)
		*min_spi = min;
	if (max_spi)
		*max_spi = max;
	return mask;
}

uint32_t
roc_nix_inl_inb_sa_sz(struct roc_nix *roc_nix, bool inl_dev_sa)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	struct nix *nix;

	if (idev == NULL)
		return 0;

	if (!inl_dev_sa && roc_nix == NULL)
		return -EINVAL;

	if (roc_nix) {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!inl_dev_sa)
			return nix->inb_sa_sz;
	}

	if (inl_dev_sa) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev)
			return inl_dev->inb_sa_sz;
	}

	return 0;
}

uintptr_t
roc_nix_inl_inb_sa_get(struct roc_nix *roc_nix, bool inb_inl_dev, uint32_t spi)
{
	uint32_t max_spi = 0, min_spi = 0, mask;
	uintptr_t sa_base;
	uint64_t sz;

	sa_base = roc_nix_inl_inb_sa_base_get(roc_nix, inb_inl_dev);
	/* Check if SA base exists */
	if (!sa_base)
		return 0;

	/* Get SA size */
	sz = roc_nix_inl_inb_sa_sz(roc_nix, inb_inl_dev);
	if (!sz)
		return 0;

	if (roc_nix && roc_nix->custom_sa_action)
		return (sa_base + (spi * sz));

	/* Check if SPI is in range */
	mask = roc_nix_inl_inb_spi_range(roc_nix, inb_inl_dev, &min_spi,
					 &max_spi);
	if (spi > max_spi || spi < min_spi)
		plt_nix_dbg("Inbound SA SPI %u not in range (%u..%u)", spi,
			 min_spi, max_spi);

	/* Basic logic of SPI->SA for now */
	return (sa_base + ((spi & mask) * sz));
}

int
roc_nix_reassembly_configure(uint32_t max_wait_time, uint16_t max_frags)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_cpt *roc_cpt;
	struct roc_cpt_rxc_time_cfg cfg;

	if (!idev)
		return -EFAULT;

	PLT_SET_USED(max_frags);
	if (idev == NULL)
		return -ENOTSUP;
	roc_cpt = idev->cpt;
	if (!roc_cpt) {
		plt_err("Cannot support inline inbound, cryptodev not probed");
		return -ENOTSUP;
	}

	cfg.step = (max_wait_time * 1000 / ROC_NIX_INL_REAS_ACTIVE_LIMIT);
	cfg.zombie_limit = ROC_NIX_INL_REAS_ZOMBIE_LIMIT;
	cfg.zombie_thres = ROC_NIX_INL_REAS_ZOMBIE_THRESHOLD;
	cfg.active_limit = ROC_NIX_INL_REAS_ACTIVE_LIMIT;
	cfg.active_thres = ROC_NIX_INL_REAS_ACTIVE_THRESHOLD;

	return roc_cpt_rxc_time_cfg(roc_cpt, &cfg);
}

static int
nix_inl_rq_mask_cfg(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_rq_cpt_field_mask_cfg_req *msk_req;
	struct idev_cfg *idev = idev_get_cfg();
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct idev_nix_inl_cfg *inl_cfg;
	uint64_t aura_handle;
	int rc = -ENOSPC;
	uint32_t buf_sz;
	int i;

	if (!idev)
		goto exit;

	inl_cfg = &idev->inl_cfg;
	msk_req = mbox_alloc_msg_nix_lf_inline_rq_cfg(mbox);
	if (msk_req == NULL)
		goto exit;

	for (i = 0; i < RQ_CTX_MASK_MAX; i++)
		msk_req->rq_ctx_word_mask[i] = 0xFFFFFFFFFFFFFFFF;

	msk_req->rq_set.len_ol3_dis = 1;
	msk_req->rq_set.len_ol4_dis = 1;
	msk_req->rq_set.len_il3_dis = 1;

	msk_req->rq_set.len_il4_dis = 1;
	msk_req->rq_set.csum_ol4_dis = 1;
	msk_req->rq_set.csum_il4_dis = 1;

	msk_req->rq_set.lenerr_dis = 1;
	msk_req->rq_set.port_ol4_dis = 1;
	msk_req->rq_set.port_il4_dis = 1;

	msk_req->rq_set.lpb_drop_ena = 0;
	msk_req->rq_set.spb_drop_ena = 0;
	msk_req->rq_set.xqe_drop_ena = 0;
	msk_req->rq_set.spb_ena = 1;

	msk_req->rq_mask.len_ol3_dis = 0;
	msk_req->rq_mask.len_ol4_dis = 0;
	msk_req->rq_mask.len_il3_dis = 0;

	msk_req->rq_mask.len_il4_dis = 0;
	msk_req->rq_mask.csum_ol4_dis = 0;
	msk_req->rq_mask.csum_il4_dis = 0;

	msk_req->rq_mask.lenerr_dis = 0;
	msk_req->rq_mask.port_ol4_dis = 0;
	msk_req->rq_mask.port_il4_dis = 0;

	msk_req->rq_mask.lpb_drop_ena = 0;
	msk_req->rq_mask.spb_drop_ena = 0;
	msk_req->rq_mask.xqe_drop_ena = 0;
	msk_req->rq_mask.spb_ena = 0;

	if (roc_nix->local_meta_aura_ena) {
		aura_handle = roc_nix->meta_aura_handle;
		buf_sz = roc_nix->buf_sz;
		if (!aura_handle && enable) {
			plt_err("NULL meta aura handle");
			goto exit;
		}
	} else {
		aura_handle = roc_npa_zero_aura_handle();
		buf_sz = inl_cfg->buf_sz;
	}

	msk_req->ipsec_cfg1.spb_cpt_aura = roc_npa_aura_handle_to_aura(aura_handle);
	msk_req->ipsec_cfg1.rq_mask_enable = enable;
	msk_req->ipsec_cfg1.spb_cpt_sizem1 = (buf_sz >> 7) - 1;
	msk_req->ipsec_cfg1.spb_cpt_enable = enable;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_inl_inb_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_cpt *roc_cpt;
	uint16_t opcode;
	uint16_t param1;
	uint16_t param2;
	int rc;

	if (idev == NULL)
		return -ENOTSUP;

	/* Unless we have another mechanism to trigger
	 * onetime Inline config in CPTPF, we cannot
	 * support without CPT being probed.
	 */
	roc_cpt = idev->cpt;
	if (!roc_cpt) {
		plt_err("Cannot support inline inbound, cryptodev not probed");
		return -ENOTSUP;
	}

	if (roc_model_is_cn9k()) {
		param1 = (ROC_ONF_IPSEC_INB_MAX_L2_SZ >> 3) & 0xf;
		param2 = ROC_IE_ON_INB_IKEV2_SINGLE_SA_SUPPORT;
		opcode =
			((ROC_IE_ON_INB_MAX_CTX_LEN << 8) |
			 (ROC_IE_ON_MAJOR_OP_PROCESS_INBOUND_IPSEC | (1 << 6)));
	} else {
		union roc_ot_ipsec_inb_param1 u;

		u.u16 = 0;
		u.s.esp_trailer_disable = 1;
		param1 = u.u16;
		param2 = 0;
		opcode = (ROC_IE_OT_MAJOR_OP_PROCESS_INBOUND_IPSEC | (1 << 6));
	}

	/* Do onetime Inbound Inline config in CPTPF */
	rc = roc_cpt_inline_ipsec_inb_cfg(roc_cpt, param1, param2, opcode);
	if (rc && rc != -EEXIST) {
		plt_err("Failed to setup inbound lf, rc=%d", rc);
		return rc;
	}

	/* Setup Inbound SA table */
	rc = nix_inl_inb_sa_tbl_setup(roc_nix);
	if (rc)
		return rc;

	if (!roc_model_is_cn9k() && !roc_errata_nix_no_meta_aura()) {
		nix->need_meta_aura = true;
		if (!roc_nix->local_meta_aura_ena)
			idev->inl_cfg.refs++;
	}

	nix->inl_inb_ena = true;
	return 0;
}

int
roc_nix_inl_inb_fini(struct roc_nix *roc_nix)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	if (!nix->inl_inb_ena)
		return 0;

	if (!idev)
		return -EFAULT;

	nix->inl_inb_ena = false;
	if (nix->need_meta_aura) {
		nix->need_meta_aura = false;
		if (roc_nix->local_meta_aura_ena) {
			nix_inl_meta_aura_destroy(roc_nix);
		} else {
			idev->inl_cfg.refs--;
			if (!idev->inl_cfg.refs)
				nix_inl_meta_aura_destroy(roc_nix);
		}
	}

	if (roc_feature_nix_has_inl_rq_mask()) {
		rc = nix_inl_rq_mask_cfg(roc_nix, false);
		if (rc) {
			plt_err("Failed to get rq mask rc=%d", rc);
			return rc;
		}
	}

	/* Flush Inbound CTX cache entries */
	roc_nix_cpt_ctx_cache_sync(roc_nix);

	/* Disable Inbound SA */
	return nix_inl_sa_tbl_release(roc_nix);
}

int
roc_nix_inl_outb_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_cpt_lf *lf_base, *lf;
	struct dev *dev = &nix->dev;
	struct msix_offset_rsp *rsp;
	struct nix_inl_dev *inl_dev;
	size_t sa_sz, ring_sz;
	uint16_t sso_pffunc;
	uint8_t eng_grpmask;
	uint64_t blkaddr, i;
	uint64_t *ring_base;
	uint16_t nb_lf;
	void *sa_base;
	int j, rc;
	void *sa;

	if (idev == NULL)
		return -ENOTSUP;

	nb_lf = roc_nix->outb_nb_crypto_qs;
	blkaddr = nix->is_nix1 ? RVU_BLOCK_ADDR_CPT1 : RVU_BLOCK_ADDR_CPT0;

	/* Retrieve inline device if present */
	inl_dev = idev->nix_inl_dev;
	sso_pffunc = inl_dev ? inl_dev->dev.pf_func : idev_sso_pffunc_get();
	/* Use sso_pffunc if explicitly requested */
	if (roc_nix->ipsec_out_sso_pffunc)
		sso_pffunc = idev_sso_pffunc_get();

	if (!sso_pffunc) {
		plt_err("Failed to setup inline outb, need either "
			"inline device or sso device");
		return -ENOTSUP;
	}

	/* Attach CPT LF for outbound */
	rc = cpt_lfs_attach(dev, blkaddr, true, nb_lf);
	if (rc) {
		plt_err("Failed to attach CPT LF for inline outb, rc=%d", rc);
		return rc;
	}

	/* Alloc CPT LF */
	eng_grpmask = (1ULL << ROC_CPT_DFLT_ENG_GRP_SE |
		       1ULL << ROC_CPT_DFLT_ENG_GRP_SE_IE |
		       1ULL << ROC_CPT_DFLT_ENG_GRP_AE);
	rc = cpt_lfs_alloc(dev, eng_grpmask, blkaddr,
			   !roc_nix->ipsec_out_sso_pffunc);
	if (rc) {
		plt_err("Failed to alloc CPT LF resources, rc=%d", rc);
		goto lf_detach;
	}

	/* Get msix offsets */
	rc = cpt_get_msix_offset(dev, &rsp);
	if (rc) {
		plt_err("Failed to get CPT LF msix offset, rc=%d", rc);
		goto lf_free;
	}

	mbox_memcpy(nix->cpt_msixoff,
		    nix->is_nix1 ? rsp->cpt1_lf_msixoff : rsp->cptlf_msixoff,
		    sizeof(nix->cpt_msixoff));

	/* Alloc required num of cpt lfs */
	lf_base = plt_zmalloc(nb_lf * sizeof(struct roc_cpt_lf), 0);
	if (!lf_base) {
		plt_err("Failed to alloc cpt lf memory");
		rc = -ENOMEM;
		goto lf_free;
	}

	/* Initialize CPT LF's */
	for (i = 0; i < nb_lf; i++) {
		lf = &lf_base[i];

		lf->lf_id = i;
		lf->nb_desc = roc_nix->outb_nb_desc;
		lf->dev = &nix->dev;
		lf->msixoff = nix->cpt_msixoff[i];
		lf->pci_dev = nix->pci_dev;

		/* Setup CPT LF instruction queue */
		rc = cpt_lf_init(lf);
		if (rc) {
			plt_err("Failed to initialize CPT LF, rc=%d", rc);
			goto lf_fini;
		}

		/* Associate this CPT LF with NIX PFFUNC */
		rc = cpt_lf_outb_cfg(dev, sso_pffunc, nix->dev.pf_func, i,
				     true);
		if (rc) {
			plt_err("Failed to setup CPT LF->(NIX,SSO) link, rc=%d",
				rc);
			goto lf_fini;
		}

		/* Enable IQ */
		roc_cpt_iq_enable(lf);
	}

	if (!roc_nix->ipsec_out_max_sa)
		goto skip_sa_alloc;

	/* CN9K SA size is different */
	if (roc_model_is_cn9k())
		sa_sz = ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ;
	else
		sa_sz = ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ;
	/* Alloc contiguous memory of outbound SA */
	sa_base = plt_zmalloc(sa_sz * roc_nix->ipsec_out_max_sa,
			      ROC_NIX_INL_SA_BASE_ALIGN);
	if (!sa_base) {
		plt_err("Outbound SA base alloc failed");
		goto lf_fini;
	}
	if (roc_model_is_cn10k()) {
		for (i = 0; i < roc_nix->ipsec_out_max_sa; i++) {
			sa = ((uint8_t *)sa_base) + (i * sa_sz);
			roc_ot_ipsec_outb_sa_init(sa);
		}
	}
	nix->outb_sa_base = sa_base;
	nix->outb_sa_sz = sa_sz;

skip_sa_alloc:

	nix->cpt_lf_base = lf_base;
	nix->nb_cpt_lf = nb_lf;
	nix->outb_err_sso_pffunc = sso_pffunc;
	nix->inl_outb_ena = true;
	nix->outb_se_ring_cnt =
		roc_nix->ipsec_out_max_sa / ROC_IPSEC_ERR_RING_MAX_ENTRY + 1;
	nix->outb_se_ring_base =
		roc_nix->port_id * ROC_NIX_SOFT_EXP_PER_PORT_MAX_RINGS;

	if (inl_dev == NULL || !inl_dev->set_soft_exp_poll) {
		nix->outb_se_ring_cnt = 0;
		return 0;
	}

	/* Allocate memory to be used as a ring buffer to poll for
	 * soft expiry event from ucode
	 */
	ring_sz = (ROC_IPSEC_ERR_RING_MAX_ENTRY + 1) * sizeof(uint64_t);
	ring_base = inl_dev->sa_soft_exp_ring;
	for (i = 0; i < nix->outb_se_ring_cnt; i++) {
		ring_base[nix->outb_se_ring_base + i] =
			PLT_U64_CAST(plt_zmalloc(ring_sz, 0));
		if (!ring_base[nix->outb_se_ring_base + i]) {
			plt_err("Couldn't allocate memory for soft exp ring");
			while (i--)
				plt_free(PLT_PTR_CAST(
					ring_base[nix->outb_se_ring_base + i]));
			rc = -ENOMEM;
			goto lf_fini;
		}
	}

	return 0;

lf_fini:
	for (j = i - 1; j >= 0; j--)
		cpt_lf_fini(&lf_base[j]);
	plt_free(lf_base);
lf_free:
	rc |= cpt_lfs_free(dev);
lf_detach:
	rc |= cpt_lfs_detach(dev);
	return rc;
}

int
roc_nix_inl_outb_fini(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_cpt_lf *lf_base = nix->cpt_lf_base;
	struct idev_cfg *idev = idev_get_cfg();
	struct dev *dev = &nix->dev;
	struct nix_inl_dev *inl_dev;
	uint64_t *ring_base;
	int i, rc, ret = 0;

	if (!nix->inl_outb_ena)
		return 0;

	nix->inl_outb_ena = false;

	/* Cleanup CPT LF instruction queue */
	for (i = 0; i < nix->nb_cpt_lf; i++)
		cpt_lf_fini(&lf_base[i]);

	/* Free LF resources */
	rc = cpt_lfs_free(dev);
	if (rc)
		plt_err("Failed to free CPT LF resources, rc=%d", rc);
	ret |= rc;

	/* Detach LF */
	rc = cpt_lfs_detach(dev);
	if (rc)
		plt_err("Failed to detach CPT LF, rc=%d", rc);

	/* Free LF memory */
	plt_free(lf_base);
	nix->cpt_lf_base = NULL;
	nix->nb_cpt_lf = 0;

	/* Free outbound SA base */
	plt_free(nix->outb_sa_base);
	nix->outb_sa_base = NULL;

	if (idev && idev->nix_inl_dev && nix->outb_se_ring_cnt) {
		inl_dev = idev->nix_inl_dev;
		ring_base = inl_dev->sa_soft_exp_ring;
		ring_base += nix->outb_se_ring_base;

		for (i = 0; i < nix->outb_se_ring_cnt; i++) {
			if (ring_base[i])
				plt_free(PLT_PTR_CAST(ring_base[i]));
		}
	}

	ret |= rc;
	return ret;
}

bool
roc_nix_inl_dev_is_probed(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev == NULL)
		return 0;

	return !!idev->nix_inl_dev;
}

bool
roc_nix_inl_inb_is_enabled(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->inl_inb_ena;
}

bool
roc_nix_inl_outb_is_enabled(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->inl_outb_ena;
}

int
roc_nix_inl_dev_rq_get(struct roc_nix_rq *rq, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(rq->roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	int port_id = rq->roc_nix->port_id;
	struct nix_inl_dev *inl_dev;
	struct roc_nix_rq *inl_rq;
	uint16_t inl_rq_id;
	struct mbox *mbox;
	struct dev *dev;
	int rc;

	if (idev == NULL)
		return 0;

	/* Update meta aura handle in RQ */
	if (nix->need_meta_aura)
		rq->meta_aura_handle = roc_npa_zero_aura_handle();

	inl_dev = idev->nix_inl_dev;
	/* Nothing to do if no inline device */
	if (!inl_dev)
		return 0;

	/* Check if this RQ is already holding reference */
	if (rq->inl_dev_refs)
		return 0;

	inl_rq_id = inl_dev->nb_rqs > 1 ? port_id : 0;
	dev = &inl_dev->dev;
	inl_rq = &inl_dev->rqs[inl_rq_id];

	/* Just take reference if already inited */
	if (inl_rq->inl_dev_refs) {
		inl_rq->inl_dev_refs++;
		rq->inl_dev_refs = 1;
		return 0;
	}
	memset(inl_rq, 0, sizeof(struct roc_nix_rq));

	/* Take RQ pool attributes from the first ethdev RQ */
	inl_rq->qid = inl_rq_id;
	inl_rq->aura_handle = rq->aura_handle;
	inl_rq->first_skip = rq->first_skip;
	inl_rq->later_skip = rq->later_skip;
	inl_rq->lpb_size = rq->lpb_size;
	inl_rq->spb_ena = rq->spb_ena;
	inl_rq->spb_aura_handle = rq->spb_aura_handle;
	inl_rq->spb_size = rq->spb_size;

	if (roc_errata_nix_no_meta_aura()) {
		uint64_t aura_limit =
			roc_npa_aura_op_limit_get(inl_rq->aura_handle);
		uint64_t aura_shift = plt_log2_u32(aura_limit);
		uint64_t aura_drop, drop_pc;

		inl_rq->lpb_drop_ena = true;

		if (aura_shift < 8)
			aura_shift = 0;
		else
			aura_shift = aura_shift - 8;

		/* Set first pass RQ to drop after part of buffers are in
		 * use to avoid metabuf alloc failure. This is needed as long
		 * as we cannot use different aura.
		 */
		drop_pc = inl_dev->lpb_drop_pc;
		aura_drop = ((aura_limit * drop_pc) / 100) >> aura_shift;
		roc_npa_aura_drop_set(inl_rq->aura_handle, aura_drop, true);
	}

	if (roc_errata_nix_no_meta_aura() && inl_rq->spb_ena) {
		uint64_t aura_limit =
			roc_npa_aura_op_limit_get(inl_rq->spb_aura_handle);
		uint64_t aura_shift = plt_log2_u32(aura_limit);
		uint64_t aura_drop, drop_pc;

		inl_rq->spb_drop_ena = true;

		if (aura_shift < 8)
			aura_shift = 0;
		else
			aura_shift = aura_shift - 8;

		/* Set first pass RQ to drop after part of buffers are in
		 * use to avoid metabuf alloc failure. This is needed as long
		 * as we cannot use different aura.
		 */
		drop_pc = inl_dev->spb_drop_pc;
		aura_drop = ((aura_limit * drop_pc) / 100) >> aura_shift;
		roc_npa_aura_drop_set(inl_rq->spb_aura_handle, aura_drop, true);
	}

	/* Enable IPSec */
	inl_rq->ipsech_ena = true;

	inl_rq->flow_tag_width = 20;
	/* Special tag mask */
	inl_rq->tag_mask = rq->tag_mask;
	inl_rq->tt = SSO_TT_ORDERED;
	inl_rq->hwgrp = 0;
	inl_rq->wqe_skip = inl_dev->wqe_skip;
	inl_rq->sso_ena = true;

	/* Prepare and send RQ init mbox */
	mbox = mbox_get(dev->mbox);
	if (roc_model_is_cn9k())
		rc = nix_rq_cn9k_cfg(dev, inl_rq, inl_dev->qints, false, enable);
	else
		rc = nix_rq_cfg(dev, inl_rq, inl_dev->qints, false, enable);
	if (rc) {
		plt_err("Failed to prepare aq_enq msg, rc=%d", rc);
		mbox_put(mbox);
		return rc;
	}

	rc = mbox_process(dev->mbox);
	if (rc) {
		plt_err("Failed to send aq_enq msg, rc=%d", rc);
		mbox_put(mbox);
		return rc;
	}
	mbox_put(mbox);

	/* Check meta aura */
	if (enable && nix->need_meta_aura) {
		rc = roc_nix_inl_meta_aura_check(rq->roc_nix, rq);
		if (rc)
			return rc;
	}

	inl_rq->inl_dev_refs++;
	rq->inl_dev_refs = 1;
	return 0;
}

int
roc_nix_inl_dev_rq_put(struct roc_nix_rq *rq)
{
	struct idev_cfg *idev = idev_get_cfg();
	int port_id = rq->roc_nix->port_id;
	struct nix_inl_dev *inl_dev;
	struct roc_nix_rq *inl_rq;
	uint16_t inl_rq_id;
	struct dev *dev;
	int rc;

	if (idev == NULL)
		return 0;

	rq->meta_aura_handle = 0;
	if (!rq->inl_dev_refs)
		return 0;

	inl_dev = idev->nix_inl_dev;
	/* Inline device should be there if we have ref */
	if (!inl_dev) {
		plt_err("Failed to find inline device with refs");
		return -EFAULT;
	}

	dev = &inl_dev->dev;
	inl_rq_id = inl_dev->nb_rqs > 1 ? port_id : 0;
	inl_rq = &inl_dev->rqs[inl_rq_id];

	rq->inl_dev_refs = 0;
	inl_rq->inl_dev_refs--;
	if (inl_rq->inl_dev_refs)
		return 0;

	/* There are no more references, disable RQ */
	rc = nix_rq_ena_dis(dev, inl_rq, false);
	if (rc)
		plt_err("Failed to disable inline device rq, rc=%d", rc);

	roc_npa_aura_drop_set(inl_rq->aura_handle, 0, false);
	if (inl_rq->spb_ena)
		roc_npa_aura_drop_set(inl_rq->spb_aura_handle, 0, false);

	/* Flush NIX LF for CN10K */
	nix_rq_vwqe_flush(rq, inl_dev->vwqe_interval);

	return rc;
}

int
roc_nix_inl_rq_ena_dis(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_nix_rq *inl_rq = roc_nix_inl_dev_rq(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	int rc;

	if (!idev)
		return -EFAULT;

	if (roc_feature_nix_has_inl_rq_mask()) {
		rc = nix_inl_rq_mask_cfg(roc_nix, true);
		if (rc) {
			plt_err("Failed to get rq mask rc=%d", rc);
			return rc;
		}
	}

	if (nix->inb_inl_dev) {
		if (!inl_rq || !idev->nix_inl_dev)
			return -EFAULT;

		inl_dev = idev->nix_inl_dev;

		rc = nix_rq_ena_dis(&inl_dev->dev, inl_rq, enable);
		if (rc)
			return rc;

		if (enable && nix->need_meta_aura)
			return roc_nix_inl_meta_aura_check(roc_nix, inl_rq);
	}
	return 0;
}

void
roc_nix_inb_mode_set(struct roc_nix *roc_nix, bool use_inl_dev)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	/* Info used by NPC flow rule add */
	nix->inb_inl_dev = use_inl_dev;
}

void
roc_nix_inl_inb_set(struct roc_nix *roc_nix, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();

	if (!idev)
		return;
	/* Need to set here for cases when inbound SA table is
	 * managed outside RoC.
	 */
	nix->inl_inb_ena = ena;

	if (roc_model_is_cn9k() || roc_errata_nix_no_meta_aura())
		return;

	if (ena) {
		nix->need_meta_aura = true;
		if (!roc_nix->local_meta_aura_ena)
			idev->inl_cfg.refs++;
	} else if (nix->need_meta_aura) {
		nix->need_meta_aura = false;
		if (roc_nix->local_meta_aura_ena) {
			nix_inl_meta_aura_destroy(roc_nix);
		} else {
			idev->inl_cfg.refs--;
			if (!idev->inl_cfg.refs)
				nix_inl_meta_aura_destroy(roc_nix);
		}
	}
}

int
roc_nix_inl_outb_soft_exp_poll_switch(struct roc_nix *roc_nix, bool poll)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	uint16_t ring_idx, i;

	if (!idev || !idev->nix_inl_dev)
		return 0;

	inl_dev = idev->nix_inl_dev;

	for (i = 0; i < nix->outb_se_ring_cnt; i++) {
		ring_idx = nix->outb_se_ring_base + i;

		if (poll)
			plt_bitmap_set(inl_dev->soft_exp_ring_bmap, ring_idx);
		else
			plt_bitmap_clear(inl_dev->soft_exp_ring_bmap, ring_idx);
	}

	if (poll)
		soft_exp_consumer_cnt++;
	else
		soft_exp_consumer_cnt--;

	return 0;
}

bool
roc_nix_inb_is_with_inl_dev(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->inb_inl_dev;
}

struct roc_nix_rq *
roc_nix_inl_dev_rq(struct roc_nix *roc_nix)
{
	struct idev_cfg *idev = idev_get_cfg();
	int port_id = roc_nix->port_id;
	struct nix_inl_dev *inl_dev;
	struct roc_nix_rq *inl_rq;
	uint16_t inl_rq_id;

	if (idev != NULL) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev != NULL) {
			inl_rq_id = inl_dev->nb_rqs > 1 ? port_id : 0;
			inl_rq = &inl_dev->rqs[inl_rq_id];
			if (inl_rq->inl_dev_refs)
				return inl_rq;
		}
	}

	return NULL;
}

uint16_t __roc_api
roc_nix_inl_outb_sso_pffunc_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->outb_err_sso_pffunc;
}

int
roc_nix_inl_cb_register(roc_nix_inl_sso_work_cb_t cb, void *args)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	if (idev == NULL)
		return -EIO;

	inl_dev = idev->nix_inl_dev;
	if (!inl_dev)
		return -EIO;

	/* Be silent if registration called with same cb and args */
	if (inl_dev->work_cb == cb && inl_dev->cb_args == args)
		return 0;

	/* Don't allow registration again if registered with different cb */
	if (inl_dev->work_cb)
		return -EBUSY;

	inl_dev->work_cb = cb;
	inl_dev->cb_args = args;
	return 0;
}

int
roc_nix_inl_cb_unregister(roc_nix_inl_sso_work_cb_t cb, void *args)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	if (idev == NULL)
		return -ENOENT;

	inl_dev = idev->nix_inl_dev;
	if (!inl_dev)
		return -ENOENT;

	if (inl_dev->work_cb != cb || inl_dev->cb_args != args)
		return -EINVAL;

	inl_dev->work_cb = NULL;
	inl_dev->cb_args = NULL;
	return 0;
}

int
roc_nix_inl_inb_tag_update(struct roc_nix *roc_nix, uint32_t tag_const,
			   uint8_t tt)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_nix_ipsec_cfg cfg;

	/* Be silent if inline inbound not enabled */
	if (!nix->inl_inb_ena)
		return 0;

	memset(&cfg, 0, sizeof(cfg));
	cfg.sa_size = nix->inb_sa_sz;
	cfg.iova = (uintptr_t)nix->inb_sa_base;
	cfg.max_sa = nix->inb_spi_mask + 1;
	cfg.tt = tt;
	cfg.tag_const = tag_const;

	return roc_nix_lf_inl_ipsec_cfg(roc_nix, &cfg, true);
}

int
roc_nix_inl_sa_sync(struct roc_nix *roc_nix, void *sa, bool inb,
		    enum roc_nix_inl_sa_sync_op op)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct roc_cpt_lf *outb_lf = NULL;
	union cpt_lf_ctx_reload reload;
	union cpt_lf_ctx_flush flush;
	bool get_inl_lf = true;
	uintptr_t rbase;
	struct nix *nix;

	/* Nothing much to do on cn9k */
	if (roc_model_is_cn9k()) {
		plt_atomic_thread_fence(__ATOMIC_ACQ_REL);
		return 0;
	}

	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (!inl_dev && roc_nix == NULL)
		return -EINVAL;

	if (roc_nix) {
		nix = roc_nix_to_nix_priv(roc_nix);
		outb_lf = nix->cpt_lf_base;
		if (inb && !nix->inb_inl_dev)
			get_inl_lf = false;
	}

	if (inb && get_inl_lf) {
		outb_lf = NULL;
		if (inl_dev && inl_dev->attach_cptlf)
			outb_lf = &inl_dev->cpt_lf;
	}

	if (outb_lf) {
		rbase = outb_lf->rbase;

		flush.u = 0;
		reload.u = 0;
		switch (op) {
		case ROC_NIX_INL_SA_OP_FLUSH_INVAL:
			flush.s.inval = 1;
			/* fall through */
		case ROC_NIX_INL_SA_OP_FLUSH:
			flush.s.cptr = ((uintptr_t)sa) >> 7;
			plt_write64(flush.u, rbase + CPT_LF_CTX_FLUSH);
			break;
		case ROC_NIX_INL_SA_OP_RELOAD:
			reload.s.cptr = ((uintptr_t)sa) >> 7;
			plt_write64(reload.u, rbase + CPT_LF_CTX_RELOAD);
			break;
		default:
			return -EINVAL;
		}
		return 0;
	}
	plt_err("Could not get CPT LF for SA sync");
	return -ENOTSUP;
}

int
roc_nix_inl_ctx_write(struct roc_nix *roc_nix, void *sa_dptr, void *sa_cptr,
		      bool inb, uint16_t sa_len)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct roc_cpt_lf *outb_lf = NULL;
	union cpt_lf_ctx_flush flush;
	bool get_inl_lf = true;
	uintptr_t rbase;
	struct nix *nix;
	int rc;

	/* Nothing much to do on cn9k */
	if (roc_model_is_cn9k()) {
		return 0;
	}

	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (!inl_dev && roc_nix == NULL)
		return -EINVAL;

	if (roc_nix) {
		nix = roc_nix_to_nix_priv(roc_nix);
		outb_lf = nix->cpt_lf_base;

		if (inb && !nix->inb_inl_dev)
			get_inl_lf = false;
	}

	if (inb && get_inl_lf) {
		outb_lf = NULL;
		if (inl_dev && inl_dev->attach_cptlf)
			outb_lf = &inl_dev->cpt_lf;
	}

	if (outb_lf) {
		rbase = outb_lf->rbase;
		flush.u = 0;

		rc = roc_cpt_ctx_write(outb_lf, sa_dptr, sa_cptr, sa_len);
		if (rc)
			return rc;
		/* Trigger CTX flush to write dirty data back to DRAM */
		flush.s.cptr = ((uintptr_t)sa_cptr) >> 7;
		plt_write64(flush.u, rbase + CPT_LF_CTX_FLUSH);

		return 0;
	}
	plt_nix_dbg("Could not get CPT LF for CTX write");
	return -ENOTSUP;
}

int
roc_nix_inl_ts_pkind_set(struct roc_nix *roc_nix, bool ts_ena, bool inb_inl_dev)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	void *sa, *sa_base = NULL;
	struct nix *nix = NULL;
	uint16_t max_spi = 0;
	uint32_t rq_refs = 0;
	uint8_t pkind = 0;
	int i;

	if (roc_model_is_cn9k())
		return 0;

	if (!inb_inl_dev && (roc_nix == NULL))
		return -EINVAL;

	if (inb_inl_dev) {
		if ((idev == NULL) || (idev->nix_inl_dev == NULL))
			return 0;
		inl_dev = idev->nix_inl_dev;
	} else {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!nix->inl_inb_ena)
			return 0;
		sa_base = nix->inb_sa_base;
		max_spi = roc_nix->ipsec_in_max_spi;
	}

	if (inl_dev) {
		for (i = 0; i < inl_dev->nb_rqs; i++)
			rq_refs += inl_dev->rqs[i].inl_dev_refs;

		if (rq_refs == 0) {
			inl_dev->ts_ena = ts_ena;
			max_spi = inl_dev->ipsec_in_max_spi;
			sa_base = inl_dev->inb_sa_base;
		} else if (inl_dev->ts_ena != ts_ena) {
			if (inl_dev->ts_ena)
				plt_err("Inline device is already configured with TS enable");
			else
				plt_err("Inline device is already configured with TS disable");
			return -ENOTSUP;
		} else {
			return 0;
		}
	}

	pkind = ts_ena ? ROC_IE_OT_CPT_TS_PKIND : ROC_IE_OT_CPT_PKIND;

	sa = (uint8_t *)sa_base;
	if (pkind == ((struct roc_ot_ipsec_inb_sa *)sa)->w0.s.pkind)
		return 0;

	for (i = 0; i < max_spi; i++) {
		sa = ((uint8_t *)sa_base) +
		     (i * ROC_NIX_INL_OT_IPSEC_INB_SA_SZ);
		((struct roc_ot_ipsec_inb_sa *)sa)->w0.s.pkind = pkind;
	}
	return 0;
}

void
roc_nix_inl_dev_lock(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		plt_spinlock_lock(&idev->nix_inl_dev_lock);
}

void
roc_nix_inl_dev_unlock(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		plt_spinlock_unlock(&idev->nix_inl_dev_lock);
}

void
roc_nix_inl_meta_pool_cb_register(roc_nix_inl_meta_pool_cb_t cb)
{
	meta_pool_cb = cb;
}
