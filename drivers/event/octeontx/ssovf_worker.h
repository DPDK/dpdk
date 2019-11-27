/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <rte_common.h>
#include <rte_branch_prediction.h>

#include <octeontx_mbox.h>

#include "ssovf_evdev.h"
#include "octeontx_rxtx.h"

enum {
	SSO_SYNC_ORDERED,
	SSO_SYNC_ATOMIC,
	SSO_SYNC_UNTAGGED,
	SSO_SYNC_EMPTY
};

#ifndef __hot
#define __hot	__attribute__((hot))
#endif

/* SSO Operations */

static __rte_always_inline struct rte_mbuf *
ssovf_octeontx_wqe_to_pkt(uint64_t work, uint16_t port_info)
{
	struct rte_mbuf *mbuf;
	octtx_wqe_t *wqe = (octtx_wqe_t *)(uintptr_t)work;

	/* Get mbuf from wqe */
	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - OCTTX_PACKET_WQE_SKIP);
	rte_prefetch_non_temporal(mbuf);
	mbuf->packet_type =
		ptype_table[wqe->s.w2.lcty][wqe->s.w2.lety][wqe->s.w2.lfty];
	mbuf->data_off = RTE_PTR_DIFF(wqe->s.w3.addr, mbuf->buf_addr);
	mbuf->pkt_len = wqe->s.w1.len;
	mbuf->data_len = mbuf->pkt_len;
	mbuf->nb_segs = 1;
	mbuf->ol_flags = 0;
	mbuf->port = rte_octeontx_pchan_map[port_info >> 4][port_info & 0xF];
	rte_mbuf_refcnt_set(mbuf, 1);

	return mbuf;
}

static __rte_always_inline void
ssovf_octeontx_wqe_free(uint64_t work)
{
	octtx_wqe_t *wqe = (octtx_wqe_t *)(uintptr_t)work;
	struct rte_mbuf *mbuf;

	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - OCTTX_PACKET_WQE_SKIP);
	rte_pktmbuf_free(mbuf);
}

static __rte_always_inline uint16_t
ssows_get_work(struct ssows *ws, struct rte_event *ev)
{
	uint64_t get_work0, get_work1;
	uint64_t sched_type_queue;

	ssovf_load_pair(get_work0, get_work1, ws->getwork);

	sched_type_queue = (get_work0 >> 32) & 0xfff;
	ws->cur_tt = sched_type_queue & 0x3;
	ws->cur_grp = sched_type_queue >> 2;
	sched_type_queue = sched_type_queue << 38;
	ev->event = sched_type_queue | (get_work0 & 0xffffffff);

	if (get_work1 && ev->event_type == RTE_EVENT_TYPE_ETHDEV) {
		ev->mbuf = ssovf_octeontx_wqe_to_pkt(get_work1,
				(ev->event >> 20) & 0x7F);
	} else if (unlikely((get_work0 & 0xFFFFFFFF) == 0xFFFFFFFF)) {
		ssovf_octeontx_wqe_free(get_work1);
		return 0;
	} else {
		ev->u64 = get_work1;
	}

	return !!get_work1;
}

static __rte_always_inline void
ssows_add_work(struct ssows *ws, const uint64_t event_ptr, const uint32_t tag,
			const uint8_t new_tt, const uint8_t grp)
{
	uint64_t add_work0;

	add_work0 = tag | ((uint64_t)(new_tt) << 32);
	ssovf_store_pair(add_work0, event_ptr, ws->grps[grp]);
}

static __rte_always_inline void
ssows_swtag_full(struct ssows *ws, const uint64_t event_ptr, const uint32_t tag,
			const uint8_t new_tt, const uint8_t grp)
{
	uint64_t swtag_full0;

	swtag_full0 = tag | ((uint64_t)(new_tt & 0x3) << 32) |
				((uint64_t)grp << 34);
	ssovf_store_pair(swtag_full0, event_ptr, (ws->base +
				SSOW_VHWS_OP_SWTAG_FULL0));
}

static __rte_always_inline void
ssows_swtag_desched(struct ssows *ws, uint32_t tag, uint8_t new_tt, uint8_t grp)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32) | ((uint64_t)grp << 34);
	ssovf_write64(val, ws->base + SSOW_VHWS_OP_SWTAG_DESCHED);
}

static __rte_always_inline void
ssows_swtag_norm(struct ssows *ws, uint32_t tag, uint8_t new_tt)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32);
	ssovf_write64(val, ws->base + SSOW_VHWS_OP_SWTAG_NORM);
}

static __rte_always_inline void
ssows_swtag_untag(struct ssows *ws)
{
	ssovf_write64(0, ws->base + SSOW_VHWS_OP_SWTAG_UNTAG);
	ws->cur_tt = SSO_SYNC_UNTAGGED;
}

static __rte_always_inline void
ssows_upd_wqp(struct ssows *ws, uint8_t grp, uint64_t event_ptr)
{
	ssovf_store_pair((uint64_t)grp << 34, event_ptr, (ws->base +
				SSOW_VHWS_OP_UPD_WQP_GRP0));
}

static __rte_always_inline void
ssows_desched(struct ssows *ws)
{
	ssovf_write64(0, ws->base + SSOW_VHWS_OP_DESCHED);
}

static __rte_always_inline void
ssows_swtag_wait(struct ssows *ws)
{
	/* Wait for the SWTAG/SWTAG_FULL operation */
	while (ssovf_read64(ws->base + SSOW_VHWS_SWTP))
	;
}
