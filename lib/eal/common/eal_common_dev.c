/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <bus_driver.h>
#include <rte_class.h>
#include <dev_driver.h>
#include <rte_devargs.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include <eal_export.h>
#include "eal_private.h"
#include "hotplug_mp.h"

RTE_EXPORT_SYMBOL(rte_driver_name)
const char *
rte_driver_name(const struct rte_driver *driver)
{
	return driver->name;
}

RTE_EXPORT_SYMBOL(rte_dev_bus)
const struct rte_bus *
rte_dev_bus(const struct rte_device *dev)
{
	return dev->bus;
}

RTE_EXPORT_SYMBOL(rte_dev_bus_info)
const char *
rte_dev_bus_info(const struct rte_device *dev)
{
	return dev->bus_info;
}

RTE_EXPORT_SYMBOL(rte_dev_devargs)
const struct rte_devargs *
rte_dev_devargs(const struct rte_device *dev)
{
	return dev->devargs;
}

RTE_EXPORT_SYMBOL(rte_dev_driver)
const struct rte_driver *
rte_dev_driver(const struct rte_device *dev)
{
	return dev->driver;
}

RTE_EXPORT_SYMBOL(rte_dev_name)
const char *
rte_dev_name(const struct rte_device *dev)
{
	return dev->name;
}

RTE_EXPORT_SYMBOL(rte_dev_numa_node)
int
rte_dev_numa_node(const struct rte_device *dev)
{
	return dev->numa_node;
}

/**
 * The device event callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the device name.
 */
struct dev_event_callback {
	TAILQ_ENTRY(dev_event_callback) next; /**< Callbacks list */
	rte_dev_event_cb_fn cb_fn;            /**< Callback address */
	void *cb_arg;                         /**< Callback parameter */
	char *dev_name;	 /**< Callback device name, NULL is for all device */
	uint32_t active;                      /**< Callback is executing */
};

/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(dev_event_cb_list, dev_event_callback);

/* The device event callback list for all registered callbacks. */
static struct dev_event_cb_list dev_event_cbs;

/* spinlock for device callbacks */
static rte_spinlock_t dev_event_lock = RTE_SPINLOCK_INITIALIZER;

struct dev_next_ctx {
	struct rte_dev_iterator *it;
	const char *bus_str;
	const char *cls_str;
};

#define CTX(it, bus_str, cls_str) \
	(&(const struct dev_next_ctx){ \
		.it = it, \
		.bus_str = bus_str, \
		.cls_str = cls_str, \
	})

#define ITCTX(ptr) \
	(((struct dev_next_ctx *)(intptr_t)ptr)->it)

#define BUSCTX(ptr) \
	(((struct dev_next_ctx *)(intptr_t)ptr)->bus_str)

#define CLSCTX(ptr) \
	(((struct dev_next_ctx *)(intptr_t)ptr)->cls_str)

static int cmp_dev_name(const struct rte_device *dev, const void *_name)
{
	const char *name = _name;

	return strcmp(dev->name, name);
}

RTE_EXPORT_SYMBOL(rte_dev_is_probed)
int
rte_dev_is_probed(const struct rte_device *dev)
{
	/* The field driver should be set only when the probe is successful. */
	return dev->driver != NULL;
}

/* helper function to build devargs, caller should free the memory */
static int
build_devargs(const char *busname, const char *devname,
	      const char *drvargs, char **devargs)
{
	int length;

	length = snprintf(NULL, 0, "%s:%s,%s", busname, devname, drvargs);
	if (length < 0)
		return -EINVAL;

	*devargs = malloc(length + 1);
	if (*devargs == NULL)
		return -ENOMEM;

	length = snprintf(*devargs, length + 1, "%s:%s,%s",
			busname, devname, drvargs);
	if (length < 0) {
		free(*devargs);
		return -EINVAL;
	}

	return 0;
}

RTE_EXPORT_SYMBOL(rte_eal_hotplug_add)
int
rte_eal_hotplug_add(const char *busname, const char *devname,
		    const char *drvargs)
{

	char *devargs;
	int ret;

	ret = build_devargs(busname, devname, drvargs, &devargs);
	if (ret != 0)
		return ret;

	ret = rte_dev_probe(devargs);
	free(devargs);

	return ret;
}

/* probe device at local process. */
int
local_dev_probe(const char *devargs, struct rte_device **new_dev)
{
	struct rte_device *dev;
	struct rte_devargs *da;
	int ret;

	*new_dev = NULL;
	da = calloc(1, sizeof(*da));
	if (da == NULL)
		return -ENOMEM;

	ret = rte_devargs_parse(da, devargs);
	if (ret)
		goto err_devarg;

	if (da->bus->plug == NULL) {
		EAL_LOG(ERR, "Function plug not supported by bus (%s)",
			da->bus->name);
		ret = -ENOTSUP;
		goto err_devarg;
	}

	ret = rte_devargs_insert(&da);
	if (ret)
		goto err_devarg;

	/* the rte_devargs will be referenced in the matching rte_device */
	ret = da->bus->scan();
	if (ret)
		goto err_devarg;

	dev = da->bus->find_device(NULL, cmp_dev_name, da->name);
	if (dev == NULL) {
		EAL_LOG(ERR, "Cannot find device (%s)",
			da->name);
		ret = -ENODEV;
		goto err_devarg;
	}
	/* Since there is a matching device, it is now its responsibility
	 * to manage the devargs we've just inserted. From this point
	 * those devargs shouldn't be removed manually anymore.
	 */

	ret = dev->bus->plug(dev);
	if (ret > 0)
		ret = -ENOTSUP;

	if (ret && !rte_dev_is_probed(dev)) { /* if hasn't ever succeeded */
		EAL_LOG(ERR, "Driver cannot attach the device (%s)",
			dev->name);
		return ret;
	}

	*new_dev = dev;
	return ret;

err_devarg:
	if (rte_devargs_remove(da) != 0) {
		rte_devargs_reset(da);
		free(da);
	}
	return ret;
}

RTE_EXPORT_SYMBOL(rte_dev_probe)
int
rte_dev_probe(const char *devargs)
{
	struct eal_dev_mp_req req;
	struct rte_device *dev;
	int ret;

	memset(&req, 0, sizeof(req));
	req.t = EAL_DEV_REQ_TYPE_ATTACH;
	strlcpy(req.devargs, devargs, EAL_DEV_MP_DEV_ARGS_MAX_LEN);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/**
		 * If in secondary process, just send IPC request to
		 * primary process.
		 */
		ret = eal_dev_hotplug_request_to_primary(&req);
		if (ret != 0) {
			EAL_LOG(ERR,
				"Failed to send hotplug request to primary");
			return -ENOMSG;
		}
		if (req.result != 0)
			EAL_LOG(ERR,
				"Failed to hotplug add device");
		return req.result;
	}

	/* attach a shared device from primary start from here: */

	/* primary attach the new device itself. */
	ret = local_dev_probe(devargs, &dev);

	if (ret != 0) {
		EAL_LOG(ERR,
			"Failed to attach device on primary process");

		/**
		 * it is possible that secondary process failed to attached a
		 * device that primary process have during initialization,
		 * so for -EEXIST case, we still need to sync with secondary
		 * process.
		 */
		if (ret != -EEXIST)
			return ret;
	}

	/* primary send attach sync request to secondary. */
	ret = eal_dev_hotplug_request_to_secondary(&req);

	/* if any communication error, we need to rollback. */
	if (ret != 0) {
		EAL_LOG(ERR,
			"Failed to send hotplug add request to secondary");
		ret = -ENOMSG;
		goto rollback;
	}

	/**
	 * if any secondary failed to attach, we need to consider if rollback
	 * is necessary.
	 */
	if (req.result != 0) {
		EAL_LOG(ERR,
			"Failed to attach device on secondary process");
		ret = req.result;

		/* for -EEXIST, we don't need to rollback. */
		if (ret == -EEXIST)
			return ret;
		goto rollback;
	}

	return 0;

rollback:
	req.t = EAL_DEV_REQ_TYPE_ATTACH_ROLLBACK;

	/* primary send rollback request to secondary. */
	if (eal_dev_hotplug_request_to_secondary(&req) != 0)
		EAL_LOG(WARNING,
			"Failed to rollback device attach on secondary."
			"Devices in secondary may not sync with primary");

	/* primary rollback itself. */
	if (local_dev_remove(dev) != 0)
		EAL_LOG(WARNING,
			"Failed to rollback device attach on primary."
			"Devices in secondary may not sync with primary");

	return ret;
}

RTE_EXPORT_SYMBOL(rte_eal_hotplug_remove)
int
rte_eal_hotplug_remove(const char *busname, const char *devname)
{
	struct rte_device *dev;
	struct rte_bus *bus;

	bus = rte_bus_find_by_name(busname);
	if (bus == NULL) {
		EAL_LOG(ERR, "Cannot find bus (%s)", busname);
		return -ENOENT;
	}

	dev = bus->find_device(NULL, cmp_dev_name, devname);
	if (dev == NULL) {
		EAL_LOG(ERR, "Cannot find plugged device (%s)", devname);
		return -EINVAL;
	}

	return rte_dev_remove(dev);
}

/* remove device at local process. */
int
local_dev_remove(struct rte_device *dev)
{
	int ret;

	if (dev->bus->unplug == NULL) {
		EAL_LOG(ERR, "Function unplug not supported by bus (%s)",
			dev->bus->name);
		return -ENOTSUP;
	}

	ret = dev->bus->unplug(dev);
	if (ret) {
		EAL_LOG(ERR, "Driver cannot detach the device (%s)",
			dev->name);
		return (ret < 0) ? ret : -ENOENT;
	}

	return 0;
}

RTE_EXPORT_SYMBOL(rte_dev_remove)
int
rte_dev_remove(struct rte_device *dev)
{
	struct eal_dev_mp_req req;
	char *devargs;
	int ret;

	if (!rte_dev_is_probed(dev)) {
		EAL_LOG(ERR, "Device is not probed");
		return -ENOENT;
	}

	ret = build_devargs(dev->bus->name, dev->name, "", &devargs);
	if (ret != 0)
		return ret;

	memset(&req, 0, sizeof(req));
	req.t = EAL_DEV_REQ_TYPE_DETACH;
	strlcpy(req.devargs, devargs, EAL_DEV_MP_DEV_ARGS_MAX_LEN);
	free(devargs);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/**
		 * If in secondary process, just send IPC request to
		 * primary process.
		 */
		ret = eal_dev_hotplug_request_to_primary(&req);
		if (ret != 0) {
			EAL_LOG(ERR,
				"Failed to send hotplug request to primary");
			return -ENOMSG;
		}
		if (req.result != 0)
			EAL_LOG(ERR,
				"Failed to hotplug remove device");
		return req.result;
	}

	/* detach a device from primary start from here: */

	/* primary send detach sync request to secondary */
	ret = eal_dev_hotplug_request_to_secondary(&req);

	/**
	 * if communication error, we need to rollback, because it is possible
	 * part of the secondary processes still detached it successfully.
	 */
	if (ret != 0) {
		EAL_LOG(ERR,
			"Failed to send device detach request to secondary");
		ret = -ENOMSG;
		goto rollback;
	}

	/**
	 * if any secondary failed to detach, we need to consider if rollback
	 * is necessary.
	 */
	if (req.result != 0) {
		EAL_LOG(ERR,
			"Failed to detach device on secondary process");
		ret = req.result;
		/**
		 * if -ENOENT, we don't need to rollback, since devices is
		 * already detached on secondary process.
		 */
		if (ret != -ENOENT)
			goto rollback;
	}

	/* primary detach the device itself. */
	ret = local_dev_remove(dev);

	/* if primary failed, still need to consider if rollback is necessary */
	if (ret != 0) {
		EAL_LOG(ERR,
			"Failed to detach device on primary process");
		/* if -ENOENT, we don't need to rollback */
		if (ret == -ENOENT)
			return ret;
		goto rollback;
	}

	return 0;

rollback:
	req.t = EAL_DEV_REQ_TYPE_DETACH_ROLLBACK;

	/* primary send rollback request to secondary. */
	if (eal_dev_hotplug_request_to_secondary(&req) != 0)
		EAL_LOG(WARNING,
			"Failed to rollback device detach on secondary."
			"Devices in secondary may not sync with primary");

	return ret;
}

RTE_EXPORT_SYMBOL(rte_dev_event_callback_register)
int
rte_dev_event_callback_register(const char *device_name,
				rte_dev_event_cb_fn cb_fn,
				void *cb_arg)
{
	struct dev_event_callback *event_cb;
	int ret;

	if (!cb_fn)
		return -EINVAL;

	rte_spinlock_lock(&dev_event_lock);

	if (TAILQ_EMPTY(&dev_event_cbs))
		TAILQ_INIT(&dev_event_cbs);

	TAILQ_FOREACH(event_cb, &dev_event_cbs, next) {
		if (event_cb->cb_fn == cb_fn && event_cb->cb_arg == cb_arg) {
			if (device_name == NULL && event_cb->dev_name == NULL)
				break;
			if (device_name == NULL || event_cb->dev_name == NULL)
				continue;
			if (!strcmp(event_cb->dev_name, device_name))
				break;
		}
	}

	/* create a new callback. */
	if (event_cb == NULL) {
		event_cb = malloc(sizeof(struct dev_event_callback));
		if (event_cb != NULL) {
			event_cb->cb_fn = cb_fn;
			event_cb->cb_arg = cb_arg;
			event_cb->active = 0;
			if (!device_name) {
				event_cb->dev_name = NULL;
			} else {
				event_cb->dev_name = strdup(device_name);
				if (event_cb->dev_name == NULL) {
					ret = -ENOMEM;
					goto error;
				}
			}
			TAILQ_INSERT_TAIL(&dev_event_cbs, event_cb, next);
		} else {
			EAL_LOG(ERR,
				"Failed to allocate memory for device "
				"event callback.");
			ret = -ENOMEM;
			goto error;
		}
	} else {
		EAL_LOG(ERR,
			"The callback is already exist, no need "
			"to register again.");
		event_cb = NULL;
		ret = -EEXIST;
		goto error;
	}

	rte_spinlock_unlock(&dev_event_lock);
	return 0;
error:
	free(event_cb);
	rte_spinlock_unlock(&dev_event_lock);
	return ret;
}

RTE_EXPORT_SYMBOL(rte_dev_event_callback_unregister)
int
rte_dev_event_callback_unregister(const char *device_name,
				  rte_dev_event_cb_fn cb_fn,
				  void *cb_arg)
{
	int ret = 0;
	struct dev_event_callback *event_cb, *next;

	if (!cb_fn)
		return -EINVAL;

	rte_spinlock_lock(&dev_event_lock);
	/*walk through the callbacks and remove all that match. */
	for (event_cb = TAILQ_FIRST(&dev_event_cbs); event_cb != NULL;
	     event_cb = next) {

		next = TAILQ_NEXT(event_cb, next);

		if (device_name != NULL && event_cb->dev_name != NULL) {
			if (strcmp(event_cb->dev_name, device_name))
				continue;
		} else if (device_name != NULL) {
			continue;
		}

		/* Remove only matching callback with arg */
		if (event_cb->cb_fn != cb_fn ||
		    (cb_arg != (void *)-1 && event_cb->cb_arg != cb_arg))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (event_cb->active == 0) {
			TAILQ_REMOVE(&dev_event_cbs, event_cb, next);
			free(event_cb->dev_name);
			free(event_cb);
			ret++;
		} else {
			ret = -EAGAIN;
			break;
		}
	}

	/* this callback is not be registered */
	if (ret == 0)
		ret = -ENOENT;

	rte_spinlock_unlock(&dev_event_lock);
	return ret;
}

RTE_EXPORT_SYMBOL(rte_dev_event_callback_process)
void
rte_dev_event_callback_process(const char *device_name,
			       enum rte_dev_event_type event)
{
	struct dev_event_callback *cb_lst;

	if (device_name == NULL)
		return;

	rte_spinlock_lock(&dev_event_lock);

	TAILQ_FOREACH(cb_lst, &dev_event_cbs, next) {
		if (cb_lst->dev_name) {
			if (strcmp(cb_lst->dev_name, device_name))
				continue;
		}
		cb_lst->active = 1;
		rte_spinlock_unlock(&dev_event_lock);
		cb_lst->cb_fn(device_name, event,
				cb_lst->cb_arg);
		rte_spinlock_lock(&dev_event_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&dev_event_lock);
}

RTE_EXPORT_SYMBOL(rte_dev_iterator_init)
int
rte_dev_iterator_init(struct rte_dev_iterator *it,
		      const char *dev_str)
{
	struct rte_devargs devargs = { .bus = NULL };
	struct rte_class *cls = NULL;
	struct rte_bus *bus = NULL;

	/* Having both bus_str and cls_str NULL is illegal,
	 * marking this iterator as invalid unless
	 * everything goes well.
	 */
	it->bus_str = NULL;
	it->cls_str = NULL;

	/* Setting data field implies no malloc in parsing. */
	devargs.data = (void *)(intptr_t)dev_str;
	if (rte_devargs_layers_parse(&devargs, dev_str))
		goto get_out;

	bus = devargs.bus;
	cls = devargs.cls;
	/* The string should have at least
	 * one layer specified.
	 */
	if (bus == NULL && cls == NULL) {
		EAL_LOG(DEBUG, "Either bus or class must be specified.");
		rte_errno = EINVAL;
		goto get_out;
	}
	if (bus != NULL && bus->dev_iterate == NULL) {
		EAL_LOG(DEBUG, "Bus %s not supported", bus->name);
		rte_errno = ENOTSUP;
		goto get_out;
	}
	if (cls != NULL && cls->dev_iterate == NULL) {
		EAL_LOG(DEBUG, "Class %s not supported", cls->name);
		rte_errno = ENOTSUP;
		goto get_out;
	}
	it->bus_str = devargs.bus_str;
	it->cls_str = devargs.cls_str;
	it->dev_str = dev_str;
	it->bus = bus;
	it->cls = cls;
	it->device = NULL;
	it->class_device = NULL;
get_out:
	return -rte_errno;
}

static char *
dev_str_sane_copy(const char *str)
{
	size_t end;
	char *copy;

	end = strcspn(str, ",/");
	if (str[end] == ',') {
		copy = strdup(&str[end + 1]);
	} else {
		/* '/' or '\0' */
		copy = strdup("");
	}
	if (copy == NULL) {
		rte_errno = ENOMEM;
	} else {
		char *slash;

		slash = strchr(copy, '/');
		if (slash != NULL)
			slash[0] = '\0';
	}
	return copy;
}

static int
class_next_dev_cmp(const struct rte_class *cls,
		   const void *ctx)
{
	struct rte_dev_iterator *it;
	const char *cls_str = NULL;
	void *dev;

	if (cls->dev_iterate == NULL)
		return 1;
	it = ITCTX(ctx);
	cls_str = CLSCTX(ctx);
	dev = it->class_device;
	/* it->cls_str != NULL means a class
	 * was specified in the devstr.
	 */
	if (it->cls_str != NULL && cls != it->cls)
		return 1;
	/* If an error occurred previously,
	 * no need to test further.
	 */
	if (rte_errno != 0)
		return -1;
	dev = cls->dev_iterate(dev, cls_str, it);
	it->class_device = dev;
	return dev == NULL;
}

static int
bus_next_dev_cmp(const struct rte_bus *bus,
		 const void *ctx)
{
	struct rte_device *dev = NULL;
	struct rte_class *cls = NULL;
	struct rte_dev_iterator *it;
	const char *bus_str = NULL;

	if (bus->dev_iterate == NULL)
		return 1;
	it = ITCTX(ctx);
	bus_str = BUSCTX(ctx);
	dev = it->device;
	/* it->bus_str != NULL means a bus
	 * was specified in the devstr.
	 */
	if (it->bus_str != NULL && bus != it->bus)
		return 1;
	/* If an error occurred previously,
	 * no need to test further.
	 */
	if (rte_errno != 0)
		return -1;
	if (it->cls_str == NULL) {
		dev = bus->dev_iterate(dev, bus_str, it);
		goto end;
	}
	/* cls_str != NULL */
	if (dev == NULL) {
next_dev_on_bus:
		dev = bus->dev_iterate(dev, bus_str, it);
		it->device = dev;
	}
	if (dev == NULL)
		return 1;
	if (it->cls != NULL)
		cls = TAILQ_PREV(it->cls, rte_class_list, next);
	cls = rte_class_find(cls, class_next_dev_cmp, ctx);
	if (cls != NULL) {
		it->cls = cls;
		goto end;
	}
	goto next_dev_on_bus;
end:
	it->device = dev;
	return dev == NULL;
}
RTE_EXPORT_SYMBOL(rte_dev_iterator_next)
struct rte_device *
rte_dev_iterator_next(struct rte_dev_iterator *it)
{
	struct rte_bus *bus = NULL;
	int old_errno = rte_errno;
	char *bus_str = NULL;
	char *cls_str = NULL;

	rte_errno = 0;
	if (it->bus_str == NULL && it->cls_str == NULL) {
		/* Invalid iterator. */
		rte_errno = EINVAL;
		return NULL;
	}
	if (it->bus != NULL)
		bus = TAILQ_PREV(it->bus, rte_bus_list, next);
	if (it->bus_str != NULL) {
		bus_str = dev_str_sane_copy(it->bus_str);
		if (bus_str == NULL)
			goto out;
	}
	if (it->cls_str != NULL) {
		cls_str = dev_str_sane_copy(it->cls_str);
		if (cls_str == NULL)
			goto out;
	}
	while ((bus = rte_bus_find(bus, bus_next_dev_cmp,
				   CTX(it, bus_str, cls_str)))) {
		if (it->device != NULL) {
			it->bus = bus;
			goto out;
		}
		if (it->bus_str != NULL ||
		    rte_errno != 0)
			break;
	}
	if (rte_errno == 0)
		rte_errno = old_errno;
out:
	free(bus_str);
	free(cls_str);
	return it->device;
}

RTE_EXPORT_SYMBOL(rte_dev_dma_map)
int
rte_dev_dma_map(struct rte_device *dev, void *addr, uint64_t iova,
		size_t len)
{
	if (dev->bus->dma_map == NULL || len == 0) {
		rte_errno = ENOTSUP;
		return -1;
	}
	/* Memory must be registered through rte_extmem_* APIs */
	if (rte_mem_virt2memseg_list(addr) == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	return dev->bus->dma_map(dev, addr, iova, len);
}

RTE_EXPORT_SYMBOL(rte_dev_dma_unmap)
int
rte_dev_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova,
		  size_t len)
{
	if (dev->bus->dma_unmap == NULL || len == 0) {
		rte_errno = ENOTSUP;
		return -1;
	}
	/* Memory must be registered through rte_extmem_* APIs */
	if (rte_mem_virt2memseg_list(addr) == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	return dev->bus->dma_unmap(dev, addr, iova, len);
}
