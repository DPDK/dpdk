/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef L3FWD_ACL_SCALAR_H
#define L3FWD_ACL_SCALAR_H

#include "l3fwd.h"
#include "l3fwd_common.h"

static inline void
l3fwd_acl_prepare_one_packet(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int index)
{
	struct rte_mbuf *pkt = pkts_in[index];

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv4[acl->num_ipv4] = MBUF_IPV4_2PROTO(pkt);
		acl->m_ipv4[(acl->num_ipv4)++] = pkt;

	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv6[acl->num_ipv6] = MBUF_IPV6_2PROTO(pkt);
		acl->m_ipv6[(acl->num_ipv6)++] = pkt;
	} else {
		/* Unknown type, drop the packet */
		rte_pktmbuf_free(pkt);
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

static inline void
l3fwd_acl_send_packets(struct lcore_conf *qconf, struct rte_mbuf *pkts[], uint32_t res[],
	uint32_t nb_tx)
{
	uint32_t i;
	uint16_t dst_port[nb_tx];

	for (i = 0; i != nb_tx; i++) {
		if (likely((res[i] & ACL_DENY_SIGNATURE) == 0 && res[i] != 0)) {
			dst_port[i] = res[i] - FWD_PORT_SHIFT;
		} else {
			dst_port[i] = BAD_PORT;
#ifdef L3FWDACL_DEBUG
			if ((res & ACL_DENY_SIGNATURE) != 0) {
				if (RTE_ETH_IS_IPV4_HDR(pkts[i]->packet_type))
					dump_acl4_rule(pkts[i], res[i]);
				else if (RTE_ETH_IS_IPV6_HDR(pkt[i]->packet_type))
					dump_acl6_rule(pkt[i], res[i]);
			}
#endif
		}
	}

	send_packets_single(qconf, pkts, dst_port, nb_tx);
}

#endif /* L3FWD_ACL_SCALAR_H */
