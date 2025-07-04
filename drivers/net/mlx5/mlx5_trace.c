/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include <rte_trace_point_register.h>
#include <mlx5_trace.h>

/* TX burst subroutines trace points. */
RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_entry,
	pmd.net.mlx5.tx.entry)

RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_exit,
	pmd.net.mlx5.tx.exit)

RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_wqe,
	pmd.net.mlx5.tx.wqe)

RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_wait,
	pmd.net.mlx5.tx.wait)

RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_push,
	pmd.net.mlx5.tx.push)

RTE_TRACE_POINT_REGISTER(rte_pmd_mlx5_trace_tx_complete,
	pmd.net.mlx5.tx.complete)
