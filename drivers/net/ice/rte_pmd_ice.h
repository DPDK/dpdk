/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_PMD_ICE_H_
#define _RTE_PMD_ICE_H_

#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#ifdef __cplusplus
extern "C" {
#endif

enum proto_xtr_type {
	PROTO_XTR_NONE,
	PROTO_XTR_VLAN,
	PROTO_XTR_IPV4,
	PROTO_XTR_IPV6,
	PROTO_XTR_IPV6_FLOW,
	PROTO_XTR_TCP,
};

struct proto_xtr_flds {
	union {
		struct {
			uint16_t data0;
			uint16_t data1;
		} raw;
		struct {
			uint16_t stag_vid:12,
				 stag_dei:1,
				 stag_pcp:3;
			uint16_t ctag_vid:12,
				 ctag_dei:1,
				 ctag_pcp:3;
		} vlan;
		struct {
			uint16_t protocol:8,
				 ttl:8;
			uint16_t tos:8,
				 ihl:4,
				 version:4;
		} ipv4;
		struct {
			uint16_t hoplimit:8,
				 nexthdr:8;
			uint16_t flowhi4:4,
				 tc:8,
				 version:4;
		} ipv6;
		struct {
			uint16_t flowlo16;
			uint16_t flowhi4:4,
				 tc:8,
				 version:4;
		} ipv6_flow;
		struct {
			uint16_t fin:1,
				 syn:1,
				 rst:1,
				 psh:1,
				 ack:1,
				 urg:1,
				 ece:1,
				 cwr:1,
				 res1:4,
				 doff:4;
			uint16_t rsvd;
		} tcp;
	} u;

	uint16_t rsvd;

	uint8_t type;

#define PROTO_XTR_MAGIC_ID	0xCE
	uint8_t magic;
};

static inline void
init_proto_xtr_flds(struct rte_mbuf *mb)
{
	mb->udata64 = 0;
}

static inline struct proto_xtr_flds *
get_proto_xtr_flds(struct rte_mbuf *mb)
{
	RTE_BUILD_BUG_ON(sizeof(struct proto_xtr_flds) > sizeof(mb->udata64));

	return (struct proto_xtr_flds *)&mb->udata64;
}

static inline void
dump_proto_xtr_flds(struct rte_mbuf *mb)
{
	struct proto_xtr_flds *xtr = get_proto_xtr_flds(mb);

	if (xtr->magic != PROTO_XTR_MAGIC_ID || xtr->type == PROTO_XTR_NONE)
		return;

	printf(" - Protocol Extraction:[0x%04x:0x%04x],",
	       xtr->u.raw.data0, xtr->u.raw.data1);

	if (xtr->type == PROTO_XTR_VLAN)
		printf("vlan,stag=%u:%u:%u,ctag=%u:%u:%u ",
		       xtr->u.vlan.stag_pcp,
		       xtr->u.vlan.stag_dei,
		       xtr->u.vlan.stag_vid,
		       xtr->u.vlan.ctag_pcp,
		       xtr->u.vlan.ctag_dei,
		       xtr->u.vlan.ctag_vid);
	else if (xtr->type == PROTO_XTR_IPV4)
		printf("ipv4,ver=%u,hdrlen=%u,tos=%u,ttl=%u,proto=%u ",
		       xtr->u.ipv4.version,
		       xtr->u.ipv4.ihl,
		       xtr->u.ipv4.tos,
		       xtr->u.ipv4.ttl,
		       xtr->u.ipv4.protocol);
	else if (xtr->type == PROTO_XTR_IPV6)
		printf("ipv6,ver=%u,tc=%u,flow_hi4=0x%x,nexthdr=%u,hoplimit=%u ",
		       xtr->u.ipv6.version,
		       xtr->u.ipv6.tc,
		       xtr->u.ipv6.flowhi4,
		       xtr->u.ipv6.nexthdr,
		       xtr->u.ipv6.hoplimit);
	else if (xtr->type == PROTO_XTR_IPV6_FLOW)
		printf("ipv6_flow,ver=%u,tc=%u,flow=0x%x%04x ",
		       xtr->u.ipv6_flow.version,
		       xtr->u.ipv6_flow.tc,
		       xtr->u.ipv6_flow.flowhi4,
		       xtr->u.ipv6_flow.flowlo16);
	else if (xtr->type == PROTO_XTR_TCP)
		printf("tcp,doff=%u,flags=%s%s%s%s%s%s%s%s ",
		       xtr->u.tcp.doff,
		       xtr->u.tcp.cwr ? "C" : "",
		       xtr->u.tcp.ece ? "E" : "",
		       xtr->u.tcp.urg ? "U" : "",
		       xtr->u.tcp.ack ? "A" : "",
		       xtr->u.tcp.psh ? "P" : "",
		       xtr->u.tcp.rst ? "R" : "",
		       xtr->u.tcp.syn ? "S" : "",
		       xtr->u.tcp.fin ? "F" : "");
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_ICE_H_ */
