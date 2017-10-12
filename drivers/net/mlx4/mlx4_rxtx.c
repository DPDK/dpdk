/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Data plane functions for mlx4 driver.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_io.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include "mlx4.h"
#include "mlx4_prm.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Pointer-value pair structure used in tx_post_send for saving the first
 * DWORD (32 byte) of a TXBB.
 */
struct pv {
	struct mlx4_wqe_data_seg *dseg;
	uint32_t val;
};

/**
 * Stamp a WQE so it won't be reused by the HW.
 *
 * Routine is used when freeing WQE used by the chip or when failing
 * building an WQ entry has failed leaving partial information on the queue.
 *
 * @param sq
 *   Pointer to the SQ structure.
 * @param index
 *   Index of the freed WQE.
 * @param num_txbbs
 *   Number of blocks to stamp.
 *   If < 0 the routine will use the size written in the WQ entry.
 * @param owner
 *   The value of the WQE owner bit to use in the stamp.
 *
 * @return
 *   The number of Tx basic blocs (TXBB) the WQE contained.
 */
static int
mlx4_txq_stamp_freed_wqe(struct mlx4_sq *sq, uint16_t index, uint8_t owner)
{
	uint32_t stamp = rte_cpu_to_be_32(MLX4_SQ_STAMP_VAL |
					  (!!owner << MLX4_SQ_STAMP_SHIFT));
	uint8_t *wqe = mlx4_get_send_wqe(sq, (index & sq->txbb_cnt_mask));
	uint32_t *ptr = (uint32_t *)wqe;
	int i;
	int txbbs_size;
	int num_txbbs;

	/* Extract the size from the control segment of the WQE. */
	num_txbbs = MLX4_SIZE_TO_TXBBS((((struct mlx4_wqe_ctrl_seg *)
					 wqe)->fence_size & 0x3f) << 4);
	txbbs_size = num_txbbs * MLX4_TXBB_SIZE;
	/* Optimize the common case when there is no wrap-around. */
	if (wqe + txbbs_size <= sq->eob) {
		/* Stamp the freed descriptor. */
		for (i = 0; i < txbbs_size; i += MLX4_SQ_STAMP_STRIDE) {
			*ptr = stamp;
			ptr += MLX4_SQ_STAMP_DWORDS;
		}
	} else {
		/* Stamp the freed descriptor. */
		for (i = 0; i < txbbs_size; i += MLX4_SQ_STAMP_STRIDE) {
			*ptr = stamp;
			ptr += MLX4_SQ_STAMP_DWORDS;
			if ((uint8_t *)ptr >= sq->eob) {
				ptr = (uint32_t *)sq->buf;
				stamp ^= RTE_BE32(0x80000000);
			}
		}
	}
	return num_txbbs;
}

/**
 * Manage Tx completions.
 *
 * When sending a burst, mlx4_tx_burst() posts several WRs.
 * To improve performance, a completion event is only required once every
 * MLX4_PMD_TX_PER_COMP_REQ sends. Doing so discards completion information
 * for other WRs, but this information would not be used anyway.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
mlx4_txq_complete(struct txq *txq)
{
	unsigned int elts_comp = txq->elts_comp;
	unsigned int elts_tail = txq->elts_tail;
	const unsigned int elts_n = txq->elts_n;
	struct mlx4_cq *cq = &txq->mcq;
	struct mlx4_sq *sq = &txq->msq;
	struct mlx4_cqe *cqe;
	uint32_t cons_index = cq->cons_index;
	uint16_t new_index;
	uint16_t nr_txbbs = 0;
	int pkts = 0;

	if (unlikely(elts_comp == 0))
		return 0;
	/*
	 * Traverse over all CQ entries reported and handle each WQ entry
	 * reported by them.
	 */
	do {
		cqe = (struct mlx4_cqe *)mlx4_get_cqe(cq, cons_index);
		if (unlikely(!!(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK) ^
		    !!(cons_index & cq->cqe_cnt)))
			break;
		/*
		 * Make sure we read the CQE after we read the ownership bit.
		 */
		rte_rmb();
		if (unlikely((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
			     MLX4_CQE_OPCODE_ERROR)) {
			struct mlx4_err_cqe *cqe_err =
				(struct mlx4_err_cqe *)cqe;
			ERROR("%p CQE error - vendor syndrome: 0x%x"
			      " syndrome: 0x%x\n",
			      (void *)txq, cqe_err->vendor_err,
			      cqe_err->syndrome);
		}
		/* Get WQE index reported in the CQE. */
		new_index =
			rte_be_to_cpu_16(cqe->wqe_index) & sq->txbb_cnt_mask;
		do {
			/* Free next descriptor. */
			nr_txbbs +=
				mlx4_txq_stamp_freed_wqe(sq,
				     (sq->tail + nr_txbbs) & sq->txbb_cnt_mask,
				     !!((sq->tail + nr_txbbs) & sq->txbb_cnt));
			pkts++;
		} while (((sq->tail + nr_txbbs) & sq->txbb_cnt_mask) !=
			 new_index);
		cons_index++;
	} while (1);
	if (unlikely(pkts == 0))
		return 0;
	/*
	 * Update CQ.
	 * To prevent CQ overflow we first update CQ consumer and only then
	 * the ring consumer.
	 */
	cq->cons_index = cons_index;
	*cq->set_ci_db = rte_cpu_to_be_32(cq->cons_index & 0xffffff);
	rte_wmb();
	sq->tail = sq->tail + nr_txbbs;
	/* Update the list of packets posted for transmission. */
	elts_comp -= pkts;
	assert(elts_comp <= txq->elts_comp);
	/*
	 * Assume completion status is successful as nothing can be done about
	 * it anyway.
	 */
	elts_tail += pkts;
	if (elts_tail >= elts_n)
		elts_tail -= elts_n;
	txq->elts_tail = elts_tail;
	txq->elts_comp = elts_comp;
	return 0;
}

/**
 * Get memory pool (MP) from mbuf. If mbuf is indirect, the pool from which
 * the cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static struct rte_mempool *
mlx4_txq_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_INDIRECT(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
}

/**
 * Get memory region (MR) <-> memory pool (MP) association from txq->mp2mr[].
 * Add MP to txq->mp2mr[] if it's not registered yet. If mp2mr[] is full,
 * remove an entry first.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param[in] mp
 *   Memory pool for which a memory region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
uint32_t
mlx4_txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
{
	unsigned int i;
	struct ibv_mr *mr;

	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].mp == mp) {
			assert(txq->mp2mr[i].lkey != (uint32_t)-1);
			assert(txq->mp2mr[i].mr->lkey == txq->mp2mr[i].lkey);
			return txq->mp2mr[i].lkey;
		}
	}
	/* Add a new entry, register MR first. */
	DEBUG("%p: discovered new memory pool \"%s\" (%p)",
	      (void *)txq, mp->name, (void *)mp);
	mr = mlx4_mp2mr(txq->priv->pd, mp);
	if (unlikely(mr == NULL)) {
		DEBUG("%p: unable to configure MR, ibv_reg_mr() failed.",
		      (void *)txq);
		return (uint32_t)-1;
	}
	if (unlikely(i == RTE_DIM(txq->mp2mr))) {
		/* Table is full, remove oldest entry. */
		DEBUG("%p: MR <-> MP table full, dropping oldest entry.",
		      (void *)txq);
		--i;
		claim_zero(ibv_dereg_mr(txq->mp2mr[0].mr));
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq->mp2mr[i].mp = mp;
	txq->mp2mr[i].mr = mr;
	txq->mp2mr[i].lkey = mr->lkey;
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq, mp->name, (void *)mp, txq->mp2mr[i].lkey);
	return txq->mp2mr[i].lkey;
}

/**
 * Posts a single work request to a send queue.
 *
 * @param txq
 *   Target Tx queue.
 * @param pkt
 *   Packet to transmit.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static inline int
mlx4_post_send(struct txq *txq, struct rte_mbuf *pkt)
{
	struct mlx4_wqe_ctrl_seg *ctrl;
	struct mlx4_wqe_data_seg *dseg;
	struct mlx4_sq *sq = &txq->msq;
	struct rte_mbuf *buf;
	uint32_t head_idx = sq->head & sq->txbb_cnt_mask;
	uint32_t lkey;
	uintptr_t addr;
	uint32_t srcrb_flags;
	uint32_t owner_opcode = MLX4_OPCODE_SEND;
	uint32_t byte_count;
	int wqe_real_size;
	int nr_txbbs;
	int rc;
	struct pv *pv = (struct pv *)txq->bounce_buf;
	int pv_counter = 0;

	/* Calculate the needed work queue entry size for this packet. */
	wqe_real_size = sizeof(struct mlx4_wqe_ctrl_seg) +
			pkt->nb_segs * sizeof(struct mlx4_wqe_data_seg);
	nr_txbbs = MLX4_SIZE_TO_TXBBS(wqe_real_size);
	/*
	 * Check that there is room for this WQE in the send queue and that
	 * the WQE size is legal.
	 */
	if (((sq->head - sq->tail) + nr_txbbs +
	     sq->headroom_txbbs) >= sq->txbb_cnt ||
	    nr_txbbs > MLX4_MAX_WQE_TXBBS) {
		rc = ENOSPC;
		goto err;
	}
	/* Get the control and data entries of the WQE. */
	ctrl = (struct mlx4_wqe_ctrl_seg *)mlx4_get_send_wqe(sq, head_idx);
	dseg = (struct mlx4_wqe_data_seg *)((uintptr_t)ctrl +
					    sizeof(struct mlx4_wqe_ctrl_seg));
	/* Fill the data segments with buffer information. */
	for (buf = pkt; buf != NULL; buf = buf->next, dseg++) {
		addr = rte_pktmbuf_mtod(buf, uintptr_t);
		rte_prefetch0((volatile void *)addr);
		/* Handle WQE wraparound. */
		if (unlikely(dseg >= (struct mlx4_wqe_data_seg *)sq->eob))
			dseg = (struct mlx4_wqe_data_seg *)sq->buf;
		dseg->addr = rte_cpu_to_be_64(addr);
		/* Memory region key for this memory pool. */
		lkey = mlx4_txq_mp2mr(txq, mlx4_txq_mb2mp(buf));
		if (unlikely(lkey == (uint32_t)-1)) {
			/* MR does not exist. */
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			/*
			 * Restamp entry in case of failure.
			 * Make sure that size is written correctly
			 * Note that we give ownership to the SW, not the HW.
			 */
			ctrl->fence_size = (wqe_real_size >> 4) & 0x3f;
			mlx4_txq_stamp_freed_wqe(sq, head_idx,
				     (sq->head & sq->txbb_cnt) ? 0 : 1);
			rc = EFAULT;
			goto err;
		}
		dseg->lkey = rte_cpu_to_be_32(lkey);
		if (likely(buf->data_len)) {
			byte_count = rte_cpu_to_be_32(buf->data_len);
		} else {
			/*
			 * Zero length segment is treated as inline segment
			 * with zero data.
			 */
			byte_count = RTE_BE32(0x80000000);
		}
		/*
		 * If the data segment is not at the beginning of a
		 * Tx basic block (TXBB) then write the byte count,
		 * else postpone the writing to just before updating the
		 * control segment.
		 */
		if ((uintptr_t)dseg & (uintptr_t)(MLX4_TXBB_SIZE - 1)) {
			/*
			 * Need a barrier here before writing the byte_count
			 * fields to make sure that all the data is visible
			 * before the byte_count field is set.
			 * Otherwise, if the segment begins a new cacheline,
			 * the HCA prefetcher could grab the 64-byte chunk and
			 * get a valid (!= 0xffffffff) byte count but stale
			 * data, and end up sending the wrong data.
			 */
			rte_io_wmb();
			dseg->byte_count = byte_count;
		} else {
			/*
			 * This data segment starts at the beginning of a new
			 * TXBB, so we need to postpone its byte_count writing
			 * for later.
			 */
			pv[pv_counter].dseg = dseg;
			pv[pv_counter++].val = byte_count;
		}
	}
	/* Write the first DWORD of each TXBB save earlier. */
	if (pv_counter) {
		/* Need a barrier here before writing the byte_count. */
		rte_io_wmb();
		for (--pv_counter; pv_counter  >= 0; pv_counter--)
			pv[pv_counter].dseg->byte_count = pv[pv_counter].val;
	}
	/* Fill the control parameters for this packet. */
	ctrl->fence_size = (wqe_real_size >> 4) & 0x3f;
	/*
	 * The caller should prepare "imm" in advance in order to support
	 * VF to VF communication (when the device is a virtual-function
	 * device (VF)).
	 */
	ctrl->imm = 0;
	/*
	 * For raw Ethernet, the SOLICIT flag is used to indicate that no ICRC
	 * should be calculated.
	 */
	txq->elts_comp_cd -= nr_txbbs;
	if (unlikely(txq->elts_comp_cd <= 0)) {
		txq->elts_comp_cd = txq->elts_comp_cd_init;
		srcrb_flags = RTE_BE32(MLX4_WQE_CTRL_SOLICIT |
				       MLX4_WQE_CTRL_CQ_UPDATE);
	} else {
		srcrb_flags = RTE_BE32(MLX4_WQE_CTRL_SOLICIT);
	}
	/* Enable HW checksum offload if requested */
	if (txq->csum &&
	    (pkt->ol_flags &
	     (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))) {
		const uint64_t is_tunneled = (pkt->ol_flags &
					      (PKT_TX_TUNNEL_GRE |
					       PKT_TX_TUNNEL_VXLAN));

		if (is_tunneled && txq->csum_l2tun) {
			owner_opcode |= MLX4_WQE_CTRL_IIP_HDR_CSUM |
					MLX4_WQE_CTRL_IL4_HDR_CSUM;
			if (pkt->ol_flags & PKT_TX_OUTER_IP_CKSUM)
				srcrb_flags |=
					RTE_BE32(MLX4_WQE_CTRL_IP_HDR_CSUM);
		} else {
			srcrb_flags |= RTE_BE32(MLX4_WQE_CTRL_IP_HDR_CSUM |
						MLX4_WQE_CTRL_TCP_UDP_CSUM);
		}
	}
	ctrl->srcrb_flags = srcrb_flags;
	/*
	 * Make sure descriptor is fully written before
	 * setting ownership bit (because HW can start
	 * executing as soon as we do).
	 */
	rte_wmb();
	ctrl->owner_opcode = rte_cpu_to_be_32(owner_opcode |
					      ((sq->head & sq->txbb_cnt) ?
					       MLX4_BIT_WQE_OWN : 0));
	sq->head += nr_txbbs;
	return 0;
err:
	rte_errno = rc;
	return -rc;
}

/**
 * DPDK callback for Tx.
 *
 * @param dpdk_txq
 *   Generic pointer to Tx queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx4_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	unsigned int elts_head = txq->elts_head;
	const unsigned int elts_n = txq->elts_n;
	unsigned int elts_comp = 0;
	unsigned int bytes_sent = 0;
	unsigned int i;
	unsigned int max;
	int err;

	assert(txq->elts_comp_cd != 0);
	mlx4_txq_complete(txq);
	max = (elts_n - (elts_head - txq->elts_tail));
	if (max > elts_n)
		max -= elts_n;
	assert(max >= 1);
	assert(max <= elts_n);
	/* Always leave one free entry in the ring. */
	--max;
	if (max == 0)
		return 0;
	if (max > pkts_n)
		max = pkts_n;
	for (i = 0; (i != max); ++i) {
		struct rte_mbuf *buf = pkts[i];
		unsigned int elts_head_next =
			(((elts_head + 1) == elts_n) ? 0 : elts_head + 1);
		struct txq_elt *elt_next = &(*txq->elts)[elts_head_next];
		struct txq_elt *elt = &(*txq->elts)[elts_head];

		/* Clean up old buffer. */
		if (likely(elt->buf != NULL)) {
			struct rte_mbuf *tmp = elt->buf;

#ifndef NDEBUG
			/* Poisoning. */
			memset(elt, 0x66, sizeof(*elt));
#endif
			/* Faster than rte_pktmbuf_free(). */
			do {
				struct rte_mbuf *next = tmp->next;

				rte_pktmbuf_free_seg(tmp);
				tmp = next;
			} while (tmp != NULL);
		}
		RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
		/* Post the packet for sending. */
		err = mlx4_post_send(txq, buf);
		if (unlikely(err)) {
			elt->buf = NULL;
			goto stop;
		}
		elt->buf = buf;
		bytes_sent += buf->pkt_len;
		++elts_comp;
		elts_head = elts_head_next;
	}
stop:
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Increment send statistics counters. */
	txq->stats.opackets += i;
	txq->stats.obytes += bytes_sent;
	/* Make sure that descriptors are written before doorbell record. */
	rte_wmb();
	/* Ring QP doorbell. */
	rte_write32(txq->msq.doorbell_qpn, txq->msq.db);
	txq->elts_head = elts_head;
	txq->elts_comp += elts_comp;
	return i;
}

/**
 * Poll one CQE from CQ.
 *
 * @param rxq
 *   Pointer to the receive queue structure.
 * @param[out] out
 *   Just polled CQE.
 *
 * @return
 *   Number of bytes of the CQE, 0 in case there is no completion.
 */
static unsigned int
mlx4_cq_poll_one(struct rxq *rxq, struct mlx4_cqe **out)
{
	int ret = 0;
	struct mlx4_cqe *cqe = NULL;
	struct mlx4_cq *cq = &rxq->mcq;

	cqe = (struct mlx4_cqe *)mlx4_get_cqe(cq, cq->cons_index);
	if (!!(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK) ^
	    !!(cq->cons_index & cq->cqe_cnt))
		goto out;
	/*
	 * Make sure we read CQ entry contents after we've checked the
	 * ownership bit.
	 */
	rte_rmb();
	assert(!(cqe->owner_sr_opcode & MLX4_CQE_IS_SEND_MASK));
	assert((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) !=
	       MLX4_CQE_OPCODE_ERROR);
	ret = rte_be_to_cpu_32(cqe->byte_cnt);
	++cq->cons_index;
out:
	*out = cqe;
	return ret;
}

/**
 * DPDK callback for Rx with scattered packets support.
 *
 * @param dpdk_rxq
 *   Generic pointer to Rx queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = dpdk_rxq;
	const uint32_t wr_cnt = (1 << rxq->elts_n) - 1;
	const uint16_t sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	unsigned int i = 0;
	uint32_t rq_ci = rxq->rq_ci << sges_n;
	int len = 0;

	while (pkts_n) {
		struct mlx4_cqe *cqe;
		uint32_t idx = rq_ci & wr_cnt;
		struct rte_mbuf *rep = (*rxq->elts)[idx];
		volatile struct mlx4_wqe_data_seg *scat = &(*rxq->wqes)[idx];

		/* Update the 'next' pointer of the previous segment. */
		if (pkt)
			seg->next = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(scat);
		rep = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			if (!pkt) {
				/*
				 * No buffers before we even started,
				 * bail out silently.
				 */
				break;
			}
			while (pkt != seg) {
				assert(pkt != (*rxq->elts)[idx]);
				rep = pkt->next;
				pkt->next = NULL;
				pkt->nb_segs = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			break;
		}
		if (!pkt) {
			/* Looking for the new packet. */
			len = mlx4_cq_poll_one(rxq, &cqe);
			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}
			if (unlikely(len < 0)) {
				/* Rx error, packet is likely too large. */
				rte_mbuf_raw_free(rep);
				++rxq->stats.idropped;
				goto skip;
			}
			pkt = seg;
			pkt->packet_type = 0;
			pkt->ol_flags = 0;
			pkt->pkt_len = len;
		}
		rep->nb_segs = 1;
		rep->port = rxq->port_id;
		rep->data_len = seg->data_len;
		rep->data_off = seg->data_off;
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer. The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		scat->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(rep, uintptr_t));
		if (len > seg->data_len) {
			len -= seg->data_len;
			++pkt->nb_segs;
			++rq_ci;
			continue;
		}
		/* The last segment. */
		seg->data_len = len;
		/* Increment bytes counter. */
		rxq->stats.ibytes += pkt->pkt_len;
		/* Return packet. */
		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++i;
skip:
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely(i == 0 && (rq_ci >> sges_n) == rxq->rq_ci))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
	*rxq->mcq.set_ci_db = rte_cpu_to_be_32(rxq->mcq.cons_index & 0xffffff);
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
	return i;
}

/**
 * Dummy DPDK callback for Tx.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to Tx queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx4_tx_burst_removed(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

/**
 * Dummy DPDK callback for Rx.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to Rx queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx4_rx_burst_removed(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}
