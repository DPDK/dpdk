/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mbuf_ptype.h>
#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_gre.h>
#include <rte_net.h>

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
ptype_tunnel(uint16_t *proto, const struct rte_mbuf *m,
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
		const struct gre_hdr *gh;
		struct gre_hdr gh_copy;
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
		if (*proto == rte_cpu_to_be_16(ETHER_TYPE_TEB))
			return RTE_PTYPE_TUNNEL_NVGRE;
		else
			return RTE_PTYPE_TUNNEL_GRE;
	}
	case IPPROTO_IPIP:
		*proto = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		return RTE_PTYPE_TUNNEL_IP;
	case IPPROTO_IPV6:
		*proto = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
		return RTE_PTYPE_TUNNEL_IP; /* IP is also valid for IPv6 */
	default:
		return 0;
	}
}

/* get the ipv4 header length */
static uint8_t
ip4_hlen(const struct ipv4_hdr *hdr)
{
	return (hdr->version_ihl & 0xf) * 4;
}

/* parse ipv6 extended headers, update offset and return next proto */
static uint16_t
skip_ip6_ext(uint16_t proto, const struct rte_mbuf *m, uint32_t *off,
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
				return 0;
			*off += (xh->len + 1) * 8;
			proto = xh->next_hdr;
			break;
		case IPPROTO_FRAGMENT:
			xh = rte_pktmbuf_read(m, *off, sizeof(*xh),
				&xh_copy);
			if (xh == NULL)
				return 0;
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
	return 0;
}

/* parse mbuf data to get packet type */
uint32_t rte_net_get_ptype(const struct rte_mbuf *m,
	struct rte_net_hdr_lens *hdr_lens, uint32_t layers)
{
	struct rte_net_hdr_lens local_hdr_lens;
	const struct ether_hdr *eh;
	struct ether_hdr eh_copy;
	uint32_t pkt_type = RTE_PTYPE_L2_ETHER;
	uint32_t off = 0;
	uint16_t proto;

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

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		goto l3; /* fast path if packet is IPv4 */

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_VLAN)) {
		const struct vlan_hdr *vh;
		struct vlan_hdr vh_copy;

		pkt_type = RTE_PTYPE_L2_ETHER_VLAN;
		vh = rte_pktmbuf_read(m, off, sizeof(*vh), &vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += sizeof(*vh);
		hdr_lens->l2_len += sizeof(*vh);
		proto = vh->eth_proto;
	} else if (proto == rte_cpu_to_be_16(ETHER_TYPE_QINQ)) {
		const struct vlan_hdr *vh;
		struct vlan_hdr vh_copy;

		pkt_type = RTE_PTYPE_L2_ETHER_QINQ;
		vh = rte_pktmbuf_read(m, off + sizeof(*vh), sizeof(*vh),
			&vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += 2 * sizeof(*vh);
		hdr_lens->l2_len += 2 * sizeof(*vh);
		proto = vh->eth_proto;
	}

 l3:
	if ((layers & RTE_PTYPE_L3_MASK) == 0)
		return pkt_type;

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
		const struct ipv4_hdr *ip4h;
		struct ipv4_hdr ip4h_copy;

		ip4h = rte_pktmbuf_read(m, off, sizeof(*ip4h), &ip4h_copy);
		if (unlikely(ip4h == NULL))
			return pkt_type;

		pkt_type |= ptype_l3_ip(ip4h->version_ihl);
		hdr_lens->l3_len = ip4_hlen(ip4h);
		off += hdr_lens->l3_len;

		if ((layers & RTE_PTYPE_L4_MASK) == 0)
			return pkt_type;

		if (ip4h->fragment_offset & rte_cpu_to_be_16(
				IPV4_HDR_OFFSET_MASK | IPV4_HDR_MF_FLAG)) {
			pkt_type |= RTE_PTYPE_L4_FRAG;
			hdr_lens->l4_len = 0;
			return pkt_type;
		}
		proto = ip4h->next_proto_id;
		pkt_type |= ptype_l4(proto);
	} else if (proto == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		const struct ipv6_hdr *ip6h;
		struct ipv6_hdr ip6h_copy;
		int frag = 0;

		ip6h = rte_pktmbuf_read(m, off, sizeof(*ip6h), &ip6h_copy);
		if (unlikely(ip6h == NULL))
			return pkt_type;

		proto = ip6h->proto;
		hdr_lens->l3_len = sizeof(*ip6h);
		off += hdr_lens->l3_len;
		pkt_type |= ptype_l3_ip6(proto);
		if ((pkt_type & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV6_EXT) {
			proto = skip_ip6_ext(proto, m, &off, &frag);
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
		hdr_lens->l4_len = sizeof(struct udp_hdr);
		return pkt_type;
	} else if ((pkt_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP) {
		const struct tcp_hdr *th;
		struct tcp_hdr th_copy;

		th = rte_pktmbuf_read(m, off, sizeof(*th), &th_copy);
		if (unlikely(th == NULL))
			return pkt_type & (RTE_PTYPE_L2_MASK |
				RTE_PTYPE_L3_MASK);
		hdr_lens->l4_len = (th->data_off & 0xf0) >> 2;
		return pkt_type;
	} else if ((pkt_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_SCTP) {
		hdr_lens->l4_len = sizeof(struct sctp_hdr);
		return pkt_type;
	} else {
		uint32_t prev_off = off;

		hdr_lens->l4_len = 0;

		if ((layers & RTE_PTYPE_TUNNEL_MASK) == 0)
			return pkt_type;

		pkt_type |= ptype_tunnel(&proto, m, &off);
		hdr_lens->tunnel_len = off - prev_off;
	}

	/* same job for inner header: we need to duplicate the code
	 * because the packet types do not have the same value.
	 */
	if ((layers & RTE_PTYPE_INNER_L2_MASK) == 0)
		return pkt_type;

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_TEB)) {
		eh = rte_pktmbuf_read(m, off, sizeof(*eh), &eh_copy);
		if (unlikely(eh == NULL))
			return pkt_type;
		pkt_type |= RTE_PTYPE_INNER_L2_ETHER;
		proto = eh->ether_type;
		off += sizeof(*eh);
		hdr_lens->inner_l2_len = sizeof(*eh);
	}

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_VLAN)) {
		const struct vlan_hdr *vh;
		struct vlan_hdr vh_copy;

		pkt_type &= ~RTE_PTYPE_INNER_L2_MASK;
		pkt_type |= RTE_PTYPE_INNER_L2_ETHER_VLAN;
		vh = rte_pktmbuf_read(m, off, sizeof(*vh), &vh_copy);
		if (unlikely(vh == NULL))
			return pkt_type;
		off += sizeof(*vh);
		hdr_lens->inner_l2_len += sizeof(*vh);
		proto = vh->eth_proto;
	} else if (proto == rte_cpu_to_be_16(ETHER_TYPE_QINQ)) {
		const struct vlan_hdr *vh;
		struct vlan_hdr vh_copy;

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

	if (proto == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
		const struct ipv4_hdr *ip4h;
		struct ipv4_hdr ip4h_copy;

		ip4h = rte_pktmbuf_read(m, off, sizeof(*ip4h), &ip4h_copy);
		if (unlikely(ip4h == NULL))
			return pkt_type;

		pkt_type |= ptype_inner_l3_ip(ip4h->version_ihl);
		hdr_lens->inner_l3_len = ip4_hlen(ip4h);
		off += hdr_lens->inner_l3_len;

		if ((layers & RTE_PTYPE_INNER_L4_MASK) == 0)
			return pkt_type;
		if (ip4h->fragment_offset &
				rte_cpu_to_be_16(IPV4_HDR_OFFSET_MASK |
					IPV4_HDR_MF_FLAG)) {
			pkt_type |= RTE_PTYPE_INNER_L4_FRAG;
			hdr_lens->inner_l4_len = 0;
			return pkt_type;
		}
		proto = ip4h->next_proto_id;
		pkt_type |= ptype_inner_l4(proto);
	} else if (proto == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		const struct ipv6_hdr *ip6h;
		struct ipv6_hdr ip6h_copy;
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
			proto = skip_ip6_ext(proto, m, &off, &frag);
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
		hdr_lens->inner_l4_len = sizeof(struct udp_hdr);
	} else if ((pkt_type & RTE_PTYPE_INNER_L4_MASK) ==
			RTE_PTYPE_INNER_L4_TCP) {
		const struct tcp_hdr *th;
		struct tcp_hdr th_copy;

		th = rte_pktmbuf_read(m, off, sizeof(*th), &th_copy);
		if (unlikely(th == NULL))
			return pkt_type & (RTE_PTYPE_INNER_L2_MASK |
				RTE_PTYPE_INNER_L3_MASK);
		hdr_lens->inner_l4_len = (th->data_off & 0xf0) >> 2;
	} else if ((pkt_type & RTE_PTYPE_INNER_L4_MASK) ==
			RTE_PTYPE_INNER_L4_SCTP) {
		hdr_lens->inner_l4_len = sizeof(struct sctp_hdr);
	} else {
		hdr_lens->inner_l4_len = 0;
	}

	return pkt_type;
}
