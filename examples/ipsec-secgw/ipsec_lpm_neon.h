/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef IPSEC_LPM_NEON_H
#define IPSEC_LPM_NEON_H

#include <arm_neon.h>
#include "ipsec_neon.h"

/*
 * Append ethernet header and read destination IPV4 addresses from 4 mbufs.
 */
static inline void
processx4_step1(struct rte_mbuf *pkt[FWDSTEP], int32x4_t *dip,
		uint64_t *inline_flag)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ether_hdr *eth_hdr;
	int32_t dst[FWDSTEP];
	int i;

	for (i = 0; i < FWDSTEP; i++) {
		eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(pkt[i],
							RTE_ETHER_HDR_LEN);
		pkt[i]->ol_flags |= RTE_MBUF_F_TX_IPV4;
		pkt[i]->l2_len = RTE_ETHER_HDR_LEN;

		ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);

		/* Fetch destination IPv4 address */
		dst[i] = ipv4_hdr->dst_addr;
		*inline_flag |= pkt[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD;
	}

	dip[0] = vld1q_s32(dst);
}

/*
 * Lookup into LPM for destination port.
 */
static inline void
processx4_step2(struct rt_ctx *rt_ctx, int32x4_t dip, uint64_t inline_flag,
		struct rte_mbuf *pkt[FWDSTEP], uint16_t dprt[FWDSTEP])
{
	uint32_t next_hop;
	rte_xmm_t dst;
	uint8_t i;

	dip = vreinterpretq_s32_u8(vrev32q_u8(vreinterpretq_u8_s32(dip)));

	/* If all 4 packets are non-inline */
	if (!inline_flag) {
		rte_lpm_lookupx4((struct rte_lpm *)rt_ctx, dip, dst.u32,
				 BAD_PORT);
		/* get rid of unused upper 16 bit for each dport. */
		vst1_s16((int16_t *)dprt, vqmovn_s32(dst.x));
		return;
	}

	/* Inline and non-inline packets */
	dst.x = dip;
	for (i = 0; i < FWDSTEP; i++) {
		if (pkt[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			next_hop = get_hop_for_offload_pkt(pkt[i], 0);
			dprt[i] = (uint16_t) (((next_hop &
						RTE_LPM_LOOKUP_SUCCESS) != 0)
						? next_hop : BAD_PORT);

		} else {
			dprt[i] = (uint16_t) ((rte_lpm_lookup(
						(struct rte_lpm *)rt_ctx,
						 dst.u32[i], &next_hop) == 0)
						? next_hop : BAD_PORT);
		}
	}
}

/*
 * Process single packets for destination port.
 */
static inline void
process_single_pkt(struct rt_ctx *rt_ctx, struct rte_mbuf *pkt,
		   uint16_t *dst_port)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	uint32_t next_hop;
	uint32_t dst_ip;

	eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(pkt,
							RTE_ETHER_HDR_LEN);
	pkt->ol_flags |= RTE_MBUF_F_TX_IPV4;
	pkt->l2_len = RTE_ETHER_HDR_LEN;

	if (pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
		next_hop = get_hop_for_offload_pkt(pkt, 0);
		*dst_port = (uint16_t) (((next_hop &
					  RTE_LPM_LOOKUP_SUCCESS) != 0)
					  ? next_hop : BAD_PORT);
	} else {
		ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
		dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
		*dst_port = (uint16_t) ((rte_lpm_lookup(
						(struct rte_lpm *)rt_ctx,
						dst_ip, &next_hop) == 0)
						? next_hop : BAD_PORT);
	}
}

/*
 * Buffer optimized handling of IPv6 packets.
 */
static inline void
route6_pkts_neon(struct rt_ctx *rt_ctx, struct rte_mbuf **pkts, int nb_rx)
{
	uint8_t dst_ip6[MAX_PKT_BURST][16];
	uint16_t dst_port[MAX_PKT_BURST];
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	int32_t hop[MAX_PKT_BURST];
	struct rte_mbuf *pkt;
	uint8_t lpm_pkts = 0;
	int32_t i;

	if (nb_rx == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_rx; i++) {
		pkt = pkts[i];
		eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(pkt,
							RTE_ETHER_HDR_LEN);
		pkt->l2_len = RTE_ETHER_HDR_LEN;
		pkt->ol_flags |= RTE_MBUF_F_TX_IPV6;

		if (!(pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) {
			/* Security offload not enabled. So an LPM lookup is
			 * required to get the hop
			 */
			ipv6_hdr = (struct rte_ipv6_hdr *)(eth_hdr + 1);
			memcpy(&dst_ip6[lpm_pkts][0],
					ipv6_hdr->dst_addr, 16);
			lpm_pkts++;
		}
	}

	rte_lpm6_lookup_bulk_func((struct rte_lpm6 *)rt_ctx, dst_ip6,
				  hop, lpm_pkts);

	lpm_pkts = 0;

	for (i = 0; i < nb_rx; i++) {
		pkt = pkts[i];
		if (pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			dst_port[i] = get_hop_for_offload_pkt(pkt, 1);
		} else {
			/* Need to use hop returned by lookup */
			dst_port[i] = (uint16_t)hop[lpm_pkts++];
		}
	}

	/* Send packets */
	send_multi_pkts(pkts, dst_port, nb_rx, 0, 0, false);
}

/*
 * Buffer optimized handling of IPv4 packets.
 */
static inline void
route4_pkts_neon(struct rt_ctx *rt_ctx, struct rte_mbuf **pkts, int nb_rx,
		 uint64_t tx_offloads, bool ip_cksum)
{
	const int32_t k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	const int32_t m = nb_rx % FWDSTEP;
	uint16_t dst_port[MAX_PKT_BURST];
	uint64_t inline_flag = 0;
	int32x4_t dip;
	int32_t i;

	if (nb_rx == 0)
		return;

	for (i = 0; i != k; i += FWDSTEP) {
		processx4_step1(&pkts[i], &dip, &inline_flag);
		processx4_step2(rt_ctx, dip, inline_flag, &pkts[i],
				&dst_port[i]);
	}

	/* Classify last up to 3 packets one by one */
	switch (m) {
	case 3:
		process_single_pkt(rt_ctx, pkts[i], &dst_port[i]);
		i++;
		/* fallthrough */
	case 2:
		process_single_pkt(rt_ctx, pkts[i], &dst_port[i]);
		i++;
		/* fallthrough */
	case 1:
		process_single_pkt(rt_ctx, pkts[i], &dst_port[i]);
	}

	send_multi_pkts(pkts, dst_port, nb_rx, tx_offloads, ip_cksum, true);
}

#endif /* IPSEC_LPM_NEON_H */
