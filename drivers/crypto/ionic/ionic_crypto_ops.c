/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>

#include "ionic_crypto.h"

static int
iocpt_op_config(struct rte_cryptodev *cdev,
		struct rte_cryptodev_config *config __rte_unused)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	iocpt_configure(dev);

	return 0;
}

static int
iocpt_op_close(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	iocpt_deinit(dev);

	return 0;
}

static void
iocpt_op_info_get(struct rte_cryptodev *cdev, struct rte_cryptodev_info *info)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	if (info == NULL)
		return;

	info->max_nb_queue_pairs = dev->max_qps;
	info->feature_flags = dev->features;
	info->capabilities = iocpt_get_caps(info->feature_flags);
	info->sym.max_nb_sessions = dev->max_sessions;
	info->driver_id = dev->driver_id;
	info->min_mbuf_headroom_req = 0;
	info->min_mbuf_tailroom_req = 0;
}

static struct rte_cryptodev_ops iocpt_ops = {
	.dev_configure = iocpt_op_config,
	.dev_close = iocpt_op_close,
	.dev_infos_get = iocpt_op_info_get,
};

int
iocpt_assign_ops(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	cdev->dev_ops = &iocpt_ops;
	cdev->feature_flags = dev->features;

	return 0;
}
