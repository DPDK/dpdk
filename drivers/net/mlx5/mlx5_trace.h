/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_PMD_MLX5_TRACE_H_
#define RTE_PMD_MLX5_TRACE_H_

/**
 * @file
 *
 * API for mlx5 PMD trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <mlx5_prm.h>
#include <rte_mbuf.h>
#include <rte_trace_point.h>

/* TX burst subroutines trace points. */
RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_entry,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
)

RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_exit,
	RTE_TRACE_POINT_ARGS(uint16_t nb_sent, uint16_t nb_req),
	rte_trace_point_emit_u16(nb_sent);
	rte_trace_point_emit_u16(nb_req);
)

RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_wqe,
	RTE_TRACE_POINT_ARGS(uint32_t opcode),
	rte_trace_point_emit_u32(opcode);
)

RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_wait,
	RTE_TRACE_POINT_ARGS(uint64_t ts),
	rte_trace_point_emit_u64(ts);
)


RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_push,
	RTE_TRACE_POINT_ARGS(const struct rte_mbuf *mbuf, uint16_t wqe_id),
	rte_trace_point_emit_ptr(mbuf);
	rte_trace_point_emit_u32(mbuf->pkt_len);
	rte_trace_point_emit_u16(mbuf->nb_segs);
	rte_trace_point_emit_u16(wqe_id);
)

RTE_TRACE_POINT_FP(
	rte_pmd_mlx5_trace_tx_complete,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id,
			     uint16_t wqe_id, uint64_t ts),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_u64(ts);
	rte_trace_point_emit_u16(wqe_id);
)

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMD_MLX5_TRACE_H_ */
