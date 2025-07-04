/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 */

#include <stdint.h>

#include <eal_export.h>
#include <rte_mbuf.h>
#include <rte_mbuf_ptype.h>
#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_gre.h>
#include <rte_mpls.h>
#include <rte_geneve.h>
#include <rte_vxlan.h>
#include <rte_gtp.h>
#include <rte_net.h>
#include <rte_os_shim.h>

/* get l3 packet type from ip6 next protocol */
static uint32_t
ptype_l3_ip6(uint8_t ip6_proto)
{
	static const uint32_t ip6_ext_proto_map[256] = {
		[IPPROTO_HOPOPTS] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
		[IPPROTO_ROUTING] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
		[IPPROTO_FRAGMENT] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
		[IPPROTO_ESP] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
		[IPPROTO_AH] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
		[IPPROTO_DSTOPTS] = RTE_PTYPE_L3_IPV6_EXT - RTE_PTYPE_L3_IPV6,
	};

	return RTE_PTYPE_L3_IPV6 + ip6_ext_proto_map[ip6_proto];
}

/* get l3 packet type from ip version and header length */
static uint32_t
ptype_l3_ip(uint8_t ipv_ihl)
{
	static const uint32_t ptype_l3_ip_proto_map[256] = {
		[0x45] = RTE_PTYPE_L3_IPV4,
		[0x46] = RTE_PTYPE_L3_IPV4_EXT,
		[0x47] = RTE_PTYPE_L3_IPV4_EXT,
		[0x48] = RTE_PTYPE_L3_IPV4_EXT,
		[0x49] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4A] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4B] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4C] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4D] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4E] = RTE_PTYPE_L3_IPV4_EXT,
		[0x4F] = RTE_PTYPE_L3_IPV4_EXT,
	};

	return ptype_l3_ip_proto_map[ipv_ihl];
}

/* get l4 packet type from proto */
static uint32_t
ptype_l4(uint8_t proto)
{
	static const uint32_t ptype_l4_proto[256] = {
		[IPPROTO_UDP] = RTE_PTYPE_L4_UDP,
		[IPPROTO_TCP] = RTE_PTYPE_L4_TCP,
		[IPPROTO_SCTP] = RTE_PTYPE_L4_SCTP,
	};

	return ptype_l4_proto[proto];
}

/* get inner l3 packet type from ip6 next protocol */
static uint32_t
ptype_inner_l3_ip6(uint8_t ip6_proto)
{
	static const uint32_t ptype_inner_ip6_ext_proto_map[256] = {
		[IPPROTO_HOPOPTS] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
		[IPPROTO_ROUTING] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
		[IPPROTO_FRAGMENT] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
		[IPPROTO_ESP] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
		[IPPROTO_AH] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
		[IPPROTO_DSTOPTS] = RTE_PTYPE_INNER_L3_IPV6_EXT -
			RTE_PTYPE_INNER_L3_IPV6,
	};

	return RTE_PTYPE_INNER_L3_IPV6 +
		ptype_inner_ip6_ext_proto_map[ip6_proto];
}

/* get inner l3 packet type from ip version and header length */
static uint32_t
ptype_inner_l3_ip(uint8_t ipv_ihl)
{
	static const uint32_t ptype_inner_l3_ip_proto_map[256] = {
		[0x45] = RTE_PTYPE_INNER_L3_IPV4,
		[0x46] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x47] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x48] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x49] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4A] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4B] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4C] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4D] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4E] = RTE_PTYPE_INNER_L3_IPV4_EXT,
		[0x4F] = RTE_PTYPE_INNER_L3_IPV4_EXT,
	};

	return ptype_inner_l3_ip_proto_map[ipv_ihl];
}

/* get inner l4 packet type from proto */
static uint32_t
ptype_inner_l4(uint8_t proto)
{
	static const uint32_t ptype_inner_l4_proto[256] = {
		[IPPROTO_UDP] = RTE_PTYPE_INNER_L4_UDP,
		[IPPROTO_TCP] = RTE_PTYPE_INNER_L4_TCP,
		[IPPROTO_SCTP] = RTE_PTYPE_INNER_L4_SCTP,
	};

	return ptype_inner_l4_proto[proto];
}

/* get the tunnel packet type if any, update proto and off. */
static uint32_t
ptype_tunnel_without_udp(uint16_t *proto, const struct rte_mbuf *m,
	uint32_t *off)
{
	switch (*proto) {
	case IPPROTO_GRE: {
		static const uint8_t opt_len[16] = {
			[0x0] = 4,
			[0x1] = 8,
			[0x2] = 8,
			[0x8] = 8,
			[0x3] = 12,
			[0x9] = 12,
			[0xa] = 12,
			[0xb] = 16,
		};
		const struct rte_gre_hdr *gh;
		struct rte_gre_hdr gh_copy;
		uint16_t flags;

		gh = rte_pktmbuf_read(m, *off, sizeof(*gh), &gh_copy);
		if (unlikely(gh == NULL))
			return 0;

		flags = rte_be_to_cpu_16(*(const uint16_t *)gh);
		flags >>= 12;
		if (opt_len[flags] == 0)
			return 0;

		*off += opt_len[flags];
		*proto = gh->proto;
		if (*proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_TEB))
			return RTE_PTYPE_TUNNEL_NVGRE;
		else
			return RTE_PTYPE_TUNNEL_GRE;
	}
	case IPPROTO_IPIP:
		*proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		return RTE_PTYPE_TUNNEL_IP;
	case IPPROTO_IPV6:
		*proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		return RTE_PTYPE_TUNNEL_IP; /* IP is also valid for IPv6 */
	default:
		return 0;
	}
}

/* get the tunnel packet type with UDP port if any, update proto and off. */
static uint32_t
ptype_tunnel_with_udp(uint16_t *proto, const struct rte_mbuf *m,
			uint32_t *off, struct rte_net_hdr_lens *hdr_lens)
{
	const struct rte_udp_hdr *uh;
	struct rte_udp_hdr uh_copy;
	uint16_t port_no;

	uh = rte_pktmbuf_read(m, *off, sizeof(*uh), &uh_copy);
	if (unlikely(uh == NULL))
		return 0;

	*off += sizeof(*uh);
	if (rte_be_to_cpu_16(uh->src_port) == RTE_GTPC_UDP_PORT)
		port_no = rte_be_to_cpu_16(uh->src_port);
	else
		port_no = rte_be_to_cpu_16(uh->dst_port);
	switch (port_no) {
	case RTE_VXLAN_DEFAULT_PORT: {
		*off += sizeof(struct rte_vxlan_hdr);
		hdr_lens->inner_l2_len = RTE_ETHER_VXLAN_HLEN;
		*proto = RTE_VXLAN_GPE_TYPE_ETH; /* just for eth header parse. */
		return RTE_PTYPE_TUNNEL_VXLAN;
	}
	case RTE_VXLAN_GPE_DEFAULT_PORT: {
		const struct rte_vxlan_gpe_hdr *vgh;
		struct rte_vxlan_gpe_hdr vgh_copy;
		vgh = rte_pktmbuf_read(m, *off, sizeof(*vgh), &vgh_copy);
		if (unlikely(vgh == NULL))
			return 0;
		*off += sizeof(struct rte_vxlan_gpe_hdr);
		hdr_lens->inner_l2_len = RTE_ETHER_VXLAN_GPE_HLEN;
		*proto = vgh->proto;

		return RTE_PTYPE_TUNNEL_VXLAN_GPE;
	}
	case RTE_GTPC_UDP_PORT:
	case RTE_GTPU_UDP_PORT: {
		const struct rte_gtp_hdr *gh;
		struct rte_gtp_hdr gh_copy;
		uint8_t gtp_len;
		uint8_t ip_ver;
		gh = rte_pktmbuf_read(m, *off, sizeof(*gh), &gh_copy);
		if (unlikely(gh == NULL))
			return 0;
		gtp_len = sizeof(*gh);
		if (gh->e || gh->s || gh->pn)
			gtp_len += sizeof(struct rte_gtp_hdr_ext_word);
		/*
		 * Check message type. If message type is 0xff, it is
		 * a GTP data packet. If not, it is a GTP control packet
		 */
		if (gh->msg_type == 0xff) {
			ip_ver = *(const uint8_t *)((const char *)gh + gtp_len);
			ip_ver = (ip_ver) & 0xf0;
			if (ip_ver == RTE_GTP_TYPE_IPV4)
				*proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
			else if (ip_ver == RTE_GTP_TYPE_IPV6)
				*proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
			else
				*proto = 0;
		} else {
			*proto = 0;
		}
		*off += gtp_len;
		hdr_lens->inner_l2_len = gtp_len + sizeof(struct rte_udp_hdr);
		if (port_no == RTE_GTPC_UDP_PORT)
			return RTE_PTYPE_TUNNEL_GTPC;
		else if (port_no == RTE_GTPU_UDP_PORT)
			return RTE_PTYPE_TUNNEL_GTPU;
		return 0;
	}
	case RTE_GENEVE_DEFAULT_PORT: {
		const struct rte_geneve_hdr *gnh;
		struct rte_geneve_hdr gnh_copy;
		uint16_t geneve_len;
		gnh = rte_pktmbuf_read(m, *off, sizeof(*gnh), &gnh_copy);
		if (unlikely(gnh == NULL))
			return 0;
		geneve_len = sizeof(*gnh) + gnh->opt_len * 4;
		*off += geneve_len;
		hdr_lens->inner_l2_len = sizeof(struct rte_udp_hdr) + geneve_len;
		*proto = gnh->proto;
		if (gnh->proto == 0)
			*proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		return RTE_PTYPE_TUNNEL_GENEVE;
	}
	default:
		return 0;
	}
}

/* parse ipv6 extended headers, update offset and return next proto */
RTE_EXPORT_SYMBOL(rte_net_skip_ip6_ext)
int
rte_net_skip_ip6_ext(uint16_t proto, const struct rte_mbuf *m, uint32_t *off,
	int *frag)
{
	struct ext_hdr {
		uint8_t next_hdr;
		uint8_t len;
	};
	const struct ext_hdr *xh;
	struct ext_hdr xh_copy;
	unsigned int i;

	*frag = 0;

#define MAX_EXT_HDRS 5
	for (i = 0; i < MAX_EXT_HDRS; i++) {
		switch (proto) {
		case IPPROTO_HOPOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_DSTOPTS:
			xh = rte_pktmbuf_read(m, *off, sizeof(*xh),
				&xh_copy);
			if (xh == NULL)
				return -1;
			*off += (xh->len + 1) * 8;
			proto = xh->next_hdr;
			break;
		case IPPROTO_FRAGMENT:
			xh = rte_pktmbuf_read(m, *off, sizeof(*xh),
				&xh_copy);
			if (xh == NULL)
				return -1;
			*off += 8;
			proto = xh->next_hdr;
			*frag = 1;
			return proto; /* this is always the last ext hdr */
		case IPPROTO_NONE:
			return 0;
		default:
			return proto;
		}
	}
	return -1;
}

/* parse mbuf data to get packet type */
RTE_EXPORT_SYMBOL(rte_net_get_ptype)
uint32_t rte_net_get_ptype(const struct rte_mbuf *m,
	struct rte_net_hdr_lens *hdr_lens, uint32_t layers)
{
	struct rte_net_hdr_lens local_hdr_lens;
	const struct rte_ether_hdr *eh;
	struct rte_ether_hdr eh_copy;
	uint32_t pkt_type = RTE_PTYPE_L2_ETHER;
	uint32_t off = 0;
	uint16_t proto;
	int ret;

	if (hdr_lens == NULL)
		hdr_lens = &local_hdr_lens;

	eh = rte_pktmbuf_read(m, off, sizeof(*eh), &eh_copy);
	if (unlikely(eh == NULL))
		return 0;
	proto = eh->ether_type;
	off = sizeof(*eh);
	hdr_lens->l2_len = off;

	if ((layers & RTE_PTYPE_L2_MASK) == 0)
		return 0;

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
		goto l3; /* fast path if packet is IPv4 */

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
		const struct rte_vlan_hdr *vh;
		struct rte_vlan_hdr vh_copy;

		pkt_type = RTE_PTYPE_L2_ETHER_VLAN;
		vh = rte_pktmbuf_read(m, off, sizeof(*vh), &vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += sizeof(*vh);
		hdr_lens->l2_len += sizeof(*vh);
		proto = vh->eth_proto;
	} else if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_QINQ)) {
		const struct rte_vlan_hdr *vh;
		struct rte_vlan_hdr vh_copy;

		pkt_type = RTE_PTYPE_L2_ETHER_QINQ;
		vh = rte_pktmbuf_read(m, off + sizeof(*vh), sizeof(*vh),
			&vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += 2 * sizeof(*vh);
		hdr_lens->l2_len += 2 * sizeof(*vh);
		proto = vh->eth_proto;
	} else if ((proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_MPLS)) ||
		(proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_MPLSM))) {
		unsigned int i;
		const struct rte_mpls_hdr *mh;
		struct rte_mpls_hdr mh_copy;

#define MAX_MPLS_HDR 5
		for (i = 0; i < MAX_MPLS_HDR; i++) {
			mh = rte_pktmbuf_read(m, off + (i * sizeof(*mh)),
				sizeof(*mh), &mh_copy);
			if (unlikely(mh == NULL))
				return pkt_type;
		}
		if (i == MAX_MPLS_HDR)
			return pkt_type;
		pkt_type = RTE_PTYPE_L2_ETHER_MPLS;
		hdr_lens->l2_len += (sizeof(*mh) * i);
		return pkt_type;
	}

l3:
	if ((layers & RTE_PTYPE_L3_MASK) == 0)
		return pkt_type;

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
		const struct rte_ipv4_hdr *ip4h;
		struct rte_ipv4_hdr ip4h_copy;

		ip4h = rte_pktmbuf_read(m, off, sizeof(*ip4h), &ip4h_copy);
		if (unlikely(ip4h == NULL))
			return pkt_type;

		pkt_type |= ptype_l3_ip(ip4h->version_ihl);
		hdr_lens->l3_len = rte_ipv4_hdr_len(ip4h);
		off += hdr_lens->l3_len;

		if ((layers & RTE_PTYPE_L4_MASK) == 0)
			return pkt_type;

		if (ip4h->fragment_offset & rte_cpu_to_be_16(
				RTE_IPV4_HDR_OFFSET_MASK | RTE_IPV4_HDR_MF_FLAG)) {
			pkt_type |= RTE_PTYPE_L4_FRAG;
			hdr_lens->l4_len = 0;
			return pkt_type;
		}
		proto = ip4h->next_proto_id;
		pkt_type |= ptype_l4(proto);
	} else if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
		const struct rte_ipv6_hdr *ip6h;
		struct rte_ipv6_hdr ip6h_copy;
		int frag = 0;

		ip6h = rte_pktmbuf_read(m, off, sizeof(*ip6h), &ip6h_copy);
		if (unlikely(ip6h == NULL))
			return pkt_type;

		proto = ip6h->proto;
		hdr_lens->l3_len = sizeof(*ip6h);
		off += hdr_lens->l3_len;
		pkt_type |= ptype_l3_ip6(proto);
		if ((pkt_type & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV6_EXT) {
			ret = rte_net_skip_ip6_ext(proto, m, &off, &frag);
			if (ret < 0)
				return pkt_type;
			proto = ret;
			hdr_lens->l3_len = off - hdr_lens->l2_len;
		}
		if (proto == 0)
			return pkt_type;

		if ((layers & RTE_PTYPE_L4_MASK) == 0)
			return pkt_type;

		if (frag) {
			pkt_type |= RTE_PTYPE_L4_FRAG;
			hdr_lens->l4_len = 0;
			return pkt_type;
		}
		pkt_type |= ptype_l4(proto);
	}

	if ((pkt_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP) {
		hdr_lens->l4_len = sizeof(struct rte_udp_hdr);
		if ((layers & RTE_PTYPE_TUNNEL_MASK) == 0)
			return pkt_type;
		pkt_type |= ptype_tunnel_with_udp(&proto, m, &off, hdr_lens);
	} else if ((pkt_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP) {
		const struct rte_tcp_hdr *th;
		struct rte_tcp_hdr th_copy;

		th = rte_pktmbuf_read(m, off, sizeof(*th), &th_copy);
		if (unlikely(th == NULL))
			return pkt_type & (RTE_PTYPE_L2_MASK |
				RTE_PTYPE_L3_MASK);
		hdr_lens->l4_len = (th->data_off & 0xf0) >> 2;
		return pkt_type;
	} else if ((pkt_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_SCTP) {
		hdr_lens->l4_len = sizeof(struct rte_sctp_hdr);
		return pkt_type;
	} else {
		uint32_t prev_off = off;

		hdr_lens->l4_len = 0;

		if ((layers & RTE_PTYPE_TUNNEL_MASK) == 0)
			return pkt_type;

		pkt_type |= ptype_tunnel_without_udp(&proto, m, &off);
		hdr_lens->tunnel_len = off - prev_off;
	}

	/* same job for inner header: we need to duplicate the code
	 * because the packet types do not have the same value.
	 */
	if ((layers & RTE_PTYPE_INNER_L2_MASK) == 0)
		return pkt_type;

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_TEB) ||
		proto == rte_cpu_to_be_16(RTE_GENEVE_TYPE_ETH) ||
		proto == RTE_VXLAN_GPE_TYPE_ETH) {
		eh = rte_pktmbuf_read(m, off, sizeof(*eh), &eh_copy);
		if (unlikely(eh == NULL))
			return pkt_type;
		pkt_type |= RTE_PTYPE_INNER_L2_ETHER;
		proto = eh->ether_type;
		off += sizeof(*eh);
		hdr_lens->inner_l2_len += sizeof(*eh);
	}

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
		const struct rte_vlan_hdr *vh;
		struct rte_vlan_hdr vh_copy;

		pkt_type &= ~RTE_PTYPE_INNER_L2_MASK;
		pkt_type |= RTE_PTYPE_INNER_L2_ETHER_VLAN;
		vh = rte_pktmbuf_read(m, off, sizeof(*vh), &vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += sizeof(*vh);
		hdr_lens->inner_l2_len += sizeof(*vh);
		proto = vh->eth_proto;
	} else if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_QINQ)) {
		const struct rte_vlan_hdr *vh;
		struct rte_vlan_hdr vh_copy;

		pkt_type &= ~RTE_PTYPE_INNER_L2_MASK;
		pkt_type |= RTE_PTYPE_INNER_L2_ETHER_QINQ;
		vh = rte_pktmbuf_read(m, off + sizeof(*vh), sizeof(*vh),
			&vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += 2 * sizeof(*vh);
		hdr_lens->inner_l2_len += 2 * sizeof(*vh);
		proto = vh->eth_proto;
	}

	if ((layers & RTE_PTYPE_INNER_L3_MASK) == 0)
		return pkt_type;

	if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) ||
		proto == RTE_VXLAN_GPE_TYPE_IPV4) {
		const struct rte_ipv4_hdr *ip4h;
		struct rte_ipv4_hdr ip4h_copy;

		ip4h = rte_pktmbuf_read(m, off, sizeof(*ip4h), &ip4h_copy);
		if (unlikely(ip4h == NULL))
			return pkt_type;

		pkt_type |= ptype_inner_l3_ip(ip4h->version_ihl);
		hdr_lens->inner_l3_len = rte_ipv4_hdr_len(ip4h);
		off += hdr_lens->inner_l3_len;

		if ((layers & RTE_PTYPE_INNER_L4_MASK) == 0)
			return pkt_type;
		if (ip4h->fragment_offset &
				rte_cpu_to_be_16(RTE_IPV4_HDR_OFFSET_MASK |
					RTE_IPV4_HDR_MF_FLAG)) {
			pkt_type |= RTE_PTYPE_INNER_L4_FRAG;
			hdr_lens->inner_l4_len = 0;
			return pkt_type;
		}
		proto = ip4h->next_proto_id;
		pkt_type |= ptype_inner_l4(proto);
	} else if (proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6) ||
			proto == RTE_VXLAN_GPE_TYPE_IPV6) {
		const struct rte_ipv6_hdr *ip6h;
		struct rte_ipv6_hdr ip6h_copy;
		int frag = 0;

		ip6h = rte_pktmbuf_read(m, off, sizeof(*ip6h), &ip6h_copy);
		if (unlikely(ip6h == NULL))
			return pkt_type;

		proto = ip6h->proto;
		hdr_lens->inner_l3_len = sizeof(*ip6h);
		off += hdr_lens->inner_l3_len;
		pkt_type |= ptype_inner_l3_ip6(proto);
		if ((pkt_type & RTE_PTYPE_INNER_L3_MASK) ==
				RTE_PTYPE_INNER_L3_IPV6_EXT) {
			uint32_t prev_off;

			prev_off = off;
			ret = rte_net_skip_ip6_ext(proto, m, &off, &frag);
			if (ret < 0)
				return pkt_type;
			proto = ret;
			hdr_lens->inner_l3_len += off - prev_off;
		}
		if (proto == 0)
			return pkt_type;

		if ((layers & RTE_PTYPE_INNER_L4_MASK) == 0)
			return pkt_type;

		if (frag) {
			pkt_type |= RTE_PTYPE_INNER_L4_FRAG;
			hdr_lens->inner_l4_len = 0;
			return pkt_type;
		}
		pkt_type |= ptype_inner_l4(proto);
	}

	if ((pkt_type & RTE_PTYPE_INNER_L4_MASK) == RTE_PTYPE_INNER_L4_UDP) {
		hdr_lens->inner_l4_len = sizeof(struct rte_udp_hdr);
	} else if ((pkt_type & RTE_PTYPE_INNER_L4_MASK) ==
			RTE_PTYPE_INNER_L4_TCP) {
		const struct rte_tcp_hdr *th;
		struct rte_tcp_hdr th_copy;

		th = rte_pktmbuf_read(m, off, sizeof(*th), &th_copy);
		if (unlikely(th == NULL))
			return pkt_type & (RTE_PTYPE_INNER_L2_MASK |
				RTE_PTYPE_INNER_L3_MASK);
		hdr_lens->inner_l4_len = (th->data_off & 0xf0) >> 2;
	} else if ((pkt_type & RTE_PTYPE_INNER_L4_MASK) ==
			RTE_PTYPE_INNER_L4_SCTP) {
		hdr_lens->inner_l4_len = sizeof(struct rte_sctp_hdr);
	} else {
		hdr_lens->inner_l4_len = 0;
	}

	return pkt_type;
}
