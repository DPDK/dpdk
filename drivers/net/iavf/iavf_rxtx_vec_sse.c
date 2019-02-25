/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>

#include "base/iavf_prototype.h"
#include "base/iavf_type.h"
#include "iavf.h"
#include "iavf_rxtx.h"
#include "iavf_rxtx_vec_common.h"

#include <tmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline void
iavf_rxq_rearm(struct iavf_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;

	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	__m128i dma_addr0, dma_addr1;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp, (void *)rxp,
				 rxq->rx_free_thresh) < 0) {
		if (rxq->rxrearm_nb + rxq->rx_free_thresh >= rxq->nb_rx_desc) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IAVF_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			rxq->rx_free_thresh;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < rxq->rx_free_thresh; i += 2, rxp += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxp[0];
		mb1 = rxp[1];

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
		vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = _mm_add_epi64(dma_addr0, hdr_room);
		dma_addr1 = _mm_add_epi64(dma_addr1, hdr_room);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr1);
	}

	rxq->rxrearm_start += rxq->rx_free_thresh;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= rxq->rx_free_thresh;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			   (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
		   "rearm_start=%u rearm_nb=%u",
		   rxq->port_id, rxq->queue_id,
		   rx_id, rxq->rxrearm_start, rxq->rxrearm_nb);

	/* Update the tail pointer on the NIC */
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
}

static inline void
desc_to_olflags_v(struct iavf_rx_queue *rxq, __m128i descs[4],
		  struct rte_mbuf **rx_pkts)
{
	const __m128i mbuf_init = _mm_set_epi64x(0, rxq->mbuf_initializer);
	__m128i rearm0, rearm1, rearm2, rearm3;

	__m128i vlan0, vlan1, rss, l3_l4e;

	/* mask everything except RSS, flow director and VLAN flags
	 * bit2 is for VLAN tag, bit11 for flow director indication
	 * bit13:12 for RSS indication.
	 */
	const __m128i rss_vlan_msk = _mm_set_epi32(
			0x1c03804, 0x1c03804, 0x1c03804, 0x1c03804);

	const __m128i cksum_mask = _mm_set_epi32(
			PKT_RX_IP_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD |
			PKT_RX_L4_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD |
			PKT_RX_EIP_CKSUM_BAD,
			PKT_RX_IP_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD |
			PKT_RX_L4_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD |
			PKT_RX_EIP_CKSUM_BAD,
			PKT_RX_IP_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD |
			PKT_RX_L4_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD |
			PKT_RX_EIP_CKSUM_BAD,
			PKT_RX_IP_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD |
			PKT_RX_L4_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD |
			PKT_RX_EIP_CKSUM_BAD);

	/* map rss and vlan type to rss hash and vlan flag */
	const __m128i vlan_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
			0, 0, 0, 0);

	const __m128i rss_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			PKT_RX_RSS_HASH | PKT_RX_FDIR, PKT_RX_RSS_HASH, 0, 0,
			0, 0, PKT_RX_FDIR, 0);

	const __m128i l3_l4e_flags = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
			/* shift right 1 bit to make sure it not exceed 255 */
			(PKT_RX_EIP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD |
			 PKT_RX_IP_CKSUM_BAD) >> 1,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_EIP_CKSUM_BAD |
			 PKT_RX_L4_CKSUM_BAD) >> 1,
			(PKT_RX_EIP_CKSUM_BAD | PKT_RX_IP_CKSUM_BAD) >> 1,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_EIP_CKSUM_BAD) >> 1,
			(PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_BAD) >> 1,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD) >> 1,
			PKT_RX_IP_CKSUM_BAD >> 1,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD) >> 1);

	vlan0 = _mm_unpackhi_epi32(descs[0], descs[1]);
	vlan1 = _mm_unpackhi_epi32(descs[2], descs[3]);
	vlan0 = _mm_unpacklo_epi64(vlan0, vlan1);

	vlan1 = _mm_and_si128(vlan0, rss_vlan_msk);
	vlan0 = _mm_shuffle_epi8(vlan_flags, vlan1);

	rss = _mm_srli_epi32(vlan1, 11);
	rss = _mm_shuffle_epi8(rss_flags, rss);

	l3_l4e = _mm_srli_epi32(vlan1, 22);
	l3_l4e = _mm_shuffle_epi8(l3_l4e_flags, l3_l4e);
	/* then we shift left 1 bit */
	l3_l4e = _mm_slli_epi32(l3_l4e, 1);
	/* we need to mask out the reduntant bits */
	l3_l4e = _mm_and_si128(l3_l4e, cksum_mask);

	vlan0 = _mm_or_si128(vlan0, rss);
	vlan0 = _mm_or_si128(vlan0, l3_l4e);

	/* At this point, we have the 4 sets of flags in the low 16-bits
	 * of each 32-bit value in vlan0.
	 * We want to extract these, and merge them with the mbuf init data
	 * so we can do a single 16-byte write to the mbuf to set the flags
	 * and all the other initialization fields. Extracting the
	 * appropriate flags means that we have to do a shift and blend for
	 * each mbuf before we do the write.
	 */
	rearm0 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vlan0, 8), 0x10);
	rearm1 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vlan0, 4), 0x10);
	rearm2 = _mm_blend_epi16(mbuf_init, vlan0, 0x10);
	rearm3 = _mm_blend_epi16(mbuf_init, _mm_srli_si128(vlan0, 4), 0x10);

	/* write the rearm data and the olflags in one write */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			offsetof(struct rte_mbuf, rearm_data) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
			RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));
	_mm_store_si128((__m128i *)&rx_pkts[0]->rearm_data, rearm0);
	_mm_store_si128((__m128i *)&rx_pkts[1]->rearm_data, rearm1);
	_mm_store_si128((__m128i *)&rx_pkts[2]->rearm_data, rearm2);
	_mm_store_si128((__m128i *)&rx_pkts[3]->rearm_data, rearm3);
}

#define PKTLEN_SHIFT     10

static inline void
desc_to_ptype_v(__m128i descs[4], struct rte_mbuf **rx_pkts)
{
	__m128i ptype0 = _mm_unpackhi_epi64(descs[0], descs[1]);
	__m128i ptype1 = _mm_unpackhi_epi64(descs[2], descs[3]);
	static const uint32_t type_table[UINT8_MAX + 1] __rte_cache_aligned = {
		/* [0] reserved */
		[1] = RTE_PTYPE_L2_ETHER,
		/* [2] - [21] reserved */
		[22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_FRAG,
		[23] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_NONFRAG,
		[24] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		/* [25] reserved */
		[26] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_TCP,
		[27] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_SCTP,
		[28] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_ICMP,
		/* All others reserved */
	};

	ptype0 = _mm_srli_epi64(ptype0, 30);
	ptype1 = _mm_srli_epi64(ptype1, 30);

	rx_pkts[0]->packet_type = type_table[_mm_extract_epi8(ptype0, 0)];
	rx_pkts[1]->packet_type = type_table[_mm_extract_epi8(ptype0, 8)];
	rx_pkts[2]->packet_type = type_table[_mm_extract_epi8(ptype1, 0)];
	rx_pkts[3]->packet_type = type_table[_mm_extract_epi8(ptype1, 8)];
}

/* Notice:
 * - nb_pkts < IAVF_VPMD_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > IAVF_VPMD_RX_MAX_BURST, only scan IAVF_VPMD_RX_MAX_BURST
 *   numbers of DD bits
 */
static inline uint16_t
_recv_raw_pkts_vec(struct iavf_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__m128i shuf_msk;

	__m128i crc_adjust = _mm_set_epi16(
				0, 0, 0,    /* ignore non-length fields */
				-rxq->crc_len, /* sub crc on data_len */
				0,          /* ignore high-16bits of pkt_len */
				-rxq->crc_len, /* sub crc on pkt_len */
				0, 0            /* ignore pkt_type field */
			);
	/* compile-time check the above crc_adjust layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi16
	 * call above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	__m128i dd_check, eop_check;

	/* nb_pkts shall be less equal than IAVF_VPMD_RX_MAX_BURST */
	nb_pkts = RTE_MIN(nb_pkts, IAVF_VPMD_RX_MAX_BURST);

	/* nb_pkts has to be floor-aligned to IAVF_VPMD_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_VPMD_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > rxq->rx_free_thresh)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
	      rte_cpu_to_le_32(1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/* 4 packets DD mask */
	dd_check = _mm_set_epi64x(0x0000000100000001LL, 0x0000000100000001LL);

	/* 4 packets EOP mask */
	eop_check = _mm_set_epi64x(0x0000000200000002LL, 0x0000000200000002LL);

	/* mask to shuffle from desc. to mbuf */
	shuf_msk = _mm_set_epi8(
		7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		3, 2,        /* octet 2~3, low 16 bits vlan_macip */
		15, 14,      /* octet 15~14, 16 bits data_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		15, 14,      /* octet 15~14, low 16 bits pkt_len */
		0xFF, 0xFF, 0xFF, 0xFF /* pkt_type set as unknown */
		);
	/* Compile-time verify the shuffle mask
	 * NOTE: some field positions already verified above, but duplicated
	 * here for completeness in case of future modifications.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache
	 */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

	/* A. load 4 packet in one loop
	 * [A*. mask out 4 unused dirty field in desc]
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */

	for (pos = 0, nb_pkts_recd = 0; pos < nb_pkts;
	     pos += IAVF_VPMD_DESCS_PER_LOOP,
	     rxdp += IAVF_VPMD_DESCS_PER_LOOP) {
		__m128i descs[IAVF_VPMD_DESCS_PER_LOOP];
		__m128i pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		__m128i zero, staterr, sterr_tmp1, sterr_tmp2;
		/* 2 64 bit or 4 32 bit mbuf pointers in one XMM reg. */
		__m128i mbp1;
#if defined(RTE_ARCH_X86_64)
		__m128i mbp2;
#endif

		/* B.1 load 2 (64 bit) or 4 (32 bit) mbuf points */
		mbp1 = _mm_loadu_si128((__m128i *)&sw_ring[pos]);
		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load 4 pkts desc */
		descs[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));
		rte_compiler_barrier();

		/* B.2 copy 2 64 bit or 4 32 bit mbuf point into rx_pkts */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

#if defined(RTE_ARCH_X86_64)
		/* B.1 load 2 64 bit mbuf points */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos + 2]);
#endif

		descs[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		rte_compiler_barrier();
		/* B.1 load 2 mbuf point */
		descs[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		rte_compiler_barrier();
		descs[0] = _mm_loadu_si128((__m128i *)(rxdp));

#if defined(RTE_ARCH_X86_64)
		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos + 2], mbp2);
#endif

		if (split_packet) {
			rte_mbuf_prefetch_part2(rx_pkts[pos]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 1]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 2]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 3]);
		}

		/* avoid compiler reorder optimization */
		rte_compiler_barrier();

		/* pkt 3,4 shift the pktlen field to be 16-bit aligned*/
		const __m128i len3 = _mm_slli_epi32(descs[3], PKTLEN_SHIFT);
		const __m128i len2 = _mm_slli_epi32(descs[2], PKTLEN_SHIFT);

		/* merge the now-aligned packet length fields back in */
		descs[3] = _mm_blend_epi16(descs[3], len3, 0x80);
		descs[2] = _mm_blend_epi16(descs[2], len2, 0x80);

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs[2], shuf_msk);

		/* C.1 4=>2 status err info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

		desc_to_olflags_v(rxq, descs, &rx_pkts[pos]);

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb4 = _mm_add_epi16(pkt_mb4, crc_adjust);
		pkt_mb3 = _mm_add_epi16(pkt_mb3, crc_adjust);

		/* pkt 1,2 shift the pktlen field to be 16-bit aligned*/
		const __m128i len1 = _mm_slli_epi32(descs[1], PKTLEN_SHIFT);
		const __m128i len0 = _mm_slli_epi32(descs[0], PKTLEN_SHIFT);

		/* merge the now-aligned packet length fields back in */
		descs[1] = _mm_blend_epi16(descs[1], len1, 0x80);
		descs[0] = _mm_blend_epi16(descs[0], len0, 0x80);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = _mm_shuffle_epi8(descs[1], shuf_msk);
		pkt_mb1 = _mm_shuffle_epi8(descs[0], shuf_msk);

		/* C.2 get 4 pkts status err value  */
		zero = _mm_xor_si128(dd_check, dd_check);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128(
			(void *)&rx_pkts[pos + 3]->rx_descriptor_fields1,
			pkt_mb4);
		_mm_storeu_si128(
			(void *)&rx_pkts[pos + 2]->rx_descriptor_fields1,
			pkt_mb3);

		/* D.2 pkt 1,2 remove crc */
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
			 * compresses the 32-bit values to 8-bit
			 */
			eop_bits = _mm_shuffle_epi8(eop_bits, eop_shuf_mask);
			/* store the resulting 32-bit value */
			*(int *)split_packet = _mm_cvtsi128_si32(eop_bits);
			split_packet += IAVF_VPMD_DESCS_PER_LOOP;
		}

		/* C.3 calc available number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128(
			(void *)&rx_pkts[pos + 1]->rx_descriptor_fields1,
			pkt_mb2);
		_mm_storeu_si128((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				 pkt_mb1);
		desc_to_ptype_v(descs, &rx_pkts[pos]);
		/* C.4 calc avaialbe number of desc */
		var = __builtin_popcountll(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != IAVF_VPMD_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/* Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > IAVF_VPMD_RX_MAX_BURST, only scan IAVF_VPMD_RX_MAX_BURST
 *   numbers of DD bits
 */
uint16_t
iavf_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		  uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

/* vPMD receive routine that reassembles scattered packets
 * Notice:
 * - nb_pkts < IAVF_VPMD_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > VPMD_RX_MAX_BURST, only scan IAVF_VPMD_RX_MAX_BURST
 *   numbers of DD bits
 */
uint16_t
iavf_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts)
{
	struct iavf_rx_queue *rxq = rx_queue;
	uint8_t split_flags[IAVF_VPMD_RX_MAX_BURST] = {0};
	unsigned int i = 0;

	/* get some new buffers */
	uint16_t nb_bufs = _recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts,
					      split_flags);
	if (nb_bufs == 0)
		return 0;

	/* happy day case, full burst + no packets to be joined */
	const uint64_t *split_fl64 = (uint64_t *)split_flags;

	if (!rxq->pkt_first_seg &&
	    split_fl64[0] == 0 && split_fl64[1] == 0 &&
	    split_fl64[2] == 0 && split_fl64[3] == 0)
		return nb_bufs;

	/* reassemble any packets that need reassembly*/
	if (!rxq->pkt_first_seg) {
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
vtx1(volatile struct iavf_tx_desc *txdp, struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw =
			(IAVF_TX_DESC_DTYPE_DATA |
			 ((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT) |
			 ((uint64_t)pkt->data_len <<
			  IAVF_TXD_QW1_TX_BUF_SZ_SHIFT));

	__m128i descriptor = _mm_set_epi64x(high_qw,
					    pkt->buf_iova + pkt->data_off);
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
iavf_vtx(volatile struct iavf_tx_desc *txdp, struct rte_mbuf **pkt,
	uint16_t nb_pkts,  uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

uint16_t
iavf_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	struct iavf_tx_queue *txq = (struct iavf_tx_queue *)tx_queue;
	volatile struct iavf_tx_desc *txdp;
	struct iavf_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = IAVF_TX_DESC_CMD_EOP | 0x04;  /* bit 2 must be set */
	uint64_t rs = IAVF_TX_DESC_CMD_RS | flags;
	int i;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->rs_thresh);

	if (txq->nb_free < txq->free_thresh)
		iavf_tx_free_bufs(txq);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;
	nb_commit = nb_pkts;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_free = (uint16_t)(txq->nb_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	iavf_vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->next_rs) {
		txq->tx_ring[txq->next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)IAVF_TX_DESC_CMD_RS) <<
					 IAVF_TXD_QW1_CMD_SHIFT);
		txq->next_rs =
			(uint16_t)(txq->next_rs + txq->rs_thresh);
	}

	txq->tx_tail = tx_id;

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_pkts=%u",
		   txq->port_id, txq->queue_id, tx_id, nb_pkts);

	IAVF_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static void __attribute__((cold))
iavf_rx_queue_release_mbufs_sse(struct iavf_rx_queue *rxq)
{
	_iavf_rx_queue_release_mbufs_vec(rxq);
}

static void __attribute__((cold))
iavf_tx_queue_release_mbufs_sse(struct iavf_tx_queue *txq)
{
	_iavf_tx_queue_release_mbufs_vec(txq);
}

static const struct iavf_rxq_ops sse_vec_rxq_ops = {
	.release_mbufs = iavf_rx_queue_release_mbufs_sse,
};

static const struct iavf_txq_ops sse_vec_txq_ops = {
	.release_mbufs = iavf_tx_queue_release_mbufs_sse,
};

int __attribute__((cold))
iavf_txq_vec_setup(struct iavf_tx_queue *txq)
{
	txq->ops = &sse_vec_txq_ops;
	return 0;
}

int __attribute__((cold))
iavf_rxq_vec_setup(struct iavf_rx_queue *rxq)
{
	rxq->ops = &sse_vec_rxq_ops;
	return iavf_rxq_vec_setup_default(rxq);
}
