/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2024-2025 Linaro ltd.
 */

#include <bus_vdev_driver.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>

#include <uadk/wd_comp.h>
#include <uadk/wd_sched.h>

#include "uadk_compress_pmd_private.h"

static struct rte_compressdev_ops uadk_compress_pmd_ops;

static int
uadk_compress_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};
	struct rte_compressdev *compressdev;
	struct uacce_dev *udev;
	const char *name;

	udev = wd_get_accel_dev("deflate");
	if (!udev)
		return -ENODEV;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct uadk_compress_priv), &init_params);
	if (compressdev == NULL) {
		UADK_LOG(ERR, "driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	compressdev->dev_ops = &uadk_compress_pmd_ops;
	compressdev->dequeue_burst = NULL;
	compressdev->enqueue_burst = NULL;
	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	return 0;
}

static int
uadk_compress_remove(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compressdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_get_named_dev(name);
	if (compressdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_vdev_driver uadk_compress_pmd = {
	.probe = uadk_compress_probe,
	.remove = uadk_compress_remove,
};

#define UADK_COMPRESS_DRIVER_NAME compress_uadk
RTE_PMD_REGISTER_VDEV(UADK_COMPRESS_DRIVER_NAME, uadk_compress_pmd);
RTE_LOG_REGISTER_DEFAULT(uadk_compress_logtype, INFO);
