/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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

#ifndef RTE_PMD_MLX5_RXTX_VEC_H_
#define RTE_PMD_MLX5_RXTX_VEC_H_

#include <rte_common.h>
#include <rte_mbuf.h>

#include "mlx5_autoconf.h"
#include "mlx5_prm.h"

/*
 * Compile time sanity check for vectorized functions.
 */

#define S_ASSERT_RTE_MBUF(s) \
	static_assert(s, "A field of struct rte_mbuf is changed")
#define S_ASSERT_MLX5_CQE(s) \
	static_assert(s, "A field of struct mlx5_cqe is changed")

/* rxq_cq_decompress_v() */
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, pkt_len) ==
		  offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, data_len) ==
		  offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, hash) ==
		  offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

/* rxq_cq_to_ptype_oflags_v() */
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, ol_flags) ==
		  offsetof(struct rte_mbuf, rearm_data) + 8);
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, rearm_data) ==
		  RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));

/* rxq_burst_v() */
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, pkt_len) ==
		  offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
S_ASSERT_RTE_MBUF(offsetof(struct rte_mbuf, data_len) ==
		  offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
#if (RTE_CACHE_LINE_SIZE == 128)
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, pkt_info) == 64);
#else
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, pkt_info) == 0);
#endif
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, rx_hash_res) ==
		  offsetof(struct mlx5_cqe, pkt_info) + 12);
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, rsvd1) +
		  sizeof(((struct mlx5_cqe *)0)->rsvd1) ==
		  offsetof(struct mlx5_cqe, hdr_type_etc));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, vlan_info) ==
		  offsetof(struct mlx5_cqe, hdr_type_etc) + 2);
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, rsvd2) +
		  sizeof(((struct mlx5_cqe *)0)->rsvd2) ==
		  offsetof(struct mlx5_cqe, byte_cnt));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, sop_drop_qpn) ==
		  RTE_ALIGN(offsetof(struct mlx5_cqe, sop_drop_qpn), 8));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, op_own) ==
		  offsetof(struct mlx5_cqe, sop_drop_qpn) + 7);

/**
 * Replenish buffers for RX in bulk.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param n
 *   Number of buffers to be replenished.
 */
static inline void
mlx5_rx_replenish_bulk_mbuf(struct mlx5_rxq_data *rxq, uint16_t n)
{
	const uint16_t q_n = 1 << rxq->elts_n;
	const uint16_t q_mask = q_n - 1;
	uint16_t elts_idx = rxq->rq_ci & q_mask;
	struct rte_mbuf **elts = &(*rxq->elts)[elts_idx];
	volatile struct mlx5_wqe_data_seg *wq = &(*rxq->wqes)[elts_idx];
	unsigned int i;

	assert(n >= MLX5_VPMD_RXQ_RPLNSH_THRESH);
	assert(n <= (uint16_t)(q_n - (rxq->rq_ci - rxq->rq_pi)));
	assert(MLX5_VPMD_RXQ_RPLNSH_THRESH > MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	n = RTE_MIN(n - MLX5_VPMD_DESCS_PER_LOOP, q_n - elts_idx);
	if (rte_mempool_get_bulk(rxq->mp, (void *)elts, n) < 0) {
		rxq->stats.rx_nombuf += n;
		return;
	}
	for (i = 0; i < n; ++i)
		wq[i].addr = rte_cpu_to_be_64((uintptr_t)elts[i]->buf_addr +
					      RTE_PKTMBUF_HEADROOM);
	rxq->rq_ci += n;
	/* Prevent overflowing into consumed mbufs. */
	elts_idx = rxq->rq_ci & q_mask;
	for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
		(*rxq->elts)[elts_idx + i] = &rxq->fake_mbuf;
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_H_ */
