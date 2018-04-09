/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include "timvf_evdev.h"

int otx_logtype_timvf;

RTE_INIT(otx_timvf_init_log);
static void
otx_timvf_init_log(void)
{
	otx_logtype_timvf = rte_log_register("pmd.event.octeontx.timer");
	if (otx_logtype_timvf >= 0)
		rte_log_set_level(otx_logtype_timvf, RTE_LOG_NOTICE);
}

static void
timvf_ring_info_get(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer_adapter_info *adptr_info)
{
	struct timvf_ring *timr = adptr->data->adapter_priv;
	adptr_info->max_tmo_ns = timr->max_tout;
	adptr_info->min_resolution_ns = timr->tck_nsec;
	rte_memcpy(&adptr_info->conf, &adptr->data->conf,
			sizeof(struct rte_event_timer_adapter_conf));
}

static int
timvf_ring_create(struct rte_event_timer_adapter *adptr)
{
	char pool_name[25];
	int ret;
	uint64_t nb_timers;
	struct rte_event_timer_adapter_conf *rcfg = &adptr->data->conf;
	struct timvf_ring *timr;
	struct timvf_info tinfo;
	const char *mempool_ops;

	if (timvf_info(&tinfo) < 0)
		return -ENODEV;

	if (adptr->data->id >= tinfo.total_timvfs)
		return -ENODEV;

	timr = rte_zmalloc("octeontx_timvf_priv",
			sizeof(struct timvf_ring), 0);
	if (timr == NULL)
		return -ENOMEM;

	adptr->data->adapter_priv = timr;
	/* Check config parameters. */
	if ((rcfg->clk_src != RTE_EVENT_TIMER_ADAPTER_CPU_CLK) &&
			(!rcfg->timer_tick_ns ||
			 rcfg->timer_tick_ns < TIM_MIN_INTERVAL)) {
		timvf_log_err("Too low timer ticks");
		goto cfg_err;
	}

	timr->clk_src = (int) rcfg->clk_src;
	timr->tim_ring_id = adptr->data->id;
	timr->tck_nsec = rcfg->timer_tick_ns;
	timr->max_tout = rcfg->max_tmo_ns;
	timr->nb_bkts = (timr->max_tout / timr->tck_nsec);
	timr->vbar0 = timvf_bar(timr->tim_ring_id, 0);
	timr->bkt_pos = (uint8_t *)timr->vbar0 + TIM_VRING_REL;
	nb_timers = rcfg->nb_timers;
	timr->get_target_bkt = bkt_mod;

	timr->nb_chunks = nb_timers / nb_chunk_slots;

	timr->bkt = rte_zmalloc("octeontx_timvf_bucket",
			(timr->nb_bkts) * sizeof(struct tim_mem_bucket),
			0);
	if (timr->bkt == NULL)
		goto mem_err;

	snprintf(pool_name, 30, "timvf_chunk_pool%d", timr->tim_ring_id);
	timr->chunk_pool = (void *)rte_mempool_create_empty(pool_name,
			timr->nb_chunks, TIM_CHUNK_SIZE, 0, 0, rte_socket_id(),
			0);

	if (!timr->chunk_pool) {
		rte_free(timr->bkt);
		timvf_log_err("Unable to create chunkpool.");
		return -ENOMEM;
	}

	mempool_ops = rte_mbuf_best_mempool_ops();
	ret = rte_mempool_set_ops_byname(timr->chunk_pool,
			mempool_ops, NULL);

	if (ret != 0) {
		timvf_log_err("Unable to set chunkpool ops.");
		goto mem_err;
	}

	ret = rte_mempool_populate_default(timr->chunk_pool);
	if (ret < 0) {
		timvf_log_err("Unable to set populate chunkpool.");
		goto mem_err;
	}
	timvf_write64(0, (uint8_t *)timr->vbar0 + TIM_VRING_BASE);
	timvf_write64(0, (uint8_t *)timr->vbar0 + TIM_VF_NRSPERR_INT);
	timvf_write64(0, (uint8_t *)timr->vbar0 + TIM_VF_NRSPERR_INT_W1S);
	timvf_write64(0x7, (uint8_t *)timr->vbar0 + TIM_VF_NRSPERR_ENA_W1C);
	timvf_write64(0x7, (uint8_t *)timr->vbar0 + TIM_VF_NRSPERR_ENA_W1S);

	return 0;
mem_err:
	rte_free(timr);
	return -ENOMEM;
cfg_err:
	rte_free(timr);
	return -EINVAL;
}

static int
timvf_ring_free(struct rte_event_timer_adapter *adptr)
{
	struct timvf_ring *timr = adptr->data->adapter_priv;
	rte_mempool_free(timr->chunk_pool);
	rte_free(timr->bkt);
	rte_free(adptr->data->adapter_priv);
	return 0;
}

static struct rte_event_timer_adapter_ops timvf_ops = {
	.init		= timvf_ring_create,
	.uninit		= timvf_ring_free,
	.get_info	= timvf_ring_info_get,
};

int
timvf_timer_adapter_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		uint32_t *caps, const struct rte_event_timer_adapter_ops **ops,
		uint8_t enable_stats)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(flags);
	RTE_SET_USED(enable_stats);
	*caps = RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT;
	*ops = &timvf_ops;
	return -EINVAL;
}
