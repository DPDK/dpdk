/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>
#include <sys/mman.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_common_os.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"
#include "mlx5_rxp.h"

#define MLX5_REGEX_MAX_MATCHES MLX5_RXP_MAX_MATCHES
#define MLX5_REGEX_MAX_PAYLOAD_SIZE MLX5_RXP_MAX_JOB_LENGTH
#define MLX5_REGEX_MAX_RULES_PER_GROUP UINT32_MAX
#define MLX5_REGEX_MAX_GROUPS MLX5_RXP_MAX_SUBSETS

#define MLX5_REGEX_RXP_ROF2_LINE_LEN 34

/* Private Declarations */
int
mlx5_regex_info_get(struct rte_regexdev *dev __rte_unused,
		    struct rte_regexdev_info *info)
{
	info->max_matches = MLX5_REGEX_MAX_MATCHES;
	info->max_payload_size = MLX5_REGEX_MAX_PAYLOAD_SIZE;
	info->max_rules_per_group = MLX5_REGEX_MAX_RULES_PER_GROUP;
	info->max_groups = MLX5_REGEX_MAX_GROUPS;
	info->regexdev_capa = RTE_REGEXDEV_SUPP_PCRE_GREEDY_F |
			      RTE_REGEXDEV_CAPA_QUEUE_PAIR_OOS_F;
	info->rule_flags = 0;
	info->max_queue_pairs = UINT16_MAX;
	return 0;
}

static int
rxp_db_setup(struct mlx5_regex_priv *priv)
{
	int ret;
	uint8_t i;

	/* Setup database memories for both RXP engines + reprogram memory. */
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT); i++) {
		priv->db[i].ptr = rte_malloc("", MLX5_MAX_DB_SIZE, 1 << 21);
		if (!priv->db[i].ptr) {
			DRV_LOG(ERR, "Failed to alloc db memory!");
			ret = ENODEV;
			goto tidyup_error;
		}
		/* Register the memory. */
		priv->db[i].umem.umem = mlx5_glue->devx_umem_reg
							(priv->cdev->ctx,
							 priv->db[i].ptr,
							 MLX5_MAX_DB_SIZE, 7);
		if (!priv->db[i].umem.umem) {
			DRV_LOG(ERR, "Failed to register memory!");
			ret = ENODEV;
			goto tidyup_error;
		}
		/* Ensure set all DB memory to 0's before setting up DB. */
		memset(priv->db[i].ptr, 0x00, MLX5_MAX_DB_SIZE);
		/* No data currently in database. */
		priv->db[i].len = 0;
		priv->db[i].active = false;
		priv->db[i].db_assigned_to_eng_num = MLX5_RXP_DB_NOT_ASSIGNED;
	}
	return 0;
tidyup_error:
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT); i++) {
		if (priv->db[i].umem.umem)
			mlx5_glue->devx_umem_dereg(priv->db[i].umem.umem);
		rte_free(priv->db[i].ptr);
		priv->db[i].ptr = NULL;
	}
	return -ret;
}

int
mlx5_regex_rules_db_import(struct rte_regexdev *dev,
		     const char *rule_db, uint32_t rule_db_len)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED) {
		DRV_LOG(ERR, "RXP programming mode not set!");
		return -1;
	}
	if (rule_db == NULL) {
		DRV_LOG(ERR, "Database empty!");
		return -ENODEV;
	}
	if (rule_db_len == 0)
		return -EINVAL;

	return 0;
}

int
mlx5_regex_configure(struct rte_regexdev *dev,
		     const struct rte_regexdev_config *cfg)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	int ret;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED)
		return -1;
	priv->nb_queues = cfg->nb_queue_pairs;
	dev->data->dev_conf.nb_queue_pairs = priv->nb_queues;
	priv->qps = rte_zmalloc(NULL, sizeof(struct mlx5_regex_qp) *
				priv->nb_queues, 0);
	if (!priv->nb_queues) {
		DRV_LOG(ERR, "can't allocate qps memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->nb_max_matches = cfg->nb_max_matches;
	/* Setup rxp db memories. */
	if (rxp_db_setup(priv)) {
		DRV_LOG(ERR, "Failed to setup RXP db memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (cfg->rule_db != NULL) {
		ret = mlx5_regex_rules_db_import(dev, cfg->rule_db,
						 cfg->rule_db_len);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to program rxp rules.");
			rte_errno = ENODEV;
			goto configure_error;
		}
	} else
		DRV_LOG(DEBUG, "Regex config without rules programming!");
	return 0;
configure_error:
	if (priv->qps)
		rte_free(priv->qps);
	return -rte_errno;
}
