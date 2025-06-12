/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _COMMON_INTEL_RX_VEC_X86_H_
#define _COMMON_INTEL_RX_VEC_X86_H_

#include <stdint.h>

#include <ethdev_driver.h>
#include <rte_io.h>

#include "rx.h"

enum ci_rx_vec_level {
	CI_RX_VEC_LEVEL_SSE = 0,
	CI_RX_VEC_LEVEL_AVX2,
	CI_RX_VEC_LEVEL_AVX512,
};

static inline int
_ci_rxq_rearm_get_bufs(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	if (rte_mempool_get_bulk(rxq->mp, (void **)rxp, rearm_thresh) < 0) {
		if (rxq->rxrearm_nb + rearm_thresh >= rxq->nb_rx_desc) {
			const __m128i zero = _mm_setzero_si128();

			for (i = 0; i < CI_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[i]), zero);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed += rearm_thresh;
		return -1;
	}
	return 0;
}

/*
 * SSE code path can handle both 16-byte and 32-byte descriptors with one code
 * path, as we only ever write 16 bytes at a time.
 */
static __rte_always_inline void
_ci_rxq_rearm_sse(struct ci_rx_queue *rxq)
{
	const __m128i hdroom = _mm_set1_epi64x(RTE_PKTMBUF_HEADROOM);
	const __m128i zero = _mm_setzero_si128();
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < rearm_thresh; i += 2, rxp += 2, rxdp += 2) {
		struct rte_mbuf *mb0 = rxp[0].mbuf;
		struct rte_mbuf *mb1 = rxp[1].mbuf;

#if RTE_IOVA_IN_MBUF
		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
#endif
		__m128i addr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		__m128i addr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);

		/* add headroom to address values */
		addr0 = _mm_add_epi64(addr0, hdroom);
		addr1 = _mm_add_epi64(addr1, hdroom);

#if RTE_IOVA_IN_MBUF
		/* move IOVA to Packet Buffer Address, erase Header Buffer Address */
		addr0 = _mm_unpackhi_epi64(addr0, zero);
		addr0 = _mm_unpackhi_epi64(addr1, zero);
#else
		/* erase Header Buffer Address */
		addr0 = _mm_unpacklo_epi64(addr0, zero);
		addr1 = _mm_unpacklo_epi64(addr1, zero);
#endif

		/* flush desc with pa dma_addr */
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[0]), addr0);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[1]), addr1);
	}
}

#ifdef RTE_NET_INTEL_USE_16BYTE_DESC
#ifdef __AVX2__
/* AVX2 version for 16-byte descriptors, handles 4 buffers at a time */
static __rte_always_inline void
_ci_rxq_rearm_avx2(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	const __m256i hdroom = _mm256_set1_epi64x(RTE_PKTMBUF_HEADROOM);
	const __m256i zero = _mm256_setzero_si256();
	volatile union ci_rx_desc *rxdp;
	int i;

	RTE_BUILD_BUG_ON(sizeof(union ci_rx_desc) != 16);

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 4 mbufs in one loop */
	for (i = 0; i < rearm_thresh; i += 4, rxp += 4, rxdp += 4) {
		struct rte_mbuf *mb0 = rxp[0].mbuf;
		struct rte_mbuf *mb1 = rxp[1].mbuf;
		struct rte_mbuf *mb2 = rxp[2].mbuf;
		struct rte_mbuf *mb3 = rxp[3].mbuf;

#if RTE_IOVA_IN_MBUF
		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
#endif
		const __m128i vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		const __m128i vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);
		const __m128i vaddr2 = _mm_loadu_si128((__m128i *)&mb2->buf_addr);
		const __m128i vaddr3 = _mm_loadu_si128((__m128i *)&mb3->buf_addr);

		/**
		 * merge 0 & 1, by casting 0 to 256-bit and inserting 1
		 * into the high lanes. Similarly for 2 & 3
		 */
		const __m256i vaddr0_256 = _mm256_castsi128_si256(vaddr0);
		const __m256i vaddr2_256 = _mm256_castsi128_si256(vaddr2);

		__m256i addr0_1 = _mm256_inserti128_si256(vaddr0_256, vaddr1, 1);
		__m256i addr2_3 = _mm256_inserti128_si256(vaddr2_256, vaddr3, 1);

		/* add headroom to address values */
		addr0_1 = _mm256_add_epi64(addr0_1, hdroom);
		addr0_1 = _mm256_add_epi64(addr0_1, hdroom);

#if RTE_IOVA_IN_MBUF
		/* extract IOVA addr into Packet Buffer Address, erase Header Buffer Address */
		addr0_1 = _mm256_unpackhi_epi64(addr0_1, zero);
		addr2_3 = _mm256_unpackhi_epi64(addr2_3, zero);
#else
		/* erase Header Buffer Address */
		addr0_1 = _mm256_unpacklo_epi64(addr0_1, zero);
		addr2_3 = _mm256_unpacklo_epi64(addr2_3, zero);
#endif

		/* flush desc with pa dma_addr */
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, &rxdp[0]), addr0_1);
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, &rxdp[2]), addr2_3);
	}
}
#endif /* __AVX2__ */

#ifdef __AVX512VL__
/* AVX512 version for 16-byte descriptors, handles 8 buffers at a time */
static __rte_always_inline void
_ci_rxq_rearm_avx512(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	const __m512i hdroom = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);
	const __m512i zero = _mm512_setzero_si512();
	volatile union ci_rx_desc *rxdp;
	int i;

	RTE_BUILD_BUG_ON(sizeof(union ci_rx_desc) != 16);

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 8 mbufs in one loop */
	for (i = 0; i < rearm_thresh; i += 8, rxp += 8, rxdp += 8) {
		struct rte_mbuf *mb0 = rxp[0].mbuf;
		struct rte_mbuf *mb1 = rxp[1].mbuf;
		struct rte_mbuf *mb2 = rxp[2].mbuf;
		struct rte_mbuf *mb3 = rxp[3].mbuf;
		struct rte_mbuf *mb4 = rxp[4].mbuf;
		struct rte_mbuf *mb5 = rxp[5].mbuf;
		struct rte_mbuf *mb6 = rxp[6].mbuf;
		struct rte_mbuf *mb7 = rxp[7].mbuf;

#if RTE_IOVA_IN_MBUF
		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
#endif
		const __m128i vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		const __m128i vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);
		const __m128i vaddr2 = _mm_loadu_si128((__m128i *)&mb2->buf_addr);
		const __m128i vaddr3 = _mm_loadu_si128((__m128i *)&mb3->buf_addr);
		const __m128i vaddr4 = _mm_loadu_si128((__m128i *)&mb4->buf_addr);
		const __m128i vaddr5 = _mm_loadu_si128((__m128i *)&mb5->buf_addr);
		const __m128i vaddr6 = _mm_loadu_si128((__m128i *)&mb6->buf_addr);
		const __m128i vaddr7 = _mm_loadu_si128((__m128i *)&mb7->buf_addr);

		/**
		 * merge 0 & 1, by casting 0 to 256-bit and inserting 1
		 * into the high lanes. Similarly for 2 & 3, and so on.
		 */
		const __m256i addr0_256 = _mm256_castsi128_si256(vaddr0);
		const __m256i addr2_256 = _mm256_castsi128_si256(vaddr2);
		const __m256i addr4_256 = _mm256_castsi128_si256(vaddr4);
		const __m256i addr6_256 = _mm256_castsi128_si256(vaddr6);

		const __m256i addr0_1 = _mm256_inserti128_si256(addr0_256, vaddr1, 1);
		const __m256i addr2_3 = _mm256_inserti128_si256(addr2_256, vaddr3, 1);
		const __m256i addr4_5 = _mm256_inserti128_si256(addr4_256, vaddr5, 1);
		const __m256i addr6_7 = _mm256_inserti128_si256(addr6_256, vaddr7, 1);

		/**
		 * merge 0_1 & 2_3, by casting 0_1 to 512-bit and inserting 2_3
		 * into the high lanes. Similarly for 4_5 & 6_7, and so on.
		 */
		const __m512i addr0_1_512 = _mm512_castsi256_si512(addr0_1);
		const __m512i addr4_5_512 = _mm512_castsi256_si512(addr4_5);

		__m512i addr0_3 = _mm512_inserti64x4(addr0_1_512, addr2_3, 1);
		__m512i addr4_7 = _mm512_inserti64x4(addr4_5_512, addr6_7, 1);

		/* add headroom to address values */
		addr0_3 = _mm512_add_epi64(addr0_3, hdroom);
		addr4_7 = _mm512_add_epi64(addr4_7, hdroom);

#if RTE_IOVA_IN_MBUF
		/* extract IOVA addr into Packet Buffer Address, erase Header Buffer Address */
		addr0_3 = _mm512_unpackhi_epi64(addr0_3, zero);
		addr4_7 = _mm512_unpackhi_epi64(addr4_7, zero);
#else
		/* erase Header Buffer Address */
		addr0_3 = _mm512_unpacklo_epi64(addr0_3, zero);
		addr4_7 = _mm512_unpacklo_epi64(addr4_7, zero);
#endif

		/* flush desc with pa dma_addr */
		_mm512_store_si512(RTE_CAST_PTR(__m512i *, &rxdp[0]), addr0_3);
		_mm512_store_si512(RTE_CAST_PTR(__m512i *, &rxdp[4]), addr4_7);
	}
}
#endif /* __AVX512VL__ */
#endif /* RTE_NET_INTEL_USE_16BYTE_DESC */

/**
 * Rearm the RX queue with new buffers.
 *
 * This function is inlined, so the last parameter will be constant-propagated
 * if specified at compile time, and thus all unnecessary branching will be
 * eliminated.
 *
 * @param rxq
 *   Pointer to the RX queue structure.
 * @param vec_level
 *   The vectorization level to use for rearming.
 */
static __rte_always_inline void
ci_rxq_rearm(struct ci_rx_queue *rxq, const enum ci_rx_vec_level vec_level)
{
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	uint16_t rx_id;

	/* Pull 'n' more MBUFs into the software ring */
	if (_ci_rxq_rearm_get_bufs(rxq) < 0)
		return;

#ifdef RTE_NET_INTEL_USE_16BYTE_DESC
	switch (vec_level) {
	case CI_RX_VEC_LEVEL_AVX512:
#ifdef __AVX512VL__
		_ci_rxq_rearm_avx512(rxq);
		break;
#else
		/* fall back to AVX2 */
		/* fall through */
#endif
	case CI_RX_VEC_LEVEL_AVX2:
#ifdef __AVX2__
		_ci_rxq_rearm_avx2(rxq);
		break;
#else
		/* fall back to SSE */
		/* fall through */
#endif
	case CI_RX_VEC_LEVEL_SSE:
		_ci_rxq_rearm_sse(rxq, desc_len);
		break;
	}
#else
	/* for 32-byte descriptors only support SSE */
	switch (vec_level) {
	case CI_RX_VEC_LEVEL_AVX512:
	case CI_RX_VEC_LEVEL_AVX2:
	case CI_RX_VEC_LEVEL_SSE:
		_ci_rxq_rearm_sse(rxq);
		break;
	}
#endif /* RTE_NET_INTEL_USE_16BYTE_DESC */

	rxq->rxrearm_start += rearm_thresh;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= rearm_thresh;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	rte_write32_wc(rte_cpu_to_le_32(rx_id), rxq->qrx_tail);
}

#endif /* _COMMON_INTEL_RX_VEC_X86_H_ */
