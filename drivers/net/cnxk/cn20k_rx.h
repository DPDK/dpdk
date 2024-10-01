/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_RX_H__
#define __CN20K_RX_H__

#include "cn20k_rxtx.h"
#include <rte_ethdev.h>
#include <rte_security_driver.h>
#include <rte_vect.h>

#define NSEC_PER_SEC 1000000000L

#define NIX_RX_OFFLOAD_NONE	     (0)
#define NIX_RX_OFFLOAD_RSS_F	     BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F	     BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F    BIT(2)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F BIT(3)
#define NIX_RX_OFFLOAD_TSTAMP_F	     BIT(4)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F  BIT(5)
#define NIX_RX_OFFLOAD_SECURITY_F    BIT(6)
#define NIX_RX_OFFLOAD_MAX	     (NIX_RX_OFFLOAD_SECURITY_F << 1)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_REAS_F	   BIT(12)
#define NIX_RX_VWQE_F	   BIT(13)
#define NIX_RX_MULTI_SEG_F BIT(14)

#define CNXK_NIX_CQ_ENTRY_SZ 128
#define NIX_DESCS_PER_LOOP   4
#define CQE_CAST(x)	     ((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

#define CQE_PTR_OFF(b, i, o, f)                                                                    \
	(((f) & NIX_RX_VWQE_F) ? (uint64_t *)(((uintptr_t)((uint64_t *)(b))[i]) + (o)) :           \
				 (uint64_t *)(((uintptr_t)(b)) + CQE_SZ(i) + (o)))
#define CQE_PTR_DIFF(b, i, o, f)                                                                   \
	(((f) & NIX_RX_VWQE_F) ? (uint64_t *)(((uintptr_t)((uint64_t *)(b))[i]) - (o)) :           \
				 (uint64_t *)(((uintptr_t)(b)) + CQE_SZ(i) - (o)))

#define NIX_RX_SEC_UCC_CONST                                                                       \
	((RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1) |                                                       \
	 ((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1) << 8 |                 \
	 ((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1) << 16 |                 \
	 ((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1) << 32 |                \
	 ((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1) << 48)

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
static inline void
nix_mbuf_validate_next(struct rte_mbuf *m)
{
	if (m->nb_segs == 1 && m->next)
		rte_panic("mbuf->next[%p] valid when mbuf->nb_segs is %d", m->next, m->nb_segs);
}
#else
static inline void
nix_mbuf_validate_next(struct rte_mbuf *m)
{
	RTE_SET_USED(m);
}
#endif

#define NIX_RX_SEC_REASSEMBLY_F (NIX_RX_REAS_F | NIX_RX_OFFLOAD_SECURITY_F)

static inline rte_eth_ip_reassembly_dynfield_t *
cnxk_ip_reassembly_dynfield(struct rte_mbuf *mbuf, int ip_reassembly_dynfield_offset)
{
	return RTE_MBUF_DYNFIELD(mbuf, ip_reassembly_dynfield_offset,
				 rte_eth_ip_reassembly_dynfield_t *);
}

union mbuf_initializer {
	struct {
		uint16_t data_off;
		uint16_t refcnt;
		uint16_t nb_segs;
		uint16_t port;
	} fields;
	uint64_t value;
};

static __rte_always_inline uint64_t
nix_clear_data_off(uint64_t oldval)
{
	union mbuf_initializer mbuf_init = {.value = oldval};

	mbuf_init.fields.data_off = 0;
	return mbuf_init.value;
}

static __rte_always_inline struct rte_mbuf *
nix_get_mbuf_from_cqe(void *cq, const uint64_t data_off)
{
	rte_iova_t buff;

	/* Skip CQE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
	buff = *((rte_iova_t *)((uint64_t *)cq + 9));
	return (struct rte_mbuf *)(buff - data_off);
}

static __rte_always_inline uint32_t
nix_ptype_get(const void *const lookup_mem, const uint64_t in)
{
	const uint16_t *const ptype = lookup_mem;
	const uint16_t lh_lg_lf = (in & 0xFFF0000000000000) >> 52;
	const uint16_t tu_l2 = ptype[(in & 0x000FFFF000000000) >> 36];
	const uint16_t il4_tu = ptype[PTYPE_NON_TUNNEL_ARRAY_SZ + lh_lg_lf];

	return (il4_tu << PTYPE_NON_TUNNEL_WIDTH) | tu_l2;
}

static __rte_always_inline uint32_t
nix_rx_olflags_get(const void *const lookup_mem, const uint64_t in)
{
	const uint32_t *const ol_flags =
		(const uint32_t *)((const uint8_t *)lookup_mem + PTYPE_ARRAY_SZ);

	return ol_flags[(in & 0xfff00000) >> 20];
}

static inline uint64_t
nix_update_match_id(const uint16_t match_id, uint64_t ol_flags, struct rte_mbuf *mbuf)
{
	/* There is no separate bit to check match_id
	 * is valid or not? and no flag to identify it is an
	 * RTE_FLOW_ACTION_TYPE_FLAG vs RTE_FLOW_ACTION_TYPE_MARK
	 * action. The former case addressed through 0 being invalid
	 * value and inc/dec match_id pair when MARK is activated.
	 * The later case addressed through defining
	 * CNXK_FLOW_MARK_DEFAULT as value for
	 * RTE_FLOW_ACTION_TYPE_MARK.
	 * This would translate to not use
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT - 1 and
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT for match_id.
	 * i.e valid mark_id's are from
	 * 0 to CNXK_FLOW_ACTION_FLAG_DEFAULT - 2
	 */
	if (likely(match_id)) {
		ol_flags |= RTE_MBUF_F_RX_FDIR;
		if (match_id != CNXK_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= RTE_MBUF_F_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const union nix_rx_parse_u *rx, struct rte_mbuf *mbuf, uint64_t rearm,
		    uintptr_t cpth, uintptr_t sa_base, const uint16_t flags)
{
	const rte_iova_t *iova_list;
	uint16_t later_skip = 0;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint16_t sg_len;
	int64_t len;
	uint64_t sg;
	uintptr_t p;

	(void)cpth;
	(void)sa_base;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;

	if (nb_segs == 1)
		return;

	len = rx->pkt_lenm1 + 1;

	mbuf->pkt_len = len - (flags & NIX_RX_OFFLOAD_TSTAMP_F ? CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	mbuf->nb_segs = nb_segs;
	head = mbuf;
	mbuf->data_len =
		(sg & 0xFFFF) - (flags & NIX_RX_OFFLOAD_TSTAMP_F ? CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	eol = ((const rte_iova_t *)(rx + 1) + ((rx->desc_sizem1 + 1) << 1));

	len -= mbuf->data_len;
	sg = sg >> 16;
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	later_skip = (uintptr_t)mbuf->buf_addr - (uintptr_t)mbuf;

	while (nb_segs) {
		mbuf->next = (struct rte_mbuf *)(*iova_list - later_skip);
		mbuf = mbuf->next;

		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		sg_len = sg & 0XFFFF;

		mbuf->data_len = sg_len;
		sg = sg >> 16;
		p = (uintptr_t)&mbuf->rearm_data;
		*(uint64_t *)p = rearm & ~0xFFFF;
		nb_segs--;
		iova_list++;

		if (!nb_segs && (iova_list + 1 < eol)) {
			sg = *(const uint64_t *)(iova_list);
			nb_segs = (sg >> 48) & 0x3;
			head->nb_segs += nb_segs;
			iova_list = (const rte_iova_t *)(iova_list + 1);
		}
	}
}

static __rte_always_inline void
cn20k_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag, struct rte_mbuf *mbuf,
		      const void *lookup_mem, const uint64_t val, const uintptr_t cpth,
		      const uintptr_t sa_base, const uint16_t flag)
{
	const union nix_rx_parse_u *rx = (const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	const uint64_t w1 = *(const uint64_t *)rx;
	uint16_t len = rx->pkt_lenm1 + 1;
	uint64_t ol_flags = 0;
	uintptr_t p;

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = nix_ptype_get(lookup_mem, w1);
	else
		mbuf->packet_type = 0;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}

	/* Skip rx ol flags extraction for Security packets */
	ol_flags |= (uint64_t)nix_rx_olflags_get(lookup_mem, w1);

	if (flag & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
		if (rx->vtag0_gone) {
			ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
			mbuf->vlan_tci = rx->vtag0_tci;
		}
		if (rx->vtag1_gone) {
			ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
			mbuf->vlan_tci_outer = rx->vtag1_tci;
		}
	}

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags = nix_update_match_id(rx->match_id, ol_flags, mbuf);

	mbuf->ol_flags = ol_flags;
	mbuf->pkt_len = len;
	mbuf->data_len = len;
	p = (uintptr_t)&mbuf->rearm_data;
	*(uint64_t *)p = val;

	if (flag & NIX_RX_MULTI_SEG_F)
		/*
		 * For multi segment packets, mbuf length correction according
		 * to Rx timestamp length will be handled later during
		 * timestamp data process.
		 * Hence, timestamp flag argument is not required.
		 */
		nix_cqe_xtract_mseg(rx, mbuf, val, cpth, sa_base, flag & ~NIX_RX_OFFLOAD_TSTAMP_F);
}

static inline uint16_t
nix_rx_nb_pkts(struct cn20k_eth_rxq *rxq, const uint64_t wdata, const uint16_t pkts,
	       const uint32_t qmask)
{
	uint32_t available = rxq->available;

	/* Update the available count if cached value is not enough */
	if (unlikely(available < pkts)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, rxq->cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) || reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
			return 0;

		tail = reg & 0xFFFFF;
		head = (reg >> 20) & 0xFFFFF;
		if (tail < head)
			available = tail - head + qmask + 1;
		else
			available = tail - head;

		rxq->available = available;
	}

	return RTE_MIN(pkts, available);
}

static __rte_always_inline void
cn20k_nix_mbuf_to_tstamp(struct rte_mbuf *mbuf, struct cnxk_timesync_info *tstamp,
			 const uint8_t ts_enable, uint64_t *tstamp_ptr)
{
	if (ts_enable) {
		mbuf->pkt_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;
		mbuf->data_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;

		/* Reading the rx timestamp inserted by CGX, viz at
		 * starting of the packet data.
		 */
		*tstamp_ptr = ((*tstamp_ptr >> 32) * NSEC_PER_SEC) + (*tstamp_ptr & 0xFFFFFFFFUL);
		*cnxk_nix_timestamp_dynfield(mbuf, tstamp) = rte_be_to_cpu_64(*tstamp_ptr);
		/* RTE_MBUF_F_RX_IEEE1588_TMST flag needs to be set only in case
		 * PTP packets are received.
		 */
		if (mbuf->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC) {
			tstamp->rx_tstamp = *cnxk_nix_timestamp_dynfield(mbuf, tstamp);
			tstamp->rx_ready = 1;
			mbuf->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP | RTE_MBUF_F_RX_IEEE1588_TMST |
					  tstamp->rx_tstamp_dynflag;
		}
	}
}

static __rte_always_inline uint16_t
cn20k_nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts, const uint16_t flags)
{
	struct cn20k_eth_rxq *rxq = rx_queue;
	const uint64_t mbuf_init = rxq->mbuf_initializer;
	const void *lookup_mem = rxq->lookup_mem;
	const uint64_t data_off = rxq->data_off;
	const uintptr_t desc = rxq->desc;
	const uint64_t wdata = rxq->wdata;
	const uint32_t qmask = rxq->qmask;
	uint16_t packets = 0, nb_pkts;
	uint32_t head = rxq->head;
	struct nix_cqe_hdr_s *cq;
	struct rte_mbuf *mbuf;
	uint64_t sa_base = 0;
	uintptr_t cpth = 0;

	nb_pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);

	while (packets < nb_pkts) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		cn20k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init, cpth, sa_base,
				      flags);
		cn20k_nix_mbuf_to_tstamp(mbuf, rxq->tstamp, (flags & NIX_RX_OFFLOAD_TSTAMP_F),
					 (uint64_t *)((uint8_t *)mbuf + data_off));
		rx_pkts[packets++] = mbuf;
		roc_prefetch_store_keep(mbuf);
		head++;
		head &= qmask;
	}

	rxq->head = head;
	rxq->available -= nb_pkts;

	/* Free all the CQs that we've processed */
	plt_write64((wdata | nb_pkts), rxq->cq_door);

	return nb_pkts;
}

static __rte_always_inline uint16_t
cn20k_nix_flush_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts,
			  const uint16_t flags)
{
	struct cn20k_eth_rxq *rxq = rx_queue;
	const uint64_t mbuf_init = rxq->mbuf_initializer;
	const void *lookup_mem = rxq->lookup_mem;
	const uint64_t data_off = rxq->data_off;
	const uint64_t wdata = rxq->wdata;
	const uint32_t qmask = rxq->qmask;
	const uintptr_t desc = rxq->desc;
	uint16_t packets = 0, nb_pkts;
	uint16_t lmt_id __rte_unused;
	uint32_t head = rxq->head;
	struct nix_cqe_hdr_s *cq;
	struct rte_mbuf *mbuf;
	uint64_t sa_base = 0;
	uintptr_t cpth = 0;

	nb_pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);

	while (packets < nb_pkts) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		cn20k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init, cpth, sa_base,
				      flags);
		cn20k_nix_mbuf_to_tstamp(mbuf, rxq->tstamp, (flags & NIX_RX_OFFLOAD_TSTAMP_F),
					 (uint64_t *)((uint8_t *)mbuf + data_off));
		rx_pkts[packets++] = mbuf;
		roc_prefetch_store_keep(mbuf);
		head++;
		head &= qmask;
	}

	rxq->head = head;
	rxq->available -= nb_pkts;

	/* Free all the CQs that we've processed */
	plt_write64((wdata | nb_pkts), rxq->cq_door);

	return nb_pkts;
}

#define RSS_F	  NIX_RX_OFFLOAD_RSS_F
#define PTYPE_F	  NIX_RX_OFFLOAD_PTYPE_F
#define CKSUM_F	  NIX_RX_OFFLOAD_CHECKSUM_F
#define MARK_F	  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F	  NIX_RX_OFFLOAD_TSTAMP_F
#define RX_VLAN_F NIX_RX_OFFLOAD_VLAN_STRIP_F
#define R_SEC_F	  NIX_RX_OFFLOAD_SECURITY_F

/* [R_SEC_F] [RX_VLAN_F] [TS] [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES_0_15                                                                 \
	R(no_offload, NIX_RX_OFFLOAD_NONE)                                                         \
	R(rss, RSS_F)                                                                              \
	R(ptype, PTYPE_F)                                                                          \
	R(ptype_rss, PTYPE_F | RSS_F)                                                              \
	R(cksum, CKSUM_F)                                                                          \
	R(cksum_rss, CKSUM_F | RSS_F)                                                              \
	R(cksum_ptype, CKSUM_F | PTYPE_F)                                                          \
	R(cksum_ptype_rss, CKSUM_F | PTYPE_F | RSS_F)                                              \
	R(mark, MARK_F)                                                                            \
	R(mark_rss, MARK_F | RSS_F)                                                                \
	R(mark_ptype, MARK_F | PTYPE_F)                                                            \
	R(mark_ptype_rss, MARK_F | PTYPE_F | RSS_F)                                                \
	R(mark_cksum, MARK_F | CKSUM_F)                                                            \
	R(mark_cksum_rss, MARK_F | CKSUM_F | RSS_F)                                                \
	R(mark_cksum_ptype, MARK_F | CKSUM_F | PTYPE_F)                                            \
	R(mark_cksum_ptype_rss, MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_16_31                                                                \
	R(ts, TS_F)                                                                                \
	R(ts_rss, TS_F | RSS_F)                                                                    \
	R(ts_ptype, TS_F | PTYPE_F)                                                                \
	R(ts_ptype_rss, TS_F | PTYPE_F | RSS_F)                                                    \
	R(ts_cksum, TS_F | CKSUM_F)                                                                \
	R(ts_cksum_rss, TS_F | CKSUM_F | RSS_F)                                                    \
	R(ts_cksum_ptype, TS_F | CKSUM_F | PTYPE_F)                                                \
	R(ts_cksum_ptype_rss, TS_F | CKSUM_F | PTYPE_F | RSS_F)                                    \
	R(ts_mark, TS_F | MARK_F)                                                                  \
	R(ts_mark_rss, TS_F | MARK_F | RSS_F)                                                      \
	R(ts_mark_ptype, TS_F | MARK_F | PTYPE_F)                                                  \
	R(ts_mark_ptype_rss, TS_F | MARK_F | PTYPE_F | RSS_F)                                      \
	R(ts_mark_cksum, TS_F | MARK_F | CKSUM_F)                                                  \
	R(ts_mark_cksum_rss, TS_F | MARK_F | CKSUM_F | RSS_F)                                      \
	R(ts_mark_cksum_ptype, TS_F | MARK_F | CKSUM_F | PTYPE_F)                                  \
	R(ts_mark_cksum_ptype_rss, TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_32_47                                                                \
	R(vlan, RX_VLAN_F)                                                                         \
	R(vlan_rss, RX_VLAN_F | RSS_F)                                                             \
	R(vlan_ptype, RX_VLAN_F | PTYPE_F)                                                         \
	R(vlan_ptype_rss, RX_VLAN_F | PTYPE_F | RSS_F)                                             \
	R(vlan_cksum, RX_VLAN_F | CKSUM_F)                                                         \
	R(vlan_cksum_rss, RX_VLAN_F | CKSUM_F | RSS_F)                                             \
	R(vlan_cksum_ptype, RX_VLAN_F | CKSUM_F | PTYPE_F)                                         \
	R(vlan_cksum_ptype_rss, RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)                             \
	R(vlan_mark, RX_VLAN_F | MARK_F)                                                           \
	R(vlan_mark_rss, RX_VLAN_F | MARK_F | RSS_F)                                               \
	R(vlan_mark_ptype, RX_VLAN_F | MARK_F | PTYPE_F)                                           \
	R(vlan_mark_ptype_rss, RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                               \
	R(vlan_mark_cksum, RX_VLAN_F | MARK_F | CKSUM_F)                                           \
	R(vlan_mark_cksum_rss, RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                               \
	R(vlan_mark_cksum_ptype, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)                           \
	R(vlan_mark_cksum_ptype_rss, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_48_63                                                                \
	R(vlan_ts, RX_VLAN_F | TS_F)                                                               \
	R(vlan_ts_rss, RX_VLAN_F | TS_F | RSS_F)                                                   \
	R(vlan_ts_ptype, RX_VLAN_F | TS_F | PTYPE_F)                                               \
	R(vlan_ts_ptype_rss, RX_VLAN_F | TS_F | PTYPE_F | RSS_F)                                   \
	R(vlan_ts_cksum, RX_VLAN_F | TS_F | CKSUM_F)                                               \
	R(vlan_ts_cksum_rss, RX_VLAN_F | TS_F | CKSUM_F | RSS_F)                                   \
	R(vlan_ts_cksum_ptype, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                               \
	R(vlan_ts_cksum_ptype_rss, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                   \
	R(vlan_ts_mark, RX_VLAN_F | TS_F | MARK_F)                                                 \
	R(vlan_ts_mark_rss, RX_VLAN_F | TS_F | MARK_F | RSS_F)                                     \
	R(vlan_ts_mark_ptype, RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                                 \
	R(vlan_ts_mark_ptype_rss, RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)                     \
	R(vlan_ts_mark_cksum, RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                                 \
	R(vlan_ts_mark_cksum_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)                     \
	R(vlan_ts_mark_cksum_ptype, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                 \
	R(vlan_ts_mark_cksum_ptype_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_64_79                                                                \
	R(sec, R_SEC_F)                                                                            \
	R(sec_rss, R_SEC_F | RSS_F)                                                                \
	R(sec_ptype, R_SEC_F | PTYPE_F)                                                            \
	R(sec_ptype_rss, R_SEC_F | PTYPE_F | RSS_F)                                                \
	R(sec_cksum, R_SEC_F | CKSUM_F)                                                            \
	R(sec_cksum_rss, R_SEC_F | CKSUM_F | RSS_F)                                                \
	R(sec_cksum_ptype, R_SEC_F | CKSUM_F | PTYPE_F)                                            \
	R(sec_cksum_ptype_rss, R_SEC_F | CKSUM_F | PTYPE_F | RSS_F)                                \
	R(sec_mark, R_SEC_F | MARK_F)                                                              \
	R(sec_mark_rss, R_SEC_F | MARK_F | RSS_F)                                                  \
	R(sec_mark_ptype, R_SEC_F | MARK_F | PTYPE_F)                                              \
	R(sec_mark_ptype_rss, R_SEC_F | MARK_F | PTYPE_F | RSS_F)                                  \
	R(sec_mark_cksum, R_SEC_F | MARK_F | CKSUM_F)                                              \
	R(sec_mark_cksum_rss, R_SEC_F | MARK_F | CKSUM_F | RSS_F)                                  \
	R(sec_mark_cksum_ptype, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F)                              \
	R(sec_mark_cksum_ptype_rss, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_80_95                                                                \
	R(sec_ts, R_SEC_F | TS_F)                                                                  \
	R(sec_ts_rss, R_SEC_F | TS_F | RSS_F)                                                      \
	R(sec_ts_ptype, R_SEC_F | TS_F | PTYPE_F)                                                  \
	R(sec_ts_ptype_rss, R_SEC_F | TS_F | PTYPE_F | RSS_F)                                      \
	R(sec_ts_cksum, R_SEC_F | TS_F | CKSUM_F)                                                  \
	R(sec_ts_cksum_rss, R_SEC_F | TS_F | CKSUM_F | RSS_F)                                      \
	R(sec_ts_cksum_ptype, R_SEC_F | TS_F | CKSUM_F | PTYPE_F)                                  \
	R(sec_ts_cksum_ptype_rss, R_SEC_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                      \
	R(sec_ts_mark, R_SEC_F | TS_F | MARK_F)                                                    \
	R(sec_ts_mark_rss, R_SEC_F | TS_F | MARK_F | RSS_F)                                        \
	R(sec_ts_mark_ptype, R_SEC_F | TS_F | MARK_F | PTYPE_F)                                    \
	R(sec_ts_mark_ptype_rss, R_SEC_F | TS_F | MARK_F | PTYPE_F | RSS_F)                        \
	R(sec_ts_mark_cksum, R_SEC_F | TS_F | MARK_F | CKSUM_F)                                    \
	R(sec_ts_mark_cksum_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | RSS_F)                        \
	R(sec_ts_mark_cksum_ptype, R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                    \
	R(sec_ts_mark_cksum_ptype_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_96_111                                                               \
	R(sec_vlan, R_SEC_F | RX_VLAN_F)                                                           \
	R(sec_vlan_rss, R_SEC_F | RX_VLAN_F | RSS_F)                                               \
	R(sec_vlan_ptype, R_SEC_F | RX_VLAN_F | PTYPE_F)                                           \
	R(sec_vlan_ptype_rss, R_SEC_F | RX_VLAN_F | PTYPE_F | RSS_F)                               \
	R(sec_vlan_cksum, R_SEC_F | RX_VLAN_F | CKSUM_F)                                           \
	R(sec_vlan_cksum_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | RSS_F)                               \
	R(sec_vlan_cksum_ptype, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F)                           \
	R(sec_vlan_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)               \
	R(sec_vlan_mark, R_SEC_F | RX_VLAN_F | MARK_F)                                             \
	R(sec_vlan_mark_rss, R_SEC_F | RX_VLAN_F | MARK_F | RSS_F)                                 \
	R(sec_vlan_mark_ptype, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F)                             \
	R(sec_vlan_mark_ptype_rss, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                 \
	R(sec_vlan_mark_cksum, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F)                             \
	R(sec_vlan_mark_cksum_rss, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                 \
	R(sec_vlan_mark_cksum_ptype, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)             \
	R(sec_vlan_mark_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_112_127                                                              \
	R(sec_vlan_ts, R_SEC_F | RX_VLAN_F | TS_F)                                                 \
	R(sec_vlan_ts_rss, R_SEC_F | RX_VLAN_F | TS_F | RSS_F)                                     \
	R(sec_vlan_ts_ptype, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F)                                 \
	R(sec_vlan_ts_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F | RSS_F)                     \
	R(sec_vlan_ts_cksum, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F)                                 \
	R(sec_vlan_ts_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | RSS_F)                     \
	R(sec_vlan_ts_cksum_ptype, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                 \
	R(sec_vlan_ts_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)     \
	R(sec_vlan_ts_mark, R_SEC_F | RX_VLAN_F | TS_F | MARK_F)                                   \
	R(sec_vlan_ts_mark_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | RSS_F)                       \
	R(sec_vlan_ts_mark_ptype, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                   \
	R(sec_vlan_ts_mark_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)       \
	R(sec_vlan_ts_mark_cksum, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                   \
	R(sec_vlan_ts_mark_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)       \
	R(sec_vlan_ts_mark_cksum_ptype, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)   \
	R(sec_vlan_ts_mark_cksum_ptype_rss,                                                        \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES                                                                      \
	NIX_RX_FASTPATH_MODES_0_15                                                                 \
	NIX_RX_FASTPATH_MODES_16_31                                                                \
	NIX_RX_FASTPATH_MODES_32_47                                                                \
	NIX_RX_FASTPATH_MODES_48_63                                                                \
	NIX_RX_FASTPATH_MODES_64_79                                                                \
	NIX_RX_FASTPATH_MODES_80_95                                                                \
	NIX_RX_FASTPATH_MODES_96_111                                                               \
	NIX_RX_FASTPATH_MODES_112_127

#define R(name, flags)                                                                             \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_##name(                              \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_mseg_##name(                         \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_##name(                          \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_mseg_##name(                     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_##name(                         \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_mseg_##name(                    \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_vec_##name(                     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_vec_mseg_##name(                \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#define NIX_RX_RECV(fn, flags)                                                                     \
	uint16_t __rte_noinline __rte_hot fn(void *rx_queue, struct rte_mbuf **rx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		return cn20k_nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));                      \
	}

#define NIX_RX_RECV_MSEG(fn, flags) NIX_RX_RECV(fn, flags | NIX_RX_MULTI_SEG_F)

#define NIX_RX_RECV_VEC(fn, flags)                                                                 \
	uint16_t __rte_noinline __rte_hot fn(void *rx_queue, struct rte_mbuf **rx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(rx_queue);                                                            \
		RTE_SET_USED(rx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_RX_RECV_VEC_MSEG(fn, flags) NIX_RX_RECV_VEC(fn, flags | NIX_RX_MULTI_SEG_F)

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_all_offload(void *rx_queue,
								  struct rte_mbuf **rx_pkts,
								  uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_all_offload(void *rx_queue,
								      struct rte_mbuf **rx_pkts,
								      uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_all_offload_tst(void *rx_queue,
								      struct rte_mbuf **rx_pkts,
								      uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_all_offload_tst(void *rx_queue,
									  struct rte_mbuf **rx_pkts,
									  uint16_t pkts);

#endif /* __CN20K_RX_H__ */
