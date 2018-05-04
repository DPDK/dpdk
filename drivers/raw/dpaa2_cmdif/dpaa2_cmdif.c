/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_bus_vdev.h>
#include <rte_atomic.h>
#include <rte_interrupts.h>
#include <rte_branch_prediction.h>
#include <rte_lcore.h>

#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>
#include "dpaa2_cmdif_logs.h"

/* Dynamic log type identifier */
int dpaa2_cmdif_logtype;

/* CMDIF driver name */
#define DPAA2_CMDIF_PMD_NAME dpaa2_dpci

/* CMDIF driver object */
static struct rte_vdev_driver dpaa2_cmdif_drv;

/*
 * This API provides the DPCI device ID in 'attr_value'.
 * The device ID shall be passed by GPP to the AIOP using CMDIF commands.
 */
static int
dpaa2_cmdif_get_attr(struct rte_rawdev *dev,
		     const char *attr_name,
		     uint64_t *attr_value)
{
	struct dpaa2_dpci_dev *cidev = dev->dev_private;

	DPAA2_CMDIF_FUNC_TRACE();

	RTE_SET_USED(attr_name);

	if (!attr_value) {
		DPAA2_CMDIF_ERR("Invalid arguments for getting attributes");
		return -EINVAL;
	}
	*attr_value = cidev->dpci_id;

	return 0;
}

static const struct rte_rawdev_ops dpaa2_cmdif_ops = {
	.attr_get = dpaa2_cmdif_get_attr,
};

static int
dpaa2_cmdif_create(const char *name,
		   struct rte_vdev_device *vdev,
		   int socket_id)
{
	struct rte_rawdev *rawdev;
	struct dpaa2_dpci_dev *cidev;

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct dpaa2_dpci_dev),
					 socket_id);
	if (!rawdev) {
		DPAA2_CMDIF_ERR("Unable to allocate rawdevice");
		return -EINVAL;
	}

	rawdev->dev_ops = &dpaa2_cmdif_ops;
	rawdev->device = &vdev->device;
	rawdev->driver_name = vdev->device.driver->name;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	cidev = rte_dpaa2_alloc_dpci_dev();
	if (!cidev) {
		DPAA2_CMDIF_ERR("Unable to allocate CI device");
		rte_rawdev_pmd_release(rawdev);
		return -ENODEV;
	}

	rawdev->dev_private = cidev;

	return 0;
}

static int
dpaa2_cmdif_destroy(const char *name)
{
	int ret;
	struct rte_rawdev *rdev;

	rdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rdev) {
		DPAA2_CMDIF_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	/* The primary process will only free the DPCI device */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_dpaa2_free_dpci_dev(rdev->dev_private);

	ret = rte_rawdev_pmd_release(rdev);
	if (ret)
		DPAA2_CMDIF_DEBUG("Device cleanup failed");

	return 0;
}

static int
dpaa2_cmdif_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	int ret = 0;

	name = rte_vdev_device_name(vdev);

	DPAA2_CMDIF_INFO("Init %s on NUMA node %d", name, rte_socket_id());

	ret = dpaa2_cmdif_create(name, vdev, rte_socket_id());

	return ret;
}

static int
dpaa2_cmdif_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	int ret;

	name = rte_vdev_device_name(vdev);

	DPAA2_CMDIF_INFO("Closing %s on NUMA node %d", name, rte_socket_id());

	ret = dpaa2_cmdif_destroy(name);

	return ret;
}

static struct rte_vdev_driver dpaa2_cmdif_drv = {
	.probe = dpaa2_cmdif_probe,
	.remove = dpaa2_cmdif_remove
};

RTE_PMD_REGISTER_VDEV(DPAA2_CMDIF_PMD_NAME, dpaa2_cmdif_drv);

RTE_INIT(dpaa2_cmdif_init_log);

static void
dpaa2_cmdif_init_log(void)
{
	dpaa2_cmdif_logtype = rte_log_register("pmd.raw.dpaa2.cmdif");
	if (dpaa2_cmdif_logtype >= 0)
		rte_log_set_level(dpaa2_cmdif_logtype, RTE_LOG_INFO);
}
