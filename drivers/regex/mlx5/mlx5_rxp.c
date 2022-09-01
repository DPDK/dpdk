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
#include "mlx5_rxp.h"

#define MLX5_REGEX_MAX_MATCHES MLX5_RXP_MAX_MATCHES
#define MLX5_REGEX_MAX_PAYLOAD_SIZE MLX5_RXP_MAX_JOB_LENGTH
#define MLX5_REGEX_MAX_RULES_PER_GROUP UINT32_MAX
#define MLX5_REGEX_MAX_GROUPS MLX5_RXP_MAX_SUBSETS

#define MLX5_REGEX_RXP_ROF2_LINE_LEN 34

const uint64_t combined_rof_tag = 0xff52544424a52475;

/* Private Declarations */
static int
rxp_create_mkey(struct mlx5_regex_priv *priv, void *ptr, size_t size,
	uint32_t access, struct mlx5_regex_mkey *mkey);
static inline void
rxp_destroy_mkey(struct mlx5_regex_mkey *mkey);

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
	info->max_segs = mlx5_regexdev_max_segs_get();
	return 0;
}

static int
rxp_create_mkey(struct mlx5_regex_priv *priv, void *ptr, size_t size,
	uint32_t access, struct mlx5_regex_mkey *mkey)
{
	struct mlx5_devx_mkey_attr mkey_attr;

	/* Register the memory. */
	mkey->umem = mlx5_glue->devx_umem_reg(priv->cdev->ctx, ptr, size, access);
	if (!mkey->umem) {
		DRV_LOG(ERR, "Failed to register memory!");
		return -ENODEV;
	}
	/* Create mkey */
	mkey_attr = (struct mlx5_devx_mkey_attr) {
		.addr = (uintptr_t)ptr,
		.size = (uint32_t)size,
		.umem_id = mlx5_os_get_umem_id(mkey->umem),
		.pg_access = 1,
		.umr_en = 0,
	};
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	mkey_attr.pd = priv->cdev->pdn;
#endif
	mkey->mkey = mlx5_devx_cmd_mkey_create(priv->cdev->ctx, &mkey_attr);
	if (!mkey->mkey) {
		DRV_LOG(ERR, "Failed to create direct mkey!");
		return -ENODEV;
	}
	return 0;
}

static inline void
rxp_destroy_mkey(struct mlx5_regex_mkey *mkey)
{
	if (mkey->mkey)
		claim_zero(mlx5_devx_cmd_destroy(mkey->mkey));
	if (mkey->umem)
		claim_zero(mlx5_glue->devx_umem_dereg(mkey->umem));
}

int
mlx5_regex_get_rxp_vers(uint32_t regexp_version, uint32_t *target_rxp_vers)
{
	int ret = 0;
	switch (regexp_version) {
	case MLX5_RXP_BF2_IDENTIFIER:
		*target_rxp_vers = MLX5_RXP_BF2_ROF_VERSION_STRING;
		break;
	case MLX5_RXP_BF3_IDENTIFIER:
		*target_rxp_vers = MLX5_RXP_BF3_ROF_VERSION_STRING;
		break;
	default:
		DRV_LOG(ERR, "Unsupported rxp version: %u", regexp_version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int
mlx5_regex_check_rof_version(uint32_t combined_rof_vers)
{
	int ret = 0;
	/* Check if combined rof version is supported */
	switch (combined_rof_vers) {
	case 1:
		break;
	default:
		DRV_LOG(ERR, "Unsupported combined rof version: %u",
		combined_rof_vers);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int
mlx5_regex_parse_rules_db(struct mlx5_regex_priv *priv,
			const char **rules_db, uint32_t *rules_db_len)
{
	int i = 0;
	uint32_t j = 0;
	int ret = 0;
	bool combined_rof = true;
	const char *rof_ptr = *rules_db;
	uint32_t combined_rof_vers = 0;
	uint32_t num_rof_blocks = 0;
	uint32_t rxpc_vers = 0;
	uint32_t target_rxp_vers = 0;
	uint32_t byte_count = 0;
	uint32_t rof_bytes_read = 0;
	bool rof_binary_found = false;
	struct mlx5_hca_attr *attr = &priv->cdev->config.hca_attr;

	/* Need minimum of 8 bytes to process single or combined rof */
	if (*rules_db_len < 8)
		return -EINVAL;

	for (i = 0; i < 8; i++) {
		if ((char) *rof_ptr !=
			(char)((combined_rof_tag >> (i * 8)) & 0xFF)) {
			combined_rof = false;
			break;
		}
		rof_ptr++;
	}
	rof_bytes_read += 8;

	if (combined_rof == true) {
		/* Need at least 24 bytes of header info: 16 byte combined */
		/* rof header and 8 byte binary rof blob header.           */
		if (*rules_db_len < 24)
			return -EINVAL;

		/* Read the combined rof version and number of rof blocks */
		for (i = 0; i < 4; i++) {
			combined_rof_vers |= *rof_ptr << (i * 8);
			rof_ptr++;
		}

		rof_bytes_read += 4;
		ret = mlx5_regex_check_rof_version(combined_rof_vers);
		if (ret < 0)
			return ret;

		for (i = 0; i < 4; i++) {
			num_rof_blocks |= *rof_ptr << (i * 8);
			rof_ptr++;
		}
		rof_bytes_read += 4;

		if (num_rof_blocks == 0)
			return -EINVAL;

		/* Get the version of rxp we need the rof for */
		ret = mlx5_regex_get_rxp_vers(attr->regexp_version, &target_rxp_vers);
		if (ret < 0)
			return ret;

		/* Try to find the rof binary blob for this version of rxp */
		for (j = 0; j < num_rof_blocks; j++) {
			rxpc_vers = 0;
			byte_count = 0;
			for (i = 0; i < 4; i++) {
				rxpc_vers |= (*rof_ptr & 0xFF) << (i * 8);
				rof_ptr++;
			}
			for (i = 0; i < 4; i++) {
				byte_count |= (*rof_ptr & 0xFF) << (i * 8);
				rof_ptr++;
			}
			rof_bytes_read += 8;

			if (rxpc_vers == target_rxp_vers) {
				/* Found corresponding binary rof entry */
				if (rof_bytes_read + byte_count <= (*rules_db_len))
					rof_binary_found = true;
				else
					DRV_LOG(ERR, "Compatible rof file found - invalid length!");
				break;
			}
				/* Move on to next rof blob */
			if (rof_bytes_read + byte_count + 8 < (*rules_db_len)) {
				rof_ptr += byte_count;
				rof_bytes_read += byte_count;
			} else {
				/* Cannot parse any more of combined rof file */
				break;
			}
		}
		if (rof_binary_found == true) {
			*rules_db = rof_ptr;
			*rules_db_len = byte_count;
		} else {
			DRV_LOG(ERR, "Compatible rof file not found!");
			return -EINVAL;
		}
	}
	return 0;
}

int
mlx5_regex_rules_db_import(struct rte_regexdev *dev,
		     const char *rule_db, uint32_t rule_db_len)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_mkey mkey;
	uint32_t id;
	int ret;
	void *ptr;

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

	ret = mlx5_regex_parse_rules_db(priv, &rule_db, &rule_db_len);
	if (ret < 0)
		return ret;

	/* copy rules - rules have to be 4KB aligned. */
	ptr = rte_malloc("", rule_db_len, 1 << 12);
	if (!ptr) {
		DRV_LOG(ERR, "Failed to allocate rules file memory.");
		return -ENOMEM;
	}
	rte_memcpy(ptr, rule_db, rule_db_len);
	/* Register umem and create rof mkey. */
	ret = rxp_create_mkey(priv, ptr, rule_db_len, /*access=*/7, &mkey);
	if (ret < 0)
		return ret;

	for (id = 0; id < priv->nb_engines; id++) {
		ret = mlx5_devx_regex_rules_program(priv->cdev->ctx, id,
			mkey.mkey->id, rule_db_len, (uintptr_t)ptr);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to program rxp rules.");
			ret = -ENODEV;
			break;
		}
		ret = 0;
	}
	rxp_destroy_mkey(&mkey);
	rte_free(ptr);
	return ret;
}

int
mlx5_regex_configure(struct rte_regexdev *dev,
		     const struct rte_regexdev_config *cfg)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	int ret;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED)
		return -1;
	if (cfg->nb_max_matches != MLX5_REGEX_MAX_MATCHES) {
		DRV_LOG(ERR, "nb_max_matches is not configurable.");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	priv->nb_queues = cfg->nb_queue_pairs;
	dev->data->dev_conf.nb_queue_pairs = priv->nb_queues;
	priv->qps = rte_zmalloc(NULL, sizeof(struct mlx5_regex_qp) *
				priv->nb_queues, 0);
	if (!priv->qps) {
		DRV_LOG(ERR, "can't allocate qps memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->nb_max_matches = cfg->nb_max_matches;
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
	rte_free(priv->qps);
	return -rte_errno;
}
