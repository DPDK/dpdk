/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#ifndef HNS3_RXTX_VEC_NEON_H
#define HNS3_RXTX_VEC_NEON_H

#include <arm_neon.h>

#pragma GCC diagnostic ignored "-Wcast-qual"

static inline void
hns3_vec_tx(volatile struct hns3_desc *desc, struct rte_mbuf *pkt)
{
	uint64x2_t val1 = {
		rte_pktmbuf_iova(pkt),
		((uint64_t)pkt->data_len) << HNS3_TXD_SEND_SIZE_SHIFT
	};
	uint64x2_t val2 = {
		0,
		((uint64_t)HNS3_TXD_DEFAULT_VLD_FE_BDTYPE) << HNS3_UINT32_BIT
	};
	vst1q_u64((uint64_t *)&desc->addr, val1);
	vst1q_u64((uint64_t *)&desc->tx.outer_vlan_tag, val2);
}

static uint16_t
hns3_xmit_fixed_burst_vec(void *__restrict tx_queue,
			  struct rte_mbuf **__restrict tx_pkts,
			  uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	volatile struct hns3_desc *tx_desc;
	struct hns3_entry *tx_entry;
	uint16_t next_to_use;
	uint16_t nb_commit;
	uint16_t nb_tx;
	uint16_t n, i;

	if (txq->tx_bd_ready < txq->tx_free_thresh)
		hns3_tx_free_buffers(txq);

	nb_commit = RTE_MIN(txq->tx_bd_ready, nb_pkts);
	if (unlikely(nb_commit == 0)) {
		txq->dfx_stats.queue_full_cnt++;
		return 0;
	}
	nb_tx = nb_commit;

	next_to_use = txq->next_to_use;
	tx_desc = &txq->tx_ring[next_to_use];
	tx_entry = &txq->sw_ring[next_to_use];

	/*
	 * We need to deal with n descriptors first for better performance,
	 * if nb_commit is greater than the difference between txq->nb_tx_desc
	 * and next_to_use in sw_ring and tx_ring.
	 */
	n = txq->nb_tx_desc - next_to_use;
	if (nb_commit >= n) {
		for (i = 0; i < n; i++, tx_pkts++, tx_desc++) {
			hns3_vec_tx(tx_desc, *tx_pkts);
			tx_entry[i].mbuf = *tx_pkts;

			/* Increment bytes counter */
			txq->basic_stats.bytes += (*tx_pkts)->pkt_len;
		}

		nb_commit -= n;
		next_to_use = 0;
		tx_desc = &txq->tx_ring[next_to_use];
		tx_entry = &txq->sw_ring[next_to_use];
	}

	for (i = 0; i < nb_commit; i++, tx_pkts++, tx_desc++) {
		hns3_vec_tx(tx_desc, *tx_pkts);
		tx_entry[i].mbuf = *tx_pkts;

		/* Increment bytes counter */
		txq->basic_stats.bytes += (*tx_pkts)->pkt_len;
	}

	next_to_use += nb_commit;
	txq->next_to_use = next_to_use;
	txq->tx_bd_ready -= nb_tx;

	hns3_write_txq_tail_reg(txq, nb_tx);

	return nb_tx;
}

static inline uint32_t
hns3_desc_parse_field(struct hns3_rx_queue *rxq,
		      struct hns3_entry *sw_ring,
		      struct hns3_desc *rxdp,
		      uint32_t   bd_vld_num)
{
	uint32_t l234_info, ol_info, bd_base_info;
	struct rte_mbuf *pkt;
	uint32_t retcode = 0;
	uint32_t i;
	int ret;

	for (i = 0; i < bd_vld_num; i++) {
		pkt = sw_ring[i].mbuf;

		/* init rte_mbuf.rearm_data last 64-bit */
		pkt->ol_flags = RTE_MBUF_F_RX_RSS_HASH;

		l234_info = rxdp[i].rx.l234_info;
		ol_info = rxdp[i].rx.ol_info;
		bd_base_info = rxdp[i].rx.bd_base_info;
		ret = hns3_handle_bdinfo(rxq, pkt, bd_base_info, l234_info);
		if (unlikely(ret)) {
			retcode |= 1u << i;
			continue;
		}

		pkt->packet_type = hns3_rx_calc_ptype(rxq, l234_info, ol_info);

		/* Increment bytes counter */
		rxq->basic_stats.bytes += pkt->pkt_len;
	}

	return retcode;
}

static inline uint16_t
hns3_recv_burst_vec(struct hns3_rx_queue *__restrict rxq,
		    struct rte_mbuf **__restrict rx_pkts,
		    uint16_t nb_pkts,
		    uint64_t *bd_err_mask)
{
	uint16_t rx_id = rxq->next_to_use;
	struct hns3_entry *sw_ring = &rxq->sw_ring[rx_id];
	struct hns3_desc *rxdp = &rxq->rx_ring[rx_id];
	uint32_t bd_valid_num, parse_retcode;
	uint16_t nb_rx = 0;
	uint32_t pos;
	int offset;

	/* mask to shuffle from desc to mbuf's rx_descriptor_fields1 */
	uint8x16_t shuf_desc_fields_msk = {
		0xff, 0xff, 0xff, 0xff,  /* packet type init zero */
		20, 21, 0xff, 0xff,      /* rx.pkt_len to rte_mbuf.pkt_len */
		22, 23,	                 /* size to rte_mbuf.data_len */
		0xff, 0xff,	         /* rte_mbuf.vlan_tci init zero */
		8, 9, 10, 11,	         /* rx.rss_hash to rte_mbuf.hash.rss */
	};

	uint16x8_t crc_adjust = {
		0, 0,         /* ignore pkt_type field */
		rxq->crc_len, /* sub crc on pkt_len */
		0,            /* ignore high-16bits of pkt_len */
		rxq->crc_len, /* sub crc on data_len */
		0, 0, 0,      /* ignore non-length fields */
	};

	/* compile-time verifies the shuffle mask */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash.rss) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	for (pos = 0; pos < nb_pkts; pos += HNS3_DEFAULT_DESCS_PER_LOOP,
				     rxdp += HNS3_DEFAULT_DESCS_PER_LOOP) {
		uint64x2x2_t descs[HNS3_DEFAULT_DESCS_PER_LOOP];
		uint8x16x2_t pkt_mbuf1, pkt_mbuf2, pkt_mbuf3, pkt_mbuf4;
		uint8x16_t pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		uint64x2_t mbp1, mbp2;
		uint16x4_t bd_vld = {0};
		uint16x8_t tmp;
		uint64_t stat;

		/* calc how many bd valid */
		bd_vld = vset_lane_u16(rxdp[0].rx.bdtype_vld_udp0, bd_vld, 0);
		bd_vld = vset_lane_u16(rxdp[1].rx.bdtype_vld_udp0, bd_vld, 1);
		bd_vld = vset_lane_u16(rxdp[2].rx.bdtype_vld_udp0, bd_vld, 2);
		bd_vld = vset_lane_u16(rxdp[3].rx.bdtype_vld_udp0, bd_vld, 3);

		bd_vld = vshl_n_u16(bd_vld,
				    HNS3_UINT16_BIT - 1 - HNS3_RXD_VLD_B);
		bd_vld = vreinterpret_u16_s16(
				vshr_n_s16(vreinterpret_s16_u16(bd_vld),
					   HNS3_UINT16_BIT - 1));
		stat = ~vget_lane_u64(vreinterpret_u64_u16(bd_vld), 0);
		if (likely(stat == 0))
			bd_valid_num = HNS3_DEFAULT_DESCS_PER_LOOP;
		else
			bd_valid_num = __builtin_ctzl(stat) / HNS3_UINT16_BIT;
		if (bd_valid_num == 0)
			break;

		/* load 4 mbuf pointer */
		mbp1 = vld1q_u64((uint64_t *)&sw_ring[pos]);
		mbp2 = vld1q_u64((uint64_t *)&sw_ring[pos + 2]);

		/* store 4 mbuf pointer into rx_pkts */
		vst1q_u64((uint64_t *)&rx_pkts[pos], mbp1);
		vst1q_u64((uint64_t *)&rx_pkts[pos + 2], mbp2);

		/* use offset to control below data load oper ordering */
		offset = rxq->offset_table[bd_valid_num];

		/* read 4 descs */
		descs[0] = vld2q_u64((uint64_t *)(rxdp + offset));
		descs[1] = vld2q_u64((uint64_t *)(rxdp + offset + 1));
		descs[2] = vld2q_u64((uint64_t *)(rxdp + offset + 2));
		descs[3] = vld2q_u64((uint64_t *)(rxdp + offset + 3));

		pkt_mbuf1.val[0] = vreinterpretq_u8_u64(descs[0].val[0]);
		pkt_mbuf1.val[1] = vreinterpretq_u8_u64(descs[0].val[1]);
		pkt_mbuf2.val[0] = vreinterpretq_u8_u64(descs[1].val[0]);
		pkt_mbuf2.val[1] = vreinterpretq_u8_u64(descs[1].val[1]);
		pkt_mbuf3.val[0] = vreinterpretq_u8_u64(descs[2].val[0]);
		pkt_mbuf3.val[1] = vreinterpretq_u8_u64(descs[2].val[1]);
		pkt_mbuf4.val[0] = vreinterpretq_u8_u64(descs[3].val[0]);
		pkt_mbuf4.val[1] = vreinterpretq_u8_u64(descs[3].val[1]);

		/* 4 packets convert format from desc to pktmbuf */
		pkt_mb1 = vqtbl2q_u8(pkt_mbuf1, shuf_desc_fields_msk);
		pkt_mb2 = vqtbl2q_u8(pkt_mbuf2, shuf_desc_fields_msk);
		pkt_mb3 = vqtbl2q_u8(pkt_mbuf3, shuf_desc_fields_msk);
		pkt_mb4 = vqtbl2q_u8(pkt_mbuf4, shuf_desc_fields_msk);

		/* 4 packets remove crc */
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb1), crc_adjust);
		pkt_mb1 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb2), crc_adjust);
		pkt_mb2 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb3), crc_adjust);
		pkt_mb3 = vreinterpretq_u8_u16(tmp);
		tmp = vsubq_u16(vreinterpretq_u16_u8(pkt_mb4), crc_adjust);
		pkt_mb4 = vreinterpretq_u8_u16(tmp);

		/* save packet info to rx_pkts mbuf */
		vst1q_u8((void *)&sw_ring[pos + 0].mbuf->rx_descriptor_fields1,
			 pkt_mb1);
		vst1q_u8((void *)&sw_ring[pos + 1].mbuf->rx_descriptor_fields1,
			 pkt_mb2);
		vst1q_u8((void *)&sw_ring[pos + 2].mbuf->rx_descriptor_fields1,
			 pkt_mb3);
		vst1q_u8((void *)&sw_ring[pos + 3].mbuf->rx_descriptor_fields1,
			 pkt_mb4);

		/* store the first 8 bytes of packets mbuf's rearm_data */
		*(uint64_t *)&sw_ring[pos + 0].mbuf->rearm_data =
			rxq->mbuf_initializer;
		*(uint64_t *)&sw_ring[pos + 1].mbuf->rearm_data =
			rxq->mbuf_initializer;
		*(uint64_t *)&sw_ring[pos + 2].mbuf->rearm_data =
			rxq->mbuf_initializer;
		*(uint64_t *)&sw_ring[pos + 3].mbuf->rearm_data =
			rxq->mbuf_initializer;

		rte_prefetch_non_temporal(rxdp + HNS3_DEFAULT_DESCS_PER_LOOP);

		parse_retcode = hns3_desc_parse_field(rxq, &sw_ring[pos],
			&rxdp[offset], bd_valid_num);
		if (unlikely(parse_retcode))
			(*bd_err_mask) |= ((uint64_t)parse_retcode) << pos;

		rte_prefetch0(sw_ring[pos +
				      HNS3_DEFAULT_DESCS_PER_LOOP + 0].mbuf);
		rte_prefetch0(sw_ring[pos +
				      HNS3_DEFAULT_DESCS_PER_LOOP + 1].mbuf);
		rte_prefetch0(sw_ring[pos +
				      HNS3_DEFAULT_DESCS_PER_LOOP + 2].mbuf);
		rte_prefetch0(sw_ring[pos +
				      HNS3_DEFAULT_DESCS_PER_LOOP + 3].mbuf);

		nb_rx += bd_valid_num;
		if (bd_valid_num < HNS3_DEFAULT_DESCS_PER_LOOP)
			break;
	}

	rxq->rx_rearm_nb += nb_rx;
	rxq->next_to_use += nb_rx;
	if (rxq->next_to_use >= rxq->nb_rx_desc)
		rxq->next_to_use = 0;

	return nb_rx;
}
#endif /* HNS3_RXTX_VEC_NEON_H */
