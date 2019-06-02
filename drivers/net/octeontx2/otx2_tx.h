/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TX_H__
#define __OTX2_TX_H__

#define NIX_TX_OFFLOAD_NONE		(0)
#define NIX_TX_OFFLOAD_L3_L4_CSUM_F	BIT(0)
#define NIX_TX_OFFLOAD_OL3_OL4_CSUM_F	BIT(1)
#define NIX_TX_OFFLOAD_VLAN_QINQ_F	BIT(2)
#define NIX_TX_OFFLOAD_MBUF_NOFF_F	BIT(3)
#define NIX_TX_OFFLOAD_TSTAMP_F		BIT(4)

/* Flags to control xmit_prepare function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_TX_MULTI_SEG_F		BIT(15)

#define NIX_TX_NEED_SEND_HDR_W1	\
	(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |	\
	 NIX_TX_OFFLOAD_VLAN_QINQ_F)

#define NIX_TX_NEED_EXT_HDR \
	(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSTAMP_F)

/* Function to determine no of tx subdesc required in case ext
 * sub desc is enabled.
 */
static __rte_always_inline int
otx2_nix_tx_ext_subs(const uint16_t flags)
{
	return (flags & NIX_TX_OFFLOAD_TSTAMP_F) ? 2 :
		((flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) ? 1 : 0);
}

static __rte_always_inline void
otx2_nix_xmit_prepare_tstamp(uint64_t *cmd,  const uint64_t *send_mem_desc,
			     const uint64_t ol_flags, const uint16_t no_segdw,
			     const uint16_t flags)
{
	if (flags & NIX_TX_OFFLOAD_TSTAMP_F) {
		struct nix_send_mem_s *send_mem;
		uint16_t off = (no_segdw - 1) << 1;

		send_mem = (struct nix_send_mem_s *)(cmd + off);
		if (flags & NIX_TX_MULTI_SEG_F)
			/* Retrieving the default desc values */
			cmd[off] = send_mem_desc[6];

		/* Packets for which PKT_TX_IEEE1588_TMST is not set, tx tstamp
		 * should not be updated at tx tstamp registered address, rather
		 * a dummy address which is eight bytes ahead would be updated
		 */
		send_mem->addr = (rte_iova_t)((uint64_t *)send_mem_desc[7] +
				!(ol_flags & PKT_TX_IEEE1588_TMST));
	}
}

static inline void
otx2_nix_xmit_prepare(struct rte_mbuf *m, uint64_t *cmd, const uint16_t flags)
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
			npa_lf_aura_handle_to_aura(m->pool->pool_id);
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
		w1.il4type =  (ol_flags & PKT_TX_L4_MASK) >> 52;

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
		w1.ol4type =  (ol_flags & PKT_TX_L4_MASK) >> 52;
	}

	if (flags & NIX_TX_NEED_EXT_HDR &&
	    flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
		send_hdr_ext->w1.vlan1_ins_ena = !!(ol_flags & PKT_TX_VLAN);
		/* HW will update ptr after vlan0 update */
		send_hdr_ext->w1.vlan1_ins_ptr = 12;
		send_hdr_ext->w1.vlan1_ins_tci = m->vlan_tci;

		send_hdr_ext->w1.vlan0_ins_ena = !!(ol_flags & PKT_TX_QINQ);
		/* 2B before end of l2 header */
		send_hdr_ext->w1.vlan0_ins_ptr = 12;
		send_hdr_ext->w1.vlan0_ins_tci = m->vlan_tci_outer;
	}

	if (flags & NIX_TX_NEED_SEND_HDR_W1)
		send_hdr->w1.u = w1.u;

	if (!(flags & NIX_TX_MULTI_SEG_F)) {
		sg->seg1_size = m->data_len;
		*(rte_iova_t *)(++sg) = rte_mbuf_data_iova(m);

		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
			/* Set don't free bit if reference count > 1 */
			if (rte_pktmbuf_prefree_seg(m) == NULL)
				send_hdr->w0.df = 1; /* SET DF */
		}
		/* Mark mempool object as "put" since it is freed by NIX */
		if (!send_hdr->w0.df)
			__mempool_check_cookies(m->pool, (void **)&m, 1, 0);
	}
}


static __rte_always_inline void
otx2_nix_xmit_one(uint64_t *cmd, void *lmt_addr,
		  const rte_iova_t io_addr, const uint32_t flags)
{
	uint64_t lmt_status;

	do {
		otx2_lmt_mov(lmt_addr, cmd, otx2_nix_tx_ext_subs(flags));
		lmt_status = otx2_lmt_submit(io_addr);
	} while (lmt_status == 0);
}

static __rte_always_inline uint16_t
otx2_nix_prepare_mseg(struct rte_mbuf *m, uint64_t *cmd, const uint16_t flags)
{
	struct nix_send_hdr_s *send_hdr;
	union nix_send_sg_s *sg;
	struct rte_mbuf *m_next;
	uint64_t *slist, sg_u;
	uint64_t nb_segs;
	uint64_t segdw;
	uint8_t off, i;

	send_hdr = (struct nix_send_hdr_s *)cmd;
	send_hdr->w0.total = m->pkt_len;
	send_hdr->w0.aura = npa_lf_aura_handle_to_aura(m->pool->pool_id);

	if (flags & NIX_TX_NEED_EXT_HDR)
		off = 2;
	else
		off = 0;

	sg = (union nix_send_sg_s *)&cmd[2 + off];
	sg_u = sg->u;
	slist = &cmd[3 + off];

	i = 0;
	nb_segs = m->nb_segs;

	/* Fill mbuf segments */
	do {
		m_next = m->next;
		sg_u = sg_u | ((uint64_t)m->data_len << (i << 4));
		*slist = rte_mbuf_data_iova(m);
		/* Set invert df if reference count > 1 */
		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F)
			sg_u |=
			((uint64_t)(rte_pktmbuf_prefree_seg(m) == NULL) <<
			 (i + 55));
		/* Mark mempool object as "put" since it is freed by NIX */
		if (!(sg_u & (1ULL << (i + 55)))) {
			m->next = NULL;
			__mempool_check_cookies(m->pool, (void **)&m, 1, 0);
		}
		slist++;
		i++;
		nb_segs--;
		if (i > 2 && nb_segs) {
			i = 0;
			/* Next SG subdesc */
			*(uint64_t *)slist = sg_u & 0xFC00000000000000;
			sg->u = sg_u;
			sg->segs = 3;
			sg = (union nix_send_sg_s *)slist;
			sg_u = sg->u;
			slist++;
		}
		m = m_next;
	} while (nb_segs);

	sg->u = sg_u;
	sg->segs = i;
	segdw = (uint64_t *)slist - (uint64_t *)&cmd[2 + off];
	/* Roundup extra dwords to multiple of 2 */
	segdw = (segdw >> 1) + (segdw & 0x1);
	/* Default dwords */
	segdw += (off >> 1) + 1 + !!(flags & NIX_TX_OFFLOAD_TSTAMP_F);
	send_hdr->w0.sizem1 = segdw - 1;

	return segdw;
}

static __rte_always_inline void
otx2_nix_xmit_mseg_one(uint64_t *cmd, void *lmt_addr,
		       rte_iova_t io_addr, uint16_t segdw)
{
	uint64_t lmt_status;

	do {
		otx2_lmt_mov_seg(lmt_addr, (const void *)cmd, segdw);
		lmt_status = otx2_lmt_submit(io_addr);
	} while (lmt_status == 0);
}

#define L3L4CSUM_F   NIX_TX_OFFLOAD_L3_L4_CSUM_F
#define OL3OL4CSUM_F NIX_TX_OFFLOAD_OL3_OL4_CSUM_F
#define VLAN_F       NIX_TX_OFFLOAD_VLAN_QINQ_F
#define NOFF_F       NIX_TX_OFFLOAD_MBUF_NOFF_F
#define TSP_F        NIX_TX_OFFLOAD_TSTAMP_F

/* [TSTMP] [NOFF] [VLAN] [OL3OL4CSUM] [L3L4CSUM] */
#define NIX_TX_FASTPATH_MODES					\
T(no_offload,				0, 0, 0, 0, 0,	4,	\
		NIX_TX_OFFLOAD_NONE)				\
T(l3l4csum,				0, 0, 0, 0, 1,	4,	\
		L3L4CSUM_F)					\
T(ol3ol4csum,				0, 0, 0, 1, 0,	4,	\
		OL3OL4CSUM_F)					\
T(ol3ol4csum_l3l4csum,			0, 0, 0, 1, 1,	4,	\
		OL3OL4CSUM_F | L3L4CSUM_F)			\
T(vlan,					0, 0, 1, 0, 0,	6,	\
		VLAN_F)						\
T(vlan_l3l4csum,			0, 0, 1, 0, 1,	6,	\
		VLAN_F | L3L4CSUM_F)				\
T(vlan_ol3ol4csum,			0, 0, 1, 1, 0,	6,	\
		VLAN_F | OL3OL4CSUM_F)				\
T(vlan_ol3ol4csum_l3l4csum,		0, 0, 1, 1, 1,	6,	\
		VLAN_F | OL3OL4CSUM_F |	L3L4CSUM_F)		\
T(noff,					0, 1, 0, 0, 0,	4,	\
		NOFF_F)						\
T(noff_l3l4csum,			0, 1, 0, 0, 1,	4,	\
		NOFF_F | L3L4CSUM_F)				\
T(noff_ol3ol4csum,			0, 1, 0, 1, 0,	4,	\
		NOFF_F | OL3OL4CSUM_F)				\
T(noff_ol3ol4csum_l3l4csum,		0, 1, 0, 1, 1,	4,	\
		NOFF_F | OL3OL4CSUM_F |	L3L4CSUM_F)		\
T(noff_vlan,				0, 1, 1, 0, 0,	6,	\
		NOFF_F | VLAN_F)				\
T(noff_vlan_l3l4csum,			0, 1, 1, 0, 1,	6,	\
		NOFF_F | VLAN_F | L3L4CSUM_F)			\
T(noff_vlan_ol3ol4csum,			0, 1, 1, 1, 0,	6,	\
		NOFF_F | VLAN_F | OL3OL4CSUM_F)			\
T(noff_vlan_ol3ol4csum_l3l4csum,	0, 1, 1, 1, 1,	6,	\
		NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)	\
T(ts,					1, 0, 0, 0, 0,	8,	\
		TSP_F)						\
T(ts_l3l4csum,			1, 0, 0, 0, 1,	8,		\
		TSP_F | L3L4CSUM_F)				\
T(ts_ol3ol4csum,			1, 0, 0, 1, 0,	8,	\
		TSP_F | OL3OL4CSUM_F)				\
T(ts_ol3ol4csum_l3l4csum,		1, 0, 0, 1, 1,	8,	\
		TSP_F | OL3OL4CSUM_F | L3L4CSUM_F)		\
T(ts_vlan,				1, 0, 1, 0, 0,	8,	\
		TSP_F | VLAN_F)					\
T(ts_vlan_l3l4csum,			1, 0, 1, 0, 1,	8,	\
		TSP_F | VLAN_F | L3L4CSUM_F)			\
T(ts_vlan_ol3ol4csum,		1, 0, 1, 1, 0,	8,		\
		TSP_F | VLAN_F | OL3OL4CSUM_F)			\
T(ts_vlan_ol3ol4csum_l3l4csum,	1, 0, 1, 1, 1,	8,		\
		TSP_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)	\
T(ts_noff,				1, 1, 0, 0, 0,	8,	\
		TSP_F | NOFF_F)					\
T(ts_noff_l3l4csum,			1, 1, 0, 0, 1,	8,	\
		TSP_F | NOFF_F | L3L4CSUM_F)			\
T(ts_noff_ol3ol4csum,		1, 1, 0, 1, 0,	8,		\
		TSP_F | NOFF_F | OL3OL4CSUM_F)			\
T(ts_noff_ol3ol4csum_l3l4csum,	1, 1, 0, 1, 1,	8,		\
		TSP_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)	\
T(ts_noff_vlan,			1, 1, 1, 0, 0,	8,		\
		TSP_F | NOFF_F | VLAN_F)			\
T(ts_noff_vlan_l3l4csum,		1, 1, 1, 0, 1,	8,	\
		TSP_F | NOFF_F | VLAN_F | L3L4CSUM_F)		\
T(ts_noff_vlan_ol3ol4csum,		1, 1, 1, 1, 0,	8,	\
		TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)		\
T(ts_noff_vlan_ol3ol4csum_l3l4csum,	1, 1, 1, 1, 1,	8,	\
		TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#endif /* __OTX2_TX_H__ */
