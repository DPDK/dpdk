/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 RehiveTech. All rights reserved.
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
 *     * Neither the name of RehiveTech nor the names of its
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

#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_spinlock.h>
#include <rte_errno.h>

#include "rte_bus_vdev.h"
#include "vdev_logs.h"

int vdev_logtype_bus;

/* Forward declare to access virtual bus name */
static struct rte_bus rte_vdev_bus;

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_device_list, rte_vdev_device);

static struct vdev_device_list vdev_device_list =
	TAILQ_HEAD_INITIALIZER(vdev_device_list);
struct vdev_driver_list vdev_driver_list =
	TAILQ_HEAD_INITIALIZER(vdev_driver_list);

struct vdev_custom_scan {
	TAILQ_ENTRY(vdev_custom_scan) next;
	rte_vdev_scan_callback callback;
	void *user_arg;
};
TAILQ_HEAD(vdev_custom_scans, vdev_custom_scan);
static struct vdev_custom_scans vdev_custom_scans =
	TAILQ_HEAD_INITIALIZER(vdev_custom_scans);
static rte_spinlock_t vdev_custom_scan_lock = RTE_SPINLOCK_INITIALIZER;

/* register a driver */
void
rte_vdev_register(struct rte_vdev_driver *driver)
{
	TAILQ_INSERT_TAIL(&vdev_driver_list, driver, next);
}

/* unregister a driver */
void
rte_vdev_unregister(struct rte_vdev_driver *driver)
{
	TAILQ_REMOVE(&vdev_driver_list, driver, next);
}

int
rte_vdev_add_custom_scan(rte_vdev_scan_callback callback, void *user_arg)
{
	struct vdev_custom_scan *custom_scan;

	rte_spinlock_lock(&vdev_custom_scan_lock);

	/* check if already registered */
	TAILQ_FOREACH(custom_scan, &vdev_custom_scans, next) {
		if (custom_scan->callback == callback &&
				custom_scan->user_arg == user_arg)
			break;
	}

	if (custom_scan == NULL) {
		custom_scan = malloc(sizeof(struct vdev_custom_scan));
		if (custom_scan != NULL) {
			custom_scan->callback = callback;
			custom_scan->user_arg = user_arg;
			TAILQ_INSERT_TAIL(&vdev_custom_scans, custom_scan, next);
		}
	}

	rte_spinlock_unlock(&vdev_custom_scan_lock);

	return (custom_scan == NULL) ? -1 : 0;
}

int
rte_vdev_remove_custom_scan(rte_vdev_scan_callback callback, void *user_arg)
{
	struct vdev_custom_scan *custom_scan, *tmp_scan;

	rte_spinlock_lock(&vdev_custom_scan_lock);
	TAILQ_FOREACH_SAFE(custom_scan, &vdev_custom_scans, next, tmp_scan) {
		if (custom_scan->callback != callback ||
				(custom_scan->user_arg != (void *)-1 &&
				custom_scan->user_arg != user_arg))
			continue;
		TAILQ_REMOVE(&vdev_custom_scans, custom_scan, next);
		free(custom_scan);
	}
	rte_spinlock_unlock(&vdev_custom_scan_lock);

	return 0;
}

static int
vdev_parse(const char *name, void *addr)
{
	struct rte_vdev_driver **out = addr;
	struct rte_vdev_driver *driver = NULL;

	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		if (strncmp(driver->driver.name, name,
			    strlen(driver->driver.name)) == 0)
			break;
		if (driver->driver.alias &&
		    strncmp(driver->driver.alias, name,
			    strlen(driver->driver.alias)) == 0)
			break;
	}
	if (driver != NULL &&
	    addr != NULL)
		*out = driver;
	return driver == NULL;
}

static int
vdev_probe_all_drivers(struct rte_vdev_device *dev)
{
	const char *name;
	struct rte_vdev_driver *driver;
	int ret;

	name = rte_vdev_device_name(dev);

	VDEV_LOG(DEBUG, "Search driver %s to probe device %s\n", name,
		rte_vdev_device_name(dev));

	if (vdev_parse(name, &driver))
		return -1;
	dev->device.driver = &driver->driver;
	ret = driver->probe(dev);
	if (ret)
		dev->device.driver = NULL;
	return ret;
}

static struct rte_vdev_device *
find_vdev(const char *name)
{
	struct rte_vdev_device *dev;

	if (!name)
		return NULL;

	TAILQ_FOREACH(dev, &vdev_device_list, next) {
		const char *devname = rte_vdev_device_name(dev);

		if (!strncmp(devname, name, strlen(name)))
			return dev;
	}

	return NULL;
}

static struct rte_devargs *
alloc_devargs(const char *name, const char *args)
{
	struct rte_devargs *devargs;
	int ret;

	devargs = calloc(1, sizeof(*devargs));
	if (!devargs)
		return NULL;

	devargs->bus = &rte_vdev_bus;
	if (args)
		devargs->args = strdup(args);
	else
		devargs->args = strdup("");

	ret = snprintf(devargs->name, sizeof(devargs->name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(devargs->name)) {
		free(devargs->args);
		free(devargs);
		return NULL;
	}

	return devargs;
}

int
rte_vdev_init(const char *name, const char *args)
{
	struct rte_vdev_device *dev;
	struct rte_devargs *devargs;
	int ret;

	if (name == NULL)
		return -EINVAL;

	dev = find_vdev(name);
	if (dev)
		return -EEXIST;

	devargs = alloc_devargs(name, args);
	if (!devargs)
		return -ENOMEM;

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		ret = -ENOMEM;
		goto fail;
	}

	dev->device.devargs = devargs;
	dev->device.numa_node = SOCKET_ID_ANY;
	dev->device.name = devargs->name;

	ret = vdev_probe_all_drivers(dev);
	if (ret) {
		if (ret > 0)
			VDEV_LOG(ERR, "no driver found for %s\n", name);
		goto fail;
	}

	TAILQ_INSERT_TAIL(&devargs_list, devargs, next);

	TAILQ_INSERT_TAIL(&vdev_device_list, dev, next);
	return 0;

fail:
	free(devargs->args);
	free(devargs);
	free(dev);
	return ret;
}

static int
vdev_remove_driver(struct rte_vdev_device *dev)
{
	const char *name = rte_vdev_device_name(dev);
	const struct rte_vdev_driver *driver;

	if (!dev->device.driver) {
		VDEV_LOG(DEBUG, "no driver attach to device %s\n", name);
		return 1;
	}

	driver = container_of(dev->device.driver, const struct rte_vdev_driver,
		driver);
	return driver->remove(dev);
}

int
rte_vdev_uninit(const char *name)
{
	struct rte_vdev_device *dev;
	struct rte_devargs *devargs;
	int ret;

	if (name == NULL)
		return -EINVAL;

	dev = find_vdev(name);
	if (!dev)
		return -ENOENT;

	devargs = dev->device.devargs;

	ret = vdev_remove_driver(dev);
	if (ret)
		return ret;

	TAILQ_REMOVE(&vdev_device_list, dev, next);

	TAILQ_REMOVE(&devargs_list, devargs, next);

	free(devargs->args);
	free(devargs);
	free(dev);
	return 0;
}

static int
vdev_scan(void)
{
	struct rte_vdev_device *dev;
	struct rte_devargs *devargs;
	struct vdev_custom_scan *custom_scan;

	/* call custom scan callbacks if any */
	rte_spinlock_lock(&vdev_custom_scan_lock);
	TAILQ_FOREACH(custom_scan, &vdev_custom_scans, next) {
		if (custom_scan->callback != NULL)
			/*
			 * the callback should update devargs list
			 * by calling rte_eal_devargs_insert() with
			 *     devargs.bus = rte_bus_find_by_name("vdev");
			 *     devargs.type = RTE_DEVTYPE_VIRTUAL;
			 *     devargs.policy = RTE_DEV_WHITELISTED;
			 */
			custom_scan->callback(custom_scan->user_arg);
	}
	rte_spinlock_unlock(&vdev_custom_scan_lock);

	/* for virtual devices we scan the devargs_list populated via cmdline */
	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->bus != &rte_vdev_bus)
			continue;

		dev = find_vdev(devargs->name);
		if (dev)
			continue;

		dev = calloc(1, sizeof(*dev));
		if (!dev)
			return -1;

		dev->device.devargs = devargs;
		dev->device.numa_node = SOCKET_ID_ANY;
		dev->device.name = devargs->name;

		TAILQ_INSERT_TAIL(&vdev_device_list, dev, next);
	}

	return 0;
}

static int
vdev_probe(void)
{
	struct rte_vdev_device *dev;
	int ret = 0;

	/* call the init function for each virtual device */
	TAILQ_FOREACH(dev, &vdev_device_list, next) {

		if (dev->device.driver)
			continue;

		if (vdev_probe_all_drivers(dev)) {
			VDEV_LOG(ERR, "failed to initialize %s device\n",
				rte_vdev_device_name(dev));
			ret = -1;
		}
	}

	return ret;
}

static struct rte_device *
vdev_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		 const void *data)
{
	struct rte_vdev_device *dev;

	TAILQ_FOREACH(dev, &vdev_device_list, next) {
		if (start && &dev->device == start) {
			start = NULL;
			continue;
		}
		if (cmp(&dev->device, data) == 0)
			return &dev->device;
	}
	return NULL;
}

static int
vdev_plug(struct rte_device *dev)
{
	return vdev_probe_all_drivers(RTE_DEV_TO_VDEV(dev));
}

static int
vdev_unplug(struct rte_device *dev)
{
	return rte_vdev_uninit(dev->name);
}

static struct rte_bus rte_vdev_bus = {
	.scan = vdev_scan,
	.probe = vdev_probe,
	.find_device = vdev_find_device,
	.plug = vdev_plug,
	.unplug = vdev_unplug,
	.parse = vdev_parse,
};

RTE_REGISTER_BUS(vdev, rte_vdev_bus);

RTE_INIT(vdev_init_log)
{
	vdev_logtype_bus = rte_log_register("bus.vdev");
	if (vdev_logtype_bus >= 0)
		rte_log_set_level(vdev_logtype_bus, RTE_LOG_NOTICE);
}
