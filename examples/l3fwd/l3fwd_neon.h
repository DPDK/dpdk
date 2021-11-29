/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */

#ifndef _L3FWD_NEON_H_
#define _L3FWD_NEON_H_

#include "l3fwd.h"
#include "l3fwd_common.h"

/*
 * Update source and destination MAC addresses in the ethernet header.
 * Perform RFC1812 checks and updates for IPV4 packets.
 */
static inline void
processx4_step3(struct rte_mbuf *pkt[FWDSTEP], uint16_t dst_port[FWDSTEP])
{
	uint32x4_t te[FWDSTEP];
	uint32x4_t ve[FWDSTEP];
	uint32_t *p[FWDSTEP];

	p[0] = rte_pktmbuf_mtod(pkt[0], uint32_t *);
	p[1] = rte_pktmbuf_mtod(pkt[1], uint32_t *);
	p[2] = rte_pktmbuf_mtod(pkt[2], uint32_t *);
	p[3] = rte_pktmbuf_mtod(pkt[3], uint32_t *);

	ve[0] = vreinterpretq_u32_s32(val_eth[dst_port[0]]);
	te[0] = vld1q_u32(p[0]);

	ve[1] = vreinterpretq_u32_s32(val_eth[dst_port[1]]);
	te[1] = vld1q_u32(p[1]);

	ve[2] = vreinterpretq_u32_s32(val_eth[dst_port[2]]);
	te[2] = vld1q_u32(p[2]);

	ve[3] = vreinterpretq_u32_s32(val_eth[dst_port[3]]);
	te[3] = vld1q_u32(p[3]);

	/* Update last 4 bytes */
	ve[0] = vsetq_lane_u32(vgetq_lane_u32(te[0], 3), ve[0], 3);
	ve[1] = vsetq_lane_u32(vgetq_lane_u32(te[1], 3), ve[1], 3);
	ve[2] = vsetq_lane_u32(vgetq_lane_u32(te[2], 3), ve[2], 3);
	ve[3] = vsetq_lane_u32(vgetq_lane_u32(te[3], 3), ve[3], 3);

	vst1q_u32(p[0], ve[0]);
	vst1q_u32(p[1], ve[1]);
	vst1q_u32(p[2], ve[2]);
	vst1q_u32(p[3], ve[3]);

	rfc1812_process((struct rte_ipv4_hdr *)
			((struct rte_ether_hdr *)p[0] + 1),
			&dst_port[0], pkt[0]->packet_type);
	rfc1812_process((struct rte_ipv4_hdr *)
			((struct rte_ether_hdr *)p[1] + 1),
			&dst_port[1], pkt[1]->packet_type);
	rfc1812_process((struct rte_ipv4_hdr *)
			((struct rte_ether_hdr *)p[2] + 1),
			&dst_port[2], pkt[2]->packet_type);
	rfc1812_process((struct rte_ipv4_hdr *)
			((struct rte_ether_hdr *)p[3] + 1),
			&dst_port[3], pkt[3]->packet_type);
}

/*
 * Group consecutive packets with the same destination port in bursts of 4.
 * Suppose we have array of destination ports:
 * dst_port[] = {a, b, c, d,, e, ... }
 * dp1 should contain: <a, b, c, d>, dp2: <b, c, d, e>.
 * We doing 4 comparisons at once and the result is 4 bit mask.
 * This mask is used as an index into prebuild array of pnum values.
 */
static inline uint16_t *
port_groupx4(uint16_t pn[FWDSTEP + 1], uint16_t *lp, uint16x8_t dp1,
	     uint16x8_t dp2)
{
	union {
		uint16_t u16[FWDSTEP + 1];
		uint64_t u64;
	} *pnum = (void *)pn;

	int32_t v;
	uint16x8_t mask = {1, 2, 4, 8, 0, 0, 0, 0};

	dp1 = vceqq_u16(dp1, dp2);
	dp1 = vandq_u16(dp1, mask);
	v = vaddvq_u16(dp1);

	/* update last port counter. */
	lp[0] += gptbl[v].lpv;
	rte_compiler_barrier();

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
	struct rte_ether_hdr *eth_hdr;
	uint32x4_t te, ve;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

	te = vld1q_u32((uint32_t *)eth_hdr);
	ve = vreinterpretq_u32_s32(val_eth[dst_port[0]]);


	rfc1812_process((struct rte_ipv4_hdr *)(eth_hdr + 1), dst_port,
			pkt->packet_type);

	ve = vcopyq_laneq_u32(ve, 3, te, 3);
	vst1q_u32((uint32_t *)eth_hdr, ve);
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
		uint16x8_t dp1, dp2;

		lp = pnum;
		lp[0] = 1;

		processx4_step3(pkts_burst, dst_port);

		/* dp1: <d[0], d[1], d[2], d[3], ... > */
		dp1 = vld1q_u16(dst_port);

		for (j = FWDSTEP; j != k; j += FWDSTEP) {
			processx4_step3(&pkts_burst[j], &dst_port[j]);

			/*
			 * dp2:
			 * <d[j-3], d[j-2], d[j-1], d[j], ... >
			 */
			dp2 = vld1q_u16(&dst_port[j - FWDSTEP + 1]);
			lp  = port_groupx4(&pnum[j - FWDSTEP], lp, dp1, dp2);

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
		/* fallthrough */
	case 2:
		process_packet(pkts_burst[j], dst_port + j);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
		/* fallthrough */
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

#endif /* _L3FWD_NEON_H_ */
