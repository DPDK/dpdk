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
 * Get Memory Pool (MP) from mbuf. If mbuf is indirect, the pool from which
 * the cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static struct rte_mempool *
txq_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_INDIRECT(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
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
txq_mp2mr(struct txq *txq, const struct rte_mempool *mp)
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
	      (void *)txq, mp->name, (const void *)mp);
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
		claim_zero(ibv_dereg_mr(txq->mp2mr[0].mr));
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq->mp2mr[i].mp = mp;
	txq->mp2mr[i].mr = mr;
	txq->mp2mr[i].lkey = mr->lkey;
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq, mp->name, (const void *)mp, txq->mp2mr[i].lkey);
	return txq->mp2mr[i].lkey;
}

struct txq_mp2mr_mbuf_check_data {
	const struct rte_mempool *mp;
	int ret;
};

/**
 * Callback function for rte_mempool_obj_iter() to check whether a given
 * mempool object looks like a mbuf.
 *
 * @param[in, out] arg
 *   Context data (struct txq_mp2mr_mbuf_check_data). Contains mempool pointer
 *   and return value.
 * @param[in] start
 *   Object start address.
 * @param[in] end
 *   Object end address.
 * @param index
 *   Unused.
 *
 * @return
 *   Nonzero value when object is not a mbuf.
 */
static void
txq_mp2mr_mbuf_check(void *arg, void *start, void *end,
		     uint32_t index __rte_unused)
{
	struct txq_mp2mr_mbuf_check_data *data = arg;
	struct rte_mbuf *buf =
		(void *)((uintptr_t)start + data->mp->header_size);

	(void)index;
	/* Check whether mbuf structure fits element size and whether mempool
	 * pointer is valid. */
	if (((uintptr_t)end >= (uintptr_t)(buf + 1)) &&
	    (buf->pool == data->mp))
		data->ret = 0;
	else
		data->ret = -1;
}

/**
 * Iterator function for rte_mempool_walk() to register existing mempools and
 * fill the MP to MR cache of a TX queue.
 *
 * @param[in] mp
 *   Memory Pool to register.
 * @param *arg
 *   Pointer to TX queue structure.
 */
void
txq_mp2mr_iter(const struct rte_mempool *mp, void *arg)
{
	struct txq *txq = arg;
	struct txq_mp2mr_mbuf_check_data data = {
		.mp = mp,
		.ret = -1,
	};

	/* Discard empty mempools. */
	if (mp->size == 0)
		return;
	/* Register mempool only if the first element looks like a mbuf. */
	rte_mempool_obj_iter((void *)mp->elt_va_start,
			     1,
			     mp->header_size + mp->elt_size + mp->trailer_size,
			     1,
			     mp->elt_pa,
			     mp->pg_num,
			     mp->pg_shift,
			     txq_mp2mr_mbuf_check,
			     &data);
	if (data.ret)
		return;
	txq_mp2mr(txq, mp);
}

#if MLX5_PMD_SGE_WR_N > 1

/**
 * Copy scattered mbuf contents to a single linear buffer.
 *
 * @param[out] linear
 *   Linear output buffer.
 * @param[in] buf
 *   Scattered input buffer.
 *
 * @return
 *   Number of bytes copied to the output buffer or 0 if not large enough.
 */
static unsigned int
linearize_mbuf(linear_t *linear, struct rte_mbuf *buf)
{
	unsigned int size = 0;
	unsigned int offset;

	do {
		unsigned int len = DATA_LEN(buf);

		offset = size;
		size += len;
		if (unlikely(size > sizeof(*linear)))
			return 0;
		memcpy(&(*linear)[offset],
		       rte_pktmbuf_mtod(buf, uint8_t *),
		       len);
		buf = NEXT(buf);
	} while (buf != NULL);
	return size;
}

/**
 * Handle scattered buffers for mlx5_tx_burst().
 *
 * @param txq
 *   TX queue structure.
 * @param segs
 *   Number of segments in buf.
 * @param elt
 *   TX queue element to fill.
 * @param[in] buf
 *   Buffer to process.
 * @param elts_head
 *   Index of the linear buffer to use if necessary (normally txq->elts_head).
 * @param[out] sges
 *   Array filled with SGEs on success.
 *
 * @return
 *   A structure containing the processed packet size in bytes and the
 *   number of SGEs. Both fields are set to (unsigned int)-1 in case of
 *   failure.
 */
static struct tx_burst_sg_ret {
	unsigned int length;
	unsigned int num;
}
tx_burst_sg(struct txq *txq, unsigned int segs, struct txq_elt *elt,
	    struct rte_mbuf *buf, unsigned int elts_head,
	    struct ibv_sge (*sges)[MLX5_PMD_SGE_WR_N])
{
	unsigned int sent_size = 0;
	unsigned int j;
	int linearize = 0;

	/* When there are too many segments, extra segments are
	 * linearized in the last SGE. */
	if (unlikely(segs > RTE_DIM(*sges))) {
		segs = (RTE_DIM(*sges) - 1);
		linearize = 1;
	}
	/* Update element. */
	elt->buf = buf;
	/* Register segments as SGEs. */
	for (j = 0; (j != segs); ++j) {
		struct ibv_sge *sge = &(*sges)[j];
		uint32_t lkey;

		/* Retrieve Memory Region key for this memory pool. */
		lkey = txq_mp2mr(txq, txq_mb2mp(buf));
		if (unlikely(lkey == (uint32_t)-1)) {
			/* MR does not exist. */
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			/* Clean up TX element. */
			elt->buf = NULL;
			goto stop;
		}
		/* Update SGE. */
		sge->addr = rte_pktmbuf_mtod(buf, uintptr_t);
		if (txq->priv->vf)
			rte_prefetch0((volatile void *)
				      (uintptr_t)sge->addr);
		sge->length = DATA_LEN(buf);
		sge->lkey = lkey;
		sent_size += sge->length;
		buf = NEXT(buf);
	}
	/* If buf is not NULL here and is not going to be linearized,
	 * nb_segs is not valid. */
	assert(j == segs);
	assert((buf == NULL) || (linearize));
	/* Linearize extra segments. */
	if (linearize) {
		struct ibv_sge *sge = &(*sges)[segs];
		linear_t *linear = &(*txq->elts_linear)[elts_head];
		unsigned int size = linearize_mbuf(linear, buf);

		assert(segs == (RTE_DIM(*sges) - 1));
		if (size == 0) {
			/* Invalid packet. */
			DEBUG("%p: packet too large to be linearized.",
			      (void *)txq);
			/* Clean up TX element. */
			elt->buf = NULL;
			goto stop;
		}
		/* If MLX5_PMD_SGE_WR_N is 1, free mbuf immediately. */
		if (RTE_DIM(*sges) == 1) {
			do {
				struct rte_mbuf *next = NEXT(buf);

				rte_pktmbuf_free_seg(buf);
				buf = next;
			} while (buf != NULL);
			elt->buf = NULL;
		}
		/* Update SGE. */
		sge->addr = (uintptr_t)&(*linear)[0];
		sge->length = size;
		sge->lkey = txq->mr_linear->lkey;
		sent_size += size;
		/* Include last segment. */
		segs++;
	}
	return (struct tx_burst_sg_ret){
		.length = sent_size,
		.num = segs,
	};
stop:
	return (struct tx_burst_sg_ret){
		.length = -1,
		.num = -1,
	};
}

#endif /* MLX5_PMD_SGE_WR_N > 1 */

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
	const unsigned int elts_n = txq->elts_n;
	unsigned int elts_comp_cd = txq->elts_comp_cd;
	unsigned int elts_comp = 0;
	unsigned int i;
	unsigned int max;
	int err;

	assert(elts_comp_cd != 0);
	txq_complete(txq);
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
		unsigned int segs = NB_SEGS(buf);
#ifdef MLX5_PMD_SOFT_COUNTERS
		unsigned int sent_size = 0;
#endif
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
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
			send_flags |= IBV_EXP_QP_BURST_IP_CSUM;
			/* HW does not support checksum offloads at arbitrary
			 * offsets but automatically recognizes the packet
			 * type. For inner L3/L4 checksums, only VXLAN (UDP)
			 * tunnels are currently supported. */
			if (RTE_ETH_IS_TUNNEL_PKT(buf->packet_type))
				send_flags |= IBV_EXP_QP_BURST_TUNNEL;
		}
		if (likely(segs == 1)) {
			uintptr_t addr;
			uint32_t length;
			uint32_t lkey;

			/* Retrieve buffer information. */
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			length = DATA_LEN(buf);
			/* Retrieve Memory Region key for this memory pool. */
			lkey = txq_mp2mr(txq, txq_mb2mp(buf));
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
#ifdef MLX5_PMD_SOFT_COUNTERS
			sent_size += length;
#endif
		} else {
#if MLX5_PMD_SGE_WR_N > 1
			struct ibv_sge sges[MLX5_PMD_SGE_WR_N];
			struct tx_burst_sg_ret ret;

			ret = tx_burst_sg(txq, segs, elt, buf, elts_head,
					  &sges);
			if (ret.length == (unsigned int)-1)
				goto stop;
			RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
			/* Put SG list into send queue. */
			err = txq->if_qp->send_pending_sg_list
				(txq->qp,
				 sges,
				 ret.num,
				 send_flags);
			if (unlikely(err))
				goto stop;
#ifdef MLX5_PMD_SOFT_COUNTERS
			sent_size += ret.length;
#endif
#else /* MLX5_PMD_SGE_WR_N > 1 */
			DEBUG("%p: TX scattered buffers support not"
			      " compiled in", (void *)txq);
			goto stop;
#endif /* MLX5_PMD_SGE_WR_N > 1 */
		}
		elts_head = elts_head_next;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += sent_size;
#endif
	}
stop:
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
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
 * Translate RX completion flags to packet type.
 *
 * @param flags
 *   RX completion flags returned by poll_length_flags().
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(uint32_t flags)
{
	uint32_t pkt_type;

	if (flags & IBV_EXP_CQ_RX_TUNNEL_PACKET)
		pkt_type =
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_OUTER_IPV4_PACKET,
				  RTE_PTYPE_L3_IPV4) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_OUTER_IPV6_PACKET,
				  RTE_PTYPE_L3_IPV6) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV4_PACKET,
				  RTE_PTYPE_INNER_L3_IPV4) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV6_PACKET,
				  RTE_PTYPE_INNER_L3_IPV6);
	else
		pkt_type =
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV4_PACKET,
				  RTE_PTYPE_L3_IPV4) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV6_PACKET,
				  RTE_PTYPE_L3_IPV6);
	return pkt_type;
}

/**
 * Translate RX completion flags to offload flags.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param flags
 *   RX completion flags returned by poll_length_flags().
 *
 * @return
 *   Offload flags (ol_flags) for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_ol_flags(const struct rxq *rxq, uint32_t flags)
{
	uint32_t ol_flags = 0;

	if (rxq->csum)
		ol_flags |=
			TRANSPOSE(~flags,
				  IBV_EXP_CQ_RX_IP_CSUM_OK,
				  PKT_RX_IP_CKSUM_BAD) |
			TRANSPOSE(~flags,
				  IBV_EXP_CQ_RX_TCP_UDP_CSUM_OK,
				  PKT_RX_L4_CKSUM_BAD);
	/*
	 * PKT_RX_IP_CKSUM_BAD and PKT_RX_L4_CKSUM_BAD are used in place
	 * of PKT_RX_EIP_CKSUM_BAD because the latter is not functional
	 * (its value is 0).
	 */
	if ((flags & IBV_EXP_CQ_RX_TUNNEL_PACKET) && (rxq->csum_l2tun))
		ol_flags |=
			TRANSPOSE(~flags,
				  IBV_EXP_CQ_RX_OUTER_IP_CSUM_OK,
				  PKT_RX_IP_CKSUM_BAD) |
			TRANSPOSE(~flags,
				  IBV_EXP_CQ_RX_OUTER_TCP_UDP_CSUM_OK,
				  PKT_RX_L4_CKSUM_BAD);
	return ol_flags;
}

/**
 * DPDK callback for RX with scattered packets support.
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
mlx5_rx_burst_sp(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct rxq_elt_sp (*elts)[rxq->elts_n] = rxq->elts.sp;
	const unsigned int elts_n = rxq->elts_n;
	unsigned int elts_head = rxq->elts_head;
	unsigned int i;
	unsigned int pkts_ret = 0;
	int ret;

	if (unlikely(!rxq->sp))
		return mlx5_rx_burst(dpdk_rxq, pkts, pkts_n);
	if (unlikely(elts == NULL)) /* See RTE_DEV_CMD_SET_MTU. */
		return 0;
	for (i = 0; (i != pkts_n); ++i) {
		struct rxq_elt_sp *elt = &(*elts)[elts_head];
		unsigned int len;
		unsigned int pkt_buf_len;
		struct rte_mbuf *pkt_buf = NULL; /* Buffer returned in pkts. */
		struct rte_mbuf **pkt_buf_next = &pkt_buf;
		unsigned int seg_headroom = RTE_PKTMBUF_HEADROOM;
		unsigned int j = 0;
		uint32_t flags;

		/* Sanity checks. */
		assert(elts_head < rxq->elts_n);
		assert(rxq->elts_head < rxq->elts_n);
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
#ifdef MLX5_PMD_SOFT_COUNTERS
				/* Increment dropped packets counter. */
				++rxq->stats.idropped;
#endif
				goto repost;
			}
			ret = wc.byte_len;
		}
		if (ret == 0)
			break;
		len = ret;
		pkt_buf_len = len;
		/*
		 * Replace spent segments with new ones, concatenate and
		 * return them as pkt_buf.
		 */
		while (1) {
			struct ibv_sge *sge = &elt->sges[j];
			struct rte_mbuf *seg = elt->bufs[j];
			struct rte_mbuf *rep;
			unsigned int seg_tailroom;

			assert(seg != NULL);
			/*
			 * Fetch initial bytes of packet descriptor into a
			 * cacheline while allocating rep.
			 */
			rte_prefetch0(seg);
			rep = __rte_mbuf_raw_alloc(rxq->mp);
			if (unlikely(rep == NULL)) {
				/*
				 * Unable to allocate a replacement mbuf,
				 * repost WR.
				 */
				DEBUG("rxq=%p: can't allocate a new mbuf",
				      (void *)rxq);
				if (pkt_buf != NULL) {
					*pkt_buf_next = NULL;
					rte_pktmbuf_free(pkt_buf);
				}
				/* Increment out of memory counters. */
				++rxq->stats.rx_nombuf;
				++rxq->priv->dev->data->rx_mbuf_alloc_failed;
				goto repost;
			}
#ifndef NDEBUG
			/* Poison user-modifiable fields in rep. */
			NEXT(rep) = (void *)((uintptr_t)-1);
			SET_DATA_OFF(rep, 0xdead);
			DATA_LEN(rep) = 0xd00d;
			PKT_LEN(rep) = 0xdeadd00d;
			NB_SEGS(rep) = 0x2a;
			PORT(rep) = 0x2a;
			rep->ol_flags = -1;
#endif
			assert(rep->buf_len == seg->buf_len);
			assert(rep->buf_len == rxq->mb_len);
			/* Reconfigure sge to use rep instead of seg. */
			assert(sge->lkey == rxq->mr->lkey);
			sge->addr = ((uintptr_t)rep->buf_addr + seg_headroom);
			elt->bufs[j] = rep;
			++j;
			/* Update pkt_buf if it's the first segment, or link
			 * seg to the previous one and update pkt_buf_next. */
			*pkt_buf_next = seg;
			pkt_buf_next = &NEXT(seg);
			/* Update seg information. */
			seg_tailroom = (seg->buf_len - seg_headroom);
			assert(sge->length == seg_tailroom);
			SET_DATA_OFF(seg, seg_headroom);
			if (likely(len <= seg_tailroom)) {
				/* Last segment. */
				DATA_LEN(seg) = len;
				PKT_LEN(seg) = len;
				/* Sanity check. */
				assert(rte_pktmbuf_headroom(seg) ==
				       seg_headroom);
				assert(rte_pktmbuf_tailroom(seg) ==
				       (seg_tailroom - len));
				break;
			}
			DATA_LEN(seg) = seg_tailroom;
			PKT_LEN(seg) = seg_tailroom;
			/* Sanity check. */
			assert(rte_pktmbuf_headroom(seg) == seg_headroom);
			assert(rte_pktmbuf_tailroom(seg) == 0);
			/* Fix len and clear headroom for next segments. */
			len -= seg_tailroom;
			seg_headroom = 0;
		}
		/* Update head and tail segments. */
		*pkt_buf_next = NULL;
		assert(pkt_buf != NULL);
		assert(j != 0);
		NB_SEGS(pkt_buf) = j;
		PORT(pkt_buf) = rxq->port_id;
		PKT_LEN(pkt_buf) = pkt_buf_len;
		pkt_buf->packet_type = rxq_cq_to_pkt_type(flags);
		pkt_buf->ol_flags = rxq_cq_to_ol_flags(rxq, flags);

		/* Return packet. */
		*(pkts++) = pkt_buf;
		++pkts_ret;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += pkt_buf_len;
#endif
repost:
		ret = rxq->if_wq->recv_sg_list(rxq->wq,
					       elt->sges,
					       RTE_DIM(elt->sges));
		if (unlikely(ret)) {
			/* Inability to repost WRs is fatal. */
			DEBUG("%p: recv_sg_list(): failed (ret=%d)",
			      (void *)rxq->priv,
			      ret);
			abort();
		}
		if (++elts_head >= elts_n)
			elts_head = 0;
		continue;
	}
	if (unlikely(i == 0))
		return 0;
	rxq->elts_head = elts_head;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += pkts_ret;
#endif
	return pkts_ret;
}

/**
 * DPDK callback for RX.
 *
 * The following function is the same as mlx5_rx_burst_sp(), except it doesn't
 * manage scattered packets. Improves performance when MRU is lower than the
 * size of the first segment.
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

	if (unlikely(rxq->sp))
		return mlx5_rx_burst_sp(dpdk_rxq, pkts, pkts_n);
	for (i = 0; (i != pkts_n); ++i) {
		struct rxq_elt *elt = &(*elts)[elts_head];
		unsigned int len;
		struct rte_mbuf *seg = elt->buf;
		struct rte_mbuf *rep;
		uint32_t flags;

		/* Sanity checks. */
		assert(seg != NULL);
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
#ifdef MLX5_PMD_SOFT_COUNTERS
				/* Increment dropped packets counter. */
				++rxq->stats.idropped;
#endif
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
			DEBUG("rxq=%p: can't allocate a new mbuf",
			      (void *)rxq);
			/* Increment out of memory counters. */
			++rxq->stats.rx_nombuf;
			++rxq->priv->dev->data->rx_mbuf_alloc_failed;
			goto repost;
		}

		/* Reconfigure sge to use rep instead of seg. */
		elt->sge.addr = (uintptr_t)rep->buf_addr + RTE_PKTMBUF_HEADROOM;
		assert(elt->sge.lkey == rxq->mr->lkey);
		elt->buf = rep;

		/* Add SGE to array for repost. */
		sges[i] = elt->sge;

		/* Update seg information. */
		SET_DATA_OFF(seg, RTE_PKTMBUF_HEADROOM);
		NB_SEGS(seg) = 1;
		PORT(seg) = rxq->port_id;
		NEXT(seg) = NULL;
		PKT_LEN(seg) = len;
		DATA_LEN(seg) = len;
		seg->packet_type = rxq_cq_to_pkt_type(flags);
		seg->ol_flags = rxq_cq_to_ol_flags(rxq, flags);

		/* Return packet. */
		*(pkts++) = seg;
		++pkts_ret;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += len;
#endif
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
	ret = rxq->if_wq->recv_burst(rxq->wq, sges, i);
	if (unlikely(ret)) {
		/* Inability to repost WRs is fatal. */
		DEBUG("%p: recv_burst(): failed (ret=%d)",
		      (void *)rxq->priv,
		      ret);
		abort();
	}
	rxq->elts_head = elts_head;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += pkts_ret;
#endif
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
