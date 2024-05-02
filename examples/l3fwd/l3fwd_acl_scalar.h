/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef L3FWD_ACL_SCALAR_H
#define L3FWD_ACL_SCALAR_H

#include "l3fwd.h"
#if defined RTE_ARCH_X86
#include "l3fwd_sse.h"
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#elif defined RTE_ARCH_PPC_64
#include "l3fwd_altivec.h"
#else
#include "l3fwd_common.h"
#endif
/*
 * If the machine has SSE, NEON or PPC 64 then multiple packets
 * can be sent at once if not only single packets will be sent.
 */
#if defined RTE_ARCH_X86 || defined __ARM_NEON || defined RTE_ARCH_PPC_64
#define ACL_SEND_MULTI
#endif

#define TYPE_NONE	0
#define TYPE_IPV4	1
#define TYPE_IPV6	2

struct acl_search_t {

	uint32_t num_ipv4;
	uint32_t num_ipv6;

	uint8_t types[MAX_PKT_BURST];

	const uint8_t *data_ipv4[MAX_PKT_BURST];
	uint32_t res_ipv4[MAX_PKT_BURST];

	const uint8_t *data_ipv6[MAX_PKT_BURST];
	uint32_t res_ipv6[MAX_PKT_BURST];
};

static inline void
l3fwd_acl_prepare_one_packet(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int index)
{
	struct rte_mbuf *pkt = pkts_in[index];

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv4[acl->num_ipv4++] = MBUF_IPV4_2PROTO(pkt);
		acl->types[index] = TYPE_IPV4;

	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv6[acl->num_ipv6++] = MBUF_IPV6_2PROTO(pkt);
		acl->types[index] = TYPE_IPV6;
	} else {
		/* Unknown type, will drop the packet */
		acl->types[index] = TYPE_NONE;
	}
}

static inline void
l3fwd_acl_prepare_acl_parameter(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int nb_rx)
{
	int i;

	acl->num_ipv4 = 0;
	acl->num_ipv6 = 0;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				pkts_in[i], void *));
	}

	for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_in[
				i + PREFETCH_OFFSET], void *));
		l3fwd_acl_prepare_one_packet(pkts_in, acl, i);
	}

	/* Process left packets */
	for (; i < nb_rx; i++)
		l3fwd_acl_prepare_one_packet(pkts_in, acl, i);
}

static inline void
send_packets_single(struct lcore_conf *qconf, struct rte_mbuf *pkts[], uint16_t hops[],
	uint32_t nb_tx)
{
	uint32_t j;
	struct rte_ether_hdr *eth_hdr;

	for (j = 0; j < nb_tx; j++) {
		/* Run rfc1812 if packet is ipv4 and checks enabled. */
		rfc1812_process((struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(
						pkts[j], struct rte_ether_hdr *) + 1),
						&hops[j], pkts[j]->packet_type);

		/* Set MAC addresses. */
		eth_hdr = rte_pktmbuf_mtod(pkts[j], struct rte_ether_hdr *);
		if (hops[j] != BAD_PORT) {
			*(uint64_t *)&eth_hdr->dst_addr = dest_eth_addr[hops[j]];
			rte_ether_addr_copy(&ports_eth_addr[hops[j]],
							&eth_hdr->src_addr);
			send_single_packet(qconf, pkts[j], hops[j]);
		} else
			rte_pktmbuf_free(pkts[j]);
	}
}

#endif /* L3FWD_ACL_SCALAR_H */
