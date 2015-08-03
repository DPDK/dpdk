/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_rxtx.h"

#include <tmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline void
ixgbe_rxq_rearm(struct ixgbe_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct ixgbe_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	__m128i dma_addr0, dma_addr1;

	const __m128i hba_msk = _mm_set_epi64x(0, UINT64_MAX);

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mb_pool,
				 (void *)rxep,
				 RTE_IXGBE_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + RTE_IXGBE_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < RTE_IXGBE_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_IXGBE_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_IXGBE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__m128i vaddr0, vaddr1;
		uintptr_t p0, p1;

		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

		/*
		 * Flush mbuf with pkt template.
		 * Data to be rearmed is 6 bytes long.
		 * Though, RX will overwrite ol_flags that are coming next
		 * anyway. So overwrite whole 8 bytes with one load:
		 * 6 bytes of rearm_data plus first 2 bytes of ol_flags.
		 */
		p0 = (uintptr_t)&mb0->rearm_data;
		*(uint64_t *)p0 = rxq->mbuf_initializer;
		p1 = (uintptr_t)&mb1->rearm_data;
		*(uint64_t *)p1 = rxq->mbuf_initializer;

		/* load buf_addr(lo 64bit) and buf_physaddr(hi 64bit) */
		vaddr0 = _mm_loadu_si128((__m128i *)&(mb0->buf_addr));
		vaddr1 = _mm_loadu_si128((__m128i *)&(mb1->buf_addr));

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = _mm_add_epi64(dma_addr0, hdr_room);
		dma_addr1 = _mm_add_epi64(dma_addr1, hdr_room);

		/* set Header Buffer Address to zero */
		dma_addr0 =  _mm_and_si128(dma_addr0, hba_msk);
		dma_addr1 =  _mm_and_si128(dma_addr1, hba_msk);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr1);
	}

	rxq->rxrearm_start += RTE_IXGBE_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_IXGBE_RXQ_REARM_THRESH;

	rx_id = (uint16_t) ((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
}

/* Handling the offload flags (olflags) field takes computation
 * time when receiving packets. Therefore we provide a flag to disable
 * the processing of the olflags field when they are not needed. This
 * gives improved performance, at the cost of losing the offload info
 * in the received packet
 */
#ifdef RTE_IXGBE_RX_OLFLAGS_ENABLE

#ifndef RTE_NEXT_ABI
#define OLFLAGS_MASK	((uint16_t)(PKT_RX_VLAN_PKT | PKT_RX_IPV4_HDR |\
			PKT_RX_IPV4_HDR_EXT | PKT_RX_IPV6_HDR |\
			PKT_RX_IPV6_HDR_EXT))
#define PTYPE_SHIFT    (1)
#endif /* RTE_NEXT_ABI */

#define VTAG_SHIFT     (3)

static inline void
desc_to_olflags_v(__m128i descs[4], struct rte_mbuf **rx_pkts)
{
#ifdef RTE_NEXT_ABI
	__m128i ptype0, ptype1, vtag0, vtag1;
	union {
		uint16_t e[4];
		uint64_t dword;
	} vol;

	/* pkt type + vlan olflags mask */
	const __m128i pkttype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			PKT_RX_VLAN_PKT, PKT_RX_VLAN_PKT, PKT_RX_VLAN_PKT, PKT_RX_VLAN_PKT);

	/* mask everything except rss type */
	const __m128i rsstype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x000F, 0x000F, 0x000F, 0x000F);

	/* map rss type to rss hash flag */
	const __m128i rss_flags = _mm_set_epi8(PKT_RX_FDIR, 0, 0, 0,
			0, 0, 0, PKT_RX_RSS_HASH,
			PKT_RX_RSS_HASH, 0, PKT_RX_RSS_HASH, 0,
			PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, 0);

	ptype0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	ptype1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	vtag0 = _mm_unpackhi_epi16(descs[0], descs[1]);
	vtag1 = _mm_unpackhi_epi16(descs[2], descs[3]);

	ptype0 = _mm_unpacklo_epi32(ptype0, ptype1);
	ptype0 = _mm_and_si128(ptype0, rsstype_msk);
	ptype0 = _mm_shuffle_epi8(rss_flags, ptype0);

	vtag1 = _mm_unpacklo_epi32(vtag0, vtag1);
	vtag1 = _mm_srli_epi16(vtag1, VTAG_SHIFT);
	vtag1 = _mm_and_si128(vtag1, pkttype_msk);

	vtag1 = _mm_or_si128(ptype0, vtag1);
	vol.dword = _mm_cvtsi128_si64(vtag1);
#else
	__m128i ptype0, ptype1, vtag0, vtag1;
	union {
		uint16_t e[4];
		uint64_t dword;
	} vol;

	/* pkt type + vlan olflags mask */
	const __m128i pkttype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			OLFLAGS_MASK, OLFLAGS_MASK, OLFLAGS_MASK, OLFLAGS_MASK);

	/* mask everything except rss type */
	const __m128i rsstype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x000F, 0x000F, 0x000F, 0x000F);

	/* rss type to PKT_RX_RSS_HASH translation */
	const __m128i rss_flags = _mm_set_epi8(PKT_RX_FDIR, 0, 0, 0,
			0, 0, 0, PKT_RX_RSS_HASH,
			PKT_RX_RSS_HASH, 0, PKT_RX_RSS_HASH, 0,
			PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, 0);

	ptype0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	ptype1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	vtag0 = _mm_unpackhi_epi16(descs[0], descs[1]);
	vtag1 = _mm_unpackhi_epi16(descs[2], descs[3]);

	ptype1 = _mm_unpacklo_epi32(ptype0, ptype1);
	vtag1 = _mm_unpacklo_epi32(vtag0, vtag1);

	ptype0 = _mm_and_si128(ptype1, rsstype_msk);
	ptype0 = _mm_shuffle_epi8(rss_flags, ptype0);

	ptype1 = _mm_slli_epi16(ptype1, PTYPE_SHIFT);
	vtag1 = _mm_srli_epi16(vtag1, VTAG_SHIFT);

	ptype1 = _mm_or_si128(ptype1, vtag1);
	ptype1 = _mm_and_si128(ptype1, pkttype_msk);

	ptype0 = _mm_or_si128(ptype0, ptype1);

	vol.dword = _mm_cvtsi128_si64(ptype0);
#endif /* RTE_NEXT_ABI */

	rx_pkts[0]->ol_flags = vol.e[0];
	rx_pkts[1]->ol_flags = vol.e[1];
	rx_pkts[2]->ol_flags = vol.e[2];
	rx_pkts[3]->ol_flags = vol.e[3];
}
#else
#define desc_to_olflags_v(desc, rx_pkts) do {} while (0)
#endif

/*
 * vPMD receive routine, now only accept (nb_pkts == RTE_IXGBE_VPMD_RX_BURST)
 * in one loop
 *
 * Notice:
 * - nb_pkts < RTE_IXGBE_VPMD_RX_BURST, just return no packet
 * - nb_pkts > RTE_IXGBE_VPMD_RX_BURST, only scan RTE_IXGBE_VPMD_RX_BURST
 *   numbers of DD bit
 * - don't support ol_flags for rss and csum err
 */
static inline uint16_t
_recv_raw_pkts_vec(struct ixgbe_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct ixgbe_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__m128i shuf_msk;
#ifdef RTE_NEXT_ABI
	__m128i crc_adjust = _mm_set_epi16(
				0, 0, 0,    /* ignore non-length fields */
				-rxq->crc_len, /* sub crc on data_len */
				0,          /* ignore high-16bits of pkt_len */
				-rxq->crc_len, /* sub crc on pkt_len */
				0, 0            /* ignore pkt_type field */
			);
	__m128i dd_check, eop_check;
	__m128i desc_mask = _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF,
					  0xFFFFFFFF, 0xFFFF07F0);
#else
	__m128i crc_adjust = _mm_set_epi16(
				0, 0, 0, 0, /* ignore non-length fields */
				0,          /* ignore high-16bits of pkt_len */
				-rxq->crc_len, /* sub crc on pkt_len */
				-rxq->crc_len, /* sub crc on data_len */
				0            /* ignore pkt_type field */
			);
	__m128i dd_check, eop_check;
#endif /* RTE_NEXT_ABI */

	if (unlikely(nb_pkts < RTE_IXGBE_VPMD_RX_BURST))
		return 0;

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	_mm_prefetch((const void *)rxdp, _MM_HINT_T0);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act */
	if (rxq->rxrearm_nb > RTE_IXGBE_RXQ_REARM_THRESH)
		ixgbe_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available */
	if (!(rxdp->wb.upper.status_error &
				rte_cpu_to_le_32(IXGBE_RXDADV_STAT_DD)))
		return 0;

	/* 4 packets DD mask */
	dd_check = _mm_set_epi64x(0x0000000100000001LL, 0x0000000100000001LL);

	/* 4 packets EOP mask */
	eop_check = _mm_set_epi64x(0x0000000200000002LL, 0x0000000200000002LL);

	/* mask to shuffle from desc. to mbuf */
#ifdef RTE_NEXT_ABI
	shuf_msk = _mm_set_epi8(
		7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		15, 14,      /* octet 14~15, low 16 bits vlan_macip */
		13, 12,      /* octet 12~13, 16 bits data_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		13, 12,      /* octet 12~13, low 16 bits pkt_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_type */
		1,           /* octet 1, 8 bits pkt_type field */
		0            /* octet 0, 4 bits offset 4 pkt_type field */
		);
#else
	shuf_msk = _mm_set_epi8(
		7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		0xFF, 0xFF,  /* skip high 16 bits vlan_macip, zero out */
		15, 14,      /* octet 14~15, low 16 bits vlan_macip */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		13, 12,      /* octet 12~13, low 16 bits pkt_len */
		13, 12,      /* octet 12~13, 16 bits data_len */
		0xFF, 0xFF   /* skip pkt_type field */
		);
#endif /* RTE_NEXT_ABI */

	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

#ifdef RTE_NEXT_ABI
	/* A. load 4 packet in one loop
	 * [A*. mask out 4 unused dirty field in desc]
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */
#else
	/* A. load 4 packet in one loop
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */
#endif /* RTE_NEXT_ABI */
	for (pos = 0, nb_pkts_recd = 0; pos < RTE_IXGBE_VPMD_RX_BURST;
			pos += RTE_IXGBE_DESCS_PER_LOOP,
			rxdp += RTE_IXGBE_DESCS_PER_LOOP) {
#ifdef RTE_NEXT_ABI
		__m128i descs0[RTE_IXGBE_DESCS_PER_LOOP];
#endif /* RTE_NEXT_ABI */
		__m128i descs[RTE_IXGBE_DESCS_PER_LOOP];
		__m128i pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		__m128i zero, staterr, sterr_tmp1, sterr_tmp2;
		__m128i mbp1, mbp2; /* two mbuf pointer in one XMM reg. */

		if (split_packet) {
			rte_prefetch0(&rx_pkts[pos]->cacheline1);
			rte_prefetch0(&rx_pkts[pos + 1]->cacheline1);
			rte_prefetch0(&rx_pkts[pos + 2]->cacheline1);
			rte_prefetch0(&rx_pkts[pos + 3]->cacheline1);
		}

		/* B.1 load 1 mbuf point */
		mbp1 = _mm_loadu_si128((__m128i *)&sw_ring[pos]);

#ifdef RTE_NEXT_ABI
		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load 4 pkts desc */
		descs0[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

		/* B.1 load 1 mbuf point */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos+2]);

		descs0[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		/* B.1 load 2 mbuf point */
		descs0[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		descs0[0] = _mm_loadu_si128((__m128i *)(rxdp));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos+2], mbp2);

		/* A* mask out 0~3 bits RSS type */
		descs[3] = _mm_and_si128(descs0[3], desc_mask);
		descs[2] = _mm_and_si128(descs0[2], desc_mask);

		/* A* mask out 0~3 bits RSS type */
		descs[1] = _mm_and_si128(descs0[1], desc_mask);
		descs[0] = _mm_and_si128(descs0[0], desc_mask);
#else
		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load 4 pkts desc */
		descs[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

		/* B.1 load 1 mbuf point */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos + 2]);

		descs[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		/* B.1 load 2 mbuf point */
		descs[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		descs[0] = _mm_loadu_si128((__m128i *)(rxdp));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos + 2], mbp2);
#endif /* RTE_NEXT_ABI */

		/* avoid compiler reorder optimization */
		rte_compiler_barrier();

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs[2], shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

#ifdef RTE_NEXT_ABI
		/* set ol_flags with vlan packet type */
		desc_to_olflags_v(descs0, &rx_pkts[pos]);
#else
		/* set ol_flags with packet type and vlan tag */
		desc_to_olflags_v(descs, &rx_pkts[pos]);
#endif /* RTE_NEXT_ABI */

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb4 = _mm_add_epi16(pkt_mb4, crc_adjust);
		pkt_mb3 = _mm_add_epi16(pkt_mb3, crc_adjust);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = _mm_shuffle_epi8(descs[1], shuf_msk);
		pkt_mb1 = _mm_shuffle_epi8(descs[0], shuf_msk);

		/* C.2 get 4 pkts staterr value  */
		zero = _mm_xor_si128(dd_check, dd_check);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos+3]->rx_descriptor_fields1,
				pkt_mb4);
		_mm_storeu_si128((void *)&rx_pkts[pos+2]->rx_descriptor_fields1,
				pkt_mb3);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		pkt_mb2 = _mm_add_epi16(pkt_mb2, crc_adjust);
		pkt_mb1 = _mm_add_epi16(pkt_mb1, crc_adjust);

		/* C* extract and record EOP bit */
		if (split_packet) {
			__m128i eop_shuf_mask = _mm_set_epi8(
					0xFF, 0xFF, 0xFF, 0xFF,
					0xFF, 0xFF, 0xFF, 0xFF,
					0xFF, 0xFF, 0xFF, 0xFF,
					0x04, 0x0C, 0x00, 0x08
					);

			/* and with mask to extract bits, flipping 1-0 */
			__m128i eop_bits = _mm_andnot_si128(staterr, eop_check);
			/* the staterr values are not in order, as the count
			 * count of dd bits doesn't care. However, for end of
			 * packet tracking, we do care, so shuffle. This also
			 * compresses the 32-bit values to 8-bit */
			eop_bits = _mm_shuffle_epi8(eop_bits, eop_shuf_mask);
			/* store the resulting 32-bit value */
			*(int *)split_packet = _mm_cvtsi128_si32(eop_bits);
			split_packet += RTE_IXGBE_DESCS_PER_LOOP;

			/* zero-out next pointers */
			rx_pkts[pos]->next = NULL;
			rx_pkts[pos + 1]->next = NULL;
			rx_pkts[pos + 2]->next = NULL;
			rx_pkts[pos + 3]->next = NULL;
		}

		/* C.3 calc available number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos+1]->rx_descriptor_fields1,
				pkt_mb2);
		_mm_storeu_si128((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				pkt_mb1);

		/* C.4 calc avaialbe number of desc */
		var = __builtin_popcountll(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != RTE_IXGBE_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/*
 * vPMD receive routine, now only accept (nb_pkts == RTE_IXGBE_VPMD_RX_BURST)
 * in one loop
 *
 * Notice:
 * - nb_pkts < RTE_IXGBE_VPMD_RX_BURST, just return no packet
 * - nb_pkts > RTE_IXGBE_VPMD_RX_BURST, only scan RTE_IXGBE_VPMD_RX_BURST
 *   numbers of DD bit
 * - don't support ol_flags for rss and csum err
 */
uint16_t
ixgbe_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

static inline uint16_t
reassemble_packets(struct ixgbe_rx_queue *rxq, struct rte_mbuf **rx_bufs,
		uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[RTE_IXGBE_VPMD_RX_BURST]; /*finished pkts*/
	struct rte_mbuf *start = rxq->pkt_first_seg;
	struct rte_mbuf *end =  rxq->pkt_last_seg;
	unsigned pkt_idx, buf_idx;


	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end != NULL) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;

			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
				start->hash = end->hash;
				start->ol_flags = end->ol_flags;
				/* we need to strip crc for the whole packet */
				start->pkt_len -= rxq->crc_len;
				if (end->data_len > rxq->crc_len)
					end->data_len -= rxq->crc_len;
				else {
					/* free up last mbuf */
					struct rte_mbuf *secondlast = start;

					start->nb_segs--;
					while (secondlast->next != end)
						secondlast = secondlast->next;
					secondlast->data_len -= (rxq->crc_len -
							end->data_len);
					secondlast->next = NULL;
					rte_pktmbuf_free_seg(end);
					end = secondlast;
				}
				pkts[pkt_idx++] = start;
				start = end = NULL;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			end = start = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;
			rx_bufs[buf_idx]->pkt_len += rxq->crc_len;
		}
	}

	/* save the partial packet for next time */
	rxq->pkt_first_seg = start;
	rxq->pkt_last_seg = end;
	memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

/*
 * vPMD receive routine that reassembles scattered packets
 *
 * Notice:
 * - don't support ol_flags for rss and csum err
 * - now only accept (nb_pkts == RTE_IXGBE_VPMD_RX_BURST)
 */
uint16_t
ixgbe_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct ixgbe_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_IXGBE_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts,
			split_flags);
	if (nb_bufs == 0)
		return 0;

	/* happy day case, full burst + no packets to be joined */
	const uint64_t *split_fl64 = (uint64_t *)split_flags;
	if (rxq->pkt_first_seg == NULL &&
			split_fl64[0] == 0 && split_fl64[1] == 0 &&
			split_fl64[2] == 0 && split_fl64[3] == 0)
		return nb_bufs;

	/* reassemble any packets that need reassembly*/
	unsigned i = 0;
	if (rxq->pkt_first_seg == NULL) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
	}
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
		&split_flags[i]);
}

static inline void
vtx1(volatile union ixgbe_adv_tx_desc *txdp,
		struct rte_mbuf *pkt, uint64_t flags)
{
	__m128i descriptor = _mm_set_epi64x((uint64_t)pkt->pkt_len << 46 |
			flags | pkt->data_len,
			pkt->buf_physaddr + pkt->data_off);
	_mm_store_si128((__m128i *)&txdp->read, descriptor);
}

static inline void
vtx(volatile union ixgbe_adv_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	int i;
	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

static inline int __attribute__((always_inline))
ixgbe_tx_free_bufs(struct ixgbe_tx_queue *txq)
{
	struct ixgbe_tx_entry_v *txep;
	uint32_t status;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[RTE_IXGBE_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].wb.status;
	if (!(status & IXGBE_ADVTXD_STAT_DD))
		return 0;

	n = txq->tx_rs_thresh;

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1)
	 */
	txep = &txq->sw_ring_v[txq->tx_next_dd - (n - 1)];
	m = __rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = __rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool))
					free[nb_free++] = m;
				else {
					rte_mempool_put_bulk(free[0]->pool,
							(void *)free, nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
	} else {
		for (i = 1; i < n; i++) {
			m = __rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
		}
	}

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

static inline void __attribute__((always_inline))
tx_backlog_entry(struct ixgbe_tx_entry_v *txep,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;
	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

uint16_t
ixgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	struct ixgbe_tx_queue *txq = (struct ixgbe_tx_queue *)tx_queue;
	volatile union ixgbe_adv_tx_desc *txdp;
	struct ixgbe_tx_entry_v *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = DCMD_DTYP_FLAGS;
	uint64_t rs = IXGBE_ADVTXD_DCMD_RS|DCMD_DTYP_FLAGS;
	int i;

	if (unlikely(nb_pkts > RTE_IXGBE_VPMD_TX_BURST))
		nb_pkts = RTE_IXGBE_VPMD_TX_BURST;

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ixgbe_tx_free_bufs(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring_v[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {

		tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &(txq->tx_ring[tx_id]);
		txep = &txq->sw_ring_v[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].read.cmd_type_len |=
			rte_cpu_to_le_32(IXGBE_ADVTXD_DCMD_RS);
		txq->tx_next_rs = (uint16_t)(txq->tx_next_rs +
			txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	IXGBE_PCI_REG_WRITE(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

static void __attribute__((cold))
ixgbe_tx_queue_release_mbufs_vec(struct ixgbe_tx_queue *txq)
{
	unsigned i;
	struct ixgbe_tx_entry_v *txe;
	const uint16_t max_desc = (uint16_t)(txq->nb_tx_desc - 1);

	if (txq->sw_ring == NULL || txq->nb_tx_free == max_desc)
		return;

	/* release the used mbufs in sw_ring */
	for (i = txq->tx_next_dd - (txq->tx_rs_thresh - 1);
	     i != txq->tx_tail;
	     i = (i + 1) & max_desc) {
		txe = &txq->sw_ring_v[i];
		rte_pktmbuf_free_seg(txe->mbuf);
	}
	txq->nb_tx_free = max_desc;

	/* reset tx_entry */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txe = &txq->sw_ring_v[i];
		txe->mbuf = NULL;
	}
}

void __attribute__((cold))
ixgbe_rx_queue_release_mbufs_vec(struct ixgbe_rx_queue *rxq)
{
	const unsigned mask = rxq->nb_rx_desc - 1;
	unsigned i;

	if (rxq->sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	for (i = rxq->rx_tail; i != rxq->rxrearm_start; i = (i + 1) & mask)
		rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static void __attribute__((cold))
ixgbe_tx_free_swring(struct ixgbe_tx_queue *txq)
{
	if (txq == NULL)
		return;

	if (txq->sw_ring != NULL) {
		rte_free(txq->sw_ring_v - 1);
		txq->sw_ring_v = NULL;
	}
}

static void __attribute__((cold))
ixgbe_reset_tx_queue(struct ixgbe_tx_queue *txq)
{
	static const union ixgbe_adv_tx_desc zeroed_desc = {{0}};
	struct ixgbe_tx_entry_v *txe = txq->sw_ring_v;
	uint16_t i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union ixgbe_adv_tx_desc *txd = &txq->tx_ring[i];
		txd->wb.status = IXGBE_TXD_STAT_DD;
		txe[i].mbuf = NULL;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;
	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset((void *)&txq->ctx_cache, 0,
		IXGBE_CTX_NUM * sizeof(struct ixgbe_advctx_info));
}

static const struct ixgbe_txq_ops vec_txq_ops = {
	.release_mbufs = ixgbe_tx_queue_release_mbufs_vec,
	.free_swring = ixgbe_tx_free_swring,
	.reset = ixgbe_reset_tx_queue,
};

int __attribute__((cold))
ixgbe_rxq_vec_setup(struct ixgbe_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	return 0;
}

int __attribute__((cold))
ixgbe_txq_vec_setup(struct ixgbe_tx_queue *txq)
{
	if (txq->sw_ring_v == NULL)
		return -1;

	/* leave the first one for overflow */
	txq->sw_ring_v = txq->sw_ring_v + 1;
	txq->ops = &vec_txq_ops;

	return 0;
}

int __attribute__((cold))
ixgbe_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev)
{
#ifndef RTE_LIBRTE_IEEE1588
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct rte_fdir_conf *fconf = &dev->data->dev_conf.fdir_conf;

#ifndef RTE_IXGBE_RX_OLFLAGS_ENABLE
	/* whithout rx ol_flags, no VP flag report */
	if (rxmode->hw_vlan_strip != 0 ||
	    rxmode->hw_vlan_extend != 0)
		return -1;
#endif

	/* no fdir support */
	if (fconf->mode != RTE_FDIR_MODE_NONE)
		return -1;

	/*
	 * - no csum error report support
	 * - no header split support
	 */
	if (rxmode->hw_ip_checksum == 1 ||
	    rxmode->header_split == 1)
		return -1;

	return 0;
#else
	RTE_SET_USED(dev);
	return -1;
#endif
}
