/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_vdev.h>

#include "dsw_evdev.h"

#define EVENTDEV_NAME_DSW_PMD event_dsw

static int
dsw_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eventdev *dev;
	struct dsw_evdev *dsw;

	name = rte_vdev_device_name(vdev);

	dev = rte_event_pmd_vdev_init(name, sizeof(struct dsw_evdev),
				      rte_socket_id());
	if (dev == NULL)
		return -EFAULT;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dsw = dev->data->dev_private;
	dsw->data = dev->data;

	return 0;
}

static int
dsw_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver evdev_dsw_pmd_drv = {
	.probe = dsw_probe,
	.remove = dsw_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DSW_PMD, evdev_dsw_pmd_drv);
