/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_RXTX_H__
#define	__OCTEONTX_RXTX_H__

#include <rte_ethdev_driver.h>

#define OFFLOAD_FLAGS					\
	uint16_t rx_offload_flags;			\
	uint16_t tx_offload_flags

#define BIT(nr) (1UL << (nr))

#define OCCTX_RX_OFFLOAD_NONE		(0)
#define OCCTX_RX_MULTI_SEG_F		BIT(0)
#define OCCTX_RX_OFFLOAD_CSUM_F         BIT(1)
#define OCCTX_RX_VLAN_FLTR_F            BIT(2)

#define OCCTX_TX_OFFLOAD_NONE		(0)
#define OCCTX_TX_MULTI_SEG_F		BIT(0)
#define OCCTX_TX_OFFLOAD_L3_L4_CSUM_F	BIT(1)
#define OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F	BIT(2)
#define OCCTX_TX_OFFLOAD_MBUF_NOFF_F	BIT(3)

/* Packet type table */
#define PTYPE_SIZE	OCCTX_PKI_LTYPE_LAST

/* octeontx send header sub descriptor structure */
RTE_STD_C11
union octeontx_send_hdr_w0_u {
	uint64_t u;
	struct {
		uint64_t total   : 16;
		uint64_t markptr : 8;
		uint64_t l3ptr   : 8;
		uint64_t l4ptr   : 8;
		uint64_t ii	 : 1;
		uint64_t shp_dis : 1;
		uint64_t ckle    : 1;
		uint64_t cklf    : 2;
		uint64_t ckl3    : 1;
		uint64_t ckl4    : 2;
		uint64_t p	 : 1;
		uint64_t format	 : 7;
		uint64_t tstamp  : 1;
		uint64_t tso_eom : 1;
		uint64_t df	 : 1;
		uint64_t tso	 : 1;
		uint64_t n2	 : 1;
		uint64_t scntn1	 : 3;
	};
};

RTE_STD_C11
union octeontx_send_hdr_w1_u {
	uint64_t u;
	struct {
		uint64_t tso_mss : 14;
		uint64_t shp_ra  : 2;
		uint64_t tso_sb  : 8;
		uint64_t leptr   : 8;
		uint64_t lfptr   : 8;
		uint64_t shp_chg : 9;
		uint64_t tso_fn  : 7;
		uint64_t l2len   : 8;
	};
};

struct octeontx_send_hdr_s {
	union octeontx_send_hdr_w0_u w0;
	union octeontx_send_hdr_w1_u w1;
};

static const uint32_t __rte_cache_aligned
ptype_table[PTYPE_SIZE][PTYPE_SIZE][PTYPE_SIZE] = {
	[LC_NONE][LE_NONE][LF_NONE] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_IPSEC_ESP] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L4_FRAG,
	[LC_NONE][LE_NONE][LF_IPCOMP] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_TCP] = RTE_PTYPE_L4_TCP,
	[LC_NONE][LE_NONE][LF_UDP] = RTE_PTYPE_L4_UDP,
	[LC_NONE][LE_NONE][LF_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[LC_NONE][LE_NONE][LF_UDP_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
	[LC_NONE][LE_NONE][LF_UDP_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[LC_NONE][LE_NONE][LF_NVGRE] = RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV4][LE_NONE][LF_NONE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[LC_IPV4][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV4,
	[LC_IPV4][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_FRAG,
	[LC_IPV4][LE_NONE][LF_IPCOMP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[LC_IPV4][LE_NONE][LF_TCP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
	[LC_IPV4][LE_NONE][LF_UDP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
	[LC_IPV4][LE_NONE][LF_GRE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV4][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV4][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV4][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV4_OPT][LE_NONE][LF_NONE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV4_OPT][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L3_IPV4,
	[LC_IPV4_OPT][LE_NONE][LF_IPFRAG] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_FRAG,
	[LC_IPV4_OPT][LE_NONE][LF_IPCOMP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV4_OPT][LE_NONE][LF_TCP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_TCP,
	[LC_IPV4_OPT][LE_NONE][LF_UDP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_UDP,
	[LC_IPV4_OPT][LE_NONE][LF_GRE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV4_OPT][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV4_OPT][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV4_OPT][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV6][LE_NONE][LF_NONE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[LC_IPV6][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L3_IPV4,
	[LC_IPV6][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_FRAG,
	[LC_IPV6][LE_NONE][LF_IPCOMP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[LC_IPV6][LE_NONE][LF_TCP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
	[LC_IPV6][LE_NONE][LF_UDP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	[LC_IPV6][LE_NONE][LF_GRE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV6][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV6][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV6][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_NVGRE,
	[LC_IPV6_OPT][LE_NONE][LF_NONE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV6_OPT][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L3_IPV4,
	[LC_IPV6_OPT][LE_NONE][LF_IPFRAG] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_FRAG,
	[LC_IPV6_OPT][LE_NONE][LF_IPCOMP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV6_OPT][LE_NONE][LF_TCP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_TCP,
	[LC_IPV6_OPT][LE_NONE][LF_UDP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
	[LC_IPV6_OPT][LE_NONE][LF_GRE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV6_OPT][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV6_OPT][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV6_OPT][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_NVGRE,

};


static __rte_always_inline uint64_t
octeontx_pktmbuf_detach(struct rte_mbuf *m, struct rte_mbuf **m_tofree)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len;
	struct rte_mbuf *md;
	uint16_t priv_size;
	uint16_t refcount;

	/* Update refcount of direct mbuf */
	md = rte_mbuf_from_indirect(m);
	/* The real data will be in the direct buffer, inform callers this */
	*m_tofree = md;
	refcount = rte_mbuf_refcnt_update(md, -1);

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = (uint32_t)(sizeof(struct rte_mbuf) + priv_size);
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;
	rte_pktmbuf_reset_headroom(m);
	m->data_len = 0;
	m->ol_flags = 0;
	m->next = NULL;
	m->nb_segs = 1;

	/* Now indirect mbuf is safe to free */
	rte_pktmbuf_free(m);

	if (refcount == 0) {
		rte_mbuf_refcnt_set(md, 1);
		md->data_len = 0;
		md->ol_flags = 0;
		md->next = NULL;
		md->nb_segs = 1;
		return 0;
	} else {
		return 1;
	}
}

static __rte_always_inline uint64_t
octeontx_prefree_seg(struct rte_mbuf *m, struct rte_mbuf **m_tofree)
{
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		if (!RTE_MBUF_DIRECT(m))
			return octeontx_pktmbuf_detach(m, m_tofree);

		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	} else if (rte_mbuf_refcnt_update(m, -1) == 0) {
		if (!RTE_MBUF_DIRECT(m))
			return octeontx_pktmbuf_detach(m, m_tofree);

		rte_mbuf_refcnt_set(m, 1);
		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	}

	/* Mbuf is having refcount more than 1 so need not to be freed */
	return 1;
}

static __rte_always_inline void
octeontx_tx_checksum_offload(uint64_t *cmd_buf, const uint16_t flags,
			     struct rte_mbuf *m)
{
	struct octeontx_send_hdr_s *send_hdr =
				(struct octeontx_send_hdr_s *)cmd_buf;
	uint64_t ol_flags = m->ol_flags;

	/* PKO Checksum L4 Algorithm Enumeration
	 * 0x0 - No checksum
	 * 0x1 - UDP L4 checksum
	 * 0x2 - TCP L4 checksum
	 * 0x3 - SCTP L4 checksum
	 */
	const uint8_t csum = (!(((ol_flags ^ PKT_TX_UDP_CKSUM) >> 52) & 0x3) +
		      (!(((ol_flags ^ PKT_TX_TCP_CKSUM) >> 52) & 0x3) * 2) +
		      (!(((ol_flags ^ PKT_TX_SCTP_CKSUM) >> 52) & 0x3) * 3));

	const uint8_t is_tunnel_parsed = (!!(ol_flags & PKT_TX_TUNNEL_GTP) ||
				      !!(ol_flags & PKT_TX_TUNNEL_VXLAN_GPE) ||
				      !!(ol_flags & PKT_TX_TUNNEL_VXLAN) ||
				      !!(ol_flags & PKT_TX_TUNNEL_GRE) ||
				      !!(ol_flags & PKT_TX_TUNNEL_GENEVE) ||
				      !!(ol_flags & PKT_TX_TUNNEL_IP) ||
				      !!(ol_flags & PKT_TX_TUNNEL_IPIP));

	const uint8_t csum_outer = (!!(ol_flags & PKT_TX_OUTER_UDP_CKSUM) ||
				    !!(ol_flags & PKT_TX_TUNNEL_UDP));
	const uint8_t outer_l2_len = m->outer_l2_len;
	const uint8_t l2_len = m->l2_len;

	if ((flags & OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
	    (flags & OCCTX_TX_OFFLOAD_L3_L4_CSUM_F)) {
		if (is_tunnel_parsed) {
			/* Outer L3 */
			send_hdr->w0.l3ptr = outer_l2_len;
			send_hdr->w0.l4ptr = outer_l2_len + m->outer_l3_len;
			/* Set clk3 for PKO to calculate IPV4 header checksum */
			send_hdr->w0.ckl3 = !!(ol_flags & PKT_TX_OUTER_IPV4);

			/* Outer L4 */
			send_hdr->w0.ckl4 = csum_outer;

			/* Inner L3 */
			send_hdr->w1.leptr = send_hdr->w0.l4ptr + l2_len;
			send_hdr->w1.lfptr = send_hdr->w1.leptr + m->l3_len;
			/* Set clke for PKO to calculate inner IPV4 header
			 * checksum.
			 */
			send_hdr->w0.ckle = !!(ol_flags & PKT_TX_IPV4);

			/* Inner L4 */
			send_hdr->w0.cklf = csum;
		} else {
			/* Inner L3 */
			send_hdr->w0.l3ptr = l2_len;
			send_hdr->w0.l4ptr = l2_len + m->l3_len;
			/* Set clk3 for PKO to calculate IPV4 header checksum */
			send_hdr->w0.ckl3 = !!(ol_flags & PKT_TX_IPV4);

			/* Inner L4 */
			send_hdr->w0.ckl4 = csum;
		}
	} else if (flags & OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F) {
		/* Outer L3 */
		send_hdr->w0.l3ptr = outer_l2_len;
		send_hdr->w0.l4ptr = outer_l2_len + m->outer_l3_len;
		/* Set clk3 for PKO to calculate IPV4 header checksum */
		send_hdr->w0.ckl3 = !!(ol_flags & PKT_TX_OUTER_IPV4);

		/* Outer L4 */
		send_hdr->w0.ckl4 = csum_outer;
	} else if (flags & OCCTX_TX_OFFLOAD_L3_L4_CSUM_F) {
		/* Inner L3 */
		send_hdr->w0.l3ptr = l2_len;
		send_hdr->w0.l4ptr = l2_len + m->l3_len;
		/* Set clk3 for PKO to calculate IPV4 header checksum */
		send_hdr->w0.ckl3 = !!(ol_flags & PKT_TX_IPV4);

		/* Inner L4 */
		send_hdr->w0.ckl4 = csum;
	}
}

static __rte_always_inline uint16_t
__octeontx_xmit_prepare(struct rte_mbuf *tx_pkt, uint64_t *cmd_buf,
			const uint16_t flag)
{
	uint16_t gaura_id, nb_desc = 0;
	struct rte_mbuf *m_tofree;
	rte_iova_t iova;
	uint16_t data_len;

	m_tofree = tx_pkt;

	data_len = tx_pkt->data_len;
	iova = rte_mbuf_data_iova(tx_pkt);

	/* Setup PKO_SEND_HDR_S */
	cmd_buf[nb_desc++] = tx_pkt->data_len & 0xffff;
	cmd_buf[nb_desc++] = 0x0;

	/* Enable tx checksum offload */
	if ((flag & OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F) ||
	    (flag & OCCTX_TX_OFFLOAD_L3_L4_CSUM_F))
		octeontx_tx_checksum_offload(cmd_buf, flag, tx_pkt);

	/* SEND_HDR[DF] bit controls if buffer is to be freed or
	 * not, as SG_DESC[I] and SEND_HDR[II] are clear.
	 */
	if (flag & OCCTX_TX_OFFLOAD_MBUF_NOFF_F)
		cmd_buf[0] |= (octeontx_prefree_seg(tx_pkt, &m_tofree) <<
			       58);

	/* Mark mempool object as "put" since it is freed by PKO */
	if (!(cmd_buf[0] & (1ULL << 58)))
		__mempool_check_cookies(m_tofree->pool, (void **)&m_tofree,
					1, 0);
	/* Get the gaura Id */
	gaura_id =
		octeontx_fpa_bufpool_gaura((uintptr_t)m_tofree->pool->pool_id);

	/* Setup PKO_SEND_BUFLINK_S */
	cmd_buf[nb_desc++] = PKO_SEND_BUFLINK_SUBDC |
		PKO_SEND_BUFLINK_LDTYPE(0x1ull) |
		PKO_SEND_BUFLINK_GAUAR((long)gaura_id) |
		data_len;
	cmd_buf[nb_desc++] = iova;

	return nb_desc;
}

static __rte_always_inline uint16_t
__octeontx_xmit_mseg_prepare(struct rte_mbuf *tx_pkt, uint64_t *cmd_buf,
			const uint16_t flag)
{
	uint16_t nb_segs, nb_desc = 0;
	uint16_t gaura_id;
	struct rte_mbuf *m_next = NULL, *m_tofree;
	rte_iova_t iova;
	uint16_t data_len;

	nb_segs = tx_pkt->nb_segs;
	/* Setup PKO_SEND_HDR_S */
	cmd_buf[nb_desc++] = tx_pkt->pkt_len & 0xffff;
	cmd_buf[nb_desc++] = 0x0;

	/* Enable tx checksum offload */
	if ((flag & OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F) ||
	    (flag & OCCTX_TX_OFFLOAD_L3_L4_CSUM_F))
		octeontx_tx_checksum_offload(cmd_buf, flag, tx_pkt);

	do {
		m_next = tx_pkt->next;
		/* Get TX parameters up front, octeontx_prefree_seg might change
		 * them
		 */
		m_tofree = tx_pkt;
		data_len = tx_pkt->data_len;
		iova = rte_mbuf_data_iova(tx_pkt);

		/* Setup PKO_SEND_GATHER_S */
		cmd_buf[nb_desc] = 0;

		/* SG_DESC[I] bit controls if buffer is to be freed or
		 * not, as SEND_HDR[DF] and SEND_HDR[II] are clear.
		 */
		if (flag & OCCTX_TX_OFFLOAD_MBUF_NOFF_F) {
			cmd_buf[nb_desc] |=
				(octeontx_prefree_seg(tx_pkt, &m_tofree) << 57);
		}

		/* To handle case where mbufs belong to diff pools, like
		 * fragmentation
		 */
		gaura_id = octeontx_fpa_bufpool_gaura((uintptr_t)
					m_tofree->pool->pool_id);

		/* Setup PKO_SEND_GATHER_S */
		cmd_buf[nb_desc] |= PKO_SEND_GATHER_SUBDC		 |
				   PKO_SEND_GATHER_LDTYPE(0x1ull)	 |
				   PKO_SEND_GATHER_GAUAR((long)gaura_id) |
				   data_len;

		/* Mark mempool object as "put" since it is freed by
		 * PKO.
		 */
		if (!(cmd_buf[nb_desc] & (1ULL << 57))) {
			tx_pkt->next = NULL;
			__mempool_check_cookies(m_tofree->pool,
						(void **)&m_tofree, 1, 0);
		}
		nb_desc++;

		cmd_buf[nb_desc++] = iova;

		nb_segs--;
		tx_pkt = m_next;
	} while (nb_segs);

	return nb_desc;
}

static __rte_always_inline uint16_t
__octeontx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts, uint64_t *cmd_buf,
		     const uint16_t flags)
{
	struct octeontx_txq *txq = tx_queue;
	octeontx_dq_t *dq = &txq->dq;
	uint16_t count = 0, nb_desc;
	rte_io_wmb();

	while (count < nb_pkts) {
		if (unlikely(*((volatile int64_t *)dq->fc_status_va) < 0))
			break;

		if (flags & OCCTX_TX_MULTI_SEG_F) {
			nb_desc = __octeontx_xmit_mseg_prepare(tx_pkts[count],
							       cmd_buf, flags);
		} else {
			nb_desc = __octeontx_xmit_prepare(tx_pkts[count],
							  cmd_buf, flags);
		}

		octeontx_reg_lmtst(dq->lmtline_va, dq->ioreg_va, cmd_buf,
				   nb_desc);

		count++;
	}
	return count;
}

uint16_t
octeontx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

#define L3L4CSUM_F   OCCTX_TX_OFFLOAD_L3_L4_CSUM_F
#define OL3OL4CSUM_F OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F
#define NOFF_F       OCCTX_TX_OFFLOAD_MBUF_NOFF_F
#define MULT_F       OCCTX_TX_MULTI_SEG_F

/* [L3L4CSUM_F] [OL3OL4CSUM_F] [NOFF] [MULTI_SEG] */
#define OCCTX_TX_FASTPATH_MODES						       \
T(no_offload,				0, 0, 0, 0,	4,		       \
					OCCTX_TX_OFFLOAD_NONE)		       \
T(mseg,					0, 0, 0, 1,	14,		       \
					MULT_F)			               \
T(l3l4csum,				0, 0, 1, 0,     4,		       \
					L3L4CSUM_F)			       \
T(l3l4csum_mseg,			0, 0, 1, 1,	14,		       \
					L3L4CSUM_F | MULT_F)		       \
T(ol3ol4csum,				0, 1, 0, 0,	4,		       \
					OL3OL4CSUM_F)			       \
T(ol3l4csum_mseg,			0, 1, 0, 1,	14,		       \
					OL3OL4CSUM_F | MULT_F)	               \
T(ol3l4csum_l3l4csum,			0, 1, 1, 0,     4,		       \
					OL3OL4CSUM_F | L3L4CSUM_F)	       \
T(ol3l4csum_l3l4csum_mseg,		0, 1, 1, 1,	14,		       \
					OL3OL4CSUM_F | L3L4CSUM_F | MULT_F)    \
T(noff,					1, 0, 0, 0,     4,		       \
					NOFF_F)				       \
T(noff_mseg,				1, 0, 0, 1,	14,		       \
					NOFF_F | MULT_F)	               \
T(noff_l3l4csum,			1, 0, 1, 0,     4,		       \
					NOFF_F | L3L4CSUM_F)		       \
T(noff_l3l4csum_mseg,			1, 0, 1, 1,	14,		       \
					NOFF_F | L3L4CSUM_F | MULT_F)	       \
T(noff_ol3ol4csum,			1, 1, 0, 0,	4,		       \
					NOFF_F | OL3OL4CSUM_F)		       \
T(noff_ol3ol4csum_mseg,			1, 1, 0, 1,	14,		       \
					NOFF_F | OL3OL4CSUM_F | MULT_F)	       \
T(noff_ol3ol4csum_l3l4csum,		1, 1, 1, 0,     4,		       \
					NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)    \
T(noff_ol3ol4csum_l3l4csum_mseg,	1, 1, 1, 1,	14,		       \
					NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F |   \
					MULT_F)

/* RX offload macros */
#define VLAN_FLTR_F     OCCTX_RX_VLAN_FLTR_F
#define CSUM_F		OCCTX_RX_OFFLOAD_CSUM_F
#define MULT_RX_F       OCCTX_RX_MULTI_SEG_F

/* [VLAN_FLTR] [CSUM_F] [MULTI_SEG] */
#define OCCTX_RX_FASTPATH_MODES						       \
R(no_offload,				0, 0, 0,  OCCTX_RX_OFFLOAD_NONE)       \
R(mseg,					0, 0, 1,  MULT_RX_F)		       \
R(csum,					0, 1, 0,  CSUM_F)		       \
R(csum_mseg,				0, 1, 1,  CSUM_F | MULT_RX_F)	       \
R(vlan,					1, 0, 0,  VLAN_FLTR_F)		       \
R(vlan_mseg,				1, 0, 1,  VLAN_FLTR_F | MULT_RX_F)     \
R(vlan_csum,				1, 1, 0,  VLAN_FLTR_F | CSUM_F)	       \
R(vlan_csum_mseg,			1, 1, 1,  CSUM_F | VLAN_FLTR_F |       \
					MULT_RX_F)

 #endif /* __OCTEONTX_RXTX_H__ */
