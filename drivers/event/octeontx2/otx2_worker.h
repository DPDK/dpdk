/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_WORKER_H__
#define __OTX2_WORKER_H__

#include <rte_common.h>
#include <rte_branch_prediction.h>

#include <otx2_common.h>
#include "otx2_evdev.h"

/* SSO Operations */

static __rte_always_inline uint16_t
otx2_ssogws_get_work(struct otx2_ssogws *ws, struct rte_event *ev)
{
	union otx2_sso_event event;
	uint64_t get_work1;

	otx2_write64(BIT_ULL(16) | /* wait for work. */
		     1, /* Use Mask set 0. */
		     ws->getwrk_op);

#ifdef RTE_ARCH_ARM64
	asm volatile(
			"		ldr %[tag], [%[tag_loc]]	\n"
			"		ldr %[wqp], [%[wqp_loc]]	\n"
			"		tbz %[tag], 63, done%=		\n"
			"		sevl				\n"
			"rty%=:		wfe				\n"
			"		ldr %[tag], [%[tag_loc]]	\n"
			"		ldr %[wqp], [%[wqp_loc]]	\n"
			"		tbnz %[tag], 63, rty%=		\n"
			"done%=:	dmb ld				\n"
			"		prfm pldl1keep, [%[wqp]]	\n"
			: [tag] "=&r" (event.get_work0),
			  [wqp] "=&r" (get_work1)
			: [tag_loc] "r" (ws->tag_op),
			  [wqp_loc] "r" (ws->wqp_op)
			);
#else
	event.get_work0 = otx2_read64(ws->tag_op);
	while ((BIT_ULL(63)) & event.get_work0)
		event.get_work0 = otx2_read64(ws->tag_op);

	get_work1 = otx2_read64(ws->wqp_op);
	rte_prefetch0((const void *)get_work1);
#endif

	event.get_work0 = (event.get_work0 & (0x3ull << 32)) << 6 |
		(event.get_work0 & (0x3FFull << 36)) << 4 |
		(event.get_work0 & 0xffffffff);
	ws->cur_tt = event.sched_type;
	ws->cur_grp = event.queue_id;


	ev->event = event.get_work0;
	ev->u64 = get_work1;

	return !!get_work1;
}

/* Used in cleaning up workslot. */
static __rte_always_inline uint16_t
otx2_ssogws_get_work_empty(struct otx2_ssogws *ws, struct rte_event *ev)
{
	union otx2_sso_event event;
	uint64_t get_work1;

#ifdef RTE_ARCH_ARM64
	asm volatile(
			"		ldr %[tag], [%[tag_loc]]	\n"
			"		ldr %[wqp], [%[wqp_loc]]	\n"
			"		tbz %[tag], 63, done%=		\n"
			"		sevl				\n"
			"rty%=:		wfe				\n"
			"		ldr %[tag], [%[tag_loc]]	\n"
			"		ldr %[wqp], [%[wqp_loc]]	\n"
			"		tbnz %[tag], 63, rty%=		\n"
			"done%=:	dmb ld				\n"
			"		prfm pldl1keep, [%[wqp]]	\n"
			: [tag] "=&r" (event.get_work0),
			  [wqp] "=&r" (get_work1)
			: [tag_loc] "r" (ws->tag_op),
			  [wqp_loc] "r" (ws->wqp_op)
			);
#else
	event.get_work0 = otx2_read64(ws->tag_op);
	while ((BIT_ULL(63)) & event.get_work0)
		event.get_work0 = otx2_read64(ws->tag_op);

	get_work1 = otx2_read64(ws->wqp_op);
	rte_prefetch0((const void *)get_work1);
#endif

	event.get_work0 = (event.get_work0 & (0x3ull << 32)) << 6 |
		(event.get_work0 & (0x3FFull << 36)) << 4 |
		(event.get_work0 & 0xffffffff);
	ws->cur_tt = event.sched_type;
	ws->cur_grp = event.queue_id;

	ev->event = event.get_work0;
	ev->u64 = get_work1;

	return !!get_work1;
}

static __rte_always_inline void
otx2_ssogws_add_work(struct otx2_ssogws *ws, const uint64_t event_ptr,
		     const uint32_t tag, const uint8_t new_tt,
		     const uint16_t grp)
{
	uint64_t add_work0;

	add_work0 = tag | ((uint64_t)(new_tt) << 32);
	otx2_store_pair(add_work0, event_ptr, ws->grps_base[grp]);
}

static __rte_always_inline void
otx2_ssogws_swtag_desched(struct otx2_ssogws *ws, uint32_t tag, uint8_t new_tt,
			  uint16_t grp)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32) | ((uint64_t)grp << 34);
	otx2_write64(val, ws->swtag_desched_op);
}

static __rte_always_inline void
otx2_ssogws_swtag_norm(struct otx2_ssogws *ws, uint32_t tag, uint8_t new_tt)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32);
	otx2_write64(val, ws->swtag_norm_op);
}

static __rte_always_inline void
otx2_ssogws_swtag_untag(struct otx2_ssogws *ws)
{
	otx2_write64(0, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_SWTAG_UNTAG);
	ws->cur_tt = SSO_SYNC_UNTAGGED;
}

static __rte_always_inline void
otx2_ssogws_swtag_flush(struct otx2_ssogws *ws)
{
	otx2_write64(0, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_SWTAG_FLUSH);
	ws->cur_tt = SSO_SYNC_EMPTY;
}

static __rte_always_inline void
otx2_ssogws_desched(struct otx2_ssogws *ws)
{
	otx2_write64(0, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_DESCHED);
}

static __rte_always_inline void
otx2_ssogws_swtag_wait(struct otx2_ssogws *ws)
{
#ifdef RTE_ARCH_ARM64
	uint64_t swtp;

	asm volatile (
			"	ldr %[swtb], [%[swtp_loc]]	\n"
			"	cbz %[swtb], done%=		\n"
			"	sevl				\n"
			"rty%=:	wfe				\n"
			"	ldr %[swtb], [%[swtp_loc]]	\n"
			"	cbnz %[swtb], rty%=		\n"
			"done%=:				\n"
			: [swtb] "=&r" (swtp)
			: [swtp_loc] "r" (ws->swtp_op)
			);
#else
	/* Wait for the SWTAG/SWTAG_FULL operation */
	while (otx2_read64(ws->swtp_op))
		;
#endif
}

#endif
