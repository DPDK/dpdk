/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2024 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_vect.h>

#include "ngbe_type.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"
#include "ngbe_rxtx_vec_common.h"

static inline void
ngbe_rxq_rearm(struct ngbe_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	uint64x2_t dma_addr0, dma_addr1;
	uint64x2_t zero = vdupq_n_u64(0);
	uint64_t paddr;
	uint8x8_t p;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (unlikely(rte_mempool_get_bulk(rxq->mb_pool,
					  (void *)rxep,
					  RTE_NGBE_RXQ_REARM_THRESH) < 0)) {
		if (rxq->rxrearm_nb + RTE_NGBE_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			for (i = 0; i < RTE_NGBE_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				vst1q_u64(RTE_CAST_PTR(uint64_t *, &rxdp[i]), zero);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_NGBE_RXQ_REARM_THRESH;
		return;
	}

	p = vld1_u8((uint8_t *)&rxq->mbuf_initializer);

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_NGBE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

		/*
		 * Flush mbuf with pkt template.
		 * Data to be rearmed is 6 bytes long.
		 */
		vst1_u8((uint8_t *)&mb0->rearm_data, p);
		paddr = mb0->buf_iova + RTE_PKTMBUF_HEADROOM;
		dma_addr0 = vsetq_lane_u64(paddr, zero, 0);
		/* flush desc with pa dma_addr */
		vst1q_u64(RTE_CAST_PTR(uint64_t *, rxdp++), dma_addr0);

		vst1_u8((uint8_t *)&mb1->rearm_data, p);
		paddr = mb1->buf_iova + RTE_PKTMBUF_HEADROOM;
		dma_addr1 = vsetq_lane_u64(paddr, zero, 0);
		vst1q_u64(RTE_CAST_PTR(uint64_t *, rxdp++), dma_addr1);
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
desc_to_olflags_v(uint8x16x2_t sterr_tmp1, uint8x16x2_t sterr_tmp2,
		  uint8x16_t staterr, uint8_t vlan_flags,
		  struct rte_mbuf **rx_pkts)
{
	uint8x16_t ptype;
	uint8x16_t vtag_lo, vtag_hi, vtag;
	uint8x16_t temp_csum, temp_vp;
	uint8x16_t vtag_mask = vdupq_n_u8(0x0F);
	uint32x4_t csum = {0, 0, 0, 0};

	union {
		uint16_t e[4];
		uint64_t word;
	} vol;

	const uint8x16_t rsstype_msk = {
			0x0F, 0x0F, 0x0F, 0x0F,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00};

	const uint8x16_t rss_flags = {
			0, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			0, RTE_MBUF_F_RX_RSS_HASH, 0, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, 0, 0, 0,
			0, 0, 0, RTE_MBUF_F_RX_FDIR};

	/* mask everything except vlan present and l4/ip csum error */
	const uint8x16_t vlan_csum_msk = {
			NGBE_RXD_STAT_VLAN, NGBE_RXD_STAT_VLAN,
			NGBE_RXD_STAT_VLAN, NGBE_RXD_STAT_VLAN,
			0, 0, 0, 0,
			0, 0, 0, 0,
			(NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 24,
			(NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 24,
			(NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 24,
			(NGBE_RXD_ERR_L4CS | NGBE_RXD_ERR_IPCS) >> 24};

	/* map vlan present and l4/ip csum error to ol_flags */
	const uint8x16_t vlan_csum_map_lo = {
			RTE_MBUF_F_RX_IP_CKSUM_GOOD,
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
			RTE_MBUF_F_RX_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
			0, 0, 0, 0,
			vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_GOOD,
			vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
			vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_BAD,
			vlan_flags | RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
			0, 0, 0, 0};

	const uint8x16_t vlan_csum_map_hi = {
			RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
			RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
			RTE_MBUF_F_RX_L4_CKSUM_GOOD >> sizeof(uint8_t), 0,
			0, 0, 0, 0};

	ptype = vzipq_u8(sterr_tmp1.val[0], sterr_tmp2.val[0]).val[0];
	ptype = vandq_u8(ptype, rsstype_msk);
	ptype = vqtbl1q_u8(rss_flags, ptype);

	/* extract vlan_flags and csum_error from staterr */
	vtag = vandq_u8(staterr, vlan_csum_msk);

	/* csum bits are in the most significant, to use shuffle we need to
	 * shift them. Change mask from 0xc0 to 0x03.
	 */
	temp_csum = vshrq_n_u8(vtag, 6);

	/* Change vlan present mask from 0x20 to 0x08.
	 */
	temp_vp = vshrq_n_u8(vtag, 2);

	/* 'OR' the most significant 32 bits containing the checksum flags with
	 * the vlan present flags. Then bits layout of each lane(8bits) will be
	 * 'xxxx,VLAN,x,ERR_IPCS,ERR_L4CS'
	 */
	csum = vsetq_lane_u32(vgetq_lane_u32(vreinterpretq_u32_u8(temp_csum), 3), csum, 0);
	vtag = vorrq_u8(vreinterpretq_u8_u32(csum), vtag);
	vtag = vorrq_u8(vtag, temp_vp);
	vtag = vandq_u8(vtag, vtag_mask);

	/* convert L4 checksum correct type to vtag_hi */
	vtag_hi = vqtbl1q_u8(vlan_csum_map_hi, vtag);
	vtag_hi = vshrq_n_u8(vtag_hi, 7);

	/* convert VP, IPE, L4E to vtag_lo */
	vtag_lo = vqtbl1q_u8(vlan_csum_map_lo, vtag);
	vtag_lo = vorrq_u8(ptype, vtag_lo);

	vtag = vzipq_u8(vtag_lo, vtag_hi).val[0];
	vol.word = vgetq_lane_u64(vreinterpretq_u64_u8(vtag), 0);

	rx_pkts[0]->ol_flags = vol.e[0];
	rx_pkts[1]->ol_flags = vol.e[1];
	rx_pkts[2]->ol_flags = vol.e[2];
	rx_pkts[3]->ol_flags = vol.e[3];
}

#define NGBE_VPMD_DESC_EOP_MASK	0x02020202
#define NGBE_UINT8_BIT			(CHAR_BIT * sizeof(uint8_t))

static inline void
desc_to_ptype_v(uint64x2_t descs[4], uint16_t pkt_type_mask,
		struct rte_mbuf **rx_pkts)
{
	uint32x4_t ptype_mask = vdupq_n_u32((uint32_t)pkt_type_mask);
	uint32x4_t ptype0 = vzipq_u32(vreinterpretq_u32_u64(descs[0]),
				vreinterpretq_u32_u64(descs[2])).val[0];
	uint32x4_t ptype1 = vzipq_u32(vreinterpretq_u32_u64(descs[1]),
				vreinterpretq_u32_u64(descs[3])).val[0];

	/* interleave low 32 bits,
	 * now we have 4 ptypes in a NEON register
	 */
	ptype0 = vzipq_u32(ptype0, ptype1).val[0];

	/* shift right by NGBE_RXD_PTID_SHIFT, and apply ptype mask */
	ptype0 = vandq_u32(vshrq_n_u32(ptype0, NGBE_RXD_PTID_SHIFT), ptype_mask);

	rx_pkts[0]->packet_type = ngbe_decode_ptype(vgetq_lane_u32(ptype0, 0));
	rx_pkts[1]->packet_type = ngbe_decode_ptype(vgetq_lane_u32(ptype0, 1));
	rx_pkts[2]->packet_type = ngbe_decode_ptype(vgetq_lane_u32(ptype0, 2));
	rx_pkts[3]->packet_type = ngbe_decode_ptype(vgetq_lane_u32(ptype0, 3));
}

/**
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
	int pos, i;
	uint8x16_t shuf_msk = {
		0xFF, 0xFF,
		0xFF, 0xFF,  /* skip 32 bits pkt_type */
		12, 13,      /* octet 12~13, low 16 bits pkt_len */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		12, 13,      /* octet 12~13, 16 bits data_len */
		14, 15,      /* octet 14~15, low 16 bits vlan_macip */
		4, 5, 6, 7  /* octet 4~7, 32bits rss */
		};
	uint16x8_t crc_adjust = {0, 0, rxq->crc_len, 0,
				 rxq->crc_len, 0, 0, 0};
	uint8_t vlan_flags;

	/* nb_pkts has to be floor-aligned to RTE_NGBE_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_NGBE_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch_non_temporal(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_NGBE_RXQ_REARM_THRESH)
		ngbe_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->qw1.lo.status & rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
		return 0;

	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache
	 */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

	/* ensure these 2 flags are in the lower 8 bits */
	RTE_BUILD_BUG_ON((RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED) > UINT8_MAX);
	vlan_flags = rxq->vlan_flags & UINT8_MAX;

	/* A. load 4 packet in one loop
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */
	for (pos = 0, nb_pkts_recd = 0; pos < nb_pkts;
			pos += RTE_NGBE_DESCS_PER_LOOP,
			rxdp += RTE_NGBE_DESCS_PER_LOOP) {
		uint64x2_t descs[RTE_NGBE_DESCS_PER_LOOP];
		uint8x16_t pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		uint8x16x2_t sterr_tmp1, sterr_tmp2;
		uint64x2_t mbp1, mbp2;
		uint8x16_t staterr;
		uint16x8_t tmp;
		uint32_t stat;

		/* B.1 load 2 mbuf point */
		mbp1 = vld1q_u64((uint64_t *)&sw_ring[pos]);

		/* B.2 copy 2 mbuf point into rx_pkts  */
		vst1q_u64((uint64_t *)&rx_pkts[pos], mbp1);

		/* B.1 load 2 mbuf point */
		mbp2 = vld1q_u64((uint64_t *)&sw_ring[pos + 2]);

		/* A. load 4 pkts descs */
		descs[0] =  vld1q_u64((uint64_t *)(uintptr_t)(rxdp));
		descs[1] =  vld1q_u64((uint64_t *)(uintptr_t)(rxdp + 1));
		descs[2] =  vld1q_u64((uint64_t *)(uintptr_t)(rxdp + 2));
		descs[3] =  vld1q_u64((uint64_t *)(uintptr_t)(rxdp + 3));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		vst1q_u64((uint64_t *)&rx_pkts[pos + 2], mbp2);

		if (split_packet) {
			rte_mbuf_prefetch_part2(rx_pkts[pos]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 1]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 2]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 3]);
		}

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[3]), shuf_msk);
		pkt_mb3 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[2]), shuf_msk);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[1]), shuf_msk);
		pkt_mb1 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[0]), shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = vzipq_u8(vreinterpretq_u8_u64(descs[1]),
				      vreinterpretq_u8_u64(descs[3]));
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = vzipq_u8(vreinterpretq_u8_u64(descs[0]),
				      vreinterpretq_u8_u64(descs[2]));

		/* C.2 get 4 pkts staterr value  */
		staterr = vzipq_u8(sterr_tmp1.val[1], sterr_tmp2.val[1]).val[0];

		/* set ol_flags with vlan packet type */
		desc_to_olflags_v(sterr_tmp1, sterr_tmp2, staterr, vlan_flags,
				  &rx_pkts[pos]);

		for (i = 0; i < RTE_NGBE_DESCS_PER_LOOP; i++) {
			if (rx_pkts[pos + i]->ol_flags &
			    (RTE_MBUF_F_RX_IP_CKSUM_BAD |
			     RTE_MBUF_F_RX_L4_CKSUM_BAD))
				rxq->csum_err++;
		}

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb4), crc_adjust);
		pkt_mb4 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb3), crc_adjust);
		pkt_mb3 = vreinterpretq_u8_u16(tmp);

		/* D.3 copy final 3,4 data to rx_pkts */
		vst1q_u8((void *)&rx_pkts[pos + 3]->rx_descriptor_fields1,
			 pkt_mb4);
		vst1q_u8((void *)&rx_pkts[pos + 2]->rx_descriptor_fields1,
			 pkt_mb3);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb2), crc_adjust);
		pkt_mb2 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb1), crc_adjust);
		pkt_mb1 = vreinterpretq_u8_u16(tmp);

		/* C* extract and record EOP bit */
		if (split_packet) {
			stat = vgetq_lane_u32(vreinterpretq_u32_u8(staterr), 0);
			/* and with mask to extract bits, flipping 1-0 */
			*(int *)split_packet = ~stat & NGBE_VPMD_DESC_EOP_MASK;

			split_packet += RTE_NGBE_DESCS_PER_LOOP;
		}

		/* C.4 expand DD bit to saturate UINT8 */
		staterr = vshlq_n_u8(staterr, NGBE_UINT8_BIT - 1);
		staterr = vreinterpretq_u8_s8(vshrq_n_s8(vreinterpretq_s8_u8(staterr),
					      NGBE_UINT8_BIT - 1));
		stat = ~vgetq_lane_u32(vreinterpretq_u32_u8(staterr), 0);

		rte_prefetch_non_temporal(rxdp + RTE_NGBE_DESCS_PER_LOOP);

		/* D.3 copy final 1,2 data to rx_pkts */
		vst1q_u8((uint8_t *)&rx_pkts[pos + 1]->rx_descriptor_fields1,
			 pkt_mb2);
		vst1q_u8((uint8_t *)&rx_pkts[pos]->rx_descriptor_fields1,
			 pkt_mb1);

		desc_to_ptype_v(descs, NGBE_PTID_MASK, &rx_pkts[pos]);

		/* C.5 calc available number of desc */
		if (unlikely(stat == 0)) {
			nb_pkts_recd += RTE_NGBE_DESCS_PER_LOOP;
		} else {
			nb_pkts_recd += rte_ctz32(stat) / NGBE_UINT8_BIT;
			break;
		}
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

/**
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

	if (pkt_len < RTE_ETHER_HDR_LEN)
		pkt_len = NGBE_FRAME_SIZE_DFT;

	uint64x2_t descriptor = {pkt->buf_iova + pkt->data_off,
				(uint64_t)pkt_len << 45 | flags | pkt_len};

	vst1q_u64(RTE_CAST_PTR(uint64_t *, txdp), descriptor);
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
