/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include "ice_rxtx_vec_common.h"

#include <tmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline __m128i
ice_flex_rxd_to_fdir_flags_vec(const __m128i fdir_id0_3)
{
#define FDID_MIS_MAGIC 0xFFFFFFFF
	RTE_BUILD_BUG_ON(PKT_RX_FDIR != (1 << 2));
	RTE_BUILD_BUG_ON(PKT_RX_FDIR_ID != (1 << 13));
	const __m128i pkt_fdir_bit = _mm_set1_epi32(PKT_RX_FDIR |
			PKT_RX_FDIR_ID);
	/* desc->flow_id field == 0xFFFFFFFF means fdir mismatch */
	const __m128i fdir_mis_mask = _mm_set1_epi32(FDID_MIS_MAGIC);
	__m128i fdir_mask = _mm_cmpeq_epi32(fdir_id0_3,
			fdir_mis_mask);
	/* this XOR op results to bit-reverse the fdir_mask */
	fdir_mask = _mm_xor_si128(fdir_mask, fdir_mis_mask);
	const __m128i fdir_flags = _mm_and_si128(fdir_mask, pkt_fdir_bit);

	return fdir_flags;
}

static inline void
ice_rxq_rearm(struct ice_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union ice_rx_flex_desc *rxdp;
	struct ice_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
					  RTE_PKTMBUF_HEADROOM);
	__m128i dma_addr0, dma_addr1;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxep,
				 ICE_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + ICE_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < ICE_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			ICE_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < ICE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
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

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr1);
	}

	rxq->rxrearm_start += ICE_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= ICE_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			   (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	ICE_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
}

static inline void
ice_rx_desc_to_olflags_v(struct ice_rx_queue *rxq, __m128i descs[4],
			 struct rte_mbuf **rx_pkts)
{
	const __m128i mbuf_init = _mm_set_epi64x(0, rxq->mbuf_initializer);
	__m128i rearm0, rearm1, rearm2, rearm3;

	__m128i tmp_desc, flags, rss_vlan;

	/* mask everything except checksum, RSS and VLAN flags.
	 * bit6:4 for checksum.
	 * bit12 for RSS indication.
	 * bit13 for VLAN indication.
	 */
	const __m128i desc_mask = _mm_set_epi32(0x30f0, 0x30f0,
						0x30f0, 0x30f0);
	const __m128i cksum_mask = _mm_set_epi32(PKT_RX_IP_CKSUM_MASK |
						 PKT_RX_L4_CKSUM_MASK |
						 PKT_RX_OUTER_L4_CKSUM_MASK |
						 PKT_RX_EIP_CKSUM_BAD,
						 PKT_RX_IP_CKSUM_MASK |
						 PKT_RX_L4_CKSUM_MASK |
						 PKT_RX_OUTER_L4_CKSUM_MASK |
						 PKT_RX_EIP_CKSUM_BAD,
						 PKT_RX_IP_CKSUM_MASK |
						 PKT_RX_L4_CKSUM_MASK |
						 PKT_RX_OUTER_L4_CKSUM_MASK |
						 PKT_RX_EIP_CKSUM_BAD,
						 PKT_RX_IP_CKSUM_MASK |
						 PKT_RX_L4_CKSUM_MASK |
						 PKT_RX_OUTER_L4_CKSUM_MASK |
						 PKT_RX_EIP_CKSUM_BAD);

	/* map the checksum, rss and vlan fields to the checksum, rss
	 * and vlan flag
	 */
	const __m128i cksum_flags =
		_mm_set_epi8((PKT_RX_OUTER_L4_CKSUM_BAD >> 20 |
		 PKT_RX_EIP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD |
		  PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_GOOD | PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_L4_CKSUM_BAD |
		 PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_L4_CKSUM_BAD |
		 PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_L4_CKSUM_GOOD |
		 PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_BAD >> 20 | PKT_RX_L4_CKSUM_GOOD |
		 PKT_RX_IP_CKSUM_GOOD) >> 1,
		/**
		 * shift right 20 bits to use the low two bits to indicate
		 * outer checksum status
		 * shift right 1 bit to make sure it not exceed 255
		 */
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_GOOD | PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_EIP_CKSUM_BAD |
		 PKT_RX_L4_CKSUM_GOOD | PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_L4_CKSUM_BAD |
		 PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_L4_CKSUM_BAD |
		 PKT_RX_IP_CKSUM_GOOD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_L4_CKSUM_GOOD |
		 PKT_RX_IP_CKSUM_BAD) >> 1,
		(PKT_RX_OUTER_L4_CKSUM_GOOD >> 20 | PKT_RX_L4_CKSUM_GOOD |
		 PKT_RX_IP_CKSUM_GOOD) >> 1);

	const __m128i rss_vlan_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			PKT_RX_RSS_HASH | PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
			PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
			PKT_RX_RSS_HASH, 0);

	/* merge 4 descriptors */
	flags = _mm_unpackhi_epi32(descs[0], descs[1]);
	tmp_desc = _mm_unpackhi_epi32(descs[2], descs[3]);
	tmp_desc = _mm_unpacklo_epi64(flags, tmp_desc);
	tmp_desc = _mm_and_si128(tmp_desc, desc_mask);

	/* checksum flags */
	tmp_desc = _mm_srli_epi32(tmp_desc, 4);
	flags = _mm_shuffle_epi8(cksum_flags, tmp_desc);
	/* then we shift left 1 bit */
	flags = _mm_slli_epi32(flags, 1);

	__m128i l4_outer_mask = _mm_set_epi32(0x6, 0x6, 0x6, 0x6);
	__m128i l4_outer_flags = _mm_and_si128(flags, l4_outer_mask);
	l4_outer_flags = _mm_slli_epi32(l4_outer_flags, 20);

	__m128i l3_l4_mask = _mm_set_epi32(~0x6, ~0x6, ~0x6, ~0x6);
	__m128i l3_l4_flags = _mm_and_si128(flags, l3_l4_mask);
	flags = _mm_or_si128(l3_l4_flags, l4_outer_flags);
	/* we need to mask out the reduntant bits introduced by RSS or
	 * VLAN fields.
	 */
	flags = _mm_and_si128(flags, cksum_mask);

	/* RSS, VLAN flag */
	tmp_desc = _mm_srli_epi32(tmp_desc, 8);
	rss_vlan = _mm_shuffle_epi8(rss_vlan_flags, tmp_desc);

	/* merge the flags */
	flags = _mm_or_si128(flags, rss_vlan);

	if (rxq->fdir_enabled) {
		const __m128i fdir_id0_1 =
			_mm_unpackhi_epi32(descs[0], descs[1]);

		const __m128i fdir_id2_3 =
			_mm_unpackhi_epi32(descs[2], descs[3]);

		const __m128i fdir_id0_3 =
			_mm_unpackhi_epi64(fdir_id0_1, fdir_id2_3);

		const __m128i fdir_flags =
			ice_flex_rxd_to_fdir_flags_vec(fdir_id0_3);

		/* merge with fdir_flags */
		flags = _mm_or_si128(flags, fdir_flags);

		/* write fdir_id to mbuf */
		rx_pkts[0]->hash.fdir.hi =
			_mm_extract_epi32(fdir_id0_3, 0);

		rx_pkts[1]->hash.fdir.hi =
			_mm_extract_epi32(fdir_id0_3, 1);

		rx_pkts[2]->hash.fdir.hi =
			_mm_extract_epi32(fdir_id0_3, 2);

		rx_pkts[3]->hash.fdir.hi =
			_mm_extract_epi32(fdir_id0_3, 3);
	} /* if() on fdir_enabled */

	/**
	 * At this point, we have the 4 sets of flags in the low 16-bits
	 * of each 32-bit value in flags.
	 * We want to extract these, and merge them with the mbuf init data
	 * so we can do a single 16-byte write to the mbuf to set the flags
	 * and all the other initialization fields. Extracting the
	 * appropriate flags means that we have to do a shift and blend for
	 * each mbuf before we do the write.
	 */
	rearm0 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(flags, 8), 0x30);
	rearm1 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(flags, 4), 0x30);
	rearm2 = _mm_blend_epi16(mbuf_init, flags, 0x30);
	rearm3 = _mm_blend_epi16(mbuf_init, _mm_srli_si128(flags, 4), 0x30);

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
ice_rx_desc_to_ptype_v(__m128i descs[4], struct rte_mbuf **rx_pkts,
		       uint32_t *ptype_tbl)
{
	const __m128i ptype_mask = _mm_set_epi16(ICE_RX_FLEX_DESC_PTYPE_M, 0,
						 ICE_RX_FLEX_DESC_PTYPE_M, 0,
						 ICE_RX_FLEX_DESC_PTYPE_M, 0,
						 ICE_RX_FLEX_DESC_PTYPE_M, 0);
	__m128i ptype_01 = _mm_unpacklo_epi32(descs[0], descs[1]);
	__m128i ptype_23 = _mm_unpacklo_epi32(descs[2], descs[3]);
	__m128i ptype_all = _mm_unpacklo_epi64(ptype_01, ptype_23);

	ptype_all = _mm_and_si128(ptype_all, ptype_mask);

	rx_pkts[0]->packet_type = ptype_tbl[_mm_extract_epi16(ptype_all, 1)];
	rx_pkts[1]->packet_type = ptype_tbl[_mm_extract_epi16(ptype_all, 3)];
	rx_pkts[2]->packet_type = ptype_tbl[_mm_extract_epi16(ptype_all, 5)];
	rx_pkts[3]->packet_type = ptype_tbl[_mm_extract_epi16(ptype_all, 7)];
}

/**
 * vPMD raw receive routine, only accept(nb_pkts >= ICE_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a ICE_DESCS_PER_LOOP power-of-two
 */
static inline uint16_t
_ice_recv_raw_pkts_vec(struct ice_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union ice_rx_flex_desc *rxdp;
	struct ice_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;
	__m128i crc_adjust = _mm_set_epi16
				(0, 0, 0,       /* ignore non-length fields */
				 -rxq->crc_len, /* sub crc on data_len */
				 0,          /* ignore high-16bits of pkt_len */
				 -rxq->crc_len, /* sub crc on pkt_len */
				 0, 0           /* ignore pkt_type field */
				);
	const __m128i zero = _mm_setzero_si128();
	/* mask to shuffle from desc. to mbuf */
	const __m128i shuf_msk = _mm_set_epi8
			(0xFF, 0xFF,
			 0xFF, 0xFF,  /* rss hash parsed separately */
			 11, 10,      /* octet 10~11, 16 bits vlan_macip */
			 5, 4,        /* octet 4~5, 16 bits data_len */
			 0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
			 5, 4,        /* octet 4~5, low 16 bits pkt_len */
			 0xFF, 0xFF,  /* pkt_type set as unknown */
			 0xFF, 0xFF   /* pkt_type set as unknown */
			);
	const __m128i eop_shuf_mask = _mm_set_epi8(0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0x04, 0x0C,
						   0x00, 0x08);

	/**
	 * compile-time check the above crc_adjust layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi16
	 * call above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);

	/* 4 packets DD mask */
	const __m128i dd_check = _mm_set_epi64x(0x0000000100000001LL,
						0x0000000100000001LL);
	/* 4 packets EOP mask */
	const __m128i eop_check = _mm_set_epi64x(0x0000000200000002LL,
						 0x0000000200000002LL);

	/* nb_pkts has to be floor-aligned to ICE_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, ICE_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > ICE_RXQ_REARM_THRESH)
		ice_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.status_error0 &
	      rte_cpu_to_le_32(1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

	/**
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
	     pos += ICE_DESCS_PER_LOOP,
	     rxdp += ICE_DESCS_PER_LOOP) {
		__m128i descs[ICE_DESCS_PER_LOOP];
		__m128i pkt_mb0, pkt_mb1, pkt_mb2, pkt_mb3;
		__m128i staterr, sterr_tmp1, sterr_tmp2;
		/* 2 64 bit or 4 32 bit mbuf pointers in one XMM reg. */
		__m128i mbp1;
#if defined(RTE_ARCH_X86_64)
		__m128i mbp2;
#endif

		/* B.1 load 2 (64 bit) or 4 (32 bit) mbuf points */
		mbp1 = _mm_loadu_si128((__m128i *)&sw_ring[pos]);
		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load desc[3] */
		descs[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));
		rte_compiler_barrier();

		/* B.2 copy 2 64 bit or 4 32 bit mbuf point into rx_pkts */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

#if defined(RTE_ARCH_X86_64)
		/* B.1 load 2 64 bit mbuf points */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos + 2]);
#endif

		/* A.1 load desc[2-0] */
		descs[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		rte_compiler_barrier();
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

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb3 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb2 = _mm_shuffle_epi8(descs[2], shuf_msk);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb1 = _mm_shuffle_epi8(descs[1], shuf_msk);
		pkt_mb0 = _mm_shuffle_epi8(descs[0], shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

		ice_rx_desc_to_olflags_v(rxq, descs, &rx_pkts[pos]);

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb3 = _mm_add_epi16(pkt_mb3, crc_adjust);
		pkt_mb2 = _mm_add_epi16(pkt_mb2, crc_adjust);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		pkt_mb1 = _mm_add_epi16(pkt_mb1, crc_adjust);
		pkt_mb0 = _mm_add_epi16(pkt_mb0, crc_adjust);

#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
		/**
		 * needs to load 2nd 16B of each desc for RSS hash parsing,
		 * will cause performance drop to get into this context.
		 */
		if (rxq->vsi->adapter->pf.dev_data->dev_conf.rxmode.offloads &
				DEV_RX_OFFLOAD_RSS_HASH) {
			/* load bottom half of every 32B desc */
			const __m128i raw_desc_bh3 =
				_mm_load_si128
					((void *)(&rxdp[3].wb.status_error1));
			rte_compiler_barrier();
			const __m128i raw_desc_bh2 =
				_mm_load_si128
					((void *)(&rxdp[2].wb.status_error1));
			rte_compiler_barrier();
			const __m128i raw_desc_bh1 =
				_mm_load_si128
					((void *)(&rxdp[1].wb.status_error1));
			rte_compiler_barrier();
			const __m128i raw_desc_bh0 =
				_mm_load_si128
					((void *)(&rxdp[0].wb.status_error1));

			/**
			 * to shift the 32b RSS hash value to the
			 * highest 32b of each 128b before mask
			 */
			__m128i rss_hash3 =
				_mm_slli_epi64(raw_desc_bh3, 32);
			__m128i rss_hash2 =
				_mm_slli_epi64(raw_desc_bh2, 32);
			__m128i rss_hash1 =
				_mm_slli_epi64(raw_desc_bh1, 32);
			__m128i rss_hash0 =
				_mm_slli_epi64(raw_desc_bh0, 32);

			__m128i rss_hash_msk =
				_mm_set_epi32(0xFFFFFFFF, 0, 0, 0);

			rss_hash3 = _mm_and_si128
					(rss_hash3, rss_hash_msk);
			rss_hash2 = _mm_and_si128
					(rss_hash2, rss_hash_msk);
			rss_hash1 = _mm_and_si128
					(rss_hash1, rss_hash_msk);
			rss_hash0 = _mm_and_si128
					(rss_hash0, rss_hash_msk);

			pkt_mb3 = _mm_or_si128(pkt_mb3, rss_hash3);
			pkt_mb2 = _mm_or_si128(pkt_mb2, rss_hash2);
			pkt_mb1 = _mm_or_si128(pkt_mb1, rss_hash1);
			pkt_mb0 = _mm_or_si128(pkt_mb0, rss_hash0);
		} /* if() on RSS hash parsing */
#endif

		/* C.2 get 4 pkts staterr value  */
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128
			((void *)&rx_pkts[pos + 3]->rx_descriptor_fields1,
			 pkt_mb3);
		_mm_storeu_si128
			((void *)&rx_pkts[pos + 2]->rx_descriptor_fields1,
			 pkt_mb2);

		/* C* extract and record EOP bit */
		if (split_packet) {
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
			split_packet += ICE_DESCS_PER_LOOP;
		}

		/* C.3 calc available number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128
			((void *)&rx_pkts[pos + 1]->rx_descriptor_fields1,
			 pkt_mb1);
		_mm_storeu_si128((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				 pkt_mb0);
		ice_rx_desc_to_ptype_v(descs, &rx_pkts[pos], ptype_tbl);
		/* C.4 calc avaialbe number of desc */
		var = __builtin_popcountll(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != ICE_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/**
 * Notice:
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > ICE_VPMD_RX_BURST, only scan ICE_VPMD_RX_BURST
 *   numbers of DD bits
 */
uint16_t
ice_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		  uint16_t nb_pkts)
{
	return _ice_recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 *
 * Notice:
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 */
static uint16_t
ice_recv_scattered_burst_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	struct ice_rx_queue *rxq = rx_queue;
	uint8_t split_flags[ICE_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _ice_recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts,
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
	unsigned int i = 0;

	if (!rxq->pkt_first_seg) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
		rxq->pkt_first_seg = rx_pkts[i];
	}
	return i + ice_rx_reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
					     &split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 */
uint16_t
ice_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > ICE_VPMD_RX_BURST) {
		uint16_t burst;

		burst = ice_recv_scattered_burst_vec(rx_queue,
						     rx_pkts + retval,
						     ICE_VPMD_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < ICE_VPMD_RX_BURST)
			return retval;
	}

	return retval + ice_recv_scattered_burst_vec(rx_queue,
						     rx_pkts + retval,
						     nb_pkts);
}

static inline void
ice_vtx1(volatile struct ice_tx_desc *txdp, struct rte_mbuf *pkt,
	 uint64_t flags)
{
	uint64_t high_qw =
		(ICE_TX_DESC_DTYPE_DATA |
		 ((uint64_t)flags  << ICE_TXD_QW1_CMD_S) |
		 ((uint64_t)pkt->data_len << ICE_TXD_QW1_TX_BUF_SZ_S));

	__m128i descriptor = _mm_set_epi64x(high_qw,
					    pkt->buf_iova + pkt->data_off);
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
ice_vtx(volatile struct ice_tx_desc *txdp, struct rte_mbuf **pkt,
	uint16_t nb_pkts, uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		ice_vtx1(txdp, *pkt, flags);
}

static uint16_t
ice_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	struct ice_tx_queue *txq = (struct ice_tx_queue *)tx_queue;
	volatile struct ice_tx_desc *txdp;
	struct ice_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = ICE_TD_CMD;
	uint64_t rs = ICE_TX_DESC_CMD_RS | ICE_TD_CMD;
	int i;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_rs_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ice_tx_free_bufs(txq);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	nb_commit = nb_pkts;
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		ice_tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			ice_vtx1(txdp, *tx_pkts, flags);

		ice_vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	ice_tx_backlog_entry(txep, tx_pkts, nb_commit);

	ice_vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)ICE_TX_DESC_CMD_RS) <<
					 ICE_TXD_QW1_CMD_S);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	ICE_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

uint16_t
ice_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		  uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct ice_tx_queue *txq = (struct ice_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = ice_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx], num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

int __rte_cold
ice_rxq_vec_setup(struct ice_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	rxq->rx_rel_mbufs = _ice_rx_queue_release_mbufs_vec;
	return ice_rxq_vec_setup_default(rxq);
}

int __rte_cold
ice_txq_vec_setup(struct ice_tx_queue __rte_unused *txq)
{
	if (!txq)
		return -1;

	txq->tx_rel_mbufs = _ice_tx_queue_release_mbufs_vec;
	return 0;
}

int __rte_cold
ice_rx_vec_dev_check(struct rte_eth_dev *dev)
{
	return ice_rx_vec_dev_check_default(dev);
}

int __rte_cold
ice_tx_vec_dev_check(struct rte_eth_dev *dev)
{
	return ice_tx_vec_dev_check_default(dev);
}
