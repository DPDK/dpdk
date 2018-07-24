/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#include <rte_bus_vdev.h>
#include <rte_common.h>

#include "zlib_pmd_private.h"

static int
zlib_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct zlib_private), init_params);
	if (dev == NULL) {
		ZLIB_PMD_ERR("driver %s: create failed", init_params->name);
		return -ENODEV;
	}

	return 0;
}

static int
zlib_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id()
	};
	const char *name;
	const char *input_args;
	int retval;

	name = rte_vdev_device_name(vdev);

	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);

	retval = rte_compressdev_pmd_parse_input_args(&init_params, input_args);
	if (retval < 0) {
		ZLIB_PMD_LOG(ERR,
			"Failed to parse initialisation arguments[%s]\n",
			input_args);
		return -EINVAL;
	}

	return zlib_create(name, vdev, &init_params);
}

static int
zlib_remove(struct rte_vdev_device *vdev)
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

static struct rte_vdev_driver zlib_pmd_drv = {
	.probe = zlib_probe,
	.remove = zlib_remove
};

RTE_PMD_REGISTER_VDEV(COMPRESSDEV_NAME_ZLIB_PMD, zlib_pmd_drv);
RTE_INIT(zlib_init_log);

static void
zlib_init_log(void)
{
	zlib_logtype_driver = rte_log_register("pmd.compress.zlib");
	if (zlib_logtype_driver >= 0)
		rte_log_set_level(zlib_logtype_driver, RTE_LOG_INFO);
}
