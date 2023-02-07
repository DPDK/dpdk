/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <rte_vect.h>
#include <idpf_common_device.h>
#include <idpf_common_rxtx.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define IDPF_DESCS_PER_LOOP_AVX 8
#define PKTLEN_SHIFT 10

static __rte_always_inline void
idpf_singleq_rearm_common(struct idpf_rx_queue *rxq)
{
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start];
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;
	uint16_t rx_id;
	int i;

	rxdp += rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxp,
				 IDPF_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + IDPF_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IDPF_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		__atomic_fetch_add(&rxq->rx_stats.mbuf_alloc_failed,
				   IDPF_RXQ_REARM_THRESH, __ATOMIC_RELAXED);
		return;
	}
	struct rte_mbuf *mb0, *mb1, *mb2, *mb3;
	struct rte_mbuf *mb4, *mb5, *mb6, *mb7;
	__m512i dma_addr0_3, dma_addr4_7;
	__m512i hdr_room = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);
	/* Initialize the mbufs in vector, process 8 mbufs in one loop */
	for (i = 0; i < IDPF_RXQ_REARM_THRESH;
			i += 8, rxp += 8, rxdp += 8) {
		__m128i vaddr0, vaddr1, vaddr2, vaddr3;
		__m128i vaddr4, vaddr5, vaddr6, vaddr7;
		__m256i vaddr0_1, vaddr2_3;
		__m256i vaddr4_5, vaddr6_7;
		__m512i vaddr0_3, vaddr4_7;

		mb0 = rxp[0];
		mb1 = rxp[1];
		mb2 = rxp[2];
		mb3 = rxp[3];
		mb4 = rxp[4];
		mb5 = rxp[5];
		mb6 = rxp[6];
		mb7 = rxp[7];

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
		vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);
		vaddr2 = _mm_loadu_si128((__m128i *)&mb2->buf_addr);
		vaddr3 = _mm_loadu_si128((__m128i *)&mb3->buf_addr);
		vaddr4 = _mm_loadu_si128((__m128i *)&mb4->buf_addr);
		vaddr5 = _mm_loadu_si128((__m128i *)&mb5->buf_addr);
		vaddr6 = _mm_loadu_si128((__m128i *)&mb6->buf_addr);
		vaddr7 = _mm_loadu_si128((__m128i *)&mb7->buf_addr);

		/**
		 * merge 0 & 1, by casting 0 to 256-bit and inserting 1
		 * into the high lanes. Similarly for 2 & 3, and so on.
		 */
		vaddr0_1 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr0),
						vaddr1, 1);
		vaddr2_3 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr2),
						vaddr3, 1);
		vaddr4_5 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr4),
						vaddr5, 1);
		vaddr6_7 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr6),
						vaddr7, 1);
		vaddr0_3 =
			_mm512_inserti64x4(_mm512_castsi256_si512(vaddr0_1),
						vaddr2_3, 1);
		vaddr4_7 =
			_mm512_inserti64x4(_mm512_castsi256_si512(vaddr4_5),
						vaddr6_7, 1);

		/* convert pa to dma_addr hdr/data */
		dma_addr0_3 = _mm512_unpackhi_epi64(vaddr0_3, vaddr0_3);
		dma_addr4_7 = _mm512_unpackhi_epi64(vaddr4_7, vaddr4_7);

		/* add headroom to pa values */
		dma_addr0_3 = _mm512_add_epi64(dma_addr0_3, hdr_room);
		dma_addr4_7 = _mm512_add_epi64(dma_addr4_7, hdr_room);

		/* flush desc with pa dma_addr */
		_mm512_store_si512((__m512i *)&rxdp->read, dma_addr0_3);
		_mm512_store_si512((__m512i *)&(rxdp + 4)->read, dma_addr4_7);
	}

	rxq->rxrearm_start += IDPF_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IDPF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IDPF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
}

static __rte_always_inline void
idpf_singleq_rearm(struct idpf_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;
	struct rte_mempool_cache *cache =
		rte_mempool_default_cache(rxq->mp, rte_lcore_id());
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start];

	rxdp += rxq->rxrearm_start;

	if (unlikely(cache == NULL))
		return idpf_singleq_rearm_common(rxq);

	/* We need to pull 'n' more MBUFs into the software ring from mempool
	 * We inline the mempool function here, so we can vectorize the copy
	 * from the cache into the shadow ring.
	 */

	/* Can this be satisfied from the cache? */
	if (cache->len < IDPF_RXQ_REARM_THRESH) {
		/* No. Backfill the cache first, and then fill from it */
		uint32_t req = IDPF_RXQ_REARM_THRESH + (cache->size -
							cache->len);

		/* How many do we require i.e. number to fill the cache + the request */
		int ret = rte_mempool_ops_dequeue_bulk
				(rxq->mp, &cache->objs[cache->len], req);
		if (ret == 0) {
			cache->len += req;
		} else {
			if (rxq->rxrearm_nb + IDPF_RXQ_REARM_THRESH >=
			    rxq->nb_rx_desc) {
				__m128i dma_addr0;

				dma_addr0 = _mm_setzero_si128();
				for (i = 0; i < IDPF_VPMD_DESCS_PER_LOOP; i++) {
					rxp[i] = &rxq->fake_mbuf;
					_mm_storeu_si128((__m128i *)&rxdp[i].read,
							 dma_addr0);
				}
			}
			__atomic_fetch_add(&rxq->rx_stats.mbuf_alloc_failed,
					   IDPF_RXQ_REARM_THRESH, __ATOMIC_RELAXED);
			return;
		}
	}

	const __m512i iova_offsets =  _mm512_set1_epi64(offsetof
							(struct rte_mbuf, buf_iova));
	const __m512i headroom = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);

	/* to shuffle the addresses to correct slots. Values 4-7 will contain
	 * zeros, so use 7 for a zero-value.
	 */
	const __m512i permute_idx = _mm512_set_epi64(7, 7, 3, 1, 7, 7, 2, 0);

	/* Initialize the mbufs in vector, process 8 mbufs in one loop, taking
	 * from mempool cache and populating both shadow and HW rings
	 */
	for (i = 0; i < IDPF_RXQ_REARM_THRESH / IDPF_DESCS_PER_LOOP_AVX; i++) {
		const __m512i mbuf_ptrs = _mm512_loadu_si512
			(&cache->objs[cache->len - IDPF_DESCS_PER_LOOP_AVX]);
		_mm512_storeu_si512(rxp, mbuf_ptrs);

		const __m512i iova_base_addrs = _mm512_i64gather_epi64
				(_mm512_add_epi64(mbuf_ptrs, iova_offsets),
				 0, /* base */
				 1  /* scale */);
		const __m512i iova_addrs = _mm512_add_epi64(iova_base_addrs,
				headroom);
		const __m512i iovas0 = _mm512_castsi256_si512
				(_mm512_extracti64x4_epi64(iova_addrs, 0));
		const __m512i iovas1 = _mm512_castsi256_si512
				(_mm512_extracti64x4_epi64(iova_addrs, 1));

		/* permute leaves desc 2-3 addresses in header address slots 0-1
		 * but these are ignored by driver since header split not
		 * enabled. Similarly for desc 6 & 7.
		 */
		const __m512i desc0_1 = _mm512_permutexvar_epi64
				(permute_idx,
				 iovas0);
		const __m512i desc2_3 = _mm512_bsrli_epi128(desc0_1, 8);

		const __m512i desc4_5 = _mm512_permutexvar_epi64
				(permute_idx,
				 iovas1);
		const __m512i desc6_7 = _mm512_bsrli_epi128(desc4_5, 8);

		_mm512_storeu_si512((void *)rxdp, desc0_1);
		_mm512_storeu_si512((void *)(rxdp + 2), desc2_3);
		_mm512_storeu_si512((void *)(rxdp + 4), desc4_5);
		_mm512_storeu_si512((void *)(rxdp + 6), desc6_7);

		rxp += IDPF_DESCS_PER_LOOP_AVX;
		rxdp += IDPF_DESCS_PER_LOOP_AVX;
		cache->len -= IDPF_DESCS_PER_LOOP_AVX;
	}

	rxq->rxrearm_start += IDPF_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IDPF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			   (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IDPF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
}

#define IDPF_RX_LEN_MASK 0x80808080
static __rte_always_inline uint16_t
_idpf_singleq_recv_raw_pkts_avx512(struct idpf_rx_queue *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts)
{
	const uint32_t *type_table = rxq->adapter->ptype_tbl;

	const __m256i mbuf_init = _mm256_set_epi64x(0, 0, 0,
						    rxq->mbuf_initializer);
	struct rte_mbuf **sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;

	rxdp += rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IDPF_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IDPF_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IDPF_RXQ_REARM_THRESH)
		idpf_singleq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if ((rxdp->flex_nic_wb.status_error0  &
	      rte_cpu_to_le_32(1 << VIRTCHNL2_RX_FLEX_DESC_STATUS0_DD_S)) == 0)
		return 0;

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* mask to shuffle from desc. to mbuf (4 descriptors)*/
	const __m512i shuf_msk =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0xFFFFFFFF,    /* rss set as unknown */
			 0xFFFF0504,    /* vlan_macip set as unknown */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 2nd descriptor */
			 0xFFFFFFFF,    /* rss set as unknown */
			 0xFFFF0504,    /* vlan_macip set as unknown */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 3rd descriptor */
			 0xFFFFFFFF,    /* rss set as unknown */
			 0xFFFF0504,    /* vlan_macip set as unknown */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 4th descriptor */
			 0xFFFFFFFF,    /* rss set as unknown */
			 0xFFFF0504,    /* vlan_macip set as unknown */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF     /* pkt_type set as unknown */
			);
	/**
	 * compile-time check the shuffle layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi
	 * calls above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IDPF_DESCS_PER_LOOP_AVX,
	     rxdp += IDPF_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m512i raw_desc0_3, raw_desc4_7;
		const __m128i raw_desc7 =
			_mm_load_si128((void *)(rxdp + 7));
		rte_compiler_barrier();
		const __m128i raw_desc6 =
			_mm_load_si128((void *)(rxdp + 6));
		rte_compiler_barrier();
		const __m128i raw_desc5 =
			_mm_load_si128((void *)(rxdp + 5));
		rte_compiler_barrier();
		const __m128i raw_desc4 =
			_mm_load_si128((void *)(rxdp + 4));
		rte_compiler_barrier();
		const __m128i raw_desc3 =
			_mm_load_si128((void *)(rxdp + 3));
		rte_compiler_barrier();
		const __m128i raw_desc2 =
			_mm_load_si128((void *)(rxdp + 2));
		rte_compiler_barrier();
		const __m128i raw_desc1 =
			_mm_load_si128((void *)(rxdp + 1));
		rte_compiler_barrier();
		const __m128i raw_desc0 =
			_mm_load_si128((void *)(rxdp + 0));

		raw_desc4_7 = _mm512_broadcast_i32x4(raw_desc4);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc5, 1);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc6, 2);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc7, 3);
		raw_desc0_3 = _mm512_broadcast_i32x4(raw_desc0);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc1, 1);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc2, 2);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc3, 3);

		/**
		 * convert descriptors 4-7 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len4_7 = _mm512_slli_epi32(raw_desc4_7,
							 PKTLEN_SHIFT);
		const __m512i desc4_7 = _mm512_mask_blend_epi16(IDPF_RX_LEN_MASK,
								raw_desc4_7,
								len4_7);
		__m512i mb4_7 = _mm512_shuffle_epi8(desc4_7, shuf_msk);

		/**
		 * to get packet types, shift 64-bit values down 30 bits
		 * and so ptype is in lower 8-bits in each
		 */
		const __m512i ptypes4_7 = _mm512_srli_epi64(desc4_7, 16);
		const __m256i ptypes6_7 = _mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 = _mm512_extracti64x4_epi64(ptypes4_7, 0);
		const uint8_t ptype7 = _mm256_extract_epi8(ptypes6_7, 16);
		const uint8_t ptype6 = _mm256_extract_epi8(ptypes6_7, 0);
		const uint8_t ptype5 = _mm256_extract_epi8(ptypes4_5, 16);
		const uint8_t ptype4 = _mm256_extract_epi8(ptypes4_5, 0);

		const __m512i ptype4_7 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype7],
			 0, 0, 0, type_table[ptype6],
			 0, 0, 0, type_table[ptype5],
			 0, 0, 0, type_table[ptype4]);
		mb4_7 = _mm512_mask_blend_epi32(0x1111, mb4_7, ptype4_7);

		/**
		 * convert descriptors 0-3 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len0_3 = _mm512_slli_epi32(raw_desc0_3,
							 PKTLEN_SHIFT);
		const __m512i desc0_3 = _mm512_mask_blend_epi16(IDPF_RX_LEN_MASK,
								raw_desc0_3,
								len0_3);
		__m512i mb0_3 = _mm512_shuffle_epi8(desc0_3, shuf_msk);

		/* get the packet types */
		const __m512i ptypes0_3 = _mm512_srli_epi64(desc0_3, 16);
		const __m256i ptypes2_3 = _mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 = _mm512_extracti64x4_epi64(ptypes0_3, 0);
		const uint8_t ptype3 = _mm256_extract_epi8(ptypes2_3, 16);
		const uint8_t ptype2 = _mm256_extract_epi8(ptypes2_3, 0);
		const uint8_t ptype1 = _mm256_extract_epi8(ptypes0_1, 16);
		const uint8_t ptype0 = _mm256_extract_epi8(ptypes0_1, 0);

		const __m512i ptype0_3 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype3],
			 0, 0, 0, type_table[ptype2],
			 0, 0, 0, type_table[ptype1],
			 0, 0, 0, type_table[ptype0]);
		mb0_3 = _mm512_mask_blend_epi32(0x1111, mb0_3, ptype0_3);

		/**
		 * use permute/extract to get status content
		 * After the operations, the packets status flags are in the
		 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
		 */
		/* merge the status bits into one register */
		const __m512i status_permute_msk = _mm512_set_epi32
			(0, 0, 0, 0,
			 0, 0, 0, 0,
			 22, 30, 6, 14,
			 18, 26, 2, 10);
		const __m512i raw_status0_7 = _mm512_permutex2var_epi32
			(raw_desc4_7, status_permute_msk, raw_desc0_3);
		__m256i status0_7 = _mm512_extracti64x4_epi64
			(raw_status0_7, 0);

		/* now do flag manipulation */

		/**
		 * At this point, we have the 8 sets of flags in the low 16-bits
		 * of each 32-bit value.
		 * We want to extract these, and merge them with the mbuf init
		 * data so we can do a single write to the mbuf to set the flags
		 * and all the other initialization fields. Extracting the
		 * appropriate flags means that we have to do a shift and blend
		 * for each mbuf before we do the write. However, we can also
		 * add in the previously computed rx_descriptor fields to
		 * make a single 256-bit write per mbuf
		 */
		/* check the structure matches expectations */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
				 offsetof(struct rte_mbuf, rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
				 RTE_ALIGN(offsetof(struct rte_mbuf,
						    rearm_data),
						    16));
		/* build up data and do writes */
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5,
			rearm6, rearm7;
		const __m256i mb4_5 = _mm512_extracti64x4_epi64(mb4_7, 0);
		const __m256i mb6_7 = _mm512_extracti64x4_epi64(mb4_7, 1);
		const __m256i mb0_1 = _mm512_extracti64x4_epi64(mb0_3, 0);
		const __m256i mb2_3 = _mm512_extracti64x4_epi64(mb0_3, 1);

		rearm6 = _mm256_permute2f128_si256(mbuf_init, mb6_7, 0x20);
		rearm4 = _mm256_permute2f128_si256(mbuf_init, mb4_5, 0x20);
		rearm2 = _mm256_permute2f128_si256(mbuf_init, mb2_3, 0x20);
		rearm0 = _mm256_permute2f128_si256(mbuf_init, mb0_1, 0x20);

		/* write to mbuf */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);

		rearm7 = _mm256_blend_epi32(mbuf_init, mb6_7, 0xF0);
		rearm5 = _mm256_blend_epi32(mbuf_init, mb4_5, 0xF0);
		rearm3 = _mm256_blend_epi32(mbuf_init, mb2_3, 0xF0);
		rearm1 = _mm256_blend_epi32(mbuf_init, mb0_1, 0xF0);

		/* again write to mbufs */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);

		/* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32(status0_7,
					       _mm256_setzero_si256());

		uint64_t burst = __builtin_popcountll
					(_mm_cvtsi128_si64
						(_mm256_extracti128_si256
							(status0_7, 1)));
		burst += __builtin_popcountll
				(_mm_cvtsi128_si64
					(_mm256_castsi256_si128(status0_7)));
		received += burst;
		if (burst != IDPF_DESCS_PER_LOOP_AVX)
			break;
	}

	/* update tail pointers */
	rxq->rx_tail += received;
	rxq->rx_tail &= (rxq->nb_rx_desc - 1);
	if ((rxq->rx_tail & 1) == 1 && received > 1) { /* keep aligned */
		rxq->rx_tail--;
		received--;
	}
	rxq->rxrearm_nb += received;
	return received;
}

/**
 * Notice:
 * - nb_pkts < IDPF_DESCS_PER_LOOP, just return no packet
 */
uint16_t
idpf_dp_singleq_recv_pkts_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return _idpf_singleq_recv_raw_pkts_avx512(rx_queue, rx_pkts, nb_pkts);
}

static __rte_always_inline void
idpf_splitq_rearm_common(struct idpf_rx_queue *rx_bufq)
{
	struct rte_mbuf **rxp = &rx_bufq->sw_ring[rx_bufq->rxrearm_start];
	volatile union virtchnl2_rx_buf_desc *rxdp = rx_bufq->rx_ring;
	uint16_t rx_id;
	int i;

	rxdp += rx_bufq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rx_bufq->mp,
				 (void *)rxp,
				 IDPF_RXQ_REARM_THRESH) < 0) {
		if (rx_bufq->rxrearm_nb + IDPF_RXQ_REARM_THRESH >=
		    rx_bufq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IDPF_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rx_bufq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i],
						dma_addr0);
			}
		}
	__atomic_fetch_add(&rx_bufq->rx_stats.mbuf_alloc_failed,
			   IDPF_RXQ_REARM_THRESH, __ATOMIC_RELAXED);
		return;
	}

	/* Initialize the mbufs in vector, process 8 mbufs in one loop */
	for (i = 0; i < IDPF_RXQ_REARM_THRESH;
			i += 8, rxp += 8, rxdp += 8) {
		rxdp[0].split_rd.pkt_addr = rxp[0]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[1].split_rd.pkt_addr = rxp[1]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[2].split_rd.pkt_addr = rxp[2]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[3].split_rd.pkt_addr = rxp[3]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[4].split_rd.pkt_addr = rxp[4]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[5].split_rd.pkt_addr = rxp[5]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[6].split_rd.pkt_addr = rxp[6]->buf_iova + RTE_PKTMBUF_HEADROOM;
		rxdp[7].split_rd.pkt_addr = rxp[7]->buf_iova + RTE_PKTMBUF_HEADROOM;
	}

	rx_bufq->rxrearm_start += IDPF_RXQ_REARM_THRESH;
	if (rx_bufq->rxrearm_start >= rx_bufq->nb_rx_desc)
		rx_bufq->rxrearm_start = 0;

	rx_bufq->rxrearm_nb -= IDPF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rx_bufq->rxrearm_start == 0) ?
			     (rx_bufq->nb_rx_desc - 1) : (rx_bufq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IDPF_PCI_REG_WRITE(rx_bufq->qrx_tail, rx_id);
}

static __rte_always_inline void
idpf_splitq_rearm(struct idpf_rx_queue *rx_bufq)
{
	int i;
	uint16_t rx_id;
	volatile union virtchnl2_rx_buf_desc *rxdp = rx_bufq->rx_ring;
	struct rte_mempool_cache *cache =
		rte_mempool_default_cache(rx_bufq->mp, rte_lcore_id());
	struct rte_mbuf **rxp = &rx_bufq->sw_ring[rx_bufq->rxrearm_start];

	rxdp += rx_bufq->rxrearm_start;

	if (unlikely(!cache))
		return idpf_splitq_rearm_common(rx_bufq);

	/* We need to pull 'n' more MBUFs into the software ring from mempool
	 * We inline the mempool function here, so we can vectorize the copy
	 * from the cache into the shadow ring.
	 */

	/* Can this be satisfied from the cache? */
	if (cache->len < IDPF_RXQ_REARM_THRESH) {
		/* No. Backfill the cache first, and then fill from it */
		uint32_t req = IDPF_RXQ_REARM_THRESH + (cache->size -
							cache->len);

		/* How many do we require i.e. number to fill the cache + the request */
		int ret = rte_mempool_ops_dequeue_bulk
				(rx_bufq->mp, &cache->objs[cache->len], req);
		if (ret == 0) {
			cache->len += req;
		} else {
			if (rx_bufq->rxrearm_nb + IDPF_RXQ_REARM_THRESH >=
			    rx_bufq->nb_rx_desc) {
				__m128i dma_addr0;

				dma_addr0 = _mm_setzero_si128();
				for (i = 0; i < IDPF_VPMD_DESCS_PER_LOOP; i++) {
					rxp[i] = &rx_bufq->fake_mbuf;
					_mm_storeu_si128((__m128i *)&rxdp[i],
							 dma_addr0);
				}
			}
		__atomic_fetch_add(&rx_bufq->rx_stats.mbuf_alloc_failed,
				   IDPF_RXQ_REARM_THRESH, __ATOMIC_RELAXED);
			return;
		}
	}

	const __m512i iova_offsets =  _mm512_set1_epi64(offsetof
							(struct rte_mbuf, buf_iova));
	const __m512i headroom = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);

	/* Initialize the mbufs in vector, process 8 mbufs in one loop, taking
	 * from mempool cache and populating both shadow and HW rings
	 */
	for (i = 0; i < IDPF_RXQ_REARM_THRESH / IDPF_DESCS_PER_LOOP_AVX; i++) {
		const __m512i mbuf_ptrs = _mm512_loadu_si512
			(&cache->objs[cache->len - IDPF_DESCS_PER_LOOP_AVX]);
		_mm512_storeu_si512(rxp, mbuf_ptrs);

		const __m512i iova_base_addrs = _mm512_i64gather_epi64
				(_mm512_add_epi64(mbuf_ptrs, iova_offsets),
				 0, /* base */
				 1  /* scale */);
		const __m512i iova_addrs = _mm512_add_epi64(iova_base_addrs,
				headroom);

		const __m512i iova_addrs_1 = _mm512_bsrli_epi128(iova_addrs, 8);

		rxdp[0].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs, 0));
		rxdp[1].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs_1, 0));
		rxdp[2].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs, 1));
		rxdp[3].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs_1, 1));
		rxdp[4].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs, 2));
		rxdp[5].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs_1, 2));
		rxdp[6].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs, 3));
		rxdp[7].split_rd.pkt_addr =
			_mm_cvtsi128_si64(_mm512_extracti32x4_epi32(iova_addrs_1, 3));

		rxp += IDPF_DESCS_PER_LOOP_AVX;
		rxdp += IDPF_DESCS_PER_LOOP_AVX;
		cache->len -= IDPF_DESCS_PER_LOOP_AVX;
	}

	rx_bufq->rxrearm_start += IDPF_RXQ_REARM_THRESH;
	if (rx_bufq->rxrearm_start >= rx_bufq->nb_rx_desc)
		rx_bufq->rxrearm_start = 0;

	rx_bufq->rxrearm_nb -= IDPF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rx_bufq->rxrearm_start == 0) ?
			   (rx_bufq->nb_rx_desc - 1) : (rx_bufq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IDPF_PCI_REG_WRITE(rx_bufq->qrx_tail, rx_id);
}

static __rte_always_inline uint16_t
_idpf_splitq_recv_raw_pkts_avx512(struct idpf_rx_queue *rxq,
				  struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	const uint32_t *type_table = rxq->adapter->ptype_tbl;
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0, 0,
						    rxq->bufq2->mbuf_initializer);
	/* only handle bufq2 here */
	struct rte_mbuf **sw_ring = &rxq->bufq2->sw_ring[rxq->rx_tail];
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;

	rxdp += rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IDPF_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IDPF_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->bufq2->rxrearm_nb > IDPF_RXQ_REARM_THRESH)
		idpf_splitq_rearm(rxq->bufq2);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (((rxdp->flex_adv_nic_3_wb.pktlen_gen_bufq_id &
	      VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_M) >>
	     VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S) != rxq->expected_gen_id)
		return 0;

	const __m512i dd_check = _mm512_set1_epi64(1);
	const __m512i gen_check = _mm512_set1_epi64((uint64_t)1<<46);

	/* mask to shuffle from desc. to mbuf (4 descriptors)*/
	const __m512i shuf_msk =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0xFFFFFFFF,    /* octet 4~7, 32bits rss */
			 0xFFFF0504,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 2nd descriptor */
			 0xFFFFFFFF,    /* octet 4~7, 32bits rss */
			 0xFFFF0504,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 3rd descriptor */
			 0xFFFFFFFF,    /* octet 4~7, 32bits rss */
			 0xFFFF0504,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 4th descriptor */
			 0xFFFFFFFF,    /* octet 4~7, 32bits rss */
			 0xFFFF0504,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0504,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF     /* pkt_type set as unknown */
			);
	/**
	 * compile-time check the above crc and shuffle layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi
	 * calls above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IDPF_DESCS_PER_LOOP_AVX,
	     rxdp += IDPF_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m512i raw_desc0_3, raw_desc4_7;
		const __m128i raw_desc7 =
			_mm_load_si128((void *)(rxdp + 7));
		rte_compiler_barrier();
		const __m128i raw_desc6 =
			_mm_load_si128((void *)(rxdp + 6));
		rte_compiler_barrier();
		const __m128i raw_desc5 =
			_mm_load_si128((void *)(rxdp + 5));
		rte_compiler_barrier();
		const __m128i raw_desc4 =
			_mm_load_si128((void *)(rxdp + 4));
		rte_compiler_barrier();
		const __m128i raw_desc3 =
			_mm_load_si128((void *)(rxdp + 3));
		rte_compiler_barrier();
		const __m128i raw_desc2 =
			_mm_load_si128((void *)(rxdp + 2));
		rte_compiler_barrier();
		const __m128i raw_desc1 =
			_mm_load_si128((void *)(rxdp + 1));
		rte_compiler_barrier();
		const __m128i raw_desc0 =
			_mm_load_si128((void *)(rxdp + 0));

		raw_desc4_7 = _mm512_broadcast_i32x4(raw_desc4);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc5, 1);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc6, 2);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc7, 3);
		raw_desc0_3 = _mm512_broadcast_i32x4(raw_desc0);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc1, 1);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc2, 2);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc3, 3);

		/**
		 * convert descriptors 4-7 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len_mask = _mm512_set_epi32(0xffffffff, 0xffffffff,
							  0xffff3fff, 0xffffffff,
							  0xffffffff, 0xffffffff,
							  0xffff3fff, 0xffffffff,
							  0xffffffff, 0xffffffff,
							  0xffff3fff, 0xffffffff,
							  0xffffffff, 0xffffffff,
							  0xffff3fff, 0xffffffff);
		const __m512i desc4_7 = _mm512_and_epi32(raw_desc4_7, len_mask);
		__m512i mb4_7 = _mm512_shuffle_epi8(desc4_7, shuf_msk);

		/**
		 * to get packet types, shift 64-bit values down 30 bits
		 * and so ptype is in lower 8-bits in each
		 */
		const __m512i ptypes4_7 = _mm512_srli_epi64(desc4_7, 16);
		const __m256i ptypes6_7 = _mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 = _mm512_extracti64x4_epi64(ptypes4_7, 0);
		const uint8_t ptype7 = _mm256_extract_epi8(ptypes6_7, 16);
		const uint8_t ptype6 = _mm256_extract_epi8(ptypes6_7, 0);
		const uint8_t ptype5 = _mm256_extract_epi8(ptypes4_5, 16);
		const uint8_t ptype4 = _mm256_extract_epi8(ptypes4_5, 0);

		const __m512i ptype4_7 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype7],
			 0, 0, 0, type_table[ptype6],
			 0, 0, 0, type_table[ptype5],
			 0, 0, 0, type_table[ptype4]);
		mb4_7 = _mm512_mask_blend_epi32(0x1111, mb4_7, ptype4_7);

		/**
		 * convert descriptors 0-3 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i desc0_3 = _mm512_and_epi32(raw_desc0_3, len_mask);
		__m512i mb0_3 = _mm512_shuffle_epi8(desc0_3, shuf_msk);

		/* get the packet types */
		const __m512i ptypes0_3 = _mm512_srli_epi64(desc0_3, 16);
		const __m256i ptypes2_3 = _mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 = _mm512_extracti64x4_epi64(ptypes0_3, 0);
		const uint8_t ptype3 = _mm256_extract_epi8(ptypes2_3, 16);
		const uint8_t ptype2 = _mm256_extract_epi8(ptypes2_3, 0);
		const uint8_t ptype1 = _mm256_extract_epi8(ptypes0_1, 16);
		const uint8_t ptype0 = _mm256_extract_epi8(ptypes0_1, 0);

		const __m512i ptype0_3 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype3],
			 0, 0, 0, type_table[ptype2],
			 0, 0, 0, type_table[ptype1],
			 0, 0, 0, type_table[ptype0]);
		mb0_3 = _mm512_mask_blend_epi32(0x1111, mb0_3, ptype0_3);

		/**
		 * use permute/extract to get status and generation bit content
		 * After the operations, the packets status flags are in the
		 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
		 */

		const __m512i dd_permute_msk = _mm512_set_epi64
			(11, 15, 3, 7, 9, 13, 1, 5);
		const __m512i status0_7 = _mm512_permutex2var_epi64
			(raw_desc4_7, dd_permute_msk, raw_desc0_3);
		const __m512i gen_permute_msk = _mm512_set_epi64
			(10, 14, 2, 6, 8, 12, 0, 4);
		const __m512i raw_gen0_7 = _mm512_permutex2var_epi64
			(raw_desc4_7, gen_permute_msk, raw_desc0_3);

		/* now do flag manipulation */

		/**
		 * At this point, we have the 8 sets of flags in the low 16-bits
		 * of each 32-bit value in vlan0.
		 * We want to extract these, and merge them with the mbuf init
		 * data so we can do a single write to the mbuf to set the flags
		 * and all the other initialization fields. Extracting the
		 * appropriate flags means that we have to do a shift and blend
		 * for each mbuf before we do the write. However, we can also
		 * add in the previously computed rx_descriptor fields to
		 * make a single 256-bit write per mbuf
		 */
		/* check the structure matches expectations */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
				 offsetof(struct rte_mbuf, rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
				 RTE_ALIGN(offsetof(struct rte_mbuf,
						    rearm_data),
						    16));
				/* build up data and do writes */
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5,
			rearm6, rearm7;
		const __m256i mb4_5 = _mm512_extracti64x4_epi64(mb4_7, 0);
		const __m256i mb6_7 = _mm512_extracti64x4_epi64(mb4_7, 1);
		const __m256i mb0_1 = _mm512_extracti64x4_epi64(mb0_3, 0);
		const __m256i mb2_3 = _mm512_extracti64x4_epi64(mb0_3, 1);

		rearm6 = _mm256_permute2f128_si256(mbuf_init, mb6_7, 0x20);
		rearm4 = _mm256_permute2f128_si256(mbuf_init, mb4_5, 0x20);
		rearm2 = _mm256_permute2f128_si256(mbuf_init, mb2_3, 0x20);
		rearm0 = _mm256_permute2f128_si256(mbuf_init, mb0_1, 0x20);

		/* write to mbuf */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);

		rearm7 = _mm256_blend_epi32(mbuf_init, mb6_7, 0xF0);
		rearm5 = _mm256_blend_epi32(mbuf_init, mb4_5, 0xF0);
		rearm3 = _mm256_blend_epi32(mbuf_init, mb2_3, 0xF0);
		rearm1 = _mm256_blend_epi32(mbuf_init, mb0_1, 0xF0);

		/* again write to mbufs */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);

		const __mmask8 dd_mask = _mm512_cmpeq_epi64_mask(
			_mm512_and_epi64(status0_7, dd_check), dd_check);
		const __mmask8 gen_mask = _mm512_cmpeq_epi64_mask(
			_mm512_and_epi64(raw_gen0_7, gen_check),
			_mm512_set1_epi64((uint64_t)rxq->expected_gen_id << 46));
		const __mmask8 recv_mask = _kand_mask8(dd_mask, gen_mask);
		uint16_t burst = __builtin_popcount(_cvtmask8_u32(recv_mask));

		received += burst;
		if (burst != IDPF_DESCS_PER_LOOP_AVX)
			break;
	}

	/* update tail pointers */
	rxq->rx_tail += received;
	rxq->expected_gen_id ^= ((rxq->rx_tail & rxq->nb_rx_desc) != 0);
	rxq->rx_tail &= (rxq->nb_rx_desc - 1);
	if ((rxq->rx_tail & 1) == 1 && received > 1) { /* keep aligned */
		rxq->rx_tail--;
		received--;
	}

	rxq->bufq2->rxrearm_nb += received;
	return received;
}

/* only bufq2 can receive pkts */
uint16_t
idpf_dp_splitq_recv_pkts_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	return _idpf_splitq_recv_raw_pkts_avx512(rx_queue, rx_pkts,
						 nb_pkts);
}

static __rte_always_inline int
idpf_tx_singleq_free_bufs_avx512(struct idpf_tx_queue *txq)
{
	struct idpf_tx_vec_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[txq->rs_thresh];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->next_dd].qw1.cmd_dtype &
			rte_cpu_to_le_64(IDPF_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_64(IDPF_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	n = txq->rs_thresh;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = (void *)txq->sw_ring;
	txep += txq->next_dd - (n - 1);

	if (txq->offloads & IDPF_TX_OFFLOAD_MBUF_FAST_FREE && (n & 31) == 0) {
		struct rte_mempool *mp = txep[0].mbuf->pool;
		struct rte_mempool_cache *cache = rte_mempool_default_cache(mp,
								rte_lcore_id());
		void **cache_objs;

		if (cache == NULL || cache->len == 0)
			goto normal;

		cache_objs = &cache->objs[cache->len];

		if (n > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			rte_mempool_ops_enqueue_bulk(mp, (void *)txep, n);
			goto done;
		}

		/* The cache follows the following algorithm
		 *   1. Add the objects to the cache
		 *   2. Anything greater than the cache min value (if it crosses the
		 *   cache flush threshold) is flushed to the ring.
		 */
		/* Add elements back into the cache */
		uint32_t copied = 0;
		/* n is multiple of 32 */
		while (copied < n) {
			const __m512i a = _mm512_loadu_si512(&txep[copied]);
			const __m512i b = _mm512_loadu_si512(&txep[copied + 8]);
			const __m512i c = _mm512_loadu_si512(&txep[copied + 16]);
			const __m512i d = _mm512_loadu_si512(&txep[copied + 24]);

			_mm512_storeu_si512(&cache_objs[copied], a);
			_mm512_storeu_si512(&cache_objs[copied + 8], b);
			_mm512_storeu_si512(&cache_objs[copied + 16], c);
			_mm512_storeu_si512(&cache_objs[copied + 24], d);
			copied += 32;
		}
		cache->len += n;

		if (cache->len >= cache->flushthresh) {
			rte_mempool_ops_enqueue_bulk(mp,
						     &cache->objs[cache->size],
						     cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

normal:
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool)) {
					free[nb_free++] = m;
				} else {
					rte_mempool_put_bulk(free[0]->pool,
							     (void *)free,
							     nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
	} else {
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
		}
	}

done:
	/* buffers were freed, update counters */
	txq->nb_free = (uint16_t)(txq->nb_free + txq->rs_thresh);
	txq->next_dd = (uint16_t)(txq->next_dd + txq->rs_thresh);
	if (txq->next_dd >= txq->nb_tx_desc)
		txq->next_dd = (uint16_t)(txq->rs_thresh - 1);

	return txq->rs_thresh;
}

static __rte_always_inline void
tx_backlog_entry_avx512(struct idpf_tx_vec_entry *txep,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

#define IDPF_FLEX_TXD_QW1_BUF_SZ_S 48
static __rte_always_inline void
idpf_singleq_vtx1(volatile struct idpf_flex_tx_desc *txdp,
	  struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw =
		(IDPF_TX_DESC_DTYPE_FLEX_DATA << IDPF_FLEX_TXD_QW1_DTYPE_S |
		 ((uint64_t)flags  << IDPF_FLEX_TXD_QW1_CMD_S) |
		 ((uint64_t)pkt->data_len << IDPF_FLEX_TXD_QW1_BUF_SZ_S));

	__m128i descriptor = _mm_set_epi64x(high_qw,
					    pkt->buf_iova + pkt->data_off);
	_mm_storeu_si128((__m128i *)txdp, descriptor);
}

#define IDPF_TX_LEN_MASK 0xAA
#define IDPF_TX_OFF_MASK 0x55
static __rte_always_inline void
idpf_singleq_vtx(volatile struct idpf_flex_tx_desc *txdp,
	 struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	const uint64_t hi_qw_tmpl = (IDPF_TX_DESC_DTYPE_FLEX_DATA  |
			((uint64_t)flags  << IDPF_FLEX_TXD_QW1_CMD_S));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		idpf_singleq_vtx1(txdp, *pkt, flags);
		nb_pkts--, txdp++, pkt++;
	}

	/* do 4 at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 IDPF_FLEX_TXD_QW1_BUF_SZ_S);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 IDPF_FLEX_TXD_QW1_BUF_SZ_S);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 IDPF_FLEX_TXD_QW1_BUF_SZ_S);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 IDPF_FLEX_TXD_QW1_BUF_SZ_S);

		__m512i desc0_3 =
			_mm512_set_epi64
				(hi_qw3,
				 pkt[3]->buf_iova + pkt[3]->data_off,
				 hi_qw2,
				 pkt[2]->buf_iova + pkt[2]->data_off,
				 hi_qw1,
				 pkt[1]->buf_iova + pkt[1]->data_off,
				 hi_qw0,
				 pkt[0]->buf_iova + pkt[0]->data_off);
		_mm512_storeu_si512((void *)txdp, desc0_3);
	}

	/* do any last ones */
	while (nb_pkts) {
		idpf_singleq_vtx1(txdp, *pkt, flags);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline uint16_t
idpf_singleq_xmit_fixed_burst_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
					 uint16_t nb_pkts)
{
	struct idpf_tx_queue *txq = tx_queue;
	volatile struct idpf_flex_tx_desc *txdp;
	struct idpf_tx_vec_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = IDPF_TX_FLEX_DESC_CMD_EOP;
	uint64_t rs = IDPF_TX_FLEX_DESC_CMD_RS | flags;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->rs_thresh);

	if (txq->nb_free < txq->free_thresh)
		idpf_tx_singleq_free_bufs_avx512(txq);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts);
	nb_commit = nb_pkts;
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = (void *)txq->sw_ring;
	txep += tx_id;

	txq->nb_free = (uint16_t)(txq->nb_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry_avx512(txep, tx_pkts, n);

		idpf_singleq_vtx(txdp, tx_pkts, n - 1, flags);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		idpf_singleq_vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = (void *)txq->sw_ring;
		txep += tx_id;
	}

	tx_backlog_entry_avx512(txep, tx_pkts, nb_commit);

	idpf_singleq_vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->next_rs) {
		txq->tx_ring[txq->next_rs].qw1.cmd_dtype |=
			rte_cpu_to_le_64(((uint64_t)IDPF_TX_FLEX_DESC_CMD_RS) <<
					 IDPF_FLEX_TXD_QW1_CMD_S);
		txq->next_rs =
			(uint16_t)(txq->next_rs + txq->rs_thresh);
	}

	txq->tx_tail = tx_id;

	IDPF_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
idpf_singleq_xmit_pkts_vec_avx512_cmn(void *tx_queue, struct rte_mbuf **tx_pkts,
			      uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct idpf_tx_queue *txq = tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->rs_thresh);
		ret = idpf_singleq_xmit_fixed_burst_vec_avx512(tx_queue, &tx_pkts[nb_tx],
						       num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
idpf_dp_singleq_xmit_pkts_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				 uint16_t nb_pkts)
{
	return idpf_singleq_xmit_pkts_vec_avx512_cmn(tx_queue, tx_pkts, nb_pkts);
}

static __rte_always_inline void
idpf_splitq_scan_cq_ring(struct idpf_tx_queue *cq)
{
	struct idpf_splitq_tx_compl_desc *compl_ring;
	struct idpf_tx_queue *txq;
	uint16_t genid, txq_qid, cq_qid, i;
	uint8_t ctype;

	cq_qid = cq->tx_tail;

	for (i = 0; i < IDPD_TXQ_SCAN_CQ_THRESH; i++) {
		if (cq_qid == cq->nb_tx_desc) {
			cq_qid = 0;
			cq->expected_gen_id ^= 1;
		}
		compl_ring = &cq->compl_ring[cq_qid];
		genid = (compl_ring->qid_comptype_gen &
			rte_cpu_to_le_64(IDPF_TXD_COMPLQ_GEN_M)) >> IDPF_TXD_COMPLQ_GEN_S;
		if (genid != cq->expected_gen_id)
			break;
		ctype = (rte_le_to_cpu_16(compl_ring->qid_comptype_gen) &
			IDPF_TXD_COMPLQ_COMPL_TYPE_M) >> IDPF_TXD_COMPLQ_COMPL_TYPE_S;
		txq_qid = (rte_le_to_cpu_16(compl_ring->qid_comptype_gen) &
			IDPF_TXD_COMPLQ_QID_M) >> IDPF_TXD_COMPLQ_QID_S;
		txq = cq->txqs[txq_qid - cq->tx_start_qid];
		txq->ctype[ctype]++;
		cq_qid++;
	}

	cq->tx_tail = cq_qid;
}

static __rte_always_inline int
idpf_tx_splitq_free_bufs_avx512(struct idpf_tx_queue *txq)
{
	struct idpf_tx_vec_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[txq->rs_thresh];

	n = txq->rs_thresh;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = (void *)txq->sw_ring;
	txep += txq->next_dd - (n - 1);

	if (txq->offloads & IDPF_TX_OFFLOAD_MBUF_FAST_FREE && (n & 31) == 0) {
		struct rte_mempool *mp = txep[0].mbuf->pool;
		struct rte_mempool_cache *cache = rte_mempool_default_cache(mp,
								rte_lcore_id());
		void **cache_objs;

		if (!cache || cache->len == 0)
			goto normal;

		cache_objs = &cache->objs[cache->len];

		if (n > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			rte_mempool_ops_enqueue_bulk(mp, (void *)txep, n);
			goto done;
		}

		/* The cache follows the following algorithm
		 *   1. Add the objects to the cache
		 *   2. Anything greater than the cache min value (if it crosses the
		 *   cache flush threshold) is flushed to the ring.
		 */
		/* Add elements back into the cache */
		uint32_t copied = 0;
		/* n is multiple of 32 */
		while (copied < n) {
			const __m512i a = _mm512_loadu_si512(&txep[copied]);
			const __m512i b = _mm512_loadu_si512(&txep[copied + 8]);
			const __m512i c = _mm512_loadu_si512(&txep[copied + 16]);
			const __m512i d = _mm512_loadu_si512(&txep[copied + 24]);

			_mm512_storeu_si512(&cache_objs[copied], a);
			_mm512_storeu_si512(&cache_objs[copied + 8], b);
			_mm512_storeu_si512(&cache_objs[copied + 16], c);
			_mm512_storeu_si512(&cache_objs[copied + 24], d);
			copied += 32;
		}
		cache->len += n;

		if (cache->len >= cache->flushthresh) {
			rte_mempool_ops_enqueue_bulk(mp,
						     &cache->objs[cache->size],
						     cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

normal:
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m)) {
				if (likely(m->pool == free[0]->pool)) {
					free[nb_free++] = m;
				} else {
					rte_mempool_put_bulk(free[0]->pool,
							     (void *)free,
							     nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
	} else {
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m)
				rte_mempool_put(m->pool, m);
		}
	}

done:
	/* buffers were freed, update counters */
	txq->nb_free = (uint16_t)(txq->nb_free + txq->rs_thresh);
	txq->next_dd = (uint16_t)(txq->next_dd + txq->rs_thresh);
	if (txq->next_dd >= txq->nb_tx_desc)
		txq->next_dd = (uint16_t)(txq->rs_thresh - 1);
	txq->ctype[IDPF_TXD_COMPLT_RS] -= txq->rs_thresh;

	return txq->rs_thresh;
}

#define IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S	48

static __rte_always_inline void
idpf_splitq_vtx1(volatile struct idpf_flex_tx_sched_desc *txdp,
	  struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw =
		(IDPF_TX_DESC_DTYPE_FLEX_FLOW_SCHE |
		 ((uint64_t)flags) |
		 ((uint64_t)pkt->data_len << IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S));

	__m128i descriptor = _mm_set_epi64x(high_qw,
					    pkt->buf_iova + pkt->data_off);
	_mm_storeu_si128((__m128i *)txdp, descriptor);
}

static __rte_always_inline void
idpf_splitq_vtx(volatile struct idpf_flex_tx_sched_desc *txdp,
	 struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	const uint64_t hi_qw_tmpl = (IDPF_TX_DESC_DTYPE_FLEX_FLOW_SCHE  |
			((uint64_t)flags));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		idpf_splitq_vtx1(txdp, *pkt, flags);
		nb_pkts--, txdp++, pkt++;
	}

	/* do 4 at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 IDPF_TXD_FLEX_QW1_TX_BUF_SZ_S);

		__m512i desc0_3 =
			_mm512_set_epi64
				(hi_qw3,
				 pkt[3]->buf_iova + pkt[3]->data_off,
				 hi_qw2,
				 pkt[2]->buf_iova + pkt[2]->data_off,
				 hi_qw1,
				 pkt[1]->buf_iova + pkt[1]->data_off,
				 hi_qw0,
				 pkt[0]->buf_iova + pkt[0]->data_off);
		_mm512_storeu_si512((void *)txdp, desc0_3);
	}

	/* do any last ones */
	while (nb_pkts) {
		idpf_splitq_vtx1(txdp, *pkt, flags);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline uint16_t
idpf_splitq_xmit_fixed_burst_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
					uint16_t nb_pkts)
{
	struct idpf_tx_queue *txq = (struct idpf_tx_queue *)tx_queue;
	volatile struct idpf_flex_tx_sched_desc *txdp;
	struct idpf_tx_vec_entry *txep;
	uint16_t n, nb_commit, tx_id;
	/* bit2 is reserved and must be set to 1 according to Spec */
	uint64_t cmd_dtype = IDPF_TXD_FLEX_FLOW_CMD_EOP;

	tx_id = txq->tx_tail;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->rs_thresh);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->desc_ring[tx_id];
	txep = (void *)txq->sw_ring;
	txep += tx_id;

	txq->nb_free = (uint16_t)(txq->nb_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry_avx512(txep, tx_pkts, n);

		idpf_splitq_vtx((void *)txdp, tx_pkts, n - 1, cmd_dtype);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		idpf_splitq_vtx1((void *)txdp, *tx_pkts++, cmd_dtype);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->desc_ring[tx_id];
		txep = (void *)txq->sw_ring;
		txep += tx_id;
	}

	tx_backlog_entry_avx512(txep, tx_pkts, nb_commit);

	idpf_splitq_vtx((void *)txdp, tx_pkts, nb_commit, cmd_dtype);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->next_rs)
		txq->next_rs =
			(uint16_t)(txq->next_rs + txq->rs_thresh);

	txq->tx_tail = tx_id;

	IDPF_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
idpf_splitq_xmit_pkts_vec_avx512_cmn(void *tx_queue, struct rte_mbuf **tx_pkts,
				     uint16_t nb_pkts)
{
	struct idpf_tx_queue *txq = (struct idpf_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	while (nb_pkts) {
		uint16_t ret, num;

		idpf_splitq_scan_cq_ring(txq->complq);

		if (txq->ctype[IDPF_TXD_COMPLT_RS] > txq->free_thresh)
			idpf_tx_splitq_free_bufs_avx512(txq);

		num = (uint16_t)RTE_MIN(nb_pkts, txq->rs_thresh);
		ret = idpf_splitq_xmit_fixed_burst_vec_avx512(tx_queue,
							      &tx_pkts[nb_tx],
							      num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
idpf_dp_splitq_xmit_pkts_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	return idpf_splitq_xmit_pkts_vec_avx512_cmn(tx_queue, tx_pkts, nb_pkts);
}

static inline void
idpf_tx_release_mbufs_avx512(struct idpf_tx_queue *txq)
{
	unsigned int i;
	const uint16_t max_desc = (uint16_t)(txq->nb_tx_desc - 1);
	struct idpf_tx_vec_entry *swr = (void *)txq->sw_ring;

	if (txq->sw_ring == NULL || txq->nb_free == max_desc)
		return;

	i = txq->next_dd - txq->rs_thresh + 1;
	if (txq->tx_tail < i) {
		for (; i < txq->nb_tx_desc; i++) {
			rte_pktmbuf_free_seg(swr[i].mbuf);
			swr[i].mbuf = NULL;
		}
		i = 0;
	}
}

static const struct idpf_txq_ops avx512_tx_vec_ops = {
	.release_mbufs = idpf_tx_release_mbufs_avx512,
};

int __rte_cold
idpf_qc_tx_vec_avx512_setup(struct idpf_tx_queue *txq)
{
	if (!txq)
		return 0;

	txq->ops = &avx512_tx_vec_ops;
	return 0;
}
