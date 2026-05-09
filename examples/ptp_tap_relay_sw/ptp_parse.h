/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 *
 * PTP packet parser — locates PTP headers through L2, VLAN, and UDP
 * encapsulations. This is a DPI helper for use within example
 * applications; it does not belong in the core library.
 */

#ifndef _PTP_PARSE_H_
#define _PTP_PARSE_H_

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_ptp.h>

/** Not a PTP packet. */
#define PTP_MSGTYPE_INVALID  (-1)

/**
 * Locate the PTP header within a packet.
 *
 * Handles L2 (EtherType 0x88F7), VLAN-tagged L2 (single/double,
 * TPIDs 0x8100/0x88A8), PTP over UDP/IPv4, PTP over UDP/IPv6,
 * and VLAN-tagged UDP variants.
 *
 * @param m
 *   Pointer to the mbuf.
 * @return
 *   Pointer to the PTP header, or NULL if not a PTP packet.
 */
static inline struct rte_ptp_hdr *
ptp_hdr_find(const struct rte_mbuf *m)
{
	const struct rte_ether_hdr *eth;
	uint16_t ether_type;
	uint32_t offset;

	if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr))
		return NULL;

	eth = rte_pktmbuf_mtod(m, const struct rte_ether_hdr *);
	ether_type = rte_be_to_cpu_16(eth->ether_type);
	offset = sizeof(struct rte_ether_hdr);

	/* Strip VLAN / QinQ tags */
	if (ether_type == RTE_ETHER_TYPE_VLAN ||
	    ether_type == RTE_ETHER_TYPE_QINQ) {
		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_vlan_hdr))
			return NULL;
		const struct rte_vlan_hdr *vlan =
			rte_pktmbuf_mtod_offset(m,
				const struct rte_vlan_hdr *, offset);
		ether_type = rte_be_to_cpu_16(vlan->eth_proto);
		offset += sizeof(struct rte_vlan_hdr);

		/* Second tag (QinQ inner or stacked VLAN) */
		if (ether_type == RTE_ETHER_TYPE_VLAN ||
		    ether_type == RTE_ETHER_TYPE_QINQ) {
			if (rte_pktmbuf_data_len(m) <
			    offset + sizeof(struct rte_vlan_hdr))
				return NULL;
			vlan = rte_pktmbuf_mtod_offset(m,
				const struct rte_vlan_hdr *, offset);
			ether_type = rte_be_to_cpu_16(vlan->eth_proto);
			offset += sizeof(struct rte_vlan_hdr);
		}
	}

	/* L2 PTP: EtherType 0x88F7 */
	if (ether_type == RTE_ETHER_TYPE_1588) {
		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_ptp_hdr))
			return NULL;
		return rte_pktmbuf_mtod_offset(m,
			struct rte_ptp_hdr *, offset);
	}

	/* PTP over UDP/IPv4 */
	if (ether_type == RTE_ETHER_TYPE_IPV4) {
		const struct rte_ipv4_hdr *iph;
		uint16_t ihl;

		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_ipv4_hdr))
			return NULL;

		iph = rte_pktmbuf_mtod_offset(m,
			const struct rte_ipv4_hdr *, offset);
		if (iph->next_proto_id != IPPROTO_UDP)
			return NULL;

		ihl = (iph->version_ihl & 0x0F) * 4;
		if (ihl < 20)
			return NULL;
		offset += ihl;

		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_udp_hdr))
			return NULL;

		const struct rte_udp_hdr *udp =
			rte_pktmbuf_mtod_offset(m,
				const struct rte_udp_hdr *, offset);
		uint16_t dst_port = rte_be_to_cpu_16(udp->dst_port);

		if (dst_port != RTE_IPPORT_PTP_EVENT &&
		    dst_port != RTE_IPPORT_PTP_GENERAL)
			return NULL;

		offset += sizeof(struct rte_udp_hdr);
		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_ptp_hdr))
			return NULL;

		return rte_pktmbuf_mtod_offset(m,
			struct rte_ptp_hdr *, offset);
	}

	/* PTP over UDP/IPv6 */
	if (ether_type == RTE_ETHER_TYPE_IPV6) {
		const struct rte_ipv6_hdr *ip6h;

		if (rte_pktmbuf_data_len(m) <
		    offset + sizeof(struct rte_ipv6_hdr))
			return NULL;

		ip6h = rte_pktmbuf_mtod_offset(m,
			const struct rte_ipv6_hdr *, offset);
		if (ip6h->proto != IPPROTO_UDP)
			return NULL;

		offset += sizeof(struct rte_ipv6_hdr);

		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_udp_hdr))
			return NULL;

		const struct rte_udp_hdr *udp =
			rte_pktmbuf_mtod_offset(m,
				const struct rte_udp_hdr *, offset);
		uint16_t dst_port = rte_be_to_cpu_16(udp->dst_port);

		if (dst_port != RTE_IPPORT_PTP_EVENT &&
		    dst_port != RTE_IPPORT_PTP_GENERAL)
			return NULL;

		offset += sizeof(struct rte_udp_hdr);
		if (rte_pktmbuf_data_len(m) < offset + sizeof(struct rte_ptp_hdr))
			return NULL;

		return rte_pktmbuf_mtod_offset(m,
			struct rte_ptp_hdr *, offset);
	}

	return NULL;
}

/**
 * Classify a packet as PTP and return the message type.
 *
 * @param m
 *   Pointer to the mbuf to classify.
 * @return
 *   PTP message type (0x0-0xF) on success, PTP_MSGTYPE_INVALID (-1)
 *   if the packet is not PTP.
 */
static inline int
ptp_classify(const struct rte_mbuf *m)
{
	struct rte_ptp_hdr *hdr = ptp_hdr_find(m);

	if (hdr == NULL)
		return PTP_MSGTYPE_INVALID;

	return hdr->msg_type;
}

/** PTP message type name table. */
static const char * const ptp_msg_names[] = {
	[RTE_PTP_MSGTYPE_SYNC]           = "Sync",
	[RTE_PTP_MSGTYPE_DELAY_REQ]      = "Delay_Req",
	[RTE_PTP_MSGTYPE_PDELAY_REQ]     = "PDelay_Req",
	[RTE_PTP_MSGTYPE_PDELAY_RESP]    = "PDelay_Resp",
	[0x4]                            = "Reserved_4",
	[0x5]                            = "Reserved_5",
	[0x6]                            = "Reserved_6",
	[0x7]                            = "Reserved_7",
	[RTE_PTP_MSGTYPE_FU]             = "Follow_Up",
	[RTE_PTP_MSGTYPE_DELAY_RESP]     = "Delay_Resp",
	[RTE_PTP_MSGTYPE_PDELAY_RESP_FU] = "PDelay_Resp_Follow_Up",
	[RTE_PTP_MSGTYPE_ANNOUNCE]       = "Announce",
	[RTE_PTP_MSGTYPE_SIGNALING]      = "Signaling",
	[RTE_PTP_MSGTYPE_MANAGEMENT]     = "Management",
	[0xE]                            = "Reserved_E",
	[0xF]                            = "Reserved_F",
};

/**
 * Get a human-readable name for a PTP message type.
 *
 * @param msg_type
 *   PTP message type (0x0-0xF or PTP_MSGTYPE_INVALID).
 * @return
 *   Static string with the message type name.
 */
static inline const char *
ptp_msg_type_str(int msg_type)
{
	if (msg_type < 0 || msg_type > 0xF)
		return "Not_PTP";
	return ptp_msg_names[msg_type];
}

#endif /* _PTP_PARSE_H_ */
