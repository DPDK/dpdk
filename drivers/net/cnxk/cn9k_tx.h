/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN9K_TX_H__
#define __CN9K_TX_H__

#define NIX_TX_OFFLOAD_NONE	      (0)
#define NIX_TX_OFFLOAD_L3_L4_CSUM_F   BIT(0)
#define NIX_TX_OFFLOAD_OL3_OL4_CSUM_F BIT(1)
#define NIX_TX_OFFLOAD_VLAN_QINQ_F    BIT(2)
#define NIX_TX_OFFLOAD_MBUF_NOFF_F    BIT(3)
#define NIX_TX_OFFLOAD_TSO_F	      BIT(4)

/* Flags to control xmit_prepare function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_TX_MULTI_SEG_F BIT(15)

#define NIX_TX_NEED_SEND_HDR_W1                                                \
	(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |         \
	 NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)

#define NIX_TX_NEED_EXT_HDR                                                    \
	(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)

#define NIX_XMIT_FC_OR_RETURN(txq, pkts)                                       \
	do {                                                                   \
		/* Cached value is low, Update the fc_cache_pkts */            \
		if (unlikely((txq)->fc_cache_pkts < (pkts))) {                 \
			/* Multiply with sqe_per_sqb to express in pkts */     \
			(txq)->fc_cache_pkts =                                 \
				((txq)->nb_sqb_bufs_adj - *(txq)->fc_mem)      \
				<< (txq)->sqes_per_sqb_log2;                   \
			/* Check it again for the room */                      \
			if (unlikely((txq)->fc_cache_pkts < (pkts)))           \
				return 0;                                      \
		}                                                              \
	} while (0)

/* Function to determine no of tx subdesc required in case ext
 * sub desc is enabled.
 */
static __rte_always_inline int
cn9k_nix_tx_ext_subs(const uint16_t flags)
{
	return (flags &
		(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)) ? 1 : 0;
}

static __rte_always_inline void
cn9k_nix_xmit_prepare_tso(struct rte_mbuf *m, const uint64_t flags)
{
	uint64_t mask, ol_flags = m->ol_flags;

	if (flags & NIX_TX_OFFLOAD_TSO_F && (ol_flags & PKT_TX_TCP_SEG)) {
		uintptr_t mdata = rte_pktmbuf_mtod(m, uintptr_t);
		uint16_t *iplen, *oiplen, *oudplen;
		uint16_t lso_sb, paylen;

		mask = -!!(ol_flags & (PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IPV6));
		lso_sb = (mask & (m->outer_l2_len + m->outer_l3_len)) +
			 m->l2_len + m->l3_len + m->l4_len;

		/* Reduce payload len from base headers */
		paylen = m->pkt_len - lso_sb;

		/* Get iplen position assuming no tunnel hdr */
		iplen = (uint16_t *)(mdata + m->l2_len +
				     (2 << !!(ol_flags & PKT_TX_IPV6)));
		/* Handle tunnel tso */
		if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
		    (ol_flags & PKT_TX_TUNNEL_MASK)) {
			const uint8_t is_udp_tun =
				(CNXK_NIX_UDP_TUN_BITMASK >>
				 ((ol_flags & PKT_TX_TUNNEL_MASK) >> 45)) &
				0x1;

			oiplen = (uint16_t *)(mdata + m->outer_l2_len +
					      (2 << !!(ol_flags &
						       PKT_TX_OUTER_IPV6)));
			*oiplen = rte_cpu_to_be_16(rte_be_to_cpu_16(*oiplen) -
						   paylen);

			/* Update format for UDP tunneled packet */
			if (is_udp_tun) {
				oudplen = (uint16_t *)(mdata + m->outer_l2_len +
						       m->outer_l3_len + 4);
				*oudplen = rte_cpu_to_be_16(
					rte_be_to_cpu_16(*oudplen) - paylen);
			}

			/* Update iplen position to inner ip hdr */
			iplen = (uint16_t *)(mdata + lso_sb - m->l3_len -
					     m->l4_len +
					     (2 << !!(ol_flags & PKT_TX_IPV6)));
		}

		*iplen = rte_cpu_to_be_16(rte_be_to_cpu_16(*iplen) - paylen);
	}
}

static __rte_always_inline void
cn9k_nix_xmit_prepare(struct rte_mbuf *m, uint64_t *cmd, const uint16_t flags,
		      const uint64_t lso_tun_fmt)
{
	struct nix_send_ext_s *send_hdr_ext;
	struct nix_send_hdr_s *send_hdr;
	uint64_t ol_flags = 0, mask;
	union nix_send_hdr_w1_u w1;
	union nix_send_sg_s *sg;

	send_hdr = (struct nix_send_hdr_s *)cmd;
	if (flags & NIX_TX_NEED_EXT_HDR) {
		send_hdr_ext = (struct nix_send_ext_s *)(cmd + 2);
		sg = (union nix_send_sg_s *)(cmd + 4);
		/* Clear previous markings */
		send_hdr_ext->w0.lso = 0;
		send_hdr_ext->w1.u = 0;
	} else {
		sg = (union nix_send_sg_s *)(cmd + 2);
	}

	if (flags & NIX_TX_NEED_SEND_HDR_W1) {
		ol_flags = m->ol_flags;
		w1.u = 0;
	}

	if (!(flags & NIX_TX_MULTI_SEG_F)) {
		send_hdr->w0.total = m->data_len;
		send_hdr->w0.aura =
			roc_npa_aura_handle_to_aura(m->pool->pool_id);
	}

	/*
	 * L3type:  2 => IPV4
	 *          3 => IPV4 with csum
	 *          4 => IPV6
	 * L3type and L3ptr needs to be set for either
	 * L3 csum or L4 csum or LSO
	 *
	 */

	if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
	    (flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F)) {
		const uint8_t csum = !!(ol_flags & PKT_TX_OUTER_UDP_CKSUM);
		const uint8_t ol3type =
			((!!(ol_flags & PKT_TX_OUTER_IPV4)) << 1) +
			((!!(ol_flags & PKT_TX_OUTER_IPV6)) << 2) +
			!!(ol_flags & PKT_TX_OUTER_IP_CKSUM);

		/* Outer L3 */
		w1.ol3type = ol3type;
		mask = 0xffffull << ((!!ol3type) << 4);
		w1.ol3ptr = ~mask & m->outer_l2_len;
		w1.ol4ptr = ~mask & (w1.ol3ptr + m->outer_l3_len);

		/* Outer L4 */
		w1.ol4type = csum + (csum << 1);

		/* Inner L3 */
		w1.il3type = ((!!(ol_flags & PKT_TX_IPV4)) << 1) +
			     ((!!(ol_flags & PKT_TX_IPV6)) << 2);
		w1.il3ptr = w1.ol4ptr + m->l2_len;
		w1.il4ptr = w1.il3ptr + m->l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.il3type = w1.il3type + !!(ol_flags & PKT_TX_IP_CKSUM);

		/* Inner L4 */
		w1.il4type = (ol_flags & PKT_TX_L4_MASK) >> 52;

		/* In case of no tunnel header use only
		 * shift IL3/IL4 fields a bit to use
		 * OL3/OL4 for header checksum
		 */
		mask = !ol3type;
		w1.u = ((w1.u & 0xFFFFFFFF00000000) >> (mask << 3)) |
		       ((w1.u & 0X00000000FFFFFFFF) >> (mask << 4));

	} else if (flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) {
		const uint8_t csum = !!(ol_flags & PKT_TX_OUTER_UDP_CKSUM);
		const uint8_t outer_l2_len = m->outer_l2_len;

		/* Outer L3 */
		w1.ol3ptr = outer_l2_len;
		w1.ol4ptr = outer_l2_len + m->outer_l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.ol3type = ((!!(ol_flags & PKT_TX_OUTER_IPV4)) << 1) +
			     ((!!(ol_flags & PKT_TX_OUTER_IPV6)) << 2) +
			     !!(ol_flags & PKT_TX_OUTER_IP_CKSUM);

		/* Outer L4 */
		w1.ol4type = csum + (csum << 1);

	} else if (flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F) {
		const uint8_t l2_len = m->l2_len;

		/* Always use OLXPTR and OLXTYPE when only
		 * when one header is present
		 */

		/* Inner L3 */
		w1.ol3ptr = l2_len;
		w1.ol4ptr = l2_len + m->l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.ol3type = ((!!(ol_flags & PKT_TX_IPV4)) << 1) +
			     ((!!(ol_flags & PKT_TX_IPV6)) << 2) +
			     !!(ol_flags & PKT_TX_IP_CKSUM);

		/* Inner L4 */
		w1.ol4type = (ol_flags & PKT_TX_L4_MASK) >> 52;
	}

	if (flags & NIX_TX_NEED_EXT_HDR && flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
		send_hdr_ext->w1.vlan1_ins_ena = !!(ol_flags & PKT_TX_VLAN);
		/* HW will update ptr after vlan0 update */
		send_hdr_ext->w1.vlan1_ins_ptr = 12;
		send_hdr_ext->w1.vlan1_ins_tci = m->vlan_tci;

		send_hdr_ext->w1.vlan0_ins_ena = !!(ol_flags & PKT_TX_QINQ);
		/* 2B before end of l2 header */
		send_hdr_ext->w1.vlan0_ins_ptr = 12;
		send_hdr_ext->w1.vlan0_ins_tci = m->vlan_tci_outer;
	}

	if (flags & NIX_TX_OFFLOAD_TSO_F && (ol_flags & PKT_TX_TCP_SEG)) {
		uint16_t lso_sb;
		uint64_t mask;

		mask = -(!w1.il3type);
		lso_sb = (mask & w1.ol4ptr) + (~mask & w1.il4ptr) + m->l4_len;

		send_hdr_ext->w0.lso_sb = lso_sb;
		send_hdr_ext->w0.lso = 1;
		send_hdr_ext->w0.lso_mps = m->tso_segsz;
		send_hdr_ext->w0.lso_format =
			NIX_LSO_FORMAT_IDX_TSOV4 + !!(ol_flags & PKT_TX_IPV6);
		w1.ol4type = NIX_SENDL4TYPE_TCP_CKSUM;

		/* Handle tunnel tso */
		if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
		    (ol_flags & PKT_TX_TUNNEL_MASK)) {
			const uint8_t is_udp_tun =
				(CNXK_NIX_UDP_TUN_BITMASK >>
				 ((ol_flags & PKT_TX_TUNNEL_MASK) >> 45)) &
				0x1;
			uint8_t shift = is_udp_tun ? 32 : 0;

			shift += (!!(ol_flags & PKT_TX_OUTER_IPV6) << 4);
			shift += (!!(ol_flags & PKT_TX_IPV6) << 3);

			w1.il4type = NIX_SENDL4TYPE_TCP_CKSUM;
			w1.ol4type = is_udp_tun ? NIX_SENDL4TYPE_UDP_CKSUM : 0;
			/* Update format for UDP tunneled packet */
			send_hdr_ext->w0.lso_format = (lso_tun_fmt >> shift);
		}
	}

	if (flags & NIX_TX_NEED_SEND_HDR_W1)
		send_hdr->w1.u = w1.u;

	if (!(flags & NIX_TX_MULTI_SEG_F)) {
		sg->seg1_size = m->data_len;
		*(rte_iova_t *)(++sg) = rte_mbuf_data_iova(m);

		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
			/* DF bit = 1 if refcount of current mbuf or parent mbuf
			 *		is greater than 1
			 * DF bit = 0 otherwise
			 */
			send_hdr->w0.df = cnxk_nix_prefree_seg(m);
			/* Ensuring mbuf fields which got updated in
			 * cnxk_nix_prefree_seg are written before LMTST.
			 */
			rte_io_wmb();
		}
		/* Mark mempool object as "put" since it is freed by NIX */
		if (!send_hdr->w0.df)
			__mempool_check_cookies(m->pool, (void **)&m, 1, 0);
	}
}

static __rte_always_inline void
cn9k_nix_xmit_one(uint64_t *cmd, void *lmt_addr, const rte_iova_t io_addr,
		  const uint32_t flags)
{
	uint64_t lmt_status;

	do {
		roc_lmt_mov(lmt_addr, cmd, cn9k_nix_tx_ext_subs(flags));
		lmt_status = roc_lmt_submit_ldeor(io_addr);
	} while (lmt_status == 0);
}

static __rte_always_inline void
cn9k_nix_xmit_prep_lmt(uint64_t *cmd, void *lmt_addr, const uint32_t flags)
{
	roc_lmt_mov(lmt_addr, cmd, cn9k_nix_tx_ext_subs(flags));
}

static __rte_always_inline uint64_t
cn9k_nix_xmit_submit_lmt(const rte_iova_t io_addr)
{
	return roc_lmt_submit_ldeor(io_addr);
}

static __rte_always_inline uint64_t
cn9k_nix_xmit_submit_lmt_release(const rte_iova_t io_addr)
{
	return roc_lmt_submit_ldeorl(io_addr);
}

static __rte_always_inline uint16_t
cn9k_nix_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts,
		   uint64_t *cmd, const uint16_t flags)
{
	struct cn9k_eth_txq *txq = tx_queue;
	const rte_iova_t io_addr = txq->io_addr;
	void *lmt_addr = txq->lmt_addr;
	uint64_t lso_tun_fmt;
	uint16_t i;

	NIX_XMIT_FC_OR_RETURN(txq, pkts);

	roc_lmt_mov(cmd, &txq->cmd[0], cn9k_nix_tx_ext_subs(flags));

	/* Perform header writes before barrier for TSO */
	if (flags & NIX_TX_OFFLOAD_TSO_F) {
		lso_tun_fmt = txq->lso_tun_fmt;

		for (i = 0; i < pkts; i++)
			cn9k_nix_xmit_prepare_tso(tx_pkts[i], flags);
	}

	/* Lets commit any changes in the packet here as no further changes
	 * to the packet will be done unless no fast free is enabled.
	 */
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
		rte_io_wmb();

	for (i = 0; i < pkts; i++) {
		cn9k_nix_xmit_prepare(tx_pkts[i], cmd, flags, lso_tun_fmt);
		cn9k_nix_xmit_one(cmd, lmt_addr, io_addr, flags);
	}

	/* Reduce the cached count */
	txq->fc_cache_pkts -= pkts;

	return pkts;
}

#define L3L4CSUM_F   NIX_TX_OFFLOAD_L3_L4_CSUM_F
#define OL3OL4CSUM_F NIX_TX_OFFLOAD_OL3_OL4_CSUM_F
#define VLAN_F	     NIX_TX_OFFLOAD_VLAN_QINQ_F
#define NOFF_F	     NIX_TX_OFFLOAD_MBUF_NOFF_F
#define TSO_F	     NIX_TX_OFFLOAD_TSO_F

/* [TSO] [NOFF] [VLAN] [OL3OL4CSUM] [L3L4CSUM] */
#define NIX_TX_FASTPATH_MODES						\
T(no_offload,				0, 0, 0, 0, 0,	4,		\
		NIX_TX_OFFLOAD_NONE)					\
T(l3l4csum,				0, 0, 0, 0, 1,	4,		\
		L3L4CSUM_F)						\
T(ol3ol4csum,				0, 0, 0, 1, 0,	4,		\
		OL3OL4CSUM_F)						\
T(ol3ol4csum_l3l4csum,			0, 0, 0, 1, 1,	4,		\
		OL3OL4CSUM_F | L3L4CSUM_F)				\
T(vlan,					0, 0, 1, 0, 0,	6,		\
		VLAN_F)							\
T(vlan_l3l4csum,			0, 0, 1, 0, 1,	6,		\
		VLAN_F | L3L4CSUM_F)					\
T(vlan_ol3ol4csum,			0, 0, 1, 1, 0,	6,		\
		VLAN_F | OL3OL4CSUM_F)					\
T(vlan_ol3ol4csum_l3l4csum,		0, 0, 1, 1, 1,	6,		\
		VLAN_F | OL3OL4CSUM_F |	L3L4CSUM_F)			\
T(noff,					0, 1, 0, 0, 0,	4,		\
		NOFF_F)							\
T(noff_l3l4csum,			0, 1, 0, 0, 1,	4,		\
		NOFF_F | L3L4CSUM_F)					\
T(noff_ol3ol4csum,			0, 1, 0, 1, 0,	4,		\
		NOFF_F | OL3OL4CSUM_F)					\
T(noff_ol3ol4csum_l3l4csum,		0, 1, 0, 1, 1,	4,		\
		NOFF_F | OL3OL4CSUM_F |	L3L4CSUM_F)			\
T(noff_vlan,				0, 1, 1, 0, 0,	6,		\
		NOFF_F | VLAN_F)					\
T(noff_vlan_l3l4csum,			0, 1, 1, 0, 1,	6,		\
		NOFF_F | VLAN_F | L3L4CSUM_F)				\
T(noff_vlan_ol3ol4csum,			0, 1, 1, 1, 0,	6,		\
		NOFF_F | VLAN_F | OL3OL4CSUM_F)				\
T(noff_vlan_ol3ol4csum_l3l4csum,	0, 1, 1, 1, 1,	6,		\
		NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)		\
T(tso,					1, 0, 0, 0, 0,	6,		\
		TSO_F)							\
T(tso_l3l4csum,				1, 0, 0, 0, 1,	6,		\
		TSO_F | L3L4CSUM_F)					\
T(tso_ol3ol4csum,			1, 0, 0, 1, 0,	6,		\
		TSO_F | OL3OL4CSUM_F)					\
T(tso_ol3ol4csum_l3l4csum,		1, 0, 0, 1, 1,	6,		\
		TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)			\
T(tso_vlan,				1, 0, 1, 0, 0,	6,		\
		TSO_F | VLAN_F)						\
T(tso_vlan_l3l4csum,			1, 0, 1, 0, 1,	6,		\
		TSO_F | VLAN_F | L3L4CSUM_F)				\
T(tso_vlan_ol3ol4csum,			1, 0, 1, 1, 0,	6,		\
		TSO_F | VLAN_F | OL3OL4CSUM_F)				\
T(tso_vlan_ol3ol4csum_l3l4csum,		1, 0, 1, 1, 1,	6,		\
		TSO_F | VLAN_F | OL3OL4CSUM_F |	L3L4CSUM_F)		\
T(tso_noff,				1, 1, 0, 0, 0,	6,		\
		TSO_F | NOFF_F)						\
T(tso_noff_l3l4csum,			1, 1, 0, 0, 1,	6,		\
		TSO_F | NOFF_F | L3L4CSUM_F)				\
T(tso_noff_ol3ol4csum,			1, 1, 0, 1, 0,	6,		\
		TSO_F | NOFF_F | OL3OL4CSUM_F)				\
T(tso_noff_ol3ol4csum_l3l4csum,		1, 1, 0, 1, 1,	6,		\
		TSO_F | NOFF_F | OL3OL4CSUM_F |	L3L4CSUM_F)		\
T(tso_noff_vlan,			1, 1, 1, 0, 0,	6,		\
		TSO_F | NOFF_F | VLAN_F)				\
T(tso_noff_vlan_l3l4csum,		1, 1, 1, 0, 1,	6,		\
		TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)			\
T(tso_noff_vlan_ol3ol4csum,		1, 1, 1, 1, 0,	6,		\
		TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)			\
T(tso_noff_vlan_ol3ol4csum_l3l4csum,	1, 1, 1, 1, 1,	6,		\
		TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define T(name, f4, f3, f2, f1, f0, sz, flags)                                 \
	uint16_t __rte_noinline __rte_hot cn9k_nix_xmit_pkts_##name(           \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);

NIX_TX_FASTPATH_MODES
#undef T

#endif /* __CN9K_TX_H__ */
