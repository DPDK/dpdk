/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

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

	cnxk_sso_hws_add_work(event_ptr, tag, new_tt, ws->grp_base + (grp << 12));
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
			cnxk_sso_hws_swtag_untag(ws->base + SSOW_LF_GWS_OP_SWTAG_UNTAG);
	} else {
		cnxk_sso_hws_swtag_norm(tag, new_tt, ws->base + SSOW_LF_GWS_OP_SWTAG_NORM);
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
	cnxk_sso_hws_swtag_desched(tag, new_tt, grp, ws->base + SSOW_LF_GWS_OP_SWTAG_DESCHED);
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

static inline int32_t
sso_read_xaq_space(struct cn10k_sso_hws *ws)
{
	return (ws->xaq_lmt - __atomic_load_n(ws->fc_mem, __ATOMIC_RELAXED)) *
	       ROC_SSO_XAE_PER_XAQ;
}

static inline void
sso_lmt_aw_wait_fc(struct cn10k_sso_hws *ws, int64_t req)
{
	int64_t cached, refill;

retry:
	while (__atomic_load_n(ws->fc_cache_space, __ATOMIC_RELAXED) < 0)
		;

	cached = __atomic_fetch_sub(ws->fc_cache_space, req, __ATOMIC_ACQUIRE) - req;
	/* Check if there is enough space, else update and retry. */
	if (cached < 0) {
		/* Check if we have space else retry. */
		do {
			refill = sso_read_xaq_space(ws);
		} while (refill <= 0);
		__atomic_compare_exchange(ws->fc_cache_space, &cached, &refill,
					  0, __ATOMIC_RELEASE,
					  __ATOMIC_RELAXED);
		goto retry;
	}
}

uint16_t __rte_hot
cn10k_sso_hws_enq(void *port, const struct rte_event *ev)
{
	struct cn10k_sso_hws *ws = port;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		return cn10k_sso_hws_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		cn10k_sso_hws_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		if (ws->swtag_req) {
			cnxk_sso_hws_desched(ev->u64, ws->base);
			ws->swtag_req = 0;
			break;
		}
		cnxk_sso_hws_swtag_flush(ws->base);
		break;
	default:
		return 0;
	}

	return 1;
}

#define VECTOR_SIZE_BITS	     0xFFFFFFFFFFF80000ULL
#define VECTOR_GET_LINE_OFFSET(line) (19 + (3 * line))

static uint64_t
vector_size_partial_mask(uint16_t off, uint16_t cnt)
{
	return (VECTOR_SIZE_BITS & ~(~0x0ULL << off)) |
	       ((uint64_t)(cnt - 1) << off);
}

static __rte_always_inline uint16_t
cn10k_sso_hws_new_event_lmtst(struct cn10k_sso_hws *ws, uint8_t queue_id,
			      const struct rte_event ev[], uint16_t n)
{
	uint16_t lines, partial_line, burst, left;
	uint64_t wdata[2], pa[2] = {0};
	uintptr_t lmt_addr;
	uint16_t sz0, sz1;
	uint16_t lmt_id;

	sz0 = sz1 = 0;
	lmt_addr = ws->lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);

	left = n;
again:
	burst = RTE_MIN(
		BIT(ROC_SSO_AW_PER_LMT_LINE_LOG2 + ROC_LMT_LINES_PER_CORE_LOG2),
		left);

	/* Set wdata */
	lines = burst >> ROC_SSO_AW_PER_LMT_LINE_LOG2;
	partial_line = burst & (BIT(ROC_SSO_AW_PER_LMT_LINE_LOG2) - 1);
	wdata[0] = wdata[1] = 0;
	if (lines > BIT(ROC_LMT_LINES_PER_STR_LOG2)) {
		wdata[0] = lmt_id;
		wdata[0] |= 15ULL << 12;
		wdata[0] |= VECTOR_SIZE_BITS;
		pa[0] = (ws->grp_base + (queue_id << 12) +
			 SSO_LF_GGRP_OP_AW_LMTST) |
			(0x7 << 4);
		sz0 = 16 << ROC_SSO_AW_PER_LMT_LINE_LOG2;

		wdata[1] = lmt_id + 16;
		pa[1] = (ws->grp_base + (queue_id << 12) +
			 SSO_LF_GGRP_OP_AW_LMTST) |
			(0x7 << 4);

		lines -= 17;
		wdata[1] |= partial_line ? (uint64_t)(lines + 1) << 12 :
						 (uint64_t)(lines << 12);
		wdata[1] |= partial_line ?
				    vector_size_partial_mask(
					    VECTOR_GET_LINE_OFFSET(lines),
					    partial_line) :
				    VECTOR_SIZE_BITS;
		sz1 = burst - sz0;
		partial_line = 0;
	} else if (lines) {
		/* We need to handle two cases here:
		 * 1. Partial line spill over to wdata[1] i.e. lines == 16
		 * 2. Partial line with spill lines < 16.
		 */
		wdata[0] = lmt_id;
		pa[0] = (ws->grp_base + (queue_id << 12) +
			 SSO_LF_GGRP_OP_AW_LMTST) |
			(0x7 << 4);
		sz0 = lines << ROC_SSO_AW_PER_LMT_LINE_LOG2;
		if (lines == 16) {
			wdata[0] |= 15ULL << 12;
			wdata[0] |= VECTOR_SIZE_BITS;
			if (partial_line) {
				wdata[1] = lmt_id + 16;
				pa[1] = (ws->grp_base + (queue_id << 12) +
					 SSO_LF_GGRP_OP_AW_LMTST) |
					((partial_line - 1) << 4);
			}
		} else {
			lines -= 1;
			wdata[0] |= partial_line ? (uint64_t)(lines + 1) << 12 :
							 (uint64_t)(lines << 12);
			wdata[0] |=
				partial_line ?
					vector_size_partial_mask(
						VECTOR_GET_LINE_OFFSET(lines),
						partial_line) :
					VECTOR_SIZE_BITS;
			sz0 += partial_line;
		}
		sz1 = burst - sz0;
		partial_line = 0;
	}

	/* Only partial lines */
	if (partial_line) {
		wdata[0] = lmt_id;
		pa[0] = (ws->grp_base + (queue_id << 12) +
			 SSO_LF_GGRP_OP_AW_LMTST) |
			((partial_line - 1) << 4);
		sz0 = partial_line;
		sz1 = burst - sz0;
	}

#if defined(RTE_ARCH_ARM64)
	uint64x2_t aw_mask = {0xC0FFFFFFFFULL, ~0x0ULL};
	uint64x2_t tt_mask = {0x300000000ULL, 0};
	uint16_t parts;

	while (burst) {
		parts = burst > 7 ? 8 : plt_align32prevpow2(burst);
		burst -= parts;
		/* Lets try to fill at least one line per burst. */
		switch (parts) {
		case 8: {
			uint64x2_t aw0, aw1, aw2, aw3, aw4, aw5, aw6, aw7;

			aw0 = vandq_u64(vld1q_u64((const uint64_t *)&ev[0]),
					aw_mask);
			aw1 = vandq_u64(vld1q_u64((const uint64_t *)&ev[1]),
					aw_mask);
			aw2 = vandq_u64(vld1q_u64((const uint64_t *)&ev[2]),
					aw_mask);
			aw3 = vandq_u64(vld1q_u64((const uint64_t *)&ev[3]),
					aw_mask);
			aw4 = vandq_u64(vld1q_u64((const uint64_t *)&ev[4]),
					aw_mask);
			aw5 = vandq_u64(vld1q_u64((const uint64_t *)&ev[5]),
					aw_mask);
			aw6 = vandq_u64(vld1q_u64((const uint64_t *)&ev[6]),
					aw_mask);
			aw7 = vandq_u64(vld1q_u64((const uint64_t *)&ev[7]),
					aw_mask);

			aw0 = vorrq_u64(vandq_u64(vshrq_n_u64(aw0, 6), tt_mask),
					aw0);
			aw1 = vorrq_u64(vandq_u64(vshrq_n_u64(aw1, 6), tt_mask),
					aw1);
			aw2 = vorrq_u64(vandq_u64(vshrq_n_u64(aw2, 6), tt_mask),
					aw2);
			aw3 = vorrq_u64(vandq_u64(vshrq_n_u64(aw3, 6), tt_mask),
					aw3);
			aw4 = vorrq_u64(vandq_u64(vshrq_n_u64(aw4, 6), tt_mask),
					aw4);
			aw5 = vorrq_u64(vandq_u64(vshrq_n_u64(aw5, 6), tt_mask),
					aw5);
			aw6 = vorrq_u64(vandq_u64(vshrq_n_u64(aw6, 6), tt_mask),
					aw6);
			aw7 = vorrq_u64(vandq_u64(vshrq_n_u64(aw7, 6), tt_mask),
					aw7);

			vst1q_u64((void *)lmt_addr, aw0);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 16), aw1);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 32), aw2);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 48), aw3);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 64), aw4);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 80), aw5);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 96), aw6);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 112), aw7);
			lmt_addr = (uintptr_t)PLT_PTR_ADD(lmt_addr, 128);
		} break;
		case 4: {
			uint64x2_t aw0, aw1, aw2, aw3;
			aw0 = vandq_u64(vld1q_u64((const uint64_t *)&ev[0]),
					aw_mask);
			aw1 = vandq_u64(vld1q_u64((const uint64_t *)&ev[1]),
					aw_mask);
			aw2 = vandq_u64(vld1q_u64((const uint64_t *)&ev[2]),
					aw_mask);
			aw3 = vandq_u64(vld1q_u64((const uint64_t *)&ev[3]),
					aw_mask);

			aw0 = vorrq_u64(vandq_u64(vshrq_n_u64(aw0, 6), tt_mask),
					aw0);
			aw1 = vorrq_u64(vandq_u64(vshrq_n_u64(aw1, 6), tt_mask),
					aw1);
			aw2 = vorrq_u64(vandq_u64(vshrq_n_u64(aw2, 6), tt_mask),
					aw2);
			aw3 = vorrq_u64(vandq_u64(vshrq_n_u64(aw3, 6), tt_mask),
					aw3);

			vst1q_u64((void *)lmt_addr, aw0);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 16), aw1);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 32), aw2);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 48), aw3);
			lmt_addr = (uintptr_t)PLT_PTR_ADD(lmt_addr, 64);
		} break;
		case 2: {
			uint64x2_t aw0, aw1;

			aw0 = vandq_u64(vld1q_u64((const uint64_t *)&ev[0]),
					aw_mask);
			aw1 = vandq_u64(vld1q_u64((const uint64_t *)&ev[1]),
					aw_mask);

			aw0 = vorrq_u64(vandq_u64(vshrq_n_u64(aw0, 6), tt_mask),
					aw0);
			aw1 = vorrq_u64(vandq_u64(vshrq_n_u64(aw1, 6), tt_mask),
					aw1);

			vst1q_u64((void *)lmt_addr, aw0);
			vst1q_u64((void *)PLT_PTR_ADD(lmt_addr, 16), aw1);
			lmt_addr = (uintptr_t)PLT_PTR_ADD(lmt_addr, 32);
		} break;
		case 1: {
			__uint128_t aw0;

			aw0 = ev[0].u64;
			aw0 <<= 64;
			aw0 |= ev[0].event & (BIT_ULL(32) - 1);
			aw0 |= (uint64_t)ev[0].sched_type << 32;

			*((__uint128_t *)lmt_addr) = aw0;
			lmt_addr = (uintptr_t)PLT_PTR_ADD(lmt_addr, 16);
		} break;
		}
		ev += parts;
	}
#else
	uint16_t i;

	for (i = 0; i < burst; i++) {
		__uint128_t aw0;

		aw0 = ev[0].u64;
		aw0 <<= 64;
		aw0 |= ev[0].event & (BIT_ULL(32) - 1);
		aw0 |= (uint64_t)ev[0].sched_type << 32;
		*((__uint128_t *)lmt_addr) = aw0;
		lmt_addr = (uintptr_t)PLT_PTR_ADD(lmt_addr, 16);
	}
#endif

	/* wdata[0] will be always valid */
	sso_lmt_aw_wait_fc(ws, sz0);
	roc_lmt_submit_steorl(wdata[0], pa[0]);
	if (wdata[1]) {
		sso_lmt_aw_wait_fc(ws, sz1);
		roc_lmt_submit_steorl(wdata[1], pa[1]);
	}

	left -= (sz0 + sz1);
	if (left)
		goto again;

	return n;
}

uint16_t __rte_hot
cn10k_sso_hws_enq_burst(void *port, const struct rte_event ev[],
			uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return cn10k_sso_hws_enq(port, ev);
}

uint16_t __rte_hot
cn10k_sso_hws_enq_new_burst(void *port, const struct rte_event ev[],
			    uint16_t nb_events)
{
	uint16_t idx = 0, done = 0, rc = 0;
	struct cn10k_sso_hws *ws = port;
	uint8_t queue_id;
	int32_t space;

	/* Do a common back-pressure check and return */
	space = sso_read_xaq_space(ws) - ROC_SSO_XAE_PER_XAQ;
	if (space <= 0)
		return 0;
	nb_events = space < nb_events ? space : nb_events;

	do {
		queue_id = ev[idx].queue_id;
		for (idx = idx + 1; idx < nb_events; idx++)
			if (queue_id != ev[idx].queue_id)
				break;

		rc = cn10k_sso_hws_new_event_lmtst(ws, queue_id, &ev[done],
						   idx - done);
		if (rc != (idx - done))
			return rc + done;
		done += rc;

	} while (done < nb_events);

	return done;
}

uint16_t __rte_hot
cn10k_sso_hws_enq_fwd_burst(void *port, const struct rte_event ev[],
			    uint16_t nb_events)
{
	struct cn10k_sso_hws *ws = port;

	RTE_SET_USED(nb_events);
	cn10k_sso_hws_forward_event(ws, ev);

	return 1;
}

int __rte_hot
cn10k_sso_hws_profile_switch(void *port, uint8_t profile)
{
	struct cn10k_sso_hws *ws = port;

	ws->gw_wdata &= ~(0xFFUL);
	ws->gw_wdata |= (profile + 1);

	return 0;
}
