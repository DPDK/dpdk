/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "base/hinic3_compat.h"
#include "base/hinic3_nic_cfg.h"
#include "base/hinic3_hwdev.h"
#include "hinic3_nic_io.h"
#include "hinic3_ethdev.h"
#include "hinic3_tx.h"

#define HINIC3_TX_TASK_WRAPPED	  1
#define HINIC3_TX_BD_DESC_WRAPPED 2

#define TX_MSS_DEFAULT 0x3E00
#define TX_MSS_MIN     0x50

#define HINIC3_MAX_TX_FREE_BULK 64

#define MAX_PAYLOAD_OFFSET 221

#define HINIC3_TX_OUTER_CHECKSUM_FLAG_SET    1
#define HINIC3_TX_OUTER_CHECKSUM_FLAG_NO_SET 0
#define MAX_TSO_NUM_FRAG 1024

#define HINIC3_TX_OFFLOAD_MASK \
	(HINIC3_TX_CKSUM_OFFLOAD_MASK | HINIC3_PKT_TX_VLAN_PKT)

#define HINIC3_TX_CKSUM_OFFLOAD_MASK                          \
	(HINIC3_PKT_TX_IP_CKSUM | HINIC3_PKT_TX_TCP_CKSUM |   \
	 HINIC3_PKT_TX_UDP_CKSUM | HINIC3_PKT_TX_SCTP_CKSUM | \
	 HINIC3_PKT_TX_OUTER_IP_CKSUM | HINIC3_PKT_TX_OUTER_UDP_CKSUM | \
	 HINIC3_PKT_TX_TCP_SEG)

static inline uint16_t
hinic3_get_sq_free_wqebbs(struct hinic3_txq *sq)
{
	return ((sq->q_depth -
		 (((sq->prod_idx - sq->cons_idx) + sq->q_depth) & sq->q_mask)) - 1);
}

static inline void
hinic3_update_sq_local_ci(struct hinic3_txq *sq, uint16_t wqe_cnt)
{
	sq->cons_idx += wqe_cnt;
}

static inline uint16_t
hinic3_get_sq_local_ci(struct hinic3_txq *sq)
{
	return MASKED_QUEUE_IDX(sq, sq->cons_idx);
}

static inline uint16_t
hinic3_get_sq_hw_ci(struct hinic3_txq *sq)
{
	return MASKED_QUEUE_IDX(sq, hinic3_hw_cpu16(*sq->ci_vaddr_base));
}

static void *
hinic3_sq_get_wqebbs(struct hinic3_txq *sq, uint16_t num_wqebbs, uint16_t *prod_idx)
{
	*prod_idx = MASKED_QUEUE_IDX(sq, sq->prod_idx);
	sq->prod_idx += num_wqebbs;

	return NIC_WQE_ADDR(sq, *prod_idx);
}

static inline uint16_t
hinic3_get_and_update_sq_owner(struct hinic3_txq *sq, uint16_t curr_pi, uint16_t wqebb_cnt)
{
	uint16_t owner = sq->owner;

	if (unlikely(curr_pi + wqebb_cnt >= sq->q_depth))
		sq->owner = !sq->owner;

	return owner;
}

static inline void
hinic3_put_sq_wqe(struct hinic3_txq *sq, struct hinic3_wqe_info *wqe_info)
{
	if (wqe_info->owner != sq->owner)
		sq->owner = wqe_info->owner;

	sq->prod_idx -= wqe_info->wqebb_cnt;
}

/**
 * Sets the WQE combination information in the transmit queue (SQ).
 *
 * @param[in] sq
 * Point to send queue.
 * @param[out] wqe_combo
 * Point to wqe_combo of send queue(SQ).
 * @param[in] wqe_info
 * Point to wqe_info of send queue(SQ).
 */
static void
hinic3_set_wqe_combo(struct hinic3_txq *sq,
		     struct hinic3_sq_wqe_combo *wqe_combo,
		     struct hinic3_wqe_info *wqe_info)
{
	uint16_t tmp_pi;

	wqe_combo->hdr = hinic3_sq_get_wqebbs(sq, 1, &wqe_info->pi);

	if (wqe_info->wqebb_cnt == 1) {
		/* compact wqe */
		wqe_combo->wqe_type = SQ_WQE_COMPACT_TYPE;
		wqe_combo->task_type = SQ_WQE_TASKSECT_4BYTES;
		wqe_combo->task = (struct hinic3_sq_task *)&wqe_combo->hdr->queue_info;
		wqe_info->owner = hinic3_get_and_update_sq_owner(sq, wqe_info->pi, 1);
		return;
	}

	/* extend normal wqe */
	wqe_combo->wqe_type = SQ_WQE_EXTENDED_TYPE;
	wqe_combo->task_type = SQ_WQE_TASKSECT_16BYTES;
	wqe_combo->task = hinic3_sq_get_wqebbs(sq, 1, &tmp_pi);
	if (wqe_info->sge_cnt > 1)
		wqe_combo->bds_head = hinic3_sq_get_wqebbs(sq, wqe_info->sge_cnt - 1, &tmp_pi);

	wqe_info->owner = hinic3_get_and_update_sq_owner(sq, wqe_info->pi, wqe_info->wqebb_cnt);
}

int
hinic3_start_all_sqs(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev = NULL;
	struct hinic3_txq *txq = NULL;
	int i;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (i = 0; i < nic_dev->num_sqs; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq->tx_deferred_start)
			continue;
		HINIC3_SET_TXQ_STARTED(txq);
		eth_dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

static inline void
hinic3_free_cpy_mbuf(struct hinic3_nic_dev *nic_dev __rte_unused,
		     struct rte_mbuf *cpy_skb)
{
	rte_pktmbuf_free(cpy_skb);
}

/**
 * Cleans up buffers (mbuf) in the send queue (txq) and returns these buffers to
 * their memory pool.
 *
 * @param[in] txq
 * Point to send queue.
 * @param[in] free_cnt
 * Number of mbufs to be released.
 * @return
 * Number of released mbufs.
 */
static int
hinic3_xmit_mbuf_cleanup(struct hinic3_txq *txq, uint32_t free_cnt)
{
	struct hinic3_tx_info *tx_info = NULL;
	struct rte_mbuf *mbuf = NULL;
	struct rte_mbuf *mbuf_temp = NULL;
	struct rte_mbuf *mbuf_free[HINIC3_MAX_TX_FREE_BULK];

	int nb_free = 0;
	int wqebb_cnt = 0;
	uint16_t hw_ci, sw_ci, sq_mask;
	uint32_t i;

	hw_ci = hinic3_get_sq_hw_ci(txq);
	sw_ci = hinic3_get_sq_local_ci(txq);
	sq_mask = txq->q_mask;

	for (i = 0; i < free_cnt; ++i) {
		tx_info = &txq->tx_info[sw_ci];
		if (hw_ci == sw_ci ||
		    (((hw_ci - sw_ci) & sq_mask) < tx_info->wqebb_cnt))
			break;
		/*
		 * The cpy_mbuf is usually used in the arge-sized package
		 * scenario.
		 */
		if (unlikely(tx_info->cpy_mbuf != NULL)) {
			hinic3_free_cpy_mbuf(txq->nic_dev, tx_info->cpy_mbuf);
			tx_info->cpy_mbuf = NULL;
		}
		sw_ci = (sw_ci + tx_info->wqebb_cnt) & sq_mask;

		wqebb_cnt += tx_info->wqebb_cnt;
		mbuf = tx_info->mbuf;

		if (likely(mbuf->nb_segs == 1)) {
			mbuf_temp = rte_pktmbuf_prefree_seg(mbuf);
			tx_info->mbuf = NULL;
			if (unlikely(mbuf_temp == NULL))
				continue;

			mbuf_free[nb_free++] = mbuf_temp;
			/*
			 * If the pools of different mbufs are different,
			 * release the mbufs of the same pool.
			 */
			if (unlikely(mbuf_temp->pool != mbuf_free[0]->pool ||
				     nb_free >= HINIC3_MAX_TX_FREE_BULK)) {
				rte_mempool_put_bulk(mbuf_free[0]->pool,
						     (void **)mbuf_free,
						     (nb_free - 1));
				nb_free = 0;
				mbuf_free[nb_free++] = mbuf_temp;
			}
		} else {
			rte_pktmbuf_free(mbuf);
			tx_info->mbuf = NULL;
		}
	}

	if (nb_free > 0)
		rte_mempool_put_bulk(mbuf_free[0]->pool, (void **)mbuf_free, nb_free);

	hinic3_update_sq_local_ci(txq, wqebb_cnt);

	return i;
}

static inline void
hinic3_tx_free_mbuf_force(struct hinic3_txq *txq __rte_unused,
			  struct rte_mbuf *mbuf)
{
	rte_pktmbuf_free(mbuf);
}

/**
 * Release the mbuf and update the consumer index for sending queue.
 *
 * @param[in] txq
 * Point to send queue.
 */
void
hinic3_free_txq_mbufs(struct hinic3_txq *txq)
{
	struct hinic3_tx_info *tx_info = NULL;
	uint16_t free_wqebbs;
	uint16_t ci;

	free_wqebbs = hinic3_get_sq_free_wqebbs(txq) + 1;

	while (free_wqebbs < txq->q_depth) {
		ci = hinic3_get_sq_local_ci(txq);

		tx_info = &txq->tx_info[ci];
		if (unlikely(tx_info->cpy_mbuf != NULL)) {
			hinic3_free_cpy_mbuf(txq->nic_dev, tx_info->cpy_mbuf);
			tx_info->cpy_mbuf = NULL;
		}
		hinic3_tx_free_mbuf_force(txq, tx_info->mbuf);
		hinic3_update_sq_local_ci(txq, tx_info->wqebb_cnt);

		free_wqebbs = (uint16_t)(free_wqebbs + tx_info->wqebb_cnt);
		tx_info->mbuf = NULL;
	}
}

void
hinic3_free_all_txq_mbufs(struct hinic3_nic_dev *nic_dev)
{
	uint16_t qid;
	for (qid = 0; qid < nic_dev->num_sqs; qid++)
		hinic3_free_txq_mbufs(nic_dev->txqs[qid]);
}

int
hinic3_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	struct hinic3_txq *tx_queue = txq;
	uint32_t try_free_cnt = !free_cnt ? tx_queue->q_depth : free_cnt;

	return hinic3_xmit_mbuf_cleanup(tx_queue, try_free_cnt);
}

/**
 * Prepare the data packet to be sent and calculate the internal L3 offset.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 * @param[in] mbuf
 * Point to the mbuf to be processed.
 * @param[out] inner_l3_offset
 * Inner(IP Layer) L3 layer offset.
 * @return
 * 0 as success, -EINVAL as failure.
 */
static int
hinic3_tx_offload_pkt_prepare(struct hinic3_nic_dev *nic_dev, struct rte_mbuf *mbuf,
			      uint16_t *inner_l3_offset)
{
	uint64_t ol_flags = mbuf->ol_flags;

	if ((ol_flags & HINIC3_PKT_TX_TUNNEL_MASK)) {
		if (!(((ol_flags & HINIC3_PKT_TX_TUNNEL_VXLAN) &&
				HINIC3_SUPPORT_VXLAN_OFFLOAD(nic_dev)) ||
		      ((ol_flags & HINIC3_PKT_TX_TUNNEL_GENEVE) &&
				HINIC3_SUPPORT_GENEVE_OFFLOAD(nic_dev)) ||
		      ((ol_flags & HINIC3_PKT_TX_TUNNEL_IPIP) &&
				HINIC3_SUPPORT_IPXIP_OFFLOAD(nic_dev))))
			return -EINVAL;
	}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	if (rte_validate_tx_offload(mbuf) != 0)
		return -EINVAL;
#endif
	/* Support tunnel. */
	if ((ol_flags & HINIC3_PKT_TX_TUNNEL_MASK)) {
		if ((ol_flags & HINIC3_PKT_TX_OUTER_IP_CKSUM) ||
		    (ol_flags & HINIC3_PKT_TX_OUTER_IPV6) ||
		    (ol_flags & HINIC3_PKT_TX_TCP_SEG)) {
			/*
			 * For this senmatic, l2_len of mbuf means
			 * len(out_udp + vxlan + in_eth).
			 */
			*inner_l3_offset = mbuf->l2_len + mbuf->outer_l2_len +
					   mbuf->outer_l3_len;
		} else {
			/*
			 * For this senmatic, l2_len of mbuf means
			 * len(out_eth + out_ip + out_udp + vxlan + in_eth).
			 */
			*inner_l3_offset = mbuf->l2_len;
		}
	} else {
		/* For non-tunnel type pkts. */
		*inner_l3_offset = mbuf->l2_len;
	}

	return 0;
}

void
hinic3_tx_set_normal_task_offload(struct hinic3_wqe_info *wqe_info,
				  struct hinic3_sq_wqe_combo *wqe_combo)
{
	struct hinic3_sq_task *task = wqe_combo->task;
	struct hinic3_offload_info *offload_info = &wqe_info->offload_info;

	task->pkt_info0 = 0;
	task->pkt_info0 |= SQ_TASK_INFO0_SET(offload_info->inner_l4_en, INNER_L4_EN);
	task->pkt_info0 |= SQ_TASK_INFO0_SET(offload_info->inner_l3_en, INNER_L3_EN);
	task->pkt_info0 |= SQ_TASK_INFO0_SET(offload_info->encapsulation, TUNNEL_FLAG);
	task->pkt_info0 |= SQ_TASK_INFO0_SET(offload_info->out_l3_en, OUT_L3_EN);
	task->pkt_info0 |= SQ_TASK_INFO0_SET(offload_info->out_l4_en, OUT_L4_EN);
	task->pkt_info0 = hinic3_hw_be32(task->pkt_info0);

	if (wqe_combo->task_type == SQ_WQE_TASKSECT_16BYTES) {
		task->ip_identify = 0;
		task->pkt_info2 = 0;
		task->vlan_offload = 0;
		task->vlan_offload = SQ_TASK_INFO3_SET(offload_info->vlan_tag, VLAN_TAG) |
				     SQ_TASK_INFO3_SET(offload_info->vlan_sel, VLAN_TYPE) |
				     SQ_TASK_INFO3_SET(offload_info->vlan_valid, VLAN_TAG_VALID);
		task->vlan_offload = hinic3_hw_be32(task->vlan_offload);
	}
}

void
hinic3_tx_set_compact_task_offload(struct hinic3_wqe_info *wqe_info,
				   struct hinic3_sq_wqe_combo *wqe_combo)
{
	struct hinic3_sq_task *task = wqe_combo->task;
	struct hinic3_offload_info *offload_info = &wqe_info->offload_info;

	task->pkt_info0 = 0;
	wqe_combo->task->pkt_info0 =
			SQ_TASK_INFO_SET(offload_info->out_l3_en, OUT_L3_EN) |
			SQ_TASK_INFO_SET(offload_info->out_l4_en, OUT_L4_EN) |
			SQ_TASK_INFO_SET(offload_info->inner_l3_en, INNER_L3_EN) |
			SQ_TASK_INFO_SET(offload_info->inner_l4_en, INNER_L4_EN) |
			SQ_TASK_INFO_SET(offload_info->vlan_valid, VLAN_VALID) |
			SQ_TASK_INFO_SET(offload_info->vlan_sel, VLAN_SEL) |
			SQ_TASK_INFO_SET(offload_info->vlan_tag, VLAN_TAG);

	task->pkt_info0 = hinic3_hw_be32(task->pkt_info0);
}

static int
hinic3_set_tx_offload(struct hinic3_nic_dev *nic_dev,
		      struct rte_mbuf *mbuf,
		      struct hinic3_sq_wqe_combo *wqe_combo,
					  struct hinic3_wqe_info *wqe_info)
{
	uint64_t ol_flags = mbuf->ol_flags;
	struct hinic3_offload_info *offload_info = &wqe_info->offload_info;

	/* Vlan offload. */
	if (unlikely(ol_flags & HINIC3_PKT_TX_VLAN_PKT)) {
		offload_info->vlan_valid = 1;
		offload_info->vlan_tag = mbuf->vlan_tci;
		offload_info->vlan_sel = HINIC3_TX_TPID0;
	}
	if (!(ol_flags & HINIC3_TX_CKSUM_OFFLOAD_MASK))
		goto set_tx_wqe_offload;

	/* Tso offload. */
	if (ol_flags & HINIC3_PKT_TX_TCP_SEG) {
		wqe_info->queue_info.payload_offset = wqe_info->payload_offset;
		if ((wqe_info->payload_offset >> 1) > MAX_PAYLOAD_OFFSET)
			return -EINVAL;

		offload_info->inner_l3_en = 1;
		offload_info->inner_l4_en = 1;
		wqe_info->queue_info.tso = 1;
		wqe_info->queue_info.mss = mbuf->tso_segsz;
	} else {
		if (ol_flags & HINIC3_PKT_TX_IP_CKSUM)
			offload_info->inner_l3_en = 1;

		switch (ol_flags & HINIC3_PKT_TX_L4_MASK) {
		case HINIC3_PKT_TX_TCP_CKSUM:
		case HINIC3_PKT_TX_UDP_CKSUM:
		case HINIC3_PKT_TX_SCTP_CKSUM:
			offload_info->inner_l4_en = 1;
			break;
		case HINIC3_PKT_TX_L4_NO_CKSUM:
			break;
		default:
			PMD_DRV_LOG(INFO, "not support pkt type");
			return -EINVAL;
		}
	}

	switch (ol_flags & HINIC3_PKT_TX_TUNNEL_MASK) {
	case HINIC3_PKT_TX_TUNNEL_VXLAN:
	case HINIC3_PKT_TX_TUNNEL_VXLAN_GPE:
	case HINIC3_PKT_TX_TUNNEL_GENEVE:
		offload_info->encapsulation = 1;
		wqe_info->queue_info.udp_dp_en = 1;
		break;
	case 0:
		break;

	default:
		PMD_DRV_LOG(INFO, "not support tunnel pkt type");
		return -EINVAL;
	}

	if (ol_flags & HINIC3_PKT_TX_OUTER_IP_CKSUM)
		offload_info->out_l3_en = 1;

	if (ol_flags & HINIC3_PKT_TX_OUTER_UDP_CKSUM)
		offload_info->out_l4_en = 1;

set_tx_wqe_offload:
	nic_dev->tx_ops->tx_set_wqe_offload(wqe_info, wqe_combo);

	return 0;
}

/**
 * Check whether the number of segments in the mbuf is valid.
 *
 * @param[in] mbuf
 * Point to the mbuf to be verified.
 * @param[in] wqe_info
 * Point to wqe_info of send queue(SQ).
 * @return
 * true as valid, false as invalid.
 */
static bool
hinic3_is_tso_sge_valid(struct rte_mbuf *mbuf, struct hinic3_wqe_info *wqe_info)
{
	uint32_t total_len, limit_len, checked_len, left_len, adjust_mss;
	uint32_t max_sges, left_sges, first_len;
	uint32_t payload_len, frag_num;
	uint32_t i;
	struct rte_mbuf *mbuf_head, *mbuf_first;
	struct rte_mbuf *mbuf_pre = mbuf;

	left_sges = mbuf->nb_segs;
	mbuf_head = mbuf;
	mbuf_first = mbuf;

	/* Calculate the number of message payload frag,
	 * if it exceeds the hardware limit of 10 bits,
	 * packet will be discarded.
	 */
	payload_len = mbuf_head->pkt_len - wqe_info->payload_offset;
	frag_num = (payload_len + mbuf_head->tso_segsz - 1) / mbuf_head->tso_segsz;
	if (frag_num > MAX_TSO_NUM_FRAG) {
		PMD_DRV_LOG(WARNING, "tso frag num over hw limit, frag_num:0x%x.", frag_num);
		return false;
	}

	/* Tso sge number validation. */
	if (unlikely(left_sges >= HINIC3_NONTSO_PKT_MAX_SGE)) {
		checked_len = 0;
		total_len = 0;
		first_len = 0;
		adjust_mss = mbuf->tso_segsz >= TX_MSS_MIN ? mbuf->tso_segsz
							   : TX_MSS_MIN;
		max_sges = HINIC3_NONTSO_PKT_MAX_SGE - 1;
		limit_len = adjust_mss + wqe_info->payload_offset;

		for (i = 0; (i < max_sges) && (total_len < limit_len); i++) {
			total_len += mbuf->data_len;
			mbuf_pre = mbuf;
			mbuf = mbuf->next;
		}

		/* Each continues 38 mbufs segmust do one check. */
		while (left_sges >= HINIC3_NONTSO_PKT_MAX_SGE) {
			if (total_len >= limit_len) {
				/* Update the limit len. */
				limit_len = adjust_mss;
				/* Update checked len. */
				checked_len += first_len;
				/* Record the first len. */
				first_len = mbuf_first->data_len;
				/* First mbuf move to the next. */
				mbuf_first = mbuf_first->next;
				/* Update total len. */
				total_len -= first_len;
				left_sges--;
				i--;
				for (;
				     (i < max_sges) && (total_len < limit_len);
				     i++) {
					total_len += mbuf->data_len;
					mbuf_pre = mbuf;
					mbuf = mbuf->next;
				}
			} else {
				/* Try to copy if not valid. */
				checked_len += (total_len - mbuf_pre->data_len);

				left_len = mbuf_head->pkt_len - checked_len;
				if (left_len > HINIC3_COPY_MBUF_SIZE)
					return false;
				wqe_info->sge_cnt = (uint16_t)(mbuf_head->nb_segs +
							  i - left_sges);
				wqe_info->cpy_mbuf_cnt = 1;

				return true;
			}
		} /**< End of while. */
	}

	wqe_info->sge_cnt = mbuf_head->nb_segs;

	return true;
}

static int
hinic3_non_tso_pkt_pre_process(struct rte_mbuf *mbuf, struct hinic3_wqe_info *wqe_info)
{
	struct rte_mbuf *mbuf_pkt = mbuf;
	uint32_t total_len = 0;
	uint16_t i;

	if (likely(HINIC3_NONTSO_SEG_NUM_VALID(mbuf->nb_segs)))
		return 0;

	/* Non-tso packet length must less than 64KB. */
	if (unlikely(mbuf->pkt_len > MAX_SINGLE_SGE_SIZE))
		return -EINVAL;

	/*
	 * Mbuf number of non-tso packet must less than the sge number
	 * that nic can support. The excess part will be copied to another
	 * mbuf.
	 */
	for (i = 0; i < (HINIC3_NONTSO_PKT_MAX_SGE - 1); i++) {
		total_len += mbuf_pkt->data_len;
		mbuf_pkt = mbuf_pkt->next;
	}

	/*
	 * Max copy mbuf size is 4KB, packet will be dropped directly,
	 * if total copy length is more than it.
	 */
	if ((total_len + HINIC3_COPY_MBUF_SIZE) < mbuf->pkt_len)
		return -EINVAL;

	wqe_info->sge_cnt = HINIC3_NONTSO_PKT_MAX_SGE;
	wqe_info->cpy_mbuf_cnt = 1;

	return 0;
}

/**
 * Checks and processes transport offload information for data packets.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 * @param[in] mbuf
 * Point to the mbuf to send.
 * @param[in] wqe_info
 * Point to wqe_info of send queue(SQ).
 * @return
 * 0 as success, -EINVAL as failure.
 */
static int
hinic3_get_tx_offload(struct hinic3_nic_dev *nic_dev, struct rte_mbuf *mbuf,
		      struct hinic3_wqe_info *wqe_info)
{
	uint64_t ol_flags = mbuf->ol_flags;
	uint16_t inner_l3_offset = 0;
	int err;

	wqe_info->sge_cnt = mbuf->nb_segs;
	wqe_info->cpy_mbuf_cnt = 0;
	/* Check if the packet set available offload flags. */
	if (!(ol_flags & HINIC3_TX_OFFLOAD_MASK)) {
		wqe_info->offload = 0;
		return hinic3_non_tso_pkt_pre_process(mbuf, wqe_info);
	}

	wqe_info->offload = 1;
	err = hinic3_tx_offload_pkt_prepare(nic_dev, mbuf, &inner_l3_offset);
	if (err)
		return err;

	/* Non-tso mbuf only check sge num. */
	if (likely(!(mbuf->ol_flags & HINIC3_PKT_TX_TCP_SEG))) {
		return hinic3_non_tso_pkt_pre_process(mbuf, wqe_info);
	}

	/* Tso mbuf. */
	wqe_info->payload_offset =
		inner_l3_offset + mbuf->l3_len + mbuf->l4_len;

	/* Too many mbuf segs. */
	if (unlikely(HINIC3_TSO_SEG_NUM_INVALID(mbuf->nb_segs)))
		return -EINVAL;

	/* Check whether can cover all tso mbuf segs or not. */
	if (unlikely(!hinic3_is_tso_sge_valid(mbuf, wqe_info)))
		return -EINVAL;

	return 0;
}

static inline void
hinic3_set_buf_desc(struct hinic3_sq_bufdesc *buf_descs, rte_iova_t addr,
		    uint32_t len)
{
	buf_descs->hi_addr = hinic3_hw_be32(upper_32_bits(addr));
	buf_descs->lo_addr = hinic3_hw_be32(lower_32_bits(addr));
	buf_descs->len = hinic3_hw_be32(len);
	buf_descs->rsvd = 0;
}

static inline struct rte_mbuf *
hinic3_alloc_cpy_mbuf(struct hinic3_nic_dev *nic_dev)
{
	return rte_pktmbuf_alloc(nic_dev->cpy_mpool);
}

/**
 * Copy packets in the send queue(SQ).
 *
 * @param[in] nic_dev
 * Point to nic device.
 * @param[in] mbuf
 * Point to the source mbuf.
 * @param[in] seg_cnt
 * Number of mbuf segments to be copied.
 * @result
 * The address of the copied mbuf.
 */
static void *
hinic3_copy_tx_mbuf(struct hinic3_nic_dev *nic_dev, struct rte_mbuf *mbuf,
		    uint16_t sge_cnt)
{
	struct rte_mbuf *dst_mbuf;
	uint32_t offset = 0;
	uint16_t i;

	if (unlikely(!nic_dev->cpy_mpool))
		return NULL;

	dst_mbuf = hinic3_alloc_cpy_mbuf(nic_dev);
	if (unlikely(!dst_mbuf))
		return NULL;

	dst_mbuf->data_off = 0;
	dst_mbuf->data_len = 0;
	for (i = 0; i < sge_cnt; i++) {
		memcpy((uint8_t *)dst_mbuf->buf_addr + offset,
			   (uint8_t *)mbuf->buf_addr + mbuf->data_off,
			   mbuf->data_len);
		dst_mbuf->data_len += mbuf->data_len;
		offset += mbuf->data_len;
		mbuf = mbuf->next;
	}
	dst_mbuf->pkt_len = dst_mbuf->data_len;

	return dst_mbuf;
}

/**
 * Map the TX mbuf to the DMA address space and set related information for
 * subsequent DMA transmission.
 *
 * @param[in] txq
 * Point to send queue.
 * @param[in] mbuf
 * Point to the tx mbuf.
 * @param[out] wqe_combo
 * Point to send queue wqe_combo.
 * @param[in] wqe_info
 * Point to wqe_info of send queue(SQ).
 * @result
 * 0 as success, -EINVAL as failure.
 */
static int
hinic3_mbuf_dma_map_sge(struct hinic3_txq *txq, struct rte_mbuf *mbuf,
			struct hinic3_sq_wqe_combo *wqe_combo,
			struct hinic3_wqe_info *wqe_info)
{
	struct hinic3_sq_wqe_desc *wqe_desc = wqe_combo->hdr;
	struct hinic3_sq_bufdesc *buf_desc = wqe_combo->bds_head;
	uint16_t nb_segs = wqe_info->sge_cnt - wqe_info->cpy_mbuf_cnt;
	uint16_t real_segs = mbuf->nb_segs;
	rte_iova_t dma_addr;
	uint32_t i;

	for (i = 0; i < nb_segs; i++) {
		if (unlikely(mbuf == NULL)) {
			txq->txq_stats.mbuf_null++;
			return -EINVAL;
		}

		if (unlikely(mbuf->data_len == 0)) {
			txq->txq_stats.sge_len0++;
			return -EINVAL;
		}

		dma_addr = rte_mbuf_data_iova(mbuf);
		if (i == 0) {
			if (wqe_combo->wqe_type == SQ_WQE_COMPACT_TYPE &&
			    mbuf->data_len > COMPACT_WQE_MAX_CTRL_LEN) {
				txq->txq_stats.sge_len_too_large++;
				return -EINVAL;
			}

			wqe_desc->hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			wqe_desc->lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
			wqe_desc->ctrl_len = mbuf->data_len;
		} else {
			/*
			 * Parts of wqe is in sq bottom while parts
			 * of wqe is in sq head.
			 */
			if (unlikely((uint64_t)buf_desc == txq->sq_bot_sge_addr))
				buf_desc = (struct hinic3_sq_bufdesc *)txq->sq_head_addr;
			hinic3_set_buf_desc(buf_desc, dma_addr, mbuf->data_len);
			buf_desc++;
		}
		mbuf = mbuf->next;
	}

	/* For now: support over 38 sge, copy the last 2 mbuf. */
	if (unlikely(wqe_info->cpy_mbuf_cnt != 0)) {
		/*
		 * Copy invalid mbuf segs to a valid buffer, lost performance.
		 */
		txq->txq_stats.cpy_pkts += 1;
		mbuf = hinic3_copy_tx_mbuf(txq->nic_dev, mbuf,
					   real_segs - nb_segs);
		if (unlikely(!mbuf))
			return -EINVAL;

		txq->tx_info[wqe_info->pi].cpy_mbuf = mbuf;

		/* Deal with the last mbuf. */
		dma_addr = rte_mbuf_data_iova(mbuf);
		if (unlikely(mbuf->data_len == 0)) {
			txq->txq_stats.sge_len0++;
			return -EINVAL;
		}
		/*
		 * Parts of wqe is in sq bottom while parts
		 * of wqe is in sq head.
		 */
		if (i == 0) {
			wqe_desc->hi_addr =
				hinic3_hw_be32(upper_32_bits(dma_addr));
			wqe_desc->lo_addr =
				hinic3_hw_be32(lower_32_bits(dma_addr));
			wqe_desc->ctrl_len = mbuf->data_len;
		} else {
			if (unlikely(((uint64_t)buf_desc == txq->sq_bot_sge_addr)))
				buf_desc = (struct hinic3_sq_bufdesc *)txq->sq_head_addr;

			hinic3_set_buf_desc(buf_desc, dma_addr, mbuf->data_len);
		}
	}

	return 0;
}

/**
 * Sets and configures fields in the transmit queue control descriptor based on
 * the WQE type.
 *
 * @param[out] wqe_combo
 * Point to wqe_combo of send queue.
 * @param[in] wqe_info
 * Point to wqe_info of send queue.
 */
static void
hinic3_prepare_sq_ctrl(struct hinic3_sq_wqe_combo *wqe_combo,
		       struct hinic3_wqe_info *wqe_info)
{
	struct hinic3_queue_info *queue_info = &wqe_info->queue_info;
	struct hinic3_sq_wqe_desc *wqe_desc = wqe_combo->hdr;
	uint32_t *qsf = &wqe_desc->queue_info;

	wqe_desc->ctrl_len |= SQ_CTRL_SET(SQ_NORMAL_WQE, DIRECT) |
			      SQ_CTRL_SET(wqe_combo->wqe_type, EXTENDED) |
			      SQ_CTRL_SET(wqe_info->owner, OWNER);

	if (wqe_combo->wqe_type == SQ_WQE_EXTENDED_TYPE) {
		wqe_desc->ctrl_len |= SQ_CTRL_SET(wqe_info->sge_cnt, BUFDESC_NUM) |
				      SQ_CTRL_SET(wqe_combo->task_type, TASKSECT_LEN) |
				      SQ_CTRL_SET(SQ_WQE_SGL, DATA_FORMAT);

		*qsf = SQ_CTRL_QUEUE_INFO_SET(1, UC) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->sctp, SCTP) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->udp_dp_en, TCPUDP_CS) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->tso, TSO) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->ufo, UFO) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->payload_offset >> 1, PLDOFF) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->pkt_type, PKT_TYPE) |
		       SQ_CTRL_QUEUE_INFO_SET(queue_info->mss, MSS);

		if (!SQ_CTRL_QUEUE_INFO_GET(*qsf, MSS)) {
			*qsf |= SQ_CTRL_QUEUE_INFO_SET(TX_MSS_DEFAULT, MSS);
		} else if (SQ_CTRL_QUEUE_INFO_GET(*qsf, MSS) < TX_MSS_MIN) {
			/* MSS should not less than 80. */
			*qsf = SQ_CTRL_QUEUE_INFO_CLEAR(*qsf, MSS);
			*qsf |= SQ_CTRL_QUEUE_INFO_SET(TX_MSS_MIN, MSS);
		}
		*qsf = hinic3_hw_be32(*qsf);
	} else {
		wqe_desc->ctrl_len |= SQ_CTRL_COMPACT_QUEUE_INFO_SET(queue_info->sctp, SCTP) |
				SQ_CTRL_COMPACT_QUEUE_INFO_SET(queue_info->udp_dp_en, UDP_DP_EN) |
				SQ_CTRL_COMPACT_QUEUE_INFO_SET(queue_info->ufo, UFO) |
				SQ_CTRL_COMPACT_QUEUE_INFO_SET(queue_info->pkt_type, PKT_TYPE);
	}

	wqe_desc->ctrl_len = hinic3_hw_be32(wqe_desc->ctrl_len);
}

/**
 * It is responsible for sending data packets.
 *
 * @param[in] tx_queue
 * Point to send queue.
 * @param[in] tx_pkts
 * Pointer to the array of data packets to be sent.
 * @param[in] nb_pkts
 * Number of sent packets.
 * @return
 * Number of actually sent packets.
 */
uint16_t
hinic3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct hinic3_txq *txq = tx_queue;
	struct hinic3_tx_info *tx_info = NULL;
	struct rte_mbuf *mbuf_pkt = NULL;
	struct hinic3_sq_wqe_combo wqe_combo = {0};
	struct hinic3_wqe_info wqe_info = {0};
	uint32_t offload_err, free_cnt;
	uint64_t tx_bytes = 0;
	uint16_t free_wqebb_cnt, nb_tx;
	int err;

#ifdef HINIC3_XSTAT_PROF_TX
	uint64_t t1, t2;
	t1 = rte_get_tsc_cycles();
#endif

	if (unlikely(!HINIC3_TXQ_IS_STARTED(txq)))
		return 0;

	free_cnt = txq->tx_free_thresh;
	/* Reclaim tx mbuf before xmit new packets. */
	if (hinic3_get_sq_free_wqebbs(txq) < txq->tx_free_thresh)
		hinic3_xmit_mbuf_cleanup(txq, free_cnt);

	/* Tx loop routine. */
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		mbuf_pkt = *tx_pkts++;
		if (unlikely(hinic3_get_tx_offload(txq->nic_dev, mbuf_pkt, &wqe_info))) {
			txq->txq_stats.offload_errors++;
			break;
		}

		wqe_info.wqebb_cnt = wqe_info.sge_cnt;
		if (likely(wqe_info.offload || wqe_info.wqebb_cnt > 1)) {
			if (txq->tx_wqe_compact_task) {
				/**
				 * One more wqebb is needed for compact task under two situations:
				 * 1. TSO: MSS field is needed, no available space for
				 *    compact task in compact wqe.
				 * 2. SGE number > 1: wqe is handlerd as extended wqe by nic.
				 */
				if (mbuf_pkt->ol_flags & HINIC3_PKT_TX_TCP_SEG ||
				    wqe_info.wqebb_cnt > 1)
					wqe_info.wqebb_cnt++;
			} else {
				/* Use extended sq wqe with normal TS */
				wqe_info.wqebb_cnt++;
			}
		}

		free_wqebb_cnt = hinic3_get_sq_free_wqebbs(txq);
		if (unlikely(wqe_info.wqebb_cnt > free_wqebb_cnt)) {
			/* Reclaim again. */
			hinic3_xmit_mbuf_cleanup(txq, free_cnt);
			free_wqebb_cnt = hinic3_get_sq_free_wqebbs(txq);
			if (unlikely(wqe_info.wqebb_cnt > free_wqebb_cnt)) {
				txq->txq_stats.tx_busy += (nb_pkts - nb_tx);
				break;
			}
		}

		/* Task or bd section maybe wrapped for one wqe. */
		hinic3_set_wqe_combo(txq, &wqe_combo, &wqe_info);

		/* Fill tx packet offload into qsf and task field. */
		offload_err = hinic3_set_tx_offload(txq->nic_dev, mbuf_pkt, &wqe_combo, &wqe_info);
			if (unlikely(offload_err)) {
				hinic3_put_sq_wqe(txq, &wqe_info);
				txq->txq_stats.offload_errors++;
				break;
			}

		/* Fill sq_wqe buf_desc and bd_desc. */
		err = hinic3_mbuf_dma_map_sge(txq, mbuf_pkt, &wqe_combo,
					      &wqe_info);
		if (err) {
			hinic3_put_sq_wqe(txq, &wqe_info);
			txq->txq_stats.offload_errors++;
			break;
		}

		/* Record tx info. */
		tx_info = &txq->tx_info[wqe_info.pi];
		tx_info->mbuf = mbuf_pkt;
		tx_info->wqebb_cnt = wqe_info.wqebb_cnt;

		/*
		 * For wqe compact type, no need to prepare
		 * sq ctrl info.
		 */
		if (wqe_combo.wqe_type != SQ_WQE_COMPACT_TYPE)
			hinic3_prepare_sq_ctrl(&wqe_combo, &wqe_info);

		tx_bytes += mbuf_pkt->pkt_len;
	}

	/* Update txq stats. */
	if (nb_tx) {
		hinic3_write_db(txq->db_addr, txq->q_id, (int)(txq->cos),
				SQ_CFLAG_DP,
				MASKED_QUEUE_IDX(txq, txq->prod_idx));
		txq->txq_stats.packets += nb_tx;
		txq->txq_stats.bytes += tx_bytes;
	}
	txq->txq_stats.burst_pkts = nb_tx;

#ifdef HINIC3_XSTAT_PROF_TX
	t2 = rte_get_tsc_cycles();
	txq->txq_stats.app_tsc = t1 - txq->prof_tx_end_tsc;
	txq->prof_tx_end_tsc = t2;
	txq->txq_stats.pmd_tsc = t2 - t1;
	txq->txq_stats.burst_pkts = nb_tx;
#endif

	return nb_tx;
}

int
hinic3_stop_sq(struct hinic3_txq *txq)
{
	struct hinic3_nic_dev *nic_dev = txq->nic_dev;
	uint64_t timeout;
	int err = -EFAULT;
	int free_wqebbs;

	timeout = msecs_to_cycles(HINIC3_FLUSH_QUEUE_TIMEOUT) + cycles;
	do {
		hinic3_tx_done_cleanup(txq, 0);
		free_wqebbs = hinic3_get_sq_free_wqebbs(txq) + 1;
		if (free_wqebbs == txq->q_depth) {
			err = 0;
			break;
		}

		rte_delay_us(1);
	} while (time_before(cycles, timeout));

	if (err) {
		PMD_DRV_LOG(WARNING, "%s Wait sq empty timeout", nic_dev->dev_name);
		PMD_DRV_LOG(WARNING,
			    "queue_idx: %u, sw_ci: %u, hw_ci: %u, sw_pi: %u, free_wqebbs: %u, q_depth:%u",
			    txq->q_id,
			    hinic3_get_sq_local_ci(txq),
			    hinic3_get_sq_hw_ci(txq),
			    MASKED_QUEUE_IDX(txq, txq->prod_idx),
			    free_wqebbs,
			    txq->q_depth);
	}

	return err;
}

/**
 * Stop all sending queues (SQs).
 *
 * @param[in] txq
 * Point to send queue.
 */
void
hinic3_flush_txqs(struct hinic3_nic_dev *nic_dev)
{
	uint16_t qid;
	int err;

	for (qid = 0; qid < nic_dev->num_sqs; qid++) {
		err = hinic3_stop_sq(nic_dev->txqs[qid]);
		if (err)
			PMD_DRV_LOG(ERR, "Stop sq%d failed", qid);
	}
}
