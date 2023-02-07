/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_ops.h"

static int
cn10k_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info)
{
	if (dev_info == NULL)
		return -EINVAL;

	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));
	dev_info->driver_name = dev->device->driver->name;
	dev_info->max_models = ML_CN10K_MAX_MODELS;
	dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE;
	dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP;
	dev_info->max_segments = ML_CN10K_MAX_SEGMENTS;
	dev_info->min_align_size = ML_CN10K_ALIGN_SIZE;

	return 0;
}

static int
cn10k_ml_dev_configure(struct rte_ml_dev *dev, const struct rte_ml_dev_config *conf)
{
	struct rte_ml_dev_info dev_info;
	struct cn10k_ml_dev *mldev;
	int ret;

	if (dev == NULL || conf == NULL)
		return -EINVAL;

	/* Get CN10K device handle */
	mldev = dev->data->dev_private;

	cn10k_ml_dev_info_get(dev, &dev_info);
	if (conf->nb_models > dev_info.max_models) {
		plt_err("Invalid device config, nb_models > %u\n", dev_info.max_models);
		return -EINVAL;
	}

	if (conf->nb_queue_pairs > dev_info.max_queue_pairs) {
		plt_err("Invalid device config, nb_queue_pairs > %u\n", dev_info.max_queue_pairs);
		return -EINVAL;
	}

	if (mldev->state == ML_CN10K_DEV_STATE_PROBED) {
		plt_ml_dbg("Configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);

		/* Load firmware */
		ret = cn10k_ml_fw_load(mldev);
		if (ret != 0)
			return ret;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CONFIGURED) {
		plt_ml_dbg("Re-configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);
	} else if (mldev->state == ML_CN10K_DEV_STATE_STARTED) {
		plt_err("Device can't be reconfigured in started state\n");
		return -ENOTSUP;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CLOSED) {
		plt_err("Device can't be reconfigured after close\n");
		return -ENOTSUP;
	}

	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;
}

static int
cn10k_ml_dev_close(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;

	if (dev == NULL)
		return -EINVAL;

	mldev = dev->data->dev_private;

	/* Unload firmware */
	cn10k_ml_fw_unload(mldev);

	/* Clear scratch registers */
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_WORK_PTR);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_FW_CTRL);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C1);

	/* Reset ML_MLR_BASE */
	roc_ml_reg_write64(&mldev->roc, 0, ML_MLR_BASE);
	plt_ml_dbg("ML_MLR_BASE = 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_MLR_BASE));

	mldev->state = ML_CN10K_DEV_STATE_CLOSED;

	/* Remove PCI device */
	return rte_dev_remove(dev->device);
}

static int
cn10k_ml_dev_start(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 |= ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_STARTED;

	return 0;
}

static int
cn10k_ml_dev_stop(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 &= ~ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;
}

struct rte_ml_dev_ops cn10k_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get, .dev_configure = cn10k_ml_dev_configure,
	.dev_close = cn10k_ml_dev_close,       .dev_start = cn10k_ml_dev_start,
	.dev_stop = cn10k_ml_dev_stop,
};
