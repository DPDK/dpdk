/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_WORKER_DUAL_H__
#define __OTX2_WORKER_DUAL_H__

#include <rte_branch_prediction.h>
#include <rte_common.h>

#include <otx2_common.h>
#include "otx2_evdev.h"

/* SSO Operations */
static __rte_always_inline uint16_t
otx2_ssogws_dual_get_work(struct otx2_ssogws_state *ws,
			  struct otx2_ssogws_state *ws_pair,
			  struct rte_event *ev, const uint32_t flags,
			  const void * const lookup_mem,
			  struct otx2_timesync_info * const tstamp)
{
	const uint64_t set_gw = BIT_ULL(16) | 1;
	union otx2_sso_event event;
	uint64_t tstamp_ptr;
	uint64_t get_work1;
	uint64_t mbuf;

	if (flags & NIX_RX_OFFLOAD_PTYPE_F)
		rte_prefetch_non_temporal(lookup_mem);
#ifdef RTE_ARCH_ARM64
	asm volatile(
			"rty%=:	                             \n"
			"        ldr %[tag], [%[tag_loc]]    \n"
			"        ldr %[wqp], [%[wqp_loc]]    \n"
			"        tbnz %[tag], 63, rty%=      \n"
			"done%=: str %[gw], [%[pong]]        \n"
			"        dmb ld                      \n"
			"        prfm pldl1keep, [%[wqp], #8]\n"
			"        sub %[mbuf], %[wqp], #0x80  \n"
			"        prfm pldl1keep, [%[mbuf]]   \n"
			: [tag] "=&r" (event.get_work0),
			  [wqp] "=&r" (get_work1),
			  [mbuf] "=&r" (mbuf)
			: [tag_loc] "r" (ws->tag_op),
			  [wqp_loc] "r" (ws->wqp_op),
			  [gw] "r" (set_gw),
			  [pong] "r" (ws_pair->getwrk_op)
			);
#else
	event.get_work0 = otx2_read64(ws->tag_op);
	while ((BIT_ULL(63)) & event.get_work0)
		event.get_work0 = otx2_read64(ws->tag_op);
	get_work1 = otx2_read64(ws->wqp_op);
	otx2_write64(set_gw, ws_pair->getwrk_op);

	rte_prefetch0((const void *)get_work1);
	mbuf = (uint64_t)((char *)get_work1 - sizeof(struct rte_mbuf));
	rte_prefetch0((const void *)mbuf);
#endif
	event.get_work0 = (event.get_work0 & (0x3ull << 32)) << 6 |
		(event.get_work0 & (0x3FFull << 36)) << 4 |
		(event.get_work0 & 0xffffffff);
	ws->cur_tt = event.sched_type;
	ws->cur_grp = event.queue_id;

	if (event.sched_type != SSO_TT_EMPTY &&
	    event.event_type == RTE_EVENT_TYPE_ETHDEV) {
		uint8_t port = event.sub_event_type;

		event.sub_event_type = 0;
		otx2_wqe_to_mbuf(get_work1, mbuf, port,
				 event.flow_id, flags, lookup_mem);
		/* Extracting tstamp, if PTP enabled. CGX will prepend the
		 * timestamp at starting of packet data and it can be derieved
		 * from WQE 9 dword which corresponds to SG iova.
		 * rte_pktmbuf_mtod_offset can be used for this purpose but it
		 * brings down the performance as it reads mbuf->buf_addr which
		 * is not part of cache in general fast path.
		 */
		tstamp_ptr = *(uint64_t *)(((struct nix_wqe_hdr_s *)get_work1)
					     + OTX2_SSO_WQE_SG_PTR);
		otx2_nix_mbuf_to_tstamp((struct rte_mbuf *)mbuf, tstamp, flags,
					(uint64_t *)tstamp_ptr);
		get_work1 = mbuf;
	}

	ev->event = event.get_work0;
	ev->u64 = get_work1;

	return !!get_work1;
}

static __rte_always_inline void
otx2_ssogws_dual_add_work(struct otx2_ssogws_dual *ws, const uint64_t event_ptr,
			  const uint32_t tag, const uint8_t new_tt,
			  const uint16_t grp)
{
	uint64_t add_work0;

	add_work0 = tag | ((uint64_t)(new_tt) << 32);
	otx2_store_pair(add_work0, event_ptr, ws->grps_base[grp]);
}

#endif
