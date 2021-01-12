/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_worker.h"

static __rte_noinline uint8_t
otx2_ssogws_new_event(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint64_t event_ptr = ev->u64;
	const uint16_t grp = ev->queue_id;

	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	otx2_ssogws_add_work(ws, event_ptr, tag, new_tt, grp);

	return 1;
}

static __rte_always_inline void
otx2_ssogws_fwd_swtag(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt = OTX2_SSOW_TT_FROM_TAG(otx2_read64(ws->tag_op));

	/* 96XX model
	 * cur_tt/new_tt     SSO_SYNC_ORDERED SSO_SYNC_ATOMIC SSO_SYNC_UNTAGGED
	 *
	 * SSO_SYNC_ORDERED        norm           norm             untag
	 * SSO_SYNC_ATOMIC         norm           norm		   untag
	 * SSO_SYNC_UNTAGGED       norm           norm             NOOP
	 */

	if (new_tt == SSO_SYNC_UNTAGGED) {
		if (cur_tt != SSO_SYNC_UNTAGGED)
			otx2_ssogws_swtag_untag(ws);
	} else {
		otx2_ssogws_swtag_norm(ws, tag, new_tt);
	}

	ws->swtag_req = 1;
}

static __rte_always_inline void
otx2_ssogws_fwd_group(struct otx2_ssogws *ws, const struct rte_event *ev,
		      const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	otx2_write64(ev->u64, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	rte_smp_wmb();
	otx2_ssogws_swtag_desched(ws, tag, new_tt, grp);
}

static __rte_always_inline void
otx2_ssogws_forward_event(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (OTX2_SSOW_GRP_FROM_TAG(otx2_read64(ws->tag_op)) == grp)
		otx2_ssogws_fwd_swtag(ws, ev);
	else
	/*
	 * Group has been changed for group based work pipelining,
	 * Use deschedule/add_work operation to transfer the event to
	 * new group/core
	 */
		otx2_ssogws_fwd_group(ws, ev, grp);
}

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
uint16_t __rte_hot								\
otx2_ssogws_deq_ ##name(void *port, struct rte_event *ev,		\
			uint64_t timeout_ticks)				\
{									\
	struct otx2_ssogws *ws = port;					\
									\
	RTE_SET_USED(timeout_ticks);					\
									\
	if (ws->swtag_req) {						\
		ws->swtag_req = 0;					\
		otx2_ssogws_swtag_wait(ws);				\
		return 1;						\
	}								\
									\
	return otx2_ssogws_get_work(ws, ev, flags, ws->lookup_mem);	\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_burst_ ##name(void *port, struct rte_event ev[],	\
			      uint16_t nb_events,			\
			      uint64_t timeout_ticks)			\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_deq_ ##name(port, ev, timeout_ticks);	\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_timeout_ ##name(void *port, struct rte_event *ev,	\
				uint64_t timeout_ticks)			\
{									\
	struct otx2_ssogws *ws = port;					\
	uint16_t ret = 1;						\
	uint64_t iter;							\
									\
	if (ws->swtag_req) {						\
		ws->swtag_req = 0;					\
		otx2_ssogws_swtag_wait(ws);				\
		return ret;						\
	}								\
									\
	ret = otx2_ssogws_get_work(ws, ev, flags, ws->lookup_mem);	\
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)	\
		ret = otx2_ssogws_get_work(ws, ev, flags,		\
					   ws->lookup_mem);		\
									\
	return ret;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_timeout_burst_ ##name(void *port, struct rte_event ev[],\
				      uint16_t nb_events,		\
				      uint64_t timeout_ticks)		\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_deq_timeout_ ##name(port, ev, timeout_ticks);\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_seg_ ##name(void *port, struct rte_event *ev,		\
			    uint64_t timeout_ticks)			\
{									\
	struct otx2_ssogws *ws = port;					\
									\
	RTE_SET_USED(timeout_ticks);					\
									\
	if (ws->swtag_req) {						\
		ws->swtag_req = 0;					\
		otx2_ssogws_swtag_wait(ws);				\
		return 1;						\
	}								\
									\
	return otx2_ssogws_get_work(ws, ev, flags | NIX_RX_MULTI_SEG_F,	\
				    ws->lookup_mem);			\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_seg_burst_ ##name(void *port, struct rte_event ev[],	\
				  uint16_t nb_events,			\
				  uint64_t timeout_ticks)		\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_deq_seg_ ##name(port, ev, timeout_ticks);	\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_seg_timeout_ ##name(void *port, struct rte_event *ev,	\
				    uint64_t timeout_ticks)		\
{									\
	struct otx2_ssogws *ws = port;					\
	uint16_t ret = 1;						\
	uint64_t iter;							\
									\
	if (ws->swtag_req) {						\
		ws->swtag_req = 0;					\
		otx2_ssogws_swtag_wait(ws);				\
		return ret;						\
	}								\
									\
	ret = otx2_ssogws_get_work(ws, ev, flags | NIX_RX_MULTI_SEG_F,	\
				   ws->lookup_mem);			\
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)	\
		ret = otx2_ssogws_get_work(ws, ev,			\
					   flags | NIX_RX_MULTI_SEG_F,	\
					   ws->lookup_mem);		\
									\
	return ret;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_deq_seg_timeout_burst_ ##name(void *port,			\
					  struct rte_event ev[],	\
					  uint16_t nb_events,		\
					  uint64_t timeout_ticks)	\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_deq_seg_timeout_ ##name(port, ev,		\
						   timeout_ticks);	\
}

SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R

uint16_t __rte_hot
otx2_ssogws_enq(void *port, const struct rte_event *ev)
{
	struct otx2_ssogws *ws = port;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		rte_smp_mb();
		return otx2_ssogws_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		otx2_ssogws_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		otx2_ssogws_swtag_flush(ws->tag_op, ws->swtag_flush_op);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __rte_hot
otx2_ssogws_enq_burst(void *port, const struct rte_event ev[],
		      uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return otx2_ssogws_enq(port, ev);
}

uint16_t __rte_hot
otx2_ssogws_enq_new_burst(void *port, const struct rte_event ev[],
			  uint16_t nb_events)
{
	struct otx2_ssogws *ws = port;
	uint16_t i, rc = 1;

	rte_smp_mb();
	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	for (i = 0; i < nb_events && rc; i++)
		rc = otx2_ssogws_new_event(ws,  &ev[i]);

	return nb_events;
}

uint16_t __rte_hot
otx2_ssogws_enq_fwd_burst(void *port, const struct rte_event ev[],
			  uint16_t nb_events)
{
	struct otx2_ssogws *ws = port;

	RTE_SET_USED(nb_events);
	otx2_ssogws_forward_event(ws,  ev);

	return 1;
}

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
uint16_t __rte_hot							\
otx2_ssogws_tx_adptr_enq_ ## name(void *port, struct rte_event ev[],	\
				  uint16_t nb_events)			\
{									\
	struct otx2_ssogws *ws = port;					\
	uint64_t cmd[sz];						\
									\
	RTE_SET_USED(nb_events);					\
	return otx2_ssogws_event_tx(ws->base, &ev[0], cmd,		\
				    (const uint64_t			\
				    (*)[RTE_MAX_QUEUES_PER_PORT])	\
				    &ws->tx_adptr_data,			\
				    flags);				\
}
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
uint16_t __rte_hot							\
otx2_ssogws_tx_adptr_enq_seg_ ## name(void *port, struct rte_event ev[],\
				      uint16_t nb_events)		\
{									\
	uint64_t cmd[(sz) + NIX_TX_MSEG_SG_DWORDS - 2];			\
	struct otx2_ssogws *ws = port;					\
									\
	RTE_SET_USED(nb_events);					\
	return otx2_ssogws_event_tx(ws->base, &ev[0], cmd,		\
				    (const uint64_t			\
				    (*)[RTE_MAX_QUEUES_PER_PORT])	\
				    &ws->tx_adptr_data,			\
				    (flags) | NIX_TX_MULTI_SEG_F);	\
}
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T

void
ssogws_flush_events(struct otx2_ssogws *ws, uint8_t queue_id, uintptr_t base,
		    otx2_handle_event_t fn, void *arg)
{
	uint64_t cq_ds_cnt = 1;
	uint64_t aq_cnt = 1;
	uint64_t ds_cnt = 1;
	struct rte_event ev;
	uint64_t enable;
	uint64_t val;

	enable = otx2_read64(base + SSO_LF_GGRP_QCTL);
	if (!enable)
		return;

	val  = queue_id;	/* GGRP ID */
	val |= BIT_ULL(18);	/* Grouped */
	val |= BIT_ULL(16);	/* WAIT */

	aq_cnt = otx2_read64(base + SSO_LF_GGRP_AQ_CNT);
	ds_cnt = otx2_read64(base + SSO_LF_GGRP_MISC_CNT);
	cq_ds_cnt = otx2_read64(base + SSO_LF_GGRP_INT_CNT);
	cq_ds_cnt &= 0x3FFF3FFF0000;

	while (aq_cnt || cq_ds_cnt || ds_cnt) {
		otx2_write64(val, ws->getwrk_op);
		otx2_ssogws_get_work_empty(ws, &ev, 0);
		if (fn != NULL && ev.u64 != 0)
			fn(arg, ev);
		if (ev.sched_type != SSO_TT_EMPTY)
			otx2_ssogws_swtag_flush(ws->tag_op, ws->swtag_flush_op);
		rte_mb();
		aq_cnt = otx2_read64(base + SSO_LF_GGRP_AQ_CNT);
		ds_cnt = otx2_read64(base + SSO_LF_GGRP_MISC_CNT);
		cq_ds_cnt = otx2_read64(base + SSO_LF_GGRP_INT_CNT);
		/* Extract cq and ds count */
		cq_ds_cnt &= 0x3FFF3FFF0000;
	}

	otx2_write64(0, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_GWC_INVAL);
	rte_mb();
}

void
ssogws_reset(struct otx2_ssogws *ws)
{
	uintptr_t base = OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op);
	uint64_t pend_state;
	uint8_t pend_tt;
	uint64_t tag;

	/* Wait till getwork/swtp/waitw/desched completes. */
	do {
		pend_state = otx2_read64(base + SSOW_LF_GWS_PENDSTATE);
		rte_mb();
	} while (pend_state & (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(58)));

	tag = otx2_read64(base + SSOW_LF_GWS_TAG);
	pend_tt = (tag >> 32) & 0x3;
	if (pend_tt != SSO_TT_EMPTY) { /* Work was pending */
		if (pend_tt == SSO_SYNC_ATOMIC || pend_tt == SSO_SYNC_ORDERED)
			otx2_ssogws_swtag_untag(ws);
		otx2_ssogws_desched(ws);
	}
	rte_mb();

	/* Wait for desched to complete. */
	do {
		pend_state = otx2_read64(base + SSOW_LF_GWS_PENDSTATE);
		rte_mb();
	} while (pend_state & BIT_ULL(58));
}
