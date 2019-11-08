// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2018 Mellanox Technologies, Ltd
 */
#include <math.h>

#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "mlx5.h"
#include "mlx5_flow.h"

/**
 * Callback to get MTR capabilities.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] cap
 *   Pointer to save MTR capabilities.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_mtr_cap_get(struct rte_eth_dev *dev,
		 struct rte_mtr_capabilities *cap,
		 struct rte_mtr_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_qos_attr *qattr = &priv->config.hca_attr.qos;

	if (!priv->mtr_en)
		return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Meter is not support");
	memset(cap, 0, sizeof(*cap));
	cap->n_max = 1 << qattr->log_max_flow_meter;
	cap->n_shared_max = cap->n_max;
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = 4 << 20;
	/* 2M flows can share the same meter. */
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = qattr->srtcm_sup ? cap->n_max : 0;
	cap->meter_rate_max = 1ULL << 40; /* 1 Tera tokens per sec. */
	cap->policer_action_drop_supported = 1;
	cap->stats_mask = RTE_MTR_STATS_N_BYTES_DROPPED |
			  RTE_MTR_STATS_N_PKTS_DROPPED;
	return 0;
}

static const struct rte_mtr_ops mlx5_flow_mtr_ops = {
	.capabilities_get = mlx5_flow_mtr_cap_get,
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
