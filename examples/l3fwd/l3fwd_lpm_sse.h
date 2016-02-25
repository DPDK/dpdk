/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef __L3FWD_LPM_SSE_H__
#define __L3FWD_LPM_SSE_H__

static inline __attribute__((always_inline)) void
send_packetsx4(struct lcore_conf *qconf, uint8_t port,
	struct rte_mbuf *m[], uint32_t num)
{
	uint32_t len, j, n;

	len = qconf->tx_mbufs[port].len;

	/*
	 * If TX buffer for that queue is empty, and we have enough packets,
	 * then send them straightway.
	 */
	if (num >= MAX_TX_BURST && len == 0) {
		n = rte_eth_tx_burst(port, qconf->tx_queue_id[port], m, num);
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
	case 3:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
	case 2:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
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
		j = 0;
		switch (len % FWDSTEP) {
		while (j < len) {
		case 0:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		case 3:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		case 2:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		case 1:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		}
		}
	}

	qconf->tx_mbufs[port].len = len;
}

#ifdef DO_RFC_1812_CHECKS

#define	IPV4_MIN_VER_IHL	0x45
#define	IPV4_MAX_VER_IHL	0x4f
#define	IPV4_MAX_VER_IHL_DIFF	(IPV4_MAX_VER_IHL - IPV4_MIN_VER_IHL)

/* Minimum value of IPV4 total length (20B) in network byte order. */
#define	IPV4_MIN_LEN_BE	(sizeof(struct ipv4_hdr) << 8)

/*
 * From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2:
 * - The IP version number must be 4.
 * - The IP header length field must be large enough to hold the
 *    minimum length legal IP datagram (20 bytes = 5 words).
 * - The IP total length field must be large enough to hold the IP
 *   datagram header, whose length is specified in the IP header length
 *   field.
 * If we encounter invalid IPV4 packet, then set destination port for it
 * to BAD_PORT value.
 */
static inline __attribute__((always_inline)) void
rfc1812_process(struct ipv4_hdr *ipv4_hdr, uint16_t *dp, uint32_t ptype)
{
	uint8_t ihl;

	if (RTE_ETH_IS_IPV4_HDR(ptype)) {
		ihl = ipv4_hdr->version_ihl - IPV4_MIN_VER_IHL;

		ipv4_hdr->time_to_live--;
		ipv4_hdr->hdr_checksum++;

		if (ihl > IPV4_MAX_VER_IHL_DIFF ||
				((uint8_t)ipv4_hdr->total_length == 0 &&
				ipv4_hdr->total_length < IPV4_MIN_LEN_BE)) {
			dp[0] = BAD_PORT;
		}
	}
}

#else
#define	rfc1812_process(mb, dp)	do { } while (0)
#endif /* DO_RFC_1812_CHECKS */

static inline __attribute__((always_inline)) uint16_t
get_dst_port(const struct lcore_conf *qconf, struct rte_mbuf *pkt,
	uint32_t dst_ipv4, uint8_t portid)
{
	uint8_t next_hop;
	struct ipv6_hdr *ipv6_hdr;
	struct ether_hdr *eth_hdr;

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		if (rte_lpm_lookup(qconf->ipv4_lookup_struct, dst_ipv4,
				&next_hop) != 0)
			next_hop = portid;
	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
		eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
		ipv6_hdr = (struct ipv6_hdr *)(eth_hdr + 1);
		if (rte_lpm6_lookup(qconf->ipv6_lookup_struct,
				ipv6_hdr->dst_addr, &next_hop) != 0)
			next_hop = portid;
	} else {
		next_hop = portid;
	}

	return next_hop;
}

static inline void
process_packet(struct lcore_conf *qconf, struct rte_mbuf *pkt,
	uint16_t *dst_port, uint8_t portid)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	uint32_t dst_ipv4;
	uint16_t dp;
	__m128i te, ve;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);

	dst_ipv4 = ipv4_hdr->dst_addr;
	dst_ipv4 = rte_be_to_cpu_32(dst_ipv4);
	dp = get_dst_port(qconf, pkt, dst_ipv4, portid);

	te = _mm_loadu_si128((__m128i *)eth_hdr);
	ve = val_eth[dp];

	dst_port[0] = dp;
	rfc1812_process(ipv4_hdr, dst_port, pkt->packet_type);

	te =  _mm_blend_epi16(te, ve, MASK_ETH);
	_mm_storeu_si128((__m128i *)eth_hdr, te);
}

/*
 * Read packet_type and destination IPV4 addresses from 4 mbufs.
 */
static inline void
processx4_step1(struct rte_mbuf *pkt[FWDSTEP],
		__m128i *dip,
		uint32_t *ipv4_flag)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ether_hdr *eth_hdr;
	uint32_t x0, x1, x2, x3;

	eth_hdr = rte_pktmbuf_mtod(pkt[0], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x0 = ipv4_hdr->dst_addr;
	ipv4_flag[0] = pkt[0]->packet_type & RTE_PTYPE_L3_IPV4;

	eth_hdr = rte_pktmbuf_mtod(pkt[1], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x1 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[1]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[2], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x2 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[2]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[3], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x3 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[3]->packet_type;

	dip[0] = _mm_set_epi32(x3, x2, x1, x0);
}

/*
 * Lookup into LPM for destination port.
 * If lookup fails, use incoming port (portid) as destination port.
 */
static inline void
processx4_step2(const struct lcore_conf *qconf,
		__m128i dip,
		uint32_t ipv4_flag,
		uint8_t portid,
		struct rte_mbuf *pkt[FWDSTEP],
		uint16_t dprt[FWDSTEP])
{
	rte_xmm_t dst;
	const  __m128i bswap_mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11,
						4, 5, 6, 7, 0, 1, 2, 3);

	/* Byte swap 4 IPV4 addresses. */
	dip = _mm_shuffle_epi8(dip, bswap_mask);

	/* if all 4 packets are IPV4. */
	if (likely(ipv4_flag)) {
		rte_lpm_lookupx4(qconf->ipv4_lookup_struct, dip, dprt, portid);
	} else {
		dst.x = dip;
		dprt[0] = get_dst_port(qconf, pkt[0], dst.u32[0], portid);
		dprt[1] = get_dst_port(qconf, pkt[1], dst.u32[1], portid);
		dprt[2] = get_dst_port(qconf, pkt[2], dst.u32[2], portid);
		dprt[3] = get_dst_port(qconf, pkt[3], dst.u32[3], portid);
	}
}

/*
 * Update source and destination MAC addresses in the ethernet header.
 * Perform RFC1812 checks and updates for IPV4 packets.
 */
static inline void
processx4_step3(struct rte_mbuf *pkt[FWDSTEP], uint16_t dst_port[FWDSTEP])
{
	__m128i te[FWDSTEP];
	__m128i ve[FWDSTEP];
	__m128i *p[FWDSTEP];

	p[0] = rte_pktmbuf_mtod(pkt[0], __m128i *);
	p[1] = rte_pktmbuf_mtod(pkt[1], __m128i *);
	p[2] = rte_pktmbuf_mtod(pkt[2], __m128i *);
	p[3] = rte_pktmbuf_mtod(pkt[3], __m128i *);

	ve[0] = val_eth[dst_port[0]];
	te[0] = _mm_loadu_si128(p[0]);

	ve[1] = val_eth[dst_port[1]];
	te[1] = _mm_loadu_si128(p[1]);

	ve[2] = val_eth[dst_port[2]];
	te[2] = _mm_loadu_si128(p[2]);

	ve[3] = val_eth[dst_port[3]];
	te[3] = _mm_loadu_si128(p[3]);

	/* Update first 12 bytes, keep rest bytes intact. */
	te[0] =  _mm_blend_epi16(te[0], ve[0], MASK_ETH);
	te[1] =  _mm_blend_epi16(te[1], ve[1], MASK_ETH);
	te[2] =  _mm_blend_epi16(te[2], ve[2], MASK_ETH);
	te[3] =  _mm_blend_epi16(te[3], ve[3], MASK_ETH);

	_mm_storeu_si128(p[0], te[0]);
	_mm_storeu_si128(p[1], te[1]);
	_mm_storeu_si128(p[2], te[2]);
	_mm_storeu_si128(p[3], te[3]);

	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[0] + 1),
		&dst_port[0], pkt[0]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[1] + 1),
		&dst_port[1], pkt[1]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[2] + 1),
		&dst_port[2], pkt[2]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[3] + 1),
		&dst_port[3], pkt[3]->packet_type);
}

/*
 * We group consecutive packets with the same destionation port into one burst.
 * To avoid extra latency this is done together with some other packet
 * processing, but after we made a final decision about packet's destination.
 * To do this we maintain:
 * pnum - array of number of consecutive packets with the same dest port for
 * each packet in the input burst.
 * lp - pointer to the last updated element in the pnum.
 * dlp - dest port value lp corresponds to.
 */

#define	GRPSZ	(1 << FWDSTEP)
#define	GRPMSK	(GRPSZ - 1)

#define GROUP_PORT_STEP(dlp, dcp, lp, pn, idx)	do { \
	if (likely((dlp) == (dcp)[(idx)])) {         \
		(lp)[0]++;                           \
	} else {                                     \
		(dlp) = (dcp)[idx];                  \
		(lp) = (pn) + (idx);                 \
		(lp)[0] = 1;                         \
	}                                            \
} while (0)

/*
 * Group consecutive packets with the same destination port in bursts of 4.
 * Suppose we have array of destionation ports:
 * dst_port[] = {a, b, c, d,, e, ... }
 * dp1 should contain: <a, b, c, d>, dp2: <b, c, d, e>.
 * We doing 4 comparisions at once and the result is 4 bit mask.
 * This mask is used as an index into prebuild array of pnum values.
 */
static inline uint16_t *
port_groupx4(uint16_t pn[FWDSTEP + 1], uint16_t *lp, __m128i dp1, __m128i dp2)
{
	static const struct {
		uint64_t pnum; /* prebuild 4 values for pnum[]. */
		int32_t  idx;  /* index for new last updated elemnet. */
		uint16_t lpv;  /* add value to the last updated element. */
	} gptbl[GRPSZ] = {
	{
		/* 0: a != b, b != c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100010001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 1: a == b, b != c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100010002),
		.idx = 4,
		.lpv = 1,
	},
	{
		/* 2: a != b, b == c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100020001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 3: a == b, b == c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100020003),
		.idx = 4,
		.lpv = 2,
	},
	{
		/* 4: a != b, b != c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200010001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 5: a == b, b != c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200010002),
		.idx = 4,
		.lpv = 1,
	},
	{
		/* 6: a != b, b == c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200030001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 7: a == b, b == c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200030004),
		.idx = 4,
		.lpv = 3,
	},
	{
		/* 8: a != b, b != c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100010001),
		.idx = 3,
		.lpv = 0,
	},
	{
		/* 9: a == b, b != c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100010002),
		.idx = 3,
		.lpv = 1,
	},
	{
		/* 0xa: a != b, b == c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100020001),
		.idx = 3,
		.lpv = 0,
	},
	{
		/* 0xb: a == b, b == c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100020003),
		.idx = 3,
		.lpv = 2,
	},
	{
		/* 0xc: a != b, b != c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300010001),
		.idx = 2,
		.lpv = 0,
	},
	{
		/* 0xd: a == b, b != c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300010002),
		.idx = 2,
		.lpv = 1,
	},
	{
		/* 0xe: a != b, b == c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300040001),
		.idx = 1,
		.lpv = 0,
	},
	{
		/* 0xf: a == b, b == c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300040005),
		.idx = 0,
		.lpv = 4,
	},
	};

	union {
		uint16_t u16[FWDSTEP + 1];
		uint64_t u64;
	} *pnum = (void *)pn;

	int32_t v;

	dp1 = _mm_cmpeq_epi16(dp1, dp2);
	dp1 = _mm_unpacklo_epi16(dp1, dp1);
	v = _mm_movemask_ps((__m128)dp1);

	/* update last port counter. */
	lp[0] += gptbl[v].lpv;

	/* if dest port value has changed. */
	if (v != GRPMSK) {
		lp = pnum->u16 + gptbl[v].idx;
		lp[0] = 1;
		pnum->u64 = gptbl[v].pnum;
	}

	return lp;
}

/*
 * Buffer optimized handling of packets, invoked
 * from main_loop.
 */
static inline void
l3fwd_lpm_send_packets(int nb_rx, struct rte_mbuf **pkts_burst,
			uint8_t portid, struct lcore_conf *qconf)
{
	int32_t j, k;
	uint16_t dlp;
	uint16_t *lp;
	uint16_t dst_port[MAX_PKT_BURST];
	__m128i dip[MAX_PKT_BURST / FWDSTEP];
	uint32_t ipv4_flag[MAX_PKT_BURST / FWDSTEP];
	uint16_t pnum[MAX_PKT_BURST + 1];

	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	for (j = 0; j != k; j += FWDSTEP) {
		processx4_step1(&pkts_burst[j],
			&dip[j / FWDSTEP],
			&ipv4_flag[j / FWDSTEP]);
	}

	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	for (j = 0; j != k; j += FWDSTEP) {
		processx4_step2(qconf, dip[j / FWDSTEP],
			ipv4_flag[j / FWDSTEP], portid,
			&pkts_burst[j], &dst_port[j]);
	}

	/*
	 * Finish packet processing and group consecutive
	 * packets with the same destination port.
	 */
	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	if (k != 0) {
		__m128i dp1, dp2;

		lp = pnum;
		lp[0] = 1;

		processx4_step3(pkts_burst, dst_port);

		/* dp1: <d[0], d[1], d[2], d[3], ... > */
		dp1 = _mm_loadu_si128((__m128i *)dst_port);

		for (j = FWDSTEP; j != k; j += FWDSTEP) {
			processx4_step3(&pkts_burst[j], &dst_port[j]);

			/*
			 * dp2:
			 * <d[j-3], d[j-2], d[j-1], d[j], ... >
			 */
			dp2 = _mm_loadu_si128((__m128i *)
					&dst_port[j - FWDSTEP + 1]);
			lp  = port_groupx4(&pnum[j - FWDSTEP], lp, dp1, dp2);

			/*
			 * dp1:
			 * <d[j], d[j+1], d[j+2], d[j+3], ... >
			 */
			dp1 = _mm_srli_si128(dp2, (FWDSTEP - 1) *
						sizeof(dst_port[0]));
		}

		/*
		 * dp2: <d[j-3], d[j-2], d[j-1], d[j-1], ... >
		 */
		dp2 = _mm_shufflelo_epi16(dp1, 0xf9);
		lp  = port_groupx4(&pnum[j - FWDSTEP], lp, dp1, dp2);

		/*
		 * remove values added by the last repeated
		 * dst port.
		 */
		lp[0]--;
		dlp = dst_port[j - 1];
	} else {
		/* set dlp and lp to the never used values. */
		dlp = BAD_PORT - 1;
		lp = pnum + MAX_PKT_BURST;
	}

	/* Process up to last 3 packets one by one. */
	switch (nb_rx % FWDSTEP) {
	case 3:
		process_packet(qconf, pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
	case 2:
		process_packet(qconf, pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
	case 1:
		process_packet(qconf, pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
	}

	/*
	 * Send packets out, through destination port.
	 * Consecuteve pacekts with the same destination port
	 * are already grouped together.
	 * If destination port for the packet equals BAD_PORT,
	 * then free the packet without sending it out.
	 */
	for (j = 0; j < nb_rx; j += k) {

		int32_t m;
		uint16_t pn;

		pn = dst_port[j];
		k = pnum[j];

		if (likely(pn != BAD_PORT)) {
			send_packetsx4(qconf, pn, pkts_burst + j, k);
		} else {
			for (m = j; m != j + k; m++)
				rte_pktmbuf_free(pkts_burst[m]);
		}
	}
}

#endif /* __L3FWD_LPM_SSE_H__ */
