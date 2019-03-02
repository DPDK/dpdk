/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2018 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_RXTX_COMMON_H_
#define _ENIC_RXTX_COMMON_H_

static inline uint16_t
enic_cq_rx_desc_ciflags(struct cq_enet_rq_desc *crd)
{
	return le16_to_cpu(crd->completed_index_flags) & ~CQ_DESC_COMP_NDX_MASK;
}

static inline uint16_t
enic_cq_rx_desc_bwflags(struct cq_enet_rq_desc *crd)
{
	return le16_to_cpu(crd->bytes_written_flags) &
			   ~CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
}

static inline uint8_t
enic_cq_rx_desc_packet_error(uint16_t bwflags)
{
	return (bwflags & CQ_ENET_RQ_DESC_FLAGS_TRUNCATED) ==
		CQ_ENET_RQ_DESC_FLAGS_TRUNCATED;
}

static inline uint8_t
enic_cq_rx_desc_eop(uint16_t ciflags)
{
	return (ciflags & CQ_ENET_RQ_DESC_FLAGS_EOP)
		== CQ_ENET_RQ_DESC_FLAGS_EOP;
}

static inline uint8_t
enic_cq_rx_desc_csum_not_calc(struct cq_enet_rq_desc *cqrd)
{
	return (le16_to_cpu(cqrd->q_number_rss_type_flags) &
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC) ==
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC;
}

static inline uint8_t
enic_cq_rx_desc_ipv4_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return (cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK;
}

static inline uint8_t
enic_cq_rx_desc_tcp_udp_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return (cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK;
}

static inline uint8_t
enic_cq_rx_desc_rss_type(struct cq_enet_rq_desc *cqrd)
{
	return (uint8_t)((le16_to_cpu(cqrd->q_number_rss_type_flags) >>
		CQ_DESC_Q_NUM_BITS) & CQ_ENET_RQ_DESC_RSS_TYPE_MASK);
}

static inline uint32_t
enic_cq_rx_desc_rss_hash(struct cq_enet_rq_desc *cqrd)
{
	return le32_to_cpu(cqrd->rss_hash);
}

static inline uint16_t
enic_cq_rx_desc_vlan(struct cq_enet_rq_desc *cqrd)
{
	return le16_to_cpu(cqrd->vlan);
}

static inline uint16_t
enic_cq_rx_desc_n_bytes(struct cq_desc *cqd)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	return le16_to_cpu(cqrd->bytes_written_flags) &
		CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
}


static inline uint8_t
enic_cq_rx_check_err(struct cq_desc *cqd)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t bwflags;

	bwflags = enic_cq_rx_desc_bwflags(cqrd);
	if (unlikely(enic_cq_rx_desc_packet_error(bwflags)))
		return 1;
	return 0;
}

/* Lookup table to translate RX CQ flags to mbuf flags. */
static uint32_t
enic_cq_rx_flags_to_pkt_type(struct cq_desc *cqd, uint8_t tnl)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint8_t cqrd_flags = cqrd->flags;
	/*
	 * Odd-numbered entries are for tunnel packets. All packet type info
	 * applies to the inner packet, and there is no info on the outer
	 * packet. The outer flags in these entries exist only to avoid
	 * changing enic_cq_rx_to_pkt_flags(). They are cleared from mbuf
	 * afterwards.
	 *
	 * Also, as there is no tunnel type info (VXLAN, NVGRE, or GENEVE), set
	 * RTE_PTYPE_TUNNEL_GRENAT..
	 */
	static const uint32_t cq_type_table[128] __rte_cache_aligned = {
		[0x00] = RTE_PTYPE_UNKNOWN,
		[0x01] = RTE_PTYPE_UNKNOWN |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER,
		[0x20] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG,
		[0x21] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_NONFRAG,
		[0x22] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		[0x23] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_UDP,
		[0x24] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		[0x25] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_TCP,
		[0x60] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x61] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x62] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x63] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x64] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x65] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x10] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG,
		[0x11] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_NONFRAG,
		[0x12] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		[0x13] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_UDP,
		[0x14] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		[0x15] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_TCP,
		[0x50] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x51] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x52] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x53] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x54] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x55] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L2_ETHER |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		/* All others reserved */
	};
	cqrd_flags &= CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT
		| CQ_ENET_RQ_DESC_FLAGS_IPV4 | CQ_ENET_RQ_DESC_FLAGS_IPV6
		| CQ_ENET_RQ_DESC_FLAGS_TCP | CQ_ENET_RQ_DESC_FLAGS_UDP;
	return cq_type_table[cqrd_flags + tnl];
}

static void
enic_cq_rx_to_pkt_flags(struct cq_desc *cqd, struct rte_mbuf *mbuf)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t bwflags, pkt_flags = 0, vlan_tci;
	bwflags = enic_cq_rx_desc_bwflags(cqrd);
	vlan_tci = enic_cq_rx_desc_vlan(cqrd);

	/* VLAN STRIPPED flag. The L2 packet type updated here also */
	if (bwflags & CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED) {
		pkt_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mbuf->packet_type |= RTE_PTYPE_L2_ETHER;
	} else {
		if (vlan_tci != 0) {
			pkt_flags |= PKT_RX_VLAN;
			mbuf->packet_type |= RTE_PTYPE_L2_ETHER_VLAN;
		} else {
			mbuf->packet_type |= RTE_PTYPE_L2_ETHER;
		}
	}
	mbuf->vlan_tci = vlan_tci;

	if ((cqd->type_color & CQ_DESC_TYPE_MASK) == CQ_DESC_TYPE_CLASSIFIER) {
		struct cq_enet_rq_clsf_desc *clsf_cqd;
		uint16_t filter_id;
		clsf_cqd = (struct cq_enet_rq_clsf_desc *)cqd;
		filter_id = clsf_cqd->filter_id;
		if (filter_id) {
			pkt_flags |= PKT_RX_FDIR;
			if (filter_id != ENIC_MAGIC_FILTER_ID) {
				/* filter_id = mark id + 1, so subtract 1 */
				mbuf->hash.fdir.hi = filter_id - 1;
				pkt_flags |= PKT_RX_FDIR_ID;
			}
		}
	} else if (enic_cq_rx_desc_rss_type(cqrd)) {
		/* RSS flag */
		pkt_flags |= PKT_RX_RSS_HASH;
		mbuf->hash.rss = enic_cq_rx_desc_rss_hash(cqrd);
	}

	/* checksum flags */
	if (mbuf->packet_type & (RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV6)) {
		if (!enic_cq_rx_desc_csum_not_calc(cqrd)) {
			uint32_t l4_flags;
			l4_flags = mbuf->packet_type & RTE_PTYPE_L4_MASK;

			/*
			 * When overlay offload is enabled, the NIC may
			 * set ipv4_csum_ok=1 if the inner packet is IPv6..
			 * So, explicitly check for IPv4 before checking
			 * ipv4_csum_ok.
			 */
			if (mbuf->packet_type & RTE_PTYPE_L3_IPV4) {
				if (enic_cq_rx_desc_ipv4_csum_ok(cqrd))
					pkt_flags |= PKT_RX_IP_CKSUM_GOOD;
				else
					pkt_flags |= PKT_RX_IP_CKSUM_BAD;
			}

			if (l4_flags == RTE_PTYPE_L4_UDP ||
			    l4_flags == RTE_PTYPE_L4_TCP) {
				if (enic_cq_rx_desc_tcp_udp_csum_ok(cqrd))
					pkt_flags |= PKT_RX_L4_CKSUM_GOOD;
				else
					pkt_flags |= PKT_RX_L4_CKSUM_BAD;
			}
		}
	}

	mbuf->ol_flags = pkt_flags;
}

#endif /* _ENIC_RXTX_COMMON_H_ */
