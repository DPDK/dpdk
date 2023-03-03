/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_WORKER_H__
#define __CN10K_WORKER_H__

#include <rte_eventdev.h>
#include "cn10k_cryptodev_event_dp.h"
#include "cn10k_rx.h"
#include "cnxk_worker.h"
#include "cn10k_eventdev.h"

/* CN10K Rx event fastpath */

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

static void
cn10k_sso_process_tstamp(uint64_t u64, uint64_t mbuf,
			 struct cnxk_timesync_info *tstamp)
{
	uint64_t tstamp_ptr;
	uint8_t laptr;

	laptr = (uint8_t) *
		(uint64_t *)(u64 + (CNXK_SSO_WQE_LAYR_PTR * sizeof(uint64_t)));
	if (laptr == sizeof(uint64_t)) {
		/* Extracting tstamp, if PTP enabled*/
		tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)u64) +
					   CNXK_SSO_WQE_SG_PTR);
		cn10k_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf, tstamp, true,
					 (uint64_t *)tstamp_ptr);
	}
}

static __rte_always_inline void
cn10k_process_vwqe(uintptr_t vwqe, uint16_t port_id, const uint32_t flags, struct cn10k_sso_hws *ws)
{
	uint64_t mbuf_init = 0x100010000ULL | RTE_PKTMBUF_HEADROOM;
	struct cnxk_timesync_info *tstamp = ws->tstamp[port_id];
	void *lookup_mem = ws->lookup_mem;
	uintptr_t lbase = ws->lmt_base;
	uint64_t meta_aura = 0, laddr;
	struct rte_event_vector *vec;
	uint16_t nb_mbufs, non_vec;
	struct rte_mempool *mp;
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

	if (flags & NIX_RX_OFFLOAD_TSTAMP_F && tstamp)
		mbuf_init |= 8;

	if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
		mp = (struct rte_mempool *)cnxk_nix_inl_metapool_get(port_id, lookup_mem);
		if (mp)
			meta_aura = mp->pool_id;
	}

	nb_mbufs = RTE_ALIGN_FLOOR(vec->nb_elem, NIX_DESCS_PER_LOOP);
	nb_mbufs = cn10k_nix_recv_pkts_vector(&mbuf_init, wqe, nb_mbufs,
					      flags | NIX_RX_VWQE_F,
					      lookup_mem, tstamp,
					      lbase, meta_aura);
	wqe += nb_mbufs;
	non_vec = vec->nb_elem - nb_mbufs;

	if (flags & NIX_RX_OFFLOAD_SECURITY_F && non_vec) {
		uint64_t sg_w1;

		mbuf = (struct rte_mbuf *)((uintptr_t)wqe[0] -
					   sizeof(struct rte_mbuf));
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
	}

	while (non_vec) {
		struct nix_cqe_hdr_s *cqe = (struct nix_cqe_hdr_s *)wqe[0];

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

		if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
			cn10k_sso_process_tstamp((uint64_t)wqe[0],
						 (uint64_t)mbuf, tstamp);
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
cn10k_sso_hws_post_process(struct cn10k_sso_hws *ws, uint64_t *u64,
			   const uint32_t flags)
{
	u64[0] = (u64[0] & (0x3ull << 32)) << 6 |
		 (u64[0] & (0x3FFull << 36)) << 4 | (u64[0] & 0xffffffff);
	if ((flags & CPT_RX_WQE_F) &&
	    (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_CRYPTODEV)) {
		u64[1] = cn10k_cpt_crypto_adapter_dequeue(u64[1]);
	} else if ((flags & CPT_RX_WQE_F) &&
		   (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_CRYPTODEV_VECTOR)) {
		u64[1] = cn10k_cpt_crypto_adapter_vector_dequeue(u64[1]);
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_ETHDEV) {
		uint8_t port = CNXK_SUB_EVENT_FROM_TAG(u64[0]);
		uint64_t mbuf;

		mbuf = u64[1] - sizeof(struct rte_mbuf);
		rte_prefetch0((void *)mbuf);
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			const uint64_t mbuf_init =
				0x100010000ULL | RTE_PKTMBUF_HEADROOM |
				(flags & NIX_RX_OFFLOAD_TSTAMP_F ? 8 : 0);
			struct rte_mbuf *m;
			uintptr_t sa_base;
			uint64_t iova = 0;
			uint8_t loff = 0;
			uint16_t d_off;
			uint64_t cq_w1;
			uint64_t cq_w5;

			m = (struct rte_mbuf *)mbuf;
			d_off = (*(uint64_t *)(u64[1] + 72)) - (uintptr_t)m;

			cq_w1 = *(uint64_t *)(u64[1] + 8);
			cq_w5 = *(uint64_t *)(u64[1] + 40);

			sa_base = cnxk_nix_sa_base_get(port, ws->lookup_mem);
			sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);

			mbuf = (uint64_t)nix_sec_meta_to_mbuf_sc(
				cq_w1, cq_w5, sa_base, (uintptr_t)&iova, &loff,
				(struct rte_mbuf *)mbuf, d_off, flags,
				mbuf_init | ((uint64_t)port) << 48);
			if (loff)
				roc_npa_aura_op_free(m->pool->pool_id, 0, iova);
		}

		u64[0] = CNXK_CLR_SUB_EVENT(u64[0]);
		cn10k_wqe_to_mbuf(u64[1], mbuf, port, u64[0] & 0xFFFFF, flags,
				  ws->lookup_mem);
		if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
			cn10k_sso_process_tstamp(u64[1], mbuf,
						 ws->tstamp[port]);
		u64[1] = mbuf;
	} else if (CNXK_EVENT_TYPE_FROM_TAG(u64[0]) == RTE_EVENT_TYPE_ETHDEV_VECTOR) {
		uint8_t port = CNXK_SUB_EVENT_FROM_TAG(u64[0]);
		__uint128_t vwqe_hdr = *(__uint128_t *)u64[1];

		vwqe_hdr = ((vwqe_hdr >> 64) & 0xFFF) | BIT_ULL(31) |
			   ((vwqe_hdr & 0xFFFF) << 48) | ((uint64_t)port << 32);
		*(uint64_t *)u64[1] = (uint64_t)vwqe_hdr;
		cn10k_process_vwqe(u64[1], port, flags, ws);
		/* Mark vector mempool object as get */
		RTE_MEMPOOL_CHECK_COOKIES(rte_mempool_from_obj((void *)u64[1]),
					  (void **)&u64[1], 1, 1);
	}
}

static __rte_always_inline uint16_t
cn10k_sso_hws_get_work(struct cn10k_sso_hws *ws, struct rte_event *ev,
		       const uint32_t flags)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

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
	if (gw.u64[1])
		cn10k_sso_hws_post_process(ws, gw.u64, flags);

	ev->event = gw.u64[0];
	ev->u64 = gw.u64[1];

	return !!gw.u64[1];
}

/* Used in cleaning up workslot. */
static __rte_always_inline uint16_t
cn10k_sso_hws_get_work_empty(struct cn10k_sso_hws *ws, struct rte_event *ev,
			     const uint32_t flags)
{
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbz %[tag], 63, done%=			\n"
		     "		sevl					\n"
		     "rty%=:	wfe					\n"
		     "		ldp %[tag], %[wqp], [%[tag_loc]]	\n"
		     "		tbnz %[tag], 63, rty%=			\n"
		     "done%=:	dmb ld					\n"
		     : [tag] "=&r"(gw.u64[0]), [wqp] "=&r"(gw.u64[1])
		     : [tag_loc] "r"(ws->base + SSOW_LF_GWS_WQE0)
		     : "memory");
#else
	do {
		roc_load_pair(gw.u64[0], gw.u64[1],
			      ws->base + SSOW_LF_GWS_WQE0);
	} while (gw.u64[0] & BIT_ULL(63));
#endif

	ws->gw_rdata = gw.u64[0];
	if (gw.u64[1])
		cn10k_sso_hws_post_process(ws, gw.u64, flags);

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
		return cn10k_sso_hws_get_work(ws, ev, flags);                  \
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
		ret = cn10k_sso_hws_get_work(ws, ev, flags);                   \
		for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)     \
			ret = cn10k_sso_hws_get_work(ws, ev, flags);           \
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

#endif
