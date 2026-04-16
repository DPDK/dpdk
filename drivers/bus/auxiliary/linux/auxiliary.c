/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <string.h>
#include <dirent.h>

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <eal_filesystem.h>

#include "../private.h"

#define AUXILIARY_SYSFS_PATH "/sys/bus/auxiliary/devices"

/* Scan one auxiliary sysfs entry, and fill the devices list from it. */
static int
auxiliary_scan_one(const char *dirname, const char *name)
{
	struct rte_auxiliary_device *dev;
	struct rte_auxiliary_device *dev2;
	char filename[PATH_MAX];
	unsigned long tmp;
	int ret;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return -1;

	memset(dev, 0, sizeof(*dev));
	if (rte_strscpy(dev->name, name, sizeof(dev->name)) < 0) {
		free(dev);
		return -1;
	}
	dev->device.name = dev->name;

	/* Get NUMA node, default to 0 if not present */
	snprintf(filename, sizeof(filename), "%s/%s/numa_node",
		 dirname, name);
	if (access(filename, F_OK) == 0 &&
	    eal_parse_sysfs_value(filename, &tmp) == 0)
		dev->device.numa_node = tmp;
	else
		dev->device.numa_node = SOCKET_ID_ANY;

	auxiliary_on_scan(dev);

	/* Device is valid, add in list (sorted) */
	RTE_BUS_FOREACH_DEV(dev2, &auxiliary_bus.bus) {
		ret = strcmp(dev->name, dev2->name);
		if (ret > 0)
			continue;
		if (ret < 0) {
			rte_bus_insert_device(&auxiliary_bus.bus, &dev2->device, &dev->device);
		} else { /* already registered */
			if (rte_dev_is_probed(&dev2->device) &&
			    dev2->device.devargs != dev->device.devargs) {
				/* To probe device with new devargs. */
				rte_devargs_remove(dev2->device.devargs);
				auxiliary_on_scan(dev2);
			}
			free(dev);
		}
		return 0;
	}
	rte_bus_add_device(&auxiliary_bus.bus, &dev->device);
	return 0;
}

/*
 * Test whether the auxiliary device exist.
 */
bool
auxiliary_dev_exists(const char *name)
{
	DIR *dir;
	char dirname[PATH_MAX];

	snprintf(dirname, sizeof(dirname), "%s/%s",
		 AUXILIARY_SYSFS_PATH, name);
	dir = opendir(dirname);
	if (dir == NULL)
		return false;
	closedir(dir);
	return true;
}

/*
 * Scan the devices in the auxiliary bus.
 */
int
auxiliary_scan(void)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];
	struct rte_auxiliary_driver *drv;

	dir = opendir(AUXILIARY_SYSFS_PATH);
	if (dir == NULL) {
		AUXILIARY_LOG(INFO, "%s not found, is auxiliary module loaded?",
			      AUXILIARY_SYSFS_PATH);
		return 0;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		if (rte_bus_device_is_ignored(&auxiliary_bus.bus, e->d_name))
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s",
			 AUXILIARY_SYSFS_PATH, e->d_name);

		/* Ignore if no driver can handle. */
		RTE_BUS_FOREACH_DRV(drv, &auxiliary_bus.bus) {
			if (drv->match(e->d_name))
				break;
		}
		if (drv == NULL)
			continue;

		if (auxiliary_scan_one(dirname, e->d_name) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}
