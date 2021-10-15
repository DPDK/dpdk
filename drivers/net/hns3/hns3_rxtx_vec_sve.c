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
#define BD_FIELD_L234_OFFSET	8
#define BD_FIELD_XLEN_OFFSET	12
#define BD_FIELD_RSS_OFFSET	16
#define BD_FIELD_OL_OFFSET	24
#define BD_FIELD_VALID_OFFSET	28

typedef struct {
	uint32_t l234_info[HNS3_SVE_DEFAULT_DESCS_PER_LOOP];
	uint32_t ol_info[HNS3_SVE_DEFAULT_DESCS_PER_LOOP];
	uint32_t bd_base_info[HNS3_SVE_DEFAULT_DESCS_PER_LOOP];
} HNS3_SVE_KEY_FIELD_S;

static inline uint32_t
hns3_desc_parse_field_sve(struct hns3_rx_queue *rxq,
			  struct rte_mbuf **rx_pkts,
			  HNS3_SVE_KEY_FIELD_S *key,
			  uint32_t   bd_vld_num)
{
	uint32_t retcode = 0;
	int ret, i;

	for (i = 0; i < (int)bd_vld_num; i++) {
		/* init rte_mbuf.rearm_data last 64-bit */
		rx_pkts[i]->ol_flags = RTE_MBUF_F_RX_RSS_HASH;

		ret = hns3_handle_bdinfo(rxq, rx_pkts[i], key->bd_base_info[i],
					 key->l234_info[i]);
		if (unlikely(ret)) {
			retcode |= 1u << i;
			continue;
		}

		rx_pkts[i]->packet_type = hns3_rx_calc_ptype(rxq,
					key->l234_info[i], key->ol_info[i]);

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
#define XLEN_ADJUST_LEN		32
#define RSS_ADJUST_LEN		16
#define GEN_VLD_U8_ZIP_INDEX	svindex_s8(28, -4)
	uint16_t rx_id = rxq->next_to_use;
	struct hns3_entry *sw_ring = &rxq->sw_ring[rx_id];
	struct hns3_desc *rxdp = &rxq->rx_ring[rx_id];
	struct hns3_desc *rxdp2;
	HNS3_SVE_KEY_FIELD_S key_field;
	uint64_t bd_valid_num;
	uint32_t parse_retcode;
	uint16_t nb_rx = 0;
	int pos, offset;

	uint16_t xlen_adjust[XLEN_ADJUST_LEN] = {
		0,  0xffff, 1,  0xffff,    /* 1st mbuf: pkt_len and dat_len */
		2,  0xffff, 3,  0xffff,    /* 2st mbuf: pkt_len and dat_len */
		4,  0xffff, 5,  0xffff,    /* 3st mbuf: pkt_len and dat_len */
		6,  0xffff, 7,  0xffff,    /* 4st mbuf: pkt_len and dat_len */
		8,  0xffff, 9,  0xffff,    /* 5st mbuf: pkt_len and dat_len */
		10, 0xffff, 11, 0xffff,    /* 6st mbuf: pkt_len and dat_len */
		12, 0xffff, 13, 0xffff,    /* 7st mbuf: pkt_len and dat_len */
		14, 0xffff, 15, 0xffff,    /* 8st mbuf: pkt_len and dat_len */
	};

	uint32_t rss_adjust[RSS_ADJUST_LEN] = {
		0, 0xffff,        /* 1st mbuf: rss */
		1, 0xffff,        /* 2st mbuf: rss */
		2, 0xffff,        /* 3st mbuf: rss */
		3, 0xffff,        /* 4st mbuf: rss */
		4, 0xffff,        /* 5st mbuf: rss */
		5, 0xffff,        /* 6st mbuf: rss */
		6, 0xffff,        /* 7st mbuf: rss */
		7, 0xffff,        /* 8st mbuf: rss */
	};

	svbool_t pg32 = svwhilelt_b32(0, HNS3_SVE_DEFAULT_DESCS_PER_LOOP);
	svuint16_t xlen_tbl1 = svld1_u16(PG16_256BIT, xlen_adjust);
	svuint16_t xlen_tbl2 = svld1_u16(PG16_256BIT, &xlen_adjust[16]);
	svuint32_t rss_tbl1 = svld1_u32(PG32_256BIT, rss_adjust);
	svuint32_t rss_tbl2 = svld1_u32(PG32_256BIT, &rss_adjust[8]);

	/* compile-time verifies the xlen_adjust mask */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, pkt_len) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			 offsetof(struct rte_mbuf, data_len) + 2);

	for (pos = 0; pos < nb_pkts; pos += HNS3_SVE_DEFAULT_DESCS_PER_LOOP,
				     rxdp += HNS3_SVE_DEFAULT_DESCS_PER_LOOP) {
		svuint64_t vld_clz, mbp1st, mbp2st, mbuf_init;
		svuint64_t xlen1st, xlen2st, rss1st, rss2st;
		svuint32_t l234, ol, vld, vld2, xlen, rss;
		svuint8_t  vld_u8;

		/* calc how many bd valid: part 1 */
		vld = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp,
			svindex_u32(BD_FIELD_VALID_OFFSET, BD_SIZE));
		vld2 = svlsl_n_u32_z(pg32, vld,
				    HNS3_UINT32_BIT - 1 - HNS3_RXD_VLD_B);
		vld2 = svreinterpret_u32_s32(svasr_n_s32_z(pg32,
			svreinterpret_s32_u32(vld2), HNS3_UINT32_BIT - 1));

		/* load 4 mbuf pointer */
		mbp1st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[pos]);

		/* calc how many bd valid: part 2 */
		vld_u8 = svtbl_u8(svreinterpret_u8_u32(vld2),
				  svreinterpret_u8_s8(GEN_VLD_U8_ZIP_INDEX));
		vld_clz = svnot_u64_z(PG64_64BIT, svreinterpret_u64_u8(vld_u8));
		vld_clz = svclz_u64_z(PG64_64BIT, vld_clz);
		svst1_u64(PG64_64BIT, &bd_valid_num, vld_clz);
		bd_valid_num /= HNS3_UINT8_BIT;

		/* load 4 more mbuf pointer */
		mbp2st = svld1_u64(PG64_256BIT, (uint64_t *)&sw_ring[pos + 4]);

		/* use offset to control below data load oper ordering */
		offset = rxq->offset_table[bd_valid_num];
		rxdp2 = rxdp + offset;

		/* store 4 mbuf pointer into rx_pkts */
		svst1_u64(PG64_256BIT, (uint64_t *)&rx_pkts[pos], mbp1st);

		/* load key field to vector reg */
		l234 = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp2,
				svindex_u32(BD_FIELD_L234_OFFSET, BD_SIZE));
		ol = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp2,
				svindex_u32(BD_FIELD_OL_OFFSET, BD_SIZE));

		/* store 4 mbuf pointer into rx_pkts again */
		svst1_u64(PG64_256BIT, (uint64_t *)&rx_pkts[pos + 4], mbp2st);

		/* load datalen, pktlen and rss_hash */
		xlen = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp2,
				svindex_u32(BD_FIELD_XLEN_OFFSET, BD_SIZE));
		rss = svld1_gather_u32offset_u32(pg32, (uint32_t *)rxdp2,
				svindex_u32(BD_FIELD_RSS_OFFSET, BD_SIZE));

		/* store key field to stash buffer */
		svst1_u32(pg32, (uint32_t *)key_field.l234_info, l234);
		svst1_u32(pg32, (uint32_t *)key_field.bd_base_info, vld);
		svst1_u32(pg32, (uint32_t *)key_field.ol_info, ol);

		/* sub crc_len for pkt_len and data_len */
		xlen = svreinterpret_u32_u16(svsub_n_u16_z(PG16_256BIT,
			svreinterpret_u16_u32(xlen), rxq->crc_len));

		/* init mbuf_initializer */
		mbuf_init = svdup_n_u64(rxq->mbuf_initializer);

		/* extract datalen, pktlen and rss from xlen and rss */
		xlen1st = svreinterpret_u64_u16(
			svtbl_u16(svreinterpret_u16_u32(xlen), xlen_tbl1));
		xlen2st = svreinterpret_u64_u16(
			svtbl_u16(svreinterpret_u16_u32(xlen), xlen_tbl2));
		rss1st = svreinterpret_u64_u32(
			svtbl_u32(svreinterpret_u32_u32(rss), rss_tbl1));
		rss2st = svreinterpret_u64_u32(
			svtbl_u32(svreinterpret_u32_u32(rss), rss_tbl2));

		/* save mbuf_initializer */
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp1st,
			offsetof(struct rte_mbuf, rearm_data), mbuf_init);
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp2st,
			offsetof(struct rte_mbuf, rearm_data), mbuf_init);

		/* save datalen and pktlen and rss */
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp1st,
			offsetof(struct rte_mbuf, pkt_len), xlen1st);
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp1st,
			offsetof(struct rte_mbuf, hash.rss), rss1st);
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp2st,
			offsetof(struct rte_mbuf, pkt_len), xlen2st);
		svst1_scatter_u64base_offset_u64(PG64_256BIT, mbp2st,
			offsetof(struct rte_mbuf, hash.rss), rss2st);

		rte_prefetch_non_temporal(rxdp +
					  HNS3_SVE_DEFAULT_DESCS_PER_LOOP);

		parse_retcode = hns3_desc_parse_field_sve(rxq, &rx_pkts[pos],
					&key_field, bd_valid_num);
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

static inline void
hns3_rxq_rearm_mbuf_sve(struct hns3_rx_queue *rxq)
{
#define REARM_LOOP_STEP_NUM	4
	struct hns3_entry *rxep = &rxq->sw_ring[rxq->rx_rearm_start];
	struct hns3_desc *rxdp = rxq->rx_ring + rxq->rx_rearm_start;
	struct hns3_entry *rxep_tmp = rxep;
	int i;

	if (unlikely(rte_mempool_get_bulk(rxq->mb_pool, (void *)rxep,
					  HNS3_DEFAULT_RXQ_REARM_THRESH) < 0)) {
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
		return;
	}

	for (i = 0; i < HNS3_DEFAULT_RXQ_REARM_THRESH; i += REARM_LOOP_STEP_NUM,
		rxep_tmp += REARM_LOOP_STEP_NUM) {
		svuint64_t prf = svld1_u64(PG64_256BIT, (uint64_t *)rxep_tmp);
		svprfd_gather_u64base(PG64_256BIT, prf, SV_PLDL1STRM);
	}

	for (i = 0; i < HNS3_DEFAULT_RXQ_REARM_THRESH; i += REARM_LOOP_STEP_NUM,
		rxep += REARM_LOOP_STEP_NUM, rxdp += REARM_LOOP_STEP_NUM) {
		uint64_t iova[REARM_LOOP_STEP_NUM];
		iova[0] = rxep[0].mbuf->buf_iova;
		iova[1] = rxep[1].mbuf->buf_iova;
		iova[2] = rxep[2].mbuf->buf_iova;
		iova[3] = rxep[3].mbuf->buf_iova;
		svuint64_t siova = svld1_u64(PG64_256BIT, iova);
		siova = svadd_n_u64_z(PG64_256BIT, siova, RTE_PKTMBUF_HEADROOM);
		svuint64_t ol_base = svdup_n_u64(0);
		svst1_scatter_u64offset_u64(PG64_256BIT,
			(uint64_t *)&rxdp[0].addr,
			svindex_u64(BD_FIELD_ADDR_OFFSET, BD_SIZE), siova);
		svst1_scatter_u64offset_u64(PG64_256BIT,
			(uint64_t *)&rxdp[0].addr,
			svindex_u64(BD_FIELD_OL_OFFSET, BD_SIZE), ol_base);
	}

	rxq->rx_rearm_start += HNS3_DEFAULT_RXQ_REARM_THRESH;
	if (rxq->rx_rearm_start >= rxq->nb_rx_desc)
		rxq->rx_rearm_start = 0;

	rxq->rx_rearm_nb -= HNS3_DEFAULT_RXQ_REARM_THRESH;

	hns3_write_reg_opt(rxq->io_head_reg, HNS3_DEFAULT_RXQ_REARM_THRESH);
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
		hns3_rxq_rearm_mbuf_sve(rxq);

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
			hns3_rxq_rearm_mbuf_sve(rxq);
	}

	return nb_rx;
}

static inline void
hns3_tx_free_buffers_sve(struct hns3_tx_queue *txq)
{
#define HNS3_SVE_CHECK_DESCS_PER_LOOP	8
#define TX_VLD_U8_ZIP_INDEX		svindex_u8(0, 4)
	svbool_t pg32 = svwhilelt_b32(0, HNS3_SVE_CHECK_DESCS_PER_LOOP);
	svuint32_t vld, vld2;
	svuint8_t vld_u8;
	uint64_t vld_all;
	struct hns3_desc *tx_desc;
	int i;

	/*
	 * All mbufs can be released only when the VLD bits of all
	 * descriptors in a batch are cleared.
	 */
	/* do logical OR operation for all desc's valid field */
	vld = svdup_n_u32(0);
	tx_desc = &txq->tx_ring[txq->next_to_clean];
	for (i = 0; i < txq->tx_rs_thresh; i += HNS3_SVE_CHECK_DESCS_PER_LOOP,
				tx_desc += HNS3_SVE_CHECK_DESCS_PER_LOOP) {
		vld2 = svld1_gather_u32offset_u32(pg32, (uint32_t *)tx_desc,
				svindex_u32(BD_FIELD_VALID_OFFSET, BD_SIZE));
		vld = svorr_u32_z(pg32, vld, vld2);
	}
	/* shift left and then right to get all valid bit */
	vld = svlsl_n_u32_z(pg32, vld,
			    HNS3_UINT32_BIT - 1 - HNS3_TXD_VLD_B);
	vld = svreinterpret_u32_s32(svasr_n_s32_z(pg32,
		svreinterpret_s32_u32(vld), HNS3_UINT32_BIT - 1));
	/* use tbl to compress 32bit-lane to 8bit-lane */
	vld_u8 = svtbl_u8(svreinterpret_u8_u32(vld), TX_VLD_U8_ZIP_INDEX);
	/* dump compressed 64bit to variable */
	svst1_u64(PG64_64BIT, &vld_all, svreinterpret_u64_u8(vld_u8));
	if (vld_all > 0)
		return;

	hns3_tx_bulk_free_buffers(txq);
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
	uint32_t i = 0;
	svbool_t pg = svwhilelt_b64_u32(i, nb_pkts);

	do {
		base_addr = svld1_u64(pg, (uint64_t *)pkts);
		/* calc mbuf's field buf_iova address */
		buf_iova = svadd_n_u64_z(pg, base_addr,
					 offsetof(struct rte_mbuf, buf_iova));
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
		uint32_t idx;
		for (idx = 0; idx < svcntd(); idx++)
			txq->basic_stats.bytes += pkts[idx]->pkt_len;

		/* update index for next loop */
		i += svcntd();
		pkts += svcntd();
		txdp += svcntd();
		tx_entry += svcntd();
		pg = svwhilelt_b64_u32(i, nb_pkts);
	} while (svptest_any(svptrue_b64(), pg));
}

static uint16_t
hns3_xmit_fixed_burst_vec_sve(void *__restrict tx_queue,
			      struct rte_mbuf **__restrict tx_pkts,
			      uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	if (txq->tx_bd_ready < txq->tx_free_thresh)
		hns3_tx_free_buffers_sve(txq);

	nb_pkts = RTE_MIN(txq->tx_bd_ready, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		txq->dfx_stats.queue_full_cnt++;
		return 0;
	}

	if (txq->next_to_use + nb_pkts > txq->nb_tx_desc) {
		nb_tx = txq->nb_tx_desc - txq->next_to_use;
		hns3_tx_fill_hw_ring_sve(txq, tx_pkts, nb_tx);
		txq->next_to_use = 0;
	}

	hns3_tx_fill_hw_ring_sve(txq, tx_pkts + nb_tx, nb_pkts - nb_tx);
	txq->next_to_use += nb_pkts - nb_tx;

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
