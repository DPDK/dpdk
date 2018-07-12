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
#include <rte_class.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_debug.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
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

	ret = rte_devargs_parsef(da, "%s:%s,%s",
				 busname, devname, devargs);
	if (ret)
		goto err_devarg;

	ret = rte_devargs_insert(da);
	if (ret)
		goto err_devarg;

	ret = bus->scan();
	if (ret)
		goto err_devarg;

	dev = bus->find_device(NULL, cmp_dev_name, devname);
	if (dev == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find device (%s)\n",
			devname);
		ret = -ENODEV;
		goto err_devarg;
	}

	if (dev->driver != NULL) {
		RTE_LOG(ERR, EAL, "Device is already plugged\n");
		return -EEXIST;
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

	if (dev->driver == NULL) {
		RTE_LOG(ERR, EAL, "Device is already unplugged\n");
		return -ENOENT;
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

__rte_experimental
int
rte_dev_iterator_init(struct rte_dev_iterator *it,
		      const char *dev_str)
{
	struct rte_devargs devargs;
	struct rte_class *cls = NULL;
	struct rte_bus *bus = NULL;

	/* Having both bus_str and cls_str NULL is illegal,
	 * marking this iterator as invalid unless
	 * everything goes well.
	 */
	it->bus_str = NULL;
	it->cls_str = NULL;

	devargs.data = dev_str;
	if (rte_devargs_layers_parse(&devargs, dev_str))
		goto get_out;

	bus = devargs.bus;
	cls = devargs.cls;
	/* The string should have at least
	 * one layer specified.
	 */
	if (bus == NULL && cls == NULL) {
		RTE_LOG(ERR, EAL,
			"Either bus or class must be specified.\n");
		rte_errno = EINVAL;
		goto get_out;
	}
	if (bus != NULL && bus->dev_iterate == NULL) {
		RTE_LOG(ERR, EAL, "Bus %s not supported\n", bus->name);
		rte_errno = ENOTSUP;
		goto get_out;
	}
	if (cls != NULL && cls->dev_iterate == NULL) {
		RTE_LOG(ERR, EAL, "Class %s not supported\n", cls->name);
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
__rte_experimental
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
