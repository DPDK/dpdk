/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_bus_vdev.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_compressdev_pmd.h>

#include "isal_compress_pmd_private.h"

int isal_logtype_driver;

/* Create ISA-L compression device */
static int
compdev_isal_create(const char *name, struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct isal_comp_private), init_params);
	if (dev == NULL) {
		ISAL_PMD_LOG(ERR, "failed to create compressdev vdev");
		return -EFAULT;
	}

	dev->dev_ops = isal_compress_pmd_ops;

	return 0;
}

/** Remove compression device */
static int
compdev_isal_remove_dev(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compdev = rte_compressdev_pmd_get_named_dev(name);
	if (compdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compdev);
}

/** Initialise ISA-L compression device */
static int
compdev_isal_probe(struct rte_vdev_device *dev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};
	const char *name, *args;
	int retval;

	name = rte_vdev_device_name(dev);
	if (name == NULL)
		return -EINVAL;

	args = rte_vdev_device_args(dev);

	retval = rte_compressdev_pmd_parse_input_args(&init_params, args);
	if (retval) {
		ISAL_PMD_LOG(ERR,
			"Failed to parse initialisation arguments[%s]\n", args);
		return -EINVAL;
	}

	return compdev_isal_create(name, dev, &init_params);
}

static struct rte_vdev_driver compdev_isal_pmd_drv = {
	.probe = compdev_isal_probe,
	.remove = compdev_isal_remove_dev,
};

RTE_PMD_REGISTER_VDEV(COMPDEV_NAME_ISAL_PMD, compdev_isal_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(COMPDEV_NAME_ISAL_PMD,
	"socket_id=<int>");

RTE_INIT(isal_init_log);

static void
isal_init_log(void)
{
	isal_logtype_driver = rte_log_register("comp_isal");
	if (isal_logtype_driver >= 0)
		rte_log_set_level(isal_logtype_driver, RTE_LOG_INFO);
}
