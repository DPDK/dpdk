/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Hisilicon Limited.
 */

#include <arm_sve.h>
#include <rte_io.h>
#include <rte_ethdev_driver.h>

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
	uint32_t cksum_err;
	int ret, i;

	for (i = 0; i < (int)bd_vld_num; i++) {
		/* init rte_mbuf.rearm_data last 64-bit */
		rx_pkts[i]->ol_flags = PKT_RX_RSS_HASH;

		ret = hns3_handle_bdinfo(rxq, rx_pkts[i], key->bd_base_info[i],
					 key->l234_info[i], &cksum_err);
		if (unlikely(ret)) {
			retcode |= 1u << i;
			continue;
		}

		rx_pkts[i]->packet_type = hns3_rx_calc_ptype(rxq,
					key->l234_info[i], key->ol_info[i]);
		if (likely(key->bd_base_info[i] & BIT(HNS3_RXD_L3L4P_B)))
			hns3_rx_set_cksum_flag(rx_pkts[i],
					rx_pkts[i]->packet_type, cksum_err);
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
	uint64_t bd_err_mask;  /* bit mask indicate whick pkts is error */
	uint16_t nb_rx;

	rte_prefetch_non_temporal(rxdp);

	nb_pkts = RTE_MIN(nb_pkts, HNS3_DEFAULT_RX_BURST);
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, HNS3_SVE_DEFAULT_DESCS_PER_LOOP);

	if (rxq->rx_rearm_nb > HNS3_DEFAULT_RXQ_REARM_THRESH)
		hns3_rxq_rearm_mbuf_sve(rxq);

	if (unlikely(!(rxdp->rx.bd_base_info &
			rte_cpu_to_le_32(1u << HNS3_RXD_VLD_B))))
		return 0;

	hns3_rx_prefetch_mbuf_sve(&rxq->sw_ring[rxq->next_to_use]);

	bd_err_mask = 0;
	nb_rx = hns3_recv_burst_vec_sve(rxq, rx_pkts, nb_pkts, &bd_err_mask);
	if (unlikely(bd_err_mask))
		nb_rx = hns3_rx_reassemble_pkts(rx_pkts, nb_rx, bd_err_mask);

	return nb_rx;
}
