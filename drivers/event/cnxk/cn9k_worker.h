/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_WORKER_H__
#define __CN9K_WORKER_H__

#include <rte_eventdev.h>
#include <rte_vect.h>

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

	cnxk_sso_hws_add_work(event_ptr, tag, new_tt,
			      ws->grp_base + (grp << 12));
	return 1;
}

static __rte_always_inline void
cn9k_sso_hws_fwd_swtag(uint64_t base, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt =
		CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_TAG));

	/* CNXK model
	 * cur_tt/new_tt     SSO_TT_ORDERED SSO_TT_ATOMIC SSO_TT_UNTAGGED
	 *
	 * SSO_TT_ORDERED        norm           norm             untag
	 * SSO_TT_ATOMIC         norm           norm		   untag
	 * SSO_TT_UNTAGGED       norm           norm             NOOP
	 */

	if (new_tt == SSO_TT_UNTAGGED) {
		if (cur_tt != SSO_TT_UNTAGGED)
			cnxk_sso_hws_swtag_untag(base +
						 SSOW_LF_GWS_OP_SWTAG_UNTAG);
	} else {
		cnxk_sso_hws_swtag_norm(tag, new_tt,
					base + SSOW_LF_GWS_OP_SWTAG_NORM);
	}
}

static __rte_always_inline void
cn9k_sso_hws_fwd_group(uint64_t base, const struct rte_event *ev,
		       const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	plt_write64(ev->u64, base + SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	cnxk_sso_hws_swtag_desched(tag, new_tt, grp,
				   base + SSOW_LF_GWS_OP_SWTAG_DESCHED);
}

static __rte_always_inline void
cn9k_sso_hws_forward_event(struct cn9k_sso_hws *ws, const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (CNXK_GRP_FROM_TAG(plt_read64(ws->base + SSOW_LF_GWS_TAG)) == grp) {
		cn9k_sso_hws_fwd_swtag(ws->base, ev);
		ws->swtag_req = 1;
	} else {
		/*
		 * Group has been changed for group based work pipelining,
		 * Use deschedule/add_work operation to transfer the event to
		 * new group/core
		 */
		cn9k_sso_hws_fwd_group(ws->base, ev, grp);
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

	cnxk_sso_hws_add_work(event_ptr, tag, new_tt,
			      dws->grp_base + (grp << 12));
	return 1;
}

static __rte_always_inline void
cn9k_sso_hws_dual_forward_event(struct cn9k_sso_hws_dual *dws, uint64_t base,
				const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (CNXK_GRP_FROM_TAG(plt_read64(base + SSOW_LF_GWS_TAG)) == grp) {
		cn9k_sso_hws_fwd_swtag(base, ev);
		dws->swtag_req = 1;
	} else {
		/*
		 * Group has been changed for group based work pipelining,
		 * Use deschedule/add_work operation to transfer the event to
		 * new group/core
		 */
		cn9k_sso_hws_fwd_group(base, ev, grp);
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
cn9k_sso_hws_dual_get_work(uint64_t base, uint64_t pair_base,
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
		     : [tag_loc] "r"(base + SSOW_LF_GWS_TAG),
		       [wqp_loc] "r"(base + SSOW_LF_GWS_WQP), [gw] "r"(set_gw),
		       [pong] "r"(pair_base + SSOW_LF_GWS_OP_GET_WORK0));
#else
	gw.u64[0] = plt_read64(base + SSOW_LF_GWS_TAG);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(base + SSOW_LF_GWS_TAG);
	gw.u64[1] = plt_read64(base + SSOW_LF_GWS_WQP);
	plt_write64(set_gw, pair_base + SSOW_LF_GWS_OP_GET_WORK0);
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
		    ws->base + SSOW_LF_GWS_OP_GET_WORK0);

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
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_TAG),
		       [wqp_loc] "r"(ws->base + SSOW_LF_GWS_WQP));
#else
	gw.u64[0] = plt_read64(ws->base + SSOW_LF_GWS_TAG);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(ws->base + SSOW_LF_GWS_TAG);

	gw.u64[1] = plt_read64(ws->base + SSOW_LF_GWS_WQP);
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
cn9k_sso_hws_get_work_empty(uint64_t base, struct rte_event *ev)
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
		     : [tag_loc] "r"(base + SSOW_LF_GWS_TAG),
		       [wqp_loc] "r"(base + SSOW_LF_GWS_WQP));
#else
	gw.u64[0] = plt_read64(base + SSOW_LF_GWS_TAG);
	while ((BIT_ULL(63)) & gw.u64[0])
		gw.u64[0] = plt_read64(base + SSOW_LF_GWS_TAG);

	gw.u64[1] = plt_read64(base + SSOW_LF_GWS_WQP);
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

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
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

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
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

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline void
cn9k_sso_hws_xmit_sec_one(const struct cn9k_eth_txq *txq, uint64_t base,
			  struct rte_mbuf *m, uint64_t *cmd,
			  uint32_t flags)
{
	struct cn9k_outb_priv_data *outb_priv;
	rte_iova_t io_addr = txq->cpt_io_addr;
	uint64_t *lmt_addr = txq->lmt_addr;
	struct cn9k_sec_sess_priv mdata;
	struct nix_send_hdr_s *send_hdr;
	uint64_t sa_base = txq->sa_base;
	uint32_t pkt_len, dlen_adj, rlen;
	uint64x2_t cmd01, cmd23;
	uint64_t lmt_status, sa;
	union nix_send_sg_s *sg;
	uintptr_t dptr, nixtx;
	uint64_t ucode_cmd[4];
	uint64_t esn, *iv;
	uint8_t l2_len;

	mdata.u64 = *rte_security_dynfield(m);
	send_hdr = (struct nix_send_hdr_s *)cmd;
	if (flags & NIX_TX_NEED_EXT_HDR)
		sg = (union nix_send_sg_s *)&cmd[4];
	else
		sg = (union nix_send_sg_s *)&cmd[2];

	if (flags & NIX_TX_NEED_SEND_HDR_W1)
		l2_len = cmd[1] & 0xFF;
	else
		l2_len = m->l2_len;

	/* Retrieve DPTR */
	dptr = *(uint64_t *)(sg + 1);
	pkt_len = send_hdr->w0.total;

	/* Calculate rlen */
	rlen = pkt_len - l2_len;
	rlen = (rlen + mdata.roundup_len) + (mdata.roundup_byte - 1);
	rlen &= ~(uint64_t)(mdata.roundup_byte - 1);
	rlen += mdata.partial_len;
	dlen_adj = rlen - pkt_len + l2_len;

	/* Update send descriptors. Security is single segment only */
	send_hdr->w0.total = pkt_len + dlen_adj;
	sg->seg1_size = pkt_len + dlen_adj;

	/* Get area where NIX descriptor needs to be stored */
	nixtx = dptr + pkt_len + dlen_adj;
	nixtx += BIT_ULL(7);
	nixtx = (nixtx - 1) & ~(BIT_ULL(7) - 1);

	roc_lmt_mov((void *)(nixtx + 16), cmd, cn9k_nix_tx_ext_subs(flags));

	/* Load opcode and cptr already prepared at pkt metadata set */
	pkt_len -= l2_len;
	pkt_len += sizeof(struct roc_onf_ipsec_outb_hdr) +
		    ROC_ONF_IPSEC_OUTB_MAX_L2_INFO_SZ;
	sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);

	sa = (uintptr_t)roc_nix_inl_onf_ipsec_outb_sa(sa_base, mdata.sa_idx);
	ucode_cmd[3] = (ROC_CPT_DFLT_ENG_GRP_SE_IE << 61 | sa);
	ucode_cmd[0] = (ROC_IE_ONF_MAJOR_OP_PROCESS_OUTBOUND_IPSEC << 48 |
			0x40UL << 48 | pkt_len);

	/* CPT Word 0 and Word 1 */
	cmd01 = vdupq_n_u64((nixtx + 16) | (cn9k_nix_tx_ext_subs(flags) + 1));
	/* CPT_RES_S is 16B above NIXTX */
	cmd01 = vsetq_lane_u8(nixtx & BIT_ULL(7), cmd01, 8);

	/* CPT word 2 and 3 */
	cmd23 = vdupq_n_u64(0);
	cmd23 = vsetq_lane_u64((((uint64_t)RTE_EVENT_TYPE_CPU << 28) |
				CNXK_ETHDEV_SEC_OUTB_EV_SUB << 20), cmd23, 0);
	cmd23 = vsetq_lane_u64((uintptr_t)m | 1, cmd23, 1);

	dptr += l2_len - ROC_ONF_IPSEC_OUTB_MAX_L2_INFO_SZ -
		sizeof(struct roc_onf_ipsec_outb_hdr);
	ucode_cmd[1] = dptr;
	ucode_cmd[2] = dptr;

	/* Update IV to zero and l2 sz */
	*(uint16_t *)(dptr + sizeof(struct roc_onf_ipsec_outb_hdr)) =
		rte_cpu_to_be_16(ROC_ONF_IPSEC_OUTB_MAX_L2_INFO_SZ);
	iv = (uint64_t *)(dptr + 8);
	iv[0] = 0;
	iv[1] = 0;

	/* Head wait if needed */
	if (base)
		roc_sso_hws_head_wait(base + SSOW_LF_GWS_TAG);

	/* ESN */
	outb_priv = roc_nix_inl_onf_ipsec_outb_sa_sw_rsvd((void *)sa);
	esn = outb_priv->esn;
	outb_priv->esn = esn + 1;

	ucode_cmd[0] |= (esn >> 32) << 16;
	esn = rte_cpu_to_be_32(esn & (BIT_ULL(32) - 1));

	/* Update ESN and IPID and IV */
	*(uint64_t *)dptr = esn << 32 | esn;

	rte_io_wmb();
	cn9k_sso_txq_fc_wait(txq);

	/* Write CPT instruction to lmt line */
	vst1q_u64(lmt_addr, cmd01);
	vst1q_u64(lmt_addr + 2, cmd23);

	roc_lmt_mov_seg(lmt_addr + 4, ucode_cmd, 2);

	if (roc_lmt_submit_ldeor(io_addr) == 0) {
		do {
			vst1q_u64(lmt_addr, cmd01);
			vst1q_u64(lmt_addr + 2, cmd23);
			roc_lmt_mov_seg(lmt_addr + 4, ucode_cmd, 2);

			lmt_status = roc_lmt_submit_ldeor(io_addr);
		} while (lmt_status == 0);
	}
}
#else

static inline void
cn9k_sso_hws_xmit_sec_one(const struct cn9k_eth_txq *txq, uint64_t base,
			  struct rte_mbuf *m, uint64_t *cmd,
			  uint32_t flags)
{
	RTE_SET_USED(txq);
	RTE_SET_USED(base);
	RTE_SET_USED(m);
	RTE_SET_USED(cmd);
	RTE_SET_USED(flags);
}
#endif

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
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) &&
	    !(flags & NIX_TX_OFFLOAD_SECURITY_F))
		rte_io_wmb();
	txq = cn9k_sso_hws_xtract_meta(m, txq_data);
	cn9k_sso_hws_prepare_pkt(txq, m, cmd, flags);

	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		uint64_t ol_flags = m->ol_flags;

		if (ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			uintptr_t ssow_base = base;

			if (ev->sched_type)
				ssow_base = 0;

			cn9k_sso_hws_xmit_sec_one(txq, ssow_base, m, cmd,
						  flags);
			goto done;
		}

		if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
			rte_io_wmb();
	}

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

done:
	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
		if (ref_cnt > 1)
			return 1;
	}

	cnxk_sso_hws_swtag_flush(base + SSOW_LF_GWS_TAG,
				 base + SSOW_LF_GWS_OP_SWTAG_FLUSH);

	return 1;
}

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
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
