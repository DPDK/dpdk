/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef IPSEC_NEON_H
#define IPSEC_NEON_H

#include "ipsec.h"
#include "neon/port_group.h"

#define MAX_TX_BURST	(MAX_PKT_BURST / 2)

extern xmm_t val_eth[RTE_MAX_ETHPORTS];

/*
 * Update source and destination MAC addresses in the ethernet header.
 */
static inline void
processx4_step3(struct rte_mbuf *pkts[FWDSTEP], uint16_t dst_port[FWDSTEP],
		uint64_t tx_offloads, bool ip_cksum, bool is_ipv4, uint8_t *l_pkt)
{
	uint32x4_t te[FWDSTEP];
	uint32x4_t ve[FWDSTEP];
	uint32_t *p[FWDSTEP];
	struct rte_mbuf *pkt;
	uint32_t val;
	uint8_t i;

	for (i = 0; i < FWDSTEP; i++) {
		pkt = pkts[i];

		/* Check if it is a large packet */
		if (pkt->pkt_len - RTE_ETHER_HDR_LEN > mtu_size)
			*l_pkt |= 1;

		p[i] = rte_pktmbuf_mtod(pkt, uint32_t *);
		ve[i] = vreinterpretq_u32_s32(val_eth[dst_port[i]]);
		te[i] = vld1q_u32(p[i]);

		/* Update last 4 bytes */
		val = vgetq_lane_u32(te[i], 3);
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		val &= 0xFFFFUL << 16;
		val |= rte_cpu_to_be_16(is_ipv4 ? RTE_ETHER_TYPE_IPV4 : RTE_ETHER_TYPE_IPV6);
#else
		val &= 0xFFFFUL;
		val |= rte_cpu_to_be_16(is_ipv4 ? RTE_ETHER_TYPE_IPV4 : RTE_ETHER_TYPE_IPV6) << 16;
#endif
		ve[i] = vsetq_lane_u32(val, ve[i], 3);
		vst1q_u32(p[i], ve[i]);

		if (ip_cksum) {
			struct rte_ipv4_hdr *ip;

			pkt->ol_flags |= tx_offloads;

			ip = (struct rte_ipv4_hdr *)
				(((uintptr_t)p[i]) + RTE_ETHER_HDR_LEN);
			ip->hdr_checksum = 0;

			/* calculate IPv4 cksum in SW */
			if ((pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) == 0)
				ip->hdr_checksum = rte_ipv4_cksum(ip);
		}

	}
}

/**
 * Process single packet:
 * Update source and destination MAC addresses in the ethernet header.
 */
static inline void
process_packet(struct rte_mbuf *pkt, uint16_t *dst_port, uint64_t tx_offloads,
	       bool ip_cksum, bool is_ipv4, uint8_t *l_pkt)
{
	struct rte_ether_hdr *eth_hdr;
	uint32x4_t te, ve;
	uint32_t val;

	/* Check if it is a large packet */
	if (pkt->pkt_len - RTE_ETHER_HDR_LEN > mtu_size)
		*l_pkt |= 1;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

	te = vld1q_u32((uint32_t *)eth_hdr);
	ve = vreinterpretq_u32_s32(val_eth[dst_port[0]]);

	val = vgetq_lane_u32(te, 3);
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	val &= 0xFFFFUL << 16;
	val |= rte_cpu_to_be_16(is_ipv4 ? RTE_ETHER_TYPE_IPV4 : RTE_ETHER_TYPE_IPV6);
#else
	val &= 0xFFFFUL;
	val |= rte_cpu_to_be_16(is_ipv4 ? RTE_ETHER_TYPE_IPV4 : RTE_ETHER_TYPE_IPV6) << 16;
#endif
	ve = vsetq_lane_u32(val, ve, 3);
	vst1q_u32((uint32_t *)eth_hdr, ve);

	if (ip_cksum) {
		struct rte_ipv4_hdr *ip;

		pkt->ol_flags |= tx_offloads;

		ip = (struct rte_ipv4_hdr *)(eth_hdr + 1);
		ip->hdr_checksum = 0;

		/* calculate IPv4 cksum in SW */
		if ((pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) == 0)
			ip->hdr_checksum = rte_ipv4_cksum(ip);
	}
}

static inline void
send_packets(struct rte_mbuf *m[], uint16_t port, uint32_t num, bool is_ipv4)
{
	uint8_t proto;
	uint32_t i;

	proto = is_ipv4 ? IPPROTO_IP : IPPROTO_IPV6;
	for (i = 0; i < num; i++)
		send_single_packet(m[i], port, proto);
}

static inline void
send_packetsx4(struct rte_mbuf *m[], uint16_t port, uint32_t num)
{
	unsigned int lcoreid = rte_lcore_id();
	struct lcore_conf *qconf;
	uint32_t len, j, n;

	qconf = &lcore_conf[lcoreid];

	len = qconf->tx_mbufs[port].len;

	/*
	 * If TX buffer for that queue is empty, and we have enough packets,
	 * then send them straightway.
	 */
	if (num >= MAX_TX_BURST && len == 0) {
		n = rte_eth_tx_burst(port, qconf->tx_queue_id[port], m, num);
		core_stats_update_tx(n);
		if (unlikely(n < num)) {
			do {
				rte_pktmbuf_free(m[n]);
			} while (++n < num);
		}
		return;
	}

	/*
	 * Put packets into TX buffer for that queue.
	 */

	n = len + num;
	n = (n > MAX_PKT_BURST) ? MAX_PKT_BURST - len : num;

	j = 0;
	switch (n % FWDSTEP) {
	while (j < n) {
		case 0:
			qconf->tx_mbufs[port].m_table[len + j] = m[j];
			j++;
			/* fallthrough */
		case 3:
			qconf->tx_mbufs[port].m_table[len + j] = m[j];
			j++;
			/* fallthrough */
		case 2:
			qconf->tx_mbufs[port].m_table[len + j] = m[j];
			j++;
			/* fallthrough */
		case 1:
			qconf->tx_mbufs[port].m_table[len + j] = m[j];
			j++;
		}
	}

	len += n;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {

		send_burst(qconf, MAX_PKT_BURST, port);

		/* copy rest of the packets into the TX buffer. */
		len = num - n;
		if (len == 0)
			goto exit;

		j = 0;
		switch (len % FWDSTEP) {
		while (j < len) {
			case 0:
				qconf->tx_mbufs[port].m_table[j] = m[n + j];
				j++;
				/* fallthrough */
			case 3:
				qconf->tx_mbufs[port].m_table[j] = m[n + j];
				j++;
				/* fallthrough */
			case 2:
				qconf->tx_mbufs[port].m_table[j] = m[n + j];
				j++;
				/* fallthrough */
			case 1:
				qconf->tx_mbufs[port].m_table[j] = m[n + j];
				j++;
		}
		}
	}

exit:
	qconf->tx_mbufs[port].len = len;
}

/**
 * Send packets burst to the ports in dst_port array
 */
static __rte_always_inline void
send_multi_pkts(struct rte_mbuf **pkts, uint16_t dst_port[MAX_PKT_BURST],
		int nb_rx, uint64_t tx_offloads, bool ip_cksum, bool is_ipv4)
{
	unsigned int lcoreid = rte_lcore_id();
	uint16_t pnum[MAX_PKT_BURST + 1];
	uint8_t l_pkt = 0;
	uint16_t dlp, *lp;
	int i = 0, k;

	/*
	 * Finish packet processing and group consecutive
	 * packets with the same destination port.
	 */
	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);

	if (k != 0) {
		uint16x8_t dp1, dp2;

		lp = pnum;
		lp[0] = 1;

		processx4_step3(pkts, dst_port, tx_offloads, ip_cksum, is_ipv4, &l_pkt);

		/* dp1: <d[0], d[1], d[2], d[3], ... > */
		dp1 = vld1q_u16(dst_port);

		for (i = FWDSTEP; i != k; i += FWDSTEP) {
			processx4_step3(&pkts[i], &dst_port[i], tx_offloads, ip_cksum, is_ipv4,
					&l_pkt);

			/*
			 * dp2:
			 * <d[j-3], d[j-2], d[j-1], d[j], ... >
			 */
			dp2 = vld1q_u16(&dst_port[i - FWDSTEP + 1]);
			lp  = port_groupx4(&pnum[i - FWDSTEP], lp, dp1, dp2);

			/*
			 * dp1:
			 * <d[j], d[j+1], d[j+2], d[j+3], ... >
			 */
			dp1 = vextq_u16(dp2, dp1, FWDSTEP - 1);
		}

		/*
		 * dp2: <d[j-3], d[j-2], d[j-1], d[j-1], ... >
		 */
		dp2 = vextq_u16(dp1, dp1, 1);
		dp2 = vsetq_lane_u16(vgetq_lane_u16(dp2, 2), dp2, 3);
		lp  = port_groupx4(&pnum[i - FWDSTEP], lp, dp1, dp2);

		/*
		 * remove values added by the last repeated
		 * dst port.
		 */
		lp[0]--;
		dlp = dst_port[i - 1];
	} else {
		/* set dlp and lp to the never used values. */
		dlp = BAD_PORT - 1;
		lp = pnum + MAX_PKT_BURST;
	}

	/* Process up to last 3 packets one by one. */
	switch (nb_rx % FWDSTEP) {
	case 3:
		process_packet(pkts[i], dst_port + i, tx_offloads, ip_cksum, is_ipv4, &l_pkt);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, i);
		i++;
		/* fallthrough */
	case 2:
		process_packet(pkts[i], dst_port + i, tx_offloads, ip_cksum, is_ipv4, &l_pkt);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, i);
		i++;
		/* fallthrough */
	case 1:
		process_packet(pkts[i], dst_port + i, tx_offloads, ip_cksum, is_ipv4, &l_pkt);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, i);
	}

	/*
	 * Send packets out, through destination port.
	 * Consecutive packets with the same destination port
	 * are already grouped together.
	 * If destination port for the packet equals BAD_PORT,
	 * then free the packet without sending it out.
	 */
	for (i = 0; i < nb_rx; i += k) {

		uint16_t pn;

		pn = dst_port[i];
		k = pnum[i];

		if (likely(pn != BAD_PORT)) {
			if (l_pkt)
				/* Large packet is present, need to send
				 * individual packets with fragment
				 */
				send_packets(pkts + i, pn, k, is_ipv4);
			else
				send_packetsx4(pkts + i, pn, k);

		} else {
			free_pkts(&pkts[i], k);
			if (is_ipv4)
				core_statistics[lcoreid].lpm4.miss++;
			else
				core_statistics[lcoreid].lpm6.miss++;
		}
	}
}

#endif /* IPSEC_NEON_H */
