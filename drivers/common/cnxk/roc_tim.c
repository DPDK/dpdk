/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define LF_ENABLE_RETRY_CNT 8

static int
tim_fill_msix(struct roc_tim *roc_tim, uint16_t nb_ring)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct tim *tim = roc_tim_to_tim_priv(roc_tim);
	struct dev *dev = &sso->dev;
	struct msix_offset_rsp *rsp;
	struct mbox *mbox = mbox_get(dev->mbox);
	int i, rc;

	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc) {
		rc = -EIO;
		goto exit;
	}

	for (i = 0; i < nb_ring; i++)
		tim->tim_msix_offsets[i] = rsp->timlf_msixoff[i];

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static void
tim_err_desc(int rc)
{
	switch (rc) {
	case TIM_AF_NO_RINGS_LEFT:
		plt_err("Unable to allocate new TIM ring.");
		break;
	case TIM_AF_INVALID_NPA_PF_FUNC:
		plt_err("Invalid NPA pf func.");
		break;
	case TIM_AF_INVALID_SSO_PF_FUNC:
		plt_err("Invalid SSO pf func.");
		break;
	case TIM_AF_RING_STILL_RUNNING:
		plt_err("Ring busy.");
		break;
	case TIM_AF_LF_INVALID:
		plt_err("Invalid Ring id.");
		break;
	case TIM_AF_CSIZE_NOT_ALIGNED:
		plt_err("Chunk size specified needs to be multiple of 16.");
		break;
	case TIM_AF_CSIZE_TOO_SMALL:
		plt_err("Chunk size too small.");
		break;
	case TIM_AF_CSIZE_TOO_BIG:
		plt_err("Chunk size too big.");
		break;
	case TIM_AF_INTERVAL_TOO_SMALL:
		plt_err("Bucket traversal interval too small.");
		break;
	case TIM_AF_INVALID_BIG_ENDIAN_VALUE:
		plt_err("Invalid Big endian value.");
		break;
	case TIM_AF_INVALID_CLOCK_SOURCE:
		plt_err("Invalid Clock source specified.");
		break;
	case TIM_AF_GPIO_CLK_SRC_NOT_ENABLED:
		plt_err("GPIO clock source not enabled.");
		break;
	case TIM_AF_INVALID_BSIZE:
		plt_err("Invalid bucket size.");
		break;
	case TIM_AF_INVALID_ENABLE_PERIODIC:
		plt_err("Invalid bucket size.");
		break;
	case TIM_AF_INVALID_ENABLE_DONTFREE:
		plt_err("Invalid Don't free value.");
		break;
	case TIM_AF_ENA_DONTFRE_NSET_PERIODIC:
		plt_err("Don't free bit not set when periodic is enabled.");
		break;
	case TIM_AF_RING_ALREADY_DISABLED:
		plt_err("Ring already stopped");
		break;
	case TIM_AF_LF_START_SYNC_FAIL:
		plt_err("Ring start sync failed.");
		break;
	default:
		plt_err("Unknown Error: %d", rc);
	}
}

int
roc_tim_capture_counters(struct roc_tim *roc_tim, uint64_t *counters, uint8_t nb_cntrs)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_capture_rsp *rsp;
	int rc, i;

	mbox_alloc_msg_tim_capture_counters(mbox);
	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
		goto fail;
	}

	for (i = 0; i < nb_cntrs; i++)
		counters[i] = rsp->counters[i];

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_lf_enable(struct roc_tim *roc_tim, uint8_t ring_id, uint64_t *start_tsc,
		  uint32_t *cur_bkt)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	uint8_t retry_cnt = LF_ENABLE_RETRY_CNT;
	struct tim_enable_rsp *rsp;
	struct tim_ring_req *req;
	int rc = -ENOSPC;

retry:
	req = mbox_alloc_msg_tim_enable_ring(mbox);
	if (req == NULL)
		goto fail;
	req->ring = ring_id;

	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc) {
		if (rc == TIM_AF_LF_START_SYNC_FAIL && retry_cnt--)
			goto retry;

		tim_err_desc(rc);
		rc = -EIO;
		goto fail;
	}

	if (cur_bkt)
		*cur_bkt = rsp->currentbucket;
	if (start_tsc)
		*start_tsc = rsp->timestarted;

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_lf_disable(struct roc_tim *roc_tim, uint8_t ring_id)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_ring_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_disable_ring(mbox);
	if (req == NULL)
		goto fail;
	req->ring = ring_id;

	rc = mbox_process(mbox);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
	}

fail:
	mbox_put(mbox);
	return rc;
}

uintptr_t
roc_tim_lf_base_get(struct roc_tim *roc_tim, uint8_t ring_id)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;

	return dev->bar2 + (RVU_BLOCK_ADDR_TIM << 20 | ring_id << 12);
}

int
roc_tim_lf_config(struct roc_tim *roc_tim, uint8_t ring_id, enum roc_tim_clk_src clk_src,
		  uint8_t ena_periodic, uint8_t ena_dfb, uint32_t bucket_sz, uint32_t chunk_sz,
		  uint64_t interval, uint64_t intervalns, uint64_t clockfreq)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_config_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_config_ring(mbox);
	if (req == NULL)
		goto fail;
	req->ring = ring_id;
	req->bigendian = false;
	req->bucketsize = bucket_sz;
	req->chunksize = chunk_sz;
	req->clocksource = clk_src;
	req->enableperiodic = ena_periodic;
	req->enabledontfreebuffer = ena_dfb;
	req->interval_lo = interval;
	req->interval_hi = interval >> 32;
	req->intervalns = intervalns;
	req->clockfreq = clockfreq;
	req->gpioedge = TIM_GPIO_LTOH_TRANS;

	rc = mbox_process(mbox);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
	}

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_lf_config_hwwqe(struct roc_tim *roc_tim, uint8_t ring_id, struct roc_tim_hwwqe_cfg *cfg)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_cfg_hwwqe_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_config_hwwqe(mbox);
	if (req == NULL)
		goto fail;
	req->ring = ring_id;
	req->hwwqe_ena = cfg->hwwqe_ena;
	req->grp_ena = cfg->grp_ena;
	req->grp_tmo_cntr = cfg->grp_tmo_cyc;
	req->flw_ctrl_ena = cfg->flw_ctrl_ena;
	req->result_offset = cfg->result_offset;
	req->event_count_offset = cfg->event_count_offset;

	req->wqe_rd_clr_ena = 1;
	req->npa_tmo_cntr = TIM_NPA_TMO;
	req->ins_min_gap = TIM_BUCKET_MIN_GAP;

	rc = mbox_process(mbox);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
	}

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_lf_interval(struct roc_tim *roc_tim, enum roc_tim_clk_src clk_src,
		    uint64_t clockfreq, uint64_t *intervalns,
		    uint64_t *interval)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_intvl_req *req;
	struct tim_intvl_rsp *rsp;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_get_min_intvl(mbox);
	if (req == NULL)
		goto fail;

	req->clockfreq = clockfreq;
	req->clocksource = clk_src;
	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
		goto fail;
	}

	*intervalns = rsp->intvl_ns;
	*interval = rsp->intvl_cyc;

fail:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_lf_alloc(struct roc_tim *roc_tim, uint8_t ring_id, uint64_t *clk)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct tim *tim = roc_tim_to_tim_priv(roc_tim);
	struct tim_ring_req *free_req;
	struct tim_lf_alloc_req *req;
	struct tim_lf_alloc_rsp *rsp;
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_lf_alloc(mbox);
	if (req == NULL)
		goto fail;
	req->npa_pf_func = idev_npa_pffunc_get();
	req->sso_pf_func = idev_sso_pffunc_get();
	req->ring = ring_id;

	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc) {
		tim_err_desc(rc);
		rc = -EIO;
		goto fail;
	}

	if (clk)
		*clk = rsp->tenns_clk;

	rc = tim_register_irq_priv(roc_tim, sso->pci_dev->intr_handle, ring_id,
				   tim->tim_msix_offsets[ring_id]);
	if (rc < 0) {
		plt_tim_dbg("Failed to register Ring[%d] IRQ", ring_id);
		free_req = mbox_alloc_msg_tim_lf_free(mbox);
		if (free_req == NULL) {
			rc = -ENOSPC;
			goto fail;
		}
		free_req->ring = ring_id;
		rc = mbox_process(mbox);
		if (rc)
			rc = -EIO;
	}

fail:
	mbox_put(dev->mbox);
	return rc;
}

int
roc_tim_lf_free(struct roc_tim *roc_tim, uint8_t ring_id)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct tim *tim = roc_tim_to_tim_priv(roc_tim);
	struct dev *dev = &sso->dev;
	struct tim_ring_req *req;
	int rc = -ENOSPC;

	tim_unregister_irq_priv(roc_tim, sso->pci_dev->intr_handle, ring_id,
				tim->tim_msix_offsets[ring_id]);

	req = mbox_alloc_msg_tim_lf_free(mbox_get(dev->mbox));
	if (req == NULL)
		goto fail;
	req->ring = ring_id;

	rc = mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		rc = -EIO;
		goto fail;
	}
	rc = 0;

fail:
	mbox_put(dev->mbox);
	return rc;
}

int
tim_free_lf_count_get(struct dev *dev, uint16_t *nb_lfs)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct free_rsrcs_rsp *rsrc_cnt;
	int rc;

	mbox_alloc_msg_free_rsrc_cnt(mbox);
	rc = mbox_process_msg(mbox, (void **)&rsrc_cnt);
	if (rc) {
		plt_err("Failed to get free resource count");
		mbox_put(mbox);
		return -EIO;
	}

	*nb_lfs = rsrc_cnt->tim;
	mbox_put(mbox);

	return 0;
}

static int
tim_hw_info_get(struct roc_tim *roc_tim)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct tim_hw_info *rsp;
	int rc;

	mbox_alloc_msg_tim_get_hw_info(mbox);
	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc && rc != MBOX_MSG_INVALID) {
		plt_err("Failed to get TIM HW info");
		rc = -EIO;
		goto exit;
	}

	if (rc != MBOX_MSG_INVALID)
		mbox_memcpy(&roc_tim->feat, &rsp->feat, sizeof(roc_tim->feat));

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_tim_init(struct roc_tim *roc_tim)
{
	struct rsrc_attach_req *attach_req;
	struct rsrc_detach_req *detach_req;
	uint16_t nb_lfs, nb_free_lfs;
	struct sso *sso;
	struct dev *dev;
	int rc;

	if (roc_tim == NULL || roc_tim->roc_sso == NULL)
		return TIM_ERR_PARAM;

	sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	dev = &sso->dev;
	dev->roc_tim = roc_tim;
	PLT_STATIC_ASSERT(sizeof(struct tim) <= TIM_MEM_SZ);
	nb_lfs = roc_tim->nb_lfs;

	rc = tim_hw_info_get(roc_tim);
	if (rc) {
		plt_tim_dbg("Failed to get TIM HW info");
		return 0;
	}

	rc = tim_free_lf_count_get(dev, &nb_free_lfs);
	if (rc) {
		plt_tim_dbg("Failed to get TIM resource count");
		nb_lfs = 0;
		return nb_lfs;
	}

	if (nb_lfs && (nb_free_lfs < nb_lfs)) {
		plt_tim_dbg("Requested LFs : %d Available LFs : %d", nb_lfs, nb_free_lfs);
		nb_lfs = 0;
		return nb_lfs;
	}

	mbox_get(dev->mbox);
	attach_req = mbox_alloc_msg_attach_resources(dev->mbox);
	if (attach_req == NULL) {
		nb_lfs = 0;
		goto fail;
	}
	attach_req->modify = true;
	attach_req->timlfs = nb_lfs ? nb_lfs : nb_free_lfs;
	nb_lfs = attach_req->timlfs;

	rc = mbox_process(dev->mbox);
	if (rc) {
		plt_err("Unable to attach TIM LFs.");
		nb_lfs = 0;
		goto fail;
	}

	mbox_put(dev->mbox);
	rc = tim_fill_msix(roc_tim, nb_lfs);
	if (rc < 0) {
		plt_err("Unable to get TIM MSIX vectors");

		detach_req = mbox_alloc_msg_detach_resources(mbox_get(dev->mbox));
		if (detach_req == NULL) {
			nb_lfs = 0;
			goto fail;
		}
		detach_req->partial = true;
		detach_req->timlfs = true;
		mbox_process(dev->mbox);
		nb_lfs = 0;
	} else {
		goto done;
	}

fail:
	mbox_put(dev->mbox);
done:
	roc_tim->nb_lfs = nb_lfs;
	return nb_lfs;
}

void
roc_tim_fini(struct roc_tim *roc_tim)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_tim->roc_sso);
	struct rsrc_detach_req *detach_req;
	struct dev *dev = &sso->dev;
	struct mbox *mbox = mbox_get(dev->mbox);

	detach_req = mbox_alloc_msg_detach_resources(mbox);
	PLT_ASSERT(detach_req);
	detach_req->partial = true;
	detach_req->timlfs = true;

	mbox_process(mbox);
	mbox_put(mbox);
}
