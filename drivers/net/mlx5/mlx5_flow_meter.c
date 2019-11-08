// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2018 Mellanox Technologies, Ltd
 */
#include <math.h>

#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "mlx5.h"

static const struct rte_mtr_ops mlx5_flow_mtr_ops = {
	.capabilities_get = NULL,
	.meter_profile_add = NULL,
	.meter_profile_delete = NULL,
	.create = NULL,
	.destroy = NULL,
	.meter_enable = NULL,
	.meter_disable = NULL,
	.meter_profile_update = NULL,
	.meter_dscp_table_update = NULL,
	.policer_actions_update = NULL,
	.stats_update = NULL,
	.stats_read = NULL,
};

/**
 * Get meter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param arg
 *   Pointer to set the mtr operations.
 *
 * @return
 *   Always 0.
 */
int
mlx5_flow_meter_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	*(const struct rte_mtr_ops **)arg = &mlx5_flow_mtr_ops;
	return 0;
}
