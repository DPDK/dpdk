/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#include <errno.h>

#include <rte_common.h>

#include "mlx5_lsc_intr.h"

int
mlx5_lsc_intr_setup(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

void
mlx5_lsc_intr_cleanup(struct rte_eth_dev *dev __rte_unused)
{
}
