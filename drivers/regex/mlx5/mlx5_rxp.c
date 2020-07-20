/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"

#define MLX5_REGEX_MAX_MATCHES 255
#define MLX5_REGEX_MAX_PAYLOAD_SIZE UINT16_MAX
#define MLX5_REGEX_MAX_RULES_PER_GROUP UINT16_MAX
#define MLX5_REGEX_MAX_GROUPS UINT16_MAX

int
mlx5_regex_info_get(struct rte_regexdev *dev __rte_unused,
		    struct rte_regexdev_info *info)
{
	info->max_matches = MLX5_REGEX_MAX_MATCHES;
	info->max_payload_size = MLX5_REGEX_MAX_PAYLOAD_SIZE;
	info->max_rules_per_group = MLX5_REGEX_MAX_RULES_PER_GROUP;
	info->max_groups = MLX5_REGEX_MAX_GROUPS;
	info->regexdev_capa = RTE_REGEXDEV_SUPP_PCRE_GREEDY_F;
	info->rule_flags = 0;
	return 0;
}

static int
rxp_poll_csr_for_value(struct ibv_context *ctx, uint32_t *value,
		       uint32_t address, uint32_t expected_value,
		       uint32_t expected_mask, uint32_t timeout_ms, uint8_t id)
{
	unsigned int i;
	int ret;

	ret = -EBUSY;
	for (i = 0; i < timeout_ms; i++) {
		if (mlx5_devx_regex_register_read(ctx, id, address, value))
			return -1;

		if ((*value & expected_mask) == expected_value) {
			ret = 0;
			break;
		}
		rte_delay_us(1000);
	}
	return ret;
}

static int
rxp_start_engine(struct ibv_context *ctx, uint8_t id)
{
	uint32_t ctrl;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl |= MLX5_RXP_CSR_CTRL_GO;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	return ret;
}

static int
rxp_stop_engine(struct ibv_context *ctx, uint8_t id)
{
	uint32_t ctrl;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_GO;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	return ret;
}

static int
rxp_init_rtru(struct ibv_context *ctx, uint8_t id, uint32_t init_bits)
{
	uint32_t ctrl_value;
	uint32_t poll_value;
	uint32_t expected_value;
	uint32_t expected_mask;
	int ret = 0;

	/* Read the rtru ctrl CSR. */
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					    &ctrl_value);
	if (ret)
		return -1;
	/* Clear any previous init modes. */
	ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_MASK);
	if (ctrl_value & MLX5_RXP_RTRU_CSR_CTRL_INIT) {
		ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT);
		mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					       ctrl_value);
	}
	/* Set the init_mode bits in the rtru ctrl CSR. */
	ctrl_value |= init_bits;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Need to sleep for a short period after pulsing the rtru init bit. */
	rte_delay_us(20000);
	/* Poll the rtru status CSR until all the init done bits are set. */
	DRV_LOG(DEBUG, "waiting for RXP rule memory to complete init");
	/* Set the init bit in the rtru ctrl CSR. */
	ctrl_value |= MLX5_RXP_RTRU_CSR_CTRL_INIT;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Clear the init bit in the rtru ctrl CSR */
	ctrl_value &= ~MLX5_RXP_RTRU_CSR_CTRL_INIT;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Check that the following bits are set in the RTRU_CSR. */
	if (init_bits == MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_L1_L2) {
		/* Must be incremental mode */
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L2C_INIT_DONE;
	} else {
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_IM_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L2C_INIT_DONE;
	}
	expected_mask = expected_value;
	ret = rxp_poll_csr_for_value(ctx, &poll_value,
				     MLX5_RXP_RTRU_CSR_STATUS,
				     expected_value, expected_mask,
				     MLX5_RXP_CSR_STATUS_TRIAL_TIMEOUT, id);
	if (ret)
		return ret;
	DRV_LOG(DEBUG, "rule memory initialise: 0x%08X", poll_value);
	/* Clear the init bit in the rtru ctrl CSR */
	ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT);
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	return 0;
}

static int
rxp_init(struct mlx5_regex_priv *priv, uint8_t id)
{
	uint32_t ctrl;
	uint32_t reg;
	struct ibv_context *ctx = priv->ctx;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	if (ctrl & MLX5_RXP_CSR_CTRL_INIT) {
		ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
		ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL,
						     ctrl);
		if (ret)
			return ret;
	}
	ctrl |= MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	rte_delay_us(20000);

	ret = rxp_poll_csr_for_value(ctx, &ctrl, MLX5_RXP_CSR_STATUS,
				     MLX5_RXP_CSR_STATUS_INIT_DONE,
				     MLX5_RXP_CSR_STATUS_INIT_DONE,
				     MLX5_RXP_CSR_STATUS_TRIAL_TIMEOUT, id);
	if (ret)
		return ret;
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL,
					     ctrl);
	if (ret)
		return ret;
	rxp_init_rtru(ctx, id, MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_IM_L1_L2);
	ret = rxp_init_rtru(ctx, id, MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_IM_L1_L2);
	if (ret)
		return ret;
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CAPABILITY_5,
					    &reg);
	if (ret)
		return ret;
	DRV_LOG(DEBUG, "max matches: %d, DDOS threshold: %d", reg >> 16,
		reg & 0xffff);
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_MAX_MATCH,
					     priv->nb_max_matches);
	ret |= mlx5_devx_regex_register_write(ctx, id,
					      MLX5_RXP_CSR_MAX_LATENCY, 0);
	ret |= mlx5_devx_regex_register_write(ctx, id,
					      MLX5_RXP_CSR_MAX_PRI_THREAD, 0);
	return ret;
}

int
mlx5_regex_configure(struct rte_regexdev *dev,
		     const struct rte_regexdev_config *cfg)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	int ret;
	uint8_t id;

	priv->nb_queues = cfg->nb_queue_pairs;
	priv->qps = rte_zmalloc(NULL, sizeof(struct mlx5_regex_qp) *
				priv->nb_queues, 0);
	if (!priv->nb_queues) {
		DRV_LOG(ERR, "can't allocate qps memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->nb_max_matches = cfg->nb_max_matches;
	for (id = 0; id < 2; id++) {
		ret = rxp_stop_engine(priv->ctx, id);
		if (ret) {
			DRV_LOG(ERR, "can't stop engine.");
			rte_errno = ENODEV;
			return -rte_errno;
		}
		ret = rxp_init(priv, id);
		if (ret) {
			DRV_LOG(ERR, "can't init engine.");
			rte_errno = ENODEV;
			return -rte_errno;
		}
		ret = mlx5_devx_regex_register_write(priv->ctx, id,
						     MLX5_RXP_CSR_MAX_MATCH,
						     priv->nb_max_matches);
		if (ret) {
			DRV_LOG(ERR, "can't update number of matches.");
			rte_errno = ENODEV;
			goto configure_error;
		}
		ret = rxp_start_engine(priv->ctx, id);
		if (ret) {
			DRV_LOG(ERR, "can't start engine.");
			rte_errno = ENODEV;
			goto configure_error;
		}

	}
	return 0;
configure_error:
	if (priv->qps)
		rte_free(priv->qps);
	return -rte_errno;
}
