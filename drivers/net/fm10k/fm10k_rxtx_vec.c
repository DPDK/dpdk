/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2015 Intel Corporation
 */

#include <inttypes.h>

#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include "fm10k.h"
#include "base/fm10k_type.h"

#include <tmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static void
fm10k_reset_tx_queue(struct fm10k_tx_queue *txq);

/* Handling the offload flags (olflags) field takes computation
 * time when receiving packets. Therefore we provide a flag to disable
 * the processing of the olflags field when they are not needed. This
 * gives improved performance, at the cost of losing the offload info
 * in the received packet
 */
#ifdef RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE

/* Vlan present flag shift */
#define VP_SHIFT     (2)
/* L3 type shift */
#define L3TYPE_SHIFT     (4)
/* L4 type shift */
#define L4TYPE_SHIFT     (7)
/* HBO flag shift */
#define HBOFLAG_SHIFT     (10)
/* RXE flag shift */
#define RXEFLAG_SHIFT     (13)
/* IPE/L4E flag shift */
#define L3L4EFLAG_SHIFT     (14)
/* shift PKT_RX_L4_CKSUM_GOOD into one byte by 1 bit */
#define CKSUM_SHIFT     (1)

static inline void
fm10k_desc_to_olflags_v(__m128i descs[4], struct rte_mbuf **rx_pkts)
{
	__m128i ptype0, ptype1, vtag0, vtag1, eflag0, eflag1, cksumflag;
	union {
		uint16_t e[4];
		uint64_t dword;
	} vol;

	const __m128i pkttype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			PKT_RX_VLAN, PKT_RX_VLAN,
			PKT_RX_VLAN, PKT_RX_VLAN);

	/* mask everything except rss type */
	const __m128i rsstype_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x000F, 0x000F, 0x000F, 0x000F);

	/* mask for HBO and RXE flag flags */
	const __m128i rxe_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x0001, 0x0001, 0x0001, 0x0001);

	/* mask the lower byte of ol_flags */
	const __m128i ol_flags_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x00FF, 0x00FF, 0x00FF, 0x00FF);

	const __m128i l3l4cksum_flag = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			(PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD) >> CKSUM_SHIFT,
			(PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_GOOD) >> CKSUM_SHIFT,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD) >> CKSUM_SHIFT,
			(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD) >> CKSUM_SHIFT);

	const __m128i rxe_flag = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0);

	/* map rss type to rss hash flag */
	const __m128i rss_flags = _mm_set_epi8(0, 0, 0, 0,
			0, 0, 0, PKT_RX_RSS_HASH,
			PKT_RX_RSS_HASH, 0, PKT_RX_RSS_HASH, 0,
			PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, 0);

	/* Calculate RSS_hash and Vlan fields */
	ptype0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	ptype1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	vtag0 = _mm_unpackhi_epi16(descs[0], descs[1]);
	vtag1 = _mm_unpackhi_epi16(descs[2], descs[3]);

	ptype0 = _mm_unpacklo_epi32(ptype0, ptype1);
	ptype0 = _mm_and_si128(ptype0, rsstype_msk);
	ptype0 = _mm_shuffle_epi8(rss_flags, ptype0);

	vtag1 = _mm_unpacklo_epi32(vtag0, vtag1);
	eflag0 = vtag1;
	cksumflag = vtag1;
	vtag1 = _mm_srli_epi16(vtag1, VP_SHIFT);
	vtag1 = _mm_and_si128(vtag1, pkttype_msk);

	vtag1 = _mm_or_si128(ptype0, vtag1);

	/* Process err flags, simply set RECIP_ERR bit if HBO/IXE is set */
	eflag1 = _mm_srli_epi16(eflag0, RXEFLAG_SHIFT);
	eflag0 = _mm_srli_epi16(eflag0, HBOFLAG_SHIFT);
	eflag0 = _mm_or_si128(eflag0, eflag1);
	eflag0 = _mm_and_si128(eflag0, rxe_msk);
	eflag0 = _mm_shuffle_epi8(rxe_flag, eflag0);

	vtag1 = _mm_or_si128(eflag0, vtag1);

	/* Process L4/L3 checksum error flags */
	cksumflag = _mm_srli_epi16(cksumflag, L3L4EFLAG_SHIFT);
	cksumflag = _mm_shuffle_epi8(l3l4cksum_flag, cksumflag);

	/* clean the higher byte and shift back the flag bits */
	cksumflag = _mm_and_si128(cksumflag, ol_flags_msk);
	cksumflag = _mm_slli_epi16(cksumflag, CKSUM_SHIFT);
	vtag1 = _mm_or_si128(cksumflag, vtag1);

	vol.dword = _mm_cvtsi128_si64(vtag1);

	rx_pkts[0]->ol_flags = vol.e[0];
	rx_pkts[1]->ol_flags = vol.e[1];
	rx_pkts[2]->ol_flags = vol.e[2];
	rx_pkts[3]->ol_flags = vol.e[3];
}

/* @note: When this function is changed, make corresponding change to
 * fm10k_dev_supported_ptypes_get().
 */
static inline void
fm10k_desc_to_pktype_v(__m128i descs[4], struct rte_mbuf **rx_pkts)
{
	__m128i l3l4type0, l3l4type1, l3type, l4type;
	union {
		uint16_t e[4];
		uint64_t dword;
	} vol;

	/* L3 pkt type mask  Bit4 to Bit6 */
	const __m128i l3type_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x0070, 0x0070, 0x0070, 0x0070);

	/* L4 pkt type mask  Bit7 to Bit9 */
	const __m128i l4type_msk = _mm_set_epi16(
			0x0000, 0x0000, 0x0000, 0x0000,
			0x0380, 0x0380, 0x0380, 0x0380);

	/* convert RRC l3 type to mbuf format */
	const __m128i l3type_flags = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, RTE_PTYPE_L3_IPV6_EXT,
			RTE_PTYPE_L3_IPV6, RTE_PTYPE_L3_IPV4_EXT,
			RTE_PTYPE_L3_IPV4, 0);

	/* Convert RRC l4 type to mbuf format l4type_flags shift-left 8 bits
	 * to fill into8 bits length.
	 */
	const __m128i l4type_flags = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0,
			RTE_PTYPE_TUNNEL_GENEVE >> 8,
			RTE_PTYPE_TUNNEL_NVGRE >> 8,
			RTE_PTYPE_TUNNEL_VXLAN >> 8,
			RTE_PTYPE_TUNNEL_GRE >> 8,
			RTE_PTYPE_L4_UDP >> 8,
			RTE_PTYPE_L4_TCP >> 8,
			0);

	l3l4type0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	l3l4type1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	l3l4type0 = _mm_unpacklo_epi32(l3l4type0, l3l4type1);

	l3type = _mm_and_si128(l3l4type0, l3type_msk);
	l4type = _mm_and_si128(l3l4type0, l4type_msk);

	l3type = _mm_srli_epi16(l3type, L3TYPE_SHIFT);
	l4type = _mm_srli_epi16(l4type, L4TYPE_SHIFT);

	l3type = _mm_shuffle_epi8(l3type_flags, l3type);
	/* l4type_flags shift-left for 8 bits, need shift-right back */
	l4type = _mm_shuffle_epi8(l4type_flags, l4type);

	l4type = _mm_slli_epi16(l4type, 8);
	l3l4type0 = _mm_or_si128(l3type, l4type);
	vol.dword = _mm_cvtsi128_si64(l3l4type0);

	rx_pkts[0]->packet_type = vol.e[0];
	rx_pkts[1]->packet_type = vol.e[1];
	rx_pkts[2]->packet_type = vol.e[2];
	rx_pkts[3]->packet_type = vol.e[3];
}
#else
#define fm10k_desc_to_olflags_v(desc, rx_pkts) do {} while (0)
#define fm10k_desc_to_pktype_v(desc, rx_pkts) do {} while (0)
#endif

int __attribute__((cold))
fm10k_rx_vec_condition_check(struct rte_eth_dev *dev)
{
#ifndef RTE_LIBRTE_IEEE1588
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct rte_fdir_conf *fconf = &dev->data->dev_conf.fdir_conf;

#ifndef RTE_FM10K_RX_OLFLAGS_ENABLE
	/* whithout rx ol_flags, no VP flag report */
	if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
		return -1;
#endif

	/* no fdir support */
	if (fconf->mode != RTE_FDIR_MODE_NONE)
		return -1;

	/* no header split support */
	if (rxmode->offloads & DEV_RX_OFFLOAD_HEADER_SPLIT)
		return -1;

	return 0;
#else
	RTE_SET_USED(dev);
	return -1;
#endif
}

int __attribute__((cold))
fm10k_rxq_vec_setup(struct fm10k_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	/* data_off will be ajusted after new mbuf allocated for 512-byte
	 * alignment.
	 */
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	return 0;
}

static inline void
fm10k_rxq_rearm(struct fm10k_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union fm10k_rx_desc *rxdp;
	struct rte_mbuf **mb_alloc = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i head_off = _mm_set_epi64x(
			RTE_PKTMBUF_HEADROOM + FM10K_RX_DATABUF_ALIGN - 1,
			RTE_PKTMBUF_HEADROOM + FM10K_RX_DATABUF_ALIGN - 1);
	__m128i dma_addr0, dma_addr1;
	/* Rx buffer need to be aligned with 512 byte */
	const __m128i hba_msk = _mm_set_epi64x(0,
				UINT64_MAX - FM10K_RX_DATABUF_ALIGN + 1);

	rxdp = rxq->hw_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)mb_alloc,
				 RTE_FM10K_RXQ_REARM_THRESH) < 0) {
		dma_addr0 = _mm_setzero_si128();
		/* Clean up all the HW/SW ring content */
		for (i = 0; i < RTE_FM10K_RXQ_REARM_THRESH; i++) {
			mb_alloc[i] = &rxq->fake_mbuf;
			_mm_store_si128((__m128i *)&rxdp[i].q,
						dma_addr0);
		}

		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_FM10K_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_FM10K_RXQ_REARM_THRESH; i += 2, mb_alloc += 2) {
		__m128i vaddr0, vaddr1;
		uintptr_t p0, p1;

		mb0 = mb_alloc[0];
		mb1 = mb_alloc[1];

		/* Flush mbuf with pkt template.
		 * Data to be rearmed is 6 bytes long.
		 */
		p0 = (uintptr_t)&mb0->rearm_data;
		*(uint64_t *)p0 = rxq->mbuf_initializer;
		p1 = (uintptr_t)&mb1->rearm_data;
		*(uint64_t *)p1 = rxq->mbuf_initializer;

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
		vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = _mm_add_epi64(dma_addr0, head_off);
		dma_addr1 = _mm_add_epi64(dma_addr1, head_off);

		/* Do 512 byte alignment to satisfy HW requirement, in the
		 * meanwhile, set Header Buffer Address to zero.
		 */
		dma_addr0 = _mm_and_si128(dma_addr0, hba_msk);
		dma_addr1 = _mm_and_si128(dma_addr1, hba_msk);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->q, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->q, dma_addr1);

		/* enforce 512B alignment on default Rx virtual addresses */
		mb0->data_off = (uint16_t)(RTE_PTR_ALIGN((char *)mb0->buf_addr
				+ RTE_PKTMBUF_HEADROOM, FM10K_RX_DATABUF_ALIGN)
				- (char *)mb0->buf_addr);
		mb1->data_off = (uint16_t)(RTE_PTR_ALIGN((char *)mb1->buf_addr
				+ RTE_PKTMBUF_HEADROOM, FM10K_RX_DATABUF_ALIGN)
				- (char *)mb1->buf_addr);
	}

	rxq->rxrearm_start += RTE_FM10K_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_FM10K_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			(rxq->nb_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	FM10K_PCI_REG_WRITE(rxq->tail_ptr, rx_id);
}

void __attribute__((cold))
fm10k_rx_queue_release_mbufs_vec(struct fm10k_rx_queue *rxq)
{
	const unsigned mask = rxq->nb_desc - 1;
	unsigned i;

	if (rxq->sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_desc)
		return;

	/* free all mbufs that are valid in the ring */
	for (i = rxq->next_dd; i != rxq->rxrearm_start; i = (i + 1) & mask)
		rte_pktmbuf_free_seg(rxq->sw_ring[i]);
	rxq->rxrearm_nb = rxq->nb_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_desc);
}

static inline uint16_t
fm10k_recv_raw_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union fm10k_rx_desc *rxdp;
	struct rte_mbuf **mbufp;
	uint16_t nb_pkts_recd;
	int pos;
	struct fm10k_rx_queue *rxq = rx_queue;
	uint64_t var;
	__m128i shuf_msk;
	__m128i dd_check, eop_check;
	uint16_t next_dd;

	next_dd = rxq->next_dd;

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->hw_ring + next_dd;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_FM10K_RXQ_REARM_THRESH)
		fm10k_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->d.staterr & FM10K_RXD_STATUS_DD))
		return 0;

	/* Vecotr RX will process 4 packets at a time, strip the unaligned
	 * tails in case it's not multiple of 4.
	 */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_FM10K_DESCS_PER_LOOP);

	/* 4 packets DD mask */
	dd_check = _mm_set_epi64x(0x0000000100000001LL, 0x0000000100000001LL);

	/* 4 packets EOP mask */
	eop_check = _mm_set_epi64x(0x0000000200000002LL, 0x0000000200000002LL);

	/* mask to shuffle from desc. to mbuf */
	shuf_msk = _mm_set_epi8(
		7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		15, 14,      /* octet 14~15, low 16 bits vlan_macip */
		13, 12,      /* octet 12~13, 16 bits data_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		13, 12,      /* octet 12~13, low 16 bits pkt_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_type */
		0xFF, 0xFF   /* Skip pkt_type field in shuffle operation */
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
	mbufp = &rxq->sw_ring[next_dd];

	/* A. load 4 packet in one loop
	 * [A*. mask out 4 unused dirty field in desc]
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */
	for (pos = 0, nb_pkts_recd = 0; pos < nb_pkts;
			pos += RTE_FM10K_DESCS_PER_LOOP,
			rxdp += RTE_FM10K_DESCS_PER_LOOP) {
		__m128i descs0[RTE_FM10K_DESCS_PER_LOOP];
		__m128i pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		__m128i zero, staterr, sterr_tmp1, sterr_tmp2;
		__m128i mbp1;
		/* 2 64 bit or 4 32 bit mbuf pointers in one XMM reg. */
#if defined(RTE_ARCH_X86_64)
		__m128i mbp2;
#endif

		/* B.1 load 2 (64 bit) or 4 (32 bit) mbuf points */
		mbp1 = _mm_loadu_si128((__m128i *)&mbufp[pos]);

		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load 4 pkts desc */
		descs0[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));
		rte_compiler_barrier();

		/* B.2 copy 2 64 bit or 4 32 bit mbuf point into rx_pkts */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos], mbp1);

#if defined(RTE_ARCH_X86_64)
		/* B.1 load 2 64 bit mbuf poitns */
		mbp2 = _mm_loadu_si128((__m128i *)&mbufp[pos+2]);
#endif

		descs0[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		rte_compiler_barrier();
		/* B.1 load 2 mbuf point */
		descs0[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		rte_compiler_barrier();
		descs0[0] = _mm_loadu_si128((__m128i *)(rxdp));

#if defined(RTE_ARCH_X86_64)
		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_storeu_si128((__m128i *)&rx_pkts[pos+2], mbp2);
#endif

		/* avoid compiler reorder optimization */
		rte_compiler_barrier();

		if (split_packet) {
			rte_mbuf_prefetch_part2(rx_pkts[pos]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 1]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 2]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 3]);
		}

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs0[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs0[2], shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs0[3], descs0[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs0[1], descs0[0]);

		/* set ol_flags with vlan packet type */
		fm10k_desc_to_olflags_v(descs0, &rx_pkts[pos]);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = _mm_shuffle_epi8(descs0[1], shuf_msk);
		pkt_mb1 = _mm_shuffle_epi8(descs0[0], shuf_msk);

		/* C.2 get 4 pkts staterr value  */
		zero = _mm_xor_si128(dd_check, dd_check);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128((void *)&rx_pkts[pos+3]->rx_descriptor_fields1,
				pkt_mb4);
		_mm_storeu_si128((void *)&rx_pkts[pos+2]->rx_descriptor_fields1,
				pkt_mb3);

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
			split_packet += RTE_FM10K_DESCS_PER_LOOP;

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

		fm10k_desc_to_pktype_v(descs0, &rx_pkts[pos]);

		/* C.4 calc avaialbe number of desc */
		var = __builtin_popcountll(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != RTE_FM10K_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->next_dd = (uint16_t)(rxq->next_dd + nb_pkts_recd);
	rxq->next_dd = (uint16_t)(rxq->next_dd & (rxq->nb_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/* vPMD receive routine
 *
 * Notice:
 * - don't support ol_flags for rss and csum err
 */
uint16_t
fm10k_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return fm10k_recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

static inline uint16_t
fm10k_reassemble_packets(struct fm10k_rx_queue *rxq,
		struct rte_mbuf **rx_bufs,
		uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[RTE_FM10K_MAX_RX_BURST]; /*finished pkts*/
	struct rte_mbuf *start = rxq->pkt_first_seg;
	struct rte_mbuf *end =  rxq->pkt_last_seg;
	unsigned pkt_idx, buf_idx;

	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end != NULL) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
#ifdef RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE
				start->hash = end->hash;
				start->ol_flags = end->ol_flags;
				start->packet_type = end->packet_type;
#endif
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
 * - nb_pkts > RTE_FM10K_MAX_RX_BURST, only scan RTE_FM10K_MAX_RX_BURST
 *   numbers of DD bit
 */
uint16_t
fm10k_recv_scattered_pkts_vec(void *rx_queue,
				struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	struct fm10k_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_FM10K_MAX_RX_BURST] = {0};
	unsigned i = 0;

	/* Split_flags only can support max of RTE_FM10K_MAX_RX_BURST */
	nb_pkts = RTE_MIN(nb_pkts, RTE_FM10K_MAX_RX_BURST);
	/* get some new buffers */
	uint16_t nb_bufs = fm10k_recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts,
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
	if (rxq->pkt_first_seg == NULL) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
	}
	return i + fm10k_reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
		&split_flags[i]);
}

static const struct fm10k_txq_ops vec_txq_ops = {
	.reset = fm10k_reset_tx_queue,
};

void __attribute__((cold))
fm10k_txq_vec_setup(struct fm10k_tx_queue *txq)
{
	txq->ops = &vec_txq_ops;
}

int __attribute__((cold))
fm10k_tx_vec_condition_check(struct fm10k_tx_queue *txq)
{
	/* Vector TX can't offload any features yet */
	if ((txq->txq_flags & FM10K_SIMPLE_TX_FLAG) != FM10K_SIMPLE_TX_FLAG)
		return -1;

	if (txq->tx_ftag_en)
		return -1;

	return 0;
}

static inline void
vtx1(volatile struct fm10k_tx_desc *txdp,
		struct rte_mbuf *pkt, uint64_t flags)
{
	__m128i descriptor = _mm_set_epi64x(flags << 56 |
			pkt->vlan_tci << 16 | pkt->data_len,
			MBUF_DMA_ADDR(pkt));
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
vtx(volatile struct fm10k_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

static __rte_always_inline int
fm10k_tx_free_bufs(struct fm10k_tx_queue *txq)
{
	struct rte_mbuf **txep;
	uint8_t flags;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[RTE_FM10K_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	flags = txq->hw_ring[txq->next_dd].flags;
	if (!(flags & FM10K_TXD_FLAG_DONE))
		return 0;

	n = txq->rs_thresh;

	/* First buffer to free from S/W ring is at index
	 * next_dd - (rs_thresh-1)
	 */
	txep = &txq->sw_ring[txq->next_dd - (n - 1)];
	m = rte_pktmbuf_prefree_seg(txep[0]);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i]);
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
			m = rte_pktmbuf_prefree_seg(txep[i]);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
		}
	}

	/* buffers were freed, update counters */
	txq->nb_free = (uint16_t)(txq->nb_free + txq->rs_thresh);
	txq->next_dd = (uint16_t)(txq->next_dd + txq->rs_thresh);
	if (txq->next_dd >= txq->nb_desc)
		txq->next_dd = (uint16_t)(txq->rs_thresh - 1);

	return txq->rs_thresh;
}

static __rte_always_inline void
tx_backlog_entry(struct rte_mbuf **txep,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i] = tx_pkts[i];
}

uint16_t
fm10k_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts)
{
	struct fm10k_tx_queue *txq = (struct fm10k_tx_queue *)tx_queue;
	volatile struct fm10k_tx_desc *txdp;
	struct rte_mbuf **txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = FM10K_TXD_FLAG_LAST;
	uint64_t rs = FM10K_TXD_FLAG_RS | FM10K_TXD_FLAG_LAST;
	int i;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->rs_thresh);

	if (txq->nb_free < txq->free_thresh)
		fm10k_tx_free_bufs(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->next_free;
	txdp = &txq->hw_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_free = (uint16_t)(txq->nb_free - nb_pkts);

	n = (uint16_t)(txq->nb_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &(txq->hw_ring[tx_id]);
		txep = &txq->sw_ring[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->next_rs) {
		txq->hw_ring[txq->next_rs].flags |= FM10K_TXD_FLAG_RS;
		txq->next_rs = (uint16_t)(txq->next_rs + txq->rs_thresh);
	}

	txq->next_free = tx_id;

	FM10K_PCI_REG_WRITE(txq->tail_ptr, txq->next_free);

	return nb_pkts;
}

static void __attribute__((cold))
fm10k_reset_tx_queue(struct fm10k_tx_queue *txq)
{
	static const struct fm10k_tx_desc zeroed_desc = {0};
	struct rte_mbuf **txe = txq->sw_ring;
	uint16_t i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_desc; i++)
		txq->hw_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	for (i = 0; i < txq->nb_desc; i++)
		txe[i] = NULL;

	txq->next_dd = (uint16_t)(txq->rs_thresh - 1);
	txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

	txq->next_free = 0;
	txq->nb_used = 0;
	/* Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->nb_free = (uint16_t)(txq->nb_desc - 1);
	FM10K_PCI_REG_WRITE(txq->tail_ptr, 0);
}
