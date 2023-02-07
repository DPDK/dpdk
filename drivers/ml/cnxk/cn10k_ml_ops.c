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

struct rte_ml_dev_ops cn10k_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get,
};
