/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

bool
roc_nix_is_lbk(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->lbk_link;
}

int
roc_nix_get_base_chan(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->rx_chan_base;
}

uint8_t
roc_nix_get_rx_chan_cnt(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->rx_chan_cnt;
}

uint16_t
roc_nix_get_vwqe_interval(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->vwqe_interval;
}

bool
roc_nix_is_sdp(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->sdp_link;
}

bool
roc_nix_is_pf(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return !dev_is_vf(&nix->dev);
}

int
roc_nix_get_pf(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev_get_pf(dev->pf_func);
}

int
roc_nix_get_vf(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev_get_vf(dev->pf_func);
}

bool
roc_nix_is_vf_or_sdp(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return (dev_is_vf(&nix->dev) != 0) || roc_nix_is_sdp(roc_nix);
}

uint16_t
roc_nix_get_pf_func(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->pf_func;
}

int
roc_nix_lf_inl_ipsec_cfg(struct roc_nix *roc_nix, struct roc_nix_ipsec_cfg *cfg,
			 bool enb)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_inline_ipsec_lf_cfg *lf_cfg;
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	int rc;

	lf_cfg = mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
	if (lf_cfg == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	if (enb) {
		lf_cfg->enable = 1;
		lf_cfg->sa_base_addr = cfg->iova;
		lf_cfg->ipsec_cfg1.sa_idx_w = plt_log2_u32(cfg->max_sa);
		lf_cfg->ipsec_cfg0.lenm1_max = roc_nix_max_pkt_len(roc_nix) - 1;
		lf_cfg->ipsec_cfg1.sa_idx_max = cfg->max_sa - 1;
		lf_cfg->ipsec_cfg0.sa_pow2_size = plt_log2_u32(cfg->sa_size);
		lf_cfg->ipsec_cfg0.tag_const = cfg->tag_const;
		lf_cfg->ipsec_cfg0.tt = cfg->tt;
	} else {
		lf_cfg->enable = 0;
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_cpt_ctx_cache_sync(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct msg_req *req;
	int rc;

	req = mbox_alloc_msg_cpt_ctx_cache_sync(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_max_pkt_len(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (roc_nix_is_sdp(roc_nix)) {
		if (roc_errata_nix_sdp_send_has_mtu_size_16k())
			return NIX_SDP_16K_HW_FRS;
		return NIX_SDP_MAX_HW_FRS;
	}

	if (roc_model_is_cn9k())
		return NIX_CN9K_MAX_HW_FRS;

	if (nix->lbk_link)
		return NIX_LBK_MAX_HW_FRS;

	return NIX_RPM_MAX_HW_FRS;
}

int
roc_nix_lf_alloc(struct roc_nix *roc_nix, uint32_t nb_rxq, uint32_t nb_txq,
		 uint64_t rx_cfg)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_lf_alloc_req *req;
	struct nix_lf_alloc_rsp *rsp;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_nix_lf_alloc(mbox);
	if (req == NULL)
		goto fail;
	req->rq_cnt = nb_rxq;
	req->sq_cnt = nb_txq;
	if (roc_nix->tx_compl_ena)
		req->cq_cnt = nb_rxq + nb_txq;
	else
		req->cq_cnt = nb_rxq;
	/* XQESZ can be W64 or W16 */
	req->xqe_sz = NIX_XQESZ_W16;
	req->rss_sz = nix->reta_sz;
	req->rss_grps = ROC_NIX_RSS_GRPS;
	req->npa_func = idev_npa_pffunc_get();
	req->sso_func = idev_sso_pffunc_get();
	req->rx_cfg = rx_cfg;
	if (roc_nix_is_lbk(roc_nix) && roc_nix->enable_loop &&
	    roc_model_is_cn98xx())
		req->flags = NIX_LF_LBK_BLK_SEL;

	if (!roc_nix->rss_tag_as_xor)
		req->flags |= NIX_LF_RSS_TAG_LSB_AS_ADDER;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto fail;

	nix->rx_cfg = rx_cfg;
	nix->sqb_size = rsp->sqb_size;
	nix->tx_chan_base = rsp->tx_chan_base;
	nix->rx_chan_base = rsp->rx_chan_base;
	if (roc_nix_is_lbk(roc_nix) && roc_nix->enable_loop)
		nix->tx_chan_base = rsp->rx_chan_base;
	nix->rx_chan_cnt = rsp->rx_chan_cnt;
	nix->tx_chan_cnt = rsp->tx_chan_cnt;
	nix->lso_tsov4_idx = rsp->lso_tsov4_idx;
	nix->lso_tsov6_idx = rsp->lso_tsov6_idx;
	nix->lf_tx_stats = rsp->lf_tx_stats;
	nix->lf_rx_stats = rsp->lf_rx_stats;
	nix->cints = rsp->cints;
	roc_nix->cints = rsp->cints;
	nix->qints = rsp->qints;
	nix->ptp_en = rsp->hw_rx_tstamp_en;
	roc_nix->rx_ptp_ena = rsp->hw_rx_tstamp_en;
	nix->cgx_links = rsp->cgx_links;
	nix->lbk_links = rsp->lbk_links;
	nix->sdp_links = rsp->sdp_links;
	nix->tx_link = rsp->tx_link;
	nix->nb_rx_queues = nb_rxq;
	nix->nb_tx_queues = nb_txq;

	nix->rqs = plt_zmalloc(sizeof(struct roc_nix_rq *) * nb_rxq, 0);
	if (!nix->rqs) {
		rc = -ENOMEM;
		goto fail;
	}

	nix->sqs = plt_zmalloc(sizeof(struct roc_nix_sq *) * nb_txq, 0);
	if (!nix->sqs) {
		rc = -ENOMEM;
		goto fail;
	}

	nix_tel_node_add(roc_nix);
fail:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_lf_free(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_lf_free_req *req;
	struct ndc_sync_op *ndc_req;
	int rc = -ENOSPC;

	plt_free(nix->rqs);
	plt_free(nix->sqs);
	nix->rqs = NULL;
	nix->sqs = NULL;

	/* Sync NDC-NIX for LF */
	ndc_req = mbox_alloc_msg_ndc_sync_op(mbox);
	if (ndc_req == NULL)
		goto exit;
	ndc_req->nix_lf_tx_sync = 1;
	ndc_req->nix_lf_rx_sync = 1;
	rc = mbox_process(mbox);
	if (rc)
		plt_err("Error on NDC-NIX-[TX, RX] LF sync, rc %d", rc);

	req = mbox_alloc_msg_nix_lf_free(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}
	/* Let AF driver free all this nix lf's
	 * NPC entries allocated using NPC MBOX.
	 */
	req->flags = 0;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

static inline int
nix_lf_attach(struct dev *dev)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct rsrc_attach_req *req;
	int rc = -ENOSPC;

	/* Attach NIX(lf) */
	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		goto exit;
	req->modify = true;
	req->nixlf = true;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

static inline int
nix_lf_get_msix_offset(struct dev *dev, struct nix *nix)
{
	struct msix_offset_rsp *msix_rsp;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	/* Get MSIX vector offsets */
	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(mbox, (void *)&msix_rsp);
	if (rc == 0)
		nix->msixoff = msix_rsp->nix_msixoff;

	mbox_put(mbox);
	return rc;
}

static inline int
nix_lf_detach(struct nix *nix)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct rsrc_detach_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		goto exit;
	req->partial = true;
	req->nixlf = true;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

static int
roc_nix_get_hw_info(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_hw_info *hw_info;
	int rc;

	mbox_alloc_msg_nix_get_hw_info(mbox);
	rc = mbox_process_msg(mbox, (void *)&hw_info);
	if (rc == 0) {
		nix->vwqe_interval = hw_info->vwqe_delay;
		if (nix->lbk_link)
			roc_nix->dwrr_mtu = hw_info->lbk_dwrr_mtu;
		else if (nix->sdp_link)
			roc_nix->dwrr_mtu = hw_info->sdp_dwrr_mtu;
		else
			roc_nix->dwrr_mtu = hw_info->rpm_dwrr_mtu;
	}

	mbox_put(mbox);
	return rc;
}

static void
sdp_lbk_id_update(struct plt_pci_device *pci_dev, struct nix *nix)
{
	nix->sdp_link = false;
	nix->lbk_link = false;

	/* Update SDP/LBK link based on PCI device id */
	switch (pci_dev->id.device_id) {
	case PCI_DEVID_CNXK_RVU_SDP_PF:
	case PCI_DEVID_CNXK_RVU_SDP_VF:
		nix->sdp_link = true;
		break;
	case PCI_DEVID_CNXK_RVU_AF_VF:
		nix->lbk_link = true;
		break;
	default:
		break;
	}
}

uint64_t
nix_get_blkaddr(struct dev *dev)
{
	uint64_t reg;

	/* Reading the discovery register to know which NIX is the LF
	 * attached to.
	 */
	reg = plt_read64(dev->bar2 +
			 RVU_PF_BLOCK_ADDRX_DISC(RVU_BLOCK_ADDR_NIX0));

	return reg & 0x1FFULL ? RVU_BLOCK_ADDR_NIX0 : RVU_BLOCK_ADDR_NIX1;
}

int
roc_nix_dev_init(struct roc_nix *roc_nix)
{
	enum roc_nix_rss_reta_sz reta_sz;
	struct plt_pci_device *pci_dev;
	struct roc_nix_list *nix_list;
	uint16_t max_sqb_count;
	uint64_t blkaddr;
	struct dev *dev;
	struct nix *nix;
	int rc;

	if (roc_nix == NULL || roc_nix->pci_dev == NULL)
		return NIX_ERR_PARAM;

	reta_sz = roc_nix->reta_sz;
	if (reta_sz != 0 && reta_sz != 64 && reta_sz != 128 && reta_sz != 256)
		return NIX_ERR_PARAM;

	if (reta_sz == 0)
		reta_sz = ROC_NIX_RSS_RETA_SZ_64;

	max_sqb_count = roc_nix->max_sqb_count;
	max_sqb_count = PLT_MIN(max_sqb_count, NIX_MAX_SQB);
	max_sqb_count = PLT_MAX(max_sqb_count, NIX_MIN_SQB);
	roc_nix->max_sqb_count = max_sqb_count;

	PLT_STATIC_ASSERT(sizeof(struct nix) <= ROC_NIX_MEM_SZ);
	nix = roc_nix_to_nix_priv(roc_nix);
	pci_dev = roc_nix->pci_dev;
	dev = &nix->dev;

	nix_list = roc_idev_nix_list_get();
	if (nix_list == NULL)
		return -EINVAL;

	TAILQ_INSERT_TAIL(nix_list, roc_nix, next);

	if (nix->dev.drv_inited)
		return 0;

	if (dev->mbox_active)
		goto skip_dev_init;

	memset(nix, 0, sizeof(*nix));

	/* Since 0 is a valid BPID, use -1 to represent invalid value. */
	memset(nix->bpid, -1, sizeof(nix->bpid));

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

skip_dev_init:
	dev->roc_nix = roc_nix;

	nix->lmt_base = dev->lmt_base;
	/* Expose base LMT line address for
	 * "Per Core LMT line" mode.
	 */
	roc_nix->lmt_base = dev->lmt_base;

	/* Attach NIX LF */
	rc = nix_lf_attach(dev);
	if (rc)
		goto dev_fini;

	blkaddr = nix_get_blkaddr(dev);
	nix->is_nix1 = (blkaddr == RVU_BLOCK_ADDR_NIX1);

	/* Calculating base address based on which NIX block LF
	 * is attached to.
	 */
	nix->base = dev->bar2 + (blkaddr << 20);

	/* Get NIX MSIX offset */
	rc = nix_lf_get_msix_offset(dev, nix);
	if (rc)
		goto lf_detach;

	/* Update nix context */
	sdp_lbk_id_update(pci_dev, nix);
	nix->pci_dev = pci_dev;
	nix->reta_sz = reta_sz;
	nix->mtu = roc_nix_max_pkt_len(roc_nix);
	nix->dmac_flt_idx = -1;

	/* Register error and ras interrupts */
	rc = nix_register_irqs(nix);
	if (rc)
		goto lf_detach;

	rc = nix_tm_conf_init(roc_nix);
	if (rc)
		goto unregister_irqs;

	/* Get NIX HW info */
	roc_nix_get_hw_info(roc_nix);
	nix->dev.drv_inited = true;

	return 0;
unregister_irqs:
	nix_unregister_irqs(nix);
lf_detach:
	nix_lf_detach(nix);
dev_fini:
	rc |= dev_fini(dev, pci_dev);
fail:
	nix_tel_node_del(roc_nix);
	return rc;
}

int
roc_nix_dev_fini(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc = 0;

	if (nix == NULL)
		return NIX_ERR_PARAM;

	if (!nix->dev.drv_inited)
		goto fini;

	nix_tm_conf_fini(roc_nix);
	nix_unregister_irqs(nix);

	rc = nix_lf_detach(nix);
	nix->dev.drv_inited = false;
fini:
	rc |= dev_fini(&nix->dev, nix->pci_dev);
	return rc;
}
