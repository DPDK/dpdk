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
#include <rte_spinlock.h>
#include <rte_malloc.h>

#include "eal_private.h"

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

int __rte_experimental rte_eal_hotplug_add(const char *busname, const char *devname,
			const char *devargs)
{
	struct rte_bus *bus;
	struct rte_device *dev;
	struct rte_devargs *da;
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

	da = calloc(1, sizeof(*da));
	if (da == NULL)
		return -ENOMEM;

	ret = rte_devargs_parse(da, "%s:%s,%s",
				    busname, devname, devargs);
	if (ret)
		goto err_devarg;

	ret = rte_devargs_insert(da);
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
	return 0;

err_devarg:
	if (rte_devargs_remove(busname, devname)) {
		free(da->args);
		free(da);
	}
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
	rte_devargs_remove(busname, devname);
	return ret;
}

int __rte_experimental
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
			RTE_LOG(ERR, EAL,
				"Failed to allocate memory for device "
				"event callback.");
			ret = -ENOMEM;
			goto error;
		}
	} else {
		RTE_LOG(ERR, EAL,
			"The callback is already exist, no need "
			"to register again.\n");
		ret = -EEXIST;
	}

	rte_spinlock_unlock(&dev_event_lock);
	return 0;
error:
	free(event_cb);
	rte_spinlock_unlock(&dev_event_lock);
	return ret;
}

int __rte_experimental
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
			if (!strcmp(event_cb->dev_name, device_name)) {
				if (event_cb->cb_fn != cb_fn ||
				    (cb_arg != (void *)-1 &&
				    event_cb->cb_arg != cb_arg))
					continue;
			}
		} else if (device_name != NULL) {
			continue;
		}

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (event_cb->active == 0) {
			TAILQ_REMOVE(&dev_event_cbs, event_cb, next);
			free(event_cb);
			ret++;
		} else {
			continue;
		}
	}
	rte_spinlock_unlock(&dev_event_lock);
	return ret;
}

void
dev_callback_process(char *device_name, enum rte_dev_event_type event)
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
