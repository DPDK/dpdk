/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/queue.h>

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
	int ret;

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
	if (strcmp(bus->name, "pci") == 0)
		return rte_eal_hotplug_add("pci", name, devargs);
	if (strcmp(bus->name, "vdev") != 0) {
		RTE_LOG(ERR, EAL, "Device attach is only supported for PCI and vdev devices.\n");
		return -ENOTSUP;
	}

	/*
	 * If we haven't found a bus device the user meant to "hotplug" a
	 * virtual device instead.
	 */
	ret = rte_vdev_init(name, devargs);
	if (ret)
		RTE_LOG(ERR, EAL, "Driver cannot attach the device (%s)\n",
			name);
	return ret;
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

int rte_eal_hotplug_add(const char *busname, const char *devname,
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

int rte_eal_hotplug_remove(const char *busname, const char *devname)
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
