/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"

#include "cn20k_cryptodev_ops.h"
#include "cn20k_ethdev.h"
#include "cn20k_eventdev.h"
#include "cn20k_tx_worker.h"
#include "cn20k_worker.h"
#include "cnxk_common.h"
#include "cnxk_eventdev.h"
#include "cnxk_vector_adptr.h"
#include "cnxk_worker.h"

#define CN20K_SET_EVDEV_DEQ_OP(dev, deq_op, deq_ops)                                               \
	deq_op = deq_ops[dev->rx_offloads & (NIX_RX_OFFLOAD_MAX - 1)]

#define CN20K_SET_EVDEV_ENQ_OP(dev, enq_op, enq_ops)                                               \
	enq_op = enq_ops[dev->tx_offloads & (NIX_TX_OFFLOAD_MAX - 1)]

static uint8_t
cn20k_sso_hw_weight(uint8_t weight)
{
	/* Map DPDK weight 0-255 to HW weight 1-255 */
	return (weight + 1) > CN20K_SSO_WEIGHT_MAX ? CN20K_SSO_WEIGHT_MAX : (weight + 1);
}

static int
cn20k_sso_queue_setup(struct rte_eventdev *event_dev, uint8_t queue_id,
		      const struct rte_event_queue_conf *queue_conf)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t priority, weight, affinity;

	priority = CNXK_QOS_NORMALIZE(queue_conf->priority, 0, RTE_EVENT_DEV_PRIORITY_LOWEST,
				      CNXK_SSO_PRIORITY_CNT);
	weight = cn20k_sso_hw_weight(queue_conf->weight);
	affinity = CNXK_QOS_NORMALIZE(queue_conf->affinity, 0, RTE_EVENT_QUEUE_AFFINITY_HIGHEST,
				      CNXK_SSO_AFFINITY_CNT);

	plt_sso_dbg("Queue=%u prio=%u weight=%u affinity=%u", queue_id, priority, weight, affinity);

	return roc_sso_hwgrp_set_priority(&dev->sso, queue_id, weight, affinity, priority);
}

static int
cn20k_sso_queue_attribute_set(struct rte_eventdev *event_dev, uint8_t queue_id, uint32_t attr_id,
			      uint64_t attr_value)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t priority, weight, affinity;
	struct rte_event_queue_conf *conf;

	conf = &event_dev->data->queues_cfg[queue_id];

	switch (attr_id) {
	case RTE_EVENT_QUEUE_ATTR_PRIORITY:
		conf->priority = attr_value;
		break;
	case RTE_EVENT_QUEUE_ATTR_WEIGHT:
		conf->weight = attr_value;
		break;
	case RTE_EVENT_QUEUE_ATTR_AFFINITY:
		conf->affinity = attr_value;
		break;
	case RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_FLOWS:
	case RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_ORDER_SEQUENCES:
	case RTE_EVENT_QUEUE_ATTR_EVENT_QUEUE_CFG:
	case RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE:
		/* FALLTHROUGH */
		plt_sso_dbg("Unsupported attribute id %u", attr_id);
		return -ENOTSUP;
	default:
		plt_err("Invalid attribute id %u", attr_id);
		return -EINVAL;
	}

	priority = CNXK_QOS_NORMALIZE(conf->priority, 0, RTE_EVENT_DEV_PRIORITY_LOWEST,
				      CNXK_SSO_PRIORITY_CNT);
	weight = cn20k_sso_hw_weight(conf->weight);
	affinity = CNXK_QOS_NORMALIZE(conf->affinity, 0, RTE_EVENT_QUEUE_AFFINITY_HIGHEST,
				      CNXK_SSO_AFFINITY_CNT);

	return roc_sso_hwgrp_set_priority(&dev->sso, queue_id, weight, affinity, priority);
}

static void *
cn20k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws;

	/* Allocate event port memory */
	ws = rte_zmalloc("cn20k_ws", sizeof(struct cn20k_sso_hws) + RTE_CACHE_LINE_SIZE,
			 RTE_CACHE_LINE_SIZE);
	if (ws == NULL) {
		plt_err("Failed to alloc memory for port=%d", port_id);
		return NULL;
	}

	/* First cache line is reserved for cookie */
	ws = (struct cn20k_sso_hws *)((uint8_t *)ws + RTE_CACHE_LINE_SIZE);
	ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
	ws->hws_id = port_id;
	ws->swtag_req = 0;
	ws->gw_wdata = cnxk_sso_hws_prf_wdata(dev);
	ws->gw_rdata = SSO_TT_EMPTY << 32;
	ws->xae_waes = dev->sso.feat.xaq_wq_entries;

	return ws;
}

static int
cn20k_sso_hws_link(void *arg, void *port, uint16_t *map, uint16_t nb_link, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = port;

	return roc_sso_hws_link(&dev->sso, ws->hws_id, map, nb_link, profile, 0);
}

static int
cn20k_sso_hws_unlink(void *arg, void *port, uint16_t *map, uint16_t nb_link, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = port;

	return roc_sso_hws_unlink(&dev->sso, ws->hws_id, map, nb_link, profile, 0);
}

static void
cn20k_sso_hws_setup(void *arg, void *hws, uintptr_t grp_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = hws;
	uint64_t val;

	ws->grp_base = grp_base;
	ws->fc_mem = (int64_t __rte_atomic *)dev->fc_iova;
	ws->xaq_lmt = dev->xaq_lmt;
	ws->fc_cache_space = (int64_t __rte_atomic *)dev->fc_cache_space;
	ws->aw_lmt = dev->sso.lmt_base;
	ws->gw_wdata = cnxk_sso_hws_prf_wdata(dev);
	ws->lmt_base = dev->sso.lmt_base;

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns);
	val = val ? val - 1 : 0;
	plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
}

static void
cn20k_sso_hws_release(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = hws;
	uint16_t i, j;

	for (i = 0; i < CNXK_SSO_MAX_PROFILES; i++)
		for (j = 0; j < dev->nb_event_queues; j++)
			roc_sso_hws_unlink(&dev->sso, ws->hws_id, &j, 1, i, 0);
	memset(ws, 0, sizeof(*ws));
}

static int
cn20k_sso_hws_flush_events(void *hws, uint8_t queue_id, uintptr_t base, cnxk_handle_event_t fn,
			   void *arg)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(arg);
	uint64_t retry = CNXK_SSO_FLUSH_RETRY_MAX;
	struct cn20k_sso_hws *ws = hws;
	uint64_t cq_ds_cnt = 1;
	uint64_t aq_cnt = 1;
	uint64_t ds_cnt = 1;
	struct rte_event ev;
	uint64_t val, req;

	plt_write64(0, base + SSO_LF_GGRP_QCTL);

	roc_sso_hws_gwc_invalidate(&dev->sso, &ws->hws_id, 1);
	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
	req = queue_id;	    /* GGRP ID */
	req |= BIT_ULL(18); /* Grouped */
	req |= BIT_ULL(16); /* WAIT */

	aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
	ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
	cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
	cq_ds_cnt &= 0x3FFF3FFF0000;

	while (aq_cnt || cq_ds_cnt || ds_cnt) {
		plt_write64(req, ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		cn20k_sso_hws_get_work_empty(ws, &ev, 0);
		if (fn != NULL && ev.u64 != 0)
			fn(arg, ev);
		if (ev.sched_type != SSO_TT_EMPTY)
			cnxk_sso_hws_swtag_flush(ws->base);
		else if (retry-- == 0)
			break;
		do {
			val = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
		} while (val & BIT_ULL(56));
		aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
		ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
		cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
		/* Extract cq and ds count */
		cq_ds_cnt &= 0x3FFF3FFF0000;
	}

	if (aq_cnt || cq_ds_cnt || ds_cnt)
		return -EAGAIN;

	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
	roc_sso_hws_gwc_invalidate(&dev->sso, &ws->hws_id, 1);
	rte_mb();

	return 0;
}

static void
cn20k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues = dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
					RTE_EVENT_MAX_QUEUES_PER_DEV :
					dev->sso.max_hwgrp;
}

static int
cn20k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_tim_evdev *tim_dev = cnxk_tim_priv_get();
	struct cnxk_sso_evdev *dev = arg;
	uint16_t nb_tim_lfs;

	nb_tim_lfs = tim_dev ? tim_dev->nb_rings : 0;
	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp, nb_tim_lfs);
}

static int
cn20k_sso_updt_tx_adptr_data(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	if (dev->tx_adptr_data == NULL)
		return 0;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cn20k_sso_hws *ws = event_dev->data->ports[i];
		void *ws_cookie;

		ws_cookie = cnxk_sso_hws_get_cookie(ws);
		ws_cookie = rte_realloc_socket(ws_cookie,
					       sizeof(struct cnxk_sso_hws_cookie) +
						       sizeof(struct cn20k_sso_hws) +
						       dev->tx_adptr_data_sz,
					       RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (ws_cookie == NULL)
			return -ENOMEM;
		ws = RTE_PTR_ADD(ws_cookie, sizeof(struct cnxk_sso_hws_cookie));
		memcpy(&ws->tx_adptr_data, dev->tx_adptr_data, dev->tx_adptr_data_sz);
		event_dev->data->ports[i] = ws;
	}

	return 0;
}

#if defined(RTE_ARCH_ARM64)
static inline void
cn20k_sso_fp_tmplt_fns_set(struct rte_eventdev *event_dev)
{
#if !defined(CNXK_DIS_TMPLT_FUNC)
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	const event_dequeue_burst_t sso_hws_deq_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_deq_tmo_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_deq_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_deq_tmo_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_reas_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_reas_deq_tmo_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_reas_deq_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags) [flags] = cn20k_sso_hws_reas_deq_tmo_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Tx modes */
	const event_tx_adapter_enqueue_t sso_hws_tx_adptr_enq[NIX_TX_OFFLOAD_MAX] = {
#define T(name, sz, flags) [flags] = cn20k_sso_hws_tx_adptr_enq_##name,
		NIX_TX_FASTPATH_MODES
#undef T
	};

	const event_tx_adapter_enqueue_t sso_hws_tx_adptr_enq_seg[NIX_TX_OFFLOAD_MAX] = {
#define T(name, sz, flags) [flags] = cn20k_sso_hws_tx_adptr_enq_seg_##name,
		NIX_TX_FASTPATH_MODES
#undef T
	};

	if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
		if (dev->rx_offloads & NIX_RX_REAS_F) {
			CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_reas_deq_seg_burst);
			if (dev->is_timeout_deq)
				CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_seg_burst);
		} else {
			CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_deq_seg_burst);

			if (dev->is_timeout_deq)
				CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_seg_burst);
		}
	} else {
		if (dev->rx_offloads & NIX_RX_REAS_F) {
			CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_reas_deq_burst);

			if (dev->is_timeout_deq)
				CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_burst);
		} else {
			CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst, sso_hws_deq_burst);

			if (dev->is_timeout_deq)
				CN20K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_burst);
		}
	}

	if (dev->tx_offloads & NIX_TX_MULTI_SEG_F)
		CN20K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue, sso_hws_tx_adptr_enq_seg);
	else
		CN20K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue, sso_hws_tx_adptr_enq);

	event_dev->txa_enqueue_same_dest = event_dev->txa_enqueue;
#else
	RTE_SET_USED(event_dev);
#endif
}

static inline void
cn20k_sso_fp_blk_fns_set(struct rte_eventdev *event_dev)
{
#if defined(CNXK_DIS_TMPLT_FUNC)
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	event_dev->dequeue_burst = cn20k_sso_hws_deq_burst_all_offload;
	if (dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)
		event_dev->dequeue_burst = cn20k_sso_hws_deq_burst_all_offload_tst;
	event_dev->txa_enqueue = cn20k_sso_hws_tx_adptr_enq_seg_all_offload;
	event_dev->txa_enqueue_same_dest = cn20k_sso_hws_tx_adptr_enq_seg_all_offload;
	if (dev->tx_offloads & (NIX_TX_OFFLOAD_OL3_OL4_CSUM_F | NIX_TX_OFFLOAD_VLAN_QINQ_F |
				NIX_TX_OFFLOAD_TSO_F | NIX_TX_OFFLOAD_TSTAMP_F)) {
		event_dev->txa_enqueue = cn20k_sso_hws_tx_adptr_enq_seg_all_offload_tst;
		event_dev->txa_enqueue_same_dest = cn20k_sso_hws_tx_adptr_enq_seg_all_offload_tst;
	}
#else
	RTE_SET_USED(event_dev);
#endif
}
#endif

static void
cn20k_sso_fp_fns_set(struct rte_eventdev *event_dev)
{
#if defined(RTE_ARCH_ARM64)
	cn20k_sso_fp_blk_fns_set(event_dev);
	cn20k_sso_fp_tmplt_fns_set(event_dev);

	event_dev->enqueue_burst = cn20k_sso_hws_enq_burst;
	event_dev->enqueue_new_burst = cn20k_sso_hws_enq_new_burst;
	event_dev->enqueue_forward_burst = cn20k_sso_hws_enq_fwd_burst;

	event_dev->ca_enqueue = cn20k_cpt_crypto_adapter_enqueue;
	event_dev->profile_switch = cn20k_sso_hws_profile_switch;
	event_dev->preschedule_modify = cn20k_sso_hws_preschedule_modify;
	event_dev->preschedule = cn20k_sso_hws_preschedule;
#else
	RTE_SET_USED(event_dev);
#endif
}

static void
cn20k_sso_info_get(struct rte_eventdev *event_dev, struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN20K_PMD);
	cnxk_sso_info_get(dev, dev_info);
	dev_info->max_event_port_enqueue_depth = UINT32_MAX;
}

static int
cn20k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev, 1, UINT32_MAX);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	rc = cn20k_sso_rsrc_init(dev, dev->nb_event_ports, dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	dev->gw_mode = cnxk_sso_hws_preschedule_get(event_dev->data->dev_conf.preschedule_type);

	rc = cnxk_setup_event_ports(event_dev, cn20k_sso_init_hws_mem, cn20k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	/* Restore any prior port-queue mapping. */
	cnxk_sso_restore_links(event_dev, cn20k_sso_hws_link);

	dev->configured = 1;
	rte_mb();

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn20k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn20k_sso_hws_setup);
}

static void
cn20k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn20k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static void
cn20k_sso_port_quiesce(struct rte_eventdev *event_dev, void *port,
		       rte_eventdev_port_flush_t flush_cb, void *args)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cn20k_sso_hws *ws = port;
	struct rte_event ev;
	uint64_t ptag;
	bool is_pend;

	is_pend = false;
	/* Work in WQE0 is always consumed, unless its a SWTAG. */
	ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	if (ptag & (BIT_ULL(62) | BIT_ULL(54)) || ws->swtag_req)
		is_pend = true;
	do {
		ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	} while (ptag & (BIT_ULL(62) | BIT_ULL(58) | BIT_ULL(56) | BIT_ULL(54)));

	cn20k_sso_hws_get_work_empty(ws, &ev,
				     (NIX_RX_OFFLOAD_MAX - 1) | NIX_RX_REAS_F | NIX_RX_MULTI_SEG_F);
	if (is_pend && ev.u64)
		if (flush_cb)
			flush_cb(event_dev->data->dev_id, ev, args);
	ptag = (plt_read64(ws->base + SSOW_LF_GWS_TAG) >> 32) & SSO_TT_EMPTY;
	if (ptag != SSO_TT_EMPTY)
		cnxk_sso_hws_swtag_flush(ws->base);

	do {
		ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	} while (ptag & BIT_ULL(56));

	/* Check if we have work in PRF_WQE0, if so extract it. */
	switch (dev->gw_mode) {
	case CNXK_GW_MODE_PREF:
	case CNXK_GW_MODE_PREF_WFE:
		while (plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0) & BIT_ULL(63))
			;
		break;
	case CNXK_GW_MODE_NONE:
	default:
		break;
	}

	if (CNXK_TT_FROM_TAG(plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0)) != SSO_TT_EMPTY) {
		plt_write64(BIT_ULL(16) | 1, ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		cn20k_sso_hws_get_work_empty(
			ws, &ev, (NIX_RX_OFFLOAD_MAX - 1) | NIX_RX_REAS_F | NIX_RX_MULTI_SEG_F);
		if (ev.u64) {
			if (flush_cb)
				flush_cb(event_dev->data->dev_id, ev, args);
		}
		cnxk_sso_hws_swtag_flush(ws->base);
		do {
			ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
		} while (ptag & BIT_ULL(56));
	}
	ws->swtag_req = 0;
	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
}

static int
cn20k_sso_port_link_profile(struct rte_eventdev *event_dev, void *port, const uint8_t queues[],
			    const uint8_t priorities[], uint16_t nb_links, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_links];
	uint16_t link;

	RTE_SET_USED(priorities);
	for (link = 0; link < nb_links; link++)
		hwgrp_ids[link] = queues[link];
	nb_links = cn20k_sso_hws_link(dev, port, hwgrp_ids, nb_links, profile);

	return (int)nb_links;
}

static int
cn20k_sso_port_unlink_profile(struct rte_eventdev *event_dev, void *port, uint8_t queues[],
			      uint16_t nb_unlinks, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_unlinks];
	uint16_t unlink;

	for (unlink = 0; unlink < nb_unlinks; unlink++)
		hwgrp_ids[unlink] = queues[unlink];
	nb_unlinks = cn20k_sso_hws_unlink(dev, port, hwgrp_ids, nb_unlinks, profile);

	return (int)nb_unlinks;
}

static int
cn20k_sso_port_link(struct rte_eventdev *event_dev, void *port, const uint8_t queues[],
		    const uint8_t priorities[], uint16_t nb_links)
{
	return cn20k_sso_port_link_profile(event_dev, port, queues, priorities, nb_links, 0);
}

static int
cn20k_sso_port_unlink(struct rte_eventdev *event_dev, void *port, uint8_t queues[],
		      uint16_t nb_unlinks)
{
	return cn20k_sso_port_unlink_profile(event_dev, port, queues, nb_unlinks, 0);
}

static int
cn20k_sso_start(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t hws[RTE_EVENT_MAX_PORTS_PER_DEV];
	int rc, i;

	cnxk_sso_configure_queue_stash(event_dev);
	rc = cnxk_sso_start(event_dev, cnxk_sso_hws_reset, cn20k_sso_hws_flush_events);
	if (rc < 0)
		return rc;
	cn20k_sso_fp_fns_set(event_dev);
	for (i = 0; i < event_dev->data->nb_ports; i++)
		hws[i] = i;
	roc_sso_hws_gwc_invalidate(&dev->sso, hws, event_dev->data->nb_ports);

	return rc;
}

static void
cn20k_sso_stop(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t hws[RTE_EVENT_MAX_PORTS_PER_DEV];
	int i;

	for (i = 0; i < event_dev->data->nb_ports; i++)
		hws[i] = i;
	roc_sso_hws_gwc_invalidate(&dev->sso, hws, event_dev->data->nb_ports);
	cnxk_sso_stop(event_dev, cnxk_sso_hws_reset, cn20k_sso_hws_flush_events);
}

static int
cn20k_sso_close(struct rte_eventdev *event_dev)
{
	return cnxk_sso_close(event_dev, cn20k_sso_hws_unlink);
}

static int
cn20k_sso_selftest(void)
{
	return cnxk_sso_selftest(RTE_STR(event_cn20k));
}

static int
cn20k_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 9);
	if (rc)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR;

	return 0;
}

static void
cn20k_sso_set_priv_mem(const struct rte_eventdev *event_dev, void *lookup_mem)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cn20k_sso_hws *ws = event_dev->data->ports[i];
		ws->xaq_lmt = dev->xaq_lmt;
		ws->fc_mem = (int64_t __rte_atomic *)dev->fc_iova;
		ws->tstamp = dev->tstamp;
		if (lookup_mem)
			ws->lookup_mem = lookup_mem;
	}
}

static void
eventdev_fops_tstamp_update(struct rte_eventdev *event_dev)
{
	struct rte_event_fp_ops *fp_op = rte_event_fp_ops + event_dev->data->dev_id;

	fp_op->dequeue_burst = event_dev->dequeue_burst;
}

static void
cn20k_sso_tstamp_hdl_update(uint16_t port_id, uint16_t flags, bool ptp_en)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct cnxk_eth_dev *cnxk_eth_dev = dev->data->dev_private;
	struct rte_eventdev *event_dev = cnxk_eth_dev->evdev_priv;
	struct cnxk_sso_evdev *evdev = cnxk_sso_pmd_priv(event_dev);

	evdev->rx_offloads |= flags;
	if (ptp_en)
		evdev->tstamp[port_id] = &cnxk_eth_dev->tstamp;
	else
		evdev->tstamp[port_id] = NULL;
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	eventdev_fops_tstamp_update(event_dev);
}

static int
cn20k_sso_rxq_enable(struct cnxk_eth_dev *cnxk_eth_dev, uint16_t rq_id, uint16_t port_id,
		     const struct rte_event_eth_rx_adapter_queue_conf *queue_conf, int agq)
{
	struct roc_nix_rq *rq;
	uint32_t tag_mask;
	uint16_t wqe_skip;
	uint8_t tt;
	int rc;

	rq = &cnxk_eth_dev->rqs[rq_id];
	if (queue_conf->rx_queue_flags & RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR) {
		tag_mask = agq;
		tt = SSO_TT_AGG;
		rq->flow_tag_width = 0;
	} else {
		tag_mask = (port_id & 0xFF) << 20;
		tag_mask |= (RTE_EVENT_TYPE_ETHDEV << 28);
		tt = queue_conf->ev.sched_type;
		rq->flow_tag_width = 20;
		if (queue_conf->rx_queue_flags & RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID) {
			rq->flow_tag_width = 0;
			tag_mask |= queue_conf->ev.flow_id;
		}
	}

	rq->tag_mask = tag_mask;
	rq->sso_ena = 1;
	rq->tt = tt;
	rq->hwgrp = queue_conf->ev.queue_id;
	wqe_skip = RTE_ALIGN_CEIL(sizeof(struct rte_mbuf), ROC_CACHE_LINE_SZ);
	wqe_skip = wqe_skip / ROC_CACHE_LINE_SZ;
	rq->wqe_skip = wqe_skip;

	rc = roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
	return rc;
}

static int
cn20k_sso_rx_adapter_vwqe_enable(struct cnxk_sso_evdev *dev, uint16_t port_id, uint16_t rq_id,
				 const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	uint32_t agq, tag_mask, stag_mask;
	struct roc_sso_agq_data data;
	int rc;

	tag_mask = (port_id & 0xff) << 20;
	if (queue_conf->rx_queue_flags & RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID)
		tag_mask |= queue_conf->ev.flow_id;
	else
		tag_mask |= rq_id;

	stag_mask = tag_mask;
	tag_mask |= RTE_EVENT_TYPE_ETHDEV_VECTOR << 28;
	stag_mask |= RTE_EVENT_TYPE_ETHDEV << 28;

	memset(&data, 0, sizeof(struct roc_sso_agq_data));
	data.tag = tag_mask;
	data.tt = queue_conf->ev.sched_type;
	data.stag = stag_mask;
	data.vwqe_aura = roc_npa_aura_handle_to_aura(queue_conf->vector_mp->pool_id);
	data.vwqe_max_sz_exp = rte_log2_u32(queue_conf->vector_sz);
	data.vwqe_wait_tmo = queue_conf->vector_timeout_ns / ((SSO_AGGR_DEF_TMO + 1) * 100);
	data.xqe_type = 0;

	agq = UINT32_MAX;
	rc = roc_sso_hwgrp_agq_alloc(&dev->sso, queue_conf->ev.queue_id, &data, &agq);
	if (rc < 0)
		return rc;

	return agq;
}

static int
cn20k_rx_adapter_queue_del(const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
			   int32_t rx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_nix_rq *rxq;
	int i, rc = 0;

	RTE_SET_USED(event_dev);
	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			cn20k_rx_adapter_queue_del(event_dev, eth_dev, i);
	} else {
		rxq = &cnxk_eth_dev->rqs[rx_queue_id];
		if (rxq->tt == SSO_TT_AGG)
			roc_sso_hwgrp_agq_free(&dev->sso, rxq->hwgrp, rxq->tag_mask);
		rc = cnxk_sso_rxq_disable(eth_dev, (uint16_t)rx_queue_id);
		cnxk_eth_dev->nb_rxq_sso--;
	}

	if (rc < 0)
		plt_err("Failed to clear Rx adapter config port=%d, q=%d", eth_dev->data->port_id,
			rx_queue_id);
	return rc;
}

static int
cn20k_rx_adapter_queues_add(const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
			    int32_t rx_queue_id[],
			    const struct rte_event_eth_rx_adapter_queue_conf queue_conf[],
			    uint16_t nb_rx_queues)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	const struct rte_event_eth_rx_adapter_queue_conf *conf;
	uint64_t old_xae_cnt = dev->adptr_xae_cnt;
	uint16_t port = eth_dev->data->port_id;
	struct cnxk_eth_rxq_sp *rxq_sp;
	uint16_t max_rx_queues;
	int i, rc = 0, agq = 0;
	int32_t queue_id;

	max_rx_queues = nb_rx_queues ? nb_rx_queues : eth_dev->data->nb_rx_queues;
	for (i = 0; i < max_rx_queues; i++) {
		conf = nb_rx_queues ? &queue_conf[i] : &queue_conf[0];
		queue_id = nb_rx_queues ? rx_queue_id[i] : i;
		rxq_sp = cnxk_eth_rxq_to_sp(eth_dev->data->rx_queues[queue_id]);
		cnxk_sso_updt_xae_cnt(dev, rxq_sp, RTE_EVENT_TYPE_ETHDEV);

		if (conf->rx_queue_flags & RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR)
			cnxk_sso_updt_xae_cnt(dev, conf->vector_mp, RTE_EVENT_TYPE_ETHDEV_VECTOR);
	}

	if (dev->adptr_xae_cnt != old_xae_cnt) {
		rc = cnxk_sso_xae_reconfigure((struct rte_eventdev *)(uintptr_t)event_dev);
		if (rc < 0)
			return rc;
	}

	for (i = 0; i < max_rx_queues; i++) {
		conf = nb_rx_queues ? &queue_conf[i] : &queue_conf[0];
		queue_id = nb_rx_queues ? rx_queue_id[i] : i;
		if (conf->rx_queue_flags & RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR) {
			rc = cn20k_sso_rx_adapter_vwqe_enable(dev, port, queue_id, conf);
			if (rc < 0) {
				plt_err("Failed to enable VWQE, port=%d, rxq=%d", port, queue_id);
				goto fail;
			}

			agq = rc;
		}

		rc = cn20k_sso_rxq_enable(cnxk_eth_dev, (uint16_t)queue_id, port, conf, agq);
		if (rc < 0) {
			plt_err("Failed to enable Rx queue, port=%d, rxq=%d", port, queue_id);
			goto fail;
		}

		cnxk_eth_dev->nb_rxq_sso++;
	}

	/* Propagate force bp devarg */
	cnxk_eth_dev->nix.force_rx_aura_bp = dev->force_ena_bp;
	cnxk_sso_tstamp_cfg(port, eth_dev, dev);
	dev->rx_offloads |= cnxk_eth_dev->rx_offload_flags;
	return 0;

fail:
	for (i = cnxk_eth_dev->nb_rxq_sso - 1; i >= 0; i--) {
		queue_id = nb_rx_queues ? rx_queue_id[i] : i;
		cn20k_rx_adapter_queue_del(event_dev, eth_dev, queue_id);
	}

	return rc;
}

static int
cn20k_sso_configure_queue_stash_default(struct cnxk_sso_evdev *dev, uint16_t hwgrp)
{
	struct roc_sso_hwgrp_stash stash;
	int rc;

	stash.hwgrp = hwgrp;
	stash.stash_offset = CN20K_SSO_DEFAULT_STASH_OFFSET;
	stash.stash_count = CN20K_SSO_DEFAULT_STASH_LENGTH;
	rc = roc_sso_hwgrp_stash_config(&dev->sso, &stash, 1);
	if (rc < 0)
		plt_warn("failed to configure HWGRP WQE stashing rc = %d", rc);

	return rc;
}

static int
cn20k_sso_rx_adapter_queue_add(const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t rx_queue_id,
			       const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cn20k_eth_rxq *rxq;
	uint16_t nb_rx_queues;
	void *lookup_mem;
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (rc)
		return -EINVAL;

	nb_rx_queues = rx_queue_id == -1 ? 0 : 1;
	rc = cn20k_rx_adapter_queues_add(event_dev, eth_dev, &rx_queue_id, queue_conf,
					 nb_rx_queues);
	if (rc)
		return -EINVAL;

	cnxk_eth_dev->cnxk_sso_ptp_tstamp_cb = cn20k_sso_tstamp_hdl_update;
	cnxk_eth_dev->evdev_priv = (struct rte_eventdev *)(uintptr_t)event_dev;

	rxq = eth_dev->data->rx_queues[0];
	lookup_mem = rxq->lookup_mem;
	cn20k_sso_set_priv_mem(event_dev, lookup_mem);
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	if (roc_feature_sso_has_stash() && dev->nb_event_ports > 1)
		rc = cn20k_sso_configure_queue_stash_default(dev, queue_conf->ev.queue_id);

	return rc;
}

static int
cn20k_sso_rx_adapter_queues_add(const struct rte_eventdev *event_dev,
				const struct rte_eth_dev *eth_dev, int32_t rx_queue_id[],
				const struct rte_event_eth_rx_adapter_queue_conf queue_conf[],
				uint16_t nb_rx_queues)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cn20k_eth_rxq *rxq;
	void *lookup_mem;
	int rc, i;

	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (rc)
		return -EINVAL;

	rc = cn20k_rx_adapter_queues_add(event_dev, eth_dev, rx_queue_id, queue_conf, nb_rx_queues);
	if (rc)
		return -EINVAL;

	cnxk_eth_dev->cnxk_sso_ptp_tstamp_cb = cn20k_sso_tstamp_hdl_update;
	cnxk_eth_dev->evdev_priv = (struct rte_eventdev *)(uintptr_t)event_dev;

	rxq = eth_dev->data->rx_queues[0];
	lookup_mem = rxq->lookup_mem;
	cn20k_sso_set_priv_mem(event_dev, lookup_mem);
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	if (roc_feature_sso_has_stash() && dev->nb_event_ports > 1) {
		uint16_t hwgrp = dev->sso.max_hwgrp;

		if (nb_rx_queues == 0)
			rc = cn20k_sso_configure_queue_stash_default(dev,
								     queue_conf[0].ev.queue_id);

		for (i = 0; i < nb_rx_queues; i++) {
			if (hwgrp == queue_conf[i].ev.queue_id)
				continue;

			hwgrp = queue_conf[i].ev.queue_id;
			rc = cn20k_sso_configure_queue_stash_default(dev, hwgrp);
			if (rc < 0)
				break;
		}
	}

	return rc;
}

static int
cn20k_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t rx_queue_id)
{
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (rc)
		return -EINVAL;

	return cn20k_rx_adapter_queue_del(event_dev, eth_dev, rx_queue_id);
}

static int
cn20k_sso_rx_adapter_vector_limits(const struct rte_eventdev *dev,
				   const struct rte_eth_dev *eth_dev,
				   struct rte_event_eth_rx_adapter_vector_limits *limits)
{
	int ret;

	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);
	ret = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (ret)
		return -ENOTSUP;

	limits->log2_sz = true;
	limits->min_sz = 1 << ROC_NIX_VWQE_MIN_SIZE_LOG2;
	limits->max_sz = 1 << ROC_NIX_VWQE_MAX_SIZE_LOG2;
	limits->min_timeout_ns = (SSO_AGGR_DEF_TMO + 1) * 100;
	limits->max_timeout_ns = (BITMASK_ULL(11, 0) + 1) * limits->min_timeout_ns;

	return 0;
}

static int
cn20k_sso_tx_adapter_caps_get(const struct rte_eventdev *dev, const struct rte_eth_dev *eth_dev,
			      uint32_t *caps)
{
	int ret;

	RTE_SET_USED(dev);
	ret = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (ret)
		*caps = 0;
	else
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR;

	return 0;
}

static void
cn20k_sso_txq_fc_update(const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cn20k_eth_txq *txq;
	struct roc_nix_sq *sq;
	int i;

	if (tx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
			cn20k_sso_txq_fc_update(eth_dev, i);
	} else {
		uint16_t sqes_per_sqb;

		sq = &cnxk_eth_dev->sqs[tx_queue_id];
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sqes_per_sqb = 1U << txq->sqes_per_sqb_log2;
		if (cnxk_eth_dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY)
			sq->nb_sqb_bufs_adj -= (cnxk_eth_dev->outb.nb_desc / sqes_per_sqb);
		txq->nb_sqb_bufs_adj = sq->nb_sqb_bufs_adj;
	}
}

static int
cn20k_sso_tx_adapter_queue_add(uint8_t id, const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint64_t tx_offloads;
	int rc;

	RTE_SET_USED(id);
	rc = cnxk_sso_tx_adapter_queue_add(event_dev, eth_dev, tx_queue_id);
	if (rc < 0)
		return rc;

	/* Can't enable tstamp if all the ports don't have it enabled. */
	tx_offloads = cnxk_eth_dev->tx_offload_flags;
	if (dev->tx_adptr_configured) {
		uint8_t tstmp_req = !!(tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F);
		uint8_t tstmp_ena = !!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F);

		if (tstmp_ena && !tstmp_req)
			dev->tx_offloads &= ~(NIX_TX_OFFLOAD_TSTAMP_F);
		else if (!tstmp_ena && tstmp_req)
			tx_offloads &= ~(NIX_TX_OFFLOAD_TSTAMP_F);
	}

	dev->tx_offloads |= tx_offloads;
	cn20k_sso_txq_fc_update(eth_dev, tx_queue_id);
	rc = cn20k_sso_updt_tx_adptr_data(event_dev);
	if (rc < 0)
		return rc;
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	dev->tx_adptr_configured = 1;

	return 0;
}

static int
cn20k_sso_tx_adapter_queue_del(uint8_t id, const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	int rc;

	RTE_SET_USED(id);
	rc = cnxk_sso_tx_adapter_queue_del(event_dev, eth_dev, tx_queue_id);
	if (rc < 0)
		return rc;
	return cn20k_sso_updt_tx_adptr_data(event_dev);
}

static int
cn20k_crypto_adapter_caps_get(const struct rte_eventdev *event_dev,
			      const struct rte_cryptodev *cdev, uint32_t *caps)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn20k", ENOTSUP);
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn20k", ENOTSUP);

	*caps = RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_EVENT_VECTOR;

	return 0;
}

static int
cn20k_crypto_adapter_qp_add(const struct rte_eventdev *event_dev, const struct rte_cryptodev *cdev,
			    int32_t queue_pair_id,
			    const struct rte_event_crypto_adapter_queue_conf *conf)
{
	int ret;

	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn20k", EINVAL);
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn20k", EINVAL);

	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	ret = cnxk_crypto_adapter_qp_add(event_dev, cdev, queue_pair_id, conf);
	cn20k_sso_set_priv_mem(event_dev, NULL);

	return ret;
}

static int
cn20k_crypto_adapter_qp_del(const struct rte_eventdev *event_dev, const struct rte_cryptodev *cdev,
			    int32_t queue_pair_id)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn20k", EINVAL);
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn20k", EINVAL);

	return cnxk_crypto_adapter_qp_del(cdev, queue_pair_id);
}

static int
cn20k_tim_caps_get(const struct rte_eventdev *evdev, uint64_t flags, uint32_t *caps,
		   const struct event_timer_adapter_ops **ops)
{
	return cnxk_tim_caps_get(evdev, flags, caps, ops, cn20k_sso_set_priv_mem);
}

static int
cn20k_crypto_adapter_vec_limits(const struct rte_eventdev *event_dev,
				const struct rte_cryptodev *cdev,
				struct rte_event_crypto_adapter_vector_limits *limits)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn20k", EINVAL);
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn20k", EINVAL);

	limits->log2_sz = false;
	limits->min_sz = 0;
	limits->max_sz = UINT16_MAX;
	/* Unused timeout, in software implementation we aggregate all crypto
	 * operations passed to the enqueue function
	 */
	limits->min_timeout_ns = 0;
	limits->max_timeout_ns = 0;

	return 0;
}

static struct eventdev_ops cn20k_sso_dev_ops = {
	.dev_infos_get = cn20k_sso_info_get,
	.dev_configure = cn20k_sso_dev_configure,

	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cn20k_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.queue_attr_set = cn20k_sso_queue_attribute_set,

	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn20k_sso_port_setup,
	.port_release = cn20k_sso_port_release,
	.port_quiesce = cn20k_sso_port_quiesce,
	.port_link = cn20k_sso_port_link,
	.port_unlink = cn20k_sso_port_unlink,
	.port_link_profile = cn20k_sso_port_link_profile,
	.port_unlink_profile = cn20k_sso_port_unlink_profile,
	.timeout_ticks = cnxk_sso_timeout_ticks,

	.eth_rx_adapter_caps_get = cn20k_sso_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = cn20k_sso_rx_adapter_queue_add,
	.eth_rx_adapter_queues_add = cn20k_sso_rx_adapter_queues_add,
	.eth_rx_adapter_queue_del = cn20k_sso_rx_adapter_queue_del,
	.eth_rx_adapter_start = cnxk_sso_rx_adapter_start,
	.eth_rx_adapter_stop = cnxk_sso_rx_adapter_stop,

	.eth_rx_adapter_vector_limits_get = cn20k_sso_rx_adapter_vector_limits,

	.eth_tx_adapter_caps_get = cn20k_sso_tx_adapter_caps_get,
	.eth_tx_adapter_queue_add = cn20k_sso_tx_adapter_queue_add,
	.eth_tx_adapter_queue_del = cn20k_sso_tx_adapter_queue_del,
	.eth_tx_adapter_start = cnxk_sso_tx_adapter_start,
	.eth_tx_adapter_stop = cnxk_sso_tx_adapter_stop,
	.eth_tx_adapter_free = cnxk_sso_tx_adapter_free,

	.timer_adapter_caps_get = cn20k_tim_caps_get,

	.crypto_adapter_caps_get = cn20k_crypto_adapter_caps_get,
	.crypto_adapter_queue_pair_add = cn20k_crypto_adapter_qp_add,
	.crypto_adapter_queue_pair_del = cn20k_crypto_adapter_qp_del,
	.crypto_adapter_vector_limits_get = cn20k_crypto_adapter_vec_limits,

	.vector_adapter_caps_get = cnxk_vector_caps_get,
	.vector_adapter_info_get = cnxk_vector_info_get,

	.xstats_get = cnxk_sso_xstats_get,
	.xstats_reset = cnxk_sso_xstats_reset,
	.xstats_get_names = cnxk_sso_xstats_get_names,

	.dump = cnxk_sso_dump,
	.dev_start = cn20k_sso_start,
	.dev_stop = cn20k_sso_stop,
	.dev_close = cn20k_sso_close,
	.dev_selftest = cn20k_sso_selftest,
};

static int
cn20k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn20k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		cn20k_sso_fp_fns_set(event_dev);
		return 0;
	}

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn20k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
	if (!dev->max_event_ports || !dev->max_event_queues) {
		plt_err("Not enough eventdev resource queues=%d ports=%d", dev->max_event_queues,
			dev->max_event_ports);
		cnxk_sso_fini(event_dev);
		return -ENODEV;
	}

	plt_sso_dbg("Initializing %s max_queues=%d max_ports=%d", event_dev->data->name,
		    dev->max_event_queues, dev->max_event_ports);

	return 0;
}

static int
cn20k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev, sizeof(struct cnxk_sso_evdev),
				       cn20k_sso_init);
}

static const struct rte_pci_id cn20k_pci_sso_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn20k_pci_sso = {
	.id_table = cn20k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn20k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn20k, cn20k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn20k, cn20k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn20k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn20k,
			      CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>"
			      CNXK_SSO_STASH "=<string>"
			      CNXK_SSO_FORCE_BP "=1"
			      CNXK_TIM_DISABLE_NPA "=1"
			      CNXK_TIM_CHNK_SLOTS "=<int>"
			      CNXK_TIM_RINGS_LMT "=<int>"
			      CNXK_TIM_STATS_ENA "=1"
			      CNXK_TIM_EXT_CLK "=<string>");
