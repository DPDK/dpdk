/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
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
#include <rte_string_fns.h>
#include <rte_errno.h>

#include "rte_bus_vdev.h"
#include "vdev_logs.h"
#include "vdev_private.h"

#define VDEV_MP_KEY	"bus_vdev_mp"

/* Forward declare to access virtual bus name */
static struct rte_bus rte_vdev_bus;

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_device_list, rte_vdev_device);

static struct vdev_device_list vdev_device_list =
	TAILQ_HEAD_INITIALIZER(vdev_device_list);
/* The lock needs to be recursive because a vdev can manage another vdev. */
static rte_spinlock_recursive_t vdev_device_list_lock =
	RTE_SPINLOCK_RECURSIVE_INITIALIZER;

static struct vdev_driver_list vdev_driver_list =
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
	RTE_TAILQ_FOREACH_SAFE(custom_scan, &vdev_custom_scans, next,
				tmp_scan) {
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
vdev_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_vdev_device *vdev = RTE_DEV_TO_VDEV(dev);
	const struct rte_vdev_driver *driver;

	if (!vdev) {
		rte_errno = EINVAL;
		return -1;
	}

	if (!vdev->device.driver) {
		VDEV_LOG(DEBUG, "no driver attach to device %s", dev->name);
		return 1;
	}

	driver = container_of(vdev->device.driver, const struct rte_vdev_driver,
			driver);

	if (driver->dma_map)
		return driver->dma_map(vdev, addr, iova, len);

	return 0;
}

static int
vdev_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_vdev_device *vdev = RTE_DEV_TO_VDEV(dev);
	const struct rte_vdev_driver *driver;

	if (!vdev) {
		rte_errno = EINVAL;
		return -1;
	}

	if (!vdev->device.driver) {
		VDEV_LOG(DEBUG, "no driver attach to device %s", dev->name);
		return 1;
	}

	driver = container_of(vdev->device.driver, const struct rte_vdev_driver,
			driver);

	if (driver->dma_unmap)
		return driver->dma_unmap(vdev, addr, iova, len);

	return 0;
}

static int
vdev_probe_all_drivers(struct rte_vdev_device *dev)
{
	const char *name;
	struct rte_vdev_driver *driver;
	enum rte_iova_mode iova_mode;
	int ret;

	if (rte_dev_is_probed(&dev->device))
		return -EEXIST;

	name = rte_vdev_device_name(dev);
	VDEV_LOG(DEBUG, "Search driver to probe device %s", name);

	if (vdev_parse(name, &driver))
		return -1;

	iova_mode = rte_eal_iova_mode();
	if ((driver->drv_flags & RTE_VDEV_DRV_NEED_IOVA_AS_VA) && (iova_mode == RTE_IOVA_PA)) {
		VDEV_LOG(ERR, "%s requires VA IOVA mode but current mode is PA, not initializing",
				name);
		return -1;
	}

	ret = driver->probe(dev);
	if (ret == 0)
		dev->device.driver = &driver->driver;
	return ret;
}

/* The caller shall be responsible for thread-safe */
static struct rte_vdev_device *
find_vdev(const char *name)
{
	struct rte_vdev_device *dev;

	if (!name)
		return NULL;

	TAILQ_FOREACH(dev, &vdev_device_list, next) {
		const char *devname = rte_vdev_device_name(dev);

		if (!strcmp(devname, name))
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
		devargs->data = strdup(args);
	else
		devargs->data = strdup("");
	devargs->args = devargs->data;

	ret = strlcpy(devargs->name, name, sizeof(devargs->name));
	if (ret < 0 || ret >= (int)sizeof(devargs->name)) {
		rte_devargs_reset(devargs);
		free(devargs);
		return NULL;
	}

	return devargs;
}

static int
insert_vdev(const char *name, const char *args,
		struct rte_vdev_device **p_dev,
		bool init)
{
	struct rte_vdev_device *dev;
	struct rte_devargs *devargs;
	int ret;

	if (name == NULL)
		return -EINVAL;

	devargs = alloc_devargs(name, args);
	if (!devargs)
		return -ENOMEM;

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		ret = -ENOMEM;
		goto fail;
	}

	dev->device.bus = &rte_vdev_bus;
	dev->device.numa_node = SOCKET_ID_ANY;
	dev->device.name = devargs->name;

	if (find_vdev(name)) {
		/*
		 * A vdev is expected to have only one port.
		 * So there is no reason to try probing again,
		 * even with new arguments.
		 */
		ret = -EEXIST;
		goto fail;
	}

	if (init)
		rte_devargs_insert(&devargs);
	dev->device.devargs = devargs;
	TAILQ_INSERT_TAIL(&vdev_device_list, dev, next);

	if (p_dev)
		*p_dev = dev;

	return 0;
fail:
	rte_devargs_reset(devargs);
	free(devargs);
	free(dev);
	return ret;
}

int
rte_vdev_init(const char *name, const char *args)
{
	struct rte_vdev_device *dev;
	int ret;

	rte_spinlock_recursive_lock(&vdev_device_list_lock);
	ret = insert_vdev(name, args, &dev, true);
	if (ret == 0) {
		ret = vdev_probe_all_drivers(dev);
		if (ret) {
			if (ret > 0)
				VDEV_LOG(ERR, "no driver found for %s", name);
			/* If fails, remove it from vdev list */
			TAILQ_REMOVE(&vdev_device_list, dev, next);
			rte_devargs_remove(dev->device.devargs);
			free(dev);
		}
	}
	rte_spinlock_recursive_unlock(&vdev_device_list_lock);
	return ret;
}

static int
vdev_remove_driver(struct rte_vdev_device *dev)
{
	const char *name = rte_vdev_device_name(dev);
	const struct rte_vdev_driver *driver;

	if (!dev->device.driver) {
		VDEV_LOG(DEBUG, "no driver attach to device %s", name);
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
	int ret;

	if (name == NULL)
		return -EINVAL;

	rte_spinlock_recursive_lock(&vdev_device_list_lock);

	dev = find_vdev(name);
	if (!dev) {
		ret = -ENOENT;
		goto unlock;
	}

	ret = vdev_remove_driver(dev);
	if (ret)
		goto unlock;

	TAILQ_REMOVE(&vdev_device_list, dev, next);
	rte_devargs_remove(dev->device.devargs);
	free(dev);

unlock:
	rte_spinlock_recursive_unlock(&vdev_device_list_lock);
	return ret;
}

struct vdev_param {
#define VDEV_SCAN_REQ	1
#define VDEV_SCAN_ONE	2
#define VDEV_SCAN_REP	3
	int type;
	int num;
	char name[RTE_DEV_NAME_MAX_LEN];
};

static int vdev_plug(struct rte_device *dev);

/**
 * This function works as the action for both primary and secondary process
 * for static vdev discovery when a secondary process is booting.
 *
 * step 1, secondary process sends a sync request to ask for vdev in primary;
 * step 2, primary process receives the request, and send vdevs one by one;
 * step 3, primary process sends back reply, which indicates how many vdevs
 * are sent.
 */
static int
vdev_action(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_vdev_device *dev;
	struct rte_mp_msg mp_resp;
	struct vdev_param *ou = (struct vdev_param *)&mp_resp.param;
	const struct vdev_param *in = (const struct vdev_param *)mp_msg->param;
	const char *devname;
	int num;
	int ret;

	strlcpy(mp_resp.name, VDEV_MP_KEY, sizeof(mp_resp.name));
	mp_resp.len_param = sizeof(*ou);
	mp_resp.num_fds = 0;

	switch (in->type) {
	case VDEV_SCAN_REQ:
		ou->type = VDEV_SCAN_ONE;
		ou->num = 1;
		num = 0;

		rte_spinlock_recursive_lock(&vdev_device_list_lock);
		TAILQ_FOREACH(dev, &vdev_device_list, next) {
			devname = rte_vdev_device_name(dev);
			if (strlen(devname) == 0) {
				VDEV_LOG(INFO, "vdev with no name is not sent");
				continue;
			}
			VDEV_LOG(INFO, "send vdev, %s", devname);
			strlcpy(ou->name, devname, RTE_DEV_NAME_MAX_LEN);
			if (rte_mp_sendmsg(&mp_resp) < 0)
				VDEV_LOG(ERR, "send vdev, %s, failed, %s",
					 devname, strerror(rte_errno));
			num++;
		}
		rte_spinlock_recursive_unlock(&vdev_device_list_lock);

		ou->type = VDEV_SCAN_REP;
		ou->num = num;
		if (rte_mp_reply(&mp_resp, peer) < 0)
			VDEV_LOG(ERR, "Failed to reply a scan request");
		break;
	case VDEV_SCAN_ONE:
		VDEV_LOG(INFO, "receive vdev, %s", in->name);
		ret = insert_vdev(in->name, NULL, NULL, false);
		if (ret == -EEXIST)
			VDEV_LOG(DEBUG, "device already exist, %s", in->name);
		else if (ret < 0)
			VDEV_LOG(ERR, "failed to add vdev, %s", in->name);
		break;
	default:
		VDEV_LOG(ERR, "vdev cannot recognize this message");
	}

	return 0;
}

static int
vdev_scan(void)
{
	struct rte_vdev_device *dev;
	struct rte_devargs *devargs;
	struct vdev_custom_scan *custom_scan;

	if (rte_mp_action_register(VDEV_MP_KEY, vdev_action) < 0 &&
	    rte_errno != EEXIST) {
		/* for primary, unsupported IPC is not an error */
		if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
				rte_errno == ENOTSUP)
			goto scan;
		VDEV_LOG(ERR, "Failed to add vdev mp action");
		return -1;
	}

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct rte_mp_msg mp_req, *mp_rep;
		struct rte_mp_reply mp_reply;
		struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
		struct vdev_param *req = (struct vdev_param *)mp_req.param;
		struct vdev_param *resp;

		strlcpy(mp_req.name, VDEV_MP_KEY, sizeof(mp_req.name));
		mp_req.len_param = sizeof(*req);
		mp_req.num_fds = 0;
		req->type = VDEV_SCAN_REQ;
		if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
		    mp_reply.nb_received == 1) {
			mp_rep = &mp_reply.msgs[0];
			resp = (struct vdev_param *)mp_rep->param;
			VDEV_LOG(INFO, "Received %d vdevs", resp->num);
			free(mp_reply.msgs);
		} else
			VDEV_LOG(ERR, "Failed to request vdev from primary");

		/* Fall through to allow private vdevs in secondary process */
	}

scan:
	/* call custom scan callbacks if any */
	rte_spinlock_lock(&vdev_custom_scan_lock);
	TAILQ_FOREACH(custom_scan, &vdev_custom_scans, next) {
		if (custom_scan->callback != NULL)
			/*
			 * the callback should update devargs list
			 * by calling rte_devargs_insert() with
			 *     devargs.bus = rte_bus_find_by_name("vdev");
			 *     devargs.type = RTE_DEVTYPE_VIRTUAL;
			 *     devargs.policy = RTE_DEV_ALLOWED;
			 */
			custom_scan->callback(custom_scan->user_arg);
	}
	rte_spinlock_unlock(&vdev_custom_scan_lock);

	/* for virtual devices we scan the devargs_list populated via cmdline */
	RTE_EAL_DEVARGS_FOREACH("vdev", devargs) {

		dev = calloc(1, sizeof(*dev));
		if (!dev)
			return -1;

		rte_spinlock_recursive_lock(&vdev_device_list_lock);

		if (find_vdev(devargs->name)) {
			rte_spinlock_recursive_unlock(&vdev_device_list_lock);
			free(dev);
			continue;
		}

		dev->device.bus = &rte_vdev_bus;
		dev->device.devargs = devargs;
		dev->device.numa_node = SOCKET_ID_ANY;
		dev->device.name = devargs->name;

		TAILQ_INSERT_TAIL(&vdev_device_list, dev, next);

		rte_spinlock_recursive_unlock(&vdev_device_list_lock);
	}

	return 0;
}

static int
vdev_probe(void)
{
	struct rte_vdev_device *dev;
	int r, ret = 0;

	/* call the init function for each virtual device */
	TAILQ_FOREACH(dev, &vdev_device_list, next) {
		/* we don't use the vdev lock here, as it's only used in DPDK
		 * initialization; and we don't want to hold such a lock when
		 * we call each driver probe.
		 */

		r = vdev_probe_all_drivers(dev);
		if (r != 0) {
			if (r == -EEXIST)
				continue;
			VDEV_LOG(ERR, "failed to initialize %s device",
				rte_vdev_device_name(dev));
			ret = -1;
		}
	}

	return ret;
}

struct rte_device *
rte_vdev_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		     const void *data)
{
	const struct rte_vdev_device *vstart;
	struct rte_vdev_device *dev;

	rte_spinlock_recursive_lock(&vdev_device_list_lock);
	if (start != NULL) {
		vstart = RTE_DEV_TO_VDEV_CONST(start);
		dev = TAILQ_NEXT(vstart, next);
	} else {
		dev = TAILQ_FIRST(&vdev_device_list);
	}
	while (dev != NULL) {
		if (cmp(&dev->device, data) == 0)
			break;
		dev = TAILQ_NEXT(dev, next);
	}
	rte_spinlock_recursive_unlock(&vdev_device_list_lock);

	return dev ? &dev->device : NULL;
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

static enum rte_iova_mode
vdev_get_iommu_class(void)
{
	const char *name;
	struct rte_vdev_device *dev;
	struct rte_vdev_driver *driver;

	TAILQ_FOREACH(dev, &vdev_device_list, next) {
		name = rte_vdev_device_name(dev);
		if (vdev_parse(name, &driver))
			continue;

		if (driver->drv_flags & RTE_VDEV_DRV_NEED_IOVA_AS_VA)
			return RTE_IOVA_VA;
	}

	return RTE_IOVA_DC;
}

static struct rte_bus rte_vdev_bus = {
	.scan = vdev_scan,
	.probe = vdev_probe,
	.find_device = rte_vdev_find_device,
	.plug = vdev_plug,
	.unplug = vdev_unplug,
	.parse = vdev_parse,
	.dma_map = vdev_dma_map,
	.dma_unmap = vdev_dma_unmap,
	.get_iommu_class = vdev_get_iommu_class,
	.dev_iterate = rte_vdev_dev_iterate,
};

RTE_REGISTER_BUS(vdev, rte_vdev_bus);
RTE_LOG_REGISTER_DEFAULT(vdev_logtype_bus, NOTICE);
