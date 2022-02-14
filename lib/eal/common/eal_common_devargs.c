/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

/* This file manages the list of devices and their arguments, as given
 * by the user at startup
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <rte_bus.h>
#include <rte_class.h>
#include <rte_compat.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_tailq.h>
#include <rte_string_fns.h>
#include "eal_private.h"

/** user device double-linked queue type definition */
TAILQ_HEAD(rte_devargs_list, rte_devargs);

/** Global list of user devices */
static struct rte_devargs_list devargs_list =
	TAILQ_HEAD_INITIALIZER(devargs_list);

/* Resolve devargs name from bus arguments. */
static int
devargs_bus_parse_default(struct rte_devargs *devargs,
			  struct rte_kvargs *bus_args)
{
	const char *name;

	/* Parse devargs name from bus key-value list. */
	name = rte_kvargs_get(bus_args, "name");
	if (name == NULL) {
		RTE_LOG(DEBUG, EAL, "devargs name not found: %s\n",
			devargs->data);
		return 0;
	}
	if (rte_strscpy(devargs->name, name, sizeof(devargs->name)) < 0) {
		RTE_LOG(ERR, EAL, "devargs name too long: %s\n",
			devargs->data);
		return -E2BIG;
	}
	return 0;
}

int
rte_devargs_layers_parse(struct rte_devargs *devargs,
			 const char *devstr)
{
	struct {
		const char *key;
		const char *str;
		struct rte_kvargs *kvlist;
	} layers[] = {
		{ RTE_DEVARGS_KEY_BUS "=",    NULL, NULL, },
		{ RTE_DEVARGS_KEY_CLASS "=",  NULL, NULL, },
		{ RTE_DEVARGS_KEY_DRIVER "=", NULL, NULL, },
	};
	struct rte_kvargs_pair *kv = NULL;
	struct rte_kvargs *bus_kvlist = NULL;
	char *s;
	size_t nblayer = 0;
	size_t i;
	int ret = 0;
	bool allocated_data = false;

	/* If the devargs points the devstr
	 * as source data, then it should not allocate
	 * anything and keep referring only to it.
	 */
	if (devargs->data != devstr) {
		devargs->data = strdup(devstr);
		if (devargs->data == NULL) {
			RTE_LOG(ERR, EAL, "OOM\n");
			ret = -ENOMEM;
			goto get_out;
		}
		allocated_data = true;
	}
	s = devargs->data;

	while (s != NULL) {
		if (nblayer > RTE_DIM(layers)) {
			ret = -E2BIG;
			goto get_out;
		}
		layers[nblayer].str = s;

		/* Locate next layer starts with valid layer key. */
		while (s != NULL) {
			s = strchr(s, '/');
			if (s == NULL)
				break;
			for (i = 0; i < RTE_DIM(layers); i++) {
				if (strncmp(s + 1, layers[i].key,
					    strlen(layers[i].key)) == 0) {
					*s = '\0';
					break;
				}
			}
			s++;
			if (i < RTE_DIM(layers))
				break;
		}

		layers[nblayer].kvlist = rte_kvargs_parse
				(layers[nblayer].str, NULL);
		if (layers[nblayer].kvlist == NULL) {
			ret = -EINVAL;
			goto get_out;
		}

		nblayer++;
	}

	/* Parse each sub-list. */
	for (i = 0; i < RTE_DIM(layers); i++) {
		if (layers[i].kvlist == NULL)
			continue;
		kv = &layers[i].kvlist->pairs[0];
		if (kv->key == NULL)
			continue;
		if (strcmp(kv->key, RTE_DEVARGS_KEY_BUS) == 0) {
			bus_kvlist = layers[i].kvlist;
			devargs->bus_str = layers[i].str;
			devargs->bus = rte_bus_find_by_name(kv->value);
			if (devargs->bus == NULL) {
				RTE_LOG(ERR, EAL, "Could not find bus \"%s\"\n",
					kv->value);
				ret = -EFAULT;
				goto get_out;
			}
		} else if (strcmp(kv->key, RTE_DEVARGS_KEY_CLASS) == 0) {
			devargs->cls_str = layers[i].str;
			devargs->cls = rte_class_find_by_name(kv->value);
			if (devargs->cls == NULL) {
				RTE_LOG(ERR, EAL, "Could not find class \"%s\"\n",
					kv->value);
				ret = -EFAULT;
				goto get_out;
			}
		} else if (strcmp(kv->key, RTE_DEVARGS_KEY_DRIVER) == 0) {
			devargs->drv_str = layers[i].str;
			continue;
		}
	}

	/* Resolve devargs name. */
	if (devargs->bus != NULL && devargs->bus->devargs_parse != NULL)
		ret = devargs->bus->devargs_parse(devargs);
	else if (bus_kvlist != NULL)
		ret = devargs_bus_parse_default(devargs, bus_kvlist);

get_out:
	for (i = 0; i < RTE_DIM(layers); i++) {
		if (layers[i].kvlist)
			rte_kvargs_free(layers[i].kvlist);
	}
	if (ret != 0) {
		if (allocated_data) {
			/* Free duplicated data. */
			free(devargs->data);
			devargs->data = NULL;
		}
		rte_errno = -ret;
	}
	return ret;
}

static int
bus_name_cmp(const struct rte_bus *bus, const void *name)
{
	return strncmp(bus->name, name, strlen(bus->name));
}

int
rte_devargs_parse(struct rte_devargs *da, const char *dev)
{
	struct rte_bus *bus = NULL;
	const char *devname;
	const size_t maxlen = sizeof(da->name);
	size_t i;

	if (da == NULL)
		return -EINVAL;
	memset(da, 0, sizeof(*da));

	/* First parse according global device syntax. */
	if (rte_devargs_layers_parse(da, dev) == 0) {
		if (da->bus != NULL || da->cls != NULL)
			return 0;
		rte_devargs_reset(da);
	}

	/* Otherwise fallback to legacy syntax: */

	/* Retrieve eventual bus info */
	do {
		devname = dev;
		bus = rte_bus_find(bus, bus_name_cmp, dev);
		if (bus == NULL)
			break;
		devname = dev + strlen(bus->name) + 1;
		if (rte_bus_find_by_device_name(devname) == bus)
			break;
	} while (1);
	/* Store device name */
	i = 0;
	while (devname[i] != '\0' && devname[i] != ',') {
		da->name[i] = devname[i];
		i++;
		if (i == maxlen) {
			RTE_LOG(WARNING, EAL, "Parsing \"%s\": device name should be shorter than %zu\n",
				dev, maxlen);
			da->name[i - 1] = '\0';
			return -EINVAL;
		}
	}
	da->name[i] = '\0';
	if (bus == NULL) {
		bus = rte_bus_find_by_device_name(da->name);
		if (bus == NULL) {
			RTE_LOG(ERR, EAL, "failed to parse device \"%s\"\n",
				da->name);
			return -EFAULT;
		}
	}
	da->bus = bus;
	/* Parse eventual device arguments */
	if (devname[i] == ',')
		da->data = strdup(&devname[i + 1]);
	else
		da->data = strdup("");
	if (da->data == NULL) {
		RTE_LOG(ERR, EAL, "not enough memory to parse arguments\n");
		return -ENOMEM;
	}
	da->drv_str = da->data;
	return 0;
}

int
rte_devargs_parsef(struct rte_devargs *da, const char *format, ...)
{
	va_list ap;
	int len;
	char *dev;
	int ret;

	if (da == NULL)
		return -EINVAL;

	va_start(ap, format);
	len = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	if (len < 0)
		return -EINVAL;

	len += 1;
	dev = calloc(1, (size_t)len);
	if (dev == NULL) {
		RTE_LOG(ERR, EAL, "not enough memory to parse device\n");
		return -ENOMEM;
	}

	va_start(ap, format);
	vsnprintf(dev, (size_t)len, format, ap);
	va_end(ap);

	ret = rte_devargs_parse(da, dev);

	free(dev);
	return ret;
}

void
rte_devargs_reset(struct rte_devargs *da)
{
	if (da == NULL)
		return;
	if (da->data)
		free(da->data);
	da->data = NULL;
}

int
rte_devargs_insert(struct rte_devargs **da)
{
	struct rte_devargs *listed_da;
	void *tmp;

	if (*da == NULL || (*da)->bus == NULL)
		return -1;

	RTE_TAILQ_FOREACH_SAFE(listed_da, &devargs_list, next, tmp) {
		if (listed_da == *da)
			/* devargs already in the list */
			return 0;
		if (strcmp(listed_da->bus->name, (*da)->bus->name) == 0 &&
				strcmp(listed_da->name, (*da)->name) == 0) {
			/* device already in devargs list, must be updated */
			(*da)->next = listed_da->next;
			rte_devargs_reset(listed_da);
			*listed_da = **da;
			/* replace provided devargs with found one */
			free(*da);
			*da = listed_da;
			return 0;
		}
	}
	/* new device in the list */
	TAILQ_INSERT_TAIL(&devargs_list, *da, next);
	return 0;
}

/* store in allowed list parameter for later parsing */
int
rte_devargs_add(enum rte_devtype devtype, const char *devargs_str)
{
	struct rte_devargs *devargs = NULL;
	struct rte_bus *bus = NULL;
	const char *dev = devargs_str;

	/* use calloc instead of rte_zmalloc as it's called early at init */
	devargs = calloc(1, sizeof(*devargs));
	if (devargs == NULL)
		goto fail;

	if (rte_devargs_parse(devargs, dev))
		goto fail;
	devargs->type = devtype;
	bus = devargs->bus;
	if (devargs->type == RTE_DEVTYPE_BLOCKED)
		devargs->policy = RTE_DEV_BLOCKED;
	if (bus->conf.scan_mode == RTE_BUS_SCAN_UNDEFINED) {
		if (devargs->policy == RTE_DEV_ALLOWED)
			bus->conf.scan_mode = RTE_BUS_SCAN_ALLOWLIST;
		else if (devargs->policy == RTE_DEV_BLOCKED)
			bus->conf.scan_mode = RTE_BUS_SCAN_BLOCKLIST;
	}
	TAILQ_INSERT_TAIL(&devargs_list, devargs, next);
	return 0;

fail:
	if (devargs) {
		rte_devargs_reset(devargs);
		free(devargs);
	}

	return -1;
}

int
rte_devargs_remove(struct rte_devargs *devargs)
{
	struct rte_devargs *d;
	void *tmp;

	if (devargs == NULL || devargs->bus == NULL)
		return -1;

	RTE_TAILQ_FOREACH_SAFE(d, &devargs_list, next, tmp) {
		if (strcmp(d->bus->name, devargs->bus->name) == 0 &&
		    strcmp(d->name, devargs->name) == 0) {
			TAILQ_REMOVE(&devargs_list, d, next);
			rte_devargs_reset(d);
			free(d);
			return 0;
		}
	}
	return 1;
}

/* count the number of devices of a specified type */
unsigned int
rte_devargs_type_count(enum rte_devtype devtype)
{
	struct rte_devargs *devargs;
	unsigned int count = 0;

	TAILQ_FOREACH(devargs, &devargs_list, next) {
		if (devargs->type != devtype)
			continue;
		count++;
	}
	return count;
}

/* dump the user devices on the console */
void
rte_devargs_dump(FILE *f)
{
	struct rte_devargs *devargs;

	fprintf(f, "User device list:\n");
	TAILQ_FOREACH(devargs, &devargs_list, next) {
		fprintf(f, "  [%s]: %s %s\n",
			(devargs->bus ? devargs->bus->name : "??"),
			devargs->name, devargs->args);
	}
}

/* bus-aware rte_devargs iterator. */
struct rte_devargs *
rte_devargs_next(const char *busname, const struct rte_devargs *start)
{
	struct rte_devargs *da;

	if (start != NULL)
		da = TAILQ_NEXT(start, next);
	else
		da = TAILQ_FIRST(&devargs_list);
	while (da != NULL) {
		if (busname == NULL ||
		    (strcmp(busname, da->bus->name) == 0))
			return da;
		da = TAILQ_NEXT(da, next);
	}
	return NULL;
}
