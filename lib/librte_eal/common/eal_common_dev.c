/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_compat.h>
#include <rte_bus.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_debug.h>
#include <rte_log.h>

#include "eal_private.h"

static int cmp_detached_dev_name(const struct rte_device *dev,
	const void *_name)
{
	const char *name = _name;

	/* skip attached devices */
	if (dev->driver != NULL)
		return 1;

	return strcmp(dev->name, name);
}

static int cmp_dev_name(const struct rte_device *dev, const void *_name)
{
	const char *name = _name;

	return strcmp(dev->name, name);
}

int rte_eal_dev_attach(const char *name, const char *devargs)
{
	struct rte_bus *bus;

	if (name == NULL || devargs == NULL) {
		RTE_LOG(ERR, EAL, "Invalid device or arguments provided\n");
		return -EINVAL;
	}

	bus = rte_bus_find_by_device_name(name);
	if (bus == NULL) {
		RTE_LOG(ERR, EAL, "Unable to find a bus for the device '%s'\n",
			name);
		return -EINVAL;
	}
	if (strcmp(bus->name, "pci") == 0 || strcmp(bus->name, "vdev") == 0)
		return rte_eal_hotplug_add(bus->name, name, devargs);

	RTE_LOG(ERR, EAL,
		"Device attach is only supported for PCI and vdev devices.\n");

	return -ENOTSUP;
}

int rte_eal_dev_detach(struct rte_device *dev)
{
	struct rte_bus *bus;
	int ret;

	if (dev == NULL) {
		RTE_LOG(ERR, EAL, "Invalid device provided.\n");
		return -EINVAL;
	}

	bus = rte_bus_find_by_device(dev);
	if (bus == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find bus for device (%s)\n",
			dev->name);
		return -EINVAL;
	}

	if (bus->unplug == NULL) {
		RTE_LOG(ERR, EAL, "Bus function not supported\n");
		return -ENOTSUP;
	}

	ret = bus->unplug(dev);
	if (ret)
		RTE_LOG(ERR, EAL, "Driver cannot detach the device (%s)\n",
			dev->name);
	return ret;
}

static char *
full_dev_name(const char *bus, const char *dev, const char *args)
{
	char *name;
	size_t len;

	len = snprintf(NULL, 0, "%s:%s,%s", bus, dev, args) + 1;
	name = calloc(1, len);
	if (name == NULL) {
		RTE_LOG(ERR, EAL, "Could not allocate full device name\n");
		return NULL;
	}
	snprintf(name, len, "%s:%s,%s", bus, dev, args);
	return name;
}

int __rte_experimental rte_eal_hotplug_add(const char *busname, const char *devname,
			const char *devargs)
{
	struct rte_bus *bus;
	struct rte_device *dev;
	struct rte_devargs *da;
	char *name;
	int ret;

	bus = rte_bus_find_by_name(busname);
	if (bus == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find bus (%s)\n", busname);
		return -ENOENT;
	}

	if (bus->plug == NULL) {
		RTE_LOG(ERR, EAL, "Function plug not supported by bus (%s)\n",
			bus->name);
		return -ENOTSUP;
	}

	name = full_dev_name(busname, devname, devargs);
	if (name == NULL)
		return -ENOMEM;

	da = calloc(1, sizeof(*da));
	if (da == NULL) {
		ret = -ENOMEM;
		goto err_name;
	}

	ret = rte_eal_devargs_parse(name, da);
	if (ret)
		goto err_devarg;

	ret = rte_eal_devargs_insert(da);
	if (ret)
		goto err_devarg;

	ret = bus->scan();
	if (ret)
		goto err_devarg;

	dev = bus->find_device(NULL, cmp_detached_dev_name, devname);
	if (dev == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find unplugged device (%s)\n",
			devname);
		ret = -ENODEV;
		goto err_devarg;
	}

	ret = bus->plug(dev);
	if (ret) {
		RTE_LOG(ERR, EAL, "Driver cannot attach the device (%s)\n",
			dev->name);
		goto err_devarg;
	}
	free(name);
	return 0;

err_devarg:
	if (rte_eal_devargs_remove(busname, devname)) {
		free(da->args);
		free(da);
	}
err_name:
	free(name);
	return ret;
}

int __rte_experimental
rte_eal_hotplug_remove(const char *busname, const char *devname)
{
	struct rte_bus *bus;
	struct rte_device *dev;
	int ret;

	bus = rte_bus_find_by_name(busname);
	if (bus == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find bus (%s)\n", busname);
		return -ENOENT;
	}

	if (bus->unplug == NULL) {
		RTE_LOG(ERR, EAL, "Function unplug not supported by bus (%s)\n",
			bus->name);
		return -ENOTSUP;
	}

	dev = bus->find_device(NULL, cmp_dev_name, devname);
	if (dev == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find plugged device (%s)\n", devname);
		return -EINVAL;
	}

	ret = bus->unplug(dev);
	if (ret)
		RTE_LOG(ERR, EAL, "Driver cannot detach the device (%s)\n",
			dev->name);
	rte_eal_devargs_remove(busname, devname);
	return ret;
}
