/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_tx_worker.h"
#include "cn10k_worker.h"
#include "cn10k_ethdev.h"
#include "cn10k_cryptodev_ops.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define CN10K_SET_EVDEV_DEQ_OP(dev, deq_op, deq_ops)                           \
	deq_op = deq_ops[dev->rx_offloads & (NIX_RX_OFFLOAD_MAX - 1)]

#define CN10K_SET_EVDEV_ENQ_OP(dev, enq_op, enq_ops)                           \
	enq_op = enq_ops[dev->tx_offloads & (NIX_TX_OFFLOAD_MAX - 1)]

static uint32_t
cn10k_sso_gw_mode_wdata(struct cnxk_sso_evdev *dev)
{
	uint32_t wdata = 1;

	if (dev->deq_tmo_ns)
		wdata |= BIT(16);

	switch (dev->gw_mode) {
	case CN10K_GW_MODE_NONE:
	default:
		break;
	case CN10K_GW_MODE_PREF:
		wdata |= BIT(19);
		break;
	case CN10K_GW_MODE_PREF_WFE:
		wdata |= BIT(20) | BIT(19);
		break;
	}

	return wdata;
}

static void *
cn10k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws;

	/* Allocate event port memory */
	ws = rte_zmalloc("cn10k_ws",
			 sizeof(struct cn10k_sso_hws) + RTE_CACHE_LINE_SIZE,
			 RTE_CACHE_LINE_SIZE);
	if (ws == NULL) {
		plt_err("Failed to alloc memory for port=%d", port_id);
		return NULL;
	}

	/* First cache line is reserved for cookie */
	ws = (struct cn10k_sso_hws *)((uint8_t *)ws + RTE_CACHE_LINE_SIZE);
	ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
	ws->hws_id = port_id;
	ws->swtag_req = 0;
	ws->gw_wdata = cn10k_sso_gw_mode_wdata(dev);
	ws->gw_rdata = SSO_TT_EMPTY << 32;
	ws->lmt_base = dev->sso.lmt_base;

	return ws;
}

static int
cn10k_sso_hws_link(void *arg, void *port, uint16_t *map, uint16_t nb_link)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = port;

	return roc_sso_hws_link(&dev->sso, ws->hws_id, map, nb_link);
}

static int
cn10k_sso_hws_unlink(void *arg, void *port, uint16_t *map, uint16_t nb_link)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = port;

	return roc_sso_hws_unlink(&dev->sso, ws->hws_id, map, nb_link);
}

static void
cn10k_sso_hws_setup(void *arg, void *hws, uintptr_t grp_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = hws;
	uint64_t val;

	ws->grp_base = grp_base;
	ws->fc_mem = (uint64_t *)dev->fc_iova;
	ws->xaq_lmt = dev->xaq_lmt;

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns);
	val = val ? val - 1 : 0;
	plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
}

static void
cn10k_sso_hws_release(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = hws;
	uint16_t i;

	for (i = 0; i < dev->nb_event_queues; i++)
		roc_sso_hws_unlink(&dev->sso, ws->hws_id, &i, 1);
	memset(ws, 0, sizeof(*ws));
}

static int
cn10k_sso_hws_flush_events(void *hws, uint8_t queue_id, uintptr_t base,
			   cnxk_handle_event_t fn, void *arg)
{
	uint64_t retry = CNXK_SSO_FLUSH_RETRY_MAX;
	struct cn10k_sso_hws *ws = hws;
	uint64_t cq_ds_cnt = 1;
	uint64_t aq_cnt = 1;
	uint64_t ds_cnt = 1;
	struct rte_event ev;
	uint64_t val, req;

	plt_write64(0, base + SSO_LF_GGRP_QCTL);

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
		cn10k_sso_hws_get_work_empty(
			ws, &ev,
			(NIX_RX_OFFLOAD_MAX - 1) | NIX_RX_REAS_F |
				NIX_RX_MULTI_SEG_F | CPT_RX_WQE_F);
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
	rte_mb();

	return 0;
}

static void
cn10k_sso_hws_reset(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = hws;
	uintptr_t base = ws->base;
	uint64_t pend_state;
	union {
		__uint128_t wdata;
		uint64_t u64[2];
	} gw;
	uint8_t pend_tt;
	bool is_pend;

	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
	/* Wait till getwork/swtp/waitw/desched completes. */
	is_pend = false;
	/* Work in WQE0 is always consumed, unless its a SWTAG. */
	pend_state = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	if (pend_state & (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(54)) ||
	    ws->swtag_req)
		is_pend = true;

	do {
		pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
	} while (pend_state & (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(58) |
			       BIT_ULL(56) | BIT_ULL(54)));
	pend_tt = CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_WQE0));
	if (is_pend && pend_tt != SSO_TT_EMPTY) { /* Work was pending */
		if (pend_tt == SSO_TT_ATOMIC || pend_tt == SSO_TT_ORDERED)
			cnxk_sso_hws_swtag_untag(base +
						 SSOW_LF_GWS_OP_SWTAG_UNTAG);
		plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
	}

	/* Wait for desched to complete. */
	do {
		pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
	} while (pend_state & BIT_ULL(58));

	switch (dev->gw_mode) {
	case CN10K_GW_MODE_PREF:
	case CN10K_GW_MODE_PREF_WFE:
		while (plt_read64(base + SSOW_LF_GWS_PRF_WQE0) & BIT_ULL(63))
			;
		break;
	case CN10K_GW_MODE_NONE:
	default:
		break;
	}

	if (CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_PRF_WQE0)) !=
	    SSO_TT_EMPTY) {
		plt_write64(BIT_ULL(16) | 1,
			    ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		do {
			roc_load_pair(gw.u64[0], gw.u64[1],
				      ws->base + SSOW_LF_GWS_WQE0);
		} while (gw.u64[0] & BIT_ULL(63));
		pend_tt = CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_WQE0));
		if (pend_tt != SSO_TT_EMPTY) { /* Work was pending */
			if (pend_tt == SSO_TT_ATOMIC ||
			    pend_tt == SSO_TT_ORDERED)
				cnxk_sso_hws_swtag_untag(
					base + SSOW_LF_GWS_OP_SWTAG_UNTAG);
			plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
		}
	}

	plt_write64(0, base + SSOW_LF_GWS_OP_GWC_INVAL);
	rte_mb();
}

static void
cn10k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues =
		dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
			      RTE_EVENT_MAX_QUEUES_PER_DEV :
			      dev->sso.max_hwgrp;
}

static int
cn10k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_tim_evdev *tim_dev = cnxk_tim_priv_get();
	struct cnxk_sso_evdev *dev = arg;
	uint16_t nb_tim_lfs;

	nb_tim_lfs = tim_dev ? tim_dev->nb_rings : 0;
	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp, nb_tim_lfs);
}

static int
cn10k_sso_updt_tx_adptr_data(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	if (dev->tx_adptr_data == NULL)
		return 0;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cn10k_sso_hws *ws = event_dev->data->ports[i];
		void *ws_cookie;

		ws_cookie = cnxk_sso_hws_get_cookie(ws);
		ws_cookie = rte_realloc_socket(
			ws_cookie,
			sizeof(struct cnxk_sso_hws_cookie) +
				sizeof(struct cn10k_sso_hws) +
				dev->tx_adptr_data_sz,
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (ws_cookie == NULL)
			return -ENOMEM;
		ws = RTE_PTR_ADD(ws_cookie, sizeof(struct cnxk_sso_hws_cookie));
		memcpy(&ws->tx_adptr_data, dev->tx_adptr_data,
		       dev->tx_adptr_data_sz);
		event_dev->data->ports[i] = ws;
	}

	return 0;
}

static void
cn10k_sso_fp_fns_set(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_cpt *cpt = roc_idev_cpt_get();
	const event_dequeue_t sso_hws_deq[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_ca[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_ca_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_ca_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo_ca[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_ca_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_ca_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_seg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_ca_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_ca_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_ca_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo_ca_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_ca_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_deq_tmo_ca_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_tmo[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_ca[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_ca_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_ca_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_tmo_ca[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_ca_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_ca_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_seg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_tmo_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_ca_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_ca_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_ca_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_reas_deq_tmo_ca_seg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_reas_deq_tmo_ca_seg_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_sso_hws_reas_deq_tmo_ca_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Tx modes */
	const event_tx_adapter_enqueue_t sso_hws_tx_adptr_enq[NIX_TX_OFFLOAD_MAX] = {
#define T(name, sz, flags)[flags] = cn10k_sso_hws_tx_adptr_enq_##name,
		NIX_TX_FASTPATH_MODES
#undef T
	};

	const event_tx_adapter_enqueue_t sso_hws_tx_adptr_enq_seg[NIX_TX_OFFLOAD_MAX] = {
#define T(name, sz, flags)[flags] = cn10k_sso_hws_tx_adptr_enq_seg_##name,
		NIX_TX_FASTPATH_MODES
#undef T
	};

	event_dev->enqueue = cn10k_sso_hws_enq;
	event_dev->enqueue_burst = cn10k_sso_hws_enq_burst;
	event_dev->enqueue_new_burst = cn10k_sso_hws_enq_new_burst;
	event_dev->enqueue_forward_burst = cn10k_sso_hws_enq_fwd_burst;
	if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
		if (dev->rx_offloads & NIX_RX_REAS_F) {
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_reas_deq_seg);
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_reas_deq_seg_burst);
			if (dev->is_timeout_deq) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_tmo_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_seg_burst);
			}
			if (dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_ca_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_ca_seg_burst);
			}
			if (dev->is_timeout_deq && dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_tmo_ca_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_ca_seg_burst);
			}
		} else {
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_seg);
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_deq_seg_burst);

			if (dev->is_timeout_deq) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_deq_tmo_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_seg_burst);
			}
			if (dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_ca_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_ca_seg_burst);
			}
			if (dev->is_timeout_deq && dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_deq_tmo_ca_seg);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_ca_seg_burst);
			}
		}
	} else {
		if (dev->rx_offloads & NIX_RX_REAS_F) {
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_reas_deq);
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					       sso_hws_reas_deq_burst);

			if (dev->is_timeout_deq) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_tmo);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_burst);
			}
			if (dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_ca);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_ca_burst);
			}
			if (dev->is_timeout_deq && dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						       sso_hws_reas_deq_tmo_ca);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_reas_deq_tmo_ca_burst);
			}
		} else {
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq);
			CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst, sso_hws_deq_burst);

			if (dev->is_timeout_deq) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_tmo);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_burst);
			}
			if (dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_ca);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_ca_burst);
			}
			if (dev->is_timeout_deq && dev->is_ca_internal_port) {
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_tmo_ca);
				CN10K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
						       sso_hws_deq_tmo_ca_burst);
			}
		}
	}

	if ((cpt != NULL) && cpt->hw_caps[CPT_ENG_TYPE_SE].sg_ver2 &&
	    cpt->hw_caps[CPT_ENG_TYPE_IE].sg_ver2)
		event_dev->ca_enqueue = cn10k_cpt_sg_ver2_crypto_adapter_enqueue;
	else
		event_dev->ca_enqueue = cn10k_cpt_sg_ver1_crypto_adapter_enqueue;

	if (dev->tx_offloads & NIX_TX_MULTI_SEG_F)
		CN10K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue, sso_hws_tx_adptr_enq_seg);
	else
		CN10K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue, sso_hws_tx_adptr_enq);

	event_dev->txa_enqueue_same_dest = event_dev->txa_enqueue;
}

static void
cn10k_sso_info_get(struct rte_eventdev *event_dev,
		   struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN10K_PMD);
	cnxk_sso_info_get(dev, dev_info);
}

static int
cn10k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	rc = cn10k_sso_rsrc_init(dev, dev->nb_event_ports,
				 dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	rc = cnxk_setup_event_ports(event_dev, cn10k_sso_init_hws_mem,
				    cn10k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	/* Restore any prior port-queue mapping. */
	cnxk_sso_restore_links(event_dev, cn10k_sso_hws_link);

	dev->configured = 1;
	rte_mb();

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn10k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn10k_sso_hws_setup);
}

static void
cn10k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn10k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static void
cn10k_sso_port_quiesce(struct rte_eventdev *event_dev, void *port,
		       rte_eventdev_port_flush_t flush_cb, void *args)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cn10k_sso_hws *ws = port;
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
	} while (ptag &
		 (BIT_ULL(62) | BIT_ULL(58) | BIT_ULL(56) | BIT_ULL(54)));

	cn10k_sso_hws_get_work_empty(ws, &ev,
				     (NIX_RX_OFFLOAD_MAX - 1) | NIX_RX_REAS_F |
					     NIX_RX_MULTI_SEG_F | CPT_RX_WQE_F);
	if (is_pend && ev.u64) {
		if (flush_cb)
			flush_cb(event_dev->data->dev_id, ev, args);
		cnxk_sso_hws_swtag_flush(ws->base);
	}

	/* Check if we have work in PRF_WQE0, if so extract it. */
	switch (dev->gw_mode) {
	case CN10K_GW_MODE_PREF:
	case CN10K_GW_MODE_PREF_WFE:
		while (plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0) &
		       BIT_ULL(63))
			;
		break;
	case CN10K_GW_MODE_NONE:
	default:
		break;
	}

	if (CNXK_TT_FROM_TAG(plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0)) !=
	    SSO_TT_EMPTY) {
		plt_write64(BIT_ULL(16) | 1,
			    ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		cn10k_sso_hws_get_work_empty(
			ws, &ev,
			(NIX_RX_OFFLOAD_MAX - 1) | NIX_RX_REAS_F |
				NIX_RX_MULTI_SEG_F | CPT_RX_WQE_F);
		if (ev.u64) {
			if (flush_cb)
				flush_cb(event_dev->data->dev_id, ev, args);
			cnxk_sso_hws_swtag_flush(ws->base);
		}
	}
	ws->swtag_req = 0;
	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
}

static int
cn10k_sso_port_link(struct rte_eventdev *event_dev, void *port,
		    const uint8_t queues[], const uint8_t priorities[],
		    uint16_t nb_links)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_links];
	uint16_t link;

	RTE_SET_USED(priorities);
	for (link = 0; link < nb_links; link++)
		hwgrp_ids[link] = queues[link];
	nb_links = cn10k_sso_hws_link(dev, port, hwgrp_ids, nb_links);

	return (int)nb_links;
}

static int
cn10k_sso_port_unlink(struct rte_eventdev *event_dev, void *port,
		      uint8_t queues[], uint16_t nb_unlinks)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_unlinks];
	uint16_t unlink;

	for (unlink = 0; unlink < nb_unlinks; unlink++)
		hwgrp_ids[unlink] = queues[unlink];
	nb_unlinks = cn10k_sso_hws_unlink(dev, port, hwgrp_ids, nb_unlinks);

	return (int)nb_unlinks;
}

static void
cn10k_sso_configure_queue_stash(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stash stash[dev->stash_cnt];
	int i, rc;

	plt_sso_dbg();
	for (i = 0; i < dev->stash_cnt; i++) {
		stash[i].hwgrp = dev->stash_parse_data[i].queue;
		stash[i].stash_offset = dev->stash_parse_data[i].stash_offset;
		stash[i].stash_count = dev->stash_parse_data[i].stash_length;
	}
	rc = roc_sso_hwgrp_stash_config(&dev->sso, stash, dev->stash_cnt);
	if (rc < 0)
		plt_warn("failed to configure HWGRP WQE stashing rc = %d", rc);
}

static int
cn10k_sso_start(struct rte_eventdev *event_dev)
{
	int rc;

	rc = cn10k_sso_updt_tx_adptr_data(event_dev);
	if (rc < 0)
		return rc;

	cn10k_sso_configure_queue_stash(event_dev);
	rc = cnxk_sso_start(event_dev, cn10k_sso_hws_reset,
			    cn10k_sso_hws_flush_events);
	if (rc < 0)
		return rc;
	cn10k_sso_fp_fns_set(event_dev);

	return rc;
}

static void
cn10k_sso_stop(struct rte_eventdev *event_dev)
{
	cnxk_sso_stop(event_dev, cn10k_sso_hws_reset,
		      cn10k_sso_hws_flush_events);
}

static int
cn10k_sso_close(struct rte_eventdev *event_dev)
{
	return cnxk_sso_close(event_dev, cn10k_sso_hws_unlink);
}

static int
cn10k_sso_selftest(void)
{
	return cnxk_sso_selftest(RTE_STR(event_cn10k));
}

static int
cn10k_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_cn10k", 9);
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
cn10k_sso_set_priv_mem(const struct rte_eventdev *event_dev, void *lookup_mem)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cn10k_sso_hws *ws = event_dev->data->ports[i];
		ws->xaq_lmt = dev->xaq_lmt;
		ws->fc_mem = (uint64_t *)dev->fc_iova;
		ws->tstamp = dev->tstamp;
		if (lookup_mem)
			ws->lookup_mem = lookup_mem;
	}
}

static int
cn10k_sso_rx_adapter_queue_add(
	const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
	int32_t rx_queue_id,
	const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stash stash;
	struct cn10k_eth_rxq *rxq;
	void *lookup_mem;
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn10k", 8);
	if (rc)
		return -EINVAL;

	rc = cnxk_sso_rx_adapter_queue_add(event_dev, eth_dev, rx_queue_id,
					   queue_conf);
	if (rc)
		return -EINVAL;
	rxq = eth_dev->data->rx_queues[0];
	lookup_mem = rxq->lookup_mem;
	cn10k_sso_set_priv_mem(event_dev, lookup_mem);
	cn10k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	if (roc_feature_sso_has_stash()) {
		stash.hwgrp = queue_conf->ev.queue_id;
		stash.stash_offset = CN10K_SSO_DEFAULT_STASH_OFFSET;
		stash.stash_count = CN10K_SSO_DEFAULT_STASH_LENGTH;
		rc = roc_sso_hwgrp_stash_config(&dev->sso, &stash, 1);
		if (rc < 0)
			plt_warn("failed to configure HWGRP WQE stashing rc = %d", rc);
	}

	return 0;
}

static int
cn10k_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev,
			       int32_t rx_queue_id)
{
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn10k", 8);
	if (rc)
		return -EINVAL;

	return cnxk_sso_rx_adapter_queue_del(event_dev, eth_dev, rx_queue_id);
}

static int
cn10k_sso_rx_adapter_vector_limits(
	const struct rte_eventdev *dev, const struct rte_eth_dev *eth_dev,
	struct rte_event_eth_rx_adapter_vector_limits *limits)
{
	struct cnxk_eth_dev *cnxk_eth_dev;
	int ret;

	RTE_SET_USED(dev);
	ret = strncmp(eth_dev->device->driver->name, "net_cn10k", 8);
	if (ret)
		return -ENOTSUP;

	cnxk_eth_dev = cnxk_eth_pmd_priv(eth_dev);
	limits->log2_sz = true;
	limits->min_sz = 1 << ROC_NIX_VWQE_MIN_SIZE_LOG2;
	limits->max_sz = 1 << ROC_NIX_VWQE_MAX_SIZE_LOG2;
	limits->min_timeout_ns =
		(roc_nix_get_vwqe_interval(&cnxk_eth_dev->nix) + 1) * 100;
	limits->max_timeout_ns = BITMASK_ULL(8, 0) * limits->min_timeout_ns;

	return 0;
}

static int
cn10k_sso_tx_adapter_caps_get(const struct rte_eventdev *dev,
			      const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int ret;

	RTE_SET_USED(dev);
	ret = strncmp(eth_dev->device->driver->name, "net_cn10k", 8);
	if (ret)
		*caps = 0;
	else
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR;

	return 0;
}

static void
cn10k_sso_txq_fc_update(const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cn10k_eth_txq *txq;
	struct roc_nix_sq *sq;
	int i;

	if (tx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
			cn10k_sso_txq_fc_update(eth_dev, i);
	} else {
		uint16_t sqes_per_sqb;

		sq = &cnxk_eth_dev->sqs[tx_queue_id];
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sqes_per_sqb = 1U << txq->sqes_per_sqb_log2;
		sq->nb_sqb_bufs_adj =
			sq->nb_sqb_bufs -
			RTE_ALIGN_MUL_CEIL(sq->nb_sqb_bufs, sqes_per_sqb) /
				sqes_per_sqb;
		if (cnxk_eth_dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY)
			sq->nb_sqb_bufs_adj -= (cnxk_eth_dev->outb.nb_desc /
						(sqes_per_sqb - 1));
		txq->nb_sqb_bufs_adj = sq->nb_sqb_bufs_adj;
		txq->nb_sqb_bufs_adj =
			(ROC_NIX_SQB_LOWER_THRESH * txq->nb_sqb_bufs_adj) / 100;
	}
}

static int
cn10k_sso_tx_adapter_queue_add(uint8_t id, const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev,
			       int32_t tx_queue_id)
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
		uint8_t tstmp_ena =
			!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F);

		if (tstmp_ena && !tstmp_req)
			dev->tx_offloads &= ~(NIX_TX_OFFLOAD_TSTAMP_F);
		else if (!tstmp_ena && tstmp_req)
			tx_offloads &= ~(NIX_TX_OFFLOAD_TSTAMP_F);
	}

	dev->tx_offloads |= tx_offloads;
	cn10k_sso_txq_fc_update(eth_dev, tx_queue_id);
	rc = cn10k_sso_updt_tx_adptr_data(event_dev);
	if (rc < 0)
		return rc;
	cn10k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	dev->tx_adptr_configured = 1;

	return 0;
}

static int
cn10k_sso_tx_adapter_queue_del(uint8_t id, const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev,
			       int32_t tx_queue_id)
{
	int rc;

	RTE_SET_USED(id);
	rc = cnxk_sso_tx_adapter_queue_del(event_dev, eth_dev, tx_queue_id);
	if (rc < 0)
		return rc;
	return cn10k_sso_updt_tx_adptr_data(event_dev);
}

static int
cn10k_crypto_adapter_caps_get(const struct rte_eventdev *event_dev,
			      const struct rte_cryptodev *cdev, uint32_t *caps)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn10k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn10k");

	*caps = RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_EVENT_VECTOR;

	return 0;
}

static int
cn10k_crypto_adapter_qp_add(const struct rte_eventdev *event_dev,
			    const struct rte_cryptodev *cdev,
			    int32_t queue_pair_id,
			    const struct rte_event_crypto_adapter_queue_conf *conf)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int ret;

	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn10k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn10k");

	dev->is_ca_internal_port = 1;
	cn10k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	ret = cnxk_crypto_adapter_qp_add(event_dev, cdev, queue_pair_id, conf);
	cn10k_sso_set_priv_mem(event_dev, NULL);

	return ret;
}

static int
cn10k_crypto_adapter_qp_del(const struct rte_eventdev *event_dev, const struct rte_cryptodev *cdev,
			    int32_t queue_pair_id)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn10k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn10k");

	return cnxk_crypto_adapter_qp_del(cdev, queue_pair_id);
}

static int
cn10k_tim_caps_get(const struct rte_eventdev *evdev, uint64_t flags,
		   uint32_t *caps, const struct event_timer_adapter_ops **ops)
{
	return cnxk_tim_caps_get(evdev, flags, caps, ops,
				 cn10k_sso_set_priv_mem);
}

static int
cn10k_crypto_adapter_vec_limits(const struct rte_eventdev *event_dev,
				const struct rte_cryptodev *cdev,
				struct rte_event_crypto_adapter_vector_limits *limits)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn10k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn10k");

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

static struct eventdev_ops cn10k_sso_dev_ops = {
	.dev_infos_get = cn10k_sso_info_get,
	.dev_configure = cn10k_sso_dev_configure,

	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cnxk_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.queue_attr_set = cnxk_sso_queue_attribute_set,

	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn10k_sso_port_setup,
	.port_release = cn10k_sso_port_release,
	.port_quiesce = cn10k_sso_port_quiesce,
	.port_link = cn10k_sso_port_link,
	.port_unlink = cn10k_sso_port_unlink,
	.timeout_ticks = cnxk_sso_timeout_ticks,

	.eth_rx_adapter_caps_get = cn10k_sso_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = cn10k_sso_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del = cn10k_sso_rx_adapter_queue_del,
	.eth_rx_adapter_start = cnxk_sso_rx_adapter_start,
	.eth_rx_adapter_stop = cnxk_sso_rx_adapter_stop,

	.eth_rx_adapter_vector_limits_get = cn10k_sso_rx_adapter_vector_limits,

	.eth_tx_adapter_caps_get = cn10k_sso_tx_adapter_caps_get,
	.eth_tx_adapter_queue_add = cn10k_sso_tx_adapter_queue_add,
	.eth_tx_adapter_queue_del = cn10k_sso_tx_adapter_queue_del,
	.eth_tx_adapter_start = cnxk_sso_tx_adapter_start,
	.eth_tx_adapter_stop = cnxk_sso_tx_adapter_stop,
	.eth_tx_adapter_free = cnxk_sso_tx_adapter_free,

	.timer_adapter_caps_get = cn10k_tim_caps_get,

	.crypto_adapter_caps_get = cn10k_crypto_adapter_caps_get,
	.crypto_adapter_queue_pair_add = cn10k_crypto_adapter_qp_add,
	.crypto_adapter_queue_pair_del = cn10k_crypto_adapter_qp_del,
	.crypto_adapter_vector_limits_get = cn10k_crypto_adapter_vec_limits,

	.xstats_get = cnxk_sso_xstats_get,
	.xstats_reset = cnxk_sso_xstats_reset,
	.xstats_get_names = cnxk_sso_xstats_get_names,

	.dump = cnxk_sso_dump,
	.dev_start = cn10k_sso_start,
	.dev_stop = cn10k_sso_stop,
	.dev_close = cn10k_sso_close,
	.dev_selftest = cn10k_sso_selftest,
};

static int
cn10k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn10k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		cn10k_sso_fp_fns_set(event_dev);
		return 0;
	}

	dev->gw_mode = CN10K_GW_MODE_PREF_WFE;
	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn10k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
	if (!dev->max_event_ports || !dev->max_event_queues) {
		plt_err("Not enough eventdev resource queues=%d ports=%d",
			dev->max_event_queues, dev->max_event_ports);
		cnxk_sso_fini(event_dev);
		return -ENODEV;
	}

	plt_sso_dbg("Initializing %s max_queues=%d max_ports=%d",
		    event_dev->data->name, dev->max_event_queues,
		    dev->max_event_ports);

	return 0;
}

static int
cn10k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct cnxk_sso_evdev),
				       cn10k_sso_init);
}

static const struct rte_pci_id cn10k_pci_sso_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn10k_pci_sso = {
	.id_table = cn10k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn10k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn10k, cn10k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn10k, cn10k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn10k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn10k, CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>"
			      CNXK_SSO_FORCE_BP "=1"
			      CN10K_SSO_GW_MODE "=<int>"
			      CN10K_SSO_STASH "=<string>"
			      CNXK_TIM_DISABLE_NPA "=1"
			      CNXK_TIM_CHNK_SLOTS "=<int>"
			      CNXK_TIM_RINGS_LMT "=<int>"
			      CNXK_TIM_STATS_ENA "=1"
			      CNXK_TIM_EXT_CLK "=<string>");
