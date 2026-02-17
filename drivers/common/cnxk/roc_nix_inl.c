/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

uint32_t soft_exp_consumer_cnt;
roc_nix_inl_meta_pool_cb_t meta_pool_cb;
roc_nix_inl_custom_meta_pool_cb_t custom_meta_pool_cb;

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

	if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena) {
		meta_aura = &inl_cfg->meta_aura;
	} else {
		meta_aura = &roc_nix->meta_aura_handle;
		snprintf(mempool_name, sizeof(mempool_name), "NIX_INL_META_POOL_%d",
			 roc_nix->port_id + 1);
		mp_name = mempool_name;
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
			 uint64_t *meta_aura, bool is_local_metaaura)
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

	if (is_local_metaaura) {
		/* Per LF Meta Aura */
		inl_rq_id = nix_inl_dev->nb_rqs > 1 ? port_id : 0;
		inl_rq = &nix_inl_dev->rqs[inl_rq_id];

		nb_bufs = roc_npa_aura_op_limit_get(inl_rq->aura_handle);
		if (inl_rq->spb_ena)
			nb_bufs += roc_npa_aura_op_limit_get(inl_rq->spb_aura_handle);

		/* Override meta buf size from NIX devargs if present */
		if (roc_nix->meta_buf_sz) {
			buf_sz = roc_nix->meta_buf_sz;
		} else {
			buf_sz = NIX_INL_META_SIZE;
			buf_sz += roc_nix->def_first_skip ? roc_nix->def_first_skip : first_skip;
		}

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

	plt_nix_dbg("Created meta aura %p(%s)for port %d", (void *)*meta_aura, mp_name,
		    roc_nix->port_id);

	if (!is_local_metaaura) {
		inl_cfg->buf_sz = buf_sz;
		inl_cfg->nb_bufs = nb_bufs;
		inl_cfg->meta_mempool = mp;
	} else
		roc_nix->buf_sz = buf_sz;

	return 0;
}

static int
nix_inl_custom_meta_aura_destroy(struct roc_nix *roc_nix)
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
	meta_aura = &roc_nix->meta_aura_handle;
	snprintf(mempool_name, sizeof(mempool_name), "NIX_INL_META_POOL_%d",
		 roc_nix->port_id + 1);
	mp_name = mempool_name;

	/* Destroy existing Meta aura */
	if (*meta_aura) {
		uint64_t avail, limit;

		/* Check if all buffers are back to pool */
		avail = roc_npa_aura_op_available(*meta_aura);
		limit = roc_npa_aura_op_limit_get(*meta_aura);
		if (avail != limit)
			plt_warn("Not all buffers are back to meta pool,"
				 " %" PRIu64 " != %" PRIu64, avail, limit);

		rc = custom_meta_pool_cb(inl_cfg->meta_mempool, &roc_nix->meta_mempool, mp_name,
					 meta_aura, 0, 0, true);
		if (rc) {
			plt_err("Failed to destroy meta aura, rc=%d", rc);
			return rc;
		}

		roc_nix->buf_sz = 0;
	}

	return 0;
}

static int
nix_inl_custom_meta_aura_create(struct idev_cfg *idev, struct roc_nix *roc_nix, uint16_t first_skip,
				uint64_t *meta_aura)
{
	uint64_t mask = BIT_ULL(ROC_NPA_BUF_TYPE_PACKET_IPSEC);
	struct idev_nix_inl_cfg *inl_cfg;
	struct nix_inl_dev *nix_inl_dev;
	char mempool_name[24] = {'\0'};
	uint32_t nb_bufs, buf_sz;
	char *mp_name = NULL;
	uintptr_t mp;
	int rc;

	inl_cfg = &idev->inl_cfg;
	nix_inl_dev = idev->nix_inl_dev;

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

	/* Create Metapool name */
	snprintf(mempool_name, sizeof(mempool_name), "NIX_INL_META_POOL_%d",
		 roc_nix->port_id + 1);
	mp_name = mempool_name;

	/* Allocate meta aura */
	rc = custom_meta_pool_cb(inl_cfg->meta_mempool, &mp, mp_name, meta_aura,
				 buf_sz, nb_bufs, false);
	if (rc) {
		plt_err("Failed to allocate meta aura, rc=%d", rc);
		return rc;
	}

	/* Overwrite */
	roc_nix->meta_mempool = mp;
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
	uint32_t buf_sz = NIX_INL_META_SIZE;

	buf_sz += roc_nix->def_first_skip ? roc_nix->def_first_skip : rq->first_skip;

	/* Validate if we have enough space for meta buffer */
	if (roc_nix->buf_sz && (buf_sz > roc_nix->buf_sz)) {
		plt_err("Meta buffer size %u is not sufficient to meet minimum required %u",
			roc_nix->buf_sz, buf_sz);
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
	bool is_local_metaaura;
	bool aura_setup = false;
	uint64_t *meta_aura;
	int rc;

	if (!idev || !meta_pool_cb)
		return -EFAULT;

	inl_cfg = &idev->inl_cfg;

	/* Create meta aura if not present */
	if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena) {
		meta_aura = &inl_cfg->meta_aura;
		is_local_metaaura = false;
	} else {
		meta_aura = &roc_nix->meta_aura_handle;
		is_local_metaaura = true;
	}

	if (!(*meta_aura)) {
		rc = nix_inl_meta_aura_create(idev, roc_nix, rq->first_skip, meta_aura,
					      is_local_metaaura);
		if (rc)
			return rc;

		aura_setup = true;
	}

	if (roc_nix->custom_meta_aura_ena) {
		/* Create metaura for 1:N pool:aura */
		if (!custom_meta_pool_cb)
			return -EFAULT;

		meta_aura = &roc_nix->meta_aura_handle;
		if (!(*meta_aura)) {
			rc = nix_inl_custom_meta_aura_create(idev, roc_nix, rq->first_skip,
							     meta_aura);
			if (rc)
				return rc;

			aura_setup = true;
		}
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
					      true, false, nix->rqs[0]->tc, ROC_NIX_AURA_THRESH);
	} else {
		rc = nix_inl_global_meta_buffer_validate(idev, rq);
		if (rc)
			return rc;
	}

	return 0;
}

static int
nix_inl_inb_ipsec_sa_tbl_setup(struct roc_nix *roc_nix)
{
	uint32_t ipsec_in_min_spi = roc_nix->ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi = roc_nix->ipsec_in_max_spi;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	uint64_t max_sa, i, sa_pow2_sz;
	uint64_t sa_idx_w, lenm1_max;
	uint64_t res_addr_offset;
	uint8_t profile_id = 0;
	struct mbox *mbox;
	size_t inb_sa_sz;
	void *sa;
	int rc;

	/* Setup default IPsec profile */
	if (roc_feature_nix_has_inl_profile()) {
		rc = nix_inl_setup_dflt_ipsec_profile(&nix->dev, &nix->ipsec_prof_id);
		if (rc)
			return rc;
		profile_id = nix->ipsec_prof_id;
	}

	mbox = mbox_get(nix->dev.mbox);

	max_sa = plt_align32pow2(ipsec_in_max_spi - ipsec_in_min_spi + 1);

	/* CN9K SA size is different */
	if (roc_nix->custom_inb_sa)
		inb_sa_sz = ROC_NIX_INL_INB_CUSTOM_SA_SZ;
	else if (roc_model_is_cn9k())
		inb_sa_sz = ROC_NIX_INL_ON_IPSEC_INB_SA_SZ;
	else if (roc_model_is_cn10k())
		inb_sa_sz = ROC_NIX_INL_OT_IPSEC_INB_SA_SZ;
	else
		inb_sa_sz = ROC_NIX_INL_OW_IPSEC_INB_SA_SZ;

	/* Alloc contiguous memory for Inbound SA's */
	nix->inb_sa_sz[profile_id] = inb_sa_sz;
	nix->inb_sa_max[profile_id] = max_sa;
	nix->inb_spi_mask = max_sa - 1;
	nix->inb_sa_base[profile_id] = plt_zmalloc(inb_sa_sz * max_sa, ROC_NIX_INL_SA_BASE_ALIGN);
	if (!nix->inb_sa_base[profile_id]) {
		rc = -ENOMEM;
		plt_err("Failed to allocate memory for Inbound SA");
		goto exit;
	}

	sa_pow2_sz = plt_log2_u32(inb_sa_sz);
	sa_idx_w = plt_log2_u32(max_sa);
	lenm1_max = roc_nix_max_pkt_len(roc_nix) - 1;

	if (!roc_model_is_cn9k()) {
		for (i = 0; i < max_sa; i++) {
			sa = ((uint8_t *)nix->inb_sa_base[profile_id]) + (i * inb_sa_sz);
			if (roc_model_is_cn10k())
				roc_ot_ipsec_inb_sa_init(sa);
			else
				roc_ow_ipsec_inb_sa_init(sa);
		}
	}

	if (roc_model_is_cn9k() || roc_model_is_cn10k()) {
		struct nix_inline_ipsec_lf_cfg *lf_cfg;

		/* Setup device specific inb SA table */
		lf_cfg = mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
		if (lf_cfg == NULL) {
			rc = -ENOSPC;
			plt_err("Failed to alloc nix inline ipsec lf cfg mbox msg");
			goto free_mem;
		}

		lf_cfg->enable = 1;
		lf_cfg->sa_base_addr = (uintptr_t)nix->inb_sa_base[profile_id];
		lf_cfg->ipsec_cfg1.sa_idx_w = sa_idx_w;
		lf_cfg->ipsec_cfg0.lenm1_max = lenm1_max;
		lf_cfg->ipsec_cfg1.sa_idx_max = max_sa - 1;
		lf_cfg->ipsec_cfg0.sa_pow2_size = sa_pow2_sz;
		lf_cfg->ipsec_cfg0.tag_const = 0;
		lf_cfg->ipsec_cfg0.tt = SSO_TT_ORDERED;
		if (roc_nix->res_addr_offset) {
			lf_cfg->ipsec_cfg0_ext.res_addr_offset_valid = 1;
			lf_cfg->ipsec_cfg0_ext.res_addr_offset =
				(roc_nix->res_addr_offset & 0x80) | abs(roc_nix->res_addr_offset);
		}
	} else {
		struct nix_rx_inl_lf_cfg_req *lf_cfg;
		uint64_t def_cptq = 0;

		/* Setup device specific inb SA table */
		lf_cfg = mbox_alloc_msg_nix_rx_inl_lf_cfg(mbox);
		if (lf_cfg == NULL) {
			rc = -ENOSPC;
			plt_err("Failed to alloc nix inline ipsec lf cfg mbox msg");
			goto free_mem;
		}

		/*TODO default cptq */
		if (idev && idev->nix_inl_dev) {
			inl_dev = idev->nix_inl_dev;
			if (!inl_dev->nb_inb_cptlfs)
				def_cptq = 0;
			else
				def_cptq = inl_dev->nix_inb_qids[inl_dev->inb_cpt_lf_id];
		}

		res_addr_offset = (uint64_t)(inl_dev->res_addr_offset & 0xFF) << 48;
		if (res_addr_offset)
			res_addr_offset |= (1UL << 56);

		lf_cfg->enable = 1;
		lf_cfg->profile_id = profile_id; /* IPsec profile is 0th one */
		lf_cfg->rx_inline_sa_base = (uintptr_t)nix->inb_sa_base[profile_id];
		lf_cfg->rx_inline_cfg0 =
			((def_cptq << 57) | res_addr_offset | ((uint64_t)SSO_TT_ORDERED << 44) |
			 (sa_pow2_sz << 16) | lenm1_max);
		lf_cfg->rx_inline_cfg1 = (max_sa - 1) | (sa_idx_w << 32);
	}

	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Failed to setup NIX Inbound SA conf, rc=%d", rc);
		goto free_mem;
	}

	mbox_put(mbox);
	return 0;
free_mem:
	plt_free(nix->inb_sa_base[profile_id]);
	nix->inb_sa_base[profile_id] = NULL;
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_inl_ipsec_sa_tbl_release(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	uint8_t profile_id = 0;
	int rc;

	if (roc_model_is_cn9k() || roc_model_is_cn10k()) {
		struct nix_inline_ipsec_lf_cfg *lf_cfg;

		lf_cfg = mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
		if (lf_cfg == NULL) {
			rc = -ENOSPC;
			goto exit;
		}

		lf_cfg->enable = 0;
	} else {
		struct nix_rx_inl_lf_cfg_req *lf_cfg;

		profile_id = nix->ipsec_prof_id;
		lf_cfg = mbox_alloc_msg_nix_rx_inl_lf_cfg(mbox);
		if (!lf_cfg) {
			rc = -ENOSPC;
			goto exit;
		}

		lf_cfg->enable = 0;
		lf_cfg->profile_id = profile_id; /* IPsec profile is 0th one */
	}

	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Failed to cleanup NIX Inbound SA conf, rc=%d", rc);
		goto exit;
	}

	plt_free(nix->inb_sa_base[profile_id]);
	nix->inb_sa_base[profile_id] = NULL;
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_inl_reass_inb_sa_tbl_setup(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_rx_inl_lf_cfg_req *lf_cfg;
	struct nix_inl_dev *inl_dev = NULL;
	uint64_t max_sa = 1, sa_pow2_sz;
	uint64_t sa_idx_w, lenm1_max;
	uint64_t res_addr_offset = 0;
	uint64_t def_cptq = 0;
	size_t inb_sa_sz = 1;
	uint8_t profile_id;
	struct mbox *mbox;
	void *sa;
	int rc;

	if (!roc_nix->reass_ena)
		return 0;

	rc = nix_inl_setup_reass_profile(&nix->dev, &nix->reass_prof_id);
	if (rc)
		return rc;

	profile_id = nix->reass_prof_id;
	nix->inb_sa_sz[profile_id] = inb_sa_sz;
	nix->inb_sa_max[profile_id] = max_sa;
	nix->inb_spi_mask = 1;
	nix->inb_sa_base[profile_id] = plt_zmalloc(inb_sa_sz * max_sa, ROC_NIX_INL_SA_BASE_ALIGN);
	if (!nix->inb_sa_base[profile_id]) {
		plt_err("Failed to allocate memory for reassembly Inbound SA");
		return -ENOMEM;
	}

	sa = ((uint8_t *)nix->inb_sa_base[profile_id]);
	roc_ow_reass_inb_sa_init(sa);

	mbox = mbox_get(nix->dev.mbox);
	/* Setup device specific inb SA table */
	lf_cfg = mbox_alloc_msg_nix_rx_inl_lf_cfg(mbox);
	if (lf_cfg == NULL) {
		rc = -ENOSPC;
		plt_err("Failed to alloc nix inline reassembly lf cfg mbox msg");
		goto free_mem;
	}

	sa_pow2_sz = plt_log2_u32(inb_sa_sz);
	sa_idx_w = plt_log2_u32(max_sa);
	lenm1_max = roc_nix_max_pkt_len(roc_nix) - 1;

	if (idev && idev->nix_inl_dev) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev->nb_inb_cptlfs)
			def_cptq = inl_dev->nix_inb_qids[inl_dev->inb_cpt_lf_id];
		res_addr_offset = (uint64_t)(inl_dev->res_addr_offset & 0xFF) << 48;
		if (res_addr_offset)
			res_addr_offset |= (1UL << 56);
	}

	lf_cfg->enable = 1;
	lf_cfg->profile_id = profile_id;
	lf_cfg->rx_inline_sa_base = (uintptr_t)nix->inb_sa_base[profile_id];
	lf_cfg->rx_inline_cfg0 =
		((def_cptq << 57) | res_addr_offset | ((uint64_t)SSO_TT_ORDERED << 44) |
		 (sa_pow2_sz << 16) | lenm1_max);
	lf_cfg->rx_inline_cfg1 = (max_sa - 1) | (sa_idx_w << 32);

	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Failed to setup NIX Inbound reassembly SA conf, rc=%d", rc);
		goto free_mem;
	}

	mbox_put(mbox);
	return 0;

free_mem:
	plt_free(nix->inb_sa_base[profile_id]);
	nix->inb_sa_base[profile_id] = NULL;
	mbox_put(mbox);
	return rc;
}

static int
nix_inl_reass_sa_tbl_release(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_rx_inl_lf_cfg_req *lf_cfg;
	uint8_t profile_id;
	int rc;

	if (!roc_nix->reass_ena)
		return 0;

	lf_cfg = mbox_alloc_msg_nix_rx_inl_lf_cfg(mbox);
	if (!lf_cfg) {
		rc = -ENOSPC;
		goto exit;
	}

	profile_id = nix->reass_prof_id;

	lf_cfg->enable = 0;
	lf_cfg->profile_id = profile_id;
	rc = mbox_process(mbox);
	if (rc) {
		plt_err("Failed to cleanup NIX Inbound Reassembly SA conf, rc=%d", rc);
		goto exit;
	}

	plt_free(nix->inb_sa_base[profile_id]);
	nix->inb_sa_base[profile_id] = NULL;
exit:
	mbox_put(mbox);
	return rc;
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

struct roc_cpt_lf *
roc_nix_inl_inb_inj_lf_get(struct roc_nix *roc_nix)
{
	struct nix *nix;
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct roc_cpt_lf *lf = NULL;

	if (!idev)
		return NULL;

	inl_dev = idev->nix_inl_dev;

	if (!inl_dev && roc_nix == NULL)
		return NULL;

	nix = roc_nix_to_nix_priv(roc_nix);

	if (nix->inb_inl_dev && inl_dev && inl_dev->attach_cptlf &&
	    inl_dev->rx_inj_ena)
		return &inl_dev->cpt_lf[inl_dev->nb_cptlf - 1];

	lf = roc_nix_inl_outb_lf_base_get(roc_nix);
	if (lf)
		lf += roc_nix->outb_nb_crypto_qs;
	return lf;
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
			return (uintptr_t)inl_dev->inb_sa_base[inl_dev->ipsec_prof_id];
		return 0;
	}

	return (uintptr_t)nix->inb_sa_base[nix->ipsec_prof_id];
}

uint16_t
roc_nix_inl_inb_ipsec_profile_id_get(struct roc_nix *roc_nix, bool inb_inl_dev)
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
		/* Return inline Ipsec profile ID */
		if (inl_dev)
			return inl_dev->ipsec_prof_id;
		return 0;
	}

	return nix->ipsec_prof_id;
}

uint16_t
roc_nix_inl_inb_reass_profile_id_get(struct roc_nix *roc_nix, bool inb_inl_dev)
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
		/* Return inline reassembly profile ID */
		if (inl_dev)
			return inl_dev->reass_prof_id;
		return 0;
	}

	return nix->reass_prof_id;
}

bool
roc_nix_inl_inb_rx_inject_enable(struct roc_nix *roc_nix, bool inb_inl_dev)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	struct nix *nix = NULL;

	if (idev == NULL)
		return 0;

	if (!inb_inl_dev && roc_nix == NULL)
		return 0;

	if (roc_nix) {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!nix->inl_inb_ena)
			return 0;
	}

	if (inb_inl_dev) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev && inl_dev->attach_cptlf && inl_dev->rx_inj_ena && roc_nix &&
		    roc_nix->rx_inj_ena)
			return true;
	}

	return roc_nix ? roc_nix->rx_inj_ena : 0;
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
			return nix->inb_sa_sz[nix->ipsec_prof_id];
	}

	if (inl_dev_sa) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev)
			return inl_dev->inb_sa_sz[inl_dev->ipsec_prof_id];
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
roc_nix_reassembly_configure(struct roc_cpt_rxc_time_cfg *req_cfg, uint32_t max_wait_time)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct cpt_rxc_time_cfg_req *req;
	struct roc_cpt_rxc_time_cfg cfg;
	struct roc_cpt *roc_cpt;
	struct mbox *mbox;
	int rc;

	if (!idev)
		return -EFAULT;

	cfg.step = req_cfg->step ? req_cfg->step :
				   (max_wait_time * 1000 / ROC_NIX_INL_REAS_ACTIVE_LIMIT);
	cfg.zombie_limit =
		req_cfg->zombie_limit ? req_cfg->zombie_limit : ROC_NIX_INL_REAS_ZOMBIE_LIMIT;
	cfg.zombie_thres =
		req_cfg->zombie_thres ? req_cfg->zombie_thres : ROC_NIX_INL_REAS_ZOMBIE_THRESHOLD;
	cfg.active_limit =
		req_cfg->active_limit ? req_cfg->active_limit : ROC_NIX_INL_REAS_ACTIVE_LIMIT;
	cfg.active_thres =
		req_cfg->active_thres ? req_cfg->active_thres : ROC_NIX_INL_REAS_ACTIVE_THRESHOLD;

	if (roc_model_is_cn10k()) {
		roc_cpt = idev->cpt;
		if (!roc_cpt) {
			plt_err("Cryptodev not probed");
			return -ENOTSUP;
		}
		return roc_cpt_rxc_time_cfg(roc_cpt, &cfg);
	}

	inl_dev = idev->nix_inl_dev;
	if (!inl_dev) {
		plt_err("Cannot support RXC config, inlinedev is not probed");
		return -ENOTSUP;
	}

	mbox = mbox_get((&inl_dev->dev)->mbox);

	req = mbox_alloc_msg_cpt_rxc_time_cfg(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->blkaddr = 0;
	req->queue_id = inl_dev->nix_inb_qids[inl_dev->inb_cpt_lf_id];
	req->step = cfg.step;
	req->zombie_limit = cfg.zombie_limit;
	req->zombie_thres = cfg.zombie_thres;
	req->active_limit = cfg.active_limit;
	req->active_thres = cfg.active_thres;
	req->cpt_af_rxc_que_cfg = ROC_NIX_INL_RXC_QUE_BLK_THR << 32;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

static void
nix_inl_rq_mask_init(struct nix_rq_cpt_field_mask_cfg_req *msk_req, uint8_t first_skip)
{
	int i;

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
	if (first_skip)
		msk_req->rq_set.first_skip = first_skip;

	if (!roc_feature_nix_has_second_pass_drop()) {
		msk_req->rq_set.ena = 1;
		msk_req->rq_set.rq_int_ena = 1;
		msk_req->rq_mask.ena = 0;
		msk_req->rq_mask.rq_int_ena = 0;
	}

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
	if (first_skip)
		msk_req->rq_mask.first_skip = 0;
}

static int
nix_inl_legacy_rq_mask_setup(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_rq_cpt_field_mask_cfg_req *msk_req;
	struct idev_cfg *idev = idev_get_cfg();
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct idev_nix_inl_cfg *inl_cfg;
	uint64_t aura_handle;
	int rc = -ENOSPC;
	uint32_t buf_sz;

	if (!idev)
		goto exit;

	inl_cfg = &idev->inl_cfg;
	msk_req = mbox_alloc_msg_nix_lf_inline_rq_cfg(mbox);
	if (msk_req == NULL)
		goto exit;

	nix_inl_rq_mask_init(msk_req, 0);
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

	msk_req->ipsec_cfg1_inline_replay.spb_cpt_aura = roc_npa_aura_handle_to_aura(aura_handle);
	msk_req->ipsec_cfg1_inline_replay.rq_mask_enable = enable;
	msk_req->ipsec_cfg1_inline_replay.spb_cpt_sizem1 = (buf_sz >> 7) - 1;
	msk_req->ipsec_cfg1_inline_replay.spb_cpt_enable = enable;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_inl_rq_mask_cfg(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_rq_cpt_field_mask_cfg_req *msk_req;
	uint64_t aura_handle;
	uint8_t first_skip;
	struct mbox *mbox;
	int rc = -ENOSPC;
	uint64_t buf_sz;

	if (roc_model_is_cn9k() | roc_model_is_cn10k())
		return nix_inl_legacy_rq_mask_setup(roc_nix, enable);

	mbox = mbox_get((&nix->dev)->mbox);
	/* RQ mask alloc and setup */
	msk_req = mbox_alloc_msg_nix_lf_inline_rq_cfg(mbox);
	if (msk_req == NULL)
		goto exit;

	first_skip = roc_nix->def_first_skip ? (roc_nix->def_first_skip / 8) : 0;
	nix_inl_rq_mask_init(msk_req, first_skip);

	if (roc_nix->local_meta_aura_ena) {
		aura_handle = roc_nix->meta_aura_handle;
		buf_sz = roc_nix->buf_sz;
		if (!aura_handle && enable) {
			plt_err("NULL meta aura handle");
			rc = -EINVAL;
			goto exit;
		}

		/* SPB setup */
		msk_req->ipsec_cfg1_inline_replay.spb_cpt_aura =
			roc_npa_aura_handle_to_aura(aura_handle);
		msk_req->ipsec_cfg1_inline_replay.rq_mask_enable = enable;
		msk_req->ipsec_cfg1_inline_replay.spb_cpt_sizem1 = (buf_sz >> 7) - 1;
		msk_req->ipsec_cfg1_inline_replay.spb_cpt_enable = enable;
	}

	rc = mbox_process(mbox);
	if (rc)
		plt_err("Failed to setup NIX Inline RQ mask, rc=%d", rc);

exit:
	mbox_put(mbox);
	return rc;
}

static void
nix_inl_eng_caps_get(struct nix *nix)
{
	struct roc_cpt_lf *lf = nix->cpt_lf_base;
	uintptr_t lmt_base = lf->lmt_base;
	union cpt_res_s res, *hw_res;
	struct cpt_inst_s inst;
	uint64_t *rptr;

	hw_res = plt_zmalloc(sizeof(*hw_res), ROC_CPT_RES_ALIGN);
	if (hw_res == NULL) {
		plt_err("Couldn't allocate memory for result address");
		return;
	}

	rptr = plt_zmalloc(ROC_ALIGN, 0);
	if (rptr == NULL) {
		plt_err("Couldn't allocate memory for rptr");
		plt_free(hw_res);
		return;
	}

	/* Fill CPT_INST_S for LOAD_FVC/HW_CRYPTO_SUPPORT microcode op */
	memset(&inst, 0, sizeof(struct cpt_inst_s));
	inst.res_addr = (uint64_t)hw_res;
	inst.rptr = (uint64_t)rptr;
	inst.w4.s.opcode_major = ROC_LOADFVC_MAJOR_OP;
	inst.w4.s.opcode_minor = ROC_LOADFVC_MINOR_OP;
	if (roc_model_is_cn9k() || roc_model_is_cn10k())
		inst.w7.s.egrp = ROC_LEGACY_CPT_DFLT_ENG_GRP_SE;
	else
		inst.w7.s.egrp = ROC_CPT_DFLT_ENG_GRP_SE;

	/* Use 1 min timeout for the poll */
	const uint64_t timeout = plt_tsc_cycles() + 60 * plt_tsc_hz();

	if (roc_model_is_cn9k()) {
		uint64_t lmt_status;

		hw_res->cn9k.compcode = CPT_COMP_NOT_DONE;
		plt_io_wmb();

		do {
			roc_lmt_mov_seg((void *)lmt_base, &inst, 4);
			lmt_status = roc_lmt_submit_ldeor(lf->io_addr);
		} while (lmt_status == 0);

		/* Wait until CPT instruction completes */
		do {
			res.u64[0] = __atomic_load_n(&hw_res->u64[0], __ATOMIC_RELAXED);
			if (unlikely(plt_tsc_cycles() > timeout))
				break;
		} while (res.cn9k.compcode == CPT_COMP_NOT_DONE);

		if (res.cn9k.compcode != CPT_COMP_GOOD) {
			plt_err("LOAD FVC operation timed out");
			return;
		}
	} else if (roc_model_is_cn10k()) {
		uint64_t lmt_arg, io_addr;
		uint16_t lmt_id;

		hw_res->cn10k.compcode = CPT_COMP_NOT_DONE;

		/* Use this reserved LMT line as no one else is using it */
		lmt_id = roc_plt_control_lmt_id_get();
		lmt_base += ((uint64_t)lmt_id << ROC_LMT_LINE_SIZE_LOG2);

		memcpy((void *)lmt_base, &inst, sizeof(inst));

		lmt_arg = ROC_CN10K_CPT_LMT_ARG | (uint64_t)lmt_id;
		io_addr = lf->io_addr | ROC_CN10K_CPT_INST_DW_M1 << 4;

		roc_lmt_submit_steorl(lmt_arg, io_addr);
		plt_io_wmb();

		/* Wait until CPT instruction completes */
		do {
			res.u64[0] = __atomic_load_n(&hw_res->u64[0], __ATOMIC_RELAXED);
			if (unlikely(plt_tsc_cycles() > timeout))
				break;
		} while (res.cn10k.compcode == CPT_COMP_NOT_DONE);

		if (res.cn10k.compcode != CPT_COMP_GOOD || res.cn10k.uc_compcode) {
			plt_err("LOAD FVC operation timed out");
			goto exit;
		}
	} else {
		uint64_t lmt_arg, io_addr;
		uint16_t lmt_id;

		hw_res->cn20k.compcode = CPT_COMP_NOT_DONE;

		/* Use this reserved LMT line as no one else is using it */
		lmt_id = roc_plt_control_lmt_id_get();
		lmt_base += ((uint64_t)lmt_id << ROC_LMT_LINE_SIZE_LOG2);

		memcpy((void *)lmt_base, &inst, sizeof(inst));

		lmt_arg = ROC_CN20K_CPT_LMT_ARG | (uint64_t)lmt_id;
		io_addr = lf->io_addr | ROC_CN20K_CPT_INST_DW_M1 << 4;

		roc_lmt_submit_steorl(lmt_arg, io_addr);
		plt_io_wmb();

		/* Wait until CPT instruction completes */
		do {
			res.u64[0] = __atomic_load_n(&hw_res->u64[0], __ATOMIC_RELAXED);
			if (unlikely(plt_tsc_cycles() > timeout))
				break;
		} while (res.cn20k.compcode == CPT_COMP_NOT_DONE);

		if (res.cn20k.compcode != CPT_COMP_GOOD || res.cn20k.uc_compcode) {
			plt_err("LOAD FVC operation timed out");
			goto exit;
		}
	}

	nix->cpt_eng_caps = plt_be_to_cpu_64(*rptr);
exit:
	plt_free(rptr);
	plt_free(hw_res);
}

static int
nix_inl_legacy_inb_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_cpt_inline_ipsec_inb_cfg cfg = {0};
	struct idev_cfg *idev = idev_get_cfg();
	uint16_t bpids[ROC_NIX_MAX_BPID_CNT];
	struct roc_cpt *roc_cpt;
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

	nix->ipsec_prof_id = 0;
	memset(&cfg, 0, sizeof(cfg));
	if (roc_model_is_cn9k()) {
		cfg.param1 = (ROC_ONF_IPSEC_INB_MAX_L2_SZ >> 3) & 0xf;
		cfg.param2 = ROC_IE_ON_INB_IKEV2_SINGLE_SA_SUPPORT;
		cfg.opcode =
			((ROC_IE_ON_INB_MAX_CTX_LEN << 8) |
			 (ROC_IE_ON_MAJOR_OP_PROCESS_INBOUND_IPSEC | (1 << 6)));
	} else {
		union roc_ot_ipsec_inb_param1 u;

		u.u16 = 0;
		u.s.esp_trailer_disable = 1;
		cfg.param1 = u.u16;
		cfg.param2 = 0;
		cfg.opcode = (ROC_IE_OT_MAJOR_OP_PROCESS_INBOUND_IPSEC | (1 << 6));

		if (roc_nix->custom_inb_sa) {
			cfg.param1 = roc_nix->inb_cfg_param1;
			cfg.param2 = roc_nix->inb_cfg_param2;
		}
		rc = roc_nix_bpids_alloc(roc_nix, ROC_NIX_INTF_TYPE_CPT_NIX, 1, bpids);
		if (rc > 0) {
			nix->cpt_nixbpid = bpids[0];
			cfg.bpid = nix->cpt_nixbpid;
		}

		if (roc_errata_cpt_has_ctx_fetch_issue()) {
			cfg.ctx_ilen_valid = true;
			cfg.ctx_ilen = (ROC_NIX_INL_OT_IPSEC_INB_HW_SZ / 128) - 1;
		}
	}

	/* Do onetime Inbound Inline config in CPTPF */
	rc = roc_cpt_inline_ipsec_inb_cfg(roc_cpt, &cfg);
	if (rc && rc != -EEXIST) {
		plt_err("Failed to setup inbound lf, rc=%d", rc);
		return rc;
	}
	nix->cpt_eng_caps = roc_cpt->hw_caps[CPT_ENG_TYPE_SE].u;

	/* Setup Inbound SA table */
	rc = nix_inl_inb_ipsec_sa_tbl_setup(roc_nix);
	if (rc)
		return rc;

	if (!roc_model_is_cn9k() && !roc_errata_nix_no_meta_aura()) {
		nix->need_meta_aura = true;
		if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena)
			idev->inl_cfg.refs++;
	}

	nix->inl_inb_ena = true;
	return 0;
}

static int
nix_inl_inb_init(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;
	int rc;

	if (idev == NULL)
		return -ENOTSUP;

	inl_dev = idev->nix_inl_dev;

	if (!inl_dev || !inl_dev->nb_inb_cptlfs) {
		plt_err("Cannot support inline inbound without inline dev");
		return -ENOTSUP;
	}

	/* FIXME get engine caps from inline device */
	nix->cpt_eng_caps = 0;

	/* Setup Inbound SA table */
	rc = nix_inl_inb_ipsec_sa_tbl_setup(roc_nix);
	if (rc)
		return rc;

	if (roc_nix->reass_ena) {
		rc = nix_inl_reass_inb_sa_tbl_setup(roc_nix);
		if (rc)
			return rc;
	}

	if (roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena) {
		nix->need_meta_aura = true;

		if (roc_nix->custom_meta_aura_ena)
			idev->inl_cfg.refs++;
	}

	nix->inl_inb_ena = true;
	return 0;
}

int
roc_nix_inl_inb_init(struct roc_nix *roc_nix)
{
	if (roc_model_is_cn9k() || roc_model_is_cn10k())
		return nix_inl_legacy_inb_init(roc_nix);

	return nix_inl_inb_init(roc_nix);
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
		if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena)
			idev->inl_cfg.refs--;

		if (roc_nix->custom_meta_aura_ena)
			nix_inl_custom_meta_aura_destroy(roc_nix);

		if (!idev->inl_cfg.refs)
			nix_inl_meta_aura_destroy(roc_nix);
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

	if (roc_nix->reass_ena)
		nix_inl_reass_sa_tbl_release(roc_nix);

	/* Disable Inbound SA */
	return nix_inl_ipsec_sa_tbl_release(roc_nix);
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
	bool ctx_ilen_valid = false;
	size_t sa_sz, ring_sz;
	uint8_t ctx_ilen = 0;
	bool rx_inj = false;
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
	if (roc_nix->rx_inj_ena && !(nix->inb_inl_dev && inl_dev && inl_dev->attach_cptlf &&
				     inl_dev->rx_inj_ena)) {
		nb_lf++;
		rx_inj = true;
	}

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

	if (!roc_model_is_cn9k() && roc_errata_cpt_has_ctx_fetch_issue()) {
		ctx_ilen = (ROC_NIX_INL_OT_IPSEC_OUTB_HW_SZ / 128) - 1;
		ctx_ilen_valid = true;
	}

	if (roc_model_is_cn9k() || roc_model_is_cn10k())
		eng_grpmask = (1ULL << ROC_LEGACY_CPT_DFLT_ENG_GRP_SE |
			       1ULL << ROC_LEGACY_CPT_DFLT_ENG_GRP_SE_IE |
			       1ULL << ROC_LEGACY_CPT_DFLT_ENG_GRP_AE);
	else
		eng_grpmask = (1ULL << ROC_CPT_DFLT_ENG_GRP_SE | 1ULL << ROC_CPT_DFLT_ENG_GRP_AE);

	/* Alloc CPT LF */
	rc = cpt_lfs_alloc(dev, eng_grpmask, blkaddr,
			   !roc_nix->ipsec_out_sso_pffunc, ctx_ilen_valid, ctx_ilen,
			   rx_inj, nb_lf - 1);
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

		if (roc_feature_nix_has_cpt_cq_support()) {
			if (inl_dev && inl_dev->cpt_cq_ena) {
				lf->dq_ack_ena = true;
				lf->cpt_cq_ena = true;
				lf->cq_entry_size = 0;
				lf->cq_all = 0;
				lf->cq_size = lf->nb_desc;
			}
		}

		/* Setup CPT LF instruction queue */
		rc = cpt_lf_init(lf, lf->cpt_cq_ena);
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
		/* Enable CQ */
		if (lf->cpt_cq_ena) {
			rc = cpt_lf_register_irqs(lf, cpt_lf_misc_irq, nix_inl_cpt_done_irq);
			if (rc)
				goto lf_fini;
			roc_cpt_cq_enable(lf);
		}
	}

	if (!roc_nix->ipsec_out_max_sa)
		goto skip_sa_alloc;

	/* CN9K SA size is different */
	if (roc_model_is_cn9k())
		sa_sz = ROC_NIX_INL_ON_IPSEC_OUTB_SA_SZ;
	else if (roc_model_is_cn10k())
		sa_sz = ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ;
	else
		sa_sz = ROC_NIX_INL_OW_IPSEC_OUTB_SA_SZ;

	/* Alloc contiguous memory of outbound SA */
	sa_base = plt_zmalloc(sa_sz * roc_nix->ipsec_out_max_sa,
			      ROC_NIX_INL_SA_BASE_ALIGN);
	if (!sa_base) {
		plt_err("Outbound SA base alloc failed");
		goto lf_fini;
	}

	if (!roc_model_is_cn9k()) {
		for (i = 0; i < roc_nix->ipsec_out_max_sa; i++) {
			sa = ((uint8_t *)sa_base) + (i * sa_sz);
			if (roc_model_is_cn10k())
				roc_ot_ipsec_outb_sa_init(sa);
			else
				roc_ow_ipsec_outb_sa_init(sa);
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

	/* Fetch engine capabilities */
	nix_inl_eng_caps_get(nix);

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
	for (j = i - 1; j >= 0; j--) {
		lf = &lf_base[j];
		cpt_lf_fini(lf, lf->cpt_cq_ena);
		if (lf->cpt_cq_ena)
			cpt_lf_unregister_irqs(lf, cpt_lf_misc_irq, nix_inl_cpt_done_irq);
	}
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
	struct roc_cpt_lf *lf;
	uint64_t *ring_base;
	int i, rc, ret = 0;

	if (!nix->inl_outb_ena)
		return 0;

	nix->inl_outb_ena = false;

	/* Cleanup CPT LF instruction queue */
	for (i = 0; i < nix->nb_cpt_lf; i++) {
		lf = &lf_base[i];
		cpt_lf_fini(lf, lf->cpt_cq_ena);
		if (lf->cpt_cq_ena)
			cpt_lf_unregister_irqs(lf, cpt_lf_misc_irq, nix_inl_cpt_done_irq);
	}
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
roc_nix_inl_dev_is_multi_channel(void)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	if (idev == NULL || !idev->nix_inl_dev)
		return false;

	inl_dev = idev->nix_inl_dev;
	return inl_dev->is_multi_channel;
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
	else if (roc_model_is_cn10k())
		rc = nix_rq_cn10k_cfg(dev, inl_rq, inl_dev->qints, false, enable);
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

	if (roc_feature_nix_has_inl_rq_mask() && enable) {
		rc = nix_inl_rq_mask_cfg(roc_nix, enable);
		if (rc) {
			plt_err("Failed to get rq mask rc=%d", rc);
			return rc;
		}
	}

	if (nix->inb_inl_dev) {
		if (!inl_rq || !idev->nix_inl_dev)
			return -EFAULT;

		inl_dev = idev->nix_inl_dev;

		if (!roc_model_is_cn10k()) {
			if (inl_rq->spb_ena) {
				rc = -EINVAL;
				plt_err("inline RQ enable is not supported rc=%d", rc);
				return rc;
			}
		}

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
		if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena)
			idev->inl_cfg.refs++;
	} else if (nix->need_meta_aura) {
		nix->need_meta_aura = false;
		if (!roc_nix->local_meta_aura_ena || roc_nix->custom_meta_aura_ena)
			idev->inl_cfg.refs--;

		if (roc_nix->custom_meta_aura_ena)
			nix_inl_custom_meta_aura_destroy(roc_nix);

		if (!idev->inl_cfg.refs)
			nix_inl_meta_aura_destroy(roc_nix);
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
	cfg.sa_size = nix->inb_sa_sz[nix->ipsec_prof_id];
	cfg.iova = (uintptr_t)nix->inb_sa_base[nix->ipsec_prof_id];
	cfg.max_sa = nix->inb_spi_mask + 1;
	cfg.tt = tt;
	cfg.tag_const = tag_const;
	if (roc_nix->res_addr_offset)
		cfg.res_addr_offset = roc_nix->res_addr_offset;

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
	union cpt_lf_ctx_err err;
	union cpt_lf_ctx_inval inval;
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

	if ((!inl_dev && roc_nix == NULL) || sa == NULL)
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
			outb_lf = &inl_dev->cpt_lf[0];
	}

	if (outb_lf) {
		rbase = outb_lf->rbase;

		flush.u = 0;
		reload.u = 0;
		inval.u = 0;
		switch (op) {
		case ROC_NIX_INL_SA_OP_FLUSH_INVAL:
			if (!roc_model_is_cn10k()) {
				inval.s.cptr = ((uintptr_t)sa) >> 7;
				plt_write64(inval.u, rbase + CPT_LF_CTX_INVAL);
				break;
			}

			flush.s.inval = 1;
			/* fall through */
		case ROC_NIX_INL_SA_OP_FLUSH:
			flush.s.cptr = ((uintptr_t)sa) >> 7;
			plt_write64(flush.u, rbase + CPT_LF_CTX_FLUSH);
			plt_atomic_thread_fence(__ATOMIC_ACQ_REL);
			/* Read a CSR to ensure that the FLUSH operation is complete */
			err.u = plt_read64(rbase + CPT_LF_CTX_ERR);

			if (err.s.flush_st_flt) {
				plt_warn("CTX flush could not complete");
				return -EIO;
			}
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
	union cpt_lf_ctx_inval inval;
	union cpt_lf_ctx_err err;
	bool get_inl_lf = true;
	uintptr_t rbase;
	struct nix *nix;
	uint64_t *sa;
	int rc;

	/* Nothing much to do on cn9k */
	if (roc_model_is_cn9k()) {
		return 0;
	}
	if (idev)
		inl_dev = idev->nix_inl_dev;

	if ((!inl_dev && roc_nix == NULL) || sa_dptr == NULL || sa_cptr == NULL)
		return -EINVAL;

	if (roc_nix) {
		if (inb && roc_nix->custom_inb_sa && sa_len > ROC_NIX_INL_INB_CUSTOM_SA_SZ) {
			plt_nix_dbg("SA length: %u is more than allocated length: %u", sa_len,
				    ROC_NIX_INL_INB_CUSTOM_SA_SZ);
			return -EINVAL;
		}
		nix = roc_nix_to_nix_priv(roc_nix);
		outb_lf = nix->cpt_lf_base;

		if (inb && !nix->inb_inl_dev)
			get_inl_lf = false;
	}

	if (inb && get_inl_lf) {
		outb_lf = NULL;
		if (inl_dev && inl_dev->attach_cptlf)
			outb_lf = &inl_dev->cpt_lf[0];
	}

	if (outb_lf == NULL)
		goto exit;

	if (roc_model_is_cn10k() || (roc_nix && roc_nix->use_write_sa)) {
		rbase = outb_lf->rbase;
		flush.u = 0;

		rc = roc_cpt_ctx_write(outb_lf, sa_dptr, sa_cptr, sa_len);
		if (rc)
			return rc;
		/* Trigger CTX flush to write dirty data back to DRAM */
		flush.s.cptr = ((uintptr_t)sa_cptr) >> 7;
		plt_write64(flush.u, rbase + CPT_LF_CTX_FLUSH);

		plt_atomic_thread_fence(__ATOMIC_ACQ_REL);

		/* Read a CSR to ensure that the FLUSH operation is complete */
		err.u = plt_read64(rbase + CPT_LF_CTX_ERR);

		if (err.s.flush_st_flt)
			plt_warn("CTX flush could not complete");
		return 0;
	} else {
		sa = sa_dptr;

		/* Clear bit 58 aop_valid */
		sa[0] &= ~(1ULL << 58);
		memcpy(sa_cptr, sa_dptr, sa_len);
		plt_io_wmb();

		/* Trigger CTX invalidate */
		rbase = outb_lf->rbase;
		inval.u = 0;
		inval.s.cptr = ((uintptr_t)sa_cptr) >> 7;
		plt_write64(inval.u, rbase + CPT_LF_CTX_INVAL);

		/* Set bit 58 aop_valid */
		sa = sa_cptr;
		sa[0] |= (1ULL << 58);
		plt_io_wmb();

		return 0;
	}

exit:
	plt_nix_dbg("Could not get CPT LF for CTX write");
	return -ENOTSUP;
}

static inline int
nix_inl_dev_cpt_lf_stats_get(struct roc_nix *roc_nix, struct roc_nix_cpt_lf_stats *stats,
			     uint16_t idx)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct roc_cpt_lf *lf = NULL;

	PLT_SET_USED(roc_nix);
	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (inl_dev && inl_dev->attach_cptlf) {
		if (idx >= inl_dev->nb_cptlf) {
			plt_err("Invalid idx: %u total lfs: %d", idx, inl_dev->nb_cptlf);
			return -EINVAL;
		}
		lf = &inl_dev->cpt_lf[idx];
	} else {
		plt_err("No CPT LF(s) are found for Inline Device");
		return -EINVAL;
	}
	stats->enc_pkts = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
	stats->enc_bytes = plt_read64(lf->rbase + CPT_LF_CTX_ENC_BYTE_CNT);
	stats->dec_pkts = plt_read64(lf->rbase + CPT_LF_CTX_DEC_PKT_CNT);
	stats->dec_bytes = plt_read64(lf->rbase + CPT_LF_CTX_DEC_BYTE_CNT);

	return 0;
}

static inline int
nix_eth_dev_cpt_lf_stats_get(struct roc_nix *roc_nix, struct roc_nix_cpt_lf_stats *stats,
			     uint16_t idx)
{
	struct roc_cpt_lf *lf;
	struct nix *nix;

	if (!roc_nix)
		return -EINVAL;
	nix = roc_nix_to_nix_priv(roc_nix);
	if (idx >= nix->nb_cpt_lf) {
		plt_err("Invalid idx: %u total lfs: %d", idx, nix->nb_cpt_lf);
		return -EINVAL;
	}
	lf = &nix->cpt_lf_base[idx];
	stats->enc_pkts = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
	stats->enc_bytes = plt_read64(lf->rbase + CPT_LF_CTX_ENC_BYTE_CNT);
	stats->dec_pkts = plt_read64(lf->rbase + CPT_LF_CTX_DEC_PKT_CNT);
	stats->dec_bytes = plt_read64(lf->rbase + CPT_LF_CTX_DEC_BYTE_CNT);

	return 0;
}

int
roc_nix_inl_cpt_lf_stats_get(struct roc_nix *roc_nix, enum roc_nix_cpt_lf_stats_type type,
			     struct roc_nix_cpt_lf_stats *stats, uint16_t idx)
{
	switch (type) {
	case ROC_NIX_CPT_LF_STATS_INL_DEV:
		return nix_inl_dev_cpt_lf_stats_get(roc_nix, stats, idx);
	case ROC_NIX_CPT_LF_STATS_ETHDEV:
		return nix_eth_dev_cpt_lf_stats_get(roc_nix, stats, idx);
	default:
		return -EINVAL;
	}
}

static int
nix_inl_ts_pkind_set(struct roc_nix *roc_nix, bool ts_ena, bool inb_inl_dev, uint8_t profile_id)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	void *sa, *sa_base = NULL;
	struct nix *nix = NULL;
	uint32_t rq_refs = 0;
	uint16_t max_sa = 0;
	uint8_t pkind = 0;
	size_t inb_sa_sz;
	int i;

	if (roc_model_is_cn9k())
		return 0;

	if (!inb_inl_dev && (roc_nix == NULL) && profile_id >= ROC_NIX_INL_PROFILE_CNT)
		return -EINVAL;

	if (inb_inl_dev) {
		if ((idev == NULL) || (idev->nix_inl_dev == NULL))
			return 0;
		inl_dev = idev->nix_inl_dev;
	} else {
		nix = roc_nix_to_nix_priv(roc_nix);
		if (!nix->inl_inb_ena)
			return 0;

		sa_base = nix->inb_sa_base[profile_id];
		if (sa_base == NULL)
			return 0;
		inb_sa_sz = nix->inb_sa_sz[profile_id];
		max_sa = nix->inb_sa_max[profile_id];
	}

	if (inl_dev) {
		for (i = 0; i < inl_dev->nb_rqs; i++)
			rq_refs += inl_dev->rqs[i].inl_dev_refs;

		if (rq_refs == 0) {
			sa_base = inl_dev->inb_sa_base[profile_id];
			if (sa_base == NULL)
				return 0;
			inl_dev->ts_ena = ts_ena;
			max_sa = inl_dev->inb_sa_max[profile_id];
			inb_sa_sz = inl_dev->inb_sa_sz[profile_id];
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

	for (i = 0; i < max_sa; i++) {
		sa = ((uint8_t *)sa_base) + (i * inb_sa_sz);
		((struct roc_ot_ipsec_inb_sa *)sa)->w0.s.pkind = pkind;
	}
	return 0;
}

int
roc_nix_inl_ts_pkind_set(struct roc_nix *roc_nix, bool ts_ena, bool inb_inl_dev, uint8_t profile_id)
{
	int cnt = 0;

	if (profile_id < ROC_NIX_INL_PROFILE_CNT) {
		return nix_inl_ts_pkind_set(roc_nix, ts_ena, inb_inl_dev, profile_id);
	} else if (profile_id == 0xFF) {
		/* Configure for all valid profiles */
		for (cnt = 0; cnt < ROC_NIX_INL_PROFILE_CNT; cnt++)
			if (nix_inl_ts_pkind_set(roc_nix, ts_ena, inb_inl_dev, cnt))
				return -EINVAL;
		return 0;
	}

	plt_err("Invalid NIX inline profile_id: %u", profile_id);
	return -EINVAL;
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

uint64_t
roc_nix_inl_eng_caps_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->cpt_eng_caps;
}

void
roc_nix_inl_custom_meta_pool_cb_register(roc_nix_inl_custom_meta_pool_cb_t cb)
{
	custom_meta_pool_cb = cb;
}

uint8_t
roc_nix_inl_is_cq_ena(struct roc_nix *roc_nix)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	PLT_SET_USED(roc_nix);
	if (idev != NULL) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev)
			return inl_dev->cpt_cq_ena;
		else
			return 0;
	} else {
		return 0;
	}
}
