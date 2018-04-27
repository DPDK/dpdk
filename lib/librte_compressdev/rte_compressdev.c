/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_memzone.h>

#include "rte_compressdev.h"
#include "rte_compressdev_internal.h"
#include "rte_compressdev_pmd.h"

#define RTE_COMPRESSDEV_DETACHED  (0)
#define RTE_COMPRESSDEV_ATTACHED  (1)

struct rte_compressdev rte_comp_devices[RTE_COMPRESS_MAX_DEVS];

struct rte_compressdev *rte_compressdevs = &rte_comp_devices[0];

static struct rte_compressdev_global compressdev_globals = {
		.devs			= &rte_comp_devices[0],
		.data			= { NULL },
		.nb_devs		= 0,
		.max_devs		= RTE_COMPRESS_MAX_DEVS
};

struct rte_compressdev_global *rte_compressdev_globals = &compressdev_globals;

static struct rte_compressdev *
rte_compressdev_get_dev(uint8_t dev_id)
{
	return &rte_compressdev_globals->devs[dev_id];
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_get_named_dev(const char *name)
{
	struct rte_compressdev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < rte_compressdev_globals->max_devs; i++) {
		dev = &rte_compressdev_globals->devs[i];

		if ((dev->attached == RTE_COMPRESSDEV_ATTACHED) &&
				(strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

static unsigned int
rte_compressdev_is_valid_dev(uint8_t dev_id)
{
	struct rte_compressdev *dev = NULL;

	if (dev_id >= rte_compressdev_globals->nb_devs)
		return 0;

	dev = rte_compressdev_get_dev(dev_id);
	if (dev->attached != RTE_COMPRESSDEV_ATTACHED)
		return 0;
	else
		return 1;
}


uint8_t __rte_experimental
rte_compressdev_count(void)
{
	return rte_compressdev_globals->nb_devs;
}

uint8_t __rte_experimental
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
	uint8_t nb_devices)
{
	uint8_t i, count = 0;
	struct rte_compressdev *devs = rte_compressdev_globals->devs;
	uint8_t max_devs = rte_compressdev_globals->max_devs;

	for (i = 0; i < max_devs && count < nb_devices;	i++) {

		if (devs[i].attached == RTE_COMPRESSDEV_ATTACHED) {
			int cmp;

			cmp = strncmp(devs[i].device->driver->name,
					driver_name,
					strlen(driver_name));

			if (cmp == 0)
				devices[count++] = devs[i].data->dev_id;
		}
	}

	return count;
}

int __rte_experimental
rte_compressdev_socket_id(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id))
		return -1;

	dev = rte_compressdev_get_dev(dev_id);

	return dev->data->socket_id;
}

static inline int
rte_compressdev_data_alloc(uint8_t dev_id, struct rte_compressdev_data **data,
		int socket_id)
{
	char mz_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name),
			"rte_compressdev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_compressdev_data),
				socket_id, 0);
	} else
		mz = rte_memzone_lookup(mz_name);

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_compressdev_data));

	return 0;
}

static uint8_t
rte_compressdev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_COMPRESS_MAX_DEVS; dev_id++) {
		if (rte_comp_devices[dev_id].attached ==
				RTE_COMPRESSDEV_DETACHED)
			return dev_id;
	}
	return RTE_COMPRESS_MAX_DEVS;
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_allocate(const char *name, int socket_id)
{
	struct rte_compressdev *compressdev;
	uint8_t dev_id;

	if (rte_compressdev_pmd_get_named_dev(name) != NULL) {
		COMPRESSDEV_LOG(ERR,
			"comp device with name %s already allocated!", name);
		return NULL;
	}

	dev_id = rte_compressdev_find_free_device_index();
	if (dev_id == RTE_COMPRESS_MAX_DEVS) {
		COMPRESSDEV_LOG(ERR, "Reached maximum number of comp devices");
		return NULL;
	}
	compressdev = rte_compressdev_get_dev(dev_id);

	if (compressdev->data == NULL) {
		struct rte_compressdev_data *compressdev_data =
				compressdev_globals.data[dev_id];

		int retval = rte_compressdev_data_alloc(dev_id,
				&compressdev_data, socket_id);

		if (retval < 0 || compressdev_data == NULL)
			return NULL;

		compressdev->data = compressdev_data;

		snprintf(compressdev->data->name, RTE_COMPRESSDEV_NAME_MAX_LEN,
				"%s", name);

		compressdev->data->dev_id = dev_id;
		compressdev->data->socket_id = socket_id;
		compressdev->data->dev_started = 0;

		compressdev->attached = RTE_COMPRESSDEV_ATTACHED;

		compressdev_globals.nb_devs++;
	}

	return compressdev;
}

int __rte_experimental
rte_compressdev_pmd_release_device(struct rte_compressdev *compressdev)
{
	int ret;

	if (compressdev == NULL)
		return -EINVAL;

	/* Close device only if device operations have been set */
	if (compressdev->dev_ops) {
		ret = rte_compressdev_close(compressdev->data->dev_id);
		if (ret < 0)
			return ret;
	}

	compressdev->attached = RTE_COMPRESSDEV_DETACHED;
	compressdev_globals.nb_devs--;
	return 0;
}

int __rte_experimental
rte_compressdev_configure(uint8_t dev_id, struct rte_compressdev_config *config)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	if (dev->data->dev_started) {
		COMPRESSDEV_LOG(ERR,
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	return (*dev->dev_ops->dev_configure)(dev, config);
}


int __rte_experimental
rte_compressdev_start(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int diag;

	COMPRESSDEV_LOG(DEBUG, "Start dev_id=%" PRIu8, dev_id);

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already started", dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

void __rte_experimental
rte_compressdev_stop(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already stopped", dev_id);
		return;
	}

	(*dev->dev_ops->dev_stop)(dev);
	dev->data->dev_started = 0;
}

int __rte_experimental
rte_compressdev_close(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int retval;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -1;
	}

	dev = &rte_comp_devices[dev_id];

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		COMPRESSDEV_LOG(ERR, "Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	retval = (*dev->dev_ops->dev_close)(dev);

	if (retval < 0)
		return retval;

	return 0;
}

void __rte_experimental
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info)
{
	struct rte_compressdev *dev;

	if (dev_id >= compressdev_globals.nb_devs) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	memset(dev_info, 0, sizeof(struct rte_compressdev_info));

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->driver_name = dev->device->driver->name;
}

const char * __rte_experimental
rte_compressdev_name_get(uint8_t dev_id)
{
	struct rte_compressdev *dev = rte_compressdev_get_dev(dev_id);

	if (dev == NULL)
		return NULL;

	return dev->data->name;
}

RTE_INIT(rte_compressdev_log);

static void
rte_compressdev_log(void)
{
	compressdev_logtype = rte_log_register("lib.compressdev");
	if (compressdev_logtype >= 0)
		rte_log_set_level(compressdev_logtype, RTE_LOG_NOTICE);
}
