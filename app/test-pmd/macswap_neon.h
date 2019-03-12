/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 *
 * Copyright(c) 2019 Intel Corporation
 *
 * Derived do_macswap implementation from app/test-pmd/macswap_sse.h
 */

#ifndef _MACSWAP_NEON_H_
#define _MACSWAP_NEON_H_

#include "macswap_common.h"
#include "rte_vect.h"

static inline void
do_macswap(struct rte_mbuf *pkts[], uint16_t nb,
		struct rte_port *txp)
{
	struct ether_hdr *eth_hdr[4];
	struct rte_mbuf *mb[4];
	uint64_t ol_flags;
	int i;
	int r;
	uint8x16_t v0, v1, v2, v3;
	/**
	 * Index map be used to shuffle the 16 bytes.
	 * byte 0-5 will be swapped with byte 6-11.
	 * byte 12-15 will keep unchanged.
	 */
	const uint8x16_t idx_map = {6, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5,
				12, 13, 14, 15};

	ol_flags = ol_flags_init(txp->dev_conf.txmode.offloads);
	vlan_qinq_set(pkts, nb, ol_flags,
			txp->tx_vlan_id, txp->tx_vlan_id_outer);

	i = 0;
	r = nb;

	while (r >= 4) {
		if (r >= 8) {
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i + 4], void *));
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i + 5], void *));
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i + 6], void *));
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i + 7], void *));
		}

		mb[0] = pkts[i++];
		eth_hdr[0] = rte_pktmbuf_mtod(mb[0], struct ether_hdr *);

		mb[1] = pkts[i++];
		eth_hdr[1] = rte_pktmbuf_mtod(mb[1], struct ether_hdr *);

		mb[2] = pkts[i++];
		eth_hdr[2] = rte_pktmbuf_mtod(mb[2], struct ether_hdr *);

		mb[3] = pkts[i++];
		eth_hdr[3] = rte_pktmbuf_mtod(mb[3], struct ether_hdr *);

		v0 = vld1q_u8((uint8_t const *)eth_hdr[0]);
		v1 = vld1q_u8((uint8_t const *)eth_hdr[1]);
		v2 = vld1q_u8((uint8_t const *)eth_hdr[2]);
		v3 = vld1q_u8((uint8_t const *)eth_hdr[3]);

		v0 = vqtbl1q_u8(v0, idx_map);
		v1 = vqtbl1q_u8(v1, idx_map);
		v2 = vqtbl1q_u8(v2, idx_map);
		v3 = vqtbl1q_u8(v3, idx_map);

		vst1q_u8((uint8_t *)eth_hdr[0], v0);
		vst1q_u8((uint8_t *)eth_hdr[1], v1);
		vst1q_u8((uint8_t *)eth_hdr[2], v2);
		vst1q_u8((uint8_t *)eth_hdr[3], v3);

		mbuf_field_set(mb[0], ol_flags);
		mbuf_field_set(mb[1], ol_flags);
		mbuf_field_set(mb[2], ol_flags);
		mbuf_field_set(mb[3], ol_flags);
		r -= 4;
	}

	for ( ; i < nb; i++) {
		if (i < nb - 1)
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i+1], void *));
		mb[0] = pkts[i];
		eth_hdr[0] = rte_pktmbuf_mtod(mb[0], struct ether_hdr *);

		/* Swap dest and src mac addresses. */
		v0 = vld1q_u8((uint8_t const *)eth_hdr[0]);
		v0 = vqtbl1q_u8(v0, idx_map);
		vst1q_u8((uint8_t *)eth_hdr[0], v0);

		mbuf_field_set(mb[0], ol_flags);
	}
}

#endif /* _MACSWAP_NEON_H_ */
