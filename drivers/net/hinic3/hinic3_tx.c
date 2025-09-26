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

#define HINIC3_TX_OFFLOAD_MASK \
	(HINIC3_TX_CKSUM_OFFLOAD_MASK | HINIC3_PKT_TX_VLAN_PKT)

#define HINIC3_TX_CKSUM_OFFLOAD_MASK                          \
	(HINIC3_PKT_TX_IP_CKSUM | HINIC3_PKT_TX_TCP_CKSUM |   \
	 HINIC3_PKT_TX_UDP_CKSUM | HINIC3_PKT_TX_SCTP_CKSUM | \
	 HINIC3_PKT_TX_OUTER_IP_CKSUM | HINIC3_PKT_TX_TCP_SEG)

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
