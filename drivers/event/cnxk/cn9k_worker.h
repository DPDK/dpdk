/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_WORKER_H__
#define __CN9K_WORKER_H__

#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

/* SSO Operations */

static __rte_always_inline uint8_t
cn9k_sso_hws_new_event(struct cn9k_sso_hws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint64_t event_ptr = ev->u64;
	const uint16_t grp = ev->queue_id;

	rte_atomic_thread_fence(__ATOMIC_ACQ_REL);
	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	cnxk_sso_hws_add_work(event_ptr, tag, new_tt, ws->grps_base[grp]);
	return 1;
}

static __rte_always_inline void
cn9k_sso_hws_fwd_swtag(struct cn9k_sso_hws_state *vws,
		       const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt = CNXK_TT_FROM_TAG(plt_read64(vws->tag_op));

	/* CNXK model
	 * cur_tt/new_tt     SSO_TT_ORDERED SSO_TT_ATOMIC SSO_TT_UNTAGGED
	 *
	 * SSO_TT_ORDERED        norm           norm             untag
	 * SSO_TT_ATOMIC         norm           norm		   untag
	 * SSO_TT_UNTAGGED       norm           norm             NOOP
	 */

	if (new_tt == SSO_TT_UNTAGGED) {
		if (cur_tt != SSO_TT_UNTAGGED)
			cnxk_sso_hws_swtag_untag(
				CN9K_SSOW_GET_BASE_ADDR(vws->getwrk_op) +
				SSOW_LF_GWS_OP_SWTAG_UNTAG);
	} else {
		cnxk_sso_hws_swtag_norm(tag, new_tt, vws->swtag_norm_op);
	}
}

static __rte_always_inline void
cn9k_sso_hws_fwd_group(struct cn9k_sso_hws_state *ws,
		       const struct rte_event *ev, const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	plt_write64(ev->u64, CN9K_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
				     SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	cnxk_sso_hws_swtag_desched(tag, new_tt, grp, ws->swtag_desched_op);
}

static __rte_always_inline void
cn9k_sso_hws_forward_event(struct cn9k_sso_hws *ws, const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (CNXK_GRP_FROM_TAG(plt_read64(ws->tag_op)) == grp) {
		cn9k_sso_hws_fwd_swtag((struct cn9k_sso_hws_state *)ws, ev);
		ws->swtag_req = 1;
	} else {
		/*
		 * Group has been changed for group based work pipelining,
		 * Use deschedule/add_work operation to transfer the event to
		 * new group/core
		 */
		cn9k_sso_hws_fwd_group((struct cn9k_sso_hws_state *)ws, ev,
				       grp);
	}
}

/* Dual ws ops. */

static __rte_always_inline uint8_t
cn9k_sso_hws_dual_new_event(struct cn9k_sso_hws_dual *dws,
			    const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint64_t event_ptr = ev->u64;
	const uint16_t grp = ev->queue_id;

	rte_atomic_thread_fence(__ATOMIC_ACQ_REL);
	if (dws->xaq_lmt <= *dws->fc_mem)
		return 0;

	cnxk_sso_hws_add_work(event_ptr, tag, new_tt, dws->grps_base[grp]);
	return 1;
}

static __rte_always_inline void
cn9k_sso_hws_dual_forward_event(struct cn9k_sso_hws_dual *dws,
				struct cn9k_sso_hws_state *vws,
				const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (CNXK_GRP_FROM_TAG(plt_read64(vws->tag_op)) == grp) {
		cn9k_sso_hws_fwd_swtag(vws, ev);
		dws->swtag_req = 1;
	} else {
		/*
		 * Group has been changed for group based work pipelining,
		 * Use deschedule/add_work operation to transfer the event to
		 * new group/core
		 */
		cn9k_sso_hws_fwd_group(vws, ev, grp);
	}
}

static __rte_always_inline uint16_t
cn9k_sso_hws_dual_get_work(struct cn9k_sso_hws_state *ws,
			   struct cn9k_sso_hws_state *ws_pair,
			   struct rte_event *ev)
{
	const uint64_t set_gw = BIT_ULL(16) | 1;
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "rty%=:					\n"
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=		\n"
		     "done%=:	str %[gw], [%[pong]]		\n"
		     "		dmb ld				\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1])
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op),
		       [gw] "r"(set_gw), [pong] "r"(ws_pair->getwrk_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);
	gw.u64[1] = plt_read64(ws->wqp_op);
	plt_write64(set_gw, ws_pair->getwrk_op);
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

static __rte_always_inline uint16_t
cn9k_sso_hws_get_work(struct cn9k_sso_hws *ws, struct rte_event *ev)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

	plt_write64(BIT_ULL(16) | /* wait for work. */
			    1,	  /* Use Mask set 0. */
		    ws->getwrk_op);
#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbz %[tag], 63, done%=		\n"
		     "		sevl				\n"
		     "rty%=:	wfe				\n"
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=		\n"
		     "done%=:	dmb ld				\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1])
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);

	gw.u64[1] = plt_read64(ws->wqp_op);
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* Used in cleaning up workslot. */
static __rte_always_inline uint16_t
cn9k_sso_hws_get_work_empty(struct cn9k_sso_hws_state *ws, struct rte_event *ev)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbz %[tag], 63, done%=		\n"
		     "		sevl				\n"
		     "rty%=:	wfe				\n"
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=		\n"
		     "done%=:	dmb ld				\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1])
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);

	gw.u64[1] = plt_read64(ws->wqp_op);
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* CN9K Fastpath functions. */
uint16_t __rte_hot cn9k_sso_hws_enq(void *port, const struct rte_event *ev);
uint16_t __rte_hot cn9k_sso_hws_enq_burst(void *port,
					  const struct rte_event ev[],
					  uint16_t nb_events);
uint16_t __rte_hot cn9k_sso_hws_enq_new_burst(void *port,
					      const struct rte_event ev[],
					      uint16_t nb_events);
uint16_t __rte_hot cn9k_sso_hws_enq_fwd_burst(void *port,
					      const struct rte_event ev[],
					      uint16_t nb_events);

uint16_t __rte_hot cn9k_sso_hws_dual_enq(void *port,
					 const struct rte_event *ev);
uint16_t __rte_hot cn9k_sso_hws_dual_enq_burst(void *port,
					       const struct rte_event ev[],
					       uint16_t nb_events);
uint16_t __rte_hot cn9k_sso_hws_dual_enq_new_burst(void *port,
						   const struct rte_event ev[],
						   uint16_t nb_events);
uint16_t __rte_hot cn9k_sso_hws_dual_enq_fwd_burst(void *port,
						   const struct rte_event ev[],
						   uint16_t nb_events);

uint16_t __rte_hot cn9k_sso_hws_deq(void *port, struct rte_event *ev,
				    uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_deq_burst(void *port, struct rte_event ev[],
					  uint16_t nb_events,
					  uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_tmo_deq(void *port, struct rte_event *ev,
					uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_tmo_deq_burst(void *port, struct rte_event ev[],
					      uint16_t nb_events,
					      uint64_t timeout_ticks);

uint16_t __rte_hot cn9k_sso_hws_dual_deq(void *port, struct rte_event *ev,
					 uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_dual_deq_burst(void *port,
					       struct rte_event ev[],
					       uint16_t nb_events,
					       uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_dual_tmo_deq(void *port, struct rte_event *ev,
					     uint64_t timeout_ticks);
uint16_t __rte_hot cn9k_sso_hws_dual_tmo_deq_burst(void *port,
						   struct rte_event ev[],
						   uint16_t nb_events,
						   uint64_t timeout_ticks);

#endif
