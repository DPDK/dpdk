/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

/**
 * Manage TX completions.
 *
 * When sending a burst, mlx5_tx_burst() posts several WRs.
 * To improve performance, a completion event is only required once every
 * MLX5_PMD_TX_PER_COMP_REQ sends. Doing so discards completion information
 * for other WRs, but this information would not be used anyway.
 *
 * @param txq
 *   Pointer to TX queue structure.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
txq_complete(struct txq *txq)
{
	unsigned int elts_comp = txq->elts_comp;
	unsigned int elts_tail = txq->elts_tail;
	const unsigned int elts_n = txq->elts_n;
	int wcs_n;

	if (unlikely(elts_comp == 0))
		return 0;
#ifdef DEBUG_SEND
	DEBUG("%p: processing %u work requests completions",
	      (void *)txq, elts_comp);
#endif
	wcs_n = txq->if_cq->poll_cnt(txq->cq, elts_comp);
	if (unlikely(wcs_n == 0))
		return 0;
	if (unlikely(wcs_n < 0)) {
		DEBUG("%p: ibv_poll_cq() failed (wcs_n=%d)",
		      (void *)txq, wcs_n);
		return -1;
	}
	elts_comp -= wcs_n;
	assert(elts_comp <= txq->elts_comp);
	/*
	 * Assume WC status is successful as nothing can be done about it
	 * anyway.
	 */
	elts_tail += wcs_n * txq->elts_comp_cd_init;
	if (elts_tail >= elts_n)
		elts_tail -= elts_n;
	txq->elts_tail = elts_tail;
	txq->elts_comp = elts_comp;
	return 0;
}

/**
 * Get Memory Region (MR) <-> Memory Pool (MP) association from txq->mp2mr[].
 * Add MP to txq->mp2mr[] if it's not registered yet. If mp2mr[] is full,
 * remove an entry first.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
static uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
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
	DEBUG("%p: discovered new memory pool %p", (void *)txq, (void *)mp);
	mr = ibv_reg_mr(txq->priv->pd,
			(void *)mp->elt_va_start,
			(mp->elt_va_end - mp->elt_va_start),
			(IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
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
		claim_zero(ibv_dereg_mr(txq->mp2mr[i].mr));
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq->mp2mr[i].mp = mp;
	txq->mp2mr[i].mr = mr;
	txq->mp2mr[i].lkey = mr->lkey;
	DEBUG("%p: new MR lkey for MP %p: 0x%08" PRIu32,
	      (void *)txq, (void *)mp, txq->mp2mr[i].lkey);
	return txq->mp2mr[i].lkey;
}

/**
 * DPDK callback for TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	unsigned int elts_head = txq->elts_head;
	const unsigned int elts_tail = txq->elts_tail;
	const unsigned int elts_n = txq->elts_n;
	unsigned int elts_comp_cd = txq->elts_comp_cd;
	unsigned int elts_comp = 0;
	unsigned int i;
	unsigned int max;
	int err;

	assert(elts_comp_cd != 0);
	txq_complete(txq);
	max = (elts_n - (elts_head - elts_tail));
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
		unsigned int segs = NB_SEGS(buf);
		uint32_t send_flags = 0;

		/* Clean up old buffer. */
		if (likely(elt->buf != NULL)) {
			struct rte_mbuf *tmp = elt->buf;

			/* Faster than rte_pktmbuf_free(). */
			do {
				struct rte_mbuf *next = NEXT(tmp);

				rte_pktmbuf_free_seg(tmp);
				tmp = next;
			} while (tmp != NULL);
		}
		/* Request TX completion. */
		if (unlikely(--elts_comp_cd == 0)) {
			elts_comp_cd = txq->elts_comp_cd_init;
			++elts_comp;
			send_flags |= IBV_EXP_QP_BURST_SIGNALED;
		}
		if (likely(segs == 1)) {
			uintptr_t addr;
			uint32_t length;
			uint32_t lkey;

			/* Retrieve buffer information. */
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			length = DATA_LEN(buf);
			/* Retrieve Memory Region key for this memory pool. */
			lkey = txq_mp2mr(txq, buf->pool);
			if (unlikely(lkey == (uint32_t)-1)) {
				/* MR does not exist. */
				DEBUG("%p: unable to get MP <-> MR"
				      " association", (void *)txq);
				/* Clean up TX element. */
				elt->buf = NULL;
				goto stop;
			}
			/* Update element. */
			elt->buf = buf;
			if (txq->priv->vf)
				rte_prefetch0((volatile void *)
					      (uintptr_t)addr);
			RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
			/* Put packet into send queue. */
#if MLX5_PMD_MAX_INLINE > 0
			if (length <= txq->max_inline)
				err = txq->if_qp->send_pending_inline
					(txq->qp,
					 (void *)addr,
					 length,
					 send_flags);
			else
#endif
				err = txq->if_qp->send_pending
					(txq->qp,
					 addr,
					 length,
					 lkey,
					 send_flags);
			if (unlikely(err))
				goto stop;
		} else {
			DEBUG("%p: TX scattered buffers support not"
			      " compiled in", (void *)txq);
			goto stop;
		}
		elts_head = elts_head_next;
	}
stop:
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Ring QP doorbell. */
	err = txq->if_qp->send_flush(txq->qp);
	if (unlikely(err)) {
		/* A nonzero value is not supposed to be returned.
		 * Nothing can be done about it. */
		DEBUG("%p: send_flush() failed with error %d",
		      (void *)txq, err);
	}
	txq->elts_head = elts_head;
	txq->elts_comp += elts_comp;
	txq->elts_comp_cd = elts_comp_cd;
	return i;
}

/**
 * DPDK callback for RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct rxq_elt (*elts)[rxq->elts_n] = rxq->elts.no_sp;
	const unsigned int elts_n = rxq->elts_n;
	unsigned int elts_head = rxq->elts_head;
	struct ibv_sge sges[pkts_n];
	unsigned int i;
	unsigned int pkts_ret = 0;
	int ret;

	for (i = 0; (i != pkts_n); ++i) {
		struct rxq_elt *elt = &(*elts)[elts_head];
		struct ibv_recv_wr *wr = &elt->wr;
		uint64_t wr_id = wr->wr_id;
		unsigned int len;
		struct rte_mbuf *seg = (void *)((uintptr_t)elt->sge.addr -
			WR_ID(wr_id).offset);
		struct rte_mbuf *rep;
		uint32_t flags;

		/* Sanity checks. */
		assert(WR_ID(wr_id).id < rxq->elts_n);
		assert(wr->sg_list == &elt->sge);
		assert(wr->num_sge == 1);
		assert(elts_head < rxq->elts_n);
		assert(rxq->elts_head < rxq->elts_n);
		/*
		 * Fetch initial bytes of packet descriptor into a
		 * cacheline while allocating rep.
		 */
		rte_prefetch0(seg);
		rte_prefetch0(&seg->cacheline1);
		ret = rxq->if_cq->poll_length_flags(rxq->cq, NULL, NULL,
						    &flags);
		if (unlikely(ret < 0)) {
			struct ibv_wc wc;
			int wcs_n;

			DEBUG("rxq=%p, poll_length() failed (ret=%d)",
			      (void *)rxq, ret);
			/* ibv_poll_cq() must be used in case of failure. */
			wcs_n = ibv_poll_cq(rxq->cq, 1, &wc);
			if (unlikely(wcs_n == 0))
				break;
			if (unlikely(wcs_n < 0)) {
				DEBUG("rxq=%p, ibv_poll_cq() failed (wcs_n=%d)",
				      (void *)rxq, wcs_n);
				break;
			}
			assert(wcs_n == 1);
			if (unlikely(wc.status != IBV_WC_SUCCESS)) {
				/* Whatever, just repost the offending WR. */
				DEBUG("rxq=%p, wr_id=%" PRIu64 ": bad work"
				      " completion status (%d): %s",
				      (void *)rxq, wc.wr_id, wc.status,
				      ibv_wc_status_str(wc.status));
				/* Add SGE to array for repost. */
				sges[i] = elt->sge;
				goto repost;
			}
			ret = wc.byte_len;
		}
		if (ret == 0)
			break;
		len = ret;
		rep = __rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			/*
			 * Unable to allocate a replacement mbuf,
			 * repost WR.
			 */
			DEBUG("rxq=%p, wr_id=%" PRIu32 ":"
			      " can't allocate a new mbuf",
			      (void *)rxq, WR_ID(wr_id).id);
			/* Increment out of memory counters. */
			++rxq->priv->dev->data->rx_mbuf_alloc_failed;
			goto repost;
		}

		/* Reconfigure sge to use rep instead of seg. */
		elt->sge.addr = (uintptr_t)rep->buf_addr + RTE_PKTMBUF_HEADROOM;
		assert(elt->sge.lkey == rxq->mr->lkey);
		WR_ID(wr->wr_id).offset =
			(((uintptr_t)rep->buf_addr + RTE_PKTMBUF_HEADROOM) -
			 (uintptr_t)rep);
		assert(WR_ID(wr->wr_id).id == WR_ID(wr_id).id);

		/* Add SGE to array for repost. */
		sges[i] = elt->sge;

		/* Update seg information. */
		SET_DATA_OFF(seg, RTE_PKTMBUF_HEADROOM);
		NB_SEGS(seg) = 1;
		PORT(seg) = rxq->port_id;
		NEXT(seg) = NULL;
		PKT_LEN(seg) = len;
		DATA_LEN(seg) = len;

		/* Return packet. */
		*(pkts++) = seg;
		++pkts_ret;
repost:
		if (++elts_head >= elts_n)
			elts_head = 0;
		continue;
	}
	if (unlikely(i == 0))
		return 0;
	/* Repost WRs. */
#ifdef DEBUG_RECV
	DEBUG("%p: reposting %u WRs", (void *)rxq, i);
#endif
	ret = rxq->if_qp->recv_burst(rxq->qp, sges, i);
	if (unlikely(ret)) {
		/* Inability to repost WRs is fatal. */
		DEBUG("%p: recv_burst(): failed (ret=%d)",
		      (void *)rxq->priv,
		      ret);
		abort();
	}
	rxq->elts_head = elts_head;
	return pkts_ret;
}

/**
 * Dummy DPDK callback for TX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
removed_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

/**
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
removed_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}
