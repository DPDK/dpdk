/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_PMD_ICE_H_
#define _RTE_PMD_ICE_H_

/**
 * @file rte_pmd_ice.h
 *
 * ice PMD specific functions.
 *
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The supported network protocol extraction metadata format.
 */
union rte_net_ice_proto_xtr_metadata {
	uint32_t metadata;

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

	uint32_t ip_ofs;
};

/* Offset of mbuf dynamic field for protocol extraction data */
extern int rte_net_ice_dynfield_proto_xtr_metadata_offs;

/* Mask of mbuf dynamic flags for protocol extraction type */
extern uint64_t rte_net_ice_dynflag_proto_xtr_vlan_mask;
extern uint64_t rte_net_ice_dynflag_proto_xtr_ipv4_mask;
extern uint64_t rte_net_ice_dynflag_proto_xtr_ipv6_mask;
extern uint64_t rte_net_ice_dynflag_proto_xtr_ipv6_flow_mask;
extern uint64_t rte_net_ice_dynflag_proto_xtr_tcp_mask;
extern uint64_t rte_net_ice_dynflag_proto_xtr_ip_offset_mask;

/**
 * The mbuf dynamic field pointer for protocol extraction metadata.
 */
#define RTE_NET_ICE_DYNF_PROTO_XTR_METADATA(m) \
	RTE_MBUF_DYNFIELD((m), \
			  rte_net_ice_dynfield_proto_xtr_metadata_offs, \
			  uint32_t *)

/**
 * The mbuf dynamic flag for VLAN protocol extraction metadata, it is valid
 * when dev_args 'proto_xtr' has 'vlan' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_VLAN \
	(rte_net_ice_dynflag_proto_xtr_vlan_mask)

/**
 * The mbuf dynamic flag for IPv4 protocol extraction metadata, it is valid
 * when dev_args 'proto_xtr' has 'ipv4' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_IPV4 \
	(rte_net_ice_dynflag_proto_xtr_ipv4_mask)

/**
 * The mbuf dynamic flag for IPv6 protocol extraction metadata, it is valid
 * when dev_args 'proto_xtr' has 'ipv6' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_IPV6 \
	(rte_net_ice_dynflag_proto_xtr_ipv6_mask)

/**
 * The mbuf dynamic flag for IPv6 with flow protocol extraction metadata, it is
 * valid when dev_args 'proto_xtr' has 'ipv6_flow' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_IPV6_FLOW \
	(rte_net_ice_dynflag_proto_xtr_ipv6_flow_mask)

/**
 * The mbuf dynamic flag for TCP protocol extraction metadata, it is valid
 * when dev_args 'proto_xtr' has 'tcp' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_TCP \
	(rte_net_ice_dynflag_proto_xtr_tcp_mask)

/**
 * The mbuf dynamic flag for IP_OFFSET extraction metadata, it is valid
 * when dev_args 'proto_xtr' has 'ip_offset' specified.
 */
#define RTE_PKT_RX_DYNF_PROTO_XTR_IP_OFFSET \
	(rte_net_ice_dynflag_proto_xtr_ip_offset_mask)

/**
 * Check if mbuf dynamic field for protocol extraction metadata is registered.
 *
 * @return
 *   True if registered, false otherwise.
 */
__rte_experimental
static __rte_always_inline int
rte_net_ice_dynf_proto_xtr_metadata_avail(void)
{
	return rte_net_ice_dynfield_proto_xtr_metadata_offs != -1;
}

/**
 * Get the mbuf dynamic field for protocol extraction metadata.
 *
 * @param m
 *    The pointer to the mbuf.
 * @return
 *   The saved protocol extraction metadata.
 */
__rte_experimental
static __rte_always_inline uint32_t
rte_net_ice_dynf_proto_xtr_metadata_get(struct rte_mbuf *m)
{
	return *RTE_NET_ICE_DYNF_PROTO_XTR_METADATA(m);
}

/**
 * Dump the mbuf dynamic field for protocol extraction metadata.
 *
 * @param m
 *    The pointer to the mbuf.
 */
__rte_experimental
static inline void
rte_net_ice_dump_proto_xtr_metadata(struct rte_mbuf *m)
{
	union rte_net_ice_proto_xtr_metadata data;

	if (!rte_net_ice_dynf_proto_xtr_metadata_avail())
		return;

	data.metadata = rte_net_ice_dynf_proto_xtr_metadata_get(m);

	if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_VLAN)
		printf(" - Protocol Extraction:[0x%04x:0x%04x],vlan,stag=%u:%u:%u,ctag=%u:%u:%u",
		       data.raw.data0, data.raw.data1,
		       data.vlan.stag_pcp,
		       data.vlan.stag_dei,
		       data.vlan.stag_vid,
		       data.vlan.ctag_pcp,
		       data.vlan.ctag_dei,
		       data.vlan.ctag_vid);
	else if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_IPV4)
		printf(" - Protocol Extraction:[0x%04x:0x%04x],ipv4,ver=%u,hdrlen=%u,tos=%u,ttl=%u,proto=%u",
		       data.raw.data0, data.raw.data1,
		       data.ipv4.version,
		       data.ipv4.ihl,
		       data.ipv4.tos,
		       data.ipv4.ttl,
		       data.ipv4.protocol);
	else if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_IPV6)
		printf(" - Protocol Extraction:[0x%04x:0x%04x],ipv6,ver=%u,tc=%u,flow_hi4=0x%x,nexthdr=%u,hoplimit=%u",
		       data.raw.data0, data.raw.data1,
		       data.ipv6.version,
		       data.ipv6.tc,
		       data.ipv6.flowhi4,
		       data.ipv6.nexthdr,
		       data.ipv6.hoplimit);
	else if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_IPV6_FLOW)
		printf(" - Protocol Extraction:[0x%04x:0x%04x],ipv6_flow,ver=%u,tc=%u,flow=0x%x%04x",
		       data.raw.data0, data.raw.data1,
		       data.ipv6_flow.version,
		       data.ipv6_flow.tc,
		       data.ipv6_flow.flowhi4,
		       data.ipv6_flow.flowlo16);
	else if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_TCP)
		printf(" - Protocol Extraction:[0x%04x:0x%04x],tcp,doff=%u,flags=%s%s%s%s%s%s%s%s",
		       data.raw.data0, data.raw.data1,
		       data.tcp.doff,
		       data.tcp.cwr ? "C" : "",
		       data.tcp.ece ? "E" : "",
		       data.tcp.urg ? "U" : "",
		       data.tcp.ack ? "A" : "",
		       data.tcp.psh ? "P" : "",
		       data.tcp.rst ? "R" : "",
		       data.tcp.syn ? "S" : "",
		       data.tcp.fin ? "F" : "");
	else if (m->ol_flags & RTE_PKT_RX_DYNF_PROTO_XTR_IP_OFFSET)
		printf(" - Protocol Offset:ip_offset=%u",
		       data.ip_ofs);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_ICE_H_ */
