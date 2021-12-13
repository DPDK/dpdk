/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define XAQ_CACHE_CNT 0x7

/* Default Rx Config for Inline NIX LF */
#define NIX_INL_LF_RX_CFG                                                      \
	(ROC_NIX_LF_RX_CFG_DROP_RE | ROC_NIX_LF_RX_CFG_L2_LEN_ERR |            \
	 ROC_NIX_LF_RX_CFG_IP6_UDP_OPT | ROC_NIX_LF_RX_CFG_DIS_APAD |          \
	 ROC_NIX_LF_RX_CFG_CSUM_IL4 | ROC_NIX_LF_RX_CFG_CSUM_OL4 |             \
	 ROC_NIX_LF_RX_CFG_LEN_IL4 | ROC_NIX_LF_RX_CFG_LEN_IL3 |               \
	 ROC_NIX_LF_RX_CFG_LEN_OL4 | ROC_NIX_LF_RX_CFG_LEN_OL3)

uint16_t
nix_inl_dev_pffunc_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	if (idev != NULL) {
		inl_dev = idev->nix_inl_dev;
		if (inl_dev)
			return inl_dev->dev.pf_func;
	}
	return 0;
}

static void
nix_inl_selftest_work_cb(uint64_t *gw, void *args)
{
	uintptr_t work = gw[1];

	*((uintptr_t *)args + (gw[0] & 0x1)) = work;

	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);
}

static int
nix_inl_selftest(void)
{
	struct idev_cfg *idev = idev_get_cfg();
	roc_nix_inl_sso_work_cb_t save_cb;
	static uintptr_t work_arr[2];
	struct nix_inl_dev *inl_dev;
	void *save_cb_args;
	uint64_t add_work0;
	int rc = 0;

	if (idev == NULL)
		return -ENOTSUP;

	inl_dev = idev->nix_inl_dev;
	if (inl_dev == NULL)
		return -ENOTSUP;

	plt_info("Performing nix inl self test");

	/* Save and update cb to test cb */
	save_cb = inl_dev->work_cb;
	save_cb_args = inl_dev->cb_args;
	inl_dev->work_cb = nix_inl_selftest_work_cb;
	inl_dev->cb_args = work_arr;

	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);

#define WORK_MAGIC1 0x335577ff0
#define WORK_MAGIC2 0xdeadbeef0

	/* Add work */
	add_work0 = ((uint64_t)(SSO_TT_ORDERED) << 32) | 0x0;
	roc_store_pair(add_work0, WORK_MAGIC1, inl_dev->sso_base);
	add_work0 = ((uint64_t)(SSO_TT_ORDERED) << 32) | 0x1;
	roc_store_pair(add_work0, WORK_MAGIC2, inl_dev->sso_base);

	plt_delay_ms(10000);

	/* Check if we got expected work */
	if (work_arr[0] != WORK_MAGIC1 || work_arr[1] != WORK_MAGIC2) {
		plt_err("Failed to get expected work, [0]=%p [1]=%p",
			(void *)work_arr[0], (void *)work_arr[1]);
		rc = -EFAULT;
		goto exit;
	}

	plt_info("Work, [0]=%p [1]=%p", (void *)work_arr[0],
		 (void *)work_arr[1]);

exit:
	/* Restore state */
	inl_dev->work_cb = save_cb;
	inl_dev->cb_args = save_cb_args;
	return rc;
}

static int
nix_inl_cpt_ctx_cache_sync(struct nix_inl_dev *inl_dev)
{
	struct mbox *mbox = (&inl_dev->dev)->mbox;
	struct msg_req *req;

	req = mbox_alloc_msg_cpt_ctx_cache_sync(mbox);
	if (req == NULL)
		return -ENOSPC;

	return mbox_process(mbox);
}

static int
nix_inl_nix_ipsec_cfg(struct nix_inl_dev *inl_dev, bool ena)
{
	struct nix_inline_ipsec_lf_cfg *lf_cfg;
	struct mbox *mbox = (&inl_dev->dev)->mbox;
	uint32_t sa_w;

	lf_cfg = mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
	if (lf_cfg == NULL)
		return -ENOSPC;

	if (ena) {
		sa_w = plt_align32pow2(inl_dev->ipsec_in_max_spi + 1);
		sa_w = plt_log2_u32(sa_w);

		lf_cfg->enable = 1;
		lf_cfg->sa_base_addr = (uintptr_t)inl_dev->inb_sa_base;
		lf_cfg->ipsec_cfg1.sa_idx_w = sa_w;
		/* CN9K SA size is different */
		if (roc_model_is_cn9k())
			lf_cfg->ipsec_cfg0.lenm1_max = NIX_CN9K_MAX_HW_FRS - 1;
		else
			lf_cfg->ipsec_cfg0.lenm1_max = NIX_RPM_MAX_HW_FRS - 1;
		lf_cfg->ipsec_cfg1.sa_idx_max = inl_dev->ipsec_in_max_spi;
		lf_cfg->ipsec_cfg0.sa_pow2_size =
			plt_log2_u32(inl_dev->inb_sa_sz);

		lf_cfg->ipsec_cfg0.tag_const = 0;
		lf_cfg->ipsec_cfg0.tt = SSO_TT_ORDERED;
	} else {
		lf_cfg->enable = 0;
	}

	return mbox_process(mbox);
}

static int
nix_inl_cpt_setup(struct nix_inl_dev *inl_dev)
{
	struct roc_cpt_lf *lf = &inl_dev->cpt_lf;
	struct dev *dev = &inl_dev->dev;
	uint8_t eng_grpmask;
	int rc;

	if (!inl_dev->attach_cptlf)
		return 0;

	/* Alloc CPT LF */
	eng_grpmask = (1ULL << ROC_CPT_DFLT_ENG_GRP_SE |
		       1ULL << ROC_CPT_DFLT_ENG_GRP_SE_IE |
		       1ULL << ROC_CPT_DFLT_ENG_GRP_AE);
	rc = cpt_lfs_alloc(dev, eng_grpmask, RVU_BLOCK_ADDR_CPT0, false);
	if (rc) {
		plt_err("Failed to alloc CPT LF resources, rc=%d", rc);
		return rc;
	}

	/* Setup CPT LF for submitting control opcode */
	lf = &inl_dev->cpt_lf;
	lf->lf_id = 0;
	lf->nb_desc = 0; /* Set to default */
	lf->dev = &inl_dev->dev;
	lf->msixoff = inl_dev->cpt_msixoff;
	lf->pci_dev = inl_dev->pci_dev;

	rc = cpt_lf_init(lf);
	if (rc) {
		plt_err("Failed to initialize CPT LF, rc=%d", rc);
		goto lf_free;
	}

	roc_cpt_iq_enable(lf);
	return 0;
lf_free:
	rc |= cpt_lfs_free(dev);
	return rc;
}

static int
nix_inl_cpt_release(struct nix_inl_dev *inl_dev)
{
	struct roc_cpt_lf *lf = &inl_dev->cpt_lf;
	struct dev *dev = &inl_dev->dev;
	int rc, ret = 0;

	if (!inl_dev->attach_cptlf)
		return 0;

	/* Cleanup CPT LF queue */
	cpt_lf_fini(lf);

	/* Free LF resources */
	rc = cpt_lfs_free(dev);
	if (rc)
		plt_err("Failed to free CPT LF resources, rc=%d", rc);
	ret |= rc;

	/* Detach LF */
	rc = cpt_lfs_detach(dev);
	if (rc)
		plt_err("Failed to detach CPT LF, rc=%d", rc);
	ret |= rc;

	return ret;
}

static int
nix_inl_sso_setup(struct nix_inl_dev *inl_dev)
{
	struct sso_lf_alloc_rsp *sso_rsp;
	struct dev *dev = &inl_dev->dev;
	uint32_t xaq_cnt, count, aura;
	uint16_t hwgrp[1] = {0};
	struct npa_pool_s pool;
	uintptr_t iova;
	int rc;

	/* Alloc SSOW LF */
	rc = sso_lf_alloc(dev, SSO_LF_TYPE_HWS, 1, NULL);
	if (rc) {
		plt_err("Failed to alloc SSO HWS, rc=%d", rc);
		return rc;
	}

	/* Alloc HWGRP LF */
	rc = sso_lf_alloc(dev, SSO_LF_TYPE_HWGRP, 1, (void **)&sso_rsp);
	if (rc) {
		plt_err("Failed to alloc SSO HWGRP, rc=%d", rc);
		goto free_ssow;
	}

	inl_dev->xaq_buf_size = sso_rsp->xaq_buf_size;
	inl_dev->xae_waes = sso_rsp->xaq_wq_entries;
	inl_dev->iue = sso_rsp->in_unit_entries;

	/* Create XAQ pool */
	xaq_cnt = XAQ_CACHE_CNT;
	xaq_cnt += inl_dev->iue / inl_dev->xae_waes;
	plt_sso_dbg("Configuring %d xaq buffers", xaq_cnt);

	inl_dev->xaq_mem = plt_zmalloc(inl_dev->xaq_buf_size * xaq_cnt,
				       inl_dev->xaq_buf_size);
	if (!inl_dev->xaq_mem) {
		rc = NIX_ERR_NO_MEM;
		plt_err("Failed to alloc xaq buf mem");
		goto free_sso;
	}

	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;
	rc = roc_npa_pool_create(&inl_dev->xaq_aura, inl_dev->xaq_buf_size,
				 xaq_cnt, NULL, &pool);
	if (rc) {
		plt_err("Failed to alloc aura for XAQ, rc=%d", rc);
		goto free_mem;
	}

	/* Fill the XAQ buffers */
	iova = (uint64_t)inl_dev->xaq_mem;
	for (count = 0; count < xaq_cnt; count++) {
		roc_npa_aura_op_free(inl_dev->xaq_aura, 0, iova);
		iova += inl_dev->xaq_buf_size;
	}
	roc_npa_aura_op_range_set(inl_dev->xaq_aura, (uint64_t)inl_dev->xaq_mem,
				  iova);

	aura = roc_npa_aura_handle_to_aura(inl_dev->xaq_aura);

	/* Setup xaq for hwgrps */
	rc = sso_hwgrp_alloc_xaq(dev, aura, 1);
	if (rc) {
		plt_err("Failed to setup hwgrp xaq aura, rc=%d", rc);
		goto destroy_pool;
	}

	/* Register SSO, SSOW error and work irq's */
	rc = nix_inl_sso_register_irqs(inl_dev);
	if (rc) {
		plt_err("Failed to register sso irq's, rc=%d", rc);
		goto release_xaq;
	}

	/* Setup hwgrp->hws link */
	sso_hws_link_modify(0, inl_dev->ssow_base, NULL, hwgrp, 1, true);

	/* Enable HWGRP */
	plt_write64(0x1, inl_dev->sso_base + SSO_LF_GGRP_QCTL);

	return 0;

release_xaq:
	sso_hwgrp_release_xaq(&inl_dev->dev, 1);
destroy_pool:
	roc_npa_pool_destroy(inl_dev->xaq_aura);
	inl_dev->xaq_aura = 0;
free_mem:
	plt_free(inl_dev->xaq_mem);
	inl_dev->xaq_mem = NULL;
free_sso:
	sso_lf_free(dev, SSO_LF_TYPE_HWGRP, 1);
free_ssow:
	sso_lf_free(dev, SSO_LF_TYPE_HWS, 1);
	return rc;
}

static int
nix_inl_sso_release(struct nix_inl_dev *inl_dev)
{
	uint16_t hwgrp[1] = {0};

	/* Disable HWGRP */
	plt_write64(0, inl_dev->sso_base + SSO_LF_GGRP_QCTL);

	/* Unregister SSO/SSOW IRQ's */
	nix_inl_sso_unregister_irqs(inl_dev);

	/* Unlink hws */
	sso_hws_link_modify(0, inl_dev->ssow_base, NULL, hwgrp, 1, false);

	/* Release XAQ aura */
	sso_hwgrp_release_xaq(&inl_dev->dev, 1);

	/* Free SSO, SSOW LF's */
	sso_lf_free(&inl_dev->dev, SSO_LF_TYPE_HWS, 1);
	sso_lf_free(&inl_dev->dev, SSO_LF_TYPE_HWGRP, 1);

	return 0;
}

static int
nix_inl_nix_setup(struct nix_inl_dev *inl_dev)
{
	uint16_t ipsec_in_max_spi = inl_dev->ipsec_in_max_spi;
	struct dev *dev = &inl_dev->dev;
	struct mbox *mbox = dev->mbox;
	struct nix_lf_alloc_rsp *rsp;
	struct nix_lf_alloc_req *req;
	struct nix_hw_info *hw_info;
	size_t inb_sa_sz;
	int i, rc = -ENOSPC;
	void *sa;

	/* Alloc NIX LF needed for single RQ */
	req = mbox_alloc_msg_nix_lf_alloc(mbox);
	if (req == NULL)
		return rc;
	req->rq_cnt = 1;
	req->sq_cnt = 1;
	req->cq_cnt = 1;
	/* XQESZ is W16 */
	req->xqe_sz = NIX_XQESZ_W16;
	/* RSS size does not matter as this RQ is only for UCAST_IPSEC action */
	req->rss_sz = ROC_NIX_RSS_RETA_SZ_64;
	req->rss_grps = ROC_NIX_RSS_GRPS;
	req->npa_func = idev_npa_pffunc_get();
	req->sso_func = dev->pf_func;
	req->rx_cfg = NIX_INL_LF_RX_CFG;
	req->flags = NIX_LF_RSS_TAG_LSB_AS_ADDER;

	if (roc_model_is_cn10ka_a0() || roc_model_is_cnf10ka_a0() ||
	    roc_model_is_cnf10kb_a0())
		req->rx_cfg &= ~ROC_NIX_LF_RX_CFG_DROP_RE;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to alloc lf, rc=%d", rc);
		return rc;
	}

	inl_dev->lf_tx_stats = rsp->lf_tx_stats;
	inl_dev->lf_rx_stats = rsp->lf_rx_stats;
	inl_dev->qints = rsp->qints;
	inl_dev->cints = rsp->cints;

	/* Get VWQE info if supported */
	if (roc_model_is_cn10k()) {
		mbox_alloc_msg_nix_get_hw_info(mbox);
		rc = mbox_process_msg(mbox, (void *)&hw_info);
		if (rc) {
			plt_err("Failed to get HW info, rc=%d", rc);
			goto lf_free;
		}
		inl_dev->vwqe_interval = hw_info->vwqe_delay;
	}

	/* Register nix interrupts */
	rc = nix_inl_nix_register_irqs(inl_dev);
	if (rc) {
		plt_err("Failed to register nix irq's, rc=%d", rc);
		goto lf_free;
	}

	/* CN9K SA is different */
	if (roc_model_is_cn9k())
		inb_sa_sz = ROC_NIX_INL_ONF_IPSEC_INB_SA_SZ;
	else
		inb_sa_sz = ROC_NIX_INL_OT_IPSEC_INB_SA_SZ;

	/* Alloc contiguous memory for Inbound SA's */
	inl_dev->inb_sa_sz = inb_sa_sz;
	inl_dev->inb_sa_base = plt_zmalloc(inb_sa_sz * ipsec_in_max_spi,
					   ROC_NIX_INL_SA_BASE_ALIGN);
	if (!inl_dev->inb_sa_base) {
		plt_err("Failed to allocate memory for Inbound SA");
		rc = -ENOMEM;
		goto unregister_irqs;
	}

	if (roc_model_is_cn10k()) {
		for (i = 0; i < ipsec_in_max_spi; i++) {
			sa = ((uint8_t *)inl_dev->inb_sa_base) +
			     (i * inb_sa_sz);
			roc_nix_inl_inb_sa_init(sa);
		}
	}
	/* Setup device specific inb SA table */
	rc = nix_inl_nix_ipsec_cfg(inl_dev, true);
	if (rc) {
		plt_err("Failed to setup NIX Inbound SA conf, rc=%d", rc);
		goto free_mem;
	}

	return 0;
free_mem:
	plt_free(inl_dev->inb_sa_base);
	inl_dev->inb_sa_base = NULL;
unregister_irqs:
	nix_inl_nix_unregister_irqs(inl_dev);
lf_free:
	mbox_alloc_msg_nix_lf_free(mbox);
	rc |= mbox_process(mbox);
	return rc;
}

static int
nix_inl_nix_release(struct nix_inl_dev *inl_dev)
{
	struct dev *dev = &inl_dev->dev;
	struct mbox *mbox = dev->mbox;
	struct nix_lf_free_req *req;
	struct ndc_sync_op *ndc_req;
	int rc = -ENOSPC;

	/* Disable Inbound processing */
	rc = nix_inl_nix_ipsec_cfg(inl_dev, false);
	if (rc)
		plt_err("Failed to disable Inbound IPSec, rc=%d", rc);

	/* Sync NDC-NIX for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox);
	if (ndc_req == NULL)
		return rc;
	ndc_req->nix_lf_rx_sync = 1;
	rc = mbox_process(mbox);
	if (rc)
		plt_err("Error on NDC-NIX-RX LF sync, rc %d", rc);

	/* Unregister IRQs */
	nix_inl_nix_unregister_irqs(inl_dev);

	/* By default all associated mcam rules are deleted */
	req = mbox_alloc_msg_nix_lf_free(mbox);
	if (req == NULL)
		return -ENOSPC;

	return mbox_process(mbox);
}

static int
nix_inl_lf_attach(struct nix_inl_dev *inl_dev)
{
	struct msix_offset_rsp *msix_rsp;
	struct dev *dev = &inl_dev->dev;
	struct mbox *mbox = dev->mbox;
	struct rsrc_attach_req *req;
	uint64_t nix_blkaddr;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		return rc;
	req->modify = true;
	/* Attach 1 NIXLF, SSO HWS and SSO HWGRP */
	req->nixlf = true;
	req->ssow = 1;
	req->sso = 1;
	if (inl_dev->attach_cptlf) {
		req->cptlfs = 1;
		req->cpt_blkaddr = RVU_BLOCK_ADDR_CPT0;
	}

	rc = mbox_process(dev->mbox);
	if (rc)
		return rc;

	/* Get MSIX vector offsets */
	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(dev->mbox, (void **)&msix_rsp);
	if (rc)
		return rc;

	inl_dev->nix_msixoff = msix_rsp->nix_msixoff;
	inl_dev->ssow_msixoff = msix_rsp->ssow_msixoff[0];
	inl_dev->sso_msixoff = msix_rsp->sso_msixoff[0];
	inl_dev->cpt_msixoff = msix_rsp->cptlf_msixoff[0];

	nix_blkaddr = nix_get_blkaddr(dev);
	inl_dev->is_nix1 = (nix_blkaddr == RVU_BLOCK_ADDR_NIX1);

	/* Update base addresses for LF's */
	inl_dev->nix_base = dev->bar2 + (nix_blkaddr << 20);
	inl_dev->ssow_base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20);
	inl_dev->sso_base = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20);
	inl_dev->cpt_base = dev->bar2 + (RVU_BLOCK_ADDR_CPT0 << 20);

	return 0;
}

static int
nix_inl_lf_detach(struct nix_inl_dev *inl_dev)
{
	struct dev *dev = &inl_dev->dev;
	struct mbox *mbox = dev->mbox;
	struct rsrc_detach_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		return rc;
	req->partial = true;
	req->nixlf = true;
	req->ssow = true;
	req->sso = true;
	req->cptlfs = !!inl_dev->attach_cptlf;

	return mbox_process(dev->mbox);
}

int
roc_nix_inl_dev_init(struct roc_nix_inl_dev *roc_inl_dev)
{
	struct plt_pci_device *pci_dev;
	struct nix_inl_dev *inl_dev;
	struct idev_cfg *idev;
	int rc;

	pci_dev = roc_inl_dev->pci_dev;

	/* Skip probe if already done */
	idev = idev_get_cfg();
	if (idev == NULL)
		return -ENOTSUP;

	if (idev->nix_inl_dev) {
		plt_info("Skipping device %s, inline device already probed",
			 pci_dev->name);
		return -EEXIST;
	}

	PLT_STATIC_ASSERT(sizeof(struct nix_inl_dev) <= ROC_NIX_INL_MEM_SZ);

	inl_dev = (struct nix_inl_dev *)roc_inl_dev->reserved;
	memset(inl_dev, 0, sizeof(*inl_dev));

	inl_dev->pci_dev = pci_dev;
	inl_dev->ipsec_in_max_spi = roc_inl_dev->ipsec_in_max_spi;
	inl_dev->selftest = roc_inl_dev->selftest;
	inl_dev->is_multi_channel = roc_inl_dev->is_multi_channel;
	inl_dev->channel = roc_inl_dev->channel;
	inl_dev->chan_mask = roc_inl_dev->chan_mask;
	inl_dev->attach_cptlf = roc_inl_dev->attach_cptlf;

	/* Initialize base device */
	rc = dev_init(&inl_dev->dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto error;
	}

	/* Attach LF resources */
	rc = nix_inl_lf_attach(inl_dev);
	if (rc) {
		plt_err("Failed to attach LF resources, rc=%d", rc);
		goto dev_cleanup;
	}

	/* Setup NIX LF */
	rc = nix_inl_nix_setup(inl_dev);
	if (rc)
		goto lf_detach;

	/* Setup SSO LF */
	rc = nix_inl_sso_setup(inl_dev);
	if (rc)
		goto nix_release;

	/* Setup CPT LF */
	rc = nix_inl_cpt_setup(inl_dev);
	if (rc)
		goto sso_release;

	/* Perform selftest if asked for */
	if (inl_dev->selftest) {
		rc = nix_inl_selftest();
		if (rc)
			goto cpt_release;
	}

	idev->nix_inl_dev = inl_dev;

	return 0;
cpt_release:
	rc |= nix_inl_cpt_release(inl_dev);
sso_release:
	rc |= nix_inl_sso_release(inl_dev);
nix_release:
	rc |= nix_inl_nix_release(inl_dev);
lf_detach:
	rc |= nix_inl_lf_detach(inl_dev);
dev_cleanup:
	rc |= dev_fini(&inl_dev->dev, pci_dev);
error:
	return rc;
}

int
roc_nix_inl_dev_fini(struct roc_nix_inl_dev *roc_inl_dev)
{
	struct plt_pci_device *pci_dev;
	struct nix_inl_dev *inl_dev;
	struct idev_cfg *idev;
	int rc;

	idev = idev_get_cfg();
	if (idev == NULL)
		return 0;

	if (!idev->nix_inl_dev ||
	    PLT_PTR_DIFF(roc_inl_dev->reserved, idev->nix_inl_dev))
		return 0;

	inl_dev = idev->nix_inl_dev;
	pci_dev = inl_dev->pci_dev;

	/* Flush Inbound CTX cache entries */
	nix_inl_cpt_ctx_cache_sync(inl_dev);

	/* Release SSO */
	rc = nix_inl_sso_release(inl_dev);

	/* Release NIX */
	rc |= nix_inl_nix_release(inl_dev);

	/* Detach LF's */
	rc |= nix_inl_lf_detach(inl_dev);

	/* Cleanup mbox */
	rc |= dev_fini(&inl_dev->dev, pci_dev);
	if (rc)
		return rc;

	idev->nix_inl_dev = NULL;
	return 0;
}
