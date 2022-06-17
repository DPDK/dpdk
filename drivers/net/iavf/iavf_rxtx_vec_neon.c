/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Copyright(c) 2022 Arm Limited
 */

#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_vect.h>

#include "iavf.h"
#include "iavf_rxtx.h"
#include "iavf_rxtx_vec_common.h"

static inline void
iavf_rxq_rearm(struct iavf_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	uint64x2_t dma_addr0, dma_addr1;
	uint64x2_t zero = vdupq_n_u64(0);
	uint64_t paddr;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (unlikely(rte_mempool_get_bulk(rxq->mp,
					  (void *)rxep,
					  IAVF_RXQ_REARM_THRESH) < 0)) {
		if (rxq->rxrearm_nb + IAVF_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			for (i = 0; i < IAVF_VPMD_DESCS_PER_LOOP; i++) {
				rxep[i] = &rxq->fake_mbuf;
				vst1q_u64((uint64_t *)&rxdp[i].read, zero);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			IAVF_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < IAVF_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		mb0 = rxep[0];
		mb1 = rxep[1];

		paddr = mb0->buf_iova + RTE_PKTMBUF_HEADROOM;
		dma_addr0 = vdupq_n_u64(paddr);

		/* flush desc with pa dma_addr */
		vst1q_u64((uint64_t *)&rxdp++->read, dma_addr0);

		paddr = mb1->buf_iova + RTE_PKTMBUF_HEADROOM;
		dma_addr1 = vdupq_n_u64(paddr);
		vst1q_u64((uint64_t *)&rxdp++->read, dma_addr1);
	}

	rxq->rxrearm_start += IAVF_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IAVF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	rte_io_wmb();
	/* Update the tail pointer on the NIC */
	IAVF_PCI_REG_WRITE_RELAXED(rxq->qrx_tail, rx_id);
}

static inline void
desc_to_olflags_v(struct iavf_rx_queue *rxq, volatile union iavf_rx_desc *rxdp,
		  uint64x2_t descs[4], struct rte_mbuf **rx_pkts)
{
	RTE_SET_USED(rxdp);

	uint32x4_t vlan0, vlan1, rss, l3_l4e;
	const uint64x2_t mbuf_init = {rxq->mbuf_initializer, 0};
	uint64x2_t rearm0, rearm1, rearm2, rearm3;

	/* mask everything except RSS, flow director and VLAN flags
	 * bit2 is for VLAN tag, bit11 for flow director indication
	 * bit13:12 for RSS indication.
	 */
	const uint32x4_t rss_vlan_msk = {
			0x1c03804, 0x1c03804, 0x1c03804, 0x1c03804};

	const uint32x4_t cksum_mask = {
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
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD};

	/* map rss and vlan type to rss hash and vlan flag */
	const uint8x16_t vlan_flags = {
			0, 0, 0, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0};

	const uint8x16_t rss_flags = {
			0, RTE_MBUF_F_RX_FDIR, 0, 0,
			0, 0, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR,
			0, 0, 0, 0,
			0, 0, 0, 0};

	const uint8x16_t l3_l4e_flags = {
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1,
			RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
			 RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			0, 0, 0, 0, 0, 0, 0, 0};

	vlan0 = vzipq_u32(vreinterpretq_u32_u64(descs[0]),
			  vreinterpretq_u32_u64(descs[2])).val[1];
	vlan1 = vzipq_u32(vreinterpretq_u32_u64(descs[1]),
			  vreinterpretq_u32_u64(descs[3])).val[1];
	vlan0 = vzipq_u32(vlan0, vlan1).val[0];

	vlan1 = vandq_u32(vlan0, rss_vlan_msk);
	vlan0 = vreinterpretq_u32_u8(vqtbl1q_u8(vlan_flags,
						vreinterpretq_u8_u32(vlan1)));

	const uint32x4_t desc_fltstat = vshrq_n_u32(vlan1, 11);
	rss = vreinterpretq_u32_u8(vqtbl1q_u8(rss_flags,
					      vreinterpretq_u8_u32(desc_fltstat)));

	l3_l4e = vshrq_n_u32(vlan1, 22);
	l3_l4e = vreinterpretq_u32_u8(vqtbl1q_u8(l3_l4e_flags,
					      vreinterpretq_u8_u32(l3_l4e)));
	/* then we shift left 1 bit */
	l3_l4e = vshlq_n_u32(l3_l4e, 1);
	/* we need to mask out the redundant bits */
	l3_l4e = vandq_u32(l3_l4e, cksum_mask);

	vlan0 = vorrq_u32(vlan0, rss);
	vlan0 = vorrq_u32(vlan0, l3_l4e);

	rearm0 = vsetq_lane_u64(vgetq_lane_u32(vlan0, 0), mbuf_init, 1);
	rearm1 = vsetq_lane_u64(vgetq_lane_u32(vlan0, 1), mbuf_init, 1);
	rearm2 = vsetq_lane_u64(vgetq_lane_u32(vlan0, 2), mbuf_init, 1);
	rearm3 = vsetq_lane_u64(vgetq_lane_u32(vlan0, 3), mbuf_init, 1);

	vst1q_u64((uint64_t *)&rx_pkts[0]->rearm_data, rearm0);
	vst1q_u64((uint64_t *)&rx_pkts[1]->rearm_data, rearm1);
	vst1q_u64((uint64_t *)&rx_pkts[2]->rearm_data, rearm2);
	vst1q_u64((uint64_t *)&rx_pkts[3]->rearm_data, rearm3);
}

#define PKTLEN_SHIFT     10
#define IAVF_UINT16_BIT (CHAR_BIT * sizeof(uint16_t))

static inline void
desc_to_ptype_v(uint64x2_t descs[4], struct rte_mbuf **__rte_restrict rx_pkts,
		uint32_t *__rte_restrict ptype_tbl)
{
	int i;
	uint8_t ptype;
	uint8x16_t tmp;

	for (i = 0; i < 4; i++) {
		tmp = vreinterpretq_u8_u64(vshrq_n_u64(descs[i], 30));
		ptype = vgetq_lane_u8(tmp, 8);
		rx_pkts[i]->packet_type = ptype_tbl[ptype];
	}
}

/**
 * vPMD raw receive routine, only accept(nb_pkts >= IAVF_VPMD_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < IAVF_VPMD_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a IAVF_VPMD_DESCS_PER_LOOP power-of-two
 */
static inline uint16_t
_recv_raw_pkts_vec(struct iavf_rx_queue *__rte_restrict rxq,
		   struct rte_mbuf **__rte_restrict rx_pkts,
		   uint16_t nb_pkts, uint8_t *split_packet)
{
	RTE_SET_USED(split_packet);

	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	/* mask to shuffle from desc. to mbuf */
	uint8x16_t shuf_msk = {
		0xFF, 0xFF,   /* pkt_type set as unknown */
		0xFF, 0xFF,   /* pkt_type set as unknown */
		14, 15,       /* octet 15~14, low 16 bits pkt_len */
		0xFF, 0xFF,   /* skip high 16 bits pkt_len, zero out */
		14, 15,       /* octet 15~14, 16 bits data_len */
		2, 3,         /* octet 2~3, low 16 bits vlan_macip */
		4, 5, 6, 7    /* octet 4~7, 32bits rss */
		};

	uint16x8_t crc_adjust = {
		0, 0,         /* ignore pkt_type field */
		rxq->crc_len, /* sub crc on pkt_len */
		0,            /* ignore high-16bits of pkt_len */
		rxq->crc_len, /* sub crc on data_len */
		0, 0, 0       /* ignore non-length fields */
		};
	/* nb_pkts has to be floor-aligned to IAVF_VPMD_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_VPMD_DESCS_PER_LOOP);

	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch_non_temporal(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IAVF_RXQ_REARM_THRESH)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
			rte_cpu_to_le_32(1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

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
		uint64x2_t descs[IAVF_VPMD_DESCS_PER_LOOP];
		uint8x16_t pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		uint16x8x2_t sterr_tmp1, sterr_tmp2;
		uint64x2_t mbp1, mbp2;
		uint16x8_t staterr;
		uint16x8_t tmp;
		uint64_t stat;

		int32x4_t len_shl = {0, 0, 0, PKTLEN_SHIFT};

		/* A.1 load desc[3-0] */
		descs[3] =  vld1q_u64((uint64_t *)(rxdp + 3));
		descs[2] =  vld1q_u64((uint64_t *)(rxdp + 2));
		descs[1] =  vld1q_u64((uint64_t *)(rxdp + 1));
		descs[0] =  vld1q_u64((uint64_t *)(rxdp));

		/* Use acquire fence to order loads of descriptor qwords */
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);
		/* A.2 reload qword0 to make it ordered after qword1 load */
		descs[3] = vld1q_lane_u64((uint64_t *)(rxdp + 3), descs[3], 0);
		descs[2] = vld1q_lane_u64((uint64_t *)(rxdp + 2), descs[2], 0);
		descs[1] = vld1q_lane_u64((uint64_t *)(rxdp + 1), descs[1], 0);
		descs[0] = vld1q_lane_u64((uint64_t *)(rxdp), descs[0], 0);

		/* B.1 load 4 mbuf point */
		mbp1 = vld1q_u64((uint64_t *)&sw_ring[pos]);
		mbp2 = vld1q_u64((uint64_t *)&sw_ring[pos + 2]);

		/* B.2 copy 4 mbuf point into rx_pkts  */
		vst1q_u64((uint64_t *)&rx_pkts[pos], mbp1);
		vst1q_u64((uint64_t *)&rx_pkts[pos + 2], mbp2);

		/* pkts shift the pktlen field to be 16-bit aligned*/
		uint32x4_t len3 = vshlq_u32(vreinterpretq_u32_u64(descs[3]),
					    len_shl);
		descs[3] = vreinterpretq_u64_u16(vsetq_lane_u16
				(vgetq_lane_u16(vreinterpretq_u16_u32(len3), 7),
				 vreinterpretq_u16_u64(descs[3]),
				 7));
		uint32x4_t len2 = vshlq_u32(vreinterpretq_u32_u64(descs[2]),
					    len_shl);
		descs[2] = vreinterpretq_u64_u16(vsetq_lane_u16
				(vgetq_lane_u16(vreinterpretq_u16_u32(len2), 7),
				 vreinterpretq_u16_u64(descs[2]),
				 7));
		uint32x4_t len1 = vshlq_u32(vreinterpretq_u32_u64(descs[1]),
					    len_shl);
		descs[1] = vreinterpretq_u64_u16(vsetq_lane_u16
				(vgetq_lane_u16(vreinterpretq_u16_u32(len1), 7),
				 vreinterpretq_u16_u64(descs[1]),
				 7));
		uint32x4_t len0 = vshlq_u32(vreinterpretq_u32_u64(descs[0]),
					    len_shl);
		descs[0] = vreinterpretq_u64_u16(vsetq_lane_u16
				(vgetq_lane_u16(vreinterpretq_u16_u32(len0), 7),
				 vreinterpretq_u16_u64(descs[0]),
				 7));
		desc_to_olflags_v(rxq, rxdp, descs, &rx_pkts[pos]);

		/* D.1 pkts convert format from desc to pktmbuf */
		pkt_mb4 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[3]), shuf_msk);
		pkt_mb3 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[2]), shuf_msk);
		pkt_mb2 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[1]), shuf_msk);
		pkt_mb1 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[0]), shuf_msk);

		/* D.2 pkts set in_port/nb_seg and remove crc */
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb4), crc_adjust);
		pkt_mb4 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb3), crc_adjust);
		pkt_mb3 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb2), crc_adjust);
		pkt_mb2 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb1), crc_adjust);
		pkt_mb1 = vreinterpretq_u8_u16(tmp);

		/* D.3 copy final data to rx_pkts */
		vst1q_u8((void *)&rx_pkts[pos + 3]->rx_descriptor_fields1,
				pkt_mb4);
		vst1q_u8((void *)&rx_pkts[pos + 2]->rx_descriptor_fields1,
				pkt_mb3);
		vst1q_u8((void *)&rx_pkts[pos + 1]->rx_descriptor_fields1,
				pkt_mb2);
		vst1q_u8((void *)&rx_pkts[pos]->rx_descriptor_fields1,
				pkt_mb1);

		desc_to_ptype_v(descs, &rx_pkts[pos], ptype_tbl);

		if (likely(pos + IAVF_VPMD_DESCS_PER_LOOP < nb_pkts))
			rte_prefetch_non_temporal(rxdp + IAVF_VPMD_DESCS_PER_LOOP);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = vzipq_u16(vreinterpretq_u16_u64(descs[1]),
				       vreinterpretq_u16_u64(descs[3]));
		sterr_tmp1 = vzipq_u16(vreinterpretq_u16_u64(descs[0]),
				       vreinterpretq_u16_u64(descs[2]));

		/* C.2 get 4 pkts staterr value  */
		staterr = vzipq_u16(sterr_tmp1.val[1],
				    sterr_tmp2.val[1]).val[0];

		staterr = vshlq_n_u16(staterr, IAVF_UINT16_BIT - 1);
		staterr = vreinterpretq_u16_s16(
				vshrq_n_s16(vreinterpretq_s16_u16(staterr),
					    IAVF_UINT16_BIT - 1));
		stat = ~vgetq_lane_u64(vreinterpretq_u64_u16(staterr), 0);

		/* C.4 calc available number of desc */
		if (unlikely(stat == 0)) {
			nb_pkts_recd += IAVF_VPMD_DESCS_PER_LOOP;
		} else {
			nb_pkts_recd += __builtin_ctzl(stat) / IAVF_UINT16_BIT;
			break;
		}
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);
	return nb_pkts_recd;
}

/*
 * Notice:
 * - nb_pkts < IAVF_VPMD_DESCS_PER_LOOP, just return no packet
 * - nb_pkts > IAVF_VPMD_RX_BURST, only scan IAVF_VPMD_RX_BURST
 *   numbers of DD bits
 */
uint16_t
iavf_recv_pkts_vec(void *__rte_restrict rx_queue,
		struct rte_mbuf **__rte_restrict rx_pkts, uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

static void __rte_cold
iavf_rx_queue_release_mbufs_neon(struct iavf_rx_queue *rxq)
{
	_iavf_rx_queue_release_mbufs_vec(rxq);
}

static const struct iavf_rxq_ops neon_vec_rxq_ops = {
	.release_mbufs = iavf_rx_queue_release_mbufs_neon,
};

int __rte_cold
iavf_rxq_vec_setup(struct iavf_rx_queue *rxq)
{
	rxq->ops = &neon_vec_rxq_ops;
	return iavf_rxq_vec_setup_default(rxq);
}

int __rte_cold
iavf_rx_vec_dev_check(struct rte_eth_dev *dev)
{
	return iavf_rx_vec_dev_check_default(dev);
}
