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
#include <sys/queue.h>

#include <rte_eal.h>
#include <rte_bus.h>
#include <rte_vdev.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_memory.h>

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_device_list, rte_vdev_device);

static struct vdev_device_list vdev_device_list =
	TAILQ_HEAD_INITIALIZER(vdev_device_list);
struct vdev_driver_list vdev_driver_list =
	TAILQ_HEAD_INITIALIZER(vdev_driver_list);

static void rte_vdev_bus_register(void);

/* register a driver */
void
rte_vdev_register(struct rte_vdev_driver *driver)
{
	rte_vdev_bus_register();

	TAILQ_INSERT_TAIL(&vdev_driver_list, driver, next);
}

/* unregister a driver */
void
rte_vdev_unregister(struct rte_vdev_driver *driver)
{
	TAILQ_REMOVE(&vdev_driver_list, driver, next);
}

/*
 * Parse "driver" devargs without adding a dependency on rte_kvargs.h
 */
static char *parse_driver_arg(const char *args)
{
	const char *c;
	char *str;

	if (!args || args[0] == '\0')
		return NULL;

	c = args;

	do {
		if (strncmp(c, "driver=", 7) == 0) {
			c += 7;
			break;
		}

		c = strchr(c, ',');
		if (c)
			c++;
	} while (c);

	if (c)
		str = strdup(c);
	else
		str = NULL;

	return str;
}

static int
vdev_probe_all_drivers(struct rte_vdev_device *dev)
{
	const char *name;
	char *drv_name;
	struct rte_vdev_driver *driver;
	int ret = 1;

	drv_name = parse_driver_arg(rte_vdev_device_args(dev));
	name = drv_name ? drv_name : rte_vdev_device_name(dev);

	RTE_LOG(DEBUG, EAL, "Search driver %s to probe device %s\n", name,
		rte_vdev_device_name(dev));

	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		/*
		 * search a driver prefix in virtual device name.
		 * For example, if the driver is pcap PMD, driver->name
		 * will be "net_pcap", but "name" will be "net_pcapN".
		 * So use strncmp to compare.
		 */
		if (!strncmp(driver->driver.name, name,
			    strlen(driver->driver.name))) {
			dev->device.driver = &driver->driver;
			ret = driver->probe(dev);
			if (ret)
				dev->device.driver = NULL;
			goto out;
		}
	}

	/* Give new names precedence over aliases. */
	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		if (driver->driver.alias &&
		    !strncmp(driver->driver.alias, name,
			    strlen(driver->driver.alias))) {
			dev->device.driver = &driver->driver;
			ret = driver->probe(dev);
			if (ret)
				dev->device.driver = NULL;
			break;
		}
	}

out:
	free(drv_name);
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

	devargs->type = RTE_DEVTYPE_VIRTUAL;
	if (args)
		devargs->args = strdup(args);

	ret = snprintf(devargs->virt.drv_name,
			       sizeof(devargs->virt.drv_name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(devargs->virt.drv_name)) {
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
	dev->device.name = devargs->virt.drv_name;

	ret = vdev_probe_all_drivers(dev);
	if (ret) {
		if (ret > 0)
			RTE_LOG(ERR, EAL, "no driver found for %s\n", name);
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
		RTE_LOG(DEBUG, EAL, "no driver attach to device %s\n", name);
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

	/* for virtual devices we scan the devargs_list populated via cmdline */

	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->type != RTE_DEVTYPE_VIRTUAL)
			continue;

		dev = find_vdev(devargs->virt.drv_name);
		if (dev)
			continue;

		dev = calloc(1, sizeof(*dev));
		if (!dev)
			return -1;

		dev->device.devargs = devargs;
		dev->device.numa_node = SOCKET_ID_ANY;
		dev->device.name = devargs->virt.drv_name;

		TAILQ_INSERT_TAIL(&vdev_device_list, dev, next);
	}

	return 0;
}

static int
vdev_probe(void)
{
	struct rte_vdev_device *dev;

	/* call the init function for each virtual device */
	TAILQ_FOREACH(dev, &vdev_device_list, next) {

		if (dev->device.driver)
			continue;

		if (vdev_probe_all_drivers(dev)) {
			RTE_LOG(ERR, EAL, "failed to initialize %s device\n",
				rte_vdev_device_name(dev));
			return -1;
		}
	}

	return 0;
}

static struct rte_bus rte_vdev_bus = {
	.scan = vdev_scan,
	.probe = vdev_probe,
};

RTE_INIT(rte_vdev_bus_register);

static void rte_vdev_bus_register(void)
{
	static int registered;

	if (registered)
		return;

	registered = 1;
	rte_vdev_bus.name = RTE_STR(virtual);
	rte_bus_register(&rte_vdev_bus);
}
