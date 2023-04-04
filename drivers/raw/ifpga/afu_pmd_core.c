/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>

#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_rawdev_pmd.h>

#include "afu_pmd_core.h"

static struct rte_afu_uuid afu_pmd_uuid_map[AFU_RAWDEV_MAX_DRVS+1];
TAILQ_HEAD(afu_drv_list, afu_rawdev_drv);
static struct afu_drv_list afu_pmd_list = TAILQ_HEAD_INITIALIZER(afu_pmd_list);

#define afu_rawdev_trylock(dev) rte_spinlock_trylock(&dev->sd->lock)
#define afu_rawdev_unlock(dev) rte_spinlock_unlock(&dev->sd->lock)

static int afu_rawdev_configure(const struct rte_rawdev *rawdev,
	rte_rawdev_obj_t config, size_t config_size)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return -ENODEV;

	if (dev->ops && dev->ops->config)
		ret = (*dev->ops->config)(dev, config, config_size);

	return ret;
}

static int afu_rawdev_start(struct rte_rawdev *rawdev)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return -ENODEV;

	ret = afu_rawdev_trylock(dev);
	if (!ret) {
		IFPGA_RAWDEV_PMD_WARN("AFU is busy, please start it later");
		return ret;
	}

	if (dev->ops && dev->ops->start)
		ret = (*dev->ops->start)(dev);

	afu_rawdev_unlock(dev);

	return ret;
}

static void afu_rawdev_stop(struct rte_rawdev *rawdev)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return;

	ret = afu_rawdev_trylock(dev);
	if (!ret) {
		IFPGA_RAWDEV_PMD_WARN("AFU is busy, please stop it later");
		return;
	}

	if (dev->ops && dev->ops->stop)
		ret = (*dev->ops->stop)(dev);

	afu_rawdev_unlock(dev);
}

static int afu_rawdev_close(struct rte_rawdev *rawdev)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return -ENODEV;

	if (dev->ops && dev->ops->close)
		ret = (*dev->ops->close)(dev);

	return ret;
}

static int afu_rawdev_reset(struct rte_rawdev *rawdev)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return -ENODEV;

	ret = afu_rawdev_trylock(dev);
	if (!ret) {
		IFPGA_RAWDEV_PMD_WARN("AFU is busy, please reset it later");
		return ret;
	}

	if (dev->ops && dev->ops->reset)
		ret = (*dev->ops->reset)(dev);

	afu_rawdev_unlock(dev);

	return ret;
}

static int afu_rawdev_selftest(uint16_t dev_id)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	if (!rte_rawdev_pmd_is_valid_dev(dev_id))
		return -ENODEV;

	dev = afu_rawdev_get_priv(&rte_rawdevs[dev_id]);
	if (!dev)
		return -ENOENT;

	ret = afu_rawdev_trylock(dev);
	if (!ret) {
		IFPGA_RAWDEV_PMD_WARN("AFU is busy, please test it later");
		return ret;
	}

	if (dev->ops && dev->ops->test)
		ret = (*dev->ops->test)(dev);

	afu_rawdev_unlock(dev);

	return ret;
}

static int afu_rawdev_dump(struct rte_rawdev *rawdev, FILE *f)
{
	struct afu_rawdev *dev = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		return -ENODEV;

	if (dev->ops && dev->ops->dump)
		ret = (*dev->ops->dump)(dev, f);

	return ret;
}

static const struct rte_rawdev_ops afu_rawdev_ops = {
	.dev_info_get = NULL,
	.dev_configure = afu_rawdev_configure,
	.dev_start = afu_rawdev_start,
	.dev_stop = afu_rawdev_stop,
	.dev_close = afu_rawdev_close,
	.dev_reset = afu_rawdev_reset,

	.queue_def_conf = NULL,
	.queue_setup = NULL,
	.queue_release = NULL,
	.queue_count = NULL,

	.attr_get = NULL,
	.attr_set = NULL,

	.enqueue_bufs = NULL,
	.dequeue_bufs = NULL,

	.dump = afu_rawdev_dump,

	.xstats_get = NULL,
	.xstats_get_names = NULL,
	.xstats_get_by_name = NULL,
	.xstats_reset = NULL,

	.firmware_status_get = NULL,
	.firmware_version_get = NULL,
	.firmware_load = NULL,
	.firmware_unload = NULL,

	.dev_selftest = afu_rawdev_selftest,
};

static int afu_shared_data_alloc(const char *name,
	struct afu_shared_data **data, int socket_id)
{
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct afu_shared_data *sd = NULL;
	int init_mz = 0;

	if (!name || !data)
		return -EINVAL;

	/* name format is afu_?|??:??.? which is unique */
	snprintf(mz_name, sizeof(mz_name), "%s", name);

	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve(mz_name, sizeof(struct afu_shared_data),
				socket_id, 0);
		init_mz = 1;
	}

	if (!mz) {
		IFPGA_RAWDEV_PMD_ERR("Allocate memory zone %s failed!",
			mz_name);
		return -ENOMEM;
	}

	sd = (struct afu_shared_data *)mz->addr;

	if (init_mz)  /* initialize memory zone on the first time */
		rte_spinlock_init(&sd->lock);

	*data = sd;

	return 0;
}

static int afu_rawdev_name_get(struct rte_afu_device *afu_dev, char *name,
	size_t size)
{
	int n = 0;

	if (!afu_dev || !name || !size)
		return -EINVAL;

	n = snprintf(name, size, "afu_%s", afu_dev->device.name);
	if (n >= (int)size) {
		IFPGA_RAWDEV_PMD_ERR("Name of AFU device is too long!");
		return -ENAMETOOLONG;
	}

	return 0;
}

static struct afu_ops *afu_ops_get(struct rte_afu_uuid *afu_id)
{
	struct afu_rawdev_drv *drv = NULL;

	if (!afu_id)
		return NULL;

	TAILQ_FOREACH(drv, &afu_pmd_list, next) {
		if ((drv->uuid.uuid_low == afu_id->uuid_low) &&
			(drv->uuid.uuid_high == afu_id->uuid_high))
			break;
	}

	return drv ? drv->ops : NULL;
}

static int afu_rawdev_create(struct rte_afu_device *afu_dev, int socket_id)
{
	struct rte_rawdev *rawdev = NULL;
	struct afu_rawdev *dev = NULL;
	char name[RTE_RAWDEV_NAME_MAX_LEN] = {0};
	int ret = 0;

	if (!afu_dev)
		return -EINVAL;

	ret = afu_rawdev_name_get(afu_dev, name, sizeof(name));
	if (ret)
		return ret;

	IFPGA_RAWDEV_PMD_INFO("Create raw device %s on NUMA node %d",
		name, socket_id);

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct afu_rawdev),
				socket_id);
	if (!rawdev) {
		IFPGA_RAWDEV_PMD_ERR("Unable to allocate raw device");
		return -ENOMEM;
	}

	rawdev->dev_ops = &afu_rawdev_ops;
	rawdev->device = &afu_dev->device;
	rawdev->driver_name = afu_dev->driver->driver.name;

	dev = afu_rawdev_get_priv(rawdev);
	if (!dev)
		goto cleanup;

	dev->rawdev = rawdev;
	dev->port = afu_dev->id.port;
	dev->addr = afu_dev->mem_resource[0].addr;
	dev->ops = afu_ops_get(&afu_dev->id.uuid);
	if (dev->ops == NULL) {
		IFPGA_RAWDEV_PMD_ERR("Unsupported AFU device");
		goto cleanup;
	}

	if (dev->ops->init) {
		ret = (*dev->ops->init)(dev);
		if (ret) {
			IFPGA_RAWDEV_PMD_ERR("Failed to init %s", name);
			goto cleanup;
		}
	}

	ret = afu_shared_data_alloc(name, &dev->sd, socket_id);
	if (ret)
		goto cleanup;

	return ret;

cleanup:
	rte_rawdev_pmd_release(rawdev);
	return ret;
}

static int afu_rawdev_destroy(struct rte_afu_device *afu_dev)
{
	struct rte_rawdev *rawdev = NULL;
	char name[RTE_RAWDEV_NAME_MAX_LEN] = {0};
	int ret = 0;

	if (!afu_dev)
		return -EINVAL;

	ret = afu_rawdev_name_get(afu_dev, name, sizeof(name));
	if (ret)
		return ret;

	IFPGA_RAWDEV_PMD_INFO("Destroy raw device %s", name);

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev) {
		IFPGA_RAWDEV_PMD_ERR("Raw device %s not found", name);
		return -EINVAL;
	}

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		IFPGA_RAWDEV_PMD_DEBUG("Device cleanup failed");

	return 0;
}

static int afu_rawdev_probe(struct rte_afu_device *afu_dev)
{
	IFPGA_RAWDEV_PMD_FUNC_TRACE();
	return afu_rawdev_create(afu_dev, rte_socket_id());
}

static int afu_rawdev_remove(struct rte_afu_device *afu_dev)
{
	IFPGA_RAWDEV_PMD_FUNC_TRACE();
	return afu_rawdev_destroy(afu_dev);
}

static struct rte_afu_driver afu_pmd = {
	.id_table = afu_pmd_uuid_map,
	.probe = afu_rawdev_probe,
	.remove = afu_rawdev_remove
};

RTE_PMD_REGISTER_AFU(afu_rawdev_driver, afu_pmd);

static void update_uuid_map(void)
{
	int i = 0;
	struct rte_afu_uuid *afu_id = afu_pmd_uuid_map;
	struct afu_rawdev_drv *drv;

	TAILQ_FOREACH(drv, &afu_pmd_list, next) {
		if (i++ < AFU_RAWDEV_MAX_DRVS) {
			afu_id->uuid_low = drv->uuid.uuid_low;
			afu_id->uuid_high = drv->uuid.uuid_high;
			afu_id++;
		}
	}
	if (i <= AFU_RAWDEV_MAX_DRVS) {
		afu_id->uuid_low = 0;
		afu_id->uuid_high = 0;
	}
}

void afu_pmd_register(struct afu_rawdev_drv *driver)
{
	TAILQ_INSERT_TAIL(&afu_pmd_list, driver, next);
	update_uuid_map();
}

void afu_pmd_unregister(struct afu_rawdev_drv *driver)
{
	TAILQ_REMOVE(&afu_pmd_list, driver, next);
	update_uuid_map();
}
