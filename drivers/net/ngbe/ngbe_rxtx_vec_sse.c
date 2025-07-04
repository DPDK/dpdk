/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2024 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "ngbe_type.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"
#include "ngbe_rxtx_vec_common.h"

#include <rte_vect.h>

static inline void
ngbe_rxq_rearm(struct ngbe_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	__m128i dma_addr0, dma_addr1;

	const __m128i hba_msk = _mm_set_epi64x(0, UINT64_MAX);

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mb_pool,
				 (void *)rxep,
				 RTE_NGBE_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + RTE_NGBE_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < RTE_NGBE_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)(uintptr_t)&rxdp[i],
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_NGBE_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_NGBE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

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

		/* set Header Buffer Address to zero */
		dma_addr0 =  _mm_and_si128(dma_addr0, hba_msk);
		dma_addr1 =  _mm_and_si128(dma_addr1, hba_msk);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)(uintptr_t)rxdp++, dma_addr0);
		_mm_store_si128((__m128i *)(uintptr_t)rxdp++, dma_addr1);
	}

	rxq->rxrearm_start += RTE_NGBE_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_NGBE_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			   (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	ngbe_set32(rxq->rdt_reg_addr, rx_id);
}

static inline void
desc_to_olflags_v(__m128i descs[4], __m128i mbuf_init, uint8_t vlan_flags,
	struct rte_mbuf **rx_pkts)
{
	__m128i ptype0, ptype1, vtag0, vtag1, csum, vp;
	__m128i rearm0, rearm1, rearm2, rearm3;

	/* mask everything except rss type */
	const __m128i rsstype_msk = _mm_set_epi16(0x0000, 0x0000, 0x0000, 0x0000,
						  0x000F, 0x000F, 0x000F, 0x000F);

	/* mask the lower byte of ol_flags */
	const __m128i ol_flags_msk = _mm_set_epi16(0x0000, 0x0000, 0x0000, 0x0000,
						   0x00FF, 0x00FF, 0x00FF, 0x00FF);

	/* map rss type to rss hash flag */
	const __m128i rss_flags = _mm_set_epi8(RTE_MBUF_F_RX_FDIR, 0, 0, 0,
			0, 0, 0, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, 0, RTE_MBUF_F_RX_RSS_HASH, 0,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, 0);

	/* mask everything except vlan present and l4/ip csum error */
	const __m128i vlan_csum_msk =
		_mm_set_epi16((NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 16,
			      (NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 16,
			      (NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 16,
			      (NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 16,
			      NGBE_RXD_STAT_VLAN, NGBE_RXD_STAT_VLAN,
			      NGBE_RXD_STAT_VLAN, NGBE_RXD_STAT_VLAN);

	/* map vlan present and l4/ip csum error to ol_flags */
	const __m128i vlan_csum_map_lo = _mm_set_epi8(0, 0, 0, 0,
		vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
		vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_BAD,
		vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
		vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_GOOD,
		0, 0, 0, 0,
		RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
		RTE_MBUF_F_RX_IP_CKSUM_BAD,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD);

	const __m128i vlan_csum_map_hi = _mm_set_epi8(0, 0, 0, 0,
		0, RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
		RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t),
		0, 0, 0, 0,
		0, RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
		RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t));

	const __m128i vtag_msk = _mm_set_epi16(0x0000, 0x0000, 0x0000, 0x0000,
					       0x000F, 0x000F, 0x000F, 0x000F);

	ptype0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	ptype1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	vtag0 = _mm_unpackhi_epi16(descs[0], descs[1]);
	vtag1 = _mm_unpackhi_epi16(descs[2], descs[3]);

	ptype0 = _mm_unpacklo_epi32(ptype0, ptype1);
	ptype0 = _mm_and_si128(ptype0, rsstype_msk);
	ptype0 = _mm_shuffle_epi8(rss_flags, ptype0);

	vtag1 = _mm_unpacklo_epi32(vtag0, vtag1);
	vtag1 = _mm_and_si128(vtag1, vlan_csum_msk);

	/* csum bits are in the most significant, to use shuffle we need to
	 * shift them. Change mask to 0xc000 to 0x0003.
	 */
	csum = _mm_srli_epi16(vtag1, 14);

	/* Change mask to 0x20 to 0x08. */
	vp = _mm_srli_epi16(vtag1, 2);

	/* now or the most significant 64 bits containing the checksum
	 * flags with the vlan present flags.
	 */
	csum = _mm_srli_si128(csum, 8);
	vtag1 = _mm_or_si128(csum, vtag1);
	vtag1 = _mm_or_si128(vtag1, vp);
	vtag1 = _mm_and_si128(vtag1, vtag_msk);

	/* convert STAT_VLAN, ERR_IPCS, ERR_L4CS to ol_flags */
	vtag0 = _mm_shuffle_epi8(vlan_csum_map_hi, vtag1);
	vtag0 = _mm_slli_epi16(vtag0, sizeof(uint8_t));

	vtag1 = _mm_shuffle_epi8(vlan_csum_map_lo, vtag1);
	vtag1 = _mm_and_si128(vtag1, ol_flags_msk);
	vtag1 = _mm_or_si128(vtag0, vtag1);

	vtag1 = _mm_or_si128(ptype0, vtag1);

	/*
	 * At this point, we have the 4 sets of flags in the low 64-bits
	 * of vtag1 (4x16).
	 * We want to extract these, and merge them with the mbuf init data
	 * so we can do a single 16-byte write to the mbuf to set the flags
	 * and all the other initialization fields. Extracting the
	 * appropriate flags means that we have to do a shift and blend for
	 * each mbuf before we do the write.
	 */
	rearm0 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vtag1, 8), 0x10);
	rearm1 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vtag1, 6), 0x10);
	rearm2 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vtag1, 4), 0x10);
	rearm3 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(vtag1, 2), 0x10);

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

static inline void
desc_to_ptype_v(__m128i descs[4], uint16_t pkt_type_mask,
		struct rte_mbuf **rx_pkts)
{
	__m128i ptype_mask = _mm_set_epi32(pkt_type_mask, pkt_type_mask,
					pkt_type_mask, pkt_type_mask);

	__m128i ptype0 = _mm_unpacklo_epi32(descs[0], descs[2]);
	__m128i ptype1 = _mm_unpacklo_epi32(descs[1], descs[3]);

	/* interleave low 32 bits,
	 * now we have 4 ptypes in a XMM register
	 */
	ptype0 = _mm_unpacklo_epi32(ptype0, ptype1);

	/* shift left by NGBE_RXD_PTID_SHIFT, and apply ptype mask */
	ptype0 = _mm_and_si128(_mm_srli_epi32(ptype0, NGBE_RXD_PTID_SHIFT),
			       ptype_mask);

	rx_pkts[0]->packet_type = ngbe_decode_ptype(_mm_extract_epi32(ptype0, 0));
	rx_pkts[1]->packet_type = ngbe_decode_ptype(_mm_extract_epi32(ptype0, 1));
	rx_pkts[2]->packet_type = ngbe_decode_ptype(_mm_extract_epi32(ptype0, 2));
	rx_pkts[3]->packet_type = ngbe_decode_ptype(_mm_extract_epi32(ptype0, 3));
}

/*
 * vPMD raw receive routine, only accept(nb_pkts >= RTE_NGBE_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < RTE_NGBE_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a RTE_NGBE_DESC_PER_LOOP power-of-two
 */
static inline uint16_t
_recv_raw_pkts_vec(struct ngbe_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__m128i shuf_msk;
	__m128i crc_adjust = _mm_set_epi16(0, 0, 0, /* ignore non-length fields */
				-rxq->crc_len, /* sub crc on data_len */
				0,             /* ignore high-16bits of pkt_len */
				-rxq->crc_len, /* sub crc on pkt_len */
				0, 0);         /* ignore pkt_type field */

	/*
	 * compile-time check the above crc_adjust layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi16
	 * call above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	__m128i dd_check, eop_check;
	__m128i mbuf_init;
	uint8_t vlan_flags;

	/*
	 * Under the circumstance that `rx_tail` wrap back to zero
	 * and the advance speed of `rx_tail` is greater than `rxrearm_start`,
	 * `rx_tail` will catch up with `rxrearm_start` and surpass it.
	 * This may cause some mbufs be reused by application.
	 *
	 * So we need to make some restrictions to ensure that
	 * `rx_tail` will not exceed `rxrearm_start`.
	 */
	nb_pkts = RTE_MIN(nb_pkts, RTE_NGBE_RXQ_REARM_THRESH);

	/* nb_pkts has to be floor-aligned to RTE_NGBE_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_NGBE_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_NGBE_RXQ_REARM_THRESH)
		ngbe_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->qw1.lo.status &
				rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
		return 0;

	/* 4 packets DD mask */
	dd_check = _mm_set_epi64x(0x0000000100000001LL, 0x0000000100000001LL);

	/* 4 packets EOP mask */
	eop_check = _mm_set_epi64x(0x0000000200000002LL, 0x0000000200000002LL);

	/* mask to shuffle from desc. to mbuf */
	shuf_msk = _mm_set_epi8(7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		15, 14,      /* octet 14~15, low 16 bits vlan_macip */
		13, 12,      /* octet 12~13, 16 bits data_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		13, 12,      /* octet 12~13, low 16 bits pkt_len */
		0xFF, 0xFF,  /* skip 32 bit pkt_type */
		0xFF, 0xFF);
	/*
	 * Compile-time verify the shuffle mask
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

	mbuf_init = _mm_set_epi64x(0, rxq->mbuf_initializer);

	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache
	 */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

	/* ensure these 2 flags are in the lower 8 bits */
	RTE_BUILD_BUG_ON((RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED) > UINT8_MAX);
	vlan_flags = rxq->vlan_flags & UINT8_MAX;

	/* A. load 4 packet in one loop
	 * [A*. mask out 4 unused dirty field in desc]
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */
	for (pos = 0, nb_pkts_recd = 0; pos < nb_pkts;
			pos += RTE_NGBE_DESCS_PER_LOOP,
			rxdp += RTE_NGBE_DESCS_PER_LOOP) {
		__m128i descs[RTE_NGBE_DESCS_PER_LOOP];
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
		/* A.1 load desc[3] */
		descs[3] = _mm_loadu_si128((__m128i *)(uintptr_t)(rxdp + 3));
		rte_compiler_barrier();

		/* B.2 copy 2 64 bit or 4 32 bit mbuf point into rx_pkts */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

#if defined(RTE_ARCH_X86_64)
		/* B.1 load 2 64 bit mbuf points */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos + 2]);
#endif

		/* A.1 load desc[2-0] */
		descs[2] = _mm_loadu_si128((__m128i *)(uintptr_t)(rxdp + 2));
		rte_compiler_barrier();
		descs[1] = _mm_loadu_si128((__m128i *)(uintptr_t)(rxdp + 1));
		rte_compiler_barrier();
		descs[0] = _mm_loadu_si128((__m128i *)(uintptr_t)(rxdp));

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

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs[2], shuf_msk);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = _mm_shuffle_epi8(descs[1], shuf_msk);
		pkt_mb1 = _mm_shuffle_epi8(descs[0], shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

		/* set ol_flags with vlan packet type */
		desc_to_olflags_v(descs, mbuf_init, vlan_flags, &rx_pkts[pos]);

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb4 = _mm_add_epi16(pkt_mb4, crc_adjust);
		pkt_mb3 = _mm_add_epi16(pkt_mb3, crc_adjust);

		/* C.2 get 4 pkts staterr value  */
		zero = _mm_xor_si128(dd_check, dd_check);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos + 3]->rx_descriptor_fields1,
				pkt_mb4);
		_mm_storeu_si128((void *)&rx_pkts[pos + 2]->rx_descriptor_fields1,
				pkt_mb3);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		pkt_mb2 = _mm_add_epi16(pkt_mb2, crc_adjust);
		pkt_mb1 = _mm_add_epi16(pkt_mb1, crc_adjust);

		/* C* extract and record EOP bit */
		if (split_packet) {
			__m128i eop_shuf_mask =
				_mm_set_epi8(0xFF, 0xFF, 0xFF, 0xFF,
					     0xFF, 0xFF, 0xFF, 0xFF,
					     0xFF, 0xFF, 0xFF, 0xFF,
					     0x04, 0x0C, 0x00, 0x08);

			/* and with mask to extract bits, flipping 1-0 */
			__m128i eop_bits = _mm_andnot_si128(staterr, eop_check);
			/* the staterr values are not in order, as the count
			 * of dd bits doesn't care. However, for end of
			 * packet tracking, we do care, so shuffle. This also
			 * compresses the 32-bit values to 8-bit
			 */
			eop_bits = _mm_shuffle_epi8(eop_bits, eop_shuf_mask);
			/* store the resulting 32-bit value */
			*(int *)split_packet = _mm_cvtsi128_si32(eop_bits);
			split_packet += RTE_NGBE_DESCS_PER_LOOP;
		}

		/* C.3 calc available number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos + 1]->rx_descriptor_fields1,
				pkt_mb2);
		_mm_storeu_si128((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				pkt_mb1);

		desc_to_ptype_v(descs, NGBE_PTID_MASK, &rx_pkts[pos]);

		/* C.4 calc available number of desc */
		var = rte_popcount64(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != RTE_NGBE_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/*
 * vPMD receive routine, only accept(nb_pkts >= RTE_NGBE_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < RTE_NGBE_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a RTE_NGBE_DESC_PER_LOOP power-of-two
 */
uint16_t
ngbe_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

/**
 * vPMD receive routine that reassembles scattered packets
 *
 * Notice:
 * - nb_pkts < RTE_NGBE_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a RTE_NGBE_DESC_PER_LOOP power-of-two
 */
static uint16_t
ngbe_recv_scattered_burst_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	struct ngbe_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_NGBE_MAX_RX_BURST] = {0};

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
	unsigned int i = 0;
	if (rxq->pkt_first_seg == NULL) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
		rxq->pkt_first_seg = rx_pkts[i];
	}
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
		&split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 */
uint16_t
ngbe_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > RTE_NGBE_MAX_RX_BURST) {
		uint16_t burst;

		burst = ngbe_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      RTE_NGBE_MAX_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < RTE_NGBE_MAX_RX_BURST)
			return retval;
	}

	return retval + ngbe_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      nb_pkts);
}

static inline void
vtx1(volatile struct ngbe_tx_desc *txdp,
		struct rte_mbuf *pkt, uint64_t flags)
{
	uint16_t pkt_len = pkt->data_len;
	__m128i descriptor;

	if (pkt_len < RTE_ETHER_HDR_LEN)
		pkt_len = NGBE_FRAME_SIZE_DFT;

	descriptor = _mm_set_epi64x((uint64_t)pkt_len << 45 | flags | pkt_len,
				pkt->buf_iova + pkt->data_off);
	_mm_store_si128((__m128i *)(uintptr_t)txdp, descriptor);
}

static inline void
vtx(volatile struct ngbe_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

uint16_t
ngbe_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct ngbe_tx_queue *txq = (struct ngbe_tx_queue *)tx_queue;
	volatile struct ngbe_tx_desc *txdp;
	struct ngbe_tx_entry_v *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = NGBE_TXD_FLAGS;
	uint64_t rs = NGBE_TXD_FLAGS;
	int i;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_free_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ngbe_tx_free_bufs(txq);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring_v[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	nb_commit = nb_pkts;
	if (nb_commit >= n) {
		tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring_v[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);

	txq->tx_tail = tx_id;

	ngbe_set32(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

static void __rte_cold
ngbe_tx_queue_release_mbufs_vec(struct ngbe_tx_queue *txq)
{
	_ngbe_tx_queue_release_mbufs_vec(txq);
}

void __rte_cold
ngbe_rx_queue_release_mbufs_vec(struct ngbe_rx_queue *rxq)
{
	_ngbe_rx_queue_release_mbufs_vec(rxq);
}

static void __rte_cold
ngbe_tx_free_swring(struct ngbe_tx_queue *txq)
{
	_ngbe_tx_free_swring_vec(txq);
}

static void __rte_cold
ngbe_reset_tx_queue(struct ngbe_tx_queue *txq)
{
	_ngbe_reset_tx_queue_vec(txq);
}

static const struct ngbe_txq_ops vec_txq_ops = {
	.release_mbufs = ngbe_tx_queue_release_mbufs_vec,
	.free_swring = ngbe_tx_free_swring,
	.reset = ngbe_reset_tx_queue,
};

int __rte_cold
ngbe_rxq_vec_setup(struct ngbe_rx_queue *rxq)
{
	return ngbe_rxq_vec_setup_default(rxq);
}

int __rte_cold
ngbe_txq_vec_setup(struct ngbe_tx_queue *txq)
{
	return ngbe_txq_vec_setup_default(txq, &vec_txq_ops);
}

int __rte_cold
ngbe_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev)
{
	return ngbe_rx_vec_dev_conf_condition_check_default(dev);
}
