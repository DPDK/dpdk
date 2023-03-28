/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_rxtx_vec_common.h"

#include <tmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline void
i40e_rxq_rearm(struct i40e_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	__m128i dma_addr0, dma_addr1;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxep,
				 RTE_I40E_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + RTE_I40E_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < RTE_I40E_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_I40E_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_I40E_RXQ_REARM_THRESH; i += 2, rxep += 2) {
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

	rxq->rxrearm_start += RTE_I40E_RXQ_REARM_THRESH;
	rx_id = rxq->rxrearm_start - 1;

	if (unlikely(rxq->rxrearm_start >= rxq->nb_rx_desc)) {
		rxq->rxrearm_start = 0;
		rx_id = rxq->nb_rx_desc - 1;
	}

	rxq->rxrearm_nb -= RTE_I40E_RXQ_REARM_THRESH;

	/* Update the tail pointer on the NIC */
	I40E_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
}

#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
/* SSE version of FDIR mark extraction for 4 32B descriptors at a time */
static inline __m128i
descs_to_fdir_32b(volatile union i40e_rx_desc *rxdp, struct rte_mbuf **rx_pkt)
{
	/* 32B descriptors: Load 2nd half of descriptors for FDIR ID data */
	__m128i desc0_qw23, desc1_qw23, desc2_qw23, desc3_qw23;
	desc0_qw23 = _mm_loadu_si128((__m128i *)&(rxdp + 0)->wb.qword2);
	desc1_qw23 = _mm_loadu_si128((__m128i *)&(rxdp + 1)->wb.qword2);
	desc2_qw23 = _mm_loadu_si128((__m128i *)&(rxdp + 2)->wb.qword2);
	desc3_qw23 = _mm_loadu_si128((__m128i *)&(rxdp + 3)->wb.qword2);

	/* FDIR ID data: move last u32 of each desc to 4 u32 lanes */
	__m128i v_unpack_01, v_unpack_23;
	v_unpack_01 = _mm_unpackhi_epi32(desc0_qw23, desc1_qw23);
	v_unpack_23 = _mm_unpackhi_epi32(desc2_qw23, desc3_qw23);
	__m128i v_fdir_ids = _mm_unpackhi_epi64(v_unpack_01, v_unpack_23);

	/* Extended Status: extract from each lower 32 bits, to u32 lanes */
	v_unpack_01 = _mm_unpacklo_epi32(desc0_qw23, desc1_qw23);
	v_unpack_23 = _mm_unpacklo_epi32(desc2_qw23, desc3_qw23);
	__m128i v_flt_status = _mm_unpacklo_epi64(v_unpack_01, v_unpack_23);

	/* Shift u32 left and right to "mask away" bits not required.
	 * Data required is 4:5 (zero based), so left shift by 26 (32-6)
	 * and then right shift by 30 (32 - 2 bits required).
	 */
	v_flt_status = _mm_slli_epi32(v_flt_status, 26);
	v_flt_status = _mm_srli_epi32(v_flt_status, 30);

	/* Generate constant 1 in all u32 lanes and compare */
	RTE_BUILD_BUG_ON(I40E_RX_DESC_EXT_STATUS_FLEXBH_FD_ID != 1);
	__m128i v_zeros = _mm_setzero_si128();
	__m128i v_ffff = _mm_cmpeq_epi32(v_zeros, v_zeros);
	__m128i v_u32_one = _mm_srli_epi32(v_ffff, 31);

	/* per desc mask, bits set if FDIR ID is valid */
	__m128i v_fd_id_mask = _mm_cmpeq_epi32(v_flt_status, v_u32_one);

	/* Mask ID data to zero if the FD_ID bit not set in desc */
	v_fdir_ids = _mm_and_si128(v_fdir_ids, v_fd_id_mask);

	/* Extract and store as u32. No advantage to combining into SSE
	 * stores, there are no surrounding stores to around fdir.hi
	 */
	rx_pkt[0]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 0);
	rx_pkt[1]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 1);
	rx_pkt[2]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 2);
	rx_pkt[3]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 3);

	/* convert fdir_id_mask into a single bit, then shift as required for
	 * correct location in the mbuf->olflags
	 */
	const uint32_t FDIR_ID_BIT_SHIFT = 13;
	RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR_ID != (1 << FDIR_ID_BIT_SHIFT));
	v_fd_id_mask = _mm_srli_epi32(v_fd_id_mask, 31);
	v_fd_id_mask = _mm_slli_epi32(v_fd_id_mask, FDIR_ID_BIT_SHIFT);

	/* The returned value must be combined into each mbuf. This is already
	 * being done for RSS and VLAN mbuf olflags, so return bits to OR in.
	 */
	return v_fd_id_mask;
}

#else /* 32 or 16B FDIR ID handling */

/* Handle 16B descriptor FDIR ID flag setting based on FLM. See scalar driver
 * for scalar implementation of the same functionality.
 */
static inline __m128i
descs_to_fdir_16b(__m128i fltstat, __m128i descs[4], struct rte_mbuf **rx_pkt)
{
	/* unpack filter-status data from descriptors */
	__m128i v_tmp_01 = _mm_unpacklo_epi32(descs[0], descs[1]);
	__m128i v_tmp_23 = _mm_unpacklo_epi32(descs[2], descs[3]);
	__m128i v_fdir_ids = _mm_unpackhi_epi64(v_tmp_01, v_tmp_23);

	/* Generate one bit in each u32 lane */
	__m128i v_zeros = _mm_setzero_si128();
	__m128i v_ffff = _mm_cmpeq_epi32(v_zeros, v_zeros);
	__m128i v_111_mask = _mm_srli_epi32(v_ffff, 29);
	__m128i v_11_mask = _mm_srli_epi32(v_ffff, 30);

	/* Top lane ones mask for FDIR isolation */
	__m128i v_desc_fdir_mask = _mm_insert_epi32(v_zeros, UINT32_MAX, 1);

	/* Compare and mask away FDIR ID data if bit not set */
	__m128i v_u32_bits = _mm_and_si128(v_111_mask, fltstat);
	__m128i v_fdir_id_mask = _mm_cmpeq_epi32(v_u32_bits, v_11_mask);
	v_fdir_ids = _mm_and_si128(v_fdir_id_mask, v_fdir_ids);

	/* Store data to fdir.hi in mbuf */
	rx_pkt[0]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 0);
	rx_pkt[1]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 1);
	rx_pkt[2]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 2);
	rx_pkt[3]->hash.fdir.hi = _mm_extract_epi32(v_fdir_ids, 3);

	/* Move fdir_id_mask to correct lane, blend RSS to zero on hits */
	__m128i v_desc3_shift = _mm_alignr_epi8(v_zeros, v_fdir_id_mask, 8);
	__m128i v_desc3_mask = _mm_and_si128(v_desc_fdir_mask, v_desc3_shift);
	descs[3] = _mm_blendv_epi8(descs[3], _mm_setzero_si128(), v_desc3_mask);

	__m128i v_desc2_shift = _mm_alignr_epi8(v_zeros, v_fdir_id_mask, 4);
	__m128i v_desc2_mask = _mm_and_si128(v_desc_fdir_mask, v_desc2_shift);
	descs[2] = _mm_blendv_epi8(descs[2], _mm_setzero_si128(), v_desc2_mask);

	__m128i v_desc1_shift = v_fdir_id_mask;
	__m128i v_desc1_mask = _mm_and_si128(v_desc_fdir_mask, v_desc1_shift);
	descs[1] = _mm_blendv_epi8(descs[1], _mm_setzero_si128(), v_desc1_mask);

	__m128i v_desc0_shift = _mm_alignr_epi8(v_fdir_id_mask, v_zeros, 12);
	__m128i v_desc0_mask = _mm_and_si128(v_desc_fdir_mask, v_desc0_shift);
	descs[0] = _mm_blendv_epi8(descs[0], _mm_setzero_si128(), v_desc0_mask);

	/* Shift to 1 or 0 bit per u32 lane, then to RTE_MBUF_F_RX_FDIR_ID offset */
	const uint32_t FDIR_ID_BIT_SHIFT = 13;
	RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR_ID != (1 << FDIR_ID_BIT_SHIFT));
	__m128i v_mask_one_bit = _mm_srli_epi32(v_fdir_id_mask, 31);
	return _mm_slli_epi32(v_mask_one_bit, FDIR_ID_BIT_SHIFT);
}
#endif

static inline void
desc_to_olflags_v(struct i40e_rx_queue *rxq, volatile union i40e_rx_desc *rxdp,
		  __m128i descs[4], struct rte_mbuf **rx_pkts)
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
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
			RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
			RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
			RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
			RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD);

	/* map rss and vlan type to rss hash and vlan flag */
	const __m128i vlan_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			0, 0, 0, 0);

	const __m128i rss_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_RSS_HASH, 0, 0,
			0, 0, RTE_MBUF_F_RX_FDIR, 0);

	const __m128i l3_l4e_flags = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
			/* shift right 1 bit to make sure it not exceed 255 */
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
			 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
			 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
			 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
			 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_BAD  | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_BAD  | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1);

	/* Unpack "status" from quadword 1, bits 0:32 */
	vlan0 = _mm_unpackhi_epi32(descs[0], descs[1]);
	vlan1 = _mm_unpackhi_epi32(descs[2], descs[3]);
	vlan0 = _mm_unpacklo_epi64(vlan0, vlan1);

	vlan1 = _mm_and_si128(vlan0, rss_vlan_msk);
	vlan0 = _mm_shuffle_epi8(vlan_flags, vlan1);

	const __m128i desc_fltstat = _mm_srli_epi32(vlan1, 11);
	rss = _mm_shuffle_epi8(rss_flags, desc_fltstat);

	l3_l4e = _mm_srli_epi32(vlan1, 22);
	l3_l4e = _mm_shuffle_epi8(l3_l4e_flags, l3_l4e);
	/* then we shift left 1 bit */
	l3_l4e = _mm_slli_epi32(l3_l4e, 1);
	/* we need to mask out the redundant bits */
	l3_l4e = _mm_and_si128(l3_l4e, cksum_mask);

	vlan0 = _mm_or_si128(vlan0, rss);
	vlan0 = _mm_or_si128(vlan0, l3_l4e);

	/* Extract FDIR ID only if FDIR is enabled to avoid useless work */
	if (rxq->fdir_enabled) {
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
		__m128i v_fdir_ol_flags = descs_to_fdir_32b(rxdp, rx_pkts);
#else
		(void)rxdp; /* rxdp not required for 16B desc mode */
		__m128i v_fdir_ol_flags = descs_to_fdir_16b(desc_fltstat,
							    descs, rx_pkts);
#endif
		/* OR in ol_flag bits after descriptor specific extraction */
		vlan0 = _mm_or_si128(vlan0, v_fdir_ol_flags);
	}

	/*
	 * At this point, we have the 4 sets of flags in the low 16-bits
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
desc_to_ptype_v(__m128i descs[4], struct rte_mbuf **rx_pkts,
		uint32_t *ptype_tbl)
{
	__m128i ptype0 = _mm_unpackhi_epi64(descs[0], descs[1]);
	__m128i ptype1 = _mm_unpackhi_epi64(descs[2], descs[3]);

	ptype0 = _mm_srli_epi64(ptype0, 30);
	ptype1 = _mm_srli_epi64(ptype1, 30);

	rx_pkts[0]->packet_type = ptype_tbl[_mm_extract_epi8(ptype0, 0)];
	rx_pkts[1]->packet_type = ptype_tbl[_mm_extract_epi8(ptype0, 8)];
	rx_pkts[2]->packet_type = ptype_tbl[_mm_extract_epi8(ptype1, 0)];
	rx_pkts[3]->packet_type = ptype_tbl[_mm_extract_epi8(ptype1, 8)];
}

/**
 * vPMD raw receive routine, only accept(nb_pkts >= RTE_I40E_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a RTE_I40E_DESCS_PER_LOOP power-of-two
 */
static inline uint16_t
_recv_raw_pkts_vec(struct i40e_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__m128i shuf_msk;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	__m128i crc_adjust = _mm_set_epi16(
				0, 0, 0,    /* ignore non-length fields */
				-rxq->crc_len, /* sub crc on data_len */
				0,          /* ignore high-16bits of pkt_len */
				-rxq->crc_len, /* sub crc on pkt_len */
				0, 0            /* ignore pkt_type field */
			);
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

	/* nb_pkts has to be floor-aligned to RTE_I40E_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_I40E_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_I40E_RXQ_REARM_THRESH)
		i40e_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
			rte_cpu_to_le_32(1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
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
		0xFF, 0xFF,  /* pkt_type set as unknown */
		0xFF, 0xFF  /*pkt_type set as unknown */
		);
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
			pos += RTE_I40E_DESCS_PER_LOOP,
			rxdp += RTE_I40E_DESCS_PER_LOOP) {
		__m128i descs[RTE_I40E_DESCS_PER_LOOP];
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
		descs[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));
		rte_compiler_barrier();

		/* B.2 copy 2 64 bit or 4 32 bit mbuf point into rx_pkts */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

#if defined(RTE_ARCH_X86_64)
		/* B.1 load 2 64 bit mbuf points */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos+2]);
#endif

		/* A.1 load desc[2-0] */
		descs[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		rte_compiler_barrier();
		descs[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		rte_compiler_barrier();
		descs[0] = _mm_loadu_si128((__m128i *)(rxdp));

#if defined(RTE_ARCH_X86_64)
		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos+2], mbp2);
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

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

		desc_to_olflags_v(rxq, rxdp, descs, &rx_pkts[pos]);

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs[2], shuf_msk);

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
			 * of dd bits doesn't care. However, for end of
			 * packet tracking, we do care, so shuffle. This also
			 * compresses the 32-bit values to 8-bit
			 */
			eop_bits = _mm_shuffle_epi8(eop_bits, eop_shuf_mask);
			/* store the resulting 32-bit value */
			*(int *)split_packet = _mm_cvtsi128_si32(eop_bits);
			split_packet += RTE_I40E_DESCS_PER_LOOP;
		}

		/* C.3 calc available number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos+1]->rx_descriptor_fields1,
				 pkt_mb2);
		_mm_storeu_si128((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				 pkt_mb1);
		desc_to_ptype_v(descs, &rx_pkts[pos], ptype_tbl);
		/* C.4 calc available number of desc */
		var = __builtin_popcountll(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != RTE_I40E_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

 /*
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > RTE_I40E_VPMD_RX_BURST, only scan RTE_I40E_VPMD_RX_BURST
 *   numbers of DD bits
 */
uint16_t
i40e_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 *
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 */
static uint16_t
i40e_recv_scattered_burst_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			      uint16_t nb_pkts)
{

	struct i40e_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_I40E_VPMD_RX_BURST] = {0};

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
		rxq->pkt_first_seg = rx_pkts[i];
	}
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
		&split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 */
uint16_t
i40e_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > RTE_I40E_VPMD_RX_BURST) {
		uint16_t burst;

		burst = i40e_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      RTE_I40E_VPMD_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < RTE_I40E_VPMD_RX_BURST)
			return retval;
	}

	return retval + i40e_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      nb_pkts);
}

static inline void
vtx1(volatile struct i40e_tx_desc *txdp,
		struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw = (I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT) |
			((uint64_t)pkt->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT));

	__m128i descriptor = _mm_set_epi64x(high_qw,
				pkt->buf_iova + pkt->data_off);
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
vtx(volatile struct i40e_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

uint16_t
i40e_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;
	volatile struct i40e_tx_desc *txdp;
	struct i40e_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = I40E_TD_CMD;
	uint64_t rs = I40E_TX_DESC_CMD_RS | I40E_TD_CMD;
	int i;

	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

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
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	I40E_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

void __rte_cold
i40e_rx_queue_release_mbufs_vec(struct i40e_rx_queue *rxq)
{
	_i40e_rx_queue_release_mbufs_vec(rxq);
}

int __rte_cold
i40e_rxq_vec_setup(struct i40e_rx_queue *rxq)
{
	return i40e_rxq_vec_setup_default(rxq);
}

int __rte_cold
i40e_txq_vec_setup(struct i40e_tx_queue __rte_unused *txq)
{
	return 0;
}

int __rte_cold
i40e_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev)
{
	return i40e_rx_vec_dev_conf_condition_check_default(dev);
}
