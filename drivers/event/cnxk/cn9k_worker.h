/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_WORKER_H__
#define __CN9K_WORKER_H__

#include "cnxk_ethdev.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"
#include "cn9k_cryptodev_ops.h"

#include "cn9k_ethdev.h"
#include "cn9k_rx.h"
#include "cn9k_tx.h"

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

static __rte_always_inline void
cn9k_wqe_to_mbuf(uint64_t wqe, const uint64_t mbuf, uint8_t port_id,
		 const uint32_t tag, const uint32_t flags,
		 const void *const lookup_mem)
{
	const uint64_t mbuf_init = 0x100010000ULL | RTE_PKTMBUF_HEADROOM |
				   (flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);

	cn9k_nix_cqe_to_mbuf((struct nix_cqe_hdr_s *)wqe, tag,
			     (struct rte_mbuf *)mbuf, lookup_mem,
			     mbuf_init | ((uint64_t)port_id) << 48, flags);
}

static __rte_always_inline uint16_t
cn9k_sso_hws_dual_get_work(struct cn9k_sso_hws_state *ws,
			   struct cn9k_sso_hws_state *ws_pair,
			   struct rte_event *ev, const uint32_t flags,
			   const void *const lookup_mem,
			   struct cnxk_timesync_info *const tstamp)
{
	const uint64_t set_gw = BIT_ULL(16) | 1;
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;
	uint64_t tstamp_ptr;
	uint64_t mbuf;

	if (flags & NIX_RX_OFFLOAD_PTYPE_F)
		rte_prefetch_non_temporal(lookup_mem);
#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "rty%=:					\n"
		     "		ldr %[tag], [%[tag_loc]]	\n"
		     "		ldr %[wqp], [%[wqp_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=		\n"
		     "done%=:	str %[gw], [%[pong]]		\n"
		     "		dmb ld				\n"
		     "		sub %[mbuf], %[wqp], #0x80	\n"
		     "		prfm pldl1keep, [%[mbuf]]	\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1]),
		       [mbuf] "=&r"(mbuf)
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op),
		       [gw] "r"(set_gw), [pong] "r"(ws_pair->getwrk_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);
	gw.u64[1] = plt_read64(ws->wqp_op);
	plt_write64(set_gw, ws_pair->getwrk_op);
	mbuf = (uint64_t)((char *)gw.u64[1] - sizeof(struct rte_mbuf));
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	if (CNXK_TT_FROM_EVENT(gw.u64[0]) != SSO_TT_EMPTY) {
		if ((flags & CPT_RX_WQE_F) &&
		    (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
		     RTE_EVENT_TYPE_CRYPTODEV)) {
			gw.u64[1] = cn9k_cpt_crypto_adapter_dequeue(gw.u64[1]);
		} else if (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
			   RTE_EVENT_TYPE_ETHDEV) {
			uint8_t port = CNXK_SUB_EVENT_FROM_TAG(gw.u64[0]);

			gw.u64[0] = CNXK_CLR_SUB_EVENT(gw.u64[0]);
			cn9k_wqe_to_mbuf(gw.u64[1], mbuf, port,
					 gw.u64[0] & 0xFFFFF, flags,
					 lookup_mem);
			/* Extracting tstamp, if PTP enabled*/
			tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)
							    gw.u64[1]) +
						   CNXK_SSO_WQE_SG_PTR);
			cnxk_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf, tstamp,
						flags & NIX_RX_OFFLOAD_TSTAMP_F,
						flags & NIX_RX_MULTI_SEG_F,
						(uint64_t *)tstamp_ptr);
			gw.u64[1] = mbuf;
		}
	}

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

static __rte_always_inline uint16_t
cn9k_sso_hws_get_work(struct cn9k_sso_hws *ws, struct rte_event *ev,
		      const uint32_t flags, const void *const lookup_mem)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;
	uint64_t tstamp_ptr;
	uint64_t mbuf;

	plt_write64(BIT_ULL(16) | /* wait for work. */
			    1,	  /* Use Mask set 0. */
		    ws->getwrk_op);

	if (flags & NIX_RX_OFFLOAD_PTYPE_F)
		rte_prefetch_non_temporal(lookup_mem);
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
		     "		sub %[mbuf], %[wqp], #0x80	\n"
		     "		prfm pldl1keep, [%[mbuf]]	\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1]),
		       [mbuf] "=&r"(mbuf)
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);

	gw.u64[1] = plt_read64(ws->wqp_op);
	mbuf = (uint64_t)((char *)gw.u64[1] - sizeof(struct rte_mbuf));
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	if (CNXK_TT_FROM_EVENT(gw.u64[0]) != SSO_TT_EMPTY) {
		if ((flags & CPT_RX_WQE_F) &&
		    (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
		     RTE_EVENT_TYPE_CRYPTODEV)) {
			gw.u64[1] = cn9k_cpt_crypto_adapter_dequeue(gw.u64[1]);
		} else if (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
			   RTE_EVENT_TYPE_ETHDEV) {
			uint8_t port = CNXK_SUB_EVENT_FROM_TAG(gw.u64[0]);

			gw.u64[0] = CNXK_CLR_SUB_EVENT(gw.u64[0]);
			cn9k_wqe_to_mbuf(gw.u64[1], mbuf, port,
					 gw.u64[0] & 0xFFFFF, flags,
					 lookup_mem);
			/* Extracting tstamp, if PTP enabled*/
			tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)
							    gw.u64[1]) +
						   CNXK_SSO_WQE_SG_PTR);
			cnxk_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf,
						ws->tstamp,
						flags & NIX_RX_OFFLOAD_TSTAMP_F,
						flags & NIX_RX_MULTI_SEG_F,
						(uint64_t *)tstamp_ptr);
			gw.u64[1] = mbuf;
		}
	}

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
	uint64_t mbuf;

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
		     "		sub %[mbuf], %[wqp], #0x80	\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1]),
		       [mbuf] "=&r"(mbuf)
		     : [tag_loc] "r"(ws->tag_op), [wqp_loc] "r"(ws->wqp_op));
#else
	gw.u64[0] = plt_read64(ws->tag_op);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->tag_op);

	gw.u64[1] = plt_read64(ws->wqp_op);
	mbuf = (uint64_t)((char *)gw.u64[1] - sizeof(struct rte_mbuf));
#endif

	gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
		    (gw.u64[0] & (0x3FFull << 36)) << 4 |
		    (gw.u64[0] & 0xffffffff);

	if (CNXK_TT_FROM_EVENT(gw.u64[0]) != SSO_TT_EMPTY) {
		if (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
		    RTE_EVENT_TYPE_ETHDEV) {
			uint8_t port = CNXK_SUB_EVENT_FROM_TAG(gw.u64[0]);

			gw.u64[0] = CNXK_CLR_SUB_EVENT(gw.u64[0]);
			cn9k_wqe_to_mbuf(gw.u64[1], mbuf, port,
					 gw.u64[0] & 0xFFFFF, 0, NULL);
			gw.u64[1] = mbuf;
		}
	}

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
uint16_t __rte_hot cn9k_sso_hws_ca_enq(void *port, struct rte_event ev[],
				       uint16_t nb_events);
uint16_t __rte_hot cn9k_sso_hws_dual_ca_enq(void *port, struct rte_event ev[],
					    uint16_t nb_events);

#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	uint16_t __rte_hot cn9k_sso_hws_deq_##name(                            \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_burst_##name(                      \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_##name(                        \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_burst_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_deq_ca_##name(                         \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_ca_burst_##name(                   \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_deq_seg_##name(                        \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_seg_burst_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_seg_##name(                    \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_seg_burst_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_deq_ca_seg_##name(                     \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_deq_ca_seg_burst_##name(               \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);

NIX_RX_FASTPATH_MODES
#undef R

#define R(name, f5, f4, f3, f2, f1, f0, flags)                                 \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_##name(                       \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_burst_##name(                 \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_tmo_##name(                   \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_tmo_burst_##name(             \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_ca_##name(                    \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_ca_burst_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_seg_##name(                   \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_seg_burst_##name(             \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_tmo_seg_##name(               \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_tmo_seg_burst_##name(         \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_ca_seg_##name(                \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_ca_seg_burst_##name(          \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);

NIX_RX_FASTPATH_MODES
#undef R

static __rte_always_inline void
cn9k_sso_txq_fc_wait(const struct cn9k_eth_txq *txq)
{
	while (!((txq->nb_sqb_bufs_adj -
		  __atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED))
		 << (txq)->sqes_per_sqb_log2))
		;
}

static __rte_always_inline const struct cn9k_eth_txq *
cn9k_sso_hws_xtract_meta(struct rte_mbuf *m,
			 const uint64_t txq_data[][RTE_MAX_QUEUES_PER_PORT])
{
	return (const struct cn9k_eth_txq *)
		txq_data[m->port][rte_event_eth_tx_adapter_txq_get(m)];
}

static __rte_always_inline void
cn9k_sso_hws_prepare_pkt(const struct cn9k_eth_txq *txq, struct rte_mbuf *m,
			 uint64_t *cmd, const uint32_t flags)
{
	roc_lmt_mov(cmd, txq->cmd, cn9k_nix_tx_ext_subs(flags));
	cn9k_nix_xmit_prepare(m, cmd, flags, txq->lso_tun_fmt);
}

static __rte_always_inline uint16_t
cn9k_sso_hws_event_tx(uint64_t base, struct rte_event *ev, uint64_t *cmd,
		      const uint64_t txq_data[][RTE_MAX_QUEUES_PER_PORT],
		      const uint32_t flags)
{
	struct rte_mbuf *m = ev->mbuf;
	const struct cn9k_eth_txq *txq;
	uint16_t ref_cnt = m->refcnt;

	/* Perform header writes before barrier for TSO */
	cn9k_nix_xmit_prepare_tso(m, flags);
	/* Lets commit any changes in the packet here in case when
	 * fast free is set as no further changes will be made to mbuf.
	 * In case of fast free is not set, both cn9k_nix_prepare_mseg()
	 * and cn9k_nix_xmit_prepare() has a barrier after refcnt update.
	 */
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
		rte_io_wmb();
	txq = cn9k_sso_hws_xtract_meta(m, txq_data);
	cn9k_sso_hws_prepare_pkt(txq, m, cmd, flags);

	if (flags & NIX_TX_MULTI_SEG_F) {
		const uint16_t segdw = cn9k_nix_prepare_mseg(m, cmd, flags);
		if (!CNXK_TT_FROM_EVENT(ev->event)) {
			cn9k_nix_xmit_mseg_prep_lmt(cmd, txq->lmt_addr, segdw);
			roc_sso_hws_head_wait(base + SSOW_LF_GWS_TAG);
			cn9k_sso_txq_fc_wait(txq);
			if (cn9k_nix_xmit_submit_lmt(txq->io_addr) == 0)
				cn9k_nix_xmit_mseg_one(cmd, txq->lmt_addr,
						       txq->io_addr, segdw);
		} else {
			cn9k_nix_xmit_mseg_one(cmd, txq->lmt_addr, txq->io_addr,
					       segdw);
		}
	} else {
		if (!CNXK_TT_FROM_EVENT(ev->event)) {
			cn9k_nix_xmit_prep_lmt(cmd, txq->lmt_addr, flags);
			roc_sso_hws_head_wait(base + SSOW_LF_GWS_TAG);
			cn9k_sso_txq_fc_wait(txq);
			if (cn9k_nix_xmit_submit_lmt(txq->io_addr) == 0)
				cn9k_nix_xmit_one(cmd, txq->lmt_addr,
						  txq->io_addr, flags);
		} else {
			cn9k_nix_xmit_one(cmd, txq->lmt_addr, txq->io_addr,
					  flags);
		}
	}

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
		if (ref_cnt > 1)
			return 1;
	}

	cnxk_sso_hws_swtag_flush(base + SSOW_LF_GWS_TAG,
				 base + SSOW_LF_GWS_OP_SWTAG_FLUSH);

	return 1;
}

#define T(name, f5, f4, f3, f2, f1, f0, sz, flags)                             \
	uint16_t __rte_hot cn9k_sso_hws_tx_adptr_enq_##name(                   \
		void *port, struct rte_event ev[], uint16_t nb_events);        \
	uint16_t __rte_hot cn9k_sso_hws_tx_adptr_enq_seg_##name(               \
		void *port, struct rte_event ev[], uint16_t nb_events);        \
	uint16_t __rte_hot cn9k_sso_hws_dual_tx_adptr_enq_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events);        \
	uint16_t __rte_hot cn9k_sso_hws_dual_tx_adptr_enq_seg_##name(          \
		void *port, struct rte_event ev[], uint16_t nb_events);

NIX_TX_FASTPATH_MODES
#undef T

#endif
