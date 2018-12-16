/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _MACSWAP_SSE_H_
#define _MACSWAP_SSE_H_

#include "macswap_common.h"

static inline void
do_macswap(struct rte_mbuf *pkts[], uint16_t nb,
		struct rte_port *txp)
{
	struct ether_hdr *eth_hdr;
	struct rte_mbuf *mb;
	uint64_t ol_flags;
	int i;
	__m128i addr;
	/**
	 * shuffle mask be used to shuffle the 16 bytes.
	 * byte 0-5 wills be swapped with byte 6-11.
	 * byte 12-15 will keep unchanged.
	 */
	__m128i shfl_msk = _mm_set_epi8(15, 14, 13, 12,
					5, 4, 3, 2,
					1, 0, 11, 10,
					9, 8, 7, 6);

	ol_flags = ol_flags_init(txp->dev_conf.txmode.offloads);
	vlan_qinq_set(pkts, nb, ol_flags,
			txp->tx_vlan_id, txp->tx_vlan_id_outer);

	for (i = 0; i < nb; i++) {
		if (likely(i < nb - 1))
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i+1], void *));
		mb = pkts[i];

		eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);

		/* Swap dest and src mac addresses. */
		addr = _mm_loadu_si128((__m128i *)eth_hdr);
		addr = _mm_shuffle_epi8(addr, shfl_msk);
		_mm_storeu_si128((__m128i *)eth_hdr, addr);

		mbuf_field_set(mb, ol_flags);
	}
}

#endif /* _MACSWAP_SSE_H_ */
