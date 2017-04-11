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

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_device_list, rte_vdev_device);

static struct vdev_device_list vdev_device_list =
	TAILQ_HEAD_INITIALIZER(vdev_device_list);
struct vdev_driver_list vdev_driver_list =
	TAILQ_HEAD_INITIALIZER(vdev_driver_list);

static void rte_vdev_bus_register(void);

/* register a driver */
void
rte_eal_vdrv_register(struct rte_vdev_driver *driver)
{
	rte_vdev_bus_register();

	TAILQ_INSERT_TAIL(&vdev_driver_list, driver, next);
	rte_eal_driver_register(&driver->driver);
}

/* unregister a driver */
void
rte_eal_vdrv_unregister(struct rte_vdev_driver *driver)
{
	rte_eal_driver_unregister(&driver->driver);
	TAILQ_REMOVE(&vdev_driver_list, driver, next);
}

static int
vdev_probe_all_drivers(const char *name, const char *args)
{
	struct rte_vdev_driver *driver;

	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		/*
		 * search a driver prefix in virtual device name.
		 * For example, if the driver is pcap PMD, driver->name
		 * will be "net_pcap", but "name" will be "net_pcapN".
		 * So use strncmp to compare.
		 */
		if (!strncmp(driver->driver.name, name,
			    strlen(driver->driver.name)))
			return driver->probe(name, args);
	}

	/* Give new names precedence over aliases. */
	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		if (driver->driver.alias &&
		    !strncmp(driver->driver.alias, name,
			    strlen(driver->driver.alias)))
			return driver->probe(name, args);
	}

	return 1;
}

int
rte_eal_vdev_init(const char *name, const char *args)
{
	int ret;

	if (name == NULL)
		return -EINVAL;

	ret = vdev_probe_all_drivers(name, args);
	if (ret  > 0)
		RTE_LOG(ERR, EAL, "no driver found for %s\n", name);

	return ret;
}

static int
vdev_remove_driver(const char *name)
{
	struct rte_vdev_driver *driver;

	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		/*
		 * search a driver prefix in virtual device name.
		 * For example, if the driver is pcap PMD, driver->name
		 * will be "net_pcap", but "name" will be "net_pcapN".
		 * So use strncmp to compare.
		 */
		if (!strncmp(driver->driver.name, name,
			     strlen(driver->driver.name)))
			return driver->remove(name);
	}

	/* Give new names precedence over aliases. */
	TAILQ_FOREACH(driver, &vdev_driver_list, next) {
		if (driver->driver.alias &&
		    !strncmp(driver->driver.alias, name,
			    strlen(driver->driver.alias)))
			return driver->remove(name);
	}

	return 1;
}

int
rte_eal_vdev_uninit(const char *name)
{
	int ret;

	if (name == NULL)
		return -EINVAL;

	ret = vdev_remove_driver(name);
	if (ret > 0)
		RTE_LOG(ERR, EAL, "no driver found for %s\n", name);

	return ret;
}

static int
vdev_scan(void)
{
	/* for virtual devices we don't need to scan anything */
	return 0;
}

static int
vdev_probe(void)
{
	struct rte_devargs *devargs;

	/*
	 * Note that the dev_driver_list is populated here
	 * from calls made to rte_eal_driver_register from constructor functions
	 * embedded into PMD modules via the RTE_PMD_REGISTER_VDEV macro
	 */

	/* call the init function for each virtual device */
	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->type != RTE_DEVTYPE_VIRTUAL)
			continue;

		if (rte_eal_vdev_init(devargs->virt.drv_name,
				      devargs->args)) {
			RTE_LOG(ERR, EAL, "failed to initialize %s device\n",
				devargs->virt.drv_name);
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
