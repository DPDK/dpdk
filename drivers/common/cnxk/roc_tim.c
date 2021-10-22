/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static int
tim_fill_msix(struct roc_tim *roc_tim, uint16_t nb_ring)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct tim *tim = roc_tim_to_tim_priv(roc_tim);
	struct msix_offset_rsp *rsp;
	int i, rc;

	mbox_alloc_msg_msix_offset(dev->mbox);
	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc < 0)
		return rc;

	for (i = 0; i < nb_ring; i++)
		tim->tim_msix_offsets[i] = rsp->timlf_msixoff[i];

	return 0;
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
	default:
		plt_err("Unknown Error.");
	}
}

int
roc_tim_lf_enable(struct roc_tim *roc_tim, uint8_t ring_id, uint64_t *start_tsc,
		  uint32_t *cur_bkt)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct tim_enable_rsp *rsp;
	struct tim_ring_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_enable_ring(dev->mbox);
	if (req == NULL)
		return rc;
	req->ring = ring_id;

	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc < 0) {
		tim_err_desc(rc);
		return rc;
	}

	if (cur_bkt)
		*cur_bkt = rsp->currentbucket;
	if (start_tsc)
		*start_tsc = rsp->timestarted;

	return 0;
}

int
roc_tim_lf_disable(struct roc_tim *roc_tim, uint8_t ring_id)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct tim_ring_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_disable_ring(dev->mbox);
	if (req == NULL)
		return rc;
	req->ring = ring_id;

	rc = mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		return rc;
	}

	return 0;
}

uintptr_t
roc_tim_lf_base_get(struct roc_tim *roc_tim, uint8_t ring_id)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;

	return dev->bar2 + (RVU_BLOCK_ADDR_TIM << 20 | ring_id << 12);
}

int
roc_tim_lf_config(struct roc_tim *roc_tim, uint8_t ring_id,
		  enum roc_tim_clk_src clk_src, uint8_t ena_periodic,
		  uint8_t ena_dfb, uint32_t bucket_sz, uint32_t chunk_sz,
		  uint32_t interval)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct tim_config_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_config_ring(dev->mbox);
	if (req == NULL)
		return rc;
	req->ring = ring_id;
	req->bigendian = false;
	req->bucketsize = bucket_sz;
	req->chunksize = chunk_sz;
	req->clocksource = clk_src;
	req->enableperiodic = ena_periodic;
	req->enabledontfreebuffer = ena_dfb;
	req->interval = interval;
	req->gpioedge = TIM_GPIO_LTOH_TRANS;

	rc = mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		return rc;
	}

	return 0;
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
	int rc = -ENOSPC;

	req = mbox_alloc_msg_tim_lf_alloc(dev->mbox);
	if (req == NULL)
		return rc;
	req->npa_pf_func = idev_npa_pffunc_get();
	req->sso_pf_func = idev_sso_pffunc_get();
	req->ring = ring_id;

	rc = mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc < 0) {
		tim_err_desc(rc);
		return rc;
	}

	if (clk)
		*clk = rsp->tenns_clk;

	rc = tim_register_irq_priv(roc_tim, sso->pci_dev->intr_handle, ring_id,
				   tim->tim_msix_offsets[ring_id]);
	if (rc < 0) {
		plt_tim_dbg("Failed to register Ring[%d] IRQ", ring_id);
		free_req = mbox_alloc_msg_tim_lf_free(dev->mbox);
		if (free_req == NULL)
			return -ENOSPC;
		free_req->ring = ring_id;
		mbox_process(dev->mbox);
	}

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

	req = mbox_alloc_msg_tim_lf_free(dev->mbox);
	if (req == NULL)
		return rc;
	req->ring = ring_id;

	rc = mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		return rc;
	}

	return 0;
}

int
roc_tim_init(struct roc_tim *roc_tim)
{
	struct rsrc_attach_req *attach_req;
	struct rsrc_detach_req *detach_req;
	struct free_rsrcs_rsp *free_rsrc;
	struct dev *dev;
	uint16_t nb_lfs;
	int rc;

	if (roc_tim == NULL || roc_tim->roc_sso == NULL)
		return TIM_ERR_PARAM;

	PLT_STATIC_ASSERT(sizeof(struct tim) <= TIM_MEM_SZ);
	dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	nb_lfs = roc_tim->nb_lfs;
	mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = mbox_process_msg(dev->mbox, (void *)&free_rsrc);
	if (rc < 0) {
		plt_err("Unable to get free rsrc count.");
		return 0;
	}

	if (nb_lfs && (free_rsrc->tim < nb_lfs)) {
		plt_tim_dbg("Requested LFs : %d Available LFs : %d", nb_lfs,
			    free_rsrc->tim);
		return 0;
	}

	attach_req = mbox_alloc_msg_attach_resources(dev->mbox);
	if (attach_req == NULL)
		return -ENOSPC;
	attach_req->modify = true;
	attach_req->timlfs = nb_lfs ? nb_lfs : free_rsrc->tim;
	nb_lfs = attach_req->timlfs;

	rc = mbox_process(dev->mbox);
	if (rc < 0) {
		plt_err("Unable to attach TIM LFs.");
		return 0;
	}

	rc = tim_fill_msix(roc_tim, nb_lfs);
	if (rc < 0) {
		plt_err("Unable to get TIM MSIX vectors");

		detach_req = mbox_alloc_msg_detach_resources(dev->mbox);
		if (detach_req == NULL)
			return -ENOSPC;
		detach_req->partial = true;
		detach_req->timlfs = true;
		mbox_process(dev->mbox);

		return 0;
	}

	return nb_lfs;
}

void
roc_tim_fini(struct roc_tim *roc_tim)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	struct rsrc_detach_req *detach_req;

	detach_req = mbox_alloc_msg_detach_resources(dev->mbox);
	PLT_ASSERT(detach_req);
	detach_req->partial = true;
	detach_req->timlfs = true;

	mbox_process(dev->mbox);
}
