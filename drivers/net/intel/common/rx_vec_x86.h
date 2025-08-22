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
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start].mbuf;
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	if (rte_mbuf_raw_alloc_bulk(rxq->mp, rxp, rearm_thresh) < 0) {
		if (rxq->rxrearm_nb + rearm_thresh >= rxq->nb_rx_desc) {
			const __m128i zero = _mm_setzero_si128();

			for (i = 0; i < CI_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rxq->fake_mbuf;
				_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[i]), zero);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed += rearm_thresh;
		return -1;
	}
	return 0;
}

/**
 * Reformat data from mbuf to descriptor for one RX descriptor, using SSE instruction set.
 *
 * @param mhdr pointer to first 16 bytes of mbuf header
 * @return 16-byte register in descriptor format.
 */
static __rte_always_inline __m128i
_ci_rxq_rearm_desc_sse(const __m128i *mhdr)
{
	const __m128i hdroom = _mm_set1_epi64x(RTE_PKTMBUF_HEADROOM);
	const __m128i zero = _mm_setzero_si128();

	/* add headroom to address values */
	__m128i reg = _mm_add_epi64(*mhdr, hdroom);

#if RTE_IOVA_IN_MBUF
	/* expect buf_addr (low 64 bit) and buf_iova (high 64bit) */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
			 offsetof(struct rte_mbuf, buf_addr) + 8);
	/* move IOVA to Packet Buffer Address, erase Header Buffer Address */
	reg = _mm_unpackhi_epi64(reg, zero);
#else
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_addr) != 0);
	/* erase Header Buffer Address */
	reg = _mm_unpacklo_epi64(reg, zero);
#endif
	return reg;
}

static __rte_always_inline void
_ci_rxq_rearm_sse(struct ci_rx_queue *rxq)
{
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	/* SSE writes 16-bytes regardless of descriptor size */
	const uint8_t desc_per_reg = 1;
	const uint8_t desc_per_iter = desc_per_reg * 2;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < rearm_thresh;
			i += desc_per_iter,
			rxp += desc_per_iter,
			rxdp += desc_per_iter) {
		const __m128i reg0 = _ci_rxq_rearm_desc_sse(
				RTE_CAST_PTR(const __m128i *, rxp[0].mbuf));
		const __m128i reg1 = _ci_rxq_rearm_desc_sse(
				RTE_CAST_PTR(const __m128i *, rxp[1].mbuf));

		/* flush descriptors */
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[0]), reg0);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[desc_per_reg]), reg1);
	}
}

#ifdef __AVX2__
/**
 * Reformat data from mbuf to descriptor for one RX descriptor, using AVX2 instruction set.
 *
 * Note that for 32-byte descriptors, the second parameter must be zeroed out.
 *
 * @param mhdr0 pointer to first 16-bytes of 1st mbuf header.
 * @param mhdr1 pointer to first 16-bytes of 2nd mbuf header.
 *
 * @return 32-byte register with two 16-byte descriptors in it.
 */
static __rte_always_inline __m256i
_ci_rxq_rearm_desc_avx2(const __m128i *mhdr0, const __m128i *mhdr1)
{
	const __m256i hdr_room = _mm256_set1_epi64x(RTE_PKTMBUF_HEADROOM);
	const __m256i zero = _mm256_setzero_si256();

	/* merge by casting 0 to 256-bit and inserting 1 into the high lanes */
	__m256i reg = _mm256_inserti128_si256(_mm256_castsi128_si256(*mhdr0), *mhdr1, 1);

	/* add headroom to address values */
	reg = _mm256_add_epi64(reg, hdr_room);

#if RTE_IOVA_IN_MBUF
	/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
			offsetof(struct rte_mbuf, buf_addr) + 8);
	/* extract IOVA addr into Packet Buffer Address, erase Header Buffer Address */
	reg = _mm256_unpackhi_epi64(reg, zero);
#else
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_addr) != 0);
	/* erase Header Buffer Address */
	reg = _mm256_unpacklo_epi64(reg, zero);
#endif
	return reg;
}

static __rte_always_inline void
_ci_rxq_rearm_avx2(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	/* how many descriptors can fit into a register */
	const uint8_t desc_per_reg = sizeof(__m256i) / sizeof(union ci_rx_desc);
	/* how many descriptors can fit into one loop iteration */
	const uint8_t desc_per_iter = desc_per_reg * 2;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 2 or 4 mbufs in one loop */
	for (i = 0; i < rearm_thresh;
			i += desc_per_iter,
			rxp += desc_per_iter,
			rxdp += desc_per_iter) {
		__m256i reg0, reg1;

		if (desc_per_iter == 2) {
			/* no need to call AVX2 version as we only need two descriptors */
			reg0 = _mm256_castsi128_si256(
				_ci_rxq_rearm_desc_sse(
					RTE_CAST_PTR(const __m128i *, rxp[0].mbuf)));
			reg1 = _mm256_castsi128_si256(
				_ci_rxq_rearm_desc_sse(
					RTE_CAST_PTR(const __m128i *, rxp[1].mbuf)));
		} else {
			/* 16 byte descriptor times four */
			reg0 = _ci_rxq_rearm_desc_avx2(
					RTE_CAST_PTR(const __m128i *, rxp[0].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[1].mbuf));
			reg1 = _ci_rxq_rearm_desc_avx2(
					RTE_CAST_PTR(const __m128i *, rxp[2].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[3].mbuf));
		}

		/* flush descriptors */
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, &rxdp[0]), reg0);
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, &rxdp[desc_per_reg]), reg1);
	}
}
#endif /* __AVX2__ */

#ifdef __AVX512VL__
/**
 * Reformat data from mbuf to descriptor for one RX descriptor, using AVX512 instruction set.
 *
 * Note that for 32-byte descriptors, every second parameter must be zeroed out.
 *
 * @param mhdr0 pointer to first 16-bytes of 1st mbuf header.
 * @param mhdr1 pointer to first 16-bytes of 2nd mbuf header.
 * @param mhdr2 pointer to first 16-bytes of 3rd mbuf header.
 * @param mhdr3 pointer to first 16-bytes of 4th mbuf header.
 *
 * @return 64-byte register with four 16-byte descriptors in it.
 */
static __rte_always_inline __m512i
_ci_rxq_rearm_desc_avx512(const __m128i *mhdr0, const __m128i *mhdr1,
		const __m128i *mhdr2, const __m128i *mhdr3)
{
	const __m512i zero = _mm512_setzero_si512();
	const __m512i hdroom = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);

	/**
	 * merge 0 & 1, by casting 0 to 256-bit and inserting 1 into the high
	 * lanes. Similarly for 2 & 3.
	 */
	const __m256i vaddr0_1 = _mm256_inserti128_si256(_mm256_castsi128_si256(*mhdr0), *mhdr1, 1);
	const __m256i vaddr2_3 = _mm256_inserti128_si256(_mm256_castsi128_si256(*mhdr2), *mhdr3, 1);
	/*
	 * merge 0+1 & 2+3, by casting 0+1 to 512-bit and inserting 2+3 into the
	 * high lanes.
	 */
	__m512i reg = _mm512_inserti64x4(_mm512_castsi256_si512(vaddr0_1), vaddr2_3, 1);

	/* add headroom to address values */
	reg = _mm512_add_epi64(reg, hdroom);

#if RTE_IOVA_IN_MBUF
	/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
			offsetof(struct rte_mbuf, buf_addr) + 8);
	/* extract IOVA addr into Packet Buffer Address, erase Header Buffer Address */
	reg = _mm512_unpackhi_epi64(reg, zero);
#else
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_addr) != 0);
	/* erase Header Buffer Address */
	reg = _mm512_unpacklo_epi64(reg, zero);
#endif
	return reg;
}

static __rte_always_inline void
_ci_rxq_rearm_avx512(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	/* how many descriptors can fit into a register */
	const uint8_t desc_per_reg = sizeof(__m512i) / sizeof(union ci_rx_desc);
	/* how many descriptors can fit into one loop iteration */
	const uint8_t desc_per_iter = desc_per_reg * 2;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 4 or 8 mbufs in one loop */
	for (i = 0; i < rearm_thresh;
			i += desc_per_iter,
			rxp += desc_per_iter,
			rxdp += desc_per_iter) {
		__m512i reg0, reg1;

		if (desc_per_iter == 4) {
			/* 16-byte descriptor, 16 byte zero, times four */
			const __m128i zero = _mm_setzero_si128();

			reg0 = _ci_rxq_rearm_desc_avx512(
					RTE_CAST_PTR(const __m128i *, rxp[0].mbuf),
					&zero,
					RTE_CAST_PTR(const __m128i *, rxp[1].mbuf),
					&zero);
			reg1 = _ci_rxq_rearm_desc_avx512(
					RTE_CAST_PTR(const __m128i *, rxp[2].mbuf),
					&zero,
					RTE_CAST_PTR(const __m128i *, rxp[3].mbuf),
					&zero);
		} else {
			/* 16-byte descriptor times eight */
			reg0 = _ci_rxq_rearm_desc_avx512(
					RTE_CAST_PTR(const __m128i *, rxp[0].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[1].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[2].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[3].mbuf));
			reg1 = _ci_rxq_rearm_desc_avx512(
					RTE_CAST_PTR(const __m128i *, rxp[4].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[5].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[6].mbuf),
					RTE_CAST_PTR(const __m128i *, rxp[7].mbuf));
		}

		/* flush desc with pa dma_addr */
		_mm512_store_si512(RTE_CAST_PTR(__m512i *, &rxdp[0]), reg0);
		_mm512_store_si512(RTE_CAST_PTR(__m512i *, &rxdp[desc_per_reg]), reg1);
	}
}
#endif /* __AVX512VL__ */

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
		_ci_rxq_rearm_sse(rxq);
		break;
	}

	rxq->rxrearm_start += rearm_thresh;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= rearm_thresh;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	rte_write32_wc(rte_cpu_to_le_32(rx_id), rxq->qrx_tail);
}

#ifdef CC_AVX512_SUPPORT
#define X86_MAX_SIMD_BITWIDTH (rte_vect_get_max_simd_bitwidth())
#else
#define X86_MAX_SIMD_BITWIDTH RTE_MIN(256, rte_vect_get_max_simd_bitwidth())
#endif /* CC_AVX512_SUPPORT */

static inline enum rte_vect_max_simd
ci_get_x86_max_simd_bitwidth(void)
{
	int ret = RTE_VECT_SIMD_DISABLED;
	int simd = X86_MAX_SIMD_BITWIDTH;

	if (simd >= 512 && rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1)
		ret = RTE_VECT_SIMD_512;
	else if (simd >= 256 && (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1))
		ret = RTE_VECT_SIMD_256;
	else if (simd >= 128)
		ret = RTE_VECT_SIMD_128;
	return ret;
}

#endif /* _COMMON_INTEL_RX_VEC_X86_H_ */
