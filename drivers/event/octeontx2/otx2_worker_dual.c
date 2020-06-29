/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_worker_dual.h"
#include "otx2_worker.h"

static __rte_noinline uint8_t
otx2_ssogws_dual_new_event(struct otx2_ssogws_dual *ws,
			   const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint64_t event_ptr = ev->u64;
	const uint16_t grp = ev->queue_id;

	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	otx2_ssogws_dual_add_work(ws, event_ptr, tag, new_tt, grp);

	return 1;
}

static __rte_always_inline void
otx2_ssogws_dual_fwd_swtag(struct otx2_ssogws_state *ws,
			   const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt = ws->cur_tt;

	/* 96XX model
	 * cur_tt/new_tt     SSO_SYNC_ORDERED SSO_SYNC_ATOMIC SSO_SYNC_UNTAGGED
	 *
	 * SSO_SYNC_ORDERED        norm           norm             untag
	 * SSO_SYNC_ATOMIC         norm           norm		   untag
	 * SSO_SYNC_UNTAGGED       norm           norm             NOOP
	 */
	if (new_tt == SSO_SYNC_UNTAGGED) {
		if (cur_tt != SSO_SYNC_UNTAGGED)
			otx2_ssogws_swtag_untag((struct otx2_ssogws *)ws);
	} else {
		otx2_ssogws_swtag_norm((struct otx2_ssogws *)ws, tag, new_tt);
	}
}

static __rte_always_inline void
otx2_ssogws_dual_fwd_group(struct otx2_ssogws_state *ws,
			   const struct rte_event *ev, const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	otx2_write64(ev->u64, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	rte_smp_wmb();
	otx2_ssogws_swtag_desched((struct otx2_ssogws *)ws, tag, new_tt, grp);
}

static __rte_always_inline void
otx2_ssogws_dual_forward_event(struct otx2_ssogws_dual *ws,
			       struct otx2_ssogws_state *vws,
			       const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (vws->cur_grp == grp) {
		otx2_ssogws_dual_fwd_swtag(vws, ev);
		ws->swtag_req = 1;
	} else {
	/*
	 * Group has been changed for group based work pipelining,
	 * Use deschedule/add_work operation to transfer the event to
	 * new group/core
	 */
		otx2_ssogws_dual_fwd_group(vws, ev, grp);
	}
}

uint16_t __rte_hot
otx2_ssogws_dual_enq(void *port, const struct rte_event *ev)
{
	struct otx2_ssogws_dual *ws = port;
	struct otx2_ssogws_state *vws = &ws->ws_state[!ws->vws];

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		rte_smp_mb();
		return otx2_ssogws_dual_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		otx2_ssogws_dual_forward_event(ws, vws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		otx2_ssogws_swtag_flush((struct otx2_ssogws *)vws);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __rte_hot
otx2_ssogws_dual_enq_burst(void *port, const struct rte_event ev[],
			   uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return otx2_ssogws_dual_enq(port, ev);
}

uint16_t __rte_hot
otx2_ssogws_dual_enq_new_burst(void *port, const struct rte_event ev[],
			       uint16_t nb_events)
{
	struct otx2_ssogws_dual *ws = port;
	uint16_t i, rc = 1;

	rte_smp_mb();
	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	for (i = 0; i < nb_events && rc; i++)
		rc = otx2_ssogws_dual_new_event(ws, &ev[i]);

	return nb_events;
}

uint16_t __rte_hot
otx2_ssogws_dual_enq_fwd_burst(void *port, const struct rte_event ev[],
			       uint16_t nb_events)
{
	struct otx2_ssogws_dual *ws = port;
	struct otx2_ssogws_state *vws = &ws->ws_state[!ws->vws];

	RTE_SET_USED(nb_events);
	otx2_ssogws_dual_forward_event(ws, vws, ev);

	return 1;
}

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_ ##name(void *port, struct rte_event *ev,		\
			     uint64_t timeout_ticks)			\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	uint8_t gw;							\
									\
	rte_prefetch_non_temporal(ws);					\
	RTE_SET_USED(timeout_ticks);					\
	if (ws->swtag_req) {						\
		otx2_ssogws_swtag_wait((struct otx2_ssogws *)		\
				       &ws->ws_state[!ws->vws]);	\
		ws->swtag_req = 0;					\
		return 1;						\
	}								\
									\
	gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],		\
				       &ws->ws_state[!ws->vws], ev,	\
				       flags, ws->lookup_mem,		\
				       ws->tstamp);			\
	ws->vws = !ws->vws;						\
									\
	return gw;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_burst_ ##name(void *port, struct rte_event ev[],	\
				   uint16_t nb_events,			\
				   uint64_t timeout_ticks)		\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_dual_deq_ ##name(port, ev, timeout_ticks);	\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_timeout_ ##name(void *port, struct rte_event *ev,	\
				     uint64_t timeout_ticks)		\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	uint64_t iter;							\
	uint8_t gw;							\
									\
	if (ws->swtag_req) {						\
		otx2_ssogws_swtag_wait((struct otx2_ssogws *)		\
				       &ws->ws_state[!ws->vws]);	\
		ws->swtag_req = 0;					\
		return 1;						\
	}								\
									\
	gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],		\
				       &ws->ws_state[!ws->vws], ev,	\
				       flags, ws->lookup_mem,		\
				       ws->tstamp);			\
	ws->vws = !ws->vws;						\
	for (iter = 1; iter < timeout_ticks && (gw == 0); iter++) {	\
		gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],	\
					       &ws->ws_state[!ws->vws],	\
					       ev, flags,		\
					       ws->lookup_mem,		\
					       ws->tstamp);		\
		ws->vws = !ws->vws;					\
	}								\
									\
	return gw;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_timeout_burst_ ##name(void *port,			\
					   struct rte_event ev[],	\
					   uint16_t nb_events,		\
					   uint64_t timeout_ticks)	\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_dual_deq_timeout_ ##name(port, ev,		\
						    timeout_ticks);	\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_seg_ ##name(void *port, struct rte_event *ev,	\
				 uint64_t timeout_ticks)		\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	uint8_t gw;							\
									\
	RTE_SET_USED(timeout_ticks);					\
	if (ws->swtag_req) {						\
		otx2_ssogws_swtag_wait((struct otx2_ssogws *)		\
				       &ws->ws_state[!ws->vws]);	\
		ws->swtag_req = 0;					\
		return 1;						\
	}								\
									\
	gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],		\
				       &ws->ws_state[!ws->vws], ev,	\
				       flags | NIX_RX_MULTI_SEG_F,	\
				       ws->lookup_mem,			\
				       ws->tstamp);			\
	ws->vws = !ws->vws;						\
									\
	return gw;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_seg_burst_ ##name(void *port,			\
				       struct rte_event ev[],		\
				       uint16_t nb_events,		\
				       uint64_t timeout_ticks)		\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_dual_deq_seg_ ##name(port, ev,		\
						timeout_ticks);		\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_seg_timeout_ ##name(void *port,			\
					 struct rte_event *ev,		\
					 uint64_t timeout_ticks)	\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	uint64_t iter;							\
	uint8_t gw;							\
									\
	if (ws->swtag_req) {						\
		otx2_ssogws_swtag_wait((struct otx2_ssogws *)		\
				       &ws->ws_state[!ws->vws]);	\
		ws->swtag_req = 0;					\
		return 1;						\
	}								\
									\
	gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],		\
				       &ws->ws_state[!ws->vws], ev,	\
				       flags | NIX_RX_MULTI_SEG_F,	\
				       ws->lookup_mem,			\
				       ws->tstamp);			\
	ws->vws = !ws->vws;						\
	for (iter = 1; iter < timeout_ticks && (gw == 0); iter++) {	\
		gw = otx2_ssogws_dual_get_work(&ws->ws_state[ws->vws],	\
					       &ws->ws_state[!ws->vws],	\
					       ev, flags |		\
					       NIX_RX_MULTI_SEG_F,	\
					       ws->lookup_mem,		\
					       ws->tstamp);		\
		ws->vws = !ws->vws;					\
	}								\
									\
	return gw;							\
}									\
									\
uint16_t __rte_hot								\
otx2_ssogws_dual_deq_seg_timeout_burst_ ##name(void *port,		\
					       struct rte_event ev[],	\
					       uint16_t nb_events,	\
					       uint64_t timeout_ticks)	\
{									\
	RTE_SET_USED(nb_events);					\
									\
	return otx2_ssogws_dual_deq_seg_timeout_ ##name(port, ev,	\
							timeout_ticks);	\
}

SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
uint16_t __rte_hot							\
otx2_ssogws_dual_tx_adptr_enq_ ## name(void *port,			\
				       struct rte_event ev[],		\
				       uint16_t nb_events)		\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	struct otx2_ssogws *vws =					\
		(struct otx2_ssogws *)&ws->ws_state[!ws->vws];		\
	uint64_t cmd[sz];						\
									\
	RTE_SET_USED(nb_events);					\
	return otx2_ssogws_event_tx(vws, ev, cmd, (const uint64_t	\
				    (*)[RTE_MAX_QUEUES_PER_PORT])	\
				    ws->tx_adptr_data,			\
				    flags);				\
}
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
uint16_t __rte_hot							\
otx2_ssogws_dual_tx_adptr_enq_seg_ ## name(void *port,			\
					   struct rte_event ev[],	\
					   uint16_t nb_events)		\
{									\
	struct otx2_ssogws_dual *ws = port;				\
	struct otx2_ssogws *vws =					\
		(struct otx2_ssogws *)&ws->ws_state[!ws->vws];		\
	uint64_t cmd[(sz) + NIX_TX_MSEG_SG_DWORDS - 2];			\
									\
	RTE_SET_USED(nb_events);					\
	return otx2_ssogws_event_tx(vws, ev, cmd, (const uint64_t	\
				    (*)[RTE_MAX_QUEUES_PER_PORT])	\
				    ws->tx_adptr_data,			\
				    (flags) | NIX_TX_MULTI_SEG_F);	\
}
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T
