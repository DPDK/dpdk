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
		.mod_fields_bitmap =
			MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_DUMP_ENABLE,
		.dirty_bitmap_dump_enable = enable,
	};
	struct mlx5_vdpa_virtq *virtq;
	int i;

	for (i = 0; i < priv->nr_virtqs; ++i) {
		attr.queue_index = i;
		virtq = &priv->virtqs[i];
		if (!virtq->configured) {
			DRV_LOG(DEBUG, "virtq %d is invalid for dirty bitmap enabling.", i);
		} else {
			struct mlx5_vdpa_virtq *virtq = &priv->virtqs[i];

			pthread_mutex_lock(&virtq->virtq_lock);
			if (mlx5_devx_cmd_modify_virtq(priv->virtqs[i].virtq,
			   &attr)) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				DRV_LOG(ERR,
					"Failed to modify virtq %d for dirty bitmap enabling.",
					i);
				return -1;
			}
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	}
	return 0;
}

int
mlx5_vdpa_dirty_bitmap_set(struct mlx5_vdpa_priv *priv, uint64_t log_base,
			   uint64_t log_size)
{
	struct mlx5_devx_virtq_attr attr = {
		.mod_fields_bitmap = MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_PARAMS,
		.dirty_bitmap_addr = log_base,
		.dirty_bitmap_size = log_size,
	};
	struct mlx5_vdpa_virtq *virtq;
	int i;
	int ret = mlx5_os_wrapped_mkey_create(priv->cdev->ctx, priv->cdev->pd,
					      priv->cdev->pdn,
					      (void *)(uintptr_t)log_base,
					      log_size, &priv->lm_mr);

	if (ret) {
		DRV_LOG(ERR, "Failed to allocate wrapped MR for lm.");
		return -1;
	}
	attr.dirty_bitmap_mkey = priv->lm_mr.lkey;
	for (i = 0; i < priv->nr_virtqs; ++i) {
		attr.queue_index = i;
		virtq = &priv->virtqs[i];
		if (!virtq->configured) {
			DRV_LOG(DEBUG, "virtq %d is invalid for LM.", i);
		} else {
			struct mlx5_vdpa_virtq *virtq = &priv->virtqs[i];

			pthread_mutex_lock(&virtq->virtq_lock);
			if (mlx5_devx_cmd_modify_virtq(
					priv->virtqs[i].virtq,
					&attr)) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				DRV_LOG(ERR,
				"Failed to modify virtq %d for LM.", i);
				goto err;
			}
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	}
	return 0;
err:
	mlx5_os_wrapped_mkey_destroy(&priv->lm_mr);
	return -1;
}

int
mlx5_vdpa_lm_log(struct mlx5_vdpa_priv *priv)
{
	uint32_t remaining_cnt = 0, err_cnt = 0, task_num = 0;
	uint32_t i, thrd_idx, data[1];
	struct mlx5_vdpa_virtq *virtq;
	uint64_t features;
	int ret;

	ret = rte_vhost_get_negotiated_features(priv->vid, &features);
	if (ret) {
		DRV_LOG(ERR, "Failed to get negotiated features.");
		return -1;
	}
	if (priv->use_c_thread && priv->nr_virtqs) {
		uint32_t main_task_idx[priv->nr_virtqs];

		for (i = 0; i < priv->nr_virtqs; i++) {
			virtq = &priv->virtqs[i];
			if (!virtq->configured)
				continue;
			thrd_idx = i % (conf_thread_mng.max_thrds + 1);
			if (!thrd_idx) {
				main_task_idx[task_num] = i;
				task_num++;
				continue;
			}
			thrd_idx = priv->last_c_thrd_idx + 1;
			if (thrd_idx >= conf_thread_mng.max_thrds)
				thrd_idx = 0;
			priv->last_c_thrd_idx = thrd_idx;
			data[0] = i;
			if (mlx5_vdpa_task_add(priv, thrd_idx,
				MLX5_VDPA_TASK_STOP_VIRTQ,
				&remaining_cnt, &err_cnt,
				(void **)&data, 1)) {
				DRV_LOG(ERR, "Fail to add "
					"task stop virtq (%d).", i);
				main_task_idx[task_num] = i;
				task_num++;
			}
		}
		for (i = 0; i < task_num; i++) {
			virtq = &priv->virtqs[main_task_idx[i]];
			pthread_mutex_lock(&virtq->virtq_lock);
			ret = mlx5_vdpa_virtq_stop(priv,
					main_task_idx[i]);
			if (ret) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				DRV_LOG(ERR,
				"Failed to stop virtq %d.", i);
				return -1;
			}
			if (RTE_VHOST_NEED_LOG(features))
				rte_vhost_log_used_vring(priv->vid, i, 0,
				MLX5_VDPA_USED_RING_LEN(virtq->vq_size));
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
		if (mlx5_vdpa_c_thread_wait_bulk_tasks_done(&remaining_cnt,
			&err_cnt, 2000)) {
			DRV_LOG(ERR,
			"Failed to wait virt-queue setup tasks ready.");
			return -1;
		}
	} else {
		for (i = 0; i < priv->nr_virtqs; i++) {
			virtq = &priv->virtqs[i];
			pthread_mutex_lock(&virtq->virtq_lock);
			if (!virtq->configured) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				continue;
			}
			ret = mlx5_vdpa_virtq_stop(priv, i);
			if (ret) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				DRV_LOG(ERR,
				"Failed to stop virtq %d for LM log.", i);
				return -1;
			}
			if (RTE_VHOST_NEED_LOG(features))
				rte_vhost_log_used_vring(priv->vid, i, 0,
				MLX5_VDPA_USED_RING_LEN(virtq->vq_size));
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	}
	return 0;
}
