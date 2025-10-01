/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN20K_WORKER_H__
#define __CN20K_WORKER_H__

#include <rte_event_timer_adapter.h>
#include <rte_eventdev.h>

#include "cn20k_cryptodev_event_dp.h"
#include "cn20k_eventdev.h"
#include "cn20k_rx.h"
#include "cnxk_worker.h"

/* CN20K Rx event fastpath */

static __rte_always_inline void
cn20k_wqe_to_mbuf(uint64_t wqe, const uint64_t __mbuf, uint8_t port_id, const uint32_t tag,
		  const uint32_t flags, const void *const lookup_mem, uintptr_t cpth,
		  uintptr_t sa_base)
{
	const uint64_t mbuf_init =
		0x100010000ULL | RTE_PKTMBUF_HEADROOM | (flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);
	struct rte_mbuf *mbuf = (struct rte_mbuf *)__mbuf;
	uint64_t buf_sz = 0;

	if (flags & NIX_RX_REAS_F)
		buf_sz = cnxk_nix_inl_bufsize_get(port_id, lookup_mem);

	cn20k_nix_cqe_to_mbuf((struct nix_cqe_hdr_s *)wqe, tag, (struct rte_mbuf *)mbuf, lookup_mem,
			      mbuf_init | ((uint64_t)port_id) << 48, cpth, sa_base, buf_sz, flags);
}

static void
cn20k_sso_process_tstamp(uint64_t u64, uint64_t mbuf, struct cnxk_timesync_info *tstamp)
{
	uint64_t tstamp_ptr;
	uint8_t laptr;

	laptr = (uint8_t)*(uint64_t *)(u64 + (CNXK_SSO_WQE_LAYR_PTR * sizeof(uint64_t)));
	if (laptr == sizeof(uint64_t)) {
		/* Extracting tstamp, if PTP enabled*/
		tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)u64) + CNXK_SSO_WQE_SG_PTR);
		cn20k_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf, tstamp, true,
					 (uint64_t *)tstamp_ptr);
	}
}

static __rte_always_inline void
cn20k_process_vwqe(uintptr_t vwqe, uint16_t port_id, const uint32_t flags, struct cn20k_sso_hws *ws)
{
	uint64_t mbuf_init = 0x100010000ULL | RTE_PKTMBUF_HEADROOM;
	struct cnxk_timesync_info *tstamp = ws->tstamp[port_id];
	uint8_t m_sz = sizeof(struct rte_mbuf);
	void *lookup_mem = ws->lookup_mem;
	uintptr_t lbase = ws->lmt_base;
	uint64_t meta_aura = 0, laddr;
	struct rte_event_vector *vec;
	uint16_t nb_mbufs, non_vec;
	struct rte_mempool *mp;
	uint16_t lmt_id, d_off;
	struct rte_mbuf **wqe;
	struct rte_mbuf *mbuf;
	uint64_t sa_base = 0;
	uint64_t buf_sz = 0;
	uintptr_t cpth = 0;
	uint8_t loff = 0;
	int i;

	mbuf_init |= ((uint64_t)port_id) << 48;
	vec = (struct rte_event_vector *)vwqe;
	wqe = vec->mbufs;

	rte_prefetch0(&vec->ptrs[0]);
#define OBJS_PER_CLINE (RTE_CACHE_LINE_SIZE / sizeof(void *))
	for (i = OBJS_PER_CLINE; i < vec->nb_elem; i += OBJS_PER_CLINE)
		rte_prefetch0(&vec->ptrs[i]);

	if (flags & NIX_RX_OFFLOAD_TSTAMP_F && tstamp)
		mbuf_init |= 8;

	if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
		mp = (struct rte_mempool *)cnxk_nix_inl_metapool_get(port_id, lookup_mem);
		if (mp)
			meta_aura = mp->pool_id;
	}

	nb_mbufs = RTE_ALIGN_FLOOR(vec->nb_elem, NIX_DESCS_PER_LOOP);
	nb_mbufs = cn20k_nix_recv_pkts_vector(&mbuf_init, wqe, nb_mbufs, flags | NIX_RX_VWQE_F,
					      lookup_mem, tstamp, lbase, meta_aura);
	wqe += nb_mbufs;
	non_vec = vec->nb_elem - nb_mbufs;

	if (flags & NIX_RX_OFFLOAD_SECURITY_F && non_vec) {
		uint64_t sg_w1;

		mbuf = (struct rte_mbuf *)((uintptr_t)wqe[0] - sizeof(struct rte_mbuf));
		/* Pick first mbuf's aura handle assuming all
		 * mbufs are from a vec and are from same RQ.
		 */
		if (!meta_aura)
			meta_aura = mbuf->pool->pool_id;
		ROC_LMT_BASE_ID_GET(lbase, lmt_id);
		laddr = lbase;
		laddr += 8;
		sg_w1 = *(uint64_t *)(((uintptr_t)wqe[0]) + 72);
		d_off = sg_w1 - (uintptr_t)mbuf;
		sa_base = cnxk_nix_sa_base_get(mbuf_init >> 48, lookup_mem);
		sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);

		if (flags & NIX_RX_REAS_F)
			buf_sz = cnxk_nix_inl_bufsize_get(port_id, lookup_mem);
	}

	while (non_vec) {
		struct nix_cqe_hdr_s *cqe = (struct nix_cqe_hdr_s *)wqe[0];

		mbuf = (struct rte_mbuf *)((char *)cqe - sizeof(struct rte_mbuf));

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		/* Translate meta to mbuf */
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			const uint64_t cq_w1 = *((const uint64_t *)cqe + 1);

			cpth = ((uintptr_t)mbuf + (uint16_t)d_off);

			if (cq_w1 & BIT(11)) {
				/* Mark meta mbuf as put */
				RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 0);

				/* Store meta in lmtline to free
				 * Assume all meta's from same aura.
				 */
				*(uint64_t *)(laddr + (loff << 3)) = (uint64_t)mbuf;
				loff = loff + 1;
				mbuf = (struct rte_mbuf *)(*(uint64_t *)(cpth + 8) - m_sz);
				/* Mark inner mbuf as get */
				RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);
			}
		}

		cn20k_nix_cqe_to_mbuf(cqe, cqe->tag, mbuf, lookup_mem, mbuf_init, cpth, sa_base,
				      buf_sz, flags);

		if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
			cn20k_sso_process_tstamp((uint64_t)wqe[0], (uint64_t)mbuf, tstamp);
		wqe[0] = (struct rte_mbuf *)mbuf;
		non_vec--;
		wqe++;
	}

	/* Free remaining meta buffers if any */
	if (flags & NIX_RX_OFFLOAD_SECURITY_F && loff) {
		nix_sec_flush_meta(laddr, lmt_id, loff, meta_aura);
		plt_io_wmb();
	}
}

static __rte_always_inline void
cn20k_sso_hws_post_process(struct cn20k_sso_hws *ws, uint64_t *u64, const uint32_t flags)
{
	uint8_t m_sz = sizeof(struct rte_mbuf);
	uintptr_t sa_base = 0;

	u64[0] = (u64[0] & (0x3ull << 32)) << 6 | (u64[0] & (0x3FFull << 36)) << 4 |
		 (u64[0] & 0xffffffff);
	if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_CRYPTODEV) {
		u64[1] = cn20k_cpt_crypto_adapter_dequeue(u64[1]);
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_CRYPTODEV_VECTOR) {
		__uint128_t vwqe_hdr = *(__uint128_t *)u64[1];
		vwqe_hdr = ((vwqe_hdr >> 64) & 0xFFF) | BIT_ULL(31) | ((vwqe_hdr & 0xFFFF) << 48);
		*(uint64_t *)u64[1] = (uint64_t)vwqe_hdr;
		u64[1] = cn20k_cpt_crypto_adapter_vector_dequeue(u64[1]);
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_ETHDEV) {
		uint8_t port = CNXK_SUB_EVENT_FROM_TAG(u64[0]);
		uintptr_t cpth = 0;
		uint64_t mbuf;

		mbuf = u64[1] - sizeof(struct rte_mbuf);
		rte_prefetch0((void *)mbuf);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(((struct rte_mbuf *)mbuf)->pool, (void **)&mbuf, 1, 1);

		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			void *lookup_mem = ws->lookup_mem;
			struct rte_mempool *mp = NULL;
			uint64_t meta_aura;
			struct rte_mbuf *m;
			uint64_t iova = 0;
			uint8_t loff = 0;
			uint16_t d_off;
			uint64_t cq_w1;

			m = (struct rte_mbuf *)mbuf;
			d_off = (*(uint64_t *)(u64[1] + 72)) - (uintptr_t)m;
			cq_w1 = *(uint64_t *)(u64[1] + 8);

			sa_base = cnxk_nix_sa_base_get(port, ws->lookup_mem);
			sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);

			cpth = ((uintptr_t)mbuf + (uint16_t)d_off);
			mp = (struct rte_mempool *)cnxk_nix_inl_metapool_get(port, lookup_mem);
			meta_aura = mp ? mp->pool_id : m->pool->pool_id;

			if (cq_w1 & BIT(11)) {
				/* Mark meta mbuf as put */
				RTE_MEMPOOL_CHECK_COOKIES(m->pool, (void **)&m, 1, 0);

				/* Store meta in lmtline to free
				 * Assume all meta's from same aura.
				 */
				*(uint64_t *)((uintptr_t)&iova + (loff << 3)) = (uint64_t)m;
				loff = loff + 1;
				mbuf = (uint64_t)(*(uint64_t *)(cpth + 8) - m_sz);
				/* Mark inner mbuf as get */
				RTE_MEMPOOL_CHECK_COOKIES(((struct rte_mbuf *)mbuf)->pool,
							  (void **)&mbuf, 1, 1);
				roc_npa_aura_op_free(meta_aura, 0, iova);
			}
		}

		u64[0] = CNXK_CLR_SUB_EVENT(u64[0]);
		cn20k_wqe_to_mbuf(u64[1], mbuf, port, u64[0] & 0xFFFFF, flags, ws->lookup_mem, cpth,
				  sa_base);
		if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
			cn20k_sso_process_tstamp(u64[1], mbuf, ws->tstamp[port]);
		u64[1] = mbuf;
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_ETHDEV_VECTOR) {
		uint8_t port = CNXK_SUB_EVENT_FROM_TAG(u64[0]);
		__uint128_t vwqe_hdr = *(__uint128_t *)u64[1];

		vwqe_hdr = ((vwqe_hdr >> 64) & 0xFFF) | BIT_ULL(31) | ((vwqe_hdr & 0xFFFF) << 48) |
			   ((uint64_t)port << 32);
		*(uint64_t *)u64[1] = (uint64_t)vwqe_hdr;
		cn20k_process_vwqe(u64[1], port, flags, ws);
		/* Mark vector mempool object as get */
		RTE_MEMPOOL_CHECK_COOKIES(rte_mempool_from_obj((void *)u64[1]), (void **)&u64[1], 1,
					  1);
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_TIMER) {
		struct rte_event_timer *tev = (struct rte_event_timer *)u64[1];

		tev->state = RTE_EVENT_TIMER_NOT_ARMED;
		u64[1] = tev->ev.u64;
	}
}

static __rte_always_inline uint16_t
cn20k_sso_hws_get_work(struct cn20k_sso_hws *ws, struct rte_event *ev, const uint32_t flags)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

	gw.get_work = ws->gw_wdata;
#if defined(RTE_ARCH_ARM64)
#if defined(__clang__)
	register uint64_t x0 __asm("x0") = (uint64_t)gw.u64[0];
	register uint64_t x1 __asm("x1") = (uint64_t)gw.u64[1];
#if defined(RTE_ARM_USE_WFE)
	plt_write64(gw.u64[0], ws->base + SSOW_LF_GWS_OP_GET_WORK0);
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldp %[x0], %[x1], [%[tag_loc]]	\n"
		     "		tbz %[x0], %[pend_gw], done%=	\n"
		     "		sevl					\n"
		     "rty%=:	wfe					\n"
		     "		ldp %[x0], %[x1], [%[tag_loc]]	\n"
		     "		tbnz %[x0], %[pend_gw], rty%=	\n"
		     "done%=:						\n"
		     "		dmb ld					\n"
		     : [x0] "+r" (x0), [x1] "+r" (x1)
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_WQE0),
		       [pend_gw] "i"(SSOW_LF_GWS_TAG_PEND_GET_WORK_BIT)
		     : "memory");
#else
	asm volatile(".arch armv8-a+lse\n"
		     "caspal %[x0], %[x1], %[x0], %[x1], [%[dst]]\n"
		     : [x0] "+r" (x0), [x1] "+r" (x1)
		     : [dst] "r"(ws->base + SSOW_LF_GWS_OP_GET_WORK0)
		     : "memory");
#endif
	gw.u64[0] = x0;
	gw.u64[1] = x1;
#else
#if defined(RTE_ARM_USE_WFE)
	plt_write64(gw.u64[0], ws->base + SSOW_LF_GWS_OP_GET_WORK0);
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldp %[wdata], %H[wdata], [%[tag_loc]]	\n"
		     "		tbz %[wdata], %[pend_gw], done%=	\n"
		     "		sevl					\n"
		     "rty%=:	wfe					\n"
		     "		ldp %[wdata], %H[wdata], [%[tag_loc]]	\n"
		     "		tbnz %[wdata], %[pend_gw], rty%=	\n"
		     "done%=:						\n"
		     "		dmb ld					\n"
		     : [wdata] "=&r"(gw.get_work)
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_WQE0),
		       [pend_gw] "i"(SSOW_LF_GWS_TAG_PEND_GET_WORK_BIT)
		     : "memory");
#else
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "caspal %[wdata], %H[wdata], %[wdata], %H[wdata], [%[gw_loc]]\n"
		     : [wdata] "+r"(gw.get_work)
		     : [gw_loc] "r"(ws->base + SSOW_LF_GWS_OP_GET_WORK0)
		     : "memory");
#endif
#endif
#else
	plt_write64(gw.u64[0], ws->base + SSOW_LF_GWS_OP_GET_WORK0);
	do {
		roc_load_pair(gw.u64[0], gw.u64[1], ws->base + SSOW_LF_GWS_WQE0);
	} while (gw.u64[0] & BIT_ULL(63));
	rte_atomic_thread_fence(rte_memory_order_seq_cst);
#endif
	ws->gw_rdata = gw.u64[0];
	if (gw.u64[1])
		cn20k_sso_hws_post_process(ws, gw.u64, flags);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* Used in cleaning up workslot. */
static __rte_always_inline uint16_t
cn20k_sso_hws_get_work_empty(struct cn20k_sso_hws *ws, struct rte_event *ev, const uint32_t flags)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbz %[tag], 63, .Ldone%=		\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbnz %[tag], 63, .Lrty%=		\n"
		     ".Ldone%=:	dmb ld					\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1])
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_WQE0)
		     : "memory");
#else
	do {
		roc_load_pair(gw.u64[0], gw.u64[1], ws->base + SSOW_LF_GWS_WQE0);
	} while (gw.u64[0] & BIT_ULL(63));
#endif

	ws->gw_rdata = gw.u64[0];
	if (gw.u64[1])
		cn20k_sso_hws_post_process(ws, gw.u64, flags);
	else
		gw.u64[0] = (gw.u64[0] & (0x3ull << 32)) << 6 |
			    (gw.u64[0] & (0x3FFull << 36)) << 4 | (gw.u64[0] & 0xffffffff);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* CN20K Fastpath functions. */
uint16_t __rte_hot cn20k_sso_hws_enq_burst(void *port, const struct rte_event ev[],
					   uint16_t nb_events);
uint16_t __rte_hot cn20k_sso_hws_enq_new_burst(void *port, const struct rte_event ev[],
					       uint16_t nb_events);
uint16_t __rte_hot cn20k_sso_hws_enq_fwd_burst(void *port, const struct rte_event ev[],
					       uint16_t nb_events);
int __rte_hot cn20k_sso_hws_profile_switch(void *port, uint8_t profile);
int __rte_hot cn20k_sso_hws_preschedule_modify(void *port,
					       enum rte_event_dev_preschedule_type type);
void __rte_hot cn20k_sso_hws_preschedule(void *port, enum rte_event_dev_preschedule_type type);

#define R(name, flags)                                                                             \
	uint16_t __rte_hot cn20k_sso_hws_deq_burst_##name(                                         \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_tmo_burst_##name(                                     \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_ca_burst_##name(                                      \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_tmo_ca_burst_##name(                                  \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_seg_burst_##name(                                     \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_tmo_seg_burst_##name(                                 \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_ca_seg_burst_##name(                                  \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_deq_tmo_ca_seg_burst_##name(                              \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_burst_##name(                                    \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_tmo_burst_##name(                                \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_ca_burst_##name(                                 \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_tmo_ca_burst_##name(                             \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_seg_burst_##name(                                \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_tmo_seg_burst_##name(                            \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_ca_seg_burst_##name(                             \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);    \
	uint16_t __rte_hot cn20k_sso_hws_reas_deq_tmo_ca_seg_burst_##name(                         \
		void *port, struct rte_event ev[], uint16_t nb_events, uint64_t timeout_ticks);

NIX_RX_FASTPATH_MODES
#undef R

#define SSO_DEQ(fn, flags)                                                                         \
	static __rte_always_inline uint16_t fn(void *port, struct rte_event *ev,                   \
					       uint64_t timeout_ticks)                             \
	{                                                                                          \
		struct cn20k_sso_hws *ws = port;                                                   \
		RTE_SET_USED(timeout_ticks);                                                       \
		if (ws->swtag_req) {                                                               \
			ws->swtag_req = 0;                                                         \
			ws->gw_rdata = cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_WQE0);       \
			return 1;                                                                  \
		}                                                                                  \
		return cn20k_sso_hws_get_work(ws, ev, flags);                                      \
	}

#define SSO_DEQ_SEG(fn, flags) SSO_DEQ(fn, flags | NIX_RX_MULTI_SEG_F)

#define SSO_DEQ_TMO(fn, flags)                                                                     \
	static __rte_always_inline uint16_t fn(void *port, struct rte_event *ev,                   \
					       uint64_t timeout_ticks)                             \
	{                                                                                          \
		struct cn20k_sso_hws *ws = port;                                                   \
		uint16_t ret = 1;                                                                  \
		uint64_t iter;                                                                     \
		if (ws->swtag_req) {                                                               \
			ws->swtag_req = 0;                                                         \
			ws->gw_rdata = cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_WQE0);       \
			return ret;                                                                \
		}                                                                                  \
		ret = cn20k_sso_hws_get_work(ws, ev, flags);                                       \
		for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)                         \
			ret = cn20k_sso_hws_get_work(ws, ev, flags);                               \
		return ret;                                                                        \
	}

#define SSO_DEQ_TMO_SEG(fn, flags) SSO_DEQ_TMO(fn, flags | NIX_RX_MULTI_SEG_F)

#define R(name, flags)                                                                             \
	SSO_DEQ(cn20k_sso_hws_deq_##name, flags)                                                   \
	SSO_DEQ(cn20k_sso_hws_reas_deq_##name, flags | NIX_RX_REAS_F)                              \
	SSO_DEQ_SEG(cn20k_sso_hws_deq_seg_##name, flags)                                           \
	SSO_DEQ_SEG(cn20k_sso_hws_reas_deq_seg_##name, flags | NIX_RX_REAS_F)                      \
	SSO_DEQ_TMO(cn20k_sso_hws_deq_tmo_##name, flags)                                           \
	SSO_DEQ_TMO(cn20k_sso_hws_reas_deq_tmo_##name, flags | NIX_RX_REAS_F)                      \
	SSO_DEQ_TMO_SEG(cn20k_sso_hws_deq_tmo_seg_##name, flags)                                   \
	SSO_DEQ_TMO_SEG(cn20k_sso_hws_reas_deq_tmo_seg_##name, flags | NIX_RX_REAS_F)

NIX_RX_FASTPATH_MODES
#undef R

#define SSO_CMN_DEQ_BURST(fnb, fn, flags)                                                          \
	uint16_t __rte_hot fnb(void *port, struct rte_event ev[], uint16_t nb_events,              \
			       uint64_t timeout_ticks)                                             \
	{                                                                                          \
		RTE_SET_USED(nb_events);                                                           \
		return fn(port, ev, timeout_ticks);                                                \
	}

#define SSO_CMN_DEQ_SEG_BURST(fnb, fn, flags)                                                      \
	uint16_t __rte_hot fnb(void *port, struct rte_event ev[], uint16_t nb_events,              \
			       uint64_t timeout_ticks)                                             \
	{                                                                                          \
		RTE_SET_USED(nb_events);                                                           \
		return fn(port, ev, timeout_ticks);                                                \
	}

uint16_t __rte_hot cn20k_sso_hws_deq_burst_all_offload(void *port, struct rte_event ev[],
						       uint16_t nb_events, uint64_t timeout_ticks);
uint16_t __rte_hot cn20k_sso_hws_deq_burst_all_offload_tst(void *port, struct rte_event ev[],
							   uint16_t nb_events,
							   uint64_t timeout_ticks);

#endif
