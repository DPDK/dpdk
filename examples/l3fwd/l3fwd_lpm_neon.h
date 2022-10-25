/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */

#ifndef __L3FWD_LPM_NEON_H__
#define __L3FWD_LPM_NEON_H__

#include <arm_neon.h>

#include "l3fwd_neon.h"

/*
 * Read packet_type and destination IPV4 addresses from 4 mbufs.
 */
static inline void
processx4_step1(struct rte_mbuf *pkt[FWDSTEP],
		int32x4_t *dip,
		uint32_t *ipv4_flag)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ether_hdr *eth_hdr;
	int32_t dst[FWDSTEP];

	eth_hdr = rte_pktmbuf_mtod(pkt[0], struct rte_ether_hdr *);
	ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
	dst[0] = ipv4_hdr->dst_addr;
	ipv4_flag[0] = pkt[0]->packet_type & RTE_PTYPE_L3_IPV4;

	eth_hdr = rte_pktmbuf_mtod(pkt[1], struct rte_ether_hdr *);
	ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
	dst[1] = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[1]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[2], struct rte_ether_hdr *);
	ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
	dst[2] = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[2]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[3], struct rte_ether_hdr *);
	ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
	dst[3] = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[3]->packet_type;

	dip[0] = vld1q_s32(dst);
}

/*
 * Lookup into LPM for destination port.
 * If lookup fails, use incoming port (portid) as destination port.
 */
static inline void
processx4_step2(const struct lcore_conf *qconf,
		int32x4_t dip,
		uint32_t ipv4_flag,
		uint16_t portid,
		struct rte_mbuf *pkt[FWDSTEP],
		uint16_t dprt[FWDSTEP])
{
	rte_xmm_t dst;

	dip = vreinterpretq_s32_u8(vrev32q_u8(vreinterpretq_u8_s32(dip)));

	/* if all 4 packets are IPV4. */
	if (likely(ipv4_flag)) {
		rte_lpm_lookupx4(qconf->ipv4_lookup_struct, dip, dst.u32,
			portid);
		/* get rid of unused upper 16 bit for each dport. */
		vst1_s16((int16_t *)dprt, vqmovn_s32(dst.x));
	} else {
		dst.x = dip;
		dprt[0] = lpm_get_dst_port_with_ipv4(qconf, pkt[0],
						     dst.u32[0], portid);
		dprt[1] = lpm_get_dst_port_with_ipv4(qconf, pkt[1],
						     dst.u32[1], portid);
		dprt[2] = lpm_get_dst_port_with_ipv4(qconf, pkt[2],
						     dst.u32[2], portid);
		dprt[3] = lpm_get_dst_port_with_ipv4(qconf, pkt[3],
						     dst.u32[3], portid);
	}
}

static inline void
l3fwd_lpm_process_packets(int nb_rx, struct rte_mbuf **pkts_burst,
			  uint16_t portid, uint16_t *dst_port,
			  struct lcore_conf *qconf, const uint8_t do_step3)
{
	int32_t i = 0, j = 0;
	int32x4_t dip;
	uint32_t ipv4_flag;
	const int32_t k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	const int32_t m = nb_rx % FWDSTEP;

	if (k) {
		for (i = 0; i < FWDSTEP; i++) {
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i],
							void *));
		}
		for (j = 0; j != k - FWDSTEP; j += FWDSTEP) {
			for (i = 0; i < FWDSTEP; i++) {
				rte_prefetch0(rte_pktmbuf_mtod(
						pkts_burst[j + i + FWDSTEP],
						void *));
			}

			processx4_step1(&pkts_burst[j], &dip, &ipv4_flag);
			processx4_step2(qconf, dip, ipv4_flag, portid,
					&pkts_burst[j], &dst_port[j]);
			if (do_step3)
				processx4_step3(&pkts_burst[j], &dst_port[j]);
		}

		processx4_step1(&pkts_burst[j], &dip, &ipv4_flag);
		processx4_step2(qconf, dip, ipv4_flag, portid, &pkts_burst[j],
				&dst_port[j]);
		if (do_step3)
			processx4_step3(&pkts_burst[j], &dst_port[j]);

		j += FWDSTEP;
	}

	if (m) {
		/* Prefetch last up to 3 packets one by one */
		switch (m) {
		case 3:
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j],
							void *));
			j++;
			/* fallthrough */
		case 2:
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j],
							void *));
			j++;
			/* fallthrough */
		case 1:
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j],
							void *));
			j++;
		}
		j -= m;
		/* Classify last up to 3 packets one by one */
		switch (m) {
		case 3:
			dst_port[j] = lpm_get_dst_port(qconf, pkts_burst[j],
						       portid);
			if (do_step3)
				process_packet(pkts_burst[j], &dst_port[j]);
			j++;
			/* fallthrough */
		case 2:
			dst_port[j] = lpm_get_dst_port(qconf, pkts_burst[j],
						       portid);
			if (do_step3)
				process_packet(pkts_burst[j], &dst_port[j]);
			j++;
			/* fallthrough */
		case 1:
			dst_port[j] = lpm_get_dst_port(qconf, pkts_burst[j],
						       portid);
			if (do_step3)
				process_packet(pkts_burst[j], &dst_port[j]);
		}
	}
}

/*
 * Buffer optimized handling of packets, invoked
 * from main_loop.
 */
static inline void
l3fwd_lpm_send_packets(int nb_rx, struct rte_mbuf **pkts_burst, uint16_t portid,
		       struct lcore_conf *qconf)
{
	uint16_t dst_port[MAX_PKT_BURST];

	l3fwd_lpm_process_packets(nb_rx, pkts_burst, portid, dst_port, qconf,
				  0);
	send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);
}

#endif /* __L3FWD_LPM_NEON_H__ */
