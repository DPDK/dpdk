/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "base/hinic3_compat.h"
#include "base/hinic3_hwif.h"
#include "base/hinic3_hwdev.h"
#include "base/hinic3_wq.h"
#include "base/hinic3_nic_cfg.h"
#include "hinic3_nic_io.h"
#include "hinic3_ethdev.h"
#include "hinic3_rx.h"

/**
 * Get wqe from receive queue.
 *
 * @param[in] rxq
 * Receive queue.
 * @param[out] rq_wqe
 * Receive queue wqe.
 * @param[out] pi
 * Current pi.
 */
static inline void
hinic3_get_rq_wqe(struct hinic3_rxq *rxq, struct hinic3_rq_wqe **rq_wqe, uint16_t *pi)
{
	*pi = MASKED_QUEUE_IDX(rxq, rxq->prod_idx);

	/* Get only one rxq wqe. */
	rxq->prod_idx++;
	rxq->delta--;

	*rq_wqe = NIC_WQE_ADDR(rxq, *pi);
}

/**
 * Put wqe into receive queue.
 *
 * @param[in] rxq
 * Receive queue.
 * @param[in] wqe_cnt
 * Wqebb counters.
 */
static inline void
hinic3_put_rq_wqe(struct hinic3_rxq *rxq, uint16_t wqe_cnt)
{
	rxq->delta += wqe_cnt;
	rxq->prod_idx -= wqe_cnt;
}

/**
 * Get receive queue local pi.
 *
 * @param[in] rxq
 * Receive queue.
 * @return
 * Receive queue local pi.
 */
static inline uint16_t
hinic3_get_rq_local_pi(struct hinic3_rxq *rxq)
{
	return MASKED_QUEUE_IDX(rxq, rxq->prod_idx);
}

uint16_t
hinic3_rx_fill_wqe(struct hinic3_rxq *rxq)
{
	struct hinic3_rq_wqe *rq_wqe = NULL;
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	rte_iova_t cqe_dma;
	uint16_t pi = 0;
	uint16_t i;

	cqe_dma = rxq->cqe_start_paddr;
	for (i = 0; i < rxq->q_depth; i++) {
		hinic3_get_rq_wqe(rxq, &rq_wqe, &pi);
		if (!rq_wqe) {
			PMD_DRV_LOG(ERR,
				    "Get rq wqe failed, rxq id: %d, wqe id: %d",
				    rxq->q_id, i);
			break;
		}

		if (rxq->wqe_type == HINIC3_EXTEND_RQ_WQE) {
			/* Unit of cqe length is 16B. */
			hinic3_set_sge(&rq_wqe->extend_wqe.cqe_sect.sge, cqe_dma,
				       HINIC3_CQE_LEN >> HINIC3_CQE_SIZE_SHIFT);
			/* Use fixed len. */
			rq_wqe->extend_wqe.buf_desc.sge.len = nic_dev->rx_buff_len;
		} else {
			rq_wqe->normal_wqe.cqe_hi_addr = upper_32_bits(cqe_dma);
			rq_wqe->normal_wqe.cqe_lo_addr = lower_32_bits(cqe_dma);
		}

		cqe_dma += sizeof(struct hinic3_rq_cqe);

		hinic3_cpu_to_hw(rq_wqe, rxq->wqebb_size);
	}

	hinic3_put_rq_wqe(rxq, i);

	return i;
}

static struct rte_mbuf *
hinic3_rx_alloc_mbuf(struct hinic3_rxq *rxq, rte_iova_t *dma_addr)
{
	struct rte_mbuf *mbuf = NULL;

	if (unlikely(rte_pktmbuf_alloc_bulk(rxq->mb_pool, &mbuf, 1) != 0))
		return NULL;

	*dma_addr = rte_mbuf_data_iova_default(mbuf);
#ifdef HINIC3_XSTAT_MBUF_USE
	rxq->rxq_stats.rx_alloc_mbuf_bytes++;
#endif
	return mbuf;
}

#ifdef HINIC3_XSTAT_RXBUF_INFO
static void
hinic3_rxq_buffer_done_count(struct hinic3_rxq *rxq)
{
	uint16_t sw_ci, avail_pkts = 0, hit_done = 0, cqe_hole = 0;
	RTE_ATOMIC(uint32_t)status;
	volatile struct hinic3_rq_cqe *rx_cqe;

	for (sw_ci = 0; sw_ci < rxq->q_depth; sw_ci++) {
		rx_cqe = &rxq->rx_cqe[sw_ci];

		/* Check current ci is done. */
		status = rx_cqe->status;
		if (!HINIC3_GET_RX_DONE(status)) {
			if (hit_done) {
				cqe_hole++;
				hit_done = 0;
			}
			continue;
		}

		avail_pkts++;
		hit_done = 1;
	}

	rxq->rxq_stats.rx_avail = avail_pkts;
	rxq->rxq_stats.rx_hole = cqe_hole;
}

void
hinic3_get_stats(struct hinic3_rxq *rxq)
{
	rxq->rxq_stats.rx_mbuf = rxq->q_depth - hinic3_get_rq_free_wqebb(rxq);

	hinic3_rxq_buffer_done_count(rxq);
}
#endif

uint16_t
hinic3_rx_fill_buffers(struct hinic3_rxq *rxq)
{
	struct hinic3_rq_wqe *rq_wqe = NULL;
	struct hinic3_rx_info *rx_info = NULL;
	struct rte_mbuf *mb = NULL;
	rte_iova_t dma_addr;
	uint16_t i, free_wqebbs;

	free_wqebbs = rxq->delta - 1;
	for (i = 0; i < free_wqebbs; i++) {
		rx_info = &rxq->rx_info[rxq->next_to_update];

		mb = hinic3_rx_alloc_mbuf(rxq, &dma_addr);
		if (!mb) {
			PMD_DRV_LOG(ERR, "Alloc mbuf failed");
			break;
		}

		rx_info->mbuf = mb;

		rq_wqe = NIC_WQE_ADDR(rxq, rxq->next_to_update);

		/* Fill buffer address only. */
		if (rxq->wqe_type == HINIC3_EXTEND_RQ_WQE) {
			rq_wqe->extend_wqe.buf_desc.sge.hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			rq_wqe->extend_wqe.buf_desc.sge.lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
		} else {
			rq_wqe->normal_wqe.buf_hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			rq_wqe->normal_wqe.buf_lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
		}

		rxq->next_to_update = (rxq->next_to_update + 1) & rxq->q_mask;
	}

	if (likely(i > 0)) {
		hinic3_write_db(rxq->db_addr, rxq->q_id, 0, RQ_CFLAG_DP,
				(rxq->next_to_update << rxq->wqe_type));
		/* Init rxq contxet used, need to optimization. */
		rxq->prod_idx = rxq->next_to_update;
		rxq->delta -= i;
	} else {
		PMD_DRV_LOG(ERR, "Alloc rx buffers failed, rxq_id: %d", rxq->q_id);
	}

	return i;
}

void
hinic3_free_rxq_mbufs(struct hinic3_rxq *rxq)
{
	struct hinic3_rx_info *rx_info = NULL;
	int free_wqebbs = hinic3_get_rq_free_wqebb(rxq) + 1;
	volatile struct hinic3_rq_cqe *rx_cqe = NULL;
	uint16_t ci;

	while (free_wqebbs++ < rxq->q_depth) {
		ci = hinic3_get_rq_local_ci(rxq);
		if (rxq->wqe_type != HINIC3_COMPACT_RQ_WQE) {
			rx_cqe = &rxq->rx_cqe[ci];
			/* Clear done bit. */
			rx_cqe->status = 0;
		}

		rx_info = &rxq->rx_info[ci];
		rte_pktmbuf_free(rx_info->mbuf);
		rx_info->mbuf = NULL;

		hinic3_update_rq_local_ci(rxq, 1);
#ifdef HINIC3_XSTAT_MBUF_USE
		rxq->rxq_stats.rx_free_mbuf_bytes++;
#endif
	}
}

void
hinic3_free_all_rxq_mbufs(struct hinic3_nic_dev *nic_dev)
{
	uint16_t qid;

	for (qid = 0; qid < nic_dev->num_rqs; qid++)
		hinic3_free_rxq_mbufs(nic_dev->rxqs[qid]);
}

static uint32_t
hinic3_rx_alloc_mbuf_bulk(struct hinic3_rxq *rxq, struct rte_mbuf **mbufs,
			  uint32_t exp_mbuf_cnt)
{
	uint32_t avail_cnt;
	int err;

	err = rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, exp_mbuf_cnt);
	if (likely(err == 0)) {
		avail_cnt = exp_mbuf_cnt;
	} else {
		avail_cnt = 0;
		rxq->rxq_stats.rx_nombuf += exp_mbuf_cnt;
	}
#ifdef HINIC3_XSTAT_MBUF_USE
	rxq->rxq_stats.rx_alloc_mbuf_bytes += avail_cnt;
#endif
	return avail_cnt;
}

static int
hinic3_rearm_rxq_mbuf(struct hinic3_rxq *rxq)
{
	struct hinic3_rq_wqe *rq_wqe = NULL;
	struct rte_mbuf **rearm_mbufs;
	uint32_t i, free_wqebbs, rearm_wqebbs, exp_wqebbs;
	rte_iova_t dma_addr;
	uint16_t pi;
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;

	/* Check free wqebb cnt fo rearm. */
	free_wqebbs = hinic3_get_rq_free_wqebb(rxq);
	if (unlikely(free_wqebbs < rxq->rx_free_thresh))
		return -ENOMEM;

	/* Get rearm mbuf array. */
	pi = hinic3_get_rq_local_pi(rxq);
	rearm_mbufs = (struct rte_mbuf **)(&rxq->rx_info[pi]);

	/* Check rxq free wqebbs turn around. */
	exp_wqebbs = rxq->q_depth - pi;
	if (free_wqebbs < exp_wqebbs)
		exp_wqebbs = free_wqebbs;

	/* Alloc mbuf in bulk. */
	rearm_wqebbs = hinic3_rx_alloc_mbuf_bulk(rxq, rearm_mbufs, exp_wqebbs);
	if (unlikely(rearm_wqebbs == 0))
		return -ENOMEM;

	/* Rearm rxq mbuf. */
	rq_wqe = NIC_WQE_ADDR(rxq, pi);
	for (i = 0; i < rearm_wqebbs; i++) {
		dma_addr = rte_mbuf_data_iova_default(rearm_mbufs[i]);

		/* Fill packet dma address into wqe. */
		if (rxq->wqe_type == HINIC3_EXTEND_RQ_WQE) {
			rq_wqe->extend_wqe.buf_desc.sge.hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			rq_wqe->extend_wqe.buf_desc.sge.lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
			rq_wqe->extend_wqe.buf_desc.sge.len =
				nic_dev->rx_buff_len;
		} else if (rxq->wqe_type == HINIC3_NORMAL_RQ_WQE) {
			rq_wqe->normal_wqe.buf_hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			rq_wqe->normal_wqe.buf_lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
		} else {
			rq_wqe->compact_wqe.buf_hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			rq_wqe->compact_wqe.buf_lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
		}

		rq_wqe =
			(struct hinic3_rq_wqe *)((uint64_t)rq_wqe + rxq->wqebb_size);
	}
	rxq->prod_idx += rearm_wqebbs;
	rxq->delta -= rearm_wqebbs;

	hinic3_write_db(rxq->db_addr, rxq->q_id, 0, RQ_CFLAG_DP,
			((pi + rearm_wqebbs) & rxq->q_mask) << rxq->wqe_type);
	return 0;
}

static int
hinic3_init_rss_key(struct hinic3_nic_dev *nic_dev,
		    struct rte_eth_rss_conf *rss_conf)
{
	uint8_t default_rss_key[HINIC3_RSS_KEY_SIZE] = {
			 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
			 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
			 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
			 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
			 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa};
	uint8_t hashkey[HINIC3_RSS_KEY_SIZE] = {0};
	int err;

	if (rss_conf->rss_key == NULL ||
	    rss_conf->rss_key_len > HINIC3_RSS_KEY_SIZE)
		memcpy(hashkey, default_rss_key, HINIC3_RSS_KEY_SIZE);
	else
		memcpy(hashkey, rss_conf->rss_key, rss_conf->rss_key_len);

	err = hinic3_rss_set_hash_key(nic_dev->hwdev, hashkey, HINIC3_RSS_KEY_SIZE);
	if (err)
		return err;

	memcpy(nic_dev->rss_key, hashkey, HINIC3_RSS_KEY_SIZE);
	return 0;
}

void
hinic3_add_rq_to_rx_queue_list(struct hinic3_nic_dev *nic_dev, uint16_t queue_id)
{
	uint16_t rss_queue_count = nic_dev->num_rss;

	RTE_ASSERT(rss_queue_count <= (RTE_DIM(nic_dev->rx_queue_list) - 1));

	nic_dev->rx_queue_list[rss_queue_count] = (uint8_t)queue_id;
	nic_dev->num_rss++;
}

void
hinic3_init_rx_queue_list(struct hinic3_nic_dev *nic_dev)
{
	nic_dev->num_rss = 0;
}

static void
hinic3_fill_indir_tbl(struct hinic3_nic_dev *nic_dev, uint32_t *indir_tbl)
{
	uint16_t rss_queue_count = nic_dev->num_rss;
	int i = 0;
	int j;

	if (rss_queue_count == 0) {
		/* Delete q_id from indir tbl. */
		for (i = 0; i < HINIC3_RSS_INDIR_SIZE; i++)
			/* Invalid value in indir tbl. */
			indir_tbl[i] = 0xFFFF;
	} else {
		while (i < HINIC3_RSS_INDIR_SIZE)
			for (j = 0; (j < rss_queue_count) &&
				    (i < HINIC3_RSS_INDIR_SIZE); j++)
				indir_tbl[i++] = nic_dev->rx_queue_list[j];
	}
}

int
hinic3_refill_indir_rqid(struct hinic3_rxq *rxq)
{
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	uint32_t *indir_tbl;
	int err;

	indir_tbl = rte_zmalloc(NULL, HINIC3_RSS_INDIR_SIZE * sizeof(uint32_t), 0);
	if (!indir_tbl) {
		PMD_DRV_LOG(ERR,
			    "Alloc indir_tbl mem failed, eth_dev:%s, queue_idx:%d",
			    nic_dev->dev_name, rxq->q_id);
		return -ENOMEM;
	}

	/* Build indir tbl according to the number of rss queue. */
	hinic3_fill_indir_tbl(nic_dev, indir_tbl);

	err = hinic3_rss_set_indir_tbl(nic_dev->hwdev, indir_tbl);
	if (err) {
		PMD_DRV_LOG(ERR,
			"Set indirect table failed, eth_dev:%s, queue_idx:%d",
			nic_dev->dev_name, rxq->q_id);
		goto out;
	}

out:
	rte_free(indir_tbl);
	return err;
}

static int
hinic3_init_rss_type(struct hinic3_nic_dev *nic_dev,
		     struct rte_eth_rss_conf *rss_conf)
{
	struct hinic3_rss_type rss_type = {0};
	uint64_t rss_hf = rss_conf->rss_hf;
	int err;

	rss_type.ipv4 = (rss_hf & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4)) ? 1 : 0;
	rss_type.tcp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) ? 1 : 0;
	rss_type.ipv6 = (rss_hf & (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6)) ? 1 : 0;
	rss_type.tcp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) ? 1 : 0;
	rss_type.udp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) ? 1 : 0;
	rss_type.udp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) ? 1 : 0;

	if (nic_dev->feature_cap & NIC_F_HTN_CMDQ) {
		rss_type.ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_EX) ? 1 : 0;
		rss_type.tcp_ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_TCP_EX) ? 1 : 0;
	} else {
		rss_type.ipv6_ext = 0;
		rss_type.tcp_ipv6_ext = 0;
	}

	err = hinic3_set_rss_type(nic_dev->hwdev, rss_type);
	return err;
}

int
hinic3_update_rss_config(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint8_t prio_tc[HINIC3_DCB_UP_MAX] = {0};
	uint8_t num_tc = 0;
	int err;

	if (rss_conf->rss_hf == 0) {
		rss_conf->rss_hf = HINIC3_RSS_OFFLOAD_ALL;
	} else if ((rss_conf->rss_hf & HINIC3_RSS_OFFLOAD_ALL) == 0) {
		PMD_DRV_LOG(ERR, "Doesn't support rss hash type: %" PRIu64,
			    rss_conf->rss_hf);
		return -EINVAL;
	}

	err = hinic3_rss_template_alloc(nic_dev->hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc rss template failed, err: %d", err);
		return err;
	}

	err = hinic3_init_rss_key(nic_dev, rss_conf);
	if (err) {
		PMD_DRV_LOG(ERR, "Init rss hash key failed, err: %d", err);
		goto init_rss_fail;
	}

	err = hinic3_init_rss_type(nic_dev, rss_conf);
	if (err) {
		PMD_DRV_LOG(ERR, "Init rss hash type failed, err: %d", err);
		goto init_rss_fail;
	}

	err = hinic3_rss_set_hash_engine(nic_dev->hwdev,
					 HINIC3_RSS_HASH_ENGINE_TYPE_TOEP);
	if (err) {
		PMD_DRV_LOG(ERR, "Init rss hash function failed, err: %d", err);
		goto init_rss_fail;
	}

	err = hinic3_rss_cfg(nic_dev->hwdev, HINIC3_RSS_ENABLE, num_tc, prio_tc);
	if (err) {
		PMD_DRV_LOG(ERR, "Enable rss failed, err: %d", err);
		goto init_rss_fail;
	}

	nic_dev->rss_state = HINIC3_RSS_ENABLE;
	return 0;

init_rss_fail:
	if (hinic3_rss_template_free(nic_dev->hwdev))
		PMD_DRV_LOG(WARNING, "Free rss template failed");

	return err;
}

/**
 * Search given queue array to find position of given id.
 * Return queue pos or queue_count if not found.
 */
static uint8_t
hinic3_find_queue_pos_by_rq_id(uint8_t *queues, uint8_t queues_count, uint8_t queue_id)
{
	uint8_t pos;

	for (pos = 0; pos < queues_count; pos++) {
		if (queue_id == queues[pos])
			break;
	}

	return pos;
}

void
hinic3_remove_rq_from_rx_queue_list(struct hinic3_nic_dev *nic_dev,
				    uint16_t queue_id)
{
	uint8_t queue_pos;
	uint16_t rss_queue_count = nic_dev->num_rss;

	queue_pos = hinic3_find_queue_pos_by_rq_id(nic_dev->rx_queue_list,
						   rss_queue_count, queue_id);
	/*
	 * If queue was not at the end of the list,
	 * shift started queues up queue array list.
	 */
	if (queue_pos < rss_queue_count) {
		rss_queue_count--;
		memmove(nic_dev->rx_queue_list + queue_pos,
			nic_dev->rx_queue_list + queue_pos + 1,
			(rss_queue_count - queue_pos) * sizeof(nic_dev->rx_queue_list[0]));
	}

	RTE_ASSERT(rss_queue_count < RTE_DIM(nic_dev->rx_queue_list));
	nic_dev->num_rss = rss_queue_count;
}

static void
hinic3_rx_queue_release_mbufs(struct hinic3_rxq *rxq)
{
	uint16_t sw_ci, ci_mask, free_wqebbs;
	uint16_t rx_buf_len;
	uint32_t vlan_len, pkt_len;
	RTE_ATOMIC(uint32_t)status;
	uint32_t pkt_left_len = 0;
	uint32_t nr_released = 0;
	struct hinic3_rx_info *rx_info;
	volatile struct hinic3_rq_cqe *rx_cqe;

	sw_ci = hinic3_get_rq_local_ci(rxq);
	rx_info = &rxq->rx_info[sw_ci];
	rx_cqe = &rxq->rx_cqe[sw_ci];
	free_wqebbs = hinic3_get_rq_free_wqebb(rxq) + 1;
	status = rx_cqe->status;
	ci_mask = rxq->q_mask;

	while (free_wqebbs < rxq->q_depth) {
		rx_buf_len = rxq->buf_len;
		if (pkt_left_len != 0) {
			/* Flush continues jumbo rqe. */
			pkt_left_len = (pkt_left_len <= rx_buf_len)
					       ? 0 : (pkt_left_len - rx_buf_len);
		} else if (HINIC3_GET_RX_FLUSH(status)) {
			/* Flush one released rqe. */
			pkt_left_len = 0;
		} else if (HINIC3_GET_RX_DONE(status)) {
			/* Flush single packet or first jumbo rqe. */
			vlan_len = hinic3_hw_cpu32(rx_cqe->vlan_len);
			pkt_len = HINIC3_GET_RX_PKT_LEN(vlan_len);
			pkt_left_len = (pkt_len <= rx_buf_len)
					       ? 0 : (pkt_len - rx_buf_len);
		} else {
			break;
		}
		rte_pktmbuf_free(rx_info->mbuf);

		rx_info->mbuf = NULL;
		rx_cqe->status = 0;
		nr_released++;
		free_wqebbs++;

		/* Update ci to next cqe. */
		sw_ci++;
		sw_ci &= ci_mask;
		rx_info = &rxq->rx_info[sw_ci];
		rx_cqe = &rxq->rx_cqe[sw_ci];
		status = rx_cqe->status;
	}

	hinic3_update_rq_local_ci(rxq, nr_released);
}

int
hinic3_poll_rq_empty(struct hinic3_rxq *rxq)
{
	uint64_t end;
	int free_wqebb;
	int err = -EFAULT;

	end = msecs_to_cycles(HINIC3_FLUSH_QUEUE_TIMEOUT) + cycles;
	do {
		free_wqebb = hinic3_get_rq_free_wqebb(rxq) + 1;
		if (free_wqebb == rxq->q_depth) {
			err = 0;
			break;
		}
		hinic3_rx_queue_release_mbufs(rxq);
		rte_delay_us(1);
	} while (time_before(cycles, end));

	return err;
}

int
hinic3_poll_integrated_cqe_rq_empty(struct hinic3_rxq *rxq)
{
	struct hinic3_rx_info *rx_info;
	struct hinic3_rq_ci_wb rq_ci;
	uint16_t sw_ci;
	uint16_t hw_ci;
	uint32_t val;

	sw_ci = hinic3_get_rq_local_ci(rxq);
	val = rte_read32(&rxq->rq_ci->dw1.value);
	rq_ci.dw1.value = hinic3_hw_cpu32(val);
	hw_ci = rq_ci.dw1.bs.hw_ci;

	while (sw_ci != hw_ci) {
		rx_info = &rxq->rx_info[sw_ci];
		rte_pktmbuf_free(rx_info->mbuf);
		rx_info->mbuf = NULL;

		sw_ci++;
		sw_ci &= rxq->q_mask;
		hinic3_update_rq_local_ci(rxq, 1);
	}

	return 0;
}

void
hinic3_dump_cqe_status(struct hinic3_rxq *rxq, uint32_t *cqe_done_cnt,
		       uint32_t *cqe_hole_cnt, uint32_t *head_ci, uint32_t *head_done)
{
	uint16_t sw_ci;
	uint16_t avail_pkts = 0;
	uint16_t hit_done = 0;
	uint16_t cqe_hole = 0;
	RTE_ATOMIC(uint32_t)status;
	volatile struct hinic3_rq_cqe *rx_cqe;

	sw_ci = hinic3_get_rq_local_ci(rxq);
	rx_cqe = &rxq->rx_cqe[sw_ci];
	status = rx_cqe->status;
	*head_done = HINIC3_GET_RX_DONE(status);
	*head_ci = sw_ci;

	for (sw_ci = 0; sw_ci < rxq->q_depth; sw_ci++) {
		rx_cqe = &rxq->rx_cqe[sw_ci];

		/* Check current ci is done. */
		status = rx_cqe->status;
		if (!HINIC3_GET_RX_DONE(status) ||
		    !HINIC3_GET_RX_FLUSH(status)) {
			if (hit_done) {
				cqe_hole++;
				hit_done = 0;
			}

			continue;
		}

		avail_pkts++;
		hit_done = 1;
	}

	*cqe_done_cnt = avail_pkts;
	*cqe_hole_cnt = cqe_hole;
}

int
hinic3_stop_rq(struct rte_eth_dev *eth_dev, struct hinic3_rxq *rxq)
{
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	uint32_t cqe_done_cnt = 0;
	uint32_t cqe_hole_cnt = 0;
	uint32_t head_ci, head_done;
	int err;

	/* Disable rxq intr. */
	hinic3_dev_rx_queue_intr_disable(eth_dev, rxq->q_id);

	/* Lock dev queue switch. */
	rte_spinlock_lock(&nic_dev->queue_list_lock);

	if (nic_dev->num_rss == 1) {
		err = hinic3_set_vport_enable(nic_dev->hwdev, false);
		if (err) {
			PMD_DRV_LOG(ERR, "%s Disable vport failed, rc:%d",
				    nic_dev->dev_name, err);
		}
	}
	hinic3_remove_rq_from_rx_queue_list(nic_dev, rxq->q_id);

	/*
	 * If RSS is enable, remove q_id from rss indir table.
	 * If RSS is disable, no mbuf in rq, packet will be dropped.
	 */
	if (nic_dev->rss_state == HINIC3_RSS_ENABLE) {
		err = hinic3_refill_indir_rqid(rxq);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "Clear rq in indirect table failed, eth_dev:%s, queue_idx:%d",
				    nic_dev->dev_name, rxq->q_id);
			hinic3_add_rq_to_rx_queue_list(nic_dev, rxq->q_id);
			goto set_indir_failed;
		}
	}

	/* Unlock dev queue list switch. */
	rte_spinlock_unlock(&nic_dev->queue_list_lock);

	/* Send flush rxq cmd to device. */
	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) == 0)
		err = hinic3_set_rq_flush(nic_dev->hwdev, rxq->q_id);
	else
		err = hinic3_set_rq_enable(nic_dev, rxq->q_id, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Flush rq failed, eth_dev:%s, queue_idx:%d",
			    nic_dev->dev_name, rxq->q_id);
		goto rq_flush_failed;
	}

	err = nic_dev->rx_ops->nic_rx_poll_rq_empty(rxq);
	if (err) {
		hinic3_dump_cqe_status(rxq, &cqe_done_cnt, &cqe_hole_cnt,
				       &head_ci, &head_done);
		PMD_DRV_LOG(ERR, "Poll rq empty timeout");
		PMD_DRV_LOG(ERR,
			    "eth_dev:%s, queue_idx:%d, mbuf_left:%d, cqe_done:%d, cqe_hole:%d, cqe[%d].done=%d",
			    nic_dev->dev_name, rxq->q_id,
			    rxq->q_depth - hinic3_get_rq_free_wqebb(rxq),
			    cqe_done_cnt, cqe_hole_cnt, head_ci, head_done);
		goto poll_rq_failed;
	}

	return 0;

poll_rq_failed:
	hinic3_set_rq_enable(nic_dev, rxq->q_id, true);
rq_flush_failed:
	rte_spinlock_lock(&nic_dev->queue_list_lock);
set_indir_failed:
	hinic3_add_rq_to_rx_queue_list(nic_dev, rxq->q_id);
	if (nic_dev->rss_state == HINIC3_RSS_ENABLE)
		hinic3_refill_indir_rqid(rxq);
	rte_spinlock_unlock(&nic_dev->queue_list_lock);
	hinic3_dev_rx_queue_intr_enable(eth_dev, rxq->q_id);
	return err;
}

int
hinic3_start_rq(struct rte_eth_dev *eth_dev, struct hinic3_rxq *rxq)
{
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	int err = 0;

	/* Lock dev queue switch.  */
	rte_spinlock_lock(&nic_dev->queue_list_lock);
	hinic3_add_rq_to_rx_queue_list(nic_dev, rxq->q_id);

	if (nic_dev->rss_state == HINIC3_RSS_ENABLE) {
		if ((hinic3_get_driver_feature(nic_dev) & NIC_F_FDIR) != 0)
			err = hinic3_set_rq_enable(nic_dev, rxq->q_id, true);
		if (err) {
			PMD_DRV_LOG(ERR, "Flush rq failed, eth_dev:%s, queue_idx:%d",
					 nic_dev->dev_name, rxq->q_id);
		} else {
			err = hinic3_refill_indir_rqid(rxq);
			if (err) {
				PMD_DRV_LOG(ERR, "Refill rq to indirect table failed,"
						 "eth_dev:%s, queue_idx:%d err:%d",
						 nic_dev->dev_name, rxq->q_id, err);
				hinic3_remove_rq_from_rx_queue_list(nic_dev, rxq->q_id);
			}
		}
	}

	(void)hinic3_rearm_rxq_mbuf(rxq);
	if (rxq->nic_dev->num_rss == 1) {
		err = hinic3_set_vport_enable(nic_dev->hwdev, true);
		if (err)
			PMD_DRV_LOG(ERR, "%s enable vport failed, err:%d",
				    nic_dev->dev_name, err);
	}

	/* Unlock dev queue list switch. */
	rte_spinlock_unlock(&nic_dev->queue_list_lock);

	hinic3_dev_rx_queue_intr_enable(eth_dev, rxq->q_id);

	return err;
}

static inline uint64_t
hinic3_rx_vlan(uint8_t vlan_offload, uint16_t vlan_tag, uint16_t *vlan_tci)
{
	if (!vlan_offload || vlan_tag == 0) {
		*vlan_tci = 0;
		return 0;
	}

	*vlan_tci = vlan_tag;

	return HINIC3_PKT_RX_VLAN | HINIC3_PKT_RX_VLAN_STRIPPED;
}

static inline uint64_t
hinic3_rx_csum(uint16_t csum_err, struct hinic3_rxq *rxq)
{
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	uint64_t flags;

	if (unlikely(!(nic_dev->rx_csum_en & HINIC3_DEFAULT_RX_CSUM_OFFLOAD)))
		return HINIC3_PKT_RX_IP_CKSUM_UNKNOWN;

	if (likely(csum_err == 0))
		return (HINIC3_PKT_RX_IP_CKSUM_GOOD |
			HINIC3_PKT_RX_L4_CKSUM_GOOD);

	/*
	 * If bypass bit is set, all other err status indications should be
	 * ignored.
	 */
	if (unlikely(csum_err & HINIC3_RX_CSUM_HW_CHECK_NONE))
		return HINIC3_PKT_RX_IP_CKSUM_UNKNOWN;

	flags = 0;

	/* IP checksum error. */
	if (csum_err & HINIC3_RX_CSUM_IP_CSUM_ERR) {
		flags |= HINIC3_PKT_RX_IP_CKSUM_BAD;
		rxq->rxq_stats.csum_errors++;
	}

	/* L4 checksum error. */
	if ((csum_err & HINIC3_RX_CSUM_TCP_CSUM_ERR) ||
	    (csum_err & HINIC3_RX_CSUM_UDP_CSUM_ERR) ||
	    (csum_err & HINIC3_RX_CSUM_SCTP_CRC_ERR)) {
		flags |= HINIC3_PKT_RX_L4_CKSUM_BAD;
		rxq->rxq_stats.csum_errors++;
	}

	if (unlikely(csum_err == HINIC3_RX_CSUM_IPSU_OTHER_ERR))
		rxq->rxq_stats.other_errors++;

	return flags;
}

static inline uint64_t
hinic3_rx_rss_hash(uint32_t rss_type, uint32_t rss_hash_value, uint32_t *rss_hash)
{

	if (likely(rss_type != 0)) {
		*rss_hash = rss_hash_value;
		return HINIC3_PKT_RX_RSS_HASH;
	}

	return 0;
}

static void
hinic3_recv_jumbo_pkt(struct hinic3_rxq *rxq, struct rte_mbuf *head_mbuf,
		      uint32_t remain_pkt_len)
{
	struct rte_mbuf *cur_mbuf = NULL;
	struct rte_mbuf *rxm = NULL;
	struct hinic3_rx_info *rx_info = NULL;
	uint16_t sw_ci, rx_buf_len = rxq->buf_len;
	uint32_t pkt_len;

	while (remain_pkt_len > 0) {
		sw_ci = hinic3_get_rq_local_ci(rxq);
		rx_info = &rxq->rx_info[sw_ci];

		hinic3_update_rq_local_ci(rxq, 1);

		pkt_len = remain_pkt_len > rx_buf_len ? rx_buf_len
						      : remain_pkt_len;
		remain_pkt_len -= pkt_len;

		cur_mbuf = rx_info->mbuf;
		cur_mbuf->data_len = (uint16_t)pkt_len;
		cur_mbuf->next = NULL;

		head_mbuf->pkt_len += cur_mbuf->data_len;
		head_mbuf->nb_segs++;
#ifdef HINIC3_XSTAT_MBUF_USE
		rxq->rxq_stats.rx_free_mbuf_bytes++;
#endif
		if (!rxm)
			head_mbuf->next = cur_mbuf;
		else
			rxm->next = cur_mbuf;

		rxm = cur_mbuf;
	}
}

int
hinic3_start_all_rqs(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev = NULL;
	struct hinic3_rxq *rxq = NULL;
	int err = 0;
	int i;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (i = 0; i < nic_dev->num_rqs; i++) {
		rxq = eth_dev->data->rx_queues[i];
		hinic3_add_rq_to_rx_queue_list(nic_dev, rxq->q_id);
		err = hinic3_rearm_rxq_mbuf(rxq);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "Fail to alloc mbuf for Rx queue %d, qid = %u, need_mbuf: %d",
				    i, rxq->q_id, rxq->q_depth);
			goto out;
		}
		hinic3_dev_rx_queue_intr_enable(eth_dev, rxq->q_id);
		if (rxq->rx_deferred_start)
			continue;
		eth_dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	if (nic_dev->rss_state == HINIC3_RSS_ENABLE) {
		err = hinic3_refill_indir_rqid(rxq);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "Refill rq to indirect table failed, eth_dev:%s, queue_idx:%d, err:%d",
				    rxq->nic_dev->dev_name, rxq->q_id, err);
			goto out;
		}
	}

	return 0;
out:
	for (i = 0; i < nic_dev->num_rqs; i++) {
		rxq = eth_dev->data->rx_queues[i];
		hinic3_remove_rq_from_rx_queue_list(nic_dev, rxq->q_id);
		hinic3_free_rxq_mbufs(rxq);
		hinic3_dev_rx_queue_intr_disable(eth_dev, rxq->q_id);
		eth_dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	return err;
}

bool
hinic3_rx_separate_cqe_done(struct hinic3_rxq *rxq, volatile struct hinic3_rq_cqe **rx_cqe)
{
	volatile struct hinic3_rq_cqe *cqe = NULL;
	uint16_t sw_ci;
	uint32_t status;

	sw_ci = hinic3_get_rq_local_ci(rxq);
	*rx_cqe = &rxq->rx_cqe[sw_ci];
	cqe = *rx_cqe;

	status = hinic3_hw_cpu32((uint32_t)(rte_atomic_load_explicit(&cqe->status,
								     rte_memory_order_acquire)));
	if (!HINIC3_GET_RX_DONE(status))
		return false;

	return true;
}

bool
hinic3_rx_integrated_cqe_done(struct hinic3_rxq *rxq, volatile struct hinic3_rq_cqe **rx_cqe)
{
	struct hinic3_rq_ci_wb rq_ci;
	struct rte_mbuf *rxm = NULL;
	uint16_t sw_ci, hw_ci;
	uint32_t val;

	sw_ci = hinic3_get_rq_local_ci(rxq);
	val = rte_read32(&rxq->rq_ci->dw1.value);
	rq_ci.dw1.value = hinic3_hw_cpu32(val);
	hw_ci = rq_ci.dw1.bs.hw_ci;

	if (hw_ci == sw_ci)
		return false;

	rxm = rxq->rx_info[sw_ci].mbuf;

	*rx_cqe = (volatile struct hinic3_rq_cqe *)rte_mbuf_data_addr_default(rxm);

	return true;
}

void
hinic3_rx_get_cqe_info(struct hinic3_rxq *rxq __rte_unused, volatile struct hinic3_rq_cqe *rx_cqe,
		       struct hinic3_cqe_info *cqe_info)
{
	uint32_t dw0 = hinic3_hw_cpu32(rx_cqe->status);
	uint32_t dw1 = hinic3_hw_cpu32(rx_cqe->vlan_len);
	uint32_t dw2 = hinic3_hw_cpu32(rx_cqe->offload_type);
	uint32_t dw3 = hinic3_hw_cpu32(rx_cqe->hash_val);

	cqe_info->lro_num = RQ_CQE_STATUS_GET(dw0, NUM_LRO);
	cqe_info->csum_err = RQ_CQE_STATUS_GET(dw0, CSUM_ERR);

	cqe_info->pkt_len = RQ_CQE_SGE_GET(dw1, LEN);
	cqe_info->vlan_tag = RQ_CQE_SGE_GET(dw1, VLAN);

	cqe_info->ptype = HINIC3_GET_RX_PTYPE_OFFLOAD(dw0);
	cqe_info->vlan_offload = RQ_CQE_OFFOLAD_TYPE_GET(dw2, VLAN_EN);
	cqe_info->rss_type = RQ_CQE_OFFOLAD_TYPE_GET(dw2, RSS_TYPE);
	cqe_info->rss_hash_value = dw3;
}

void
hinic3_rx_get_compact_cqe_info(struct hinic3_rxq *rxq, volatile struct hinic3_rq_cqe *rx_cqe,
			       struct hinic3_cqe_info *cqe_info)
{
	uint32_t dw0, dw1, dw2;

	if (rxq->wqe_type != HINIC3_COMPACT_RQ_WQE) {
		dw0 = hinic3_hw_cpu32(rx_cqe->status);
		dw1 = hinic3_hw_cpu32(rx_cqe->vlan_len);
		dw2 = hinic3_hw_cpu32(rx_cqe->offload_type);
	} else {
		/* Compact Rx CQE mode integrates cqe with packet in big endian way. */
		dw0 = rte_be_to_cpu_32(rx_cqe->status);
		dw1 = rte_be_to_cpu_32(rx_cqe->vlan_len);
		dw2 = rte_be_to_cpu_32(rx_cqe->offload_type);
	}

	cqe_info->cqe_type = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, CQE_TYPE);
	cqe_info->csum_err = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, CSUM_ERR);
	cqe_info->vlan_offload = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, VLAN_EN);
	cqe_info->cqe_len = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, CQE_LEN);
	cqe_info->pkt_len = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, PKT_LEN);
	cqe_info->ts_flag = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, TS_FLAG);
	cqe_info->ptype = HINIC3_RQ_COMPACT_CQE_STATUS_GET(dw0, PTYPE);
	cqe_info->rss_hash_value = dw1;

	if (cqe_info->cqe_len == HINIC3_RQ_COMPACT_CQE_16BYTE) {
		cqe_info->lro_num = HINIC3_RQ_COMPACT_CQE_OFFLOAD_GET(dw2, NUM_LRO);
		cqe_info->vlan_tag = HINIC3_RQ_COMPACT_CQE_OFFLOAD_GET(dw2, VLAN);
	}

	if (cqe_info->cqe_type == HINIC3_RQ_CQE_INTEGRATE)
		cqe_info->data_offset =
				(cqe_info->cqe_len == HINIC3_RQ_COMPACT_CQE_16BYTE) ? 16 : 8;
}

#define HINIC3_RX_EMPTY_THRESHOLD 3
uint16_t
hinic3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct hinic3_rxq *rxq = rx_queue;
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;
	struct hinic3_rx_info *rx_info = NULL;
	volatile struct hinic3_rq_cqe *rx_cqe = NULL;
	struct hinic3_cqe_info cqe_info = {0};
	struct rte_mbuf *rxm = NULL;
	uint16_t sw_ci, rx_buf_len, pkts = 0;
	uint32_t pkt_len;
	uint64_t rx_bytes = 0;

#ifdef HINIC3_XSTAT_PROF_RX
	uint64_t t1 = rte_get_tsc_cycles();
	uint64_t t2;
#endif
	if (((rte_get_timer_cycles() - rxq->rxq_stats.tsc) < rxq->wait_time_cycle) &&
	    rxq->rxq_stats.empty >= HINIC3_RX_EMPTY_THRESHOLD)
		goto out;

	sw_ci = hinic3_get_rq_local_ci(rxq);

	while (pkts < nb_pkts) {
		rx_cqe = &rxq->rx_cqe[sw_ci];
		if (!nic_dev->rx_ops->nic_rx_cqe_done(rxq, &rx_cqe)) {
			rxq->rxq_stats.empty++;
			break;
		}

		nic_dev->rx_ops->nic_rx_get_cqe_info(rxq, rx_cqe, &cqe_info);

		pkt_len = cqe_info.pkt_len;
		/*
		 * Compact Rx CQE mode integrates cqe with packet,
		 * so mbuf length needs to remove the length of cqe.
		 */
		rx_buf_len = rxq->buf_len - cqe_info.data_offset;

		rx_info = &rxq->rx_info[sw_ci];
		rxm = rx_info->mbuf;

		/* 1. Next ci point and prefetch. */
		sw_ci++;
		sw_ci &= rxq->q_mask;

		/* 2. Prefetch next mbuf first 64B. */
		rte_prefetch0(rxq->rx_info[sw_ci].mbuf);

		/* 3. Jumbo frame process. */
		if (likely(pkt_len <= rx_buf_len)) {
			rxm->data_len = (uint16_t)pkt_len;
			rxm->pkt_len = pkt_len;
			hinic3_update_rq_local_ci(rxq, 1);
		} else {
			rxm->data_len = rx_buf_len;
			rxm->pkt_len = rx_buf_len;

			/*
			 * If receive jumbo, updating ci will be done by
			 * hinic3_recv_jumbo_pkt function.
			 */
			hinic3_update_rq_local_ci(rxq, 1);
			hinic3_recv_jumbo_pkt(rxq, rxm, pkt_len - rx_buf_len);
			sw_ci = hinic3_get_rq_local_ci(rxq);
		}

		rxm->data_off = RTE_PKTMBUF_HEADROOM + cqe_info.data_offset;
		rxm->port = rxq->port_id;

		/* 4. Rx checksum offload. */
		rxm->ol_flags |= hinic3_rx_csum(cqe_info.csum_err, rxq);

		/* 5. Vlan offload. */
		rxm->ol_flags |= hinic3_rx_vlan(cqe_info.vlan_offload, cqe_info.vlan_tag,
						&rxm->vlan_tci);

		/* 6. RSS. */
		rxm->ol_flags |= hinic3_rx_rss_hash(cqe_info.rss_type, cqe_info.rss_hash_value,
						    &rxm->hash.rss);
		/* 8. LRO. */
		if (unlikely(cqe_info.lro_num != 0)) {
			rxm->ol_flags |= HINIC3_PKT_RX_LRO;
			rxm->tso_segsz = pkt_len / cqe_info.lro_num;
		}

		rx_cqe->status = 0;

		rx_bytes += pkt_len;
		rx_pkts[pkts++] = rxm;
	}

	if (pkts) {
		/* Update packet stats. */
		rxq->rxq_stats.packets += pkts;
		rxq->rxq_stats.bytes += rx_bytes;
		rxq->rxq_stats.empty = 0;
#ifdef HINIC3_XSTAT_MBUF_USE
		rxq->rxq_stats.rx_free_mbuf_bytes += pkts;
#endif
	}
	rxq->rxq_stats.burst_pkts = pkts;
	rxq->rxq_stats.tsc = rte_get_timer_cycles();
out:
	/* 10. Rearm mbuf to rxq. */
	hinic3_rearm_rxq_mbuf(rxq);

#ifdef HINIC3_XSTAT_PROF_RX
	/* Do profiling stats. */
	t2 = rte_get_tsc_cycles();
	rxq->rxq_stats.app_tsc = t1 - rxq->prof_rx_end_tsc;
	rxq->prof_rx_end_tsc = t2;
	rxq->rxq_stats.pmd_tsc = t2 - t1;
#endif

	return pkts;
}
