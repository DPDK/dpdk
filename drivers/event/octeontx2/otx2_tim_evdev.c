/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>

#include "otx2_evdev.h"
#include "otx2_tim_evdev.h"

static struct rte_event_timer_adapter_ops otx2_tim_ops;

static int
tim_chnk_pool_create(struct otx2_tim_ring *tim_ring,
		     struct rte_event_timer_adapter_conf *rcfg)
{
	unsigned int cache_sz = (tim_ring->nb_chunks / 1.5);
	unsigned int mp_flags = 0;
	char pool_name[25];
	int rc;

	/* Create chunk pool. */
	if (rcfg->flags & RTE_EVENT_TIMER_ADAPTER_F_SP_PUT) {
		mp_flags = MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET;
		otx2_tim_dbg("Using single producer mode");
		tim_ring->prod_type_sp = true;
	}

	snprintf(pool_name, sizeof(pool_name), "otx2_tim_chunk_pool%d",
		 tim_ring->ring_id);

	if (cache_sz > RTE_MEMPOOL_CACHE_MAX_SIZE)
		cache_sz = RTE_MEMPOOL_CACHE_MAX_SIZE;

	/* NPA need not have cache as free is not visible to SW */
	tim_ring->chunk_pool = rte_mempool_create_empty(pool_name,
							tim_ring->nb_chunks,
							tim_ring->chunk_sz,
							0, 0, rte_socket_id(),
							mp_flags);

	if (tim_ring->chunk_pool == NULL) {
		otx2_err("Unable to create chunkpool.");
		return -ENOMEM;
	}

	rc = rte_mempool_set_ops_byname(tim_ring->chunk_pool,
					rte_mbuf_platform_mempool_ops(), NULL);
	if (rc < 0) {
		otx2_err("Unable to set chunkpool ops");
		goto free;
	}

	rc = rte_mempool_populate_default(tim_ring->chunk_pool);
	if (rc < 0) {
		otx2_err("Unable to set populate chunkpool.");
		goto free;
	}
	tim_ring->aura = npa_lf_aura_handle_to_aura(
						tim_ring->chunk_pool->pool_id);
	tim_ring->ena_dfb = 0;

	return 0;

free:
	rte_mempool_free(tim_ring->chunk_pool);
	return rc;
}

static void
tim_err_desc(int rc)
{
	switch (rc) {
	case TIM_AF_NO_RINGS_LEFT:
		otx2_err("Unable to allocat new TIM ring.");
		break;
	case TIM_AF_INVALID_NPA_PF_FUNC:
		otx2_err("Invalid NPA pf func.");
		break;
	case TIM_AF_INVALID_SSO_PF_FUNC:
		otx2_err("Invalid SSO pf func.");
		break;
	case TIM_AF_RING_STILL_RUNNING:
		otx2_tim_dbg("Ring busy.");
		break;
	case TIM_AF_LF_INVALID:
		otx2_err("Invalid Ring id.");
		break;
	case TIM_AF_CSIZE_NOT_ALIGNED:
		otx2_err("Chunk size specified needs to be multiple of 16.");
		break;
	case TIM_AF_CSIZE_TOO_SMALL:
		otx2_err("Chunk size too small.");
		break;
	case TIM_AF_CSIZE_TOO_BIG:
		otx2_err("Chunk size too big.");
		break;
	case TIM_AF_INTERVAL_TOO_SMALL:
		otx2_err("Bucket traversal interval too small.");
		break;
	case TIM_AF_INVALID_BIG_ENDIAN_VALUE:
		otx2_err("Invalid Big endian value.");
		break;
	case TIM_AF_INVALID_CLOCK_SOURCE:
		otx2_err("Invalid Clock source specified.");
		break;
	case TIM_AF_GPIO_CLK_SRC_NOT_ENABLED:
		otx2_err("GPIO clock source not enabled.");
		break;
	case TIM_AF_INVALID_BSIZE:
		otx2_err("Invalid bucket size.");
		break;
	case TIM_AF_INVALID_ENABLE_PERIODIC:
		otx2_err("Invalid bucket size.");
		break;
	case TIM_AF_INVALID_ENABLE_DONTFREE:
		otx2_err("Invalid Don't free value.");
		break;
	case TIM_AF_ENA_DONTFRE_NSET_PERIODIC:
		otx2_err("Don't free bit not set when periodic is enabled.");
		break;
	case TIM_AF_RING_ALREADY_DISABLED:
		otx2_err("Ring already stopped");
		break;
	default:
		otx2_err("Unknown Error.");
	}
}

static int
otx2_tim_ring_create(struct rte_event_timer_adapter *adptr)
{
	struct rte_event_timer_adapter_conf *rcfg = &adptr->data->conf;
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct otx2_tim_ring *tim_ring;
	struct tim_config_req *cfg_req;
	struct tim_ring_req *free_req;
	struct tim_lf_alloc_req *req;
	struct tim_lf_alloc_rsp *rsp;
	uint64_t nb_timers;
	int rc;

	if (dev == NULL)
		return -ENODEV;

	if (adptr->data->id >= dev->nb_rings)
		return -ENODEV;

	req = otx2_mbox_alloc_msg_tim_lf_alloc(dev->mbox);
	req->npa_pf_func = otx2_npa_pf_func_get();
	req->sso_pf_func = otx2_sso_pf_func_get();
	req->ring = adptr->data->id;

	rc = otx2_mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc < 0) {
		tim_err_desc(rc);
		return -ENODEV;
	}

	if (NSEC2TICK(RTE_ALIGN_MUL_CEIL(rcfg->timer_tick_ns, 10),
		      rsp->tenns_clk) < OTX2_TIM_MIN_TMO_TKS) {
		rc = -ERANGE;
		goto rng_mem_err;
	}

	tim_ring = rte_zmalloc("otx2_tim_prv", sizeof(struct otx2_tim_ring), 0);
	if (tim_ring == NULL) {
		rc =  -ENOMEM;
		goto rng_mem_err;
	}

	adptr->data->adapter_priv = tim_ring;

	tim_ring->tenns_clk_freq = rsp->tenns_clk;
	tim_ring->clk_src = (int)rcfg->clk_src;
	tim_ring->ring_id = adptr->data->id;
	tim_ring->tck_nsec = RTE_ALIGN_MUL_CEIL(rcfg->timer_tick_ns, 10);
	tim_ring->max_tout = rcfg->max_tmo_ns;
	tim_ring->nb_bkts = (tim_ring->max_tout / tim_ring->tck_nsec);
	tim_ring->chunk_sz = OTX2_TIM_RING_DEF_CHUNK_SZ;
	nb_timers = rcfg->nb_timers;
	tim_ring->nb_chunks = nb_timers / OTX2_TIM_NB_CHUNK_SLOTS(
							tim_ring->chunk_sz);
	tim_ring->nb_chunk_slots = OTX2_TIM_NB_CHUNK_SLOTS(tim_ring->chunk_sz);

	/* Create buckets. */
	tim_ring->bkt = rte_zmalloc("otx2_tim_bucket", (tim_ring->nb_bkts) *
				    sizeof(struct otx2_tim_bkt),
				    RTE_CACHE_LINE_SIZE);
	if (tim_ring->bkt == NULL)
		goto bkt_mem_err;

	rc = tim_chnk_pool_create(tim_ring, rcfg);
	if (rc < 0)
		goto chnk_mem_err;

	cfg_req = otx2_mbox_alloc_msg_tim_config_ring(dev->mbox);

	cfg_req->ring = tim_ring->ring_id;
	cfg_req->bigendian = false;
	cfg_req->clocksource = tim_ring->clk_src;
	cfg_req->enableperiodic = false;
	cfg_req->enabledontfreebuffer = tim_ring->ena_dfb;
	cfg_req->bucketsize = tim_ring->nb_bkts;
	cfg_req->chunksize = tim_ring->chunk_sz;
	cfg_req->interval = NSEC2TICK(tim_ring->tck_nsec,
				      tim_ring->tenns_clk_freq);

	rc = otx2_mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		goto chnk_mem_err;
	}

	tim_ring->base = dev->bar2 +
		(RVU_BLOCK_ADDR_TIM << 20 | tim_ring->ring_id << 12);

	otx2_write64((uint64_t)tim_ring->bkt,
		     tim_ring->base + TIM_LF_RING_BASE);
	otx2_write64(tim_ring->aura, tim_ring->base + TIM_LF_RING_AURA);

	return rc;

chnk_mem_err:
	rte_free(tim_ring->bkt);
bkt_mem_err:
	rte_free(tim_ring);
rng_mem_err:
	free_req = otx2_mbox_alloc_msg_tim_lf_free(dev->mbox);
	free_req->ring = adptr->data->id;
	otx2_mbox_process(dev->mbox);
	return rc;
}

static int
otx2_tim_ring_free(struct rte_event_timer_adapter *adptr)
{
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct tim_ring_req *req;
	int rc;

	if (dev == NULL)
		return -ENODEV;

	req = otx2_mbox_alloc_msg_tim_lf_free(dev->mbox);
	req->ring = tim_ring->ring_id;

	rc = otx2_mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		return -EBUSY;
	}

	rte_free(tim_ring->bkt);
	rte_mempool_free(tim_ring->chunk_pool);
	rte_free(adptr->data->adapter_priv);

	return 0;
}

int
otx2_tim_caps_get(const struct rte_eventdev *evdev, uint64_t flags,
		  uint32_t *caps,
		  const struct rte_event_timer_adapter_ops **ops)
{
	struct otx2_tim_evdev *dev = tim_priv_get();

	RTE_SET_USED(flags);
	if (dev == NULL)
		return -ENODEV;

	otx2_tim_ops.init = otx2_tim_ring_create;
	otx2_tim_ops.uninit = otx2_tim_ring_free;

	/* Store evdev pointer for later use. */
	dev->event_dev = (struct rte_eventdev *)(uintptr_t)evdev;
	*caps = RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT;
	*ops = &otx2_tim_ops;

	return 0;
}

void
otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev)
{
	struct rsrc_attach_req *atch_req;
	struct free_rsrcs_rsp *rsrc_cnt;
	const struct rte_memzone *mz;
	struct otx2_tim_evdev *dev;
	int rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	mz = rte_memzone_reserve(RTE_STR(OTX2_TIM_EVDEV_NAME),
				 sizeof(struct otx2_tim_evdev),
				 rte_socket_id(), 0);
	if (mz == NULL) {
		otx2_tim_dbg("Unable to allocate memory for TIM Event device");
		return;
	}

	dev = mz->addr;
	dev->pci_dev = pci_dev;
	dev->mbox = cmn_dev->mbox;
	dev->bar2 = cmn_dev->bar2;

	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count.");
		goto mz_free;
	}

	dev->nb_rings = rsrc_cnt->tim;

	if (!dev->nb_rings) {
		otx2_tim_dbg("No TIM Logical functions provisioned.");
		goto mz_free;
	}

	atch_req = otx2_mbox_alloc_msg_attach_resources(dev->mbox);
	atch_req->modify = true;
	atch_req->timlfs = dev->nb_rings;

	rc = otx2_mbox_process(dev->mbox);
	if (rc < 0) {
		otx2_err("Unable to attach TIM rings.");
		goto mz_free;
	}

	return;

mz_free:
	rte_memzone_free(mz);
}

void
otx2_tim_fini(void)
{
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct rsrc_detach_req *dtch_req;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	dtch_req = otx2_mbox_alloc_msg_detach_resources(dev->mbox);
	dtch_req->partial = true;
	dtch_req->timlfs = true;

	otx2_mbox_process(dev->mbox);
	rte_memzone_free(rte_memzone_lookup(RTE_STR(OTX2_TIM_EVDEV_NAME)));
}
