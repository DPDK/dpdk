/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 NXP
 */

#ifndef _DPAAX_PTP_H_
#define _DPAAX_PTP_H_
#include <stdlib.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#define UDP_PTP_EVENT_DST_PORT 319
#define UDP_PTP_GENERAL_DST_PORT 320

struct __rte_packed_begin rte_dpaax_ptp_header {
	uint8_t tsmt;  /* transportSpecific | messageType */
	uint8_t ver;   /* reserved          | versionPTP  */
	rte_be16_t msg_len;
	uint8_t domain_number;
	uint8_t rsv;
	uint8_t flags[2];
	rte_be64_t correction;
	uint8_t unused[];
} __rte_packed_end;

static inline struct rte_dpaax_ptp_header *
dpaax_timesync_ptp_parse_header(struct rte_mbuf *buf,
	uint16_t *ts_offset, int *is_udp)
{
	struct rte_ether_hdr *eth = rte_pktmbuf_mtod(buf, void *);
	void *next_hdr;
	rte_be16_t ether_type;
	struct rte_vlan_hdr *vlan;
	struct rte_ipv4_hdr *ipv4;
	struct rte_ipv6_hdr *ipv6;
	struct rte_udp_hdr *udp;
	struct rte_dpaax_ptp_header *ptp = NULL;
	uint16_t offset = offsetof(struct rte_dpaax_ptp_header, correction);

	if (is_udp)
		*is_udp = false;

	offset += sizeof(struct rte_ether_hdr);
	if (eth->ether_type == htons(RTE_ETHER_TYPE_1588)) {
		ptp = (void *)(eth + 1);
		goto quit;
	}

	if (eth->ether_type == htons(RTE_ETHER_TYPE_VLAN)) {
		vlan = (void *)(eth + 1);
		ether_type = vlan->eth_proto;
		next_hdr = (void *)(vlan + 1);
		offset += sizeof(struct rte_vlan_hdr);
		if (ether_type == htons(RTE_ETHER_TYPE_1588)) {
			ptp = next_hdr;
			goto quit;
		}
	} else {
		ether_type = eth->ether_type;
		next_hdr = (void *)(eth + 1);
	}

	if (ether_type == htons(RTE_ETHER_TYPE_IPV4)) {
		ipv4 = next_hdr;
		offset += sizeof(struct rte_ipv4_hdr);
		if (ipv4->next_proto_id != IPPROTO_UDP)
			return NULL;
		udp = (void *)(ipv4 + 1);
		goto parse_udp;
	} else if (ether_type == htons(RTE_ETHER_TYPE_IPV6)) {
		ipv6 = next_hdr;
		offset += sizeof(struct rte_ipv6_hdr);
		if (ipv6->proto != IPPROTO_UDP)
			return NULL;
		udp = (void *)(ipv6 + 1);
		goto parse_udp;
	} else {
		return NULL;
	}
parse_udp:
	offset += sizeof(struct rte_udp_hdr);
	if (udp->dst_port != UDP_PTP_EVENT_DST_PORT &&
		udp->dst_port != UDP_PTP_GENERAL_DST_PORT)
		return NULL;
	ptp = (void *)(udp + 1);
	if (is_udp)
		*is_udp = true;
quit:
	if (ts_offset)
		*ts_offset = offset;

	return ptp;
}

#endif /* _DPAAX_PTP_H_ */
