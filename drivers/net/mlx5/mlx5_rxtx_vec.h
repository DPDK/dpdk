/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_H_
#define RTE_PMD_MLX5_RXTX_VEC_H_

#include <rte_common.h>
#include <rte_mbuf.h>

#include <mlx5_prm.h>

#include "mlx5_autoconf.h"
#include "mlx5_mr.h"

/* HW checksum offload capabilities of vectorized Tx. */
#define MLX5_VEC_TX_CKSUM_OFFLOAD_CAP \
	(DEV_TX_OFFLOAD_IPV4_CKSUM | \
	 DEV_TX_OFFLOAD_UDP_CKSUM | \
	 DEV_TX_OFFLOAD_TCP_CKSUM | \
	 DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM)

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
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, rsvd1) + 11 ==
		  offsetof(struct mlx5_cqe, hdr_type_etc));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, vlan_info) ==
		  offsetof(struct mlx5_cqe, hdr_type_etc) + 2);
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, lro_num_seg) + 12 ==
		  offsetof(struct mlx5_cqe, byte_cnt));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, sop_drop_qpn) ==
		  RTE_ALIGN(offsetof(struct mlx5_cqe, sop_drop_qpn), 8));
S_ASSERT_MLX5_CQE(offsetof(struct mlx5_cqe, op_own) ==
		  offsetof(struct mlx5_cqe, sop_drop_qpn) + 7);

#endif /* RTE_PMD_MLX5_RXTX_VEC_H_ */
