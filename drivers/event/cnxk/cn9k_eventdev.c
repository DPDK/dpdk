/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define CN9K_DUAL_WS_NB_WS	    2
#define CN9K_DUAL_WS_PAIR_ID(x, id) (((x)*CN9K_DUAL_WS_NB_WS) + id)

#define CN9K_SET_EVDEV_DEQ_OP(dev, deq_op, deq_ops)                            \
	(deq_op = deq_ops[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]  \
			 [!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]      \
			 [!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)] \
			 [!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]    \
			 [!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]       \
			 [!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)])

#define CN9K_SET_EVDEV_ENQ_OP(dev, enq_op, enq_ops)                            \
	(enq_op =                                                              \
		 enq_ops[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F)]       \
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSO_F)]          \
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_MBUF_NOFF_F)]    \
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_VLAN_QINQ_F)]    \
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)] \
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_L3_L4_CSUM_F)])

static void
cn9k_init_hws_ops(struct cn9k_sso_hws_state *ws, uintptr_t base)
{
	ws->tag_op = base + SSOW_LF_GWS_TAG;
	ws->wqp_op = base + SSOW_LF_GWS_WQP;
	ws->getwrk_op = base + SSOW_LF_GWS_OP_GET_WORK0;
	ws->swtag_flush_op = base + SSOW_LF_GWS_OP_SWTAG_FLUSH;
	ws->swtag_norm_op = base + SSOW_LF_GWS_OP_SWTAG_NORM;
	ws->swtag_desched_op = base + SSOW_LF_GWS_OP_SWTAG_DESCHED;
}

static int
cn9k_sso_hws_link(void *arg, void *port, uint16_t *map, uint16_t nb_link)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	int rc;

	if (dev->dual_ws) {
		dws = port;
		rc = roc_sso_hws_link(&dev->sso,
				      CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 0), map,
				      nb_link);
		rc |= roc_sso_hws_link(&dev->sso,
				       CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 1),
				       map, nb_link);
	} else {
		ws = port;
		rc = roc_sso_hws_link(&dev->sso, ws->hws_id, map, nb_link);
	}

	return rc;
}

static int
cn9k_sso_hws_unlink(void *arg, void *port, uint16_t *map, uint16_t nb_link)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	int rc;

	if (dev->dual_ws) {
		dws = port;
		rc = roc_sso_hws_unlink(&dev->sso,
					CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 0),
					map, nb_link);
		rc |= roc_sso_hws_unlink(&dev->sso,
					 CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 1),
					 map, nb_link);
	} else {
		ws = port;
		rc = roc_sso_hws_unlink(&dev->sso, ws->hws_id, map, nb_link);
	}

	return rc;
}

static void
cn9k_sso_hws_setup(void *arg, void *hws, uintptr_t *grps_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	uint64_t val;

	/* Set get_work tmo for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns) - 1;
	if (dev->dual_ws) {
		dws = hws;
		rte_memcpy(dws->grps_base, grps_base,
			   sizeof(uintptr_t) * CNXK_SSO_MAX_HWGRP);
		dws->fc_mem = dev->fc_mem;
		dws->xaq_lmt = dev->xaq_lmt;

		plt_write64(val, dws->base[0] + SSOW_LF_GWS_NW_TIM);
		plt_write64(val, dws->base[1] + SSOW_LF_GWS_NW_TIM);
	} else {
		ws = hws;
		rte_memcpy(ws->grps_base, grps_base,
			   sizeof(uintptr_t) * CNXK_SSO_MAX_HWGRP);
		ws->fc_mem = dev->fc_mem;
		ws->xaq_lmt = dev->xaq_lmt;

		plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
	}
}

static void
cn9k_sso_hws_release(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	int i;

	if (dev->dual_ws) {
		dws = hws;
		for (i = 0; i < dev->nb_event_queues; i++) {
			roc_sso_hws_unlink(&dev->sso,
					   CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 0),
					   (uint16_t *)&i, 1);
			roc_sso_hws_unlink(&dev->sso,
					   CN9K_DUAL_WS_PAIR_ID(dws->hws_id, 1),
					   (uint16_t *)&i, 1);
		}
		memset(dws, 0, sizeof(*dws));
	} else {
		ws = hws;
		for (i = 0; i < dev->nb_event_queues; i++)
			roc_sso_hws_unlink(&dev->sso, ws->hws_id,
					   (uint16_t *)&i, 1);
		memset(ws, 0, sizeof(*ws));
	}
}

static void
cn9k_sso_hws_flush_events(void *hws, uint8_t queue_id, uintptr_t base,
			  cnxk_handle_event_t fn, void *arg)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(arg);
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws_state *st;
	struct cn9k_sso_hws *ws;
	uint64_t cq_ds_cnt = 1;
	uint64_t aq_cnt = 1;
	uint64_t ds_cnt = 1;
	struct rte_event ev;
	uintptr_t ws_base;
	uint64_t val, req;

	plt_write64(0, base + SSO_LF_GGRP_QCTL);

	req = queue_id;	    /* GGRP ID */
	req |= BIT_ULL(18); /* Grouped */
	req |= BIT_ULL(16); /* WAIT */

	aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
	ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
	cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
	cq_ds_cnt &= 0x3FFF3FFF0000;

	if (dev->dual_ws) {
		dws = hws;
		st = &dws->ws_state[0];
		ws_base = dws->base[0];
	} else {
		ws = hws;
		st = (struct cn9k_sso_hws_state *)ws;
		ws_base = ws->base;
	}

	while (aq_cnt || cq_ds_cnt || ds_cnt) {
		plt_write64(req, st->getwrk_op);
		cn9k_sso_hws_get_work_empty(st, &ev);
		if (fn != NULL && ev.u64 != 0)
			fn(arg, ev);
		if (ev.sched_type != SSO_TT_EMPTY)
			cnxk_sso_hws_swtag_flush(st->tag_op,
						 st->swtag_flush_op);
		do {
			val = plt_read64(ws_base + SSOW_LF_GWS_PENDSTATE);
		} while (val & BIT_ULL(56));
		aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
		ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
		cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
		/* Extract cq and ds count */
		cq_ds_cnt &= 0x3FFF3FFF0000;
	}

	plt_write64(0, ws_base + SSOW_LF_GWS_OP_GWC_INVAL);
}

static void
cn9k_sso_hws_reset(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	uint64_t pend_state;
	uint8_t pend_tt;
	uintptr_t base;
	uint64_t tag;
	uint8_t i;

	dws = hws;
	ws = hws;
	for (i = 0; i < (dev->dual_ws ? CN9K_DUAL_WS_NB_WS : 1); i++) {
		base = dev->dual_ws ? dws->base[i] : ws->base;
		/* Wait till getwork/swtp/waitw/desched completes. */
		do {
			pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
		} while (pend_state & (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(58) |
				       BIT_ULL(56)));

		tag = plt_read64(base + SSOW_LF_GWS_TAG);
		pend_tt = (tag >> 32) & 0x3;
		if (pend_tt != SSO_TT_EMPTY) { /* Work was pending */
			if (pend_tt == SSO_TT_ATOMIC ||
			    pend_tt == SSO_TT_ORDERED)
				cnxk_sso_hws_swtag_untag(
					base + SSOW_LF_GWS_OP_SWTAG_UNTAG);
			plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
		}

		/* Wait for desched to complete. */
		do {
			pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
		} while (pend_state & BIT_ULL(58));
	}
}

void
cn9k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	if (dev->dual_ws)
		dev->max_event_ports = dev->sso.max_hws / CN9K_DUAL_WS_NB_WS;
	else
		dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues =
		dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
			      RTE_EVENT_MAX_QUEUES_PER_DEV :
			      dev->sso.max_hwgrp;
}

static int
cn9k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_sso_evdev *dev = arg;

	if (dev->dual_ws)
		hws = hws * CN9K_DUAL_WS_NB_WS;

	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp);
}

static int
cn9k_sso_updt_tx_adptr_data(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	if (dev->tx_adptr_data == NULL)
		return 0;

	for (i = 0; i < dev->nb_event_ports; i++) {
		if (dev->dual_ws) {
			struct cn9k_sso_hws_dual *dws =
				event_dev->data->ports[i];
			void *ws_cookie;

			ws_cookie = cnxk_sso_hws_get_cookie(dws);
			ws_cookie = rte_realloc_socket(
				ws_cookie,
				sizeof(struct cnxk_sso_hws_cookie) +
					sizeof(struct cn9k_sso_hws_dual) +
					(sizeof(uint64_t) *
					 (dev->max_port_id + 1) *
					 RTE_MAX_QUEUES_PER_PORT),
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
			if (ws_cookie == NULL)
				return -ENOMEM;
			dws = RTE_PTR_ADD(ws_cookie,
					  sizeof(struct cnxk_sso_hws_cookie));
			memcpy(&dws->tx_adptr_data, dev->tx_adptr_data,
			       sizeof(uint64_t) * (dev->max_port_id + 1) *
				       RTE_MAX_QUEUES_PER_PORT);
			event_dev->data->ports[i] = dws;
		} else {
			struct cn9k_sso_hws *ws = event_dev->data->ports[i];
			void *ws_cookie;

			ws_cookie = cnxk_sso_hws_get_cookie(ws);
			ws_cookie = rte_realloc_socket(
				ws_cookie,
				sizeof(struct cnxk_sso_hws_cookie) +
					sizeof(struct cn9k_sso_hws_dual) +
					(sizeof(uint64_t) *
					 (dev->max_port_id + 1) *
					 RTE_MAX_QUEUES_PER_PORT),
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
			if (ws_cookie == NULL)
				return -ENOMEM;
			ws = RTE_PTR_ADD(ws_cookie,
					 sizeof(struct cnxk_sso_hws_cookie));
			memcpy(&ws->tx_adptr_data, dev->tx_adptr_data,
			       sizeof(uint64_t) * (dev->max_port_id + 1) *
				       RTE_MAX_QUEUES_PER_PORT);
			event_dev->data->ports[i] = ws;
		}
	}
	rte_mb();

	return 0;
}

static void
cn9k_sso_fp_fns_set(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	/* Single WS modes */
	const event_dequeue_t sso_hws_deq[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_tmo_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_tmo_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_tmo_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_ca[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_ca_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_ca_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_deq_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_seg_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_tmo_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_tmo_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_deq_tmo_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_tmo_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_deq_ca_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_deq_ca_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_deq_ca_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Dual WS modes */
	const event_dequeue_t sso_hws_dual_deq[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t sso_hws_dual_deq_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_burst_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_dual_deq_tmo[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_tmo_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_dual_deq_tmo_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_tmo_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_dual_deq_ca[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_ca_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_dual_deq_ca_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_ca_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_t sso_hws_dual_deq_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_dual_deq_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
		};

	const event_dequeue_t sso_hws_dual_deq_tmo_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_tmo_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_dual_deq_tmo_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_tmo_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
		};

	const event_dequeue_t sso_hws_dual_deq_ca_seg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_ca_seg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const event_dequeue_burst_t
		sso_hws_dual_deq_ca_seg_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_deq_ca_seg_burst_##name,
			NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Tx modes */
	const event_tx_adapter_enqueue
		sso_hws_tx_adptr_enq[2][2][2][2][2][2] = {
#define T(name, f5, f4, f3, f2, f1, f0, sz, flags)                             \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_tx_adptr_enq_##name,
			NIX_TX_FASTPATH_MODES
#undef T
		};

	const event_tx_adapter_enqueue
		sso_hws_tx_adptr_enq_seg[2][2][2][2][2][2] = {
#define T(name, f5, f4, f3, f2, f1, f0, sz, flags)                             \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_tx_adptr_enq_seg_##name,
			NIX_TX_FASTPATH_MODES
#undef T
		};

	const event_tx_adapter_enqueue
		sso_hws_dual_tx_adptr_enq[2][2][2][2][2][2] = {
#define T(name, f5, f4, f3, f2, f1, f0, sz, flags)                             \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_tx_adptr_enq_##name,
			NIX_TX_FASTPATH_MODES
#undef T
		};

	const event_tx_adapter_enqueue
		sso_hws_dual_tx_adptr_enq_seg[2][2][2][2][2][2] = {
#define T(name, f5, f4, f3, f2, f1, f0, sz, flags)                             \
	[f5][f4][f3][f2][f1][f0] = cn9k_sso_hws_dual_tx_adptr_enq_seg_##name,
			NIX_TX_FASTPATH_MODES
#undef T
		};

	event_dev->enqueue = cn9k_sso_hws_enq;
	event_dev->enqueue_burst = cn9k_sso_hws_enq_burst;
	event_dev->enqueue_new_burst = cn9k_sso_hws_enq_new_burst;
	event_dev->enqueue_forward_burst = cn9k_sso_hws_enq_fwd_burst;
	if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
		CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq_seg);
		CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
				      sso_hws_deq_seg_burst);
		if (dev->is_timeout_deq) {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_deq_tmo_seg);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_deq_tmo_seg_burst);
		}
		if (dev->is_ca_internal_port) {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_deq_ca_seg);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_deq_ca_seg_burst);
		}
	} else {
		CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue, sso_hws_deq);
		CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
				      sso_hws_deq_burst);
		if (dev->is_timeout_deq) {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_deq_tmo);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_deq_tmo_burst);
		}
		if (dev->is_ca_internal_port) {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_deq_ca);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_deq_ca_burst);
		}
	}
	event_dev->ca_enqueue = cn9k_sso_hws_ca_enq;

	if (dev->tx_offloads & NIX_TX_MULTI_SEG_F)
		CN9K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue,
				      sso_hws_tx_adptr_enq_seg);
	else
		CN9K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue,
				      sso_hws_tx_adptr_enq);

	if (dev->dual_ws) {
		event_dev->enqueue = cn9k_sso_hws_dual_enq;
		event_dev->enqueue_burst = cn9k_sso_hws_dual_enq_burst;
		event_dev->enqueue_new_burst = cn9k_sso_hws_dual_enq_new_burst;
		event_dev->enqueue_forward_burst =
			cn9k_sso_hws_dual_enq_fwd_burst;
		event_dev->ca_enqueue = cn9k_sso_hws_dual_ca_enq;

		if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_dual_deq_seg);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_dual_deq_seg_burst);
			if (dev->is_timeout_deq) {
				CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						      sso_hws_dual_deq_tmo_seg);
				CN9K_SET_EVDEV_DEQ_OP(
					dev, event_dev->dequeue_burst,
					sso_hws_dual_deq_tmo_seg_burst);
			}
			if (dev->is_ca_internal_port) {
				CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						      sso_hws_dual_deq_ca_seg);
				CN9K_SET_EVDEV_DEQ_OP(
					dev, event_dev->dequeue_burst,
					sso_hws_dual_deq_ca_seg_burst);
			}
		} else {
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
					      sso_hws_dual_deq);
			CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue_burst,
					      sso_hws_dual_deq_burst);
			if (dev->is_timeout_deq) {
				CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						      sso_hws_dual_deq_tmo);
				CN9K_SET_EVDEV_DEQ_OP(
					dev, event_dev->dequeue_burst,
					sso_hws_dual_deq_tmo_burst);
			}
			if (dev->is_ca_internal_port) {
				CN9K_SET_EVDEV_DEQ_OP(dev, event_dev->dequeue,
						      sso_hws_dual_deq_ca);
				CN9K_SET_EVDEV_DEQ_OP(
					dev, event_dev->dequeue_burst,
					sso_hws_dual_deq_ca_burst);
			}
		}

		if (dev->tx_offloads & NIX_TX_MULTI_SEG_F)
			CN9K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue,
					      sso_hws_dual_tx_adptr_enq_seg);
		else
			CN9K_SET_EVDEV_ENQ_OP(dev, event_dev->txa_enqueue,
					      sso_hws_dual_tx_adptr_enq);
	}

	event_dev->txa_enqueue_same_dest = event_dev->txa_enqueue;
	rte_mb();
}

static void *
cn9k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	void *data;

	if (dev->dual_ws) {
		dws = rte_zmalloc("cn9k_dual_ws",
				  sizeof(struct cn9k_sso_hws_dual) +
					  RTE_CACHE_LINE_SIZE,
				  RTE_CACHE_LINE_SIZE);
		if (dws == NULL) {
			plt_err("Failed to alloc memory for port=%d", port_id);
			return NULL;
		}

		dws = RTE_PTR_ADD(dws, sizeof(struct cnxk_sso_hws_cookie));
		dws->base[0] = roc_sso_hws_base_get(
			&dev->sso, CN9K_DUAL_WS_PAIR_ID(port_id, 0));
		dws->base[1] = roc_sso_hws_base_get(
			&dev->sso, CN9K_DUAL_WS_PAIR_ID(port_id, 1));
		cn9k_init_hws_ops(&dws->ws_state[0], dws->base[0]);
		cn9k_init_hws_ops(&dws->ws_state[1], dws->base[1]);
		dws->hws_id = port_id;
		dws->swtag_req = 0;
		dws->vws = 0;

		data = dws;
	} else {
		/* Allocate event port memory */
		ws = rte_zmalloc("cn9k_ws",
				 sizeof(struct cn9k_sso_hws) +
					 RTE_CACHE_LINE_SIZE,
				 RTE_CACHE_LINE_SIZE);
		if (ws == NULL) {
			plt_err("Failed to alloc memory for port=%d", port_id);
			return NULL;
		}

		/* First cache line is reserved for cookie */
		ws = RTE_PTR_ADD(ws, sizeof(struct cnxk_sso_hws_cookie));
		ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
		cn9k_init_hws_ops((struct cn9k_sso_hws_state *)ws, ws->base);
		ws->hws_id = port_id;
		ws->swtag_req = 0;

		data = ws;
	}

	return data;
}

static void
cn9k_sso_info_get(struct rte_eventdev *event_dev,
		  struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN9K_PMD);
	cnxk_sso_info_get(dev, dev_info);
}

static int
cn9k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	roc_sso_rsrc_fini(&dev->sso);

	rc = cn9k_sso_rsrc_init(dev, dev->nb_event_ports, dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	rc = cnxk_setup_event_ports(event_dev, cn9k_sso_init_hws_mem,
				    cn9k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	/* Restore any prior port-queue mapping. */
	cnxk_sso_restore_links(event_dev, cn9k_sso_hws_link);

	dev->configured = 1;
	rte_mb();

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn9k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		    const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn9k_sso_hws_setup);
}

static void
cn9k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn9k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static int
cn9k_sso_port_link(struct rte_eventdev *event_dev, void *port,
		   const uint8_t queues[], const uint8_t priorities[],
		   uint16_t nb_links)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_links];
	uint16_t link;

	RTE_SET_USED(priorities);
	for (link = 0; link < nb_links; link++)
		hwgrp_ids[link] = queues[link];
	nb_links = cn9k_sso_hws_link(dev, port, hwgrp_ids, nb_links);

	return (int)nb_links;
}

static int
cn9k_sso_port_unlink(struct rte_eventdev *event_dev, void *port,
		     uint8_t queues[], uint16_t nb_unlinks)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_unlinks];
	uint16_t unlink;

	for (unlink = 0; unlink < nb_unlinks; unlink++)
		hwgrp_ids[unlink] = queues[unlink];
	nb_unlinks = cn9k_sso_hws_unlink(dev, port, hwgrp_ids, nb_unlinks);

	return (int)nb_unlinks;
}

static int
cn9k_sso_start(struct rte_eventdev *event_dev)
{
	int rc;

	rc = cn9k_sso_updt_tx_adptr_data(event_dev);
	if (rc < 0)
		return rc;

	rc = cnxk_sso_start(event_dev, cn9k_sso_hws_reset,
			    cn9k_sso_hws_flush_events);
	if (rc < 0)
		return rc;

	cn9k_sso_fp_fns_set(event_dev);

	return rc;
}

static void
cn9k_sso_stop(struct rte_eventdev *event_dev)
{
	cnxk_sso_stop(event_dev, cn9k_sso_hws_reset, cn9k_sso_hws_flush_events);
}

static int
cn9k_sso_close(struct rte_eventdev *event_dev)
{
	return cnxk_sso_close(event_dev, cn9k_sso_hws_unlink);
}

static int
cn9k_sso_selftest(void)
{
	return cnxk_sso_selftest(RTE_STR(event_cn9k));
}

static int
cn9k_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
			     const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_cn9k", 9);
	if (rc)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID;

	return 0;
}

static void
cn9k_sso_set_priv_mem(const struct rte_eventdev *event_dev, void *lookup_mem,
		      void *tstmp_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		if (dev->dual_ws) {
			struct cn9k_sso_hws_dual *dws =
				event_dev->data->ports[i];
			dws->lookup_mem = lookup_mem;
			dws->tstamp = tstmp_info;
		} else {
			struct cn9k_sso_hws *ws = event_dev->data->ports[i];
			ws->lookup_mem = lookup_mem;
			ws->tstamp = tstmp_info;
		}
	}
}

static int
cn9k_sso_rx_adapter_queue_add(
	const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
	int32_t rx_queue_id,
	const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cn9k_eth_rxq *rxq;
	void *lookup_mem;
	void *tstmp_info;
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn9k", 8);
	if (rc)
		return -EINVAL;

	rc = cnxk_sso_rx_adapter_queue_add(event_dev, eth_dev, rx_queue_id,
					   queue_conf);
	if (rc)
		return -EINVAL;

	rxq = eth_dev->data->rx_queues[0];
	lookup_mem = rxq->lookup_mem;
	tstmp_info = rxq->tstamp;
	cn9k_sso_set_priv_mem(event_dev, lookup_mem, tstmp_info);
	cn9k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	return 0;
}

static int
cn9k_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t rx_queue_id)
{
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn9k", 8);
	if (rc)
		return -EINVAL;

	return cnxk_sso_rx_adapter_queue_del(event_dev, eth_dev, rx_queue_id);
}

static int
cn9k_sso_tx_adapter_caps_get(const struct rte_eventdev *dev,
			     const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int ret;

	RTE_SET_USED(dev);
	ret = strncmp(eth_dev->device->driver->name, "net_cn9k", 8);
	if (ret)
		*caps = 0;
	else
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

static void
cn9k_sso_txq_fc_update(const struct rte_eth_dev *eth_dev, int32_t tx_queue_id,
		       bool ena)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cn9k_eth_txq *txq;
	struct roc_nix_sq *sq;
	int i;

	if (tx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
			cn9k_sso_txq_fc_update(eth_dev, i, ena);
	} else {
		uint16_t sq_limit;

		sq = &cnxk_eth_dev->sqs[tx_queue_id];
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sq_limit =
			ena ? RTE_MIN(CNXK_SSO_SQB_LIMIT, sq->aura_sqb_bufs) :
				    sq->nb_sqb_bufs;
		txq->nb_sqb_bufs_adj =
			sq_limit -
			RTE_ALIGN_MUL_CEIL(sq_limit,
					   (1ULL << txq->sqes_per_sqb_log2)) /
				(1ULL << txq->sqes_per_sqb_log2);
		txq->nb_sqb_bufs_adj = (70 * txq->nb_sqb_bufs_adj) / 100;
	}
}

static int
cn9k_sso_tx_adapter_queue_add(uint8_t id, const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	int rc;

	RTE_SET_USED(id);
	rc = cnxk_sso_tx_adapter_queue_add(event_dev, eth_dev, tx_queue_id);
	if (rc < 0)
		return rc;
	cn9k_sso_txq_fc_update(eth_dev, tx_queue_id, true);
	rc = cn9k_sso_updt_tx_adptr_data(event_dev);
	if (rc < 0)
		return rc;
	cn9k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	return 0;
}

static int
cn9k_sso_tx_adapter_queue_del(uint8_t id, const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	int rc;

	RTE_SET_USED(id);
	rc = cnxk_sso_tx_adapter_queue_del(event_dev, eth_dev, tx_queue_id);
	if (rc < 0)
		return rc;
	cn9k_sso_txq_fc_update(eth_dev, tx_queue_id, false);
	return cn9k_sso_updt_tx_adptr_data(event_dev);
}

static int
cn9k_crypto_adapter_caps_get(const struct rte_eventdev *event_dev,
			     const struct rte_cryptodev *cdev, uint32_t *caps)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn9k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn9k");

	*caps = RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA;

	return 0;
}

static int
cn9k_crypto_adapter_qp_add(const struct rte_eventdev *event_dev,
			   const struct rte_cryptodev *cdev,
			   int32_t queue_pair_id, const struct rte_event *event)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	RTE_SET_USED(event);

	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn9k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn9k");

	dev->is_ca_internal_port = 1;
	cn9k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	return cnxk_crypto_adapter_qp_add(event_dev, cdev, queue_pair_id);
}

static int
cn9k_crypto_adapter_qp_del(const struct rte_eventdev *event_dev,
			   const struct rte_cryptodev *cdev,
			   int32_t queue_pair_id)
{
	CNXK_VALID_DEV_OR_ERR_RET(event_dev->dev, "event_cn9k");
	CNXK_VALID_DEV_OR_ERR_RET(cdev->device, "crypto_cn9k");

	return cnxk_crypto_adapter_qp_del(cdev, queue_pair_id);
}

static struct rte_eventdev_ops cn9k_sso_dev_ops = {
	.dev_infos_get = cn9k_sso_info_get,
	.dev_configure = cn9k_sso_dev_configure,
	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cnxk_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn9k_sso_port_setup,
	.port_release = cn9k_sso_port_release,
	.port_link = cn9k_sso_port_link,
	.port_unlink = cn9k_sso_port_unlink,
	.timeout_ticks = cnxk_sso_timeout_ticks,

	.eth_rx_adapter_caps_get = cn9k_sso_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = cn9k_sso_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del = cn9k_sso_rx_adapter_queue_del,
	.eth_rx_adapter_start = cnxk_sso_rx_adapter_start,
	.eth_rx_adapter_stop = cnxk_sso_rx_adapter_stop,

	.eth_tx_adapter_caps_get = cn9k_sso_tx_adapter_caps_get,
	.eth_tx_adapter_queue_add = cn9k_sso_tx_adapter_queue_add,
	.eth_tx_adapter_queue_del = cn9k_sso_tx_adapter_queue_del,

	.timer_adapter_caps_get = cnxk_tim_caps_get,

	.crypto_adapter_caps_get = cn9k_crypto_adapter_caps_get,
	.crypto_adapter_queue_pair_add = cn9k_crypto_adapter_qp_add,
	.crypto_adapter_queue_pair_del = cn9k_crypto_adapter_qp_del,

	.dump = cnxk_sso_dump,
	.dev_start = cn9k_sso_start,
	.dev_stop = cn9k_sso_stop,
	.dev_close = cn9k_sso_close,
	.dev_selftest = cn9k_sso_selftest,
};

static int
cn9k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	if (RTE_CACHE_LINE_SIZE != 128) {
		plt_err("Driver not compiled for CN9K");
		return -EFAULT;
	}

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn9k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		cn9k_sso_fp_fns_set(event_dev);
		return 0;
	}

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn9k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
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
cn9k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(
		pci_drv, pci_dev, sizeof(struct cnxk_sso_evdev), cn9k_sso_init);
}

static const struct rte_pci_id cn9k_pci_sso_map[] = {
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn9k_pci_sso = {
	.id_table = cn9k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn9k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn9k, cn9k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn9k, cn9k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn9k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn9k, CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>"
			      CNXK_SSO_FORCE_BP "=1"
			      CN9K_SSO_SINGLE_WS "=1"
			      CNXK_TIM_DISABLE_NPA "=1"
			      CNXK_TIM_CHNK_SLOTS "=<int>"
			      CNXK_TIM_RINGS_LMT "=<int>"
			      CNXK_TIM_STATS_ENA "=1");
