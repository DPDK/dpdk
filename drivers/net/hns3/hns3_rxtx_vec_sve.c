/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#include <arm_sve.h>
#include <rte_io.h>
#include <ethdev_driver.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_rxtx_vec.h"

#define PG16_128BIT		svwhilelt_b16(0, 8)
#define PG16_256BIT		svwhilelt_b16(0, 16)
#define PG32_256BIT		svwhilelt_b32(0, 8)
#define PG64_64BIT		svwhilelt_b64(0, 1)
#define PG64_128BIT		svwhilelt_b64(0, 2)
#define PG64_256BIT		svwhilelt_b64(0, 4)
#define PG64_ALLBIT		svptrue_b64()

#define BD_SIZE			32
#define BD_FIELD_ADDR_OFFSET	0
#define BD_FIELD_VALID_OFFSET	28

static inline uint32_t
hns3_desc_parse_field_sve(struct hns3_rx_queue *rxq,
			  struct rte_mbuf **rx_pkts,
			  struct hns3_desc *rxdp,
			  uint32_t   bd_vld_num)
{
	uint32_t l234_info, ol_info, bd_base_info;
	uint32_t retcode = 0;
	int ret, i;

	for (i = 0; i < (int)bd_vld_num; i++) {
		/* init rte_mbuf.rearm_data last 64-bit */
		rx_pkts[i]->ol_flags = RTE_MBUF_F_RX_RSS_HASH;
		rx_pkts[i]->hash.rss = rxdp[i].rx.rss_hash;
		rx_pkts[i]->pkt_len = rte_le_to_cpu_16(rxdp[i].rx.pkt_len) -
					rxq->crc_len;
		rx_pkts[i]->data_len = rx_pkts[i]->pkt_len;

		l234_info = rxdp[i].rx.l234_info;
		ol_info = rxdp[i].rx.ol_info;
		bd_base_info = rxdp[i].rx.bd_base_info;
		ret = hns3_handle_bdinfo(rxq, rx_pkts[i], bd_base_info, l234_info);
		if (unlikely(ret)) {
			retcode |= 1u << i;
			continue;
		}

		rx_pkts[i]->packet_type = hns3_rx_calc_ptype(rxq, l234_info, ol_info);

		/* Increment bytes counter */
		rxq->basic_stats.bytes += rx_pkts[i]->pkt_len;
	}

	return retcode;
}

static inline void
hns3_rx_prefetch_mbuf_sve(struct hns3_entry *sw_ring)
{
	svuint64_t prf1st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[0]);
	svuint64_t prf2st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[4]);
	svprfd_gather_u64base(PG64_256BIT, prf1st, SV_PLDL1KEEP);
	svprfd_gather_u64base(PG64_256BIT, prf2st, SV_PLDL1KEEP);
}

static inline uint16_t
hns3_recv_burst_vec_sve(struct hns3_rx_queue *__restrict rxq,
			struct rte_mbuf **__restrict rx_pkts,
			uint16_t nb_pkts,
			uint64_t *bd_err_mask)
{
	uint16_t rx_id = rxq->next_to_use;
	struct hns3_entry *sw_ring = &rxq->sw_ring[rx_id];
	struct hns3_desc *rxdp = &rxq->rx_ring[rx_id];
	struct hns3_desc *rxdp2, *next_rxdp;
	uint64_t bd_valid_num;
	uint32_t parse_retcode;
	uint16_t nb_rx = 0;
	int pos, offset;

	svbool_t pg32 = svwhilelt_b32(0, HNS3_SVE_DEFAULT_DESCS_PER_LOOP);

	/* compile-time verifies the xlen_adjust mask */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, pkt_len) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			 offsetof(struct rte_mbuf, data_len) + 2);

	for (pos = 0; pos < nb_pkts; pos += HNS3_SVE_DEFAULT_DESCS_PER_LOOP,
				     rxdp += HNS3_SVE_DEFAULT_DESCS_PER_LOOP) {
		svuint64_t mbp1st, mbp2st, mbuf_init;
		svuint32_t vld;
		svbool_t vld_op;

		/* calc how many bd valid: part 1 */
		vld = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp,
			svindex_u32(BD_FIELD_VALID_OFFSET, BD_SIZE));
		vld = svand_n_u32_z(pg32, vld, BIT(HNS3_RXD_VLD_B));
		vld_op = svcmpne_n_u32(pg32, vld, BIT(HNS3_RXD_VLD_B));
		bd_valid_num = svcntp_b32(pg32, svbrkb_b_z(pg32, vld_op));
		if (bd_valid_num == 0)
			break;

		/* load 4 mbuf pointer */
		mbp1st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[pos]);
		/* load 4 more mbuf pointer */
		mbp2st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[pos + 4]);

		/* use offset to control below data load oper ordering */
		offset = rxq->offset_table[bd_valid_num];
		rxdp2 = rxdp + offset;

		/* store 4 mbuf pointer into rx_pkts */
		svst1_u64(PG64_256BIT, (uint64_t *)&rx_pkts[pos], mbp1st);
		/* store 4 mbuf pointer into rx_pkts again */
		svst1_u64(PG64_256BIT, (uint64_t *)&rx_pkts[pos + 4], mbp2st);

		/* init mbuf_initializer */
		mbuf_init = svdup_n_u64(rxq->mbuf_initializer);
		/* save mbuf_initializer */
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp1st,
			offsetof(struct rte_mbuf, rearm_data), mbuf_init);
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp2st,
			offsetof(struct rte_mbuf, rearm_data), mbuf_init);

		next_rxdp = rxdp + HNS3_SVE_DEFAULT_DESCS_PER_LOOP;
		rte_prefetch_non_temporal(next_rxdp);
		rte_prefetch_non_temporal(next_rxdp + 2);
		rte_prefetch_non_temporal(next_rxdp + 4);
		rte_prefetch_non_temporal(next_rxdp + 6);

		parse_retcode = hns3_desc_parse_field_sve(rxq, &rx_pkts[pos],
					&rxdp2[offset], bd_valid_num);
		if (unlikely(parse_retcode))
			(*bd_err_mask) |= ((uint64_t)parse_retcode) << pos;

		hns3_rx_prefetch_mbuf_sve(&sw_ring[pos +
					HNS3_SVE_DEFAULT_DESCS_PER_LOOP]);

		nb_rx += bd_valid_num;
		if (unlikely(bd_valid_num < HNS3_SVE_DEFAULT_DESCS_PER_LOOP))
			break;
	}

	rxq->rx_rearm_nb += nb_rx;
	rxq->next_to_use += nb_rx;
	if (rxq->next_to_use >= rxq->nb_rx_desc)
		rxq->next_to_use = 0;

	return nb_rx;
}

uint16_t
hns3_recv_pkts_vec_sve(void *__restrict rx_queue,
		       struct rte_mbuf **__restrict rx_pkts,
		       uint16_t nb_pkts)
{
	struct hns3_rx_queue *rxq = rx_queue;
	struct hns3_desc *rxdp = &rxq->rx_ring[rxq->next_to_use];
	uint64_t pkt_err_mask;  /* bit mask indicate whick pkts is error */
	uint16_t nb_rx;

	rte_prefetch_non_temporal(rxdp);

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, HNS3_SVE_DEFAULT_DESCS_PER_LOOP);

	if (rxq->rx_rearm_nb > HNS3_DEFAULT_RXQ_REARM_THRESH)
		hns3_rxq_rearm_mbuf(rxq);

	if (unlikely(!(rxdp->rx.bd_base_info &
			rte_cpu_to_le_32(1u << HNS3_RXD_VLD_B))))
		return 0;

	hns3_rx_prefetch_mbuf_sve(&rxq->sw_ring[rxq->next_to_use]);

	if (likely(nb_pkts <= HNS3_DEFAULT_RX_BURST)) {
		pkt_err_mask = 0;
		nb_rx = hns3_recv_burst_vec_sve(rxq, rx_pkts, nb_pkts,
						&pkt_err_mask);
		nb_rx = hns3_rx_reassemble_pkts(rx_pkts, nb_rx, pkt_err_mask);
		return nb_rx;
	}

	nb_rx = 0;
	while (nb_pkts > 0) {
		uint16_t ret, n;

		n = RTE_MIN(nb_pkts, HNS3_DEFAULT_RX_BURST);
		pkt_err_mask = 0;
		ret = hns3_recv_burst_vec_sve(rxq, &rx_pkts[nb_rx], n,
					      &pkt_err_mask);
		nb_pkts -= ret;
		nb_rx += hns3_rx_reassemble_pkts(&rx_pkts[nb_rx], ret,
						 pkt_err_mask);
		if (ret < n)
			break;

		if (rxq->rx_rearm_nb > HNS3_DEFAULT_RXQ_REARM_THRESH)
			hns3_rxq_rearm_mbuf(rxq);
	}

	return nb_rx;
}

static inline void
hns3_tx_fill_hw_ring_sve(struct hns3_tx_queue *txq,
			 struct rte_mbuf **pkts,
			 uint16_t nb_pkts)
{
#define DATA_OFF_LEN_VAL_MASK	0xFFFF
	struct hns3_desc *txdp = &txq->tx_ring[txq->next_to_use];
	struct hns3_entry *tx_entry = &txq->sw_ring[txq->next_to_use];
	const uint64_t valid_bit = (BIT(HNS3_TXD_VLD_B) | BIT(HNS3_TXD_FE_B)) <<
				   HNS3_UINT32_BIT;
	svuint64_t base_addr, buf_iova, data_off, data_len, addr;
	svuint64_t offsets = svindex_u64(0, BD_SIZE);
	uint32_t cnt = svcntd();
	svbool_t pg;
	uint32_t i;

	for (i = 0; i < nb_pkts; /* i is updated in the inner loop */) {
		pg = svwhilelt_b64_u32(i, nb_pkts);
		base_addr = svld1_u64(pg, (uint64_t *)pkts);
		/* calc mbuf's field buf_iova address */
#if RTE_IOVA_IN_MBUF
		buf_iova = svadd_n_u64_z(pg, base_addr,
					 offsetof(struct rte_mbuf, buf_iova));
#else
		buf_iova = svadd_n_u64_z(pg, base_addr,
					 offsetof(struct rte_mbuf, buf_addr));
#endif
		/* calc mbuf's field data_off address */
		data_off = svadd_n_u64_z(pg, base_addr,
					 offsetof(struct rte_mbuf, data_off));
		/* calc mbuf's field data_len address */
		data_len = svadd_n_u64_z(pg, base_addr,
					 offsetof(struct rte_mbuf, data_len));
		/* store mbuf to tx_entry */
		svst1_u64(pg, (uint64_t *)tx_entry, base_addr);
		/* read pkts->buf_iova */
		buf_iova = svld1_gather_u64base_u64(pg, buf_iova);
		/* read pkts->data_off's 64bit val  */
		data_off = svld1_gather_u64base_u64(pg, data_off);
		/* read pkts->data_len's 64bit val */
		data_len = svld1_gather_u64base_u64(pg, data_len);
		/* zero data_off high 48bit by svand ops */
		data_off = svand_n_u64_z(pg, data_off, DATA_OFF_LEN_VAL_MASK);
		/* zero data_len high 48bit by svand ops */
		data_len = svand_n_u64_z(pg, data_len, DATA_OFF_LEN_VAL_MASK);
		/* calc mbuf data region iova addr */
		addr = svadd_u64_z(pg, buf_iova, data_off);
		/* shift due data_len's offset is 2byte of BD's second 8byte */
		data_len = svlsl_n_u64_z(pg, data_len, HNS3_UINT16_BIT);
		/* save offset 0~7byte of every BD */
		svst1_scatter_u64offset_u64(pg, (uint64_t *)&txdp->addr,
					    offsets, addr);
		/* save offset 8~15byte of every BD */
		svst1_scatter_u64offset_u64(pg, (uint64_t *)&txdp->tx.vlan_tag,
					    offsets, data_len);
		/* save offset 16~23byte of every BD */
		svst1_scatter_u64offset_u64(pg,
				(uint64_t *)&txdp->tx.outer_vlan_tag,
				offsets, svdup_n_u64(0));
		/* save offset 24~31byte of every BD */
		svst1_scatter_u64offset_u64(pg,
				(uint64_t *)&txdp->tx.paylen_fd_dop_ol4cs,
				offsets, svdup_n_u64(valid_bit));

		/* Increment bytes counter */
		txq->basic_stats.bytes +=
			(svaddv_u64(pg, data_len) >> HNS3_UINT16_BIT);

		/* update index for next loop */
		i += cnt;
		pkts += cnt;
		txdp += cnt;
		tx_entry += cnt;
	}
}

static uint16_t
hns3_xmit_fixed_burst_vec_sve(void *__restrict tx_queue,
			      struct rte_mbuf **__restrict tx_pkts,
			      uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	if (txq->tx_bd_ready < txq->tx_free_thresh)
		hns3_tx_free_buffers(txq);

	nb_pkts = RTE_MIN(txq->tx_bd_ready, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		txq->dfx_stats.queue_full_cnt++;
		return 0;
	}

	if (txq->next_to_use + nb_pkts >= txq->nb_tx_desc) {
		nb_tx = txq->nb_tx_desc - txq->next_to_use;
		hns3_tx_fill_hw_ring_sve(txq, tx_pkts, nb_tx);
		txq->next_to_use = 0;
	}

	if (nb_pkts > nb_tx) {
		hns3_tx_fill_hw_ring_sve(txq, tx_pkts + nb_tx, nb_pkts - nb_tx);
		txq->next_to_use += nb_pkts - nb_tx;
	}

	txq->tx_bd_ready -= nb_pkts;
	hns3_write_txq_tail_reg(txq, nb_pkts);

	return nb_pkts;
}

uint16_t
hns3_xmit_pkts_vec_sve(void *tx_queue,
		       struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	uint16_t ret, new_burst;
	uint16_t nb_tx = 0;

	while (nb_pkts) {
		new_burst = RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = hns3_xmit_fixed_burst_vec_sve(tx_queue, &tx_pkts[nb_tx],
						    new_burst);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < new_burst)
			break;
	}

	return nb_tx;
}
