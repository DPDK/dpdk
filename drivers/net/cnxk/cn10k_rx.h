/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_RX_H__
#define __CN10K_RX_H__

#include <rte_ether.h>

#define NIX_RX_OFFLOAD_NONE	     (0)
#define NIX_RX_OFFLOAD_RSS_F	     BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F	     BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F    BIT(2)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F BIT(3)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_MULTI_SEG_F BIT(15)

#define CNXK_NIX_CQ_ENTRY_SZ 128
#define NIX_DESCS_PER_LOOP   4
#define CQE_CAST(x)	     ((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

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
		(const uint32_t *)((const uint8_t *)lookup_mem +
				   PTYPE_ARRAY_SZ);

	return ol_flags[(in & 0xfff00000) >> 20];
}

static inline uint64_t
nix_update_match_id(const uint16_t match_id, uint64_t ol_flags,
		    struct rte_mbuf *mbuf)
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
		ol_flags |= PKT_RX_FDIR;
		if (match_id != CNXK_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= PKT_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const union nix_rx_parse_u *rx, struct rte_mbuf *mbuf,
		    uint64_t rearm)
{
	const rte_iova_t *iova_list;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint64_t sg;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;
	mbuf->nb_segs = nb_segs;
	mbuf->data_len = sg & 0xFFFF;
	sg = sg >> 16;

	eol = ((const rte_iova_t *)(rx + 1) + ((rx->desc_sizem1 + 1) << 1));
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	rearm = rearm & ~0xFFFF;

	head = mbuf;
	while (nb_segs) {
		mbuf->next = ((struct rte_mbuf *)*iova_list) - 1;
		mbuf = mbuf->next;

		__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

		mbuf->data_len = sg & 0xFFFF;
		sg = sg >> 16;
		*(uint64_t *)(&mbuf->rearm_data) = rearm;
		nb_segs--;
		iova_list++;

		if (!nb_segs && (iova_list + 1 < eol)) {
			sg = *(const uint64_t *)(iova_list);
			nb_segs = (sg >> 48) & 0x3;
			head->nb_segs += nb_segs;
			iova_list = (const rte_iova_t *)(iova_list + 1);
		}
	}
	mbuf->next = NULL;
}

static __rte_always_inline void
cn10k_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag,
		      struct rte_mbuf *mbuf, const void *lookup_mem,
		      const uint64_t val, const uint16_t flag)
{
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	const uint16_t len = rx->pkt_lenm1 + 1;
	const uint64_t w1 = *(const uint64_t *)rx;
	uint64_t ol_flags = 0;

	/* Mark mempool obj as "get" as it is alloc'ed by NIX */
	__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = nix_ptype_get(lookup_mem, w1);
	else
		mbuf->packet_type = 0;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= PKT_RX_RSS_HASH;
	}

	if (flag & NIX_RX_OFFLOAD_CHECKSUM_F)
		ol_flags |= nix_rx_olflags_get(lookup_mem, w1);

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags = nix_update_match_id(rx->match_id, ol_flags, mbuf);

	mbuf->ol_flags = ol_flags;
	*(uint64_t *)(&mbuf->rearm_data) = val;
	mbuf->pkt_len = len;

	if (flag & NIX_RX_MULTI_SEG_F) {
		nix_cqe_xtract_mseg(rx, mbuf, val);
	} else {
		mbuf->data_len = len;
		mbuf->next = NULL;
	}
}

static inline uint16_t
nix_rx_nb_pkts(struct cn10k_eth_rxq *rxq, const uint64_t wdata,
	       const uint16_t pkts, const uint32_t qmask)
{
	uint32_t available = rxq->available;

	/* Update the available count if cached value is not enough */
	if (unlikely(available < pkts)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, rxq->cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) ||
		    reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
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

static __rte_always_inline uint16_t
cn10k_nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts,
		    const uint16_t flags)
{
	struct cn10k_eth_rxq *rxq = rx_queue;
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

	nb_pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);

	while (packets < nb_pkts) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal(
			(void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		cn10k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				      flags);
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

/* [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES					       \
R(no_offload,			0, 0, 0, 0, NIX_RX_OFFLOAD_NONE)       \
R(rss,				0, 0, 0, 1, RSS_F)		       \
R(ptype,			0, 0, 1, 0, PTYPE_F)		       \
R(ptype_rss,			0, 0, 1, 1, PTYPE_F | RSS_F)	       \
R(cksum,			0, 1, 0, 0, CKSUM_F)		       \
R(cksum_rss,			0, 1, 0, 1, CKSUM_F | RSS_F)	       \
R(cksum_ptype,			0, 1, 1, 0, CKSUM_F | PTYPE_F)	       \
R(cksum_ptype_rss,		0, 1, 1, 1, CKSUM_F | PTYPE_F | RSS_F) \
R(mark,				1, 0, 0, 0, MARK_F)		       \
R(mark_rss,			1, 0, 0, 1, MARK_F | RSS_F)	       \
R(mark_ptype,			1, 0, 1, 0, MARK_F | PTYPE_F)	       \
R(mark_ptype_rss,		1, 0, 1, 1, MARK_F | PTYPE_F | RSS_F)  \
R(mark_cksum,			1, 1, 0, 0, MARK_F | CKSUM_F)	       \
R(mark_cksum_rss,		1, 1, 0, 1, MARK_F | CKSUM_F | RSS_F)  \
R(mark_cksum_ptype,		1, 1, 1, 0, MARK_F | CKSUM_F | PTYPE_F)\
R(mark_cksum_ptype_rss,		1, 1, 1, 1, MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define R(name, f3, f2, f1, f0, flags)                                         \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_##name(          \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
									       \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_mseg_##name(     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#endif /* __CN10K_RX_H__ */
