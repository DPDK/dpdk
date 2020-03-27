/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>

#include "otx2_evdev.h"
#include "otx2_tim_evdev.h"

static struct rte_event_timer_adapter_ops otx2_tim_ops;

static inline int
tim_get_msix_offsets(void)
{
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct otx2_mbox *mbox = dev->mbox;
	struct msix_offset_rsp *msix_rsp;
	int i, rc;

	/* Get TIM MSIX vector offsets */
	otx2_mbox_alloc_msg_msix_offset(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&msix_rsp);

	for (i = 0; i < dev->nb_rings; i++)
		dev->tim_msixoff[i] = msix_rsp->timlf_msixoff[i];

	return rc;
}

static void
tim_set_fp_ops(struct otx2_tim_ring *tim_ring)
{
	uint8_t prod_flag = !tim_ring->prod_type_sp;

	/* [MOD/AND] [DFB/FB] [SP][MP]*/
	const rte_event_timer_arm_burst_t arm_burst[2][2][2][2] = {
#define FP(_name, _f4, _f3, _f2, _f1, flags) \
		[_f4][_f3][_f2][_f1] = otx2_tim_arm_burst_ ## _name,
TIM_ARM_FASTPATH_MODES
#undef FP
	};

	const rte_event_timer_arm_tmo_tick_burst_t arm_tmo_burst[2][2][2] = {
#define FP(_name, _f3, _f2, _f1, flags) \
		[_f3][_f2][_f1] = otx2_tim_arm_tmo_tick_burst_ ## _name,
TIM_ARM_TMO_FASTPATH_MODES
#undef FP
	};

	otx2_tim_ops.arm_burst =
		arm_burst[tim_ring->enable_stats][tim_ring->optimized]
			[tim_ring->ena_dfb][prod_flag];
	otx2_tim_ops.arm_tmo_tick_burst =
		arm_tmo_burst[tim_ring->enable_stats][tim_ring->optimized]
			[tim_ring->ena_dfb];
	otx2_tim_ops.cancel_burst = otx2_tim_timer_cancel_burst;
}

static void
otx2_tim_ring_info_get(const struct rte_event_timer_adapter *adptr,
		       struct rte_event_timer_adapter_info *adptr_info)
{
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;

	adptr_info->max_tmo_ns = tim_ring->max_tout;
	adptr_info->min_resolution_ns = tim_ring->tck_nsec;
	rte_memcpy(&adptr_info->conf, &adptr->data->conf,
		   sizeof(struct rte_event_timer_adapter_conf));
}

static void
tim_optimze_bkt_param(struct otx2_tim_ring *tim_ring)
{
	uint64_t tck_nsec;
	uint32_t hbkts;
	uint32_t lbkts;

	hbkts = rte_align32pow2(tim_ring->nb_bkts);
	tck_nsec = RTE_ALIGN_MUL_CEIL(tim_ring->max_tout / (hbkts - 1), 10);

	if ((tck_nsec < TICK2NSEC(OTX2_TIM_MIN_TMO_TKS,
				  tim_ring->tenns_clk_freq) ||
	    hbkts > OTX2_TIM_MAX_BUCKETS))
		hbkts = 0;

	lbkts = rte_align32prevpow2(tim_ring->nb_bkts);
	tck_nsec = RTE_ALIGN_MUL_CEIL((tim_ring->max_tout / (lbkts - 1)), 10);

	if ((tck_nsec < TICK2NSEC(OTX2_TIM_MIN_TMO_TKS,
				  tim_ring->tenns_clk_freq) ||
	    lbkts > OTX2_TIM_MAX_BUCKETS))
		lbkts = 0;

	if (!hbkts && !lbkts)
		return;

	if (!hbkts) {
		tim_ring->nb_bkts = lbkts;
		goto end;
	} else if (!lbkts) {
		tim_ring->nb_bkts = hbkts;
		goto end;
	}

	tim_ring->nb_bkts = (hbkts - tim_ring->nb_bkts) <
		(tim_ring->nb_bkts - lbkts) ? hbkts : lbkts;
end:
	tim_ring->optimized = true;
	tim_ring->tck_nsec = RTE_ALIGN_MUL_CEIL((tim_ring->max_tout /
						(tim_ring->nb_bkts - 1)), 10);
	otx2_tim_dbg("Optimized configured values");
	otx2_tim_dbg("Nb_bkts  : %" PRIu32 "", tim_ring->nb_bkts);
	otx2_tim_dbg("Tck_nsec : %" PRIu64 "", tim_ring->tck_nsec);
}

static int
tim_chnk_pool_create(struct otx2_tim_ring *tim_ring,
		     struct rte_event_timer_adapter_conf *rcfg)
{
	unsigned int cache_sz = (tim_ring->nb_chunks / 1.5);
	unsigned int mp_flags = 0;
	char pool_name[25];
	int rc;

	cache_sz /= rte_lcore_count();
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

	if (!tim_ring->disable_npa) {
		tim_ring->chunk_pool = rte_mempool_create_empty(pool_name,
				tim_ring->nb_chunks, tim_ring->chunk_sz,
				cache_sz, 0, rte_socket_id(), mp_flags);

		if (tim_ring->chunk_pool == NULL) {
			otx2_err("Unable to create chunkpool.");
			return -ENOMEM;
		}

		rc = rte_mempool_set_ops_byname(tim_ring->chunk_pool,
						rte_mbuf_platform_mempool_ops(),
						NULL);
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
	} else {
		tim_ring->chunk_pool = rte_mempool_create(pool_name,
				tim_ring->nb_chunks, tim_ring->chunk_sz,
				cache_sz, 0, NULL, NULL, NULL, NULL,
				rte_socket_id(),
				mp_flags);
		if (tim_ring->chunk_pool == NULL) {
			otx2_err("Unable to create chunkpool.");
			return -ENOMEM;
		}
		tim_ring->ena_dfb = 1;
	}

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
	int i, rc;

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
		if (rcfg->flags & RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES)
			rcfg->timer_tick_ns = TICK2NSEC(OTX2_TIM_MIN_TMO_TKS,
					rsp->tenns_clk);
		else {
			rc = -ERANGE;
			goto rng_mem_err;
		}
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
	tim_ring->chunk_sz = dev->chunk_sz;
	tim_ring->nb_timers = rcfg->nb_timers;
	tim_ring->disable_npa = dev->disable_npa;
	tim_ring->enable_stats = dev->enable_stats;

	for (i = 0; i < dev->ring_ctl_cnt ; i++) {
		struct otx2_tim_ctl *ring_ctl = &dev->ring_ctl_data[i];

		if (ring_ctl->ring == tim_ring->ring_id) {
			tim_ring->chunk_sz = ring_ctl->chunk_slots ?
				((uint32_t)(ring_ctl->chunk_slots + 1) *
				 OTX2_TIM_CHUNK_ALIGNMENT) : tim_ring->chunk_sz;
			tim_ring->enable_stats = ring_ctl->enable_stats;
			tim_ring->disable_npa = ring_ctl->disable_npa;
		}
	}

	tim_ring->nb_chunks = tim_ring->nb_timers / OTX2_TIM_NB_CHUNK_SLOTS(
							tim_ring->chunk_sz);
	tim_ring->nb_chunk_slots = OTX2_TIM_NB_CHUNK_SLOTS(tim_ring->chunk_sz);

	/* Try to optimize the bucket parameters. */
	if ((rcfg->flags & RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES)) {
		if (rte_is_power_of_2(tim_ring->nb_bkts))
			tim_ring->optimized = true;
		else
			tim_optimze_bkt_param(tim_ring);
	}

	if (tim_ring->disable_npa)
		tim_ring->nb_chunks = tim_ring->nb_chunks * tim_ring->nb_bkts;
	else
		tim_ring->nb_chunks = tim_ring->nb_chunks + tim_ring->nb_bkts;

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

	rc = tim_register_irq(tim_ring->ring_id);
	if (rc < 0)
		goto chnk_mem_err;

	otx2_write64((uint64_t)tim_ring->bkt,
		     tim_ring->base + TIM_LF_RING_BASE);
	otx2_write64(tim_ring->aura, tim_ring->base + TIM_LF_RING_AURA);

	/* Set fastpath ops. */
	tim_set_fp_ops(tim_ring);

	/* Update SSO xae count. */
	sso_updt_xae_cnt(sso_pmd_priv(dev->event_dev), (void *)tim_ring,
			 RTE_EVENT_TYPE_TIMER);
	sso_xae_reconfigure(dev->event_dev);

	otx2_tim_dbg("Total memory used %"PRIu64"MB\n",
			(uint64_t)(((tim_ring->nb_chunks * tim_ring->chunk_sz)
			+ (tim_ring->nb_bkts * sizeof(struct otx2_tim_bkt))) /
			BIT_ULL(20)));

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

static void
otx2_tim_calibrate_start_tsc(struct otx2_tim_ring *tim_ring)
{
#define OTX2_TIM_CALIB_ITER	1E6
	uint32_t real_bkt, bucket;
	int icount, ecount = 0;
	uint64_t bkt_cyc;

	for (icount = 0; icount < OTX2_TIM_CALIB_ITER; icount++) {
		real_bkt = otx2_read64(tim_ring->base + TIM_LF_RING_REL) >> 44;
		bkt_cyc = rte_rdtsc();
		bucket = (bkt_cyc - tim_ring->ring_start_cyc) /
							tim_ring->tck_int;
		bucket = bucket % (tim_ring->nb_bkts);
		tim_ring->ring_start_cyc = bkt_cyc - (real_bkt *
							tim_ring->tck_int);
		if (bucket != real_bkt)
			ecount++;
	}
	tim_ring->last_updt_cyc = bkt_cyc;
	otx2_tim_dbg("Bucket mispredict %3.2f distance %d\n",
		     100 - (((double)(icount - ecount) / (double)icount) * 100),
		     bucket - real_bkt);
}

static int
otx2_tim_ring_start(const struct rte_event_timer_adapter *adptr)
{
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct tim_enable_rsp *rsp;
	struct tim_ring_req *req;
	int rc;

	if (dev == NULL)
		return -ENODEV;

	req = otx2_mbox_alloc_msg_tim_enable_ring(dev->mbox);
	req->ring = tim_ring->ring_id;

	rc = otx2_mbox_process_msg(dev->mbox, (void **)&rsp);
	if (rc < 0) {
		tim_err_desc(rc);
		goto fail;
	}
#ifdef RTE_ARM_EAL_RDTSC_USE_PMU
	uint64_t tenns_stmp, tenns_diff;
	uint64_t pmu_stmp;

	pmu_stmp = rte_rdtsc();
	asm volatile("mrs %0, cntvct_el0" : "=r" (tenns_stmp));

	tenns_diff = tenns_stmp - rsp->timestarted;
	pmu_stmp = pmu_stmp - (NSEC2TICK(tenns_diff  * 10, rte_get_timer_hz()));
	tim_ring->ring_start_cyc = pmu_stmp;
#else
	tim_ring->ring_start_cyc = rsp->timestarted;
#endif
	tim_ring->tck_int = NSEC2TICK(tim_ring->tck_nsec, rte_get_timer_hz());
	tim_ring->tot_int = tim_ring->tck_int * tim_ring->nb_bkts;
	tim_ring->fast_div = rte_reciprocal_value_u64(tim_ring->tck_int);

	otx2_tim_calibrate_start_tsc(tim_ring);

fail:
	return rc;
}

static int
otx2_tim_ring_stop(const struct rte_event_timer_adapter *adptr)
{
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct tim_ring_req *req;
	int rc;

	if (dev == NULL)
		return -ENODEV;

	req = otx2_mbox_alloc_msg_tim_disable_ring(dev->mbox);
	req->ring = tim_ring->ring_id;

	rc = otx2_mbox_process(dev->mbox);
	if (rc < 0) {
		tim_err_desc(rc);
		rc = -EBUSY;
	}

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

	tim_unregister_irq(tim_ring->ring_id);

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

static int
otx2_tim_stats_get(const struct rte_event_timer_adapter *adapter,
		   struct rte_event_timer_adapter_stats *stats)
{
	struct otx2_tim_ring *tim_ring = adapter->data->adapter_priv;
	uint64_t bkt_cyc = rte_rdtsc() - tim_ring->ring_start_cyc;


	stats->evtim_exp_count = __atomic_load_n(&tim_ring->arm_cnt,
						 __ATOMIC_RELAXED);
	stats->ev_enq_count = stats->evtim_exp_count;
	stats->adapter_tick_count = rte_reciprocal_divide_u64(bkt_cyc,
				&tim_ring->fast_div);
	return 0;
}

static int
otx2_tim_stats_reset(const struct rte_event_timer_adapter *adapter)
{
	struct otx2_tim_ring *tim_ring = adapter->data->adapter_priv;

	__atomic_store_n(&tim_ring->arm_cnt, 0, __ATOMIC_RELAXED);
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
	otx2_tim_ops.start = otx2_tim_ring_start;
	otx2_tim_ops.stop = otx2_tim_ring_stop;
	otx2_tim_ops.get_info	= otx2_tim_ring_info_get;

	if (dev->enable_stats) {
		otx2_tim_ops.stats_get   = otx2_tim_stats_get;
		otx2_tim_ops.stats_reset = otx2_tim_stats_reset;
	}

	/* Store evdev pointer for later use. */
	dev->event_dev = (struct rte_eventdev *)(uintptr_t)evdev;
	*caps = RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT;
	*ops = &otx2_tim_ops;

	return 0;
}

#define OTX2_TIM_DISABLE_NPA	"tim_disable_npa"
#define OTX2_TIM_CHNK_SLOTS	"tim_chnk_slots"
#define OTX2_TIM_STATS_ENA	"tim_stats_ena"
#define OTX2_TIM_RINGS_LMT	"tim_rings_lmt"
#define OTX2_TIM_RING_CTL	"tim_ring_ctl"

static void
tim_parse_ring_param(char *value, void *opaque)
{
	struct otx2_tim_evdev *dev = opaque;
	struct otx2_tim_ctl ring_ctl = {0};
	char *tok = strtok(value, "-");
	struct otx2_tim_ctl *old_ptr;
	uint16_t *val;

	val = (uint16_t *)&ring_ctl;

	if (!strlen(value))
		return;

	while (tok != NULL) {
		*val = atoi(tok);
		tok = strtok(NULL, "-");
		val++;
	}

	if (val != (&ring_ctl.enable_stats + 1)) {
		otx2_err(
		"Invalid ring param expected [ring-chunk_sz-disable_npa-enable_stats]");
		return;
	}

	dev->ring_ctl_cnt++;
	old_ptr = dev->ring_ctl_data;
	dev->ring_ctl_data = rte_realloc(dev->ring_ctl_data,
					 sizeof(struct otx2_tim_ctl) *
					 dev->ring_ctl_cnt, 0);
	if (dev->ring_ctl_data == NULL) {
		dev->ring_ctl_data = old_ptr;
		dev->ring_ctl_cnt--;
		return;
	}

	dev->ring_ctl_data[dev->ring_ctl_cnt - 1] = ring_ctl;
}

static void
tim_parse_ring_ctl_list(const char *value, void *opaque)
{
	char *s = strdup(value);
	char *start = NULL;
	char *end = NULL;
	char *f = s;

	while (*s) {
		if (*s == '[')
			start = s;
		else if (*s == ']')
			end = s;

		if (start && start < end) {
			*end = 0;
			tim_parse_ring_param(start + 1, opaque);
			start = end;
			s = end;
		}
		s++;
	}

	free(f);
}

static int
tim_parse_kvargs_dict(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	/* Dict format [ring-chunk_sz-disable_npa-enable_stats] use '-' as ','
	 * isn't allowed. 0 represents default.
	 */
	tim_parse_ring_ctl_list(value, opaque);

	return 0;
}

static void
tim_parse_devargs(struct rte_devargs *devargs, struct otx2_tim_evdev *dev)
{
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		return;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	rte_kvargs_process(kvlist, OTX2_TIM_DISABLE_NPA,
			   &parse_kvargs_flag, &dev->disable_npa);
	rte_kvargs_process(kvlist, OTX2_TIM_CHNK_SLOTS,
			   &parse_kvargs_value, &dev->chunk_slots);
	rte_kvargs_process(kvlist, OTX2_TIM_STATS_ENA, &parse_kvargs_flag,
			   &dev->enable_stats);
	rte_kvargs_process(kvlist, OTX2_TIM_RINGS_LMT, &parse_kvargs_value,
			   &dev->min_ring_cnt);
	rte_kvargs_process(kvlist, OTX2_TIM_RING_CTL,
			   &tim_parse_kvargs_dict, &dev);

	rte_kvargs_free(kvlist);
}

void
otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev)
{
	struct rsrc_attach_req *atch_req;
	struct rsrc_detach_req *dtch_req;
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

	tim_parse_devargs(pci_dev->device.devargs, dev);

	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count.");
		goto mz_free;
	}

	dev->nb_rings = dev->min_ring_cnt ?
		RTE_MIN(dev->min_ring_cnt, rsrc_cnt->tim) : rsrc_cnt->tim;

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

	rc = tim_get_msix_offsets();
	if (rc < 0) {
		otx2_err("Unable to get MSIX offsets for TIM.");
		goto detach;
	}

	if (dev->chunk_slots &&
	    dev->chunk_slots <= OTX2_TIM_MAX_CHUNK_SLOTS &&
	    dev->chunk_slots >= OTX2_TIM_MIN_CHUNK_SLOTS) {
		dev->chunk_sz = (dev->chunk_slots + 1) *
			OTX2_TIM_CHUNK_ALIGNMENT;
	} else {
		dev->chunk_sz = OTX2_TIM_RING_DEF_CHUNK_SZ;
	}

	return;

detach:
	dtch_req = otx2_mbox_alloc_msg_detach_resources(dev->mbox);
	dtch_req->partial = true;
	dtch_req->timlfs = true;

	otx2_mbox_process(dev->mbox);
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
