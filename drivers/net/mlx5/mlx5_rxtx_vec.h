/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_H_
#define RTE_PMD_MLX5_RXTX_VEC_H_

#include <rte_common.h>
#include <rte_mbuf.h>

#include "mlx5_autoconf.h"
#include "mlx5_prm.h"

/* HW checksum offload capabilities of vectorized Tx. */
#define MLX5_VEC_TX_CKSUM_OFFLOAD_CAP \
	(DEV_TX_OFFLOAD_IPV4_CKSUM | \
	 DEV_TX_OFFLOAD_UDP_CKSUM | \
	 DEV_TX_OFFLOAD_TCP_CKSUM | \
	 DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM)

/* HW offload capabilities of vectorized Tx. */
#define MLX5_VEC_TX_OFFLOAD_CAP \
	(MLX5_VEC_TX_CKSUM_OFFLOAD_CAP | \
	 DEV_TX_OFFLOAD_MATCH_METADATA | \
	 DEV_TX_OFFLOAD_MULTI_SEGS)

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
	volatile struct mlx5_wqe_data_seg *wq =
		&((volatile struct mlx5_wqe_data_seg *)rxq->wqes)[elts_idx];
	unsigned int i;

	assert(n >= MLX5_VPMD_RXQ_RPLNSH_THRESH(q_n));
	assert(n <= (uint16_t)(q_n - (rxq->rq_ci - rxq->rq_pi)));
	assert(MLX5_VPMD_RXQ_RPLNSH_THRESH(q_n) > MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	n = RTE_MIN(n - MLX5_VPMD_DESCS_PER_LOOP, q_n - elts_idx);
	if (rte_mempool_get_bulk(rxq->mp, (void *)elts, n) < 0) {
		rxq->stats.rx_nombuf += n;
		return;
	}
	for (i = 0; i < n; ++i) {
		void *buf_addr;

		/*
		 * Load the virtual address for Rx WQE. non-x86 processors
		 * (mostly RISC such as ARM and Power) are more vulnerable to
		 * load stall. For x86, reducing the number of instructions
		 * seems to matter most.
		 */
#ifdef RTE_ARCH_X86_64
		buf_addr = elts[i]->buf_addr;
		assert(buf_addr == rte_mbuf_buf_addr(elts[i], rxq->mp));
#else
		buf_addr = rte_mbuf_buf_addr(elts[i], rxq->mp);
		assert(buf_addr == elts[i]->buf_addr);
#endif
		wq[i].addr = rte_cpu_to_be_64((uintptr_t)buf_addr +
					      RTE_PKTMBUF_HEADROOM);
		/* If there's only one MR, no need to replace LKey in WQE. */
		if (unlikely(mlx5_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
			wq[i].lkey = mlx5_rx_mb2mr(rxq, elts[i]);
	}
	rxq->rq_ci += n;
	/* Prevent overflowing into consumed mbufs. */
	elts_idx = rxq->rq_ci & q_mask;
	for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
		(*rxq->elts)[elts_idx + i] = &rxq->fake_mbuf;
	rte_cio_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_H_ */
