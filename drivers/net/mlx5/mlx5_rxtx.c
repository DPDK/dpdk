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
#include <infiniband/mlx5_hw.h>
#include <infiniband/arch.h>
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
#include <rte_ether.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

#ifndef NDEBUG

/**
 * Verify or set magic value in CQE.
 *
 * @param cqe
 *   Pointer to CQE.
 *
 * @return
 *   0 the first time.
 */
static inline int
check_cqe64_seen(volatile struct mlx5_cqe64 *cqe)
{
	static const uint8_t magic[] = "seen";
	volatile uint8_t (*buf)[sizeof(cqe->rsvd40)] = &cqe->rsvd40;
	int ret = 1;
	unsigned int i;

	for (i = 0; i < sizeof(magic) && i < sizeof(*buf); ++i)
		if (!ret || (*buf)[i] != magic[i]) {
			ret = 0;
			(*buf)[i] = magic[i];
		}
	return ret;
}

#endif /* NDEBUG */

static inline int
check_cqe64(volatile struct mlx5_cqe64 *cqe,
	    unsigned int cqes_n, const uint16_t ci)
	    __attribute__((always_inline));

/**
 * Check whether CQE is valid.
 *
 * @param cqe
 *   Pointer to CQE.
 * @param cqes_n
 *   Size of completion queue.
 * @param ci
 *   Consumer index.
 *
 * @return
 *   0 on success, 1 on failure.
 */
static inline int
check_cqe64(volatile struct mlx5_cqe64 *cqe,
		unsigned int cqes_n, const uint16_t ci)
{
	uint16_t idx = ci & cqes_n;
	uint8_t op_own = cqe->op_own;
	uint8_t op_owner = MLX5_CQE_OWNER(op_own);
	uint8_t op_code = MLX5_CQE_OPCODE(op_own);

	if (unlikely((op_owner != (!!(idx))) || (op_code == MLX5_CQE_INVALID)))
		return 1; /* No CQE. */
#ifndef NDEBUG
	if ((op_code == MLX5_CQE_RESP_ERR) ||
	    (op_code == MLX5_CQE_REQ_ERR)) {
		volatile struct mlx5_err_cqe *err_cqe = (volatile void *)cqe;
		uint8_t syndrome = err_cqe->syndrome;

		if ((syndrome == MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR) ||
		    (syndrome == MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR))
			return 0;
		if (!check_cqe64_seen(cqe))
			ERROR("unexpected CQE error %u (0x%02x)"
			      " syndrome 0x%02x",
			      op_code, op_code, syndrome);
		return 1;
	} else if ((op_code != MLX5_CQE_RESP_SEND) &&
		   (op_code != MLX5_CQE_REQ)) {
		if (!check_cqe64_seen(cqe))
			ERROR("unexpected CQE opcode %u (0x%02x)",
			      op_code, op_code);
		return 1;
	}
#endif /* NDEBUG */
	return 0;
}

/**
 * Manage TX completions.
 *
 * When sending a burst, mlx5_tx_burst() posts several WRs.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static void
txq_complete(struct txq *txq)
{
	const unsigned int elts_n = txq->elts_n;
	const unsigned int cqe_n = txq->cqe_n;
	const unsigned int cqe_cnt = cqe_n - 1;
	uint16_t elts_free = txq->elts_tail;
	uint16_t elts_tail;
	uint16_t cq_ci = txq->cq_ci;
	volatile struct mlx5_cqe64 *cqe = NULL;
	volatile union mlx5_wqe *wqe;

	do {
		volatile struct mlx5_cqe64 *tmp;

		tmp = &(*txq->cqes)[cq_ci & cqe_cnt].cqe64;
		if (check_cqe64(tmp, cqe_n, cq_ci))
			break;
		cqe = tmp;
#ifndef NDEBUG
		if (MLX5_CQE_FORMAT(cqe->op_own) == MLX5_COMPRESSED) {
			if (!check_cqe64_seen(cqe))
				ERROR("unexpected compressed CQE, TX stopped");
			return;
		}
		if ((MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_RESP_ERR) ||
		    (MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_REQ_ERR)) {
			if (!check_cqe64_seen(cqe))
				ERROR("unexpected error CQE, TX stopped");
			return;
		}
#endif /* NDEBUG */
		++cq_ci;
	} while (1);
	if (unlikely(cqe == NULL))
		return;
	wqe = &(*txq->wqes)[htons(cqe->wqe_counter) & (txq->wqe_n - 1)];
	elts_tail = wqe->wqe.ctrl.data[3];
	assert(elts_tail < txq->wqe_n);
	/* Free buffers. */
	while (elts_free != elts_tail) {
		struct rte_mbuf *elt = (*txq->elts)[elts_free];
		unsigned int elts_free_next =
			(elts_free + 1) & (elts_n - 1);
		struct rte_mbuf *elt_next = (*txq->elts)[elts_free_next];

#ifndef NDEBUG
		/* Poisoning. */
		memset(&(*txq->elts)[elts_free],
		       0x66,
		       sizeof((*txq->elts)[elts_free]));
#endif
		RTE_MBUF_PREFETCH_TO_FREE(elt_next);
		/* Only one segment needs to be freed. */
		rte_pktmbuf_free_seg(elt);
		elts_free = elts_free_next;
	}
	txq->cq_ci = cq_ci;
	txq->elts_tail = elts_tail;
	/* Update the consumer index. */
	rte_wmb();
	*txq->cq_db = htonl(cq_ci);
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

static inline uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
	__attribute__((always_inline));

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
static inline uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
{
	unsigned int i;
	uint32_t lkey = (uint32_t)-1;

	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].mp == mp) {
			assert(txq->mp2mr[i].lkey != (uint32_t)-1);
			assert(htonl(txq->mp2mr[i].mr->lkey) ==
			       txq->mp2mr[i].lkey);
			lkey = txq->mp2mr[i].lkey;
			break;
		}
	}
	if (unlikely(lkey == (uint32_t)-1))
		lkey = txq_mp2mr_reg(txq, mp, i);
	return lkey;
}

/**
 * Write a regular WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the WQE to fill.
 * @param addr
 *   Buffer data address.
 * @param length
 *   Packet length.
 * @param lkey
 *   Memory region lkey.
 */
static inline void
mlx5_wqe_write(struct txq *txq, volatile union mlx5_wqe *wqe,
	       uintptr_t addr, uint32_t length, uint32_t lkey)
{
	wqe->wqe.ctrl.data[0] = htonl((txq->wqe_ci << 8) | MLX5_OPCODE_SEND);
	wqe->wqe.ctrl.data[1] = htonl((txq->qp_num_8s) | 4);
	wqe->wqe.ctrl.data[3] = 0;
	wqe->inl.eseg.rsvd0 = 0;
	wqe->inl.eseg.rsvd1 = 0;
	wqe->inl.eseg.mss = 0;
	wqe->inl.eseg.rsvd2 = 0;
	wqe->wqe.eseg.inline_hdr_sz = htons(MLX5_ETH_INLINE_HEADER_SIZE);
	/* Copy the first 16 bytes into inline header. */
	rte_memcpy((uint8_t *)(uintptr_t)wqe->wqe.eseg.inline_hdr_start,
		   (uint8_t *)(uintptr_t)addr,
		   MLX5_ETH_INLINE_HEADER_SIZE);
	addr += MLX5_ETH_INLINE_HEADER_SIZE;
	length -= MLX5_ETH_INLINE_HEADER_SIZE;
	/* Store remaining data in data segment. */
	wqe->wqe.dseg.byte_count = htonl(length);
	wqe->wqe.dseg.lkey = lkey;
	wqe->wqe.dseg.addr = htonll(addr);
	/* Increment consumer index. */
	++txq->wqe_ci;
}

/**
 * Write a regular WQE with VLAN.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the WQE to fill.
 * @param addr
 *   Buffer data address.
 * @param length
 *   Packet length.
 * @param lkey
 *   Memory region lkey.
 * @param vlan_tci
 *   VLAN field to insert in packet.
 */
static inline void
mlx5_wqe_write_vlan(struct txq *txq, volatile union mlx5_wqe *wqe,
		    uintptr_t addr, uint32_t length, uint32_t lkey,
		    uint16_t vlan_tci)
{
	uint32_t vlan = htonl(0x81000000 | vlan_tci);

	wqe->wqe.ctrl.data[0] = htonl((txq->wqe_ci << 8) | MLX5_OPCODE_SEND);
	wqe->wqe.ctrl.data[1] = htonl((txq->qp_num_8s) | 4);
	wqe->wqe.ctrl.data[3] = 0;
	wqe->inl.eseg.rsvd0 = 0;
	wqe->inl.eseg.rsvd1 = 0;
	wqe->inl.eseg.mss = 0;
	wqe->inl.eseg.rsvd2 = 0;
	wqe->wqe.eseg.inline_hdr_sz = htons(MLX5_ETH_VLAN_INLINE_HEADER_SIZE);
	/*
	 * Copy 12 bytes of source & destination MAC address.
	 * Copy 4 bytes of VLAN.
	 * Copy 2 bytes of Ether type.
	 */
	rte_memcpy((uint8_t *)(uintptr_t)wqe->wqe.eseg.inline_hdr_start,
		   (uint8_t *)(uintptr_t)addr, 12);
	rte_memcpy((uint8_t *)((uintptr_t)wqe->wqe.eseg.inline_hdr_start + 12),
		   &vlan, sizeof(vlan));
	rte_memcpy((uint8_t *)((uintptr_t)wqe->wqe.eseg.inline_hdr_start + 16),
		   (uint8_t *)((uintptr_t)addr + 12), 2);
	addr += MLX5_ETH_VLAN_INLINE_HEADER_SIZE - sizeof(vlan);
	length -= MLX5_ETH_VLAN_INLINE_HEADER_SIZE - sizeof(vlan);
	/* Store remaining data in data segment. */
	wqe->wqe.dseg.byte_count = htonl(length);
	wqe->wqe.dseg.lkey = lkey;
	wqe->wqe.dseg.addr = htonll(addr);
	/* Increment consumer index. */
	++txq->wqe_ci;
}

/**
 * Ring TX queue doorbell.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static inline void
mlx5_tx_dbrec(struct txq *txq)
{
	uint8_t *dst = (uint8_t *)((uintptr_t)txq->bf_reg + txq->bf_offset);
	uint32_t data[4] = {
		htonl((txq->wqe_ci << 8) | MLX5_OPCODE_SEND),
		htonl(txq->qp_num_8s),
		0,
		0,
	};
	rte_wmb();
	*txq->qp_db = htonl(txq->wqe_ci);
	/* Ensure ordering between DB record and BF copy. */
	rte_wmb();
	rte_mov16(dst, (uint8_t *)data);
	txq->bf_offset ^= txq->bf_buf_size;
}

/**
 * Prefetch a CQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param cqe_ci
 *   CQE consumer index.
 */
static inline void
tx_prefetch_cqe(struct txq *txq, uint16_t ci)
{
	volatile struct mlx5_cqe64 *cqe;

	cqe = &(*txq->cqes)[ci & (txq->cqe_n - 1)].cqe64;
	rte_prefetch0(cqe);
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
	uint16_t elts_head = txq->elts_head;
	const unsigned int elts_n = txq->elts_n;
	unsigned int i;
	unsigned int max;
	unsigned int comp;
	volatile union mlx5_wqe *wqe;
	struct rte_mbuf *buf;

	if (unlikely(!pkts_n))
		return 0;
	buf = pkts[0];
	/* Prefetch first packet cacheline. */
	tx_prefetch_cqe(txq, txq->cq_ci);
	tx_prefetch_cqe(txq, txq->cq_ci + 1);
	rte_prefetch0(buf);
	/* Start processing. */
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
		unsigned int elts_head_next = (elts_head + 1) & (elts_n - 1);
		uintptr_t addr;
		uint32_t length;
		uint32_t lkey;

		wqe = &(*txq->wqes)[txq->wqe_ci & (txq->wqe_n - 1)];
		rte_prefetch0(wqe);
		if (i + 1 < max)
			rte_prefetch0(pkts[i + 1]);
		/* Retrieve buffer information. */
		addr = rte_pktmbuf_mtod(buf, uintptr_t);
		length = DATA_LEN(buf);
		/* Update element. */
		(*txq->elts)[elts_head] = buf;
		/* Prefetch next buffer data. */
		if (i + 1 < max)
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i + 1],
						       volatile void *));
		/* Retrieve Memory Region key for this memory pool. */
		lkey = txq_mp2mr(txq, txq_mb2mp(buf));
		if (buf->ol_flags & PKT_TX_VLAN_PKT)
			mlx5_wqe_write_vlan(txq, wqe, addr, length, lkey,
					    buf->vlan_tci);
		else
			mlx5_wqe_write(txq, wqe, addr, length, lkey);
		wqe->wqe.ctrl.data[2] = 0;
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
			wqe->wqe.eseg.cs_flags =
				MLX5_ETH_WQE_L3_CSUM |
				MLX5_ETH_WQE_L4_CSUM;
		} else {
			wqe->wqe.eseg.cs_flags = 0;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		elts_head = elts_head_next;
		buf = pkts[i + 1];
	}
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	comp = txq->elts_comp + i;
	if (comp >= MLX5_TX_COMP_THRESH) {
		/* Request completion on last WQE. */
		wqe->wqe.ctrl.data[2] = htonl(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->wqe.ctrl.data[3] = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Translate RX completion flags to packet type.
 *
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(volatile struct mlx5_cqe64 *cqe)
{
	uint32_t pkt_type;
	uint8_t flags = cqe->l4_hdr_type_etc;
	uint8_t info = cqe->rsvd0[0];

	if (info & IBV_EXP_CQ_RX_TUNNEL_PACKET)
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
				  MLX5_CQE_L3_HDR_TYPE_IPV6,
				  RTE_PTYPE_L3_IPV6) |
			TRANSPOSE(flags,
				  MLX5_CQE_L3_HDR_TYPE_IPV4,
				  RTE_PTYPE_L3_IPV4);
	return pkt_type;
}

/**
 * Get size of the next packet for a given CQE. For compressed CQEs, the
 * consumer index is updated only once all packets of the current one have
 * been processed.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param cqe
 *   CQE to process.
 *
 * @return
 *   Packet size in bytes (0 if there is none), -1 in case of completion
 *   with error.
 */
static inline int
mlx5_rx_poll_len(struct rxq *rxq, volatile struct mlx5_cqe64 *cqe,
		 uint16_t cqe_cnt)
{
	struct rxq_zip *zip = &rxq->zip;
	uint16_t cqe_n = cqe_cnt + 1;
	int len = 0;

	/* Process compressed data in the CQE and mini arrays. */
	if (zip->ai) {
		volatile struct mlx5_mini_cqe8 (*mc)[8] =
			(volatile struct mlx5_mini_cqe8 (*)[8])
			(uintptr_t)(&(*rxq->cqes)[zip->ca & cqe_cnt].cqe64);

		len = ntohl((*mc)[zip->ai & 7].byte_cnt);
		if ((++zip->ai & 7) == 0) {
			/*
			 * Increment consumer index to skip the number of
			 * CQEs consumed. Hardware leaves holes in the CQ
			 * ring for software use.
			 */
			zip->ca = zip->na;
			zip->na += 8;
		}
		if (unlikely(rxq->zip.ai == rxq->zip.cqe_cnt)) {
			uint16_t idx = rxq->cq_ci;
			uint16_t end = zip->cq_ci;

			while (idx != end) {
				(*rxq->cqes)[idx & cqe_cnt].cqe64.op_own =
					MLX5_CQE_INVALIDATE;
				++idx;
			}
			rxq->cq_ci = zip->cq_ci;
			zip->ai = 0;
		}
	/* No compressed data, get next CQE and verify if it is compressed. */
	} else {
		int ret;
		int8_t op_own;

		ret = check_cqe64(cqe, cqe_n, rxq->cq_ci);
		if (unlikely(ret == 1))
			return 0;
		++rxq->cq_ci;
		op_own = cqe->op_own;
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
			volatile struct mlx5_mini_cqe8 (*mc)[8] =
				(volatile struct mlx5_mini_cqe8 (*)[8])
				(uintptr_t)(&(*rxq->cqes)[rxq->cq_ci &
							  cqe_cnt].cqe64);

			/* Fix endianness. */
			zip->cqe_cnt = ntohl(cqe->byte_cnt);
			/*
			 * Current mini array position is the one returned by
			 * check_cqe64().
			 *
			 * If completion comprises several mini arrays, as a
			 * special case the second one is located 7 CQEs after
			 * the initial CQE instead of 8 for subsequent ones.
			 */
			zip->ca = rxq->cq_ci & cqe_cnt;
			zip->na = zip->ca + 7;
			/* Compute the next non compressed CQE. */
			--rxq->cq_ci;
			zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
			/* Get packet size to return. */
			len = ntohl((*mc)[0].byte_cnt);
			zip->ai = 1;
		} else {
			len = ntohl(cqe->byte_cnt);
		}
		/* Error while receiving packet. */
		if (unlikely(MLX5_CQE_OPCODE(op_own) == MLX5_CQE_RESP_ERR))
			return -1;
	}
	return len;
}

/**
 * Translate RX completion flags to offload flags.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @return
 *   Offload flags (ol_flags) for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_ol_flags(struct rxq *rxq, volatile struct mlx5_cqe64 *cqe)
{
	uint32_t ol_flags = 0;
	uint8_t l3_hdr = (cqe->l4_hdr_type_etc) & MLX5_CQE_L3_HDR_TYPE_MASK;
	uint8_t l4_hdr = (cqe->l4_hdr_type_etc) & MLX5_CQE_L4_HDR_TYPE_MASK;
	uint8_t info = cqe->rsvd0[0];

	if ((l3_hdr == MLX5_CQE_L3_HDR_TYPE_IPV4) ||
	    (l3_hdr == MLX5_CQE_L3_HDR_TYPE_IPV6))
		ol_flags |=
			(!(cqe->hds_ip_ext & MLX5_CQE_L3_OK) *
			 PKT_RX_IP_CKSUM_BAD);
	if ((l4_hdr == MLX5_CQE_L4_HDR_TYPE_TCP) ||
	    (l4_hdr == MLX5_CQE_L4_HDR_TYPE_TCP_EMP_ACK) ||
	    (l4_hdr == MLX5_CQE_L4_HDR_TYPE_TCP_ACK) ||
	    (l4_hdr == MLX5_CQE_L4_HDR_TYPE_UDP))
		ol_flags |=
			(!(cqe->hds_ip_ext & MLX5_CQE_L4_OK) *
			 PKT_RX_L4_CKSUM_BAD);
	/*
	 * PKT_RX_IP_CKSUM_BAD and PKT_RX_L4_CKSUM_BAD are used in place
	 * of PKT_RX_EIP_CKSUM_BAD because the latter is not functional
	 * (its value is 0).
	 */
	if ((info & IBV_EXP_CQ_RX_TUNNEL_PACKET) && (rxq->csum_l2tun))
		ol_flags |=
			TRANSPOSE(~cqe->l4_hdr_type_etc,
				  IBV_EXP_CQ_RX_OUTER_IP_CSUM_OK,
				  PKT_RX_IP_CKSUM_BAD) |
			TRANSPOSE(~cqe->l4_hdr_type_etc,
				  IBV_EXP_CQ_RX_OUTER_TCP_UDP_CSUM_OK,
				  PKT_RX_L4_CKSUM_BAD);
	return ol_flags;
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
	struct rxq *rxq = dpdk_rxq;
	unsigned int pkts_ret = 0;
	unsigned int i;
	unsigned int rq_ci = rxq->rq_ci;
	const unsigned int elts_n = rxq->elts_n;
	const unsigned int wqe_cnt = elts_n - 1;
	const unsigned int cqe_cnt = rxq->cqe_n - 1;

	for (i = 0; (i != pkts_n); ++i) {
		unsigned int idx = rq_ci & wqe_cnt;
		int len;
		struct rte_mbuf *rep;
		struct rte_mbuf *pkt;
		volatile struct mlx5_wqe_data_seg *wqe = &(*rxq->wqes)[idx];
		volatile struct mlx5_cqe64 *cqe =
			&(*rxq->cqes)[rxq->cq_ci & cqe_cnt].cqe64;

		pkt = (*rxq->elts)[idx];
		rte_prefetch0(cqe);
		rep = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			break;
		}
		SET_DATA_OFF(rep, RTE_PKTMBUF_HEADROOM);
		NB_SEGS(rep) = 1;
		PORT(rep) = rxq->port_id;
		NEXT(rep) = NULL;
		len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt);
		if (unlikely(len == 0)) {
			rte_mbuf_refcnt_set(rep, 0);
			__rte_mbuf_raw_free(rep);
			break;
		}
		if (unlikely(len == -1)) {
			/* RX error, packet is likely too large. */
			rte_mbuf_refcnt_set(rep, 0);
			__rte_mbuf_raw_free(rep);
			++rxq->stats.idropped;
			--i;
			goto skip;
		}
		/*
		 * Fill NIC descriptor with the new buffer.  The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		wqe->addr = htonll((uintptr_t)rep->buf_addr +
				   RTE_PKTMBUF_HEADROOM);
		(*rxq->elts)[idx] = rep;
		/* Update pkt information. */
		if (rxq->csum | rxq->csum_l2tun | rxq->vlan_strip |
		    rxq->crc_present) {
			if (rxq->csum) {
				pkt->packet_type = rxq_cq_to_pkt_type(cqe);
				pkt->ol_flags = rxq_cq_to_ol_flags(rxq, cqe);
			}
			if (cqe->l4_hdr_type_etc & MLX5_CQE_VLAN_STRIPPED) {
				pkt->ol_flags |= PKT_RX_VLAN_PKT |
					PKT_RX_VLAN_STRIPPED;
				pkt->vlan_tci = ntohs(cqe->vlan_info);
			}
			if (rxq->crc_present)
				len -= ETHER_CRC_LEN;
		}
		PKT_LEN(pkt) = len;
		DATA_LEN(pkt) = len;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += len;
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		++pkts_ret;
skip:
		++rq_ci;
	}
	if (unlikely((i == 0) && (rq_ci == rxq->rq_ci)))
		return 0;
	/* Repost WRs. */
#ifdef DEBUG_RECV
	DEBUG("%p: reposting %u WRs", (void *)rxq, i);
#endif
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci;
	rte_wmb();
	*rxq->cq_db = htonl(rxq->cq_ci);
	rte_wmb();
	*rxq->rq_db = htonl(rxq->rq_ci);
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
