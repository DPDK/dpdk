/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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


#ifndef _L3FWD_SSE_H_
#define _L3FWD_SSE_H_

#include "l3fwd.h"
#include "l3fwd_common.h"

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
 * Group consecutive packets with the same destination port in bursts of 4.
 * Suppose we have array of destionation ports:
 * dst_port[] = {a, b, c, d,, e, ... }
 * dp1 should contain: <a, b, c, d>, dp2: <b, c, d, e>.
 * We doing 4 comparisons at once and the result is 4 bit mask.
 * This mask is used as an index into prebuild array of pnum values.
 */
static inline uint16_t *
port_groupx4(uint16_t pn[FWDSTEP + 1], uint16_t *lp, __m128i dp1, __m128i dp2)
{
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
		pnum->u64 = gptbl[v].pnum;
		pnum->u16[FWDSTEP] = 1;
		lp = pnum->u16 + gptbl[v].idx;
	}

	return lp;
}

/**
 * Process one packet:
 * Update source and destination MAC addresses in the ethernet header.
 * Perform RFC1812 checks and updates for IPV4 packets.
 */
static inline void
process_packet(struct rte_mbuf *pkt, uint16_t *dst_port)
{
	struct ether_hdr *eth_hdr;
	__m128i te, ve;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	te = _mm_loadu_si128((__m128i *)eth_hdr);
	ve = val_eth[dst_port[0]];

	rfc1812_process((struct ipv4_hdr *)(eth_hdr + 1), dst_port,
			pkt->packet_type);

	te =  _mm_blend_epi16(te, ve, MASK_ETH);
	_mm_storeu_si128((__m128i *)eth_hdr, te);
}

/**
 * Send packets burst from pkts_burst to the ports in dst_port array
 */
static __rte_always_inline void
send_packets_multi(struct lcore_conf *qconf, struct rte_mbuf **pkts_burst,
		uint16_t dst_port[MAX_PKT_BURST], int nb_rx)
{
	int32_t k;
	int j = 0;
	uint16_t dlp;
	uint16_t *lp;
	uint16_t pnum[MAX_PKT_BURST + 1];

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
		process_packet(pkts_burst[j], dst_port + j);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
		/* fall-through */
	case 2:
		process_packet(pkts_burst[j], dst_port + j);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
		/* fall-through */
	case 1:
		process_packet(pkts_burst[j], dst_port + j);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
	}

	/*
	 * Send packets out, through destination port.
	 * Consecutive packets with the same destination port
	 * are already grouped together.
	 * If destination port for the packet equals BAD_PORT,
	 * then free the packet without sending it out.
	 */
	for (j = 0; j < nb_rx; j += k) {

		int32_t m;
		uint16_t pn;

		pn = dst_port[j];
		k = pnum[j];

		if (likely(pn != BAD_PORT))
			send_packetsx4(qconf, pn, pkts_burst + j, k);
		else
			for (m = j; m != j + k; m++)
				rte_pktmbuf_free(pkts_burst[m]);

	}
}

#endif /* _L3FWD_SSE_H_ */
