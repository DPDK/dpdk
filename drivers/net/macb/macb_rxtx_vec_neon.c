/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#include <rte_bus_vdev.h>
#include <ethdev_driver.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_vect.h>
#include <stdint.h>

#include <fcntl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <rte_ether.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>

#include "macb_rxtx.h"

#define MACB_UINT8_BIT (CHAR_BIT * sizeof(uint8_t))

#define MACB_DESC_EOF_MASK 0x80808080

static inline uint32_t macb_get_packet_type(struct rte_mbuf *rxm)
{
	struct rte_ether_hdr *eth_hdr;
	uint16_t ether_type;

	eth_hdr = rte_pktmbuf_mtod(rxm, struct rte_ether_hdr *);
	ether_type = eth_hdr->ether_type;

	if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
		return RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
	else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))
		return RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
	else
		return RTE_PTYPE_UNKNOWN;
}

static inline uint8x8_t macb_mbuf_initializer(struct macb_rx_queue *rxq)
{
	volatile struct rte_mbuf mbuf = {.buf_addr = 0}; /* zeroed mbuf */
	uint64x1_t mbuf_initializer;
	uint8x8_t rearm_data_vec;

	mbuf_initializer = vdup_n_u64(0);
	mbuf.data_off = RTE_PKTMBUF_HEADROOM + MACB_RX_DATA_OFFSET;
	mbuf.nb_segs = 1;
	mbuf.port = rxq->port_id;
	rte_mbuf_refcnt_set((struct rte_mbuf *)(uintptr_t)&mbuf, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	mbuf_initializer =
		vset_lane_u64(*(uint64_t *)(&mbuf.rearm_data), mbuf_initializer, 0);
	rearm_data_vec = vld1_u8((uint8_t *)&mbuf_initializer);
	return rearm_data_vec;
}

static inline void macb_rxq_rearm(struct macb_rx_queue *rxq)
{
	uint64_t dma_addr;
	struct macb_dma_desc *desc;
	unsigned int entry;
	struct rte_mbuf *nmb;
	struct macb *bp;
	register int i = 0;
	struct macb_rx_entry *rxe;
	uint8x8_t rearm_data_vec;

	bp = rxq->bp;
	rxe = &rxq->rx_sw_ring[rxq->rxrearm_start];

	entry = macb_rx_ring_wrap(bp, rxq->rxrearm_start);
	desc = macb_rx_desc(rxq, entry);

	rearm_data_vec = macb_mbuf_initializer(rxq);

	/* Pull 'n' more MBUFs into the software ring */
	if (unlikely(rte_mempool_get_bulk(rxq->mb_pool, (void *)rxe,
					MACB_RXQ_REARM_THRESH) < 0)) {
		MACB_LOG(ERR, "allocate mbuf fail!");
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
							MACB_RXQ_REARM_THRESH;
		return;
	}

	for (i = 0; i < MACB_RXQ_REARM_THRESH; ++i) {
		nmb = rxe[i].mbuf;
		entry = macb_rx_ring_wrap(bp, rxq->rxrearm_start);
		desc = macb_rx_desc(rxq, entry);
		rxq->rxrearm_start++;
		vst1_u8((uint8_t *)&nmb->rearm_data, rearm_data_vec);
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		if (unlikely(entry == rxq->nb_rx_desc - 1))
			dma_addr |= MACB_BIT(RX_WRAP);
		desc->ctrl = 0;
		/* Setting addr clears RX_USED and allows reception,
		 * make sure ctrl is cleared first to avoid a race.
		 */
		rte_wmb();
		macb_set_addr(bp, desc, dma_addr);
	}
	if (unlikely(rxq->rxrearm_start >= rxq->nb_rx_desc))
		rxq->rxrearm_start = 0;
	rxq->rxrearm_nb -= MACB_RXQ_REARM_THRESH;
}

static inline void macb_pkts_to_ptype_v(struct rte_mbuf **rx_pkts)
{
	if (likely(rx_pkts[0]->buf_addr != NULL))
		rx_pkts[0]->packet_type = macb_get_packet_type(rx_pkts[0]);

	if (likely(rx_pkts[1]->buf_addr != NULL))
		rx_pkts[1]->packet_type = macb_get_packet_type(rx_pkts[1]);

	if (likely(rx_pkts[2]->buf_addr != NULL))
		rx_pkts[2]->packet_type = macb_get_packet_type(rx_pkts[2]);

	if (likely(rx_pkts[3]->buf_addr != NULL))
		rx_pkts[3]->packet_type = macb_get_packet_type(rx_pkts[3]);
}

static inline void macb_pkts_to_port_v(struct rte_mbuf **rx_pkts, uint16_t port_id)
{
	rx_pkts[0]->port = port_id;
	rx_pkts[1]->port = port_id;
	rx_pkts[2]->port = port_id;
	rx_pkts[3]->port = port_id;
}

static inline void macb_free_rx_pkts(struct macb_rx_queue *rxq,
				     struct rte_mbuf **rx_pkts, int pos, uint16_t count)
{
	for (int j = 0; j < count; j++) {
		if (likely(rx_pkts[pos + j] != NULL)) {
			rte_pktmbuf_free_seg(rx_pkts[pos + j]);
			rx_pkts[pos + j] = NULL;
		}
	}
	rxq->rx_tail += count;
	rxq->rxrearm_nb += count;
	rxq->stats.rx_dropped += count;
}

static inline void macb_desc_to_olflags_v(struct rte_mbuf **rx_pkts)
{
	rx_pkts[0]->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	rx_pkts[1]->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	rx_pkts[2]->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	rx_pkts[3]->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;
}

static uint16_t macb_recv_raw_pkts_vec(struct macb_rx_queue *rxq,
					struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts, uint8_t *split_packet)
{
	struct macb_dma_desc *desc;
	struct macb_rx_entry *rx_sw_ring;
	struct macb_rx_entry *rxn;
	uint16_t nb_pkts_recv = 0;
	register uint16_t pos;
	uint16_t bytes_len = 0;

	uint8x16_t shuf_msk = {
		0xFF, 0xFF, 0xFF, 0xFF, 4,	  5,	0xFF, 0xFF,
		4,	  5,	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	};
	uint16x8_t crc_adjust = {0, 0, rxq->crc_len, 0, rxq->crc_len, 0, 0, 0};

	desc = rxq->rx_ring + rxq->rx_tail * MACB_DESC_ADDR_INTERVAL;
	rte_prefetch_non_temporal(desc);

	if (rxq->rxrearm_nb >= MACB_RXQ_REARM_THRESH)
		macb_rxq_rearm(rxq);

	nb_pkts = RTE_MIN(nb_pkts, rxq->nb_rx_desc - rxq->rxrearm_nb);
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, MACB_DESCS_PER_LOOP);
	nb_pkts = RTE_MIN(nb_pkts, MACB_MAX_RX_BURST);

	/* Make hw descriptor updates visible to CPU */
	rte_rmb();

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!((desc->addr & MACB_BIT(RX_USED)) ? true : false))
		return 0;

	rx_sw_ring = &rxq->rx_sw_ring[rxq->rx_tail];
	/* A. load 4 packet in one loop
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of RX_USED bits among the 4 packets
	 * D. fill info. from desc to mbuf
	 */
	for (pos = 0, nb_pkts_recv = 0; pos < nb_pkts; pos += MACB_DESCS_PER_LOOP,
		desc += MACB_DESCS_PER_LOOP * MACB_DESC_ADDR_INTERVAL) {
		uint64x2_t mbp1, mbp2;
		uint64x2_t descs[MACB_DESCS_PER_LOOP];
		uint8x16x2_t sterr_tmp1, sterr_tmp2;
		uint8x16_t staterr;
		uint8x16_t pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		uint16x8_t pkt_mb_mask;
		uint16x8_t tmp;
		uint16_t cur_bytes_len[MACB_DESCS_PER_LOOP] = {0, 0, 0, 0};
		uint32_t stat;
		uint16_t nb_used = 0;
		uint16_t i;

		/* B.1 load 2 mbuf point */
		mbp1 = vld1q_u64((uint64_t *)&rx_sw_ring[pos]);
		/* B.2 copy 2 mbuf point into rx_pkts */
		vst1q_u64((uint64_t *)&rx_pkts[pos], mbp1);

		/* B.1 load 2 mbuf point */
		mbp2 = vld1q_u64((uint64_t *)&rx_sw_ring[pos + 2]);
		/* B.2 copy 2 mbuf point into rx_pkts */
		vst1q_u64((uint64_t *)&rx_pkts[pos + 2], mbp2);

		rte_mbuf_prefetch_part2(rx_pkts[pos]);
		rte_mbuf_prefetch_part2(rx_pkts[pos + 1]);
		rte_mbuf_prefetch_part2(rx_pkts[pos + 2]);
		rte_mbuf_prefetch_part2(rx_pkts[pos + 3]);

		/* A. load 4 pkts descs */
		descs[0] = vld1q_u64((uint64_t *)(desc));
		descs[1] = vld1q_u64((uint64_t *)(desc + 1 * MACB_DESC_ADDR_INTERVAL));
		descs[2] = vld1q_u64((uint64_t *)(desc + 2 * MACB_DESC_ADDR_INTERVAL));
		descs[3] = vld1q_u64((uint64_t *)(desc + 3 * MACB_DESC_ADDR_INTERVAL));

		rxn = &rx_sw_ring[pos + 0 + MACB_NEON_PREFETCH_ENTRY];
		rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);
		rxn = &rx_sw_ring[pos + 1 + MACB_NEON_PREFETCH_ENTRY];
		rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);
		rxn = &rx_sw_ring[pos + 2 + MACB_NEON_PREFETCH_ENTRY];
		rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);
		rxn = &rx_sw_ring[pos + 3 + MACB_NEON_PREFETCH_ENTRY];
		rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);

		/* D.1 pkt convert format from desc to pktmbuf */
		pkt_mb1 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[0]), shuf_msk);
		pkt_mb2 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[1]), shuf_msk);
		pkt_mb3 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[2]), shuf_msk);
		pkt_mb4 = vqtbl1q_u8(vreinterpretq_u8_u64(descs[3]), shuf_msk);

		/* D.2 pkt 1,2 set length and remove crc */
		if (split_packet)
			pkt_mb_mask = vdupq_n_u16(MACB_RX_JFRMLEN_MASK);
		else
			pkt_mb_mask = vdupq_n_u16(MACB_RX_FRMLEN_MASK);

		tmp = vsubq_u16(vandq_u16(vreinterpretq_u16_u8(pkt_mb1), pkt_mb_mask), crc_adjust);
		pkt_mb1 = vreinterpretq_u8_u16(tmp);
		cur_bytes_len[0] = vgetq_lane_u16(tmp, 2);

		tmp = vsubq_u16(vandq_u16(vreinterpretq_u16_u8(pkt_mb2), pkt_mb_mask), crc_adjust);
		pkt_mb2 = vreinterpretq_u8_u16(tmp);
		cur_bytes_len[1] = vgetq_lane_u16(tmp, 2);

		vst1q_u8((uint8_t *)&rx_pkts[pos]->rx_descriptor_fields1, pkt_mb1);
		vst1q_u8((uint8_t *)&rx_pkts[pos + 1]->rx_descriptor_fields1, pkt_mb2);

		/* D.2 pkt 3,4 length and remove crc */
		tmp = vsubq_u16(vandq_u16(vreinterpretq_u16_u8(pkt_mb3), pkt_mb_mask), crc_adjust);
		pkt_mb3 = vreinterpretq_u8_u16(tmp);
		cur_bytes_len[2] = vgetq_lane_u16(tmp, 2);

		tmp = vsubq_u16(vandq_u16(vreinterpretq_u16_u8(pkt_mb4), pkt_mb_mask), crc_adjust);
		pkt_mb4 = vreinterpretq_u8_u16(tmp);
		cur_bytes_len[3] = vgetq_lane_u16(tmp, 2);

		vst1q_u8((void *)&rx_pkts[pos + 2]->rx_descriptor_fields1, pkt_mb3);
		vst1q_u8((void *)&rx_pkts[pos + 3]->rx_descriptor_fields1, pkt_mb4);

		/*C.1 filter RX_USED or SOF_EOF info only */
		sterr_tmp1 = vzipq_u8(vreinterpretq_u8_u64(descs[0]),
							  vreinterpretq_u8_u64(descs[2]));
		sterr_tmp2 = vzipq_u8(vreinterpretq_u8_u64(descs[1]),
							  vreinterpretq_u8_u64(descs[3]));

		/* C* extract and record EOF bit */
		if (split_packet) {
			uint8x16_t eof;

			eof = vzipq_u8(sterr_tmp1.val[0], sterr_tmp2.val[0]).val[1];
			stat = vgetq_lane_u32(vreinterpretq_u32_u8(eof), 1);
			/* and with mask to extract bits, flipping 1-0 */
			*(int *)split_packet = ~stat & MACB_DESC_EOF_MASK;

			split_packet += MACB_DESCS_PER_LOOP;
		}

		/* C.2 get 4 pkts RX_USED value */
		staterr = vzipq_u8(sterr_tmp1.val[0], sterr_tmp2.val[0]).val[0];

		macb_desc_to_olflags_v(&rx_pkts[pos]);

		/* C.3 expand RX_USED bit to saturate UINT8 */
		staterr = vshlq_n_u8(staterr, MACB_UINT8_BIT - 1);
		staterr = vreinterpretq_u8_s8(vshrq_n_s8(vreinterpretq_s8_u8(staterr),
					      MACB_UINT8_BIT - 1));
		stat = ~vgetq_lane_u32(vreinterpretq_u32_u8(staterr), 0);

		rte_prefetch_non_temporal(desc + MACB_DESCS_PER_LOOP *
								MACB_DESC_ADDR_INTERVAL);

		/* C.4 calc available number of desc */
		if (unlikely(stat == 0))
			nb_used = MACB_DESCS_PER_LOOP;
		else
			nb_used = rte_ctz32(stat) / MACB_UINT8_BIT;

		macb_pkts_to_ptype_v(&rx_pkts[pos]);
		macb_pkts_to_port_v(&rx_pkts[pos], rxq->port_id);

		if (nb_used == MACB_DESCS_PER_LOOP) {
			if (split_packet == NULL) {
				uint8x16_t sof_eof;

				sof_eof = vzipq_u8(sterr_tmp1.val[0], sterr_tmp2.val[0]).val[1];
				sof_eof = vreinterpretq_u8_s8
					(vshrq_n_s8(vreinterpretq_s8_u8(sof_eof),
						    MACB_UINT8_BIT - 2));

				/*get 4 pkts SOF_EOF value*/
				stat = ~vgetq_lane_u32(vreinterpretq_u32_u8(sof_eof), 1);
				if (unlikely(stat != 0)) {
					MACB_LOG(ERR, "not whole frame pointed by descriptor");
					macb_free_rx_pkts(rxq, rx_pkts, pos, MACB_DESCS_PER_LOOP);
					goto out;
				}
			}
		} else {
			u32 ctrl;

			if (split_packet == NULL) {
				for (i = 0; i < nb_used; i++, desc += MACB_DESC_ADDR_INTERVAL) {
					ctrl = desc->ctrl;
					if (unlikely((ctrl & (MACB_BIT(RX_SOF) | MACB_BIT(RX_EOF)))
					    != (MACB_BIT(RX_SOF) | MACB_BIT(RX_EOF)))) {
						MACB_LOG(ERR, "not whole frame pointed by descriptor");
						macb_free_rx_pkts(rxq, rx_pkts, pos, nb_used);
						goto out;
					}
				}
			}
		}

		nb_pkts_recv += nb_used;
		for (i = 0; i < nb_used; i++)
			bytes_len += (cur_bytes_len[i] + rxq->crc_len);

		if (nb_used < MACB_DESCS_PER_LOOP)
			break;
	}

out:
	rxq->stats.rx_bytes += (unsigned long)bytes_len;
	rxq->stats.rx_packets += nb_pkts_recv;
	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recv);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recv);
	/* Make descriptor updates visible to hardware */
	rte_wmb();

	return nb_pkts_recv;
}

uint16_t macb_eth_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
								uint16_t nb_pkts)
{
	return macb_recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

static inline uint16_t macb_reassemble_packets(struct macb_rx_queue *rxq,
							struct rte_mbuf **rx_bufs,
							uint16_t nb_bufs,
							uint8_t *split_flags)
{
	struct rte_mbuf *pkts[MACB_MAX_RX_BURST]; /*finished pkts*/
	struct rte_mbuf *start = rxq->pkt_first_seg;
	struct rte_mbuf *end = rxq->pkt_last_seg;
	unsigned int pkt_idx, buf_idx;
	struct rte_mbuf *curr = rxq->pkt_last_seg;
	uint16_t data_bus_width_mask;

	data_bus_width_mask = MACB_DATA_BUS_WIDTH_MASK(rxq->bp->data_bus_width);
	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		uint16_t len = 0;

		if (end != NULL) {
			/* processing a split packet */
			end = rx_bufs[buf_idx];
			curr->next = end;
			len = end->data_len + rxq->crc_len;
			end->data_len =
				len ? (len - start->pkt_len) : rxq->bp->rx_buffer_size;
			end->data_off = RTE_PKTMBUF_HEADROOM & ~data_bus_width_mask;

			start->nb_segs++;
			rxq->stats.rx_packets--;
			start->pkt_len += end->data_len;

			if (!split_flags[buf_idx]) {
				end->next = NULL;
				/* we need to strip crc for the whole packet */
				if (unlikely(rxq->crc_len > 0)) {
					start->pkt_len -= RTE_ETHER_CRC_LEN;
					if (end->data_len > RTE_ETHER_CRC_LEN) {
						end->data_len -= RTE_ETHER_CRC_LEN;
					} else {
						start->nb_segs--;
						curr->data_len -= RTE_ETHER_CRC_LEN - end->data_len;
						curr->next = NULL;
						/* free up last mbuf */
						rte_pktmbuf_free_seg(end);
					}
				}
				pkts[pkt_idx++] = start;
				start = NULL;
				end = NULL;
			} else {
				curr = curr->next;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			start = rx_bufs[buf_idx];
			start->pkt_len = rxq->bp->rx_buffer_size - MACB_RX_DATA_OFFSET
					 - (RTE_PKTMBUF_HEADROOM & data_bus_width_mask);
			start->data_len = start->pkt_len;
			start->port = rxq->port_id;
			curr = start;
			end = start;
		}
	}

	/* save the partial packet for next time */
	rxq->pkt_first_seg = start;
	rxq->pkt_last_seg = end;
	rte_memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static uint16_t eth_macb_recv_scattered_burst_vec(void *rx_queue,
							struct rte_mbuf **rx_pkts,
							uint16_t nb_pkts)
{
	struct macb_rx_queue *rxq = rx_queue;
	uint8_t split_flags[MACB_MAX_RX_BURST] = {0};
	uint16_t nb_bufs;
	const uint64_t *split_fl64;
	uint16_t i;
	uint16_t reassemble_packets;

	/* get some new buffers */
	nb_bufs = macb_recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts, split_flags);
	if (nb_bufs == 0)
		return 0;

	/* happy day case, full burst + no packets to be joined */
	split_fl64 = (uint64_t *)split_flags;
	if (rxq->pkt_first_seg == NULL && split_fl64[0] == 0 &&
		split_fl64[1] == 0 && split_fl64[2] == 0 && split_fl64[3] == 0)
		return nb_bufs;

	/* reassemble any packets that need reassembly*/
	i = 0;
	if (rxq->pkt_first_seg == NULL) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
	}

	reassemble_packets = macb_reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
							&split_flags[i]);
	return i + reassemble_packets;
}

uint16_t macb_eth_recv_scattered_pkts_vec(void *rx_queue,
						struct rte_mbuf **rx_pkts,
						uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > MACB_MAX_RX_BURST) {
		uint16_t burst;

		burst = eth_macb_recv_scattered_burst_vec(rx_queue, rx_pkts + retval,
								MACB_MAX_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < MACB_MAX_RX_BURST)
			return retval;
	}

	return retval + eth_macb_recv_scattered_burst_vec(rx_queue,
							  rx_pkts + retval, nb_pkts);
}

static inline void macb_set_txdesc(struct macb_tx_queue *queue,
						struct macb_dma_desc *txdesc,
						struct rte_mbuf **tx_pkts, unsigned int pos)
{
	uint32x4_t ctrl_v = vdupq_n_u32(0);
	uint32x4_t data_len_v = vdupq_n_u32(0);
	uint32x4_t BIT_TX_USED = vdupq_n_u32(MACB_BIT(TX_USED));
	uint32x4_t BIT_TX_LAST = vdupq_n_u32(MACB_BIT(TX_LAST));
	uint32x4_t BIT_TX_WARP = vdupq_n_u32(0);
	uint32x4_t BIT_TX_UNUSED = vdupq_n_u32(~MACB_BIT(TX_USED));
	uint64_t buf_dma_addr;

	data_len_v =
		vsetq_lane_u32((uint32_t)(tx_pkts[0]->data_len), data_len_v, 0);
	data_len_v =
		vsetq_lane_u32((uint32_t)(tx_pkts[1]->data_len), data_len_v, 1);
	data_len_v =
		vsetq_lane_u32((uint32_t)(tx_pkts[2]->data_len), data_len_v, 2);
	data_len_v =
		vsetq_lane_u32((uint32_t)(tx_pkts[3]->data_len), data_len_v, 3);

	ctrl_v = vorrq_u32(vorrq_u32(data_len_v, BIT_TX_USED), BIT_TX_LAST);

	if (unlikely(pos + MACB_DESCS_PER_LOOP == queue->nb_tx_desc)) {
		BIT_TX_WARP = vsetq_lane_u32(MACB_BIT(TX_WRAP), BIT_TX_WARP, 3);
		ctrl_v = vorrq_u32(ctrl_v, BIT_TX_WARP);
	}

	buf_dma_addr = rte_mbuf_data_iova(tx_pkts[0]);
	macb_set_addr(queue->bp, txdesc, buf_dma_addr);
	buf_dma_addr = rte_mbuf_data_iova(tx_pkts[1]);
	macb_set_addr(queue->bp, txdesc + 1 * MACB_DESC_ADDR_INTERVAL,
				  buf_dma_addr);
	buf_dma_addr = rte_mbuf_data_iova(tx_pkts[2]);
	macb_set_addr(queue->bp, txdesc + 2 * MACB_DESC_ADDR_INTERVAL,
				  buf_dma_addr);
	buf_dma_addr = rte_mbuf_data_iova(tx_pkts[3]);
	macb_set_addr(queue->bp, txdesc + 3 * MACB_DESC_ADDR_INTERVAL,
				  buf_dma_addr);

	ctrl_v = vandq_u32(ctrl_v, BIT_TX_UNUSED);
	rte_wmb();

	txdesc->ctrl = vgetq_lane_u32(ctrl_v, 0);
	(txdesc + 1 * MACB_DESC_ADDR_INTERVAL)->ctrl = vgetq_lane_u32(ctrl_v, 1);
	(txdesc + 2 * MACB_DESC_ADDR_INTERVAL)->ctrl = vgetq_lane_u32(ctrl_v, 2);
	(txdesc + 3 * MACB_DESC_ADDR_INTERVAL)->ctrl = vgetq_lane_u32(ctrl_v, 3);
}

static inline uint16_t
macb_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct macb_tx_queue *queue;
	struct macb_tx_entry *txe;
	struct macb_dma_desc *txdesc;
	struct macb *bp;
	uint32_t tx_tail;
	uint16_t nb_xmit_vec;
	uint16_t nb_tx;
	uint16_t nb_txok;
	uint16_t nb_idx;
	uint64x2_t mbp1, mbp2;
	uint16x4_t nb_segs_v = vdup_n_u16(0);

	queue = (struct macb_tx_queue *)tx_queue;
	bp = queue->bp;
	nb_tx = 0;

	nb_xmit_vec = nb_pkts - nb_pkts % MACB_DESCS_PER_LOOP;
	tx_tail = queue->tx_tail;
	txe = &queue->tx_sw_ring[tx_tail];
	txdesc = queue->tx_ring + tx_tail * MACB_DESC_ADDR_INTERVAL;

	for (nb_idx = 0; nb_idx < nb_xmit_vec; tx_tail += MACB_DESCS_PER_LOOP,
		nb_idx += MACB_DESCS_PER_LOOP,
		txdesc += MACB_DESCS_PER_LOOP * MACB_DESC_ADDR_INTERVAL) {
		nb_segs_v = vset_lane_u16(tx_pkts[nb_tx]->nb_segs, nb_segs_v, 0);
		nb_segs_v = vset_lane_u16(tx_pkts[nb_tx + 1]->nb_segs, nb_segs_v, 1);
		nb_segs_v = vset_lane_u16(tx_pkts[nb_tx + 2]->nb_segs, nb_segs_v, 2);
		nb_segs_v = vset_lane_u16(tx_pkts[nb_tx + 3]->nb_segs, nb_segs_v, 3);
		if (vmaxv_u16(nb_segs_v) > 1) {
			queue->tx_tail = macb_tx_ring_wrap(bp, tx_tail);
			nb_txok = macb_eth_xmit_pkts(queue, &tx_pkts[nb_tx], nb_pkts);
			nb_tx += nb_txok;
			goto out;
		}

		if (likely(txe[nb_tx].mbuf != NULL))
			rte_pktmbuf_free_seg(txe[nb_tx].mbuf);
		if (likely(txe[nb_tx + 1].mbuf != NULL))
			rte_pktmbuf_free_seg(txe[nb_tx + 1].mbuf);
		if (likely(txe[nb_tx + 2].mbuf != NULL))
			rte_pktmbuf_free_seg(txe[nb_tx + 2].mbuf);
		if (likely(txe[nb_tx + 3].mbuf != NULL))
			rte_pktmbuf_free_seg(txe[nb_tx + 3].mbuf);

		mbp1 = vld1q_u64((uint64_t *)&tx_pkts[nb_tx]);
		mbp2 = vld1q_u64((uint64_t *)&tx_pkts[nb_tx + 2]);
		vst1q_u64((uint64_t *)&txe[nb_tx], mbp1);
		vst1q_u64((uint64_t *)&txe[nb_tx + 2], mbp2);

		queue->stats.tx_bytes +=
			tx_pkts[nb_tx]->pkt_len + tx_pkts[nb_tx + 1]->pkt_len +
			tx_pkts[nb_tx + 2]->pkt_len + tx_pkts[nb_tx + 3]->pkt_len;
		macb_set_txdesc(queue, txdesc, &tx_pkts[nb_tx], tx_tail);
		queue->stats.tx_packets += MACB_DESCS_PER_LOOP;
		nb_tx += MACB_DESCS_PER_LOOP;
		nb_pkts = nb_pkts - MACB_DESCS_PER_LOOP;
	}

	tx_tail = macb_tx_ring_wrap(bp, tx_tail);
	queue->tx_tail = tx_tail;
	if (nb_pkts > 0)
		nb_tx += macb_eth_xmit_pkts(queue, &tx_pkts[nb_tx], nb_pkts);
	else
		macb_writel(bp, NCR, macb_readl(bp, NCR) | MACB_BIT(TSTART));

out:
	return nb_tx;
}

uint16_t macb_eth_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
								uint16_t nb_pkts)
{
	struct macb_tx_queue *queue;
	struct macb *bp;
	uint16_t nb_free;
	uint16_t nb_total_free;
	uint32_t tx_head, tx_tail;
	uint16_t nb_tx, nb_total_tx = 0;

	queue = (struct macb_tx_queue *)tx_queue;
	bp = queue->bp;

	macb_reclaim_txd(queue);

retry:
	tx_head = queue->tx_head;
	tx_tail = queue->tx_tail;

	if (unlikely(tx_head == tx_tail))
		nb_total_free = bp->tx_ring_size - 1;
	else if (tx_head > tx_tail)
		nb_total_free = tx_head - tx_tail - 1;
	else
		nb_total_free = bp->tx_ring_size - (tx_tail - tx_head) - 1;

	nb_pkts = RTE_MIN(nb_total_free, nb_pkts);
	nb_free = bp->tx_ring_size - tx_tail;

	if (nb_pkts > nb_free && nb_free > 0) {
		nb_tx = macb_xmit_pkts_vec(queue, tx_pkts, nb_free);
		nb_total_tx += nb_tx;
		nb_pkts -= nb_tx;
		tx_pkts += nb_tx;
		goto retry;
	}
	if (nb_pkts > 0)
		nb_total_tx += macb_xmit_pkts_vec(queue, tx_pkts, nb_pkts);

	return nb_total_tx;
}
