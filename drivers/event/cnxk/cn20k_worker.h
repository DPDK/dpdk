/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN20K_WORKER_H__
#define __CN20K_WORKER_H__

#include <rte_eventdev.h>

#include "cn20k_eventdev.h"
#include "cnxk_worker.h"

static __rte_always_inline void
cn20k_sso_hws_post_process(struct cn20k_sso_hws *ws, uint64_t *u64, const uint32_t flags)
{
	RTE_SET_USED(ws);
	RTE_SET_USED(flags);

	u64[0] = (u64[0] & (0x3ull << 32)) << 6 | (u64[0] & (0x3FFull << 36)) << 4 |
		 (u64[0] & 0xffffffff);
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

uint16_t __rte_hot cn20k_sso_hws_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks);
uint16_t __rte_hot cn20k_sso_hws_deq_burst(void *port, struct rte_event ev[], uint16_t nb_events,
					   uint64_t timeout_ticks);
uint16_t __rte_hot cn20k_sso_hws_tmo_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks);
uint16_t __rte_hot cn20k_sso_hws_tmo_deq_burst(void *port, struct rte_event ev[],
					       uint16_t nb_events, uint64_t timeout_ticks);

#endif
