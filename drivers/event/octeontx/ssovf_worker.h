/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <arpa/inet.h>

#ifndef _SSOVF_WORKER_H_
#define _SSOVF_WORKER_H_

#include <rte_common.h>
#include <rte_branch_prediction.h>

#include <octeontx_mbox.h>

#include "ssovf_evdev.h"
#include "octeontx_rxtx.h"
#include "otx_cryptodev_ops.h"

/* Alignment */
#define OCCTX_ALIGN  128

/* Fastpath lookup */
#define OCCTX_FASTPATH_LOOKUP_MEM	"octeontx_fastpath_lookup_mem"

/* WQE's ERRCODE + ERRLEV (11 bits) */
#define ERRCODE_ERRLEN_WIDTH		11
#define ERR_ARRAY_SZ			((BIT(ERRCODE_ERRLEN_WIDTH)) *\
					sizeof(uint32_t))

#define LOOKUP_ARRAY_SZ			(ERR_ARRAY_SZ)

#define OCCTX_EC_IP4_NOT		0x41
#define OCCTX_EC_IP4_CSUM		0x42
#define OCCTX_EC_L4_CSUM		0x62

enum OCCTX_ERRLEV_E {
	OCCTX_ERRLEV_RE = 0,
	OCCTX_ERRLEV_LA = 1,
	OCCTX_ERRLEV_LB = 2,
	OCCTX_ERRLEV_LC = 3,
	OCCTX_ERRLEV_LD = 4,
	OCCTX_ERRLEV_LE = 5,
	OCCTX_ERRLEV_LF = 6,
	OCCTX_ERRLEV_LG = 7,
};

enum {
	SSO_SYNC_ORDERED,
	SSO_SYNC_ATOMIC,
	SSO_SYNC_UNTAGGED,
	SSO_SYNC_EMPTY
};

/* SSO Operations */

static __rte_always_inline uint32_t
ssovf_octeontx_rx_olflags_get(const void * const lookup_mem, const uint64_t in)
{
	const uint32_t * const ol_flags = (const uint32_t *)lookup_mem;

	return ol_flags[(in & 0x7ff)];
}

static __rte_always_inline void
ssovf_octeontx_wqe_xtract_mseg(octtx_wqe_t *wqe,
			       struct rte_mbuf *mbuf)
{
	octtx_pki_buflink_t *buflink;
	rte_iova_t *iova_list;
	uint8_t nb_segs;
	uint64_t bytes_left = wqe->s.w1.len - wqe->s.w5.size;

	nb_segs = wqe->s.w0.bufs;

	buflink = (octtx_pki_buflink_t *)((uintptr_t)wqe->s.w3.addr -
					  sizeof(octtx_pki_buflink_t));

	while (--nb_segs) {
		iova_list = (rte_iova_t *)(uintptr_t)(buflink->w1.s.addr);
		mbuf->next = (struct rte_mbuf *)(rte_iova_t *)(iova_list - 2)
			      - (OCTTX_PACKET_LATER_SKIP / 128);
		mbuf = mbuf->next;

		mbuf->data_off = sizeof(octtx_pki_buflink_t);

		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);
		if (nb_segs == 1)
			mbuf->data_len = bytes_left;
		else
			mbuf->data_len = buflink->w0.s.size;

		bytes_left = bytes_left - buflink->w0.s.size;
		buflink = (octtx_pki_buflink_t *)(rte_iova_t *)(iova_list - 2);

	}
}

static __rte_always_inline struct rte_mbuf *
ssovf_octeontx_wqe_to_pkt(uint64_t work, uint16_t port_info,
			  const uint16_t flag, const void *lookup_mem)
{
	struct rte_mbuf *mbuf;
	octtx_wqe_t *wqe = (octtx_wqe_t *)(uintptr_t)work;

	/* Get mbuf from wqe */
	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - OCTTX_PACKET_WQE_SKIP);
	rte_prefetch_non_temporal(mbuf);
	mbuf->packet_type =
		ptype_table[wqe->s.w2.lcty][wqe->s.w2.lety][wqe->s.w2.lfty];
	mbuf->data_off = RTE_PTR_DIFF(wqe->s.w3.addr, mbuf->buf_addr);
	mbuf->ol_flags = 0;
	mbuf->pkt_len = wqe->s.w1.len;

	if (!!(flag & OCCTX_RX_OFFLOAD_CSUM_F))
		mbuf->ol_flags = ssovf_octeontx_rx_olflags_get(lookup_mem,
							       wqe->w[2]);

	if (!!(flag & OCCTX_RX_MULTI_SEG_F)) {
		mbuf->nb_segs = wqe->s.w0.bufs;
		mbuf->data_len = wqe->s.w5.size;
		ssovf_octeontx_wqe_xtract_mseg(wqe, mbuf);
	} else {
		mbuf->nb_segs = 1;
		mbuf->data_len = mbuf->pkt_len;
	}

	if (!!(flag & OCCTX_RX_VLAN_FLTR_F)) {
		if (likely(wqe->s.w2.vv)) {
			mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN;
			mbuf->vlan_tci =
				ntohs(*((uint16_t *)((char *)mbuf->buf_addr +
					mbuf->data_off + wqe->s.w4.vlptr + 2)));
		}
	}

	mbuf->port = rte_octeontx_pchan_map[port_info >> 4][port_info & 0xF];
	rte_mbuf_refcnt_set(mbuf, 1);

	return mbuf;
}

static __rte_always_inline void
ssovf_octeontx_wqe_free(uint64_t work)
{
	octtx_wqe_t *wqe = (octtx_wqe_t *)(uintptr_t)work;
	uint8_t nb_segs = wqe->s.w0.bufs;
	octtx_pki_buflink_t *buflink;
	struct rte_mbuf *mbuf, *head;
	rte_iova_t *iova_list;

	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - OCTTX_PACKET_WQE_SKIP);
	buflink = (octtx_pki_buflink_t *)((uintptr_t)wqe->s.w3.addr -
					  sizeof(octtx_pki_buflink_t));
	head = mbuf;
	while (--nb_segs) {
		iova_list = (rte_iova_t *)(uintptr_t)(buflink->w1.s.addr);
		mbuf = (struct rte_mbuf *)(rte_iova_t *)(iova_list - 2)
			- (OCTTX_PACKET_LATER_SKIP / 128);

		mbuf->next = NULL;
		rte_pktmbuf_free(mbuf);
		buflink = (octtx_pki_buflink_t *)(rte_iova_t *)(iova_list - 2);
	}
	rte_pktmbuf_free(head);
}

static __rte_always_inline uint16_t
ssows_get_work(struct ssows *ws, struct rte_event *ev, const uint16_t flag)
{
	uint64_t get_work0, get_work1;
	uint64_t sched_type_queue;

	ssovf_load_pair(get_work0, get_work1, ws->getwork);

	sched_type_queue = (get_work0 >> 32) & 0xfff;
	ws->cur_tt = sched_type_queue & 0x3;
	ws->cur_grp = sched_type_queue >> 2;
	sched_type_queue = sched_type_queue << 38;
	ev->event = sched_type_queue | (get_work0 & 0xffffffff);

	if (get_work1) {
		if (ev->event_type == RTE_EVENT_TYPE_ETHDEV)
			get_work1 = (uintptr_t)ssovf_octeontx_wqe_to_pkt(
				get_work1, (ev->event >> 20) & 0x7F, flag,
				ws->lookup_mem);
		else if (ev->event_type == RTE_EVENT_TYPE_CRYPTODEV)
			get_work1 = otx_crypto_adapter_dequeue(get_work1);
		ev->u64 = get_work1;
	} else if (unlikely((get_work0 & 0xFFFFFFFF) == 0xFFFFFFFF)) {
		ssovf_octeontx_wqe_free(get_work1);
		return 0;
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

static __rte_always_inline void
ssows_head_wait(struct ssows *ws)
{
	while (!(ssovf_read64(ws->base + SSOW_VHWS_TAG) & (1ULL << 35)))
		;
}
#endif /* _SSOVF_WORKER_H_ */
