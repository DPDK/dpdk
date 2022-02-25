/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_WORKER_H__
#define __CN10K_WORKER_H__

#include <rte_vect.h>

#include "cn10k_cryptodev_ops.h"
#include "cnxk_ethdev.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#include "cn10k_ethdev.h"
#include "cn10k_rx.h"
#include "cn10k_tx.h"

/* SSO Operations */

static __rte_always_inline uint8_t
cn10k_sso_hws_new_event(struct cn10k_sso_hws *ws, const struct rte_event *ev)
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
cn10k_sso_hws_fwd_swtag(struct cn10k_sso_hws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt = CNXK_TT_FROM_TAG(ws->gw_rdata);

	/* CNXK model
	 * cur_tt/new_tt     SSO_TT_ORDERED SSO_TT_ATOMIC SSO_TT_UNTAGGED
	 *
	 * SSO_TT_ORDERED        norm           norm             untag
	 * SSO_TT_ATOMIC         norm           norm		   untag
	 * SSO_TT_UNTAGGED       norm           norm             NOOP
	 */

	if (new_tt == SSO_TT_UNTAGGED) {
		if (cur_tt != SSO_TT_UNTAGGED)
			cnxk_sso_hws_swtag_untag(ws->base +
						 SSOW_LF_GWS_OP_SWTAG_UNTAG);
	} else {
		cnxk_sso_hws_swtag_norm(tag, new_tt,
					ws->base + SSOW_LF_GWS_OP_SWTAG_NORM);
	}
	ws->swtag_req = 1;
}

static __rte_always_inline void
cn10k_sso_hws_fwd_group(struct cn10k_sso_hws *ws, const struct rte_event *ev,
			const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	plt_write64(ev->u64, ws->base + SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	cnxk_sso_hws_swtag_desched(tag, new_tt, grp,
				   ws->base + SSOW_LF_GWS_OP_SWTAG_DESCHED);
}

static __rte_always_inline void
cn10k_sso_hws_forward_event(struct cn10k_sso_hws *ws,
			    const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (CNXK_GRP_FROM_TAG(ws->gw_rdata) == grp)
		cn10k_sso_hws_fwd_swtag(ws, ev);
	else
		/*
		 * Group has been changed for group based work pipelining,
		 * Use deschedule/add_work operation to transfer the event to
		 * new group/core
		 */
		cn10k_sso_hws_fwd_group(ws, ev, grp);
}

static __rte_always_inline void
cn10k_wqe_to_mbuf(uint64_t wqe, const uint64_t __mbuf, uint8_t port_id,
		  const uint32_t tag, const uint32_t flags,
		  const void *const lookup_mem)
{
	const uint64_t mbuf_init = 0x100010000ULL | RTE_PKTMBUF_HEADROOM |
				   (flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);
	struct rte_mbuf *mbuf = (struct rte_mbuf *)__mbuf;

	/* Mark mempool obj as "get" as it is alloc'ed by NIX */
	RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

	cn10k_nix_cqe_to_mbuf((struct nix_cqe_hdr_s *)wqe, tag,
			      (struct rte_mbuf *)mbuf, lookup_mem,
			      mbuf_init | ((uint64_t)port_id) << 48, flags);
}

static __rte_always_inline void
cn10k_process_vwqe(uintptr_t vwqe, uint16_t port_id, const uint32_t flags,
		   void *lookup_mem, void *tstamp, uintptr_t lbase)
{
	uint64_t mbuf_init = 0x100010000ULL | RTE_PKTMBUF_HEADROOM |
			     (flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);
	struct rte_event_vector *vec;
	uint64_t aura_handle, laddr;
	uint16_t nb_mbufs, non_vec;
	uint16_t lmt_id, d_off;
	struct rte_mbuf **wqe;
	struct rte_mbuf *mbuf;
	uint8_t loff = 0;
	uint64_t sa_base;
	int i;

	mbuf_init |= ((uint64_t)port_id) << 48;
	vec = (struct rte_event_vector *)vwqe;
	wqe = vec->mbufs;

	rte_prefetch0(&vec->ptrs[0]);
#define OBJS_PER_CLINE (RTE_CACHE_LINE_SIZE / sizeof(void *))
	for (i = OBJS_PER_CLINE; i < vec->nb_elem; i += OBJS_PER_CLINE)
		rte_prefetch0(&vec->ptrs[i]);

	nb_mbufs = RTE_ALIGN_FLOOR(vec->nb_elem, NIX_DESCS_PER_LOOP);
	nb_mbufs = cn10k_nix_recv_pkts_vector(&mbuf_init, wqe, nb_mbufs,
					      flags | NIX_RX_VWQE_F, lookup_mem,
					      tstamp, lbase);
	wqe += nb_mbufs;
	non_vec = vec->nb_elem - nb_mbufs;

	if (flags & NIX_RX_OFFLOAD_SECURITY_F && non_vec) {
		mbuf = (struct rte_mbuf *)((uintptr_t)wqe[0] -
					   sizeof(struct rte_mbuf));
		/* Pick first mbuf's aura handle assuming all
		 * mbufs are from a vec and are from same RQ.
		 */
		aura_handle = mbuf->pool->pool_id;
		ROC_LMT_BASE_ID_GET(lbase, lmt_id);
		laddr = lbase;
		laddr += 8;
		d_off = ((uintptr_t)mbuf->buf_addr - (uintptr_t)mbuf);
		d_off += (mbuf_init & 0xFFFF);
		sa_base = cnxk_nix_sa_base_get(mbuf_init >> 48, lookup_mem);
		sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);
	}

	while (non_vec) {
		struct nix_cqe_hdr_s *cqe = (struct nix_cqe_hdr_s *)wqe[0];
		uint64_t tstamp_ptr;

		mbuf = (struct rte_mbuf *)((char *)cqe -
					   sizeof(struct rte_mbuf));

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		/* Translate meta to mbuf */
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			const uint64_t cq_w1 = *((const uint64_t *)cqe + 1);
			const uint64_t cq_w5 = *((const uint64_t *)cqe + 5);

			mbuf = nix_sec_meta_to_mbuf_sc(cq_w1, cq_w5, sa_base, laddr,
						       &loff, mbuf, d_off,
						       flags, mbuf_init);
		}

		cn10k_nix_cqe_to_mbuf(cqe, cqe->tag, mbuf, lookup_mem,
				      mbuf_init, flags);
		/* Extracting tstamp, if PTP enabled*/
		tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)cqe) +
					   CNXK_SSO_WQE_SG_PTR);
		cn10k_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf, tstamp,
					flags & NIX_RX_OFFLOAD_TSTAMP_F,
					(uint64_t *)tstamp_ptr);
		wqe[0] = (struct rte_mbuf *)mbuf;
		non_vec--;
		wqe++;
	}

	/* Free remaining meta buffers if any */
	if (flags & NIX_RX_OFFLOAD_SECURITY_F && loff) {
		nix_sec_flush_meta(laddr, lmt_id, loff, aura_handle);
		plt_io_wmb();
	}
}

static __rte_always_inline uint16_t
cn10k_sso_hws_get_work(struct cn10k_sso_hws *ws, struct rte_event *ev,
		       const uint32_t flags, void *lookup_mem)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;
	uint64_t tstamp_ptr;

	gw.get_work = ws->gw_wdata;
#if defined(RTE_ARCH_ARM64) && !defined(__clang__)
	asm volatile(
		PLT_CPU_FEATURE_PREAMBLE
		"caspal %[wdata], %H[wdata], %[wdata], %H[wdata], [%[gw_loc]]\n"
		: [wdata] "+r"(gw.get_work)
		: [gw_loc] "r"(ws->base + SSOW_LF_GWS_OP_GET_WORK0)
		: "memory");
#else
	plt_write64(gw.u64[0], ws->base + SSOW_LF_GWS_OP_GET_WORK0);
	do {
		roc_load_pair(gw.u64[0], gw.u64[1],
			      ws->base + SSOW_LF_GWS_WQE0);
	} while (gw.u64[0] & BIT_ULL(63));
#endif
	ws->gw_rdata = gw.u64[0];
	if (gw.u64[1]) {
		gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
			    (gw.u64[0] & (0x3FFull << 36)) << 4 |
			    (gw.u64[0] & 0xffffffff);
		if ((flags & CPT_RX_WQE_F) &&
		    (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
		     RTE_EVENT_TYPE_CRYPTODEV)) {
			gw.u64[1] = cn10k_cpt_crypto_adapter_dequeue(gw.u64[1]);
		} else if (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
			   RTE_EVENT_TYPE_ETHDEV) {
			uint8_t port = CNXK_SUB_EVENT_FROM_TAG(gw.u64[0]);
			uint64_t mbuf;

			mbuf = gw.u64[1] - sizeof(struct rte_mbuf);
			rte_prefetch0((void *)mbuf);
			if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
				const uint64_t mbuf_init = 0x100010000ULL |
					RTE_PKTMBUF_HEADROOM |
					(flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);
				struct rte_mbuf *m;
				uintptr_t sa_base;
				uint64_t iova = 0;
				uint8_t loff = 0;
				uint16_t d_off;
				uint64_t cq_w1;
				uint64_t cq_w5;

				m = (struct rte_mbuf *)mbuf;
				d_off = (uintptr_t)(m->buf_addr) - (uintptr_t)m;
				d_off += RTE_PKTMBUF_HEADROOM;

				cq_w1 = *(uint64_t *)(gw.u64[1] + 8);
				cq_w5 = *(uint64_t *)(gw.u64[1] + 40);

				sa_base =
					cnxk_nix_sa_base_get(port, lookup_mem);
				sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);

				mbuf = (uint64_t)nix_sec_meta_to_mbuf_sc(
					cq_w1, cq_w5, sa_base, (uintptr_t)&iova, &loff,
					(struct rte_mbuf *)mbuf, d_off, flags,
					mbuf_init | ((uint64_t)port) << 48);
				if (loff)
					roc_npa_aura_op_free(m->pool->pool_id,
							     0, iova);
			}

			gw.u64[0] = CNXK_CLR_SUB_EVENT(gw.u64[0]);
			cn10k_wqe_to_mbuf(gw.u64[1], mbuf, port,
					  gw.u64[0] & 0xFFFFF, flags,
					  lookup_mem);
			/* Extracting tstamp, if PTP enabled*/
			tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)
							    gw.u64[1]) +
						   CNXK_SSO_WQE_SG_PTR);
			cn10k_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf,
						ws->tstamp,
						flags & NIX_RX_OFFLOAD_TSTAMP_F,
						(uint64_t *)tstamp_ptr);
			gw.u64[1] = mbuf;
		} else if (CNXK_EVENT_TYPE_FROM_TAG(gw.u64[0]) ==
			   RTE_EVENT_TYPE_ETHDEV_VECTOR) {
			uint8_t port = CNXK_SUB_EVENT_FROM_TAG(gw.u64[0]);
			__uint128_t vwqe_hdr = *(__uint128_t *)gw.u64[1];

			vwqe_hdr = ((vwqe_hdr >> 64) & 0xFFF) | BIT_ULL(31) |
				   ((vwqe_hdr & 0xFFFF) << 48) |
				   ((uint64_t)port << 32);
			*(uint64_t *)gw.u64[1] = (uint64_t)vwqe_hdr;
			cn10k_process_vwqe(gw.u64[1], port, flags, lookup_mem,
					   ws->tstamp, ws->lmt_base);
			/* Mark vector mempool object as get */
			RTE_MEMPOOL_CHECK_COOKIES(
				rte_mempool_from_obj((void *)gw.u64[1]),
				(void **)&gw.u64[1], 1, 1);
		}
	}

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* Used in cleaning up workslot. */
static __rte_always_inline uint16_t
cn10k_sso_hws_get_work_empty(struct cn10k_sso_hws *ws, struct rte_event *ev)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;
	uint64_t mbuf;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbz %[tag], 63, done%=			\n"
		     "		sevl					\n"
		     "rty%=:	wfe					\n"
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=			\n"
		     "done%=:	dmb ld					\n"
		     "		sub %[mbuf], %[wqp], #0x80		\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1]),
		       [mbuf] "=&r"(mbuf)
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_WQE0)
		     : "memory");
#else
	do {
		roc_load_pair(gw.u64[0], gw.u64[1],
			      ws->base + SSOW_LF_GWS_WQE0);
	} while (gw.u64[0] & BIT_ULL(63));
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
			cn10k_wqe_to_mbuf(gw.u64[1], mbuf, port,
					  gw.u64[0] & 0xFFFFF, 0, NULL);
			gw.u64[1] = mbuf;
		}
	}

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* CN10K Fastpath functions. */
uint16_t __rte_hot cn10k_sso_hws_enq(void *port, const struct rte_event *ev);
uint16_t __rte_hot cn10k_sso_hws_enq_burst(void *port,
					   const struct rte_event ev[],
					   uint16_t nb_events);
uint16_t __rte_hot cn10k_sso_hws_enq_new_burst(void *port,
					       const struct rte_event ev[],
					       uint16_t nb_events);
uint16_t __rte_hot cn10k_sso_hws_enq_fwd_burst(void *port,
					       const struct rte_event ev[],
					       uint16_t nb_events);
uint16_t __rte_hot cn10k_sso_hws_ca_enq(void *port, struct rte_event ev[],
					uint16_t nb_events);

#define R(name, flags)                                                         \
	uint16_t __rte_hot cn10k_sso_hws_deq_##name(                           \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_burst_##name(                     \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_##name(                       \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_burst_##name(                 \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_ca_##name(                        \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_ca_burst_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_ca_##name(                    \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_ca_burst_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_seg_##name(                       \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_seg_burst_##name(                 \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_seg_##name(                   \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_seg_burst_##name(             \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_ca_seg_##name(                    \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_ca_seg_burst_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_ca_seg_##name(                \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_deq_tmo_ca_seg_burst_##name(          \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_##name(                      \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_burst_##name(                \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_##name(                  \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_burst_##name(            \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_ca_##name(                   \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_ca_burst_##name(             \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_ca_##name(               \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_ca_burst_##name(         \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_seg_##name(                  \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_seg_burst_##name(            \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_seg_##name(              \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_seg_burst_##name(        \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_ca_seg_##name(               \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_ca_seg_burst_##name(         \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);                                       \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_ca_seg_##name(           \
		void *port, struct rte_event *ev, uint64_t timeout_ticks);     \
	uint16_t __rte_hot cn10k_sso_hws_reas_deq_tmo_ca_seg_burst_##name(     \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks);

NIX_RX_FASTPATH_MODES
#undef R

#define SSO_DEQ(fn, flags)                                                     \
	uint16_t __rte_hot fn(void *port, struct rte_event *ev,                \
			      uint64_t timeout_ticks)                          \
	{                                                                      \
		struct cn10k_sso_hws *ws = port;                               \
		RTE_SET_USED(timeout_ticks);                                   \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			ws->gw_rdata = cnxk_sso_hws_swtag_wait(                \
				ws->base + SSOW_LF_GWS_WQE0);                  \
			return 1;                                              \
		}                                                              \
		return cn10k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);  \
	}

#define SSO_DEQ_SEG(fn, flags)	  SSO_DEQ(fn, flags | NIX_RX_MULTI_SEG_F)
#define SSO_DEQ_CA(fn, flags)	  SSO_DEQ(fn, flags | CPT_RX_WQE_F)
#define SSO_DEQ_CA_SEG(fn, flags) SSO_DEQ_SEG(fn, flags | CPT_RX_WQE_F)

#define SSO_DEQ_TMO(fn, flags)                                                 \
	uint16_t __rte_hot fn(void *port, struct rte_event *ev,                \
			      uint64_t timeout_ticks)                          \
	{                                                                      \
		struct cn10k_sso_hws *ws = port;                               \
		uint16_t ret = 1;                                              \
		uint64_t iter;                                                 \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			ws->gw_rdata = cnxk_sso_hws_swtag_wait(                \
				ws->base + SSOW_LF_GWS_WQE0);                  \
			return ret;                                            \
		}                                                              \
		ret = cn10k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);   \
		for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)     \
			ret = cn10k_sso_hws_get_work(ws, ev, flags,            \
						     ws->lookup_mem);          \
		return ret;                                                    \
	}

#define SSO_DEQ_TMO_SEG(fn, flags)    SSO_DEQ_TMO(fn, flags | NIX_RX_MULTI_SEG_F)
#define SSO_DEQ_TMO_CA(fn, flags)     SSO_DEQ_TMO(fn, flags | CPT_RX_WQE_F)
#define SSO_DEQ_TMO_CA_SEG(fn, flags) SSO_DEQ_TMO_SEG(fn, flags | CPT_RX_WQE_F)

#define SSO_CMN_DEQ_BURST(fnb, fn, flags)                                      \
	uint16_t __rte_hot fnb(void *port, struct rte_event ev[],              \
			       uint16_t nb_events, uint64_t timeout_ticks)     \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
		return fn(port, ev, timeout_ticks);                            \
	}

#define SSO_CMN_DEQ_SEG_BURST(fnb, fn, flags)                                  \
	uint16_t __rte_hot fnb(void *port, struct rte_event ev[],              \
			       uint16_t nb_events, uint64_t timeout_ticks)     \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
		return fn(port, ev, timeout_ticks);                            \
	}

static __rte_always_inline struct cn10k_eth_txq *
cn10k_sso_hws_xtract_meta(struct rte_mbuf *m, const uint64_t *txq_data)
{
	return (struct cn10k_eth_txq
			*)(txq_data[(txq_data[m->port] >> 48) +
				    rte_event_eth_tx_adapter_txq_get(m)] &
			   (BIT_ULL(48) - 1));
}

static __rte_always_inline void
cn10k_sso_txq_fc_wait(const struct cn10k_eth_txq *txq)
{
	while ((uint64_t)txq->nb_sqb_bufs_adj <=
	       __atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED))
		;
}

static __rte_always_inline void
cn10k_sso_tx_one(struct cn10k_sso_hws *ws, struct rte_mbuf *m, uint64_t *cmd,
		 uint16_t lmt_id, uintptr_t lmt_addr, uint8_t sched_type,
		 const uint64_t *txq_data, const uint32_t flags)
{
	uint8_t lnum = 0, loff = 0, shft = 0;
	struct cn10k_eth_txq *txq;
	uintptr_t laddr;
	uint16_t segdw;
	uintptr_t pa;
	bool sec;

	txq = cn10k_sso_hws_xtract_meta(m, txq_data);
	cn10k_nix_tx_skeleton(txq, cmd, flags, 0);
	/* Perform header writes before barrier
	 * for TSO
	 */
	if (flags & NIX_TX_OFFLOAD_TSO_F)
		cn10k_nix_xmit_prepare_tso(m, flags);

	cn10k_nix_xmit_prepare(m, cmd, flags, txq->lso_tun_fmt, &sec,
			       txq->mark_flag, txq->mark_fmt);

	laddr = lmt_addr;
	/* Prepare CPT instruction and get nixtx addr if
	 * it is for CPT on same lmtline.
	 */
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
		cn10k_nix_prep_sec(m, cmd, &laddr, lmt_addr, &lnum, &loff,
				   &shft, txq->sa_base, flags);

	/* Move NIX desc to LMT/NIXTX area */
	cn10k_nix_xmit_mv_lmt_base(laddr, cmd, flags);

	if (flags & NIX_TX_MULTI_SEG_F)
		segdw = cn10k_nix_prepare_mseg(m, (uint64_t *)laddr, flags);
	else
		segdw = cn10k_nix_tx_ext_subs(flags) + 2;

	cn10k_nix_xmit_prepare_tstamp(txq, laddr, m->ol_flags, segdw, flags);
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
		pa = txq->cpt_io_addr | 3 << 4;
	else
		pa = txq->io_addr | ((segdw - 1) << 4);

	if (!CNXK_TAG_IS_HEAD(ws->gw_rdata) && !sched_type)
		ws->gw_rdata = roc_sso_hws_head_wait(ws->base);

	cn10k_sso_txq_fc_wait(txq);
	roc_lmt_submit_steorl(lmt_id, pa);
}

static __rte_always_inline void
cn10k_sso_vwqe_split_tx(struct cn10k_sso_hws *ws, struct rte_mbuf **mbufs,
			uint16_t nb_mbufs, uint64_t *cmd, uint16_t lmt_id,
			uintptr_t lmt_addr, uint8_t sched_type,
			const uint64_t *txq_data, const uint32_t flags)
{
	uint16_t port[4], queue[4];
	uint16_t i, j, pkts, scalar;
	struct cn10k_eth_txq *txq;

	scalar = nb_mbufs & (NIX_DESCS_PER_LOOP - 1);
	pkts = RTE_ALIGN_FLOOR(nb_mbufs, NIX_DESCS_PER_LOOP);

	for (i = 0; i < pkts; i += NIX_DESCS_PER_LOOP) {
		port[0] = mbufs[i]->port;
		port[1] = mbufs[i + 1]->port;
		port[2] = mbufs[i + 2]->port;
		port[3] = mbufs[i + 3]->port;

		queue[0] = rte_event_eth_tx_adapter_txq_get(mbufs[i]);
		queue[1] = rte_event_eth_tx_adapter_txq_get(mbufs[i + 1]);
		queue[2] = rte_event_eth_tx_adapter_txq_get(mbufs[i + 2]);
		queue[3] = rte_event_eth_tx_adapter_txq_get(mbufs[i + 3]);

		if (((port[0] ^ port[1]) & (port[2] ^ port[3])) ||
		    ((queue[0] ^ queue[1]) & (queue[2] ^ queue[3]))) {
			for (j = 0; j < 4; j++)
				cn10k_sso_tx_one(ws, mbufs[i + j], cmd, lmt_id,
						 lmt_addr, sched_type, txq_data,
						 flags);
		} else {
			txq = (struct cn10k_eth_txq
				       *)(txq_data[(txq_data[port[0]] >> 48) +
						   queue[0]] &
					  (BIT_ULL(48) - 1));
			cn10k_nix_xmit_pkts_vector(txq, (uint64_t *)ws,
						   &mbufs[i], 4, cmd,
						   flags | NIX_TX_VWQE_F);
		}
	}

	mbufs += i;

	for (i = 0; i < scalar; i++) {
		cn10k_sso_tx_one(ws, mbufs[i], cmd, lmt_id, lmt_addr,
				 sched_type, txq_data, flags);
	}
}

static __rte_always_inline uint16_t
cn10k_sso_hws_event_tx(struct cn10k_sso_hws *ws, struct rte_event *ev,
		       uint64_t *cmd, const uint64_t *txq_data,
		       const uint32_t flags)
{
	struct cn10k_eth_txq *txq;
	struct rte_mbuf *m;
	uintptr_t lmt_addr;
	uint16_t lmt_id;

	lmt_addr = ws->lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);

	if (ev->event_type & RTE_EVENT_TYPE_VECTOR) {
		struct rte_mbuf **mbufs = ev->vec->mbufs;
		uint64_t meta = *(uint64_t *)ev->vec;

		if (meta & BIT(31)) {
			txq = (struct cn10k_eth_txq
				       *)(txq_data[(txq_data[meta >> 32] >>
						    48) +
						   (meta >> 48)] &
					  (BIT_ULL(48) - 1));

			cn10k_nix_xmit_pkts_vector(txq, (uint64_t *)ws, mbufs,
						   meta & 0xFFFF, cmd,
						   flags | NIX_TX_VWQE_F);
		} else {
			cn10k_sso_vwqe_split_tx(
				ws, mbufs, meta & 0xFFFF, cmd, lmt_id, lmt_addr,
				ev->sched_type, txq_data, flags);
		}
		rte_mempool_put(rte_mempool_from_obj(ev->vec), ev->vec);
		rte_prefetch0(ws);
		return (meta & 0xFFFF);
	}

	m = ev->mbuf;
	cn10k_sso_tx_one(ws, m, cmd, lmt_id, lmt_addr, ev->sched_type, txq_data,
			 flags);

	return 1;
}

#define T(name, sz, flags)                                                     \
	uint16_t __rte_hot cn10k_sso_hws_tx_adptr_enq_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events);        \
	uint16_t __rte_hot cn10k_sso_hws_tx_adptr_enq_seg_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events);

NIX_TX_FASTPATH_MODES
#undef T

#define SSO_TX(fn, sz, flags)                                                  \
	uint16_t __rte_hot fn(void *port, struct rte_event ev[],               \
			      uint16_t nb_events)                              \
	{                                                                      \
		struct cn10k_sso_hws *ws = port;                               \
		uint64_t cmd[sz];                                              \
		RTE_SET_USED(nb_events);                                       \
		return cn10k_sso_hws_event_tx(                                 \
			ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data,  \
			flags);                                                \
	}

#define SSO_TX_SEG(fn, sz, flags)                                              \
	uint16_t __rte_hot fn(void *port, struct rte_event ev[],               \
			      uint16_t nb_events)                              \
	{                                                                      \
		uint64_t cmd[(sz) + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];           \
		struct cn10k_sso_hws *ws = port;                               \
		RTE_SET_USED(nb_events);                                       \
		return cn10k_sso_hws_event_tx(                                 \
			ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data,  \
			(flags) | NIX_TX_MULTI_SEG_F);                         \
	}

#endif
