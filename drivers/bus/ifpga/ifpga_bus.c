/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <rte_errno.h>
#include <rte_bus.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_alarm.h>
#include <rte_string_fns.h>

#include "rte_rawdev.h"
#include "rte_rawdev_pmd.h"
#include "rte_bus_ifpga.h"
#include "ifpga_logs.h"
#include "ifpga_common.h"

int ifpga_bus_logtype;

/* Forward declaration to access Intel FPGA bus
 * on which iFPGA devices are connected
 */
static struct rte_bus rte_ifpga_bus;

static struct ifpga_afu_dev_list ifpga_afu_dev_list =
	TAILQ_HEAD_INITIALIZER(ifpga_afu_dev_list);
static struct ifpga_afu_drv_list ifpga_afu_drv_list =
	TAILQ_HEAD_INITIALIZER(ifpga_afu_drv_list);


/* register a ifpga bus based driver */
void rte_ifpga_driver_register(struct rte_afu_driver *driver)
{
	RTE_VERIFY(driver);

	TAILQ_INSERT_TAIL(&ifpga_afu_drv_list, driver, next);
}

/* un-register a fpga bus based driver */
void rte_ifpga_driver_unregister(struct rte_afu_driver *driver)
{
	TAILQ_REMOVE(&ifpga_afu_drv_list, driver, next);
}

static struct rte_afu_device *
ifpga_find_afu_dev(const struct rte_rawdev *rdev,
	const struct rte_afu_id *afu_id)
{
	struct rte_afu_device *afu_dev = NULL;

	TAILQ_FOREACH(afu_dev, &ifpga_afu_dev_list, next) {
		if (afu_dev &&
			afu_dev->rawdev == rdev &&
			!ifpga_afu_id_cmp(&afu_dev->id, afu_id))
			return afu_dev;
	}
	return NULL;
}

static const char * const valid_args[] = {
#define IFPGA_ARG_NAME         "ifpga"
	IFPGA_ARG_NAME,
#define IFPGA_ARG_PORT         "port"
	IFPGA_ARG_PORT,
#define IFPGA_AFU_BTS          "afu_bts"
	IFPGA_AFU_BTS,
	NULL
};

/*
 * Scan the content of the FPGA bus, and the devices in the devices
 * list
 */
static struct rte_afu_device *
ifpga_scan_one(struct rte_rawdev *rawdev,
		struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist = NULL;
	struct rte_afu_device *afu_dev = NULL;
	struct rte_afu_pr_conf afu_pr_conf;
	int ret = 0;
	char *path = NULL;

	memset(&afu_pr_conf, 0, sizeof(struct rte_afu_pr_conf));

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (!kvlist) {
		IFPGA_BUS_ERR("error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, IFPGA_ARG_PORT) == 1) {
		if (rte_kvargs_process(kvlist, IFPGA_ARG_PORT,
		&rte_ifpga_get_integer32_arg, &afu_pr_conf.afu_id.port) < 0) {
			IFPGA_BUS_ERR("error to parse %s",
				     IFPGA_ARG_PORT);
			goto end;
		}
	} else {
		IFPGA_BUS_ERR("arg %s is mandatory for ifpga bus",
			  IFPGA_ARG_PORT);
		goto end;
	}

	if (rte_kvargs_count(kvlist, IFPGA_AFU_BTS) == 1) {
		if (rte_kvargs_process(kvlist, IFPGA_AFU_BTS,
				       &rte_ifpga_get_string_arg, &path) < 0) {
			IFPGA_BUS_ERR("Failed to parse %s",
				     IFPGA_AFU_BTS);
			goto end;
		}
		afu_pr_conf.pr_enable = 1;
	} else {
		afu_pr_conf.pr_enable = 0;
	}

	afu_pr_conf.afu_id.uuid.uuid_low = 0;
	afu_pr_conf.afu_id.uuid.uuid_high = 0;

	if (ifpga_find_afu_dev(rawdev, &afu_pr_conf.afu_id))
		goto end;

	afu_dev = calloc(1, sizeof(*afu_dev));
	if (!afu_dev)
		goto end;

	afu_dev->device.bus = &rte_ifpga_bus;
	afu_dev->device.devargs = devargs;
	afu_dev->device.numa_node = SOCKET_ID_ANY;
	afu_dev->device.name = devargs->name;
	afu_dev->rawdev = rawdev;
	afu_dev->id.uuid.uuid_low  = 0;
	afu_dev->id.uuid.uuid_high = 0;
	afu_dev->id.port      = afu_pr_conf.afu_id.port;

	if (rawdev->dev_ops && rawdev->dev_ops->dev_info_get)
		rawdev->dev_ops->dev_info_get(rawdev, afu_dev);

	if (rawdev->dev_ops &&
		rawdev->dev_ops->dev_start &&
		rawdev->dev_ops->dev_start(rawdev))
		goto end;

	strlcpy(afu_pr_conf.bs_path, path, sizeof(afu_pr_conf.bs_path));
	if (rawdev->dev_ops &&
		rawdev->dev_ops->firmware_load &&
		rawdev->dev_ops->firmware_load(rawdev,
				&afu_pr_conf)){
		IFPGA_BUS_ERR("firmware load error %d\n", ret);
		goto end;
	}
	afu_dev->id.uuid.uuid_low  = afu_pr_conf.afu_id.uuid.uuid_low;
	afu_dev->id.uuid.uuid_high = afu_pr_conf.afu_id.uuid.uuid_high;

	rte_kvargs_free(kvlist);
	free(path);
	return afu_dev;

end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (path)
		free(path);
	if (afu_dev)
		free(afu_dev);

	return NULL;
}

/*
 * Scan the content of the FPGA bus, and the devices in the devices
 * list
 */
static int
ifpga_scan(void)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist = NULL;
	struct rte_rawdev *rawdev = NULL;
	char *name = NULL;
	char name1[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_afu_device *afu_dev = NULL;

	/* for FPGA devices we scan the devargs_list populated via cmdline */
	RTE_EAL_DEVARGS_FOREACH(IFPGA_ARG_NAME, devargs) {
		if (devargs->bus != &rte_ifpga_bus)
			continue;

		kvlist = rte_kvargs_parse(devargs->args, valid_args);
		if (!kvlist) {
			IFPGA_BUS_ERR("error when parsing param");
			goto end;
		}

		if (rte_kvargs_count(kvlist, IFPGA_ARG_NAME) == 1) {
			if (rte_kvargs_process(kvlist, IFPGA_ARG_NAME,
				       &rte_ifpga_get_string_arg, &name) < 0) {
				IFPGA_BUS_ERR("error to parse %s",
				     IFPGA_ARG_NAME);
				goto end;
			}
		} else {
			IFPGA_BUS_ERR("arg %s is mandatory for ifpga bus",
			  IFPGA_ARG_NAME);
			goto end;
		}

		memset(name1, 0, sizeof(name1));
		snprintf(name1, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%s", name);

		rawdev = rte_rawdev_pmd_get_named_dev(name1);
		if (!rawdev)
			goto end;

		afu_dev = ifpga_scan_one(rawdev, devargs);
		if (afu_dev != NULL)
			TAILQ_INSERT_TAIL(&ifpga_afu_dev_list, afu_dev, next);
	}

end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (name)
		free(name);

	return 0;
}

/*
 * Match the AFU Driver and AFU Device using the ID Table
 */
static int
rte_afu_match(const struct rte_afu_driver *afu_drv,
	      const struct rte_afu_device *afu_dev)
{
	const struct rte_afu_uuid *id_table;

	for (id_table = afu_drv->id_table;
		((id_table->uuid_low != 0) && (id_table->uuid_high != 0));
	     id_table++) {
		/* check if device's identifiers match the driver's ones */
		if ((id_table->uuid_low != afu_dev->id.uuid.uuid_low) ||
				id_table->uuid_high !=
				 afu_dev->id.uuid.uuid_high)
			continue;

		return 1;
	}

	return 0;
}

static int
ifpga_probe_one_driver(struct rte_afu_driver *drv,
			struct rte_afu_device *afu_dev)
{
	int ret;

	if (!rte_afu_match(drv, afu_dev))
		/* Match of device and driver failed */
		return 1;

	/* reference driver structure */
	afu_dev->driver = drv;

	/* call the driver probe() function */
	ret = drv->probe(afu_dev);
	if (ret)
		afu_dev->driver = NULL;
	else
		afu_dev->device.driver = &drv->driver;

	return ret;
}

static int
ifpga_probe_all_drivers(struct rte_afu_device *afu_dev)
{
	struct rte_afu_driver *drv = NULL;
	int ret = 0;

	if (afu_dev == NULL)
		return -1;

	/* Check if a driver is already loaded */
	if (rte_dev_is_probed(&afu_dev->device)) {
		IFPGA_BUS_DEBUG("Device %s is already probed\n",
				rte_ifpga_device_name(afu_dev));
		return -EEXIST;
	}

	TAILQ_FOREACH(drv, &ifpga_afu_drv_list, next) {
		ret = ifpga_probe_one_driver(drv, afu_dev);
		if (ret < 0)
			/* negative value is an error */
			return ret;
		if (ret > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	if ((ret > 0) && (afu_dev->driver == NULL))
		return 0;
	else
		return ret;
}

/*
 * Scan the content of the Intel FPGA bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
static int
ifpga_probe(void)
{
	struct rte_afu_device *afu_dev = NULL;
	int ret = 0;

	TAILQ_FOREACH(afu_dev, &ifpga_afu_dev_list, next) {
		ret = ifpga_probe_all_drivers(afu_dev);
		if (ret == -EEXIST)
			continue;
		if (ret < 0)
			IFPGA_BUS_ERR("failed to initialize %s device\n",
				rte_ifpga_device_name(afu_dev));
	}

	return ret;
}

static int
ifpga_plug(struct rte_device *dev)
{
	return ifpga_probe_all_drivers(RTE_DEV_TO_AFU(dev));
}

static int
ifpga_remove_driver(struct rte_afu_device *afu_dev)
{
	const char *name;

	name = rte_ifpga_device_name(afu_dev);
	if (afu_dev->driver == NULL) {
		IFPGA_BUS_DEBUG("no driver attach to device %s\n", name);
		return 1;
	}

	return afu_dev->driver->remove(afu_dev);
}

static int
ifpga_unplug(struct rte_device *dev)
{
	struct rte_afu_device *afu_dev = NULL;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	afu_dev = RTE_DEV_TO_AFU(dev);
	if (!afu_dev)
		return -ENOENT;

	ret = ifpga_remove_driver(afu_dev);
	if (ret)
		return ret;

	TAILQ_REMOVE(&ifpga_afu_dev_list, afu_dev, next);

	rte_devargs_remove(dev->devargs);
	free(afu_dev);
	return 0;

}

static struct rte_device *
ifpga_find_device(const struct rte_device *start,
	rte_dev_cmp_t cmp, const void *data)
{
	struct rte_afu_device *afu_dev;

	TAILQ_FOREACH(afu_dev, &ifpga_afu_dev_list, next) {
		if (start && &afu_dev->device == start) {
			start = NULL;
			continue;
		}
		if (cmp(&afu_dev->device, data) == 0)
			return &afu_dev->device;
	}

	return NULL;
}
static int
ifpga_parse(const char *name, void *addr)
{
	int *out = addr;
	struct rte_rawdev *rawdev = NULL;
	char rawdev_name[RTE_RAWDEV_NAME_MAX_LEN];
	char *c1 = NULL;
	char *c2 = NULL;
	int port = IFPGA_BUS_DEV_PORT_MAX;
	char str_port[8];
	int str_port_len = 0;
	int ret;

	memset(str_port, 0, 8);
	c1 = strchr(name, '|');
	if (c1 != NULL) {
		str_port_len = c1 - name;
		c2 = c1 + 1;
	}

	if (str_port_len < 8 &&
		str_port_len > 0) {
		memcpy(str_port, name, str_port_len);
		ret = sscanf(str_port, "%d", &port);
		if (ret == -1)
			return 0;
	}

	memset(rawdev_name, 0, sizeof(rawdev_name));
	snprintf(rawdev_name, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%s", c2);
	rawdev = rte_rawdev_pmd_get_named_dev(rawdev_name);

	if ((port < IFPGA_BUS_DEV_PORT_MAX) &&
		rawdev &&
		(addr != NULL))
		*out = port;

	if ((port < IFPGA_BUS_DEV_PORT_MAX) &&
		rawdev)
		return 0;
	else
		return 1;
}

static struct rte_bus rte_ifpga_bus = {
	.scan        = ifpga_scan,
	.probe       = ifpga_probe,
	.find_device = ifpga_find_device,
	.plug        = ifpga_plug,
	.unplug      = ifpga_unplug,
	.parse       = ifpga_parse,
};

RTE_REGISTER_BUS(IFPGA_BUS_NAME, rte_ifpga_bus);

RTE_INIT(ifpga_init_log)
{
	ifpga_bus_logtype = rte_log_register("bus.ifpga");
	if (ifpga_bus_logtype >= 0)
		rte_log_set_level(ifpga_bus_logtype, RTE_LOG_NOTICE);
}
