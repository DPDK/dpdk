/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_RX_H__
#define __CN9K_RX_H__

#include <rte_ether.h>
#include <rte_vect.h>

#include <cnxk_ethdev.h>

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
#define NIX_RX_MULTI_SEG_F BIT(14)
#define CPT_RX_WQE_F	   BIT(15)

#define CNXK_NIX_CQ_ENTRY_SZ 128
#define NIX_DESCS_PER_LOOP   4
#define CQE_CAST(x)	     ((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

#define IPSEC_SQ_LO_IDX 4
#define IPSEC_SQ_HI_IDX 8

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
		ol_flags |= RTE_MBUF_F_RX_FDIR;
		if (match_id != CNXK_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= RTE_MBUF_F_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const union nix_rx_parse_u *rx, struct rte_mbuf *mbuf,
		    uint64_t rearm, const uint16_t flags)
{
	const rte_iova_t *iova_list;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint64_t sg;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;

	if (nb_segs == 1) {
		mbuf->next = NULL;
		return;
	}

	mbuf->pkt_len = (rx->pkt_lenm1 + 1) - (flags & NIX_RX_OFFLOAD_TSTAMP_F ?
					       CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	mbuf->data_len = (sg & 0xFFFF) - (flags & NIX_RX_OFFLOAD_TSTAMP_F ?
					  CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	mbuf->nb_segs = nb_segs;
	sg = sg >> 16;

	eol = ((const rte_iova_t *)(rx + 1) +
	       ((rx->cn9k.desc_sizem1 + 1) << 1));
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	rearm = rearm & ~0xFFFF;

	head = mbuf;
	while (nb_segs) {
		mbuf->next = ((struct rte_mbuf *)*iova_list) - 1;
		mbuf = mbuf->next;

		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

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

static inline int
ipsec_antireplay_check(struct roc_ie_on_inb_sa *sa,
		       struct cn9k_inb_priv_data *priv, uintptr_t data,
		       uint32_t win_sz)
{
	struct cnxk_on_ipsec_ar *ar = &priv->ar;
	uint64_t seq_in_sa;
	uint32_t seqh = 0;
	uint32_t seql;
	uint64_t seq;
	uint8_t esn;
	int rc;

	esn = sa->common_sa.ctl.esn_en;
	seql = rte_be_to_cpu_32(*((uint32_t *)(data + IPSEC_SQ_LO_IDX)));

	if (!esn) {
		seq = (uint64_t)seql;
	} else {
		seqh = rte_be_to_cpu_32(*((uint32_t *)(data +
					IPSEC_SQ_HI_IDX)));
		seq = ((uint64_t)seqh << 32) | seql;
	}

	if (unlikely(seq == 0))
		return -1;

	rte_spinlock_lock(&ar->lock);
	rc = cnxk_on_anti_replay_check(seq, ar, win_sz);
	if (esn && !rc) {
		seq_in_sa = ((uint64_t)rte_be_to_cpu_32(sa->common_sa.seq_t.th)
			     << 32) |
			    rte_be_to_cpu_32(sa->common_sa.seq_t.tl);
		if (seq > seq_in_sa) {
			sa->common_sa.seq_t.tl = rte_cpu_to_be_32(seql);
			sa->common_sa.seq_t.th = rte_cpu_to_be_32(seqh);
		}
	}
	rte_spinlock_unlock(&ar->lock);

	return rc;
}

static inline uint64_t
nix_rx_sec_mbuf_err_update(const union nix_rx_parse_u *rx, uint16_t res,
			   uint64_t *rearm_val, uint16_t *len)
{
	uint8_t uc_cc = res >> 8;
	uint8_t cc = res & 0xFF;
	uint64_t data_off;
	uint64_t ol_flags;
	uint16_t m_len;

	if (unlikely(cc != CPT_COMP_GOOD))
		return RTE_MBUF_F_RX_SEC_OFFLOAD |
		       RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;

	data_off = *rearm_val & (BIT_ULL(16) - 1);
	m_len = rx->cn9k.pkt_lenm1 + 1;

	switch (uc_cc) {
	case ROC_IE_ON_UCC_IP_PAYLOAD_TYPE_ERR:
	case ROC_IE_ON_UCC_AUTH_ERR:
	case ROC_IE_ON_UCC_PADDING_INVALID:
		/* Adjust data offset to start at copied L2 */
		data_off += ROC_ONF_IPSEC_INB_SPI_SEQ_SZ +
			    ROC_ONF_IPSEC_INB_MAX_L2_SZ;
		ol_flags = RTE_MBUF_F_RX_SEC_OFFLOAD |
			   RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;
		break;
	case ROC_IE_ON_UCC_CTX_INVALID:
	case ROC_IE_ON_UCC_SPI_MISMATCH:
	case ROC_IE_ON_UCC_SA_MISMATCH:
		/* Return as normal packet */
		ol_flags = 0;
		break;
	default:
		/* Return as error packet after updating packet lengths */
		ol_flags = RTE_MBUF_F_RX_SEC_OFFLOAD |
			   RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;
		break;
	}

	*len = m_len;
	*rearm_val = *rearm_val & ~(BIT_ULL(16) - 1);
	*rearm_val |= data_off;
	return ol_flags;
}

static __rte_always_inline uint64_t
nix_rx_sec_mbuf_update(const struct nix_cqe_hdr_s *cq, struct rte_mbuf *m, uintptr_t sa_base,
		       uint64_t *rearm_val, uint16_t *len, uint32_t packet_type)
{
	uintptr_t res_sg0 = ((uintptr_t)cq + ROC_ONF_IPSEC_INB_RES_OFF - 8);
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	struct cn9k_inb_priv_data *sa_priv;
	struct roc_ie_on_inb_sa *sa;
	uint8_t lcptr = rx->lcptr;
	struct rte_ipv4_hdr *ip;
	struct rte_ipv6_hdr *ip6;
	uint16_t data_off, res;
	uint32_t spi, win_sz;
	uint32_t spi_mask;
	uintptr_t data;
	__uint128_t dw;
	uint8_t sa_w;

	res = *(uint64_t *)(res_sg0 + 8);
	data_off = *rearm_val & (BIT_ULL(16) - 1);
	data = (uintptr_t)m->buf_addr;

	data += data_off;

	rte_prefetch0((void *)data);

	if (unlikely(res != (CPT_COMP_GOOD | ROC_IE_ON_UCC_SUCCESS << 8)))
		return nix_rx_sec_mbuf_err_update(rx, res, rearm_val, len);

	data += lcptr;
	/* 20 bits of tag would have the SPI */
	spi = cq->tag & CNXK_ETHDEV_SPI_TAG_MASK;

	/* Get SA */
	sa_w = sa_base & (ROC_NIX_INL_SA_BASE_ALIGN - 1);
	sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);
	spi_mask = (1ULL << sa_w) - 1;
	sa = roc_nix_inl_on_ipsec_inb_sa(sa_base, spi & spi_mask);

	/* Update dynamic field with userdata */
	sa_priv = roc_nix_inl_on_ipsec_inb_sa_sw_rsvd(sa);
	dw = *(__uint128_t *)sa_priv;
	*rte_security_dynfield(m) = (uint64_t)dw;

	/* Check if anti-replay is enabled */
	win_sz = (uint32_t)(dw >> 64);
	if (win_sz) {
		if (ipsec_antireplay_check(sa, sa_priv, data, win_sz) < 0)
			return RTE_MBUF_F_RX_SEC_OFFLOAD | RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;
	}

	/* Get total length from IPv4 header. We can assume only IPv4 */
	ip = (struct rte_ipv4_hdr *)(data + ROC_ONF_IPSEC_INB_SPI_SEQ_SZ +
				     ROC_ONF_IPSEC_INB_MAX_L2_SZ);

	packet_type = (packet_type & ~(RTE_PTYPE_L3_MASK | RTE_PTYPE_TUNNEL_MASK));
	if (((ip->version_ihl & 0xf0) >> RTE_IPV4_IHL_MULTIPLIER) ==
	    IPVERSION) {
		*len = rte_be_to_cpu_16(ip->total_length) + lcptr;
		packet_type |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	} else {
		PLT_ASSERT(((ip->version_ihl & 0xf0) >>
			    RTE_IPV4_IHL_MULTIPLIER) == 6);
		ip6 = (struct rte_ipv6_hdr *)ip;
		*len = rte_be_to_cpu_16(ip6->payload_len) +
		       sizeof(struct rte_ipv6_hdr) + lcptr;
		packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	}

	/* Update data offset */
	data_off +=
		(ROC_ONF_IPSEC_INB_SPI_SEQ_SZ + ROC_ONF_IPSEC_INB_MAX_L2_SZ);
	*rearm_val = *rearm_val & ~(BIT_ULL(16) - 1);
	*rearm_val |= data_off;

	m->packet_type = packet_type;
	return RTE_MBUF_F_RX_SEC_OFFLOAD;
}

static __rte_always_inline void
cn9k_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag,
		     struct rte_mbuf *mbuf, const void *lookup_mem,
		     uint64_t val, const uint16_t flag)
{
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	uint16_t len = rx->cn9k.pkt_lenm1 + 1;
	const uint64_t w1 = *(const uint64_t *)rx;
	uint32_t packet_type;
	uint64_t ol_flags = 0;

	/* Mark mempool obj as "get" as it is alloc'ed by NIX */
	RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		packet_type = nix_ptype_get(lookup_mem, w1);
	else
		packet_type = 0;

	if ((flag & NIX_RX_OFFLOAD_SECURITY_F) &&
	    cq->cqe_type == NIX_XQE_TYPE_RX_IPSECH) {
		uint16_t port = val >> 48;
		uintptr_t sa_base;

		/* Get SA Base from lookup mem */
		sa_base = cnxk_nix_sa_base_get(port, lookup_mem);

		ol_flags |= nix_rx_sec_mbuf_update(cq, mbuf, sa_base, &val, &len, packet_type);
		goto skip_parse;
	}

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = packet_type;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}

	if (flag & NIX_RX_OFFLOAD_CHECKSUM_F)
		ol_flags |= nix_rx_olflags_get(lookup_mem, w1);

skip_parse:
	if (flag & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
		if (rx->cn9k.vtag0_gone) {
			ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
			mbuf->vlan_tci = rx->cn9k.vtag0_tci;
		}
		if (rx->cn9k.vtag1_gone) {
			ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
			mbuf->vlan_tci_outer = rx->cn9k.vtag1_tci;
		}
	}

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags =
			nix_update_match_id(rx->cn9k.match_id, ol_flags, mbuf);

	mbuf->ol_flags = ol_flags;
	*(uint64_t *)(&mbuf->rearm_data) = val;
	mbuf->pkt_len = len;
	mbuf->data_len = len;

	if (flag & NIX_RX_MULTI_SEG_F)
		/*
		 * For multi segment packets, mbuf length correction according
		 * to Rx timestamp length will be handled later during
		 * timestamp data process.
		 * Hence, flag argument is not required.
		 */
		nix_cqe_xtract_mseg(rx, mbuf, val, 0);
	else
		mbuf->next = NULL;
}

static inline uint16_t
nix_rx_nb_pkts(struct cn9k_eth_rxq *rxq, const uint64_t wdata,
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

static __rte_always_inline void
cn9k_nix_mbuf_to_tstamp(struct rte_mbuf *mbuf,
			struct cnxk_timesync_info *tstamp,
			const uint8_t ts_enable, uint64_t *tstamp_ptr)
{
	if (ts_enable) {
		mbuf->pkt_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;
		mbuf->data_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;

		/* Reading the rx timestamp inserted by CGX, viz at
		 * starting of the packet data.
		 */
		*cnxk_nix_timestamp_dynfield(mbuf, tstamp) =
			rte_be_to_cpu_64(*tstamp_ptr);
		/* RTE_MBUF_F_RX_IEEE1588_TMST flag needs to be set only in case
		 * PTP packets are received.
		 */
		if (mbuf->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC) {
			tstamp->rx_tstamp =
				*cnxk_nix_timestamp_dynfield(mbuf, tstamp);
			tstamp->rx_ready = 1;
			mbuf->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP |
					  RTE_MBUF_F_RX_IEEE1588_TMST |
					  tstamp->rx_tstamp_dynflag;
		}
	}
}

static __rte_always_inline uint16_t
cn9k_nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts,
		   const uint16_t flags)
{
	struct cn9k_eth_rxq *rxq = rx_queue;
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

		cn9k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				     flags);
		cn9k_nix_mbuf_to_tstamp(mbuf, rxq->tstamp,
					(flags & NIX_RX_OFFLOAD_TSTAMP_F),
					(uint64_t *)((uint8_t *)mbuf
								+ data_off));
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

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline uint64_t
nix_vlan_update(const uint64_t w2, uint64_t ol_flags, uint8x16_t *f)
{
	if (w2 & BIT_ULL(21) /* vtag0_gone */) {
		ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		*f = vsetq_lane_u16((uint16_t)(w2 >> 32), *f, 5);
	}

	return ol_flags;
}

static __rte_always_inline uint64_t
nix_qinq_update(const uint64_t w2, uint64_t ol_flags, struct rte_mbuf *mbuf)
{
	if (w2 & BIT_ULL(23) /* vtag1_gone */) {
		ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
		mbuf->vlan_tci_outer = (uint16_t)(w2 >> 48);
	}

	return ol_flags;
}

static __rte_always_inline uint16_t
cn9k_nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
			  uint16_t pkts, const uint16_t flags)
{
	struct cn9k_eth_rxq *rxq = rx_queue;
	uint16_t packets = 0;
	uint64x2_t cq0_w8, cq1_w8, cq2_w8, cq3_w8, mbuf01, mbuf23;
	const uint64_t mbuf_initializer = rxq->mbuf_initializer;
	const uint64x2_t data_off = vdupq_n_u64(rxq->data_off);
	uint64_t ol_flags0, ol_flags1, ol_flags2, ol_flags3;
	uint64x2_t rearm0 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm1 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm2 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm3 = vdupq_n_u64(mbuf_initializer);
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	const uint16_t *lookup_mem = rxq->lookup_mem;
	const uint32_t qmask = rxq->qmask;
	const uint64_t wdata = rxq->wdata;
	const uintptr_t desc = rxq->desc;
	uint8x16_t f0, f1, f2, f3;
	uint32_t head = rxq->head;
	uint16_t pkts_left;

	pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);
	pkts_left = pkts & (NIX_DESCS_PER_LOOP - 1);

	/* Packets has to be floor-aligned to NIX_DESCS_PER_LOOP */
	pkts = RTE_ALIGN_FLOOR(pkts, NIX_DESCS_PER_LOOP);

	while (packets < pkts) {
		/* Exit loop if head is about to wrap and become unaligned */
		if (((head + NIX_DESCS_PER_LOOP - 1) & qmask) <
		    NIX_DESCS_PER_LOOP) {
			pkts_left += (pkts - packets);
			break;
		}

		const uintptr_t cq0 = desc + CQE_SZ(head);

		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(8)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(9)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(10)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(11)));

		/* Get NIX_RX_SG_S for size and buffer pointer */
		cq0_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(0) + 64));
		cq1_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(1) + 64));
		cq2_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(2) + 64));
		cq3_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(3) + 64));

		/* Extract mbuf from NIX_RX_SG_S */
		mbuf01 = vzip2q_u64(cq0_w8, cq1_w8);
		mbuf23 = vzip2q_u64(cq2_w8, cq3_w8);
		mbuf01 = vqsubq_u64(mbuf01, data_off);
		mbuf23 = vqsubq_u64(mbuf23, data_off);

		/* Move mbufs to scalar registers for future use */
		mbuf0 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 0);
		mbuf1 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 1);
		mbuf2 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 0);
		mbuf3 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 1);

		/* Mask to get packet len from NIX_RX_SG_S */
		const uint8x16_t shuf_msk = {
			0xFF, 0xFF, /* pkt_type set as unknown */
			0xFF, 0xFF, /* pkt_type set as unknown */
			0,    1,    /* octet 1~0, low 16 bits pkt_len */
			0xFF, 0xFF, /* skip high 16 bits pkt_len, zero out */
			0,    1,    /* octet 1~0, 16 bits data_len */
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

		/* Form the rx_descriptor_fields1 with pkt_len and data_len */
		f0 = vqtbl1q_u8(cq0_w8, shuf_msk);
		f1 = vqtbl1q_u8(cq1_w8, shuf_msk);
		f2 = vqtbl1q_u8(cq2_w8, shuf_msk);
		f3 = vqtbl1q_u8(cq3_w8, shuf_msk);

		/* Load CQE word0 and word 1 */
		uint64_t cq0_w0 = ((uint64_t *)(cq0 + CQE_SZ(0)))[0];
		uint64_t cq0_w1 = ((uint64_t *)(cq0 + CQE_SZ(0)))[1];
		uint64_t cq1_w0 = ((uint64_t *)(cq0 + CQE_SZ(1)))[0];
		uint64_t cq1_w1 = ((uint64_t *)(cq0 + CQE_SZ(1)))[1];
		uint64_t cq2_w0 = ((uint64_t *)(cq0 + CQE_SZ(2)))[0];
		uint64_t cq2_w1 = ((uint64_t *)(cq0 + CQE_SZ(2)))[1];
		uint64_t cq3_w0 = ((uint64_t *)(cq0 + CQE_SZ(3)))[0];
		uint64_t cq3_w1 = ((uint64_t *)(cq0 + CQE_SZ(3)))[1];

		if (flags & NIX_RX_OFFLOAD_RSS_F) {
			/* Fill rss in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(cq0_w0, f0, 3);
			f1 = vsetq_lane_u32(cq1_w0, f1, 3);
			f2 = vsetq_lane_u32(cq2_w0, f2, 3);
			f3 = vsetq_lane_u32(cq3_w0, f3, 3);
			ol_flags0 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags1 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags2 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags3 = RTE_MBUF_F_RX_RSS_HASH;
		} else {
			ol_flags0 = 0;
			ol_flags1 = 0;
			ol_flags2 = 0;
			ol_flags3 = 0;
		}

		if (flags & NIX_RX_OFFLOAD_PTYPE_F) {
			/* Fill packet_type in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq0_w1),
					    f0, 0);
			f1 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq1_w1),
					    f1, 0);
			f2 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq2_w1),
					    f2, 0);
			f3 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq3_w1),
					    f3, 0);
		}

		if (flags & NIX_RX_OFFLOAD_CHECKSUM_F) {
			ol_flags0 |= nix_rx_olflags_get(lookup_mem, cq0_w1);
			ol_flags1 |= nix_rx_olflags_get(lookup_mem, cq1_w1);
			ol_flags2 |= nix_rx_olflags_get(lookup_mem, cq2_w1);
			ol_flags3 |= nix_rx_olflags_get(lookup_mem, cq3_w1);
		}

		if (flags & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
			uint64_t cq0_w2 = *(uint64_t *)(cq0 + CQE_SZ(0) + 16);
			uint64_t cq1_w2 = *(uint64_t *)(cq0 + CQE_SZ(1) + 16);
			uint64_t cq2_w2 = *(uint64_t *)(cq0 + CQE_SZ(2) + 16);
			uint64_t cq3_w2 = *(uint64_t *)(cq0 + CQE_SZ(3) + 16);

			ol_flags0 = nix_vlan_update(cq0_w2, ol_flags0, &f0);
			ol_flags1 = nix_vlan_update(cq1_w2, ol_flags1, &f1);
			ol_flags2 = nix_vlan_update(cq2_w2, ol_flags2, &f2);
			ol_flags3 = nix_vlan_update(cq3_w2, ol_flags3, &f3);

			ol_flags0 = nix_qinq_update(cq0_w2, ol_flags0, mbuf0);
			ol_flags1 = nix_qinq_update(cq1_w2, ol_flags1, mbuf1);
			ol_flags2 = nix_qinq_update(cq2_w2, ol_flags2, mbuf2);
			ol_flags3 = nix_qinq_update(cq3_w2, ol_flags3, mbuf3);
		}

		if (flags & NIX_RX_OFFLOAD_MARK_UPDATE_F) {
			ol_flags0 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(0) + 38), ol_flags0,
				mbuf0);
			ol_flags1 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(1) + 38), ol_flags1,
				mbuf1);
			ol_flags2 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(2) + 38), ol_flags2,
				mbuf2);
			ol_flags3 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(3) + 38), ol_flags3,
				mbuf3);
		}

		if (flags & NIX_RX_OFFLOAD_TSTAMP_F) {
			const uint16x8_t len_off = {
				0,			     /* ptype   0:15 */
				0,			     /* ptype  16:32 */
				CNXK_NIX_TIMESYNC_RX_OFFSET, /* pktlen  0:15*/
				0,			     /* pktlen 16:32 */
				CNXK_NIX_TIMESYNC_RX_OFFSET, /* datalen 0:15 */
				0,
				0,
				0};
			const uint32x4_t ptype = {RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC};
			const uint64_t ts_olf = RTE_MBUF_F_RX_IEEE1588_PTP |
						RTE_MBUF_F_RX_IEEE1588_TMST |
						rxq->tstamp->rx_tstamp_dynflag;
			const uint32x4_t and_mask = {0x1, 0x2, 0x4, 0x8};
			uint64x2_t ts01, ts23, mask;
			uint64_t ts[4];
			uint8_t res;

			/* Subtract timesync length from total pkt length. */
			f0 = vsubq_u16(f0, len_off);
			f1 = vsubq_u16(f1, len_off);
			f2 = vsubq_u16(f2, len_off);
			f3 = vsubq_u16(f3, len_off);

			/* Get the address of actual timestamp. */
			ts01 = vaddq_u64(mbuf01, data_off);
			ts23 = vaddq_u64(mbuf23, data_off);
			/* Load timestamp from address. */
			ts01 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts01,
									  0),
					      ts01, 0);
			ts01 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts01,
									  1),
					      ts01, 1);
			ts23 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts23,
									  0),
					      ts23, 0);
			ts23 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts23,
									  1),
					      ts23, 1);
			/* Convert from be to cpu byteorder. */
			ts01 = vrev64q_u8(ts01);
			ts23 = vrev64q_u8(ts23);
			/* Store timestamp into scalar for later use. */
			ts[0] = vgetq_lane_u64(ts01, 0);
			ts[1] = vgetq_lane_u64(ts01, 1);
			ts[2] = vgetq_lane_u64(ts23, 0);
			ts[3] = vgetq_lane_u64(ts23, 1);

			/* Store timestamp into dynfield. */
			*cnxk_nix_timestamp_dynfield(mbuf0, rxq->tstamp) =
				ts[0];
			*cnxk_nix_timestamp_dynfield(mbuf1, rxq->tstamp) =
				ts[1];
			*cnxk_nix_timestamp_dynfield(mbuf2, rxq->tstamp) =
				ts[2];
			*cnxk_nix_timestamp_dynfield(mbuf3, rxq->tstamp) =
				ts[3];

			/* Generate ptype mask to filter L2 ether timesync */
			mask = vdupq_n_u32(vgetq_lane_u32(f0, 0));
			mask = vsetq_lane_u32(vgetq_lane_u32(f1, 0), mask, 1);
			mask = vsetq_lane_u32(vgetq_lane_u32(f2, 0), mask, 2);
			mask = vsetq_lane_u32(vgetq_lane_u32(f3, 0), mask, 3);

			/* Match against L2 ether timesync. */
			mask = vceqq_u32(mask, ptype);
			/* Convert from vector from scalar mask */
			res = vaddvq_u32(vandq_u32(mask, and_mask));
			res &= 0xF;

			if (res) {
				/* Fill in the ol_flags for any packets that
				 * matched.
				 */
				ol_flags0 |= ((res & 0x1) ? ts_olf : 0);
				ol_flags1 |= ((res & 0x2) ? ts_olf : 0);
				ol_flags2 |= ((res & 0x4) ? ts_olf : 0);
				ol_flags3 |= ((res & 0x8) ? ts_olf : 0);

				/* Update Rxq timestamp with the latest
				 * timestamp.
				 */
				rxq->tstamp->rx_ready = 1;
				rxq->tstamp->rx_tstamp =
					ts[31 - __builtin_clz(res)];
			}
		}

		/* Form rearm_data with ol_flags */
		rearm0 = vsetq_lane_u64(ol_flags0, rearm0, 1);
		rearm1 = vsetq_lane_u64(ol_flags1, rearm1, 1);
		rearm2 = vsetq_lane_u64(ol_flags2, rearm2, 1);
		rearm3 = vsetq_lane_u64(ol_flags3, rearm3, 1);

		/* Update rx_descriptor_fields1 */
		vst1q_u64((uint64_t *)mbuf0->rx_descriptor_fields1, f0);
		vst1q_u64((uint64_t *)mbuf1->rx_descriptor_fields1, f1);
		vst1q_u64((uint64_t *)mbuf2->rx_descriptor_fields1, f2);
		vst1q_u64((uint64_t *)mbuf3->rx_descriptor_fields1, f3);

		/* Update rearm_data */
		vst1q_u64((uint64_t *)mbuf0->rearm_data, rearm0);
		vst1q_u64((uint64_t *)mbuf1->rearm_data, rearm1);
		vst1q_u64((uint64_t *)mbuf2->rearm_data, rearm2);
		vst1q_u64((uint64_t *)mbuf3->rearm_data, rearm3);

		if (flags & NIX_RX_MULTI_SEG_F) {
			/* Multi segment is enable build mseg list for
			 * individual mbufs in scalar mode.
			 */
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
						(cq0 + CQE_SZ(0) + 8), mbuf0,
					    mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
						(cq0 + CQE_SZ(1) + 8), mbuf1,
					    mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
						(cq0 + CQE_SZ(2) + 8), mbuf2,
					    mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
						(cq0 + CQE_SZ(3) + 8), mbuf3,
					    mbuf_initializer, flags);
		} else {
			/* Update that no more segments */
			mbuf0->next = NULL;
			mbuf1->next = NULL;
			mbuf2->next = NULL;
			mbuf3->next = NULL;
		}

		/* Store the mbufs to rx_pkts */
		vst1q_u64((uint64_t *)&rx_pkts[packets], mbuf01);
		vst1q_u64((uint64_t *)&rx_pkts[packets + 2], mbuf23);

		/* Prefetch mbufs */
		roc_prefetch_store_keep(mbuf0);
		roc_prefetch_store_keep(mbuf1);
		roc_prefetch_store_keep(mbuf2);
		roc_prefetch_store_keep(mbuf3);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf0->pool, (void **)&mbuf0, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf1->pool, (void **)&mbuf1, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf2->pool, (void **)&mbuf2, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf3->pool, (void **)&mbuf3, 1, 1);

		/* Advance head pointer and packets */
		head += NIX_DESCS_PER_LOOP;
		head &= qmask;
		packets += NIX_DESCS_PER_LOOP;
	}

	rxq->head = head;
	rxq->available -= packets;

	rte_io_wmb();
	/* Free all the CQs that we've processed */
	plt_write64((rxq->wdata | packets), rxq->cq_door);

	if (unlikely(pkts_left))
		packets += cn9k_nix_recv_pkts(rx_queue, &rx_pkts[packets],
					      pkts_left, flags);

	return packets;
}

#else

static inline uint16_t
cn9k_nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
			  uint16_t pkts, const uint16_t flags)
{
	RTE_SET_USED(rx_queue);
	RTE_SET_USED(rx_pkts);
	RTE_SET_USED(pkts);
	RTE_SET_USED(flags);

	return 0;
}

#endif

#define RSS_F	  NIX_RX_OFFLOAD_RSS_F
#define PTYPE_F	  NIX_RX_OFFLOAD_PTYPE_F
#define CKSUM_F	  NIX_RX_OFFLOAD_CHECKSUM_F
#define MARK_F	  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F	  NIX_RX_OFFLOAD_TSTAMP_F
#define RX_VLAN_F NIX_RX_OFFLOAD_VLAN_STRIP_F
#define R_SEC_F   NIX_RX_OFFLOAD_SECURITY_F

/* [R_SEC_F] [RX_VLAN_F] [TS] [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES_0_15                                             \
	R(no_offload, NIX_RX_OFFLOAD_NONE)                                     \
	R(rss, RSS_F)                                                          \
	R(ptype, PTYPE_F)                                                      \
	R(ptype_rss, PTYPE_F | RSS_F)                                          \
	R(cksum, CKSUM_F)                                                      \
	R(cksum_rss, CKSUM_F | RSS_F)                                          \
	R(cksum_ptype, CKSUM_F | PTYPE_F)                                      \
	R(cksum_ptype_rss, CKSUM_F | PTYPE_F | RSS_F)                          \
	R(mark, MARK_F)                                                        \
	R(mark_rss, MARK_F | RSS_F)                                            \
	R(mark_ptype, MARK_F | PTYPE_F)                                        \
	R(mark_ptype_rss, MARK_F | PTYPE_F | RSS_F)                            \
	R(mark_cksum, MARK_F | CKSUM_F)                                        \
	R(mark_cksum_rss, MARK_F | CKSUM_F | RSS_F)                            \
	R(mark_cksum_ptype, MARK_F | CKSUM_F | PTYPE_F)                        \
	R(mark_cksum_ptype_rss, MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_16_31                                            \
	R(ts, TS_F)                                                            \
	R(ts_rss, TS_F | RSS_F)                                                \
	R(ts_ptype, TS_F | PTYPE_F)                                            \
	R(ts_ptype_rss, TS_F | PTYPE_F | RSS_F)                                \
	R(ts_cksum, TS_F | CKSUM_F)                                            \
	R(ts_cksum_rss, TS_F | CKSUM_F | RSS_F)                                \
	R(ts_cksum_ptype, TS_F | CKSUM_F | PTYPE_F)                            \
	R(ts_cksum_ptype_rss, TS_F | CKSUM_F | PTYPE_F | RSS_F)                \
	R(ts_mark, TS_F | MARK_F)                                              \
	R(ts_mark_rss, TS_F | MARK_F | RSS_F)                                  \
	R(ts_mark_ptype, TS_F | MARK_F | PTYPE_F)                              \
	R(ts_mark_ptype_rss, TS_F | MARK_F | PTYPE_F | RSS_F)                  \
	R(ts_mark_cksum, TS_F | MARK_F | CKSUM_F)                              \
	R(ts_mark_cksum_rss, TS_F | MARK_F | CKSUM_F | RSS_F)                  \
	R(ts_mark_cksum_ptype, TS_F | MARK_F | CKSUM_F | PTYPE_F)              \
	R(ts_mark_cksum_ptype_rss, TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_32_47                                            \
	R(vlan, RX_VLAN_F)                                                     \
	R(vlan_rss, RX_VLAN_F | RSS_F)                                         \
	R(vlan_ptype, RX_VLAN_F | PTYPE_F)                                     \
	R(vlan_ptype_rss, RX_VLAN_F | PTYPE_F | RSS_F)                         \
	R(vlan_cksum, RX_VLAN_F | CKSUM_F)                                     \
	R(vlan_cksum_rss, RX_VLAN_F | CKSUM_F | RSS_F)                         \
	R(vlan_cksum_ptype, RX_VLAN_F | CKSUM_F | PTYPE_F)                     \
	R(vlan_cksum_ptype_rss, RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)         \
	R(vlan_mark, RX_VLAN_F | MARK_F)                                       \
	R(vlan_mark_rss, RX_VLAN_F | MARK_F | RSS_F)                           \
	R(vlan_mark_ptype, RX_VLAN_F | MARK_F | PTYPE_F)                       \
	R(vlan_mark_ptype_rss, RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)           \
	R(vlan_mark_cksum, RX_VLAN_F | MARK_F | CKSUM_F)                       \
	R(vlan_mark_cksum_rss, RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)           \
	R(vlan_mark_cksum_ptype, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)       \
	R(vlan_mark_cksum_ptype_rss,                                           \
	  RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_48_63                                            \
	R(vlan_ts, RX_VLAN_F | TS_F)                                           \
	R(vlan_ts_rss, RX_VLAN_F | TS_F | RSS_F)                               \
	R(vlan_ts_ptype, RX_VLAN_F | TS_F | PTYPE_F)                           \
	R(vlan_ts_ptype_rss, RX_VLAN_F | TS_F | PTYPE_F | RSS_F)               \
	R(vlan_ts_cksum, RX_VLAN_F | TS_F | CKSUM_F)                           \
	R(vlan_ts_cksum_rss, RX_VLAN_F | TS_F | CKSUM_F | RSS_F)               \
	R(vlan_ts_cksum_ptype, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)           \
	R(vlan_ts_cksum_ptype_rss,                                             \
	  RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                        \
	R(vlan_ts_mark, RX_VLAN_F | TS_F | MARK_F)                             \
	R(vlan_ts_mark_rss, RX_VLAN_F | TS_F | MARK_F | RSS_F)                 \
	R(vlan_ts_mark_ptype, RX_VLAN_F | TS_F | MARK_F | PTYPE_F)             \
	R(vlan_ts_mark_ptype_rss, RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F) \
	R(vlan_ts_mark_cksum, RX_VLAN_F | TS_F | MARK_F | CKSUM_F)             \
	R(vlan_ts_mark_cksum_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F) \
	R(vlan_ts_mark_cksum_ptype,                                            \
	  RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                       \
	R(vlan_ts_mark_cksum_ptype_rss,                                        \
	  RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_64_79                                            \
	R(sec, R_SEC_F)                                                        \
	R(sec_rss, R_SEC_F | RSS_F)                                            \
	R(sec_ptype, R_SEC_F | PTYPE_F)                                        \
	R(sec_ptype_rss, R_SEC_F | PTYPE_F | RSS_F)                            \
	R(sec_cksum, R_SEC_F | CKSUM_F)                                        \
	R(sec_cksum_rss, R_SEC_F | CKSUM_F | RSS_F)                            \
	R(sec_cksum_ptype, R_SEC_F | CKSUM_F | PTYPE_F)                        \
	R(sec_cksum_ptype_rss, R_SEC_F | CKSUM_F | PTYPE_F | RSS_F)            \
	R(sec_mark, R_SEC_F | MARK_F)                                          \
	R(sec_mark_rss, R_SEC_F | MARK_F | RSS_F)                              \
	R(sec_mark_ptype, R_SEC_F | MARK_F | PTYPE_F)                          \
	R(sec_mark_ptype_rss, R_SEC_F | MARK_F | PTYPE_F | RSS_F)              \
	R(sec_mark_cksum, R_SEC_F | MARK_F | CKSUM_F)                          \
	R(sec_mark_cksum_rss, R_SEC_F | MARK_F | CKSUM_F | RSS_F)              \
	R(sec_mark_cksum_ptype, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F)          \
	R(sec_mark_cksum_ptype_rss,                                            \
	  R_SEC_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_80_95                                            \
	R(sec_ts, R_SEC_F | TS_F)                                              \
	R(sec_ts_rss, R_SEC_F | TS_F | RSS_F)                                  \
	R(sec_ts_ptype, R_SEC_F | TS_F | PTYPE_F)                              \
	R(sec_ts_ptype_rss, R_SEC_F | TS_F | PTYPE_F | RSS_F)                  \
	R(sec_ts_cksum, R_SEC_F | TS_F | CKSUM_F)                              \
	R(sec_ts_cksum_rss, R_SEC_F | TS_F | CKSUM_F | RSS_F)                  \
	R(sec_ts_cksum_ptype, R_SEC_F | TS_F | CKSUM_F | PTYPE_F)              \
	R(sec_ts_cksum_ptype_rss, R_SEC_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)  \
	R(sec_ts_mark, R_SEC_F | TS_F | MARK_F)                                \
	R(sec_ts_mark_rss, R_SEC_F | TS_F | MARK_F | RSS_F)                    \
	R(sec_ts_mark_ptype, R_SEC_F | TS_F | MARK_F | PTYPE_F)                \
	R(sec_ts_mark_ptype_rss, R_SEC_F | TS_F | MARK_F | PTYPE_F | RSS_F)    \
	R(sec_ts_mark_cksum, R_SEC_F | TS_F | MARK_F | CKSUM_F)                \
	R(sec_ts_mark_cksum_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | RSS_F)    \
	R(sec_ts_mark_cksum_ptype,                                             \
	  R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                         \
	R(sec_ts_mark_cksum_ptype_rss,                                         \
	  R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_96_111                                           \
	R(sec_vlan, R_SEC_F | RX_VLAN_F)                                       \
	R(sec_vlan_rss, R_SEC_F | RX_VLAN_F | RSS_F)                           \
	R(sec_vlan_ptype, R_SEC_F | RX_VLAN_F | PTYPE_F)                       \
	R(sec_vlan_ptype_rss, R_SEC_F | RX_VLAN_F | PTYPE_F | RSS_F)           \
	R(sec_vlan_cksum, R_SEC_F | RX_VLAN_F | CKSUM_F)                       \
	R(sec_vlan_cksum_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | RSS_F)           \
	R(sec_vlan_cksum_ptype, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F)       \
	R(sec_vlan_cksum_ptype_rss,                                            \
	  R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)                     \
	R(sec_vlan_mark, R_SEC_F | RX_VLAN_F | MARK_F)                         \
	R(sec_vlan_mark_rss, R_SEC_F | RX_VLAN_F | MARK_F | RSS_F)             \
	R(sec_vlan_mark_ptype, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F)         \
	R(sec_vlan_mark_ptype_rss,                                             \
	  R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                      \
	R(sec_vlan_mark_cksum, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F)         \
	R(sec_vlan_mark_cksum_rss,                                             \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                      \
	R(sec_vlan_mark_cksum_ptype,                                           \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)                    \
	R(sec_vlan_mark_cksum_ptype_rss,                                       \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_112_127                                          \
	R(sec_vlan_ts, R_SEC_F | RX_VLAN_F | TS_F)                             \
	R(sec_vlan_ts_rss, R_SEC_F | RX_VLAN_F | TS_F | RSS_F)                 \
	R(sec_vlan_ts_ptype, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F)             \
	R(sec_vlan_ts_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F | RSS_F) \
	R(sec_vlan_ts_cksum, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F)             \
	R(sec_vlan_ts_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | RSS_F) \
	R(sec_vlan_ts_cksum_ptype,                                             \
	  R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                      \
	R(sec_vlan_ts_cksum_ptype_rss,                                         \
	  R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)              \
	R(sec_vlan_ts_mark, R_SEC_F | RX_VLAN_F | TS_F | MARK_F)               \
	R(sec_vlan_ts_mark_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | RSS_F)   \
	R(sec_vlan_ts_mark_ptype,                                              \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                       \
	R(sec_vlan_ts_mark_ptype_rss,                                          \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)               \
	R(sec_vlan_ts_mark_cksum,                                              \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                       \
	R(sec_vlan_ts_mark_cksum_rss,                                          \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)               \
	R(sec_vlan_ts_mark_cksum_ptype,                                        \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)             \
	R(sec_vlan_ts_mark_cksum_ptype_rss,                                    \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES                                                  \
	NIX_RX_FASTPATH_MODES_0_15                                             \
	NIX_RX_FASTPATH_MODES_16_31                                            \
	NIX_RX_FASTPATH_MODES_32_47                                            \
	NIX_RX_FASTPATH_MODES_48_63                                            \
	NIX_RX_FASTPATH_MODES_64_79                                            \
	NIX_RX_FASTPATH_MODES_80_95                                            \
	NIX_RX_FASTPATH_MODES_96_111                                           \
	NIX_RX_FASTPATH_MODES_112_127

#define R(name, flags)                                                         \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_##name(           \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_mseg_##name(      \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_vec_##name(       \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_vec_mseg_##name(  \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#define NIX_RX_RECV(fn, flags)                                                 \
	uint16_t __rte_noinline __rte_hot fn(                                  \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn9k_nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));   \
	}

#define NIX_RX_RECV_MSEG(fn, flags) NIX_RX_RECV(fn, flags | NIX_RX_MULTI_SEG_F)

#define NIX_RX_RECV_VEC(fn, flags)                                             \
	uint16_t __rte_noinline __rte_hot fn(                                  \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn9k_nix_recv_pkts_vector(rx_queue, rx_pkts, pkts,      \
						 (flags));                     \
	}

#define NIX_RX_RECV_VEC_MSEG(fn, flags)                                        \
	NIX_RX_RECV_VEC(fn, flags | NIX_RX_MULTI_SEG_F)

#endif /* __CN9K_RX_H__ */
