/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include "mlx5_tx.h"

/*
 * Generate routines with Legacy Multi-Packet Write support.
 * This mode is supported by ConnectX-4 Lx only and imposes
 * offload limitations, not supported:
 *   - ACL/Flows (metadata are becoming meaningless)
 *   - WQE Inline headers
 *   - SRIOV (E-Switch offloads)
 *   - VLAN insertion
 *   - tunnel encapsulation/decapsulation
 *   - TSO
 */
MLX5_TXOFF_DECL(none_mpw,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(mci_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(mc_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_EMPW | MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(i_mpw,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)
