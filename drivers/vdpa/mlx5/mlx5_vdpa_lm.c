/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <rte_malloc.h>
#include <rte_errno.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


int
mlx5_vdpa_logging_enable(struct mlx5_vdpa_priv *priv, int enable)
{
	struct mlx5_devx_virtq_attr attr = {
		.type = MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_DUMP_ENABLE,
		.dirty_bitmap_dump_enable = enable,
	};
	int i;

	for (i = 0; i < priv->nr_virtqs; ++i) {
		attr.queue_index = i;
		if (!priv->virtqs[i].virtq) {
			DRV_LOG(DEBUG, "virtq %d is invalid for dirty bitmap "
				"enabling.", i);
		} else if (mlx5_devx_cmd_modify_virtq(priv->virtqs[i].virtq,
			   &attr)) {
			DRV_LOG(ERR, "Failed to modify virtq %d for dirty "
				"bitmap enabling.", i);
			return -1;
		}
	}
	return 0;
}

int
mlx5_vdpa_dirty_bitmap_set(struct mlx5_vdpa_priv *priv, uint64_t log_base,
			   uint64_t log_size)
{
	struct mlx5_devx_virtq_attr attr = {
		.type = MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_PARAMS,
		.dirty_bitmap_addr = log_base,
		.dirty_bitmap_size = log_size,
	};
	int i;
	int ret = mlx5_os_wrapped_mkey_create(priv->ctx, priv->pd,
					      priv->pdn,
					      (void *)(uintptr_t)log_base,
					      log_size, &priv->lm_mr);

	if (ret) {
		DRV_LOG(ERR, "Failed to allocate wrapped MR for lm.");
		return -1;
	}
	attr.dirty_bitmap_mkey = priv->lm_mr.lkey;
	for (i = 0; i < priv->nr_virtqs; ++i) {
		attr.queue_index = i;
		if (!priv->virtqs[i].virtq) {
			DRV_LOG(DEBUG, "virtq %d is invalid for LM.", i);
		} else if (mlx5_devx_cmd_modify_virtq(priv->virtqs[i].virtq,
						      &attr)) {
			DRV_LOG(ERR, "Failed to modify virtq %d for LM.", i);
			goto err;
		}
	}
	return 0;
err:
	mlx5_os_wrapped_mkey_destroy(&priv->lm_mr);
	return -1;
}

#define MLX5_VDPA_USED_RING_LEN(size) \
	((size) * sizeof(struct vring_used_elem) + sizeof(uint16_t) * 3)

int
mlx5_vdpa_lm_log(struct mlx5_vdpa_priv *priv)
{
	uint64_t features;
	int ret = rte_vhost_get_negotiated_features(priv->vid, &features);
	int i;

	if (ret) {
		DRV_LOG(ERR, "Failed to get negotiated features.");
		return -1;
	}
	if (!RTE_VHOST_NEED_LOG(features))
		return 0;
	for (i = 0; i < priv->nr_virtqs; ++i) {
		if (!priv->virtqs[i].virtq) {
			DRV_LOG(DEBUG, "virtq %d is invalid for LM log.", i);
		} else {
			ret = mlx5_vdpa_virtq_stop(priv, i);
			if (ret) {
				DRV_LOG(ERR, "Failed to stop virtq %d for LM "
					"log.", i);
				return -1;
			}
		}
		rte_vhost_log_used_vring(priv->vid, i, 0,
			      MLX5_VDPA_USED_RING_LEN(priv->virtqs[i].vq_size));
	}
	return 0;
}
