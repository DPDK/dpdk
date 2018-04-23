/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

/* This file manages the list of devices and their arguments, as given
 * by the user at startup
 *
 * Code here should not call rte_log since the EAL environment
 * may not be initialized.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <rte_compat.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_tailq.h>
#include "eal_private.h"

/** user device double-linked queue type definition */
TAILQ_HEAD(rte_devargs_list, rte_devargs);

/** Global list of user devices */
struct rte_devargs_list devargs_list =
	TAILQ_HEAD_INITIALIZER(devargs_list);

int
rte_eal_parse_devargs_str(const char *devargs_str,
			char **drvname, char **drvargs)
{
	char *sep;

	if ((devargs_str) == NULL || (drvname) == NULL || (drvargs == NULL))
		return -1;

	*drvname = strdup(devargs_str);
	if (*drvname == NULL)
		return -1;

	/* set the first ',' to '\0' to split name and arguments */
	sep = strchr(*drvname, ',');
	if (sep != NULL) {
		sep[0] = '\0';
		*drvargs = strdup(sep + 1);
	} else {
		*drvargs = strdup("");
	}

	if (*drvargs == NULL) {
		free(*drvname);
		*drvname = NULL;
		return -1;
	}
	return 0;
}

static int
bus_name_cmp(const struct rte_bus *bus, const void *name)
{
	return strncmp(bus->name, name, strlen(bus->name));
}

int __rte_experimental
rte_devargs_parse(struct rte_devargs *da, const char *format, ...)
{
	struct rte_bus *bus = NULL;
	va_list ap;
	va_start(ap, format);
	char dev[vsnprintf(NULL, 0, format, ap) + 1];
	const char *devname;
	const size_t maxlen = sizeof(da->name);
	size_t i;

	va_end(ap);
	if (da == NULL)
		return -EINVAL;

	va_start(ap, format);
	vsnprintf(dev, sizeof(dev), format, ap);
	va_end(ap);
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
			fprintf(stderr, "WARNING: Parsing \"%s\": device name should be shorter than %zu\n",
				dev, maxlen);
			da->name[i - 1] = '\0';
			return -EINVAL;
		}
	}
	da->name[i] = '\0';
	if (bus == NULL) {
		bus = rte_bus_find_by_device_name(da->name);
		if (bus == NULL) {
			fprintf(stderr, "ERROR: failed to parse device \"%s\"\n",
				da->name);
			return -EFAULT;
		}
	}
	da->bus = bus;
	/* Parse eventual device arguments */
	if (devname[i] == ',')
		da->args = strdup(&devname[i + 1]);
	else
		da->args = strdup("");
	if (da->args == NULL) {
		fprintf(stderr, "ERROR: not enough memory to parse arguments\n");
		return -ENOMEM;
	}
	return 0;
}

int __rte_experimental
rte_devargs_insert(struct rte_devargs *da)
{
	int ret;

	ret = rte_devargs_remove(da->bus->name, da->name);
	if (ret < 0)
		return ret;
	TAILQ_INSERT_TAIL(&devargs_list, da, next);
	return 0;
}

/* store a whitelist parameter for later parsing */
__rte_experimental
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

	if (rte_devargs_parse(devargs, "%s", dev))
		goto fail;
	devargs->type = devtype;
	bus = devargs->bus;
	if (devargs->type == RTE_DEVTYPE_BLACKLISTED_PCI)
		devargs->policy = RTE_DEV_BLACKLISTED;
	if (bus->conf.scan_mode == RTE_BUS_SCAN_UNDEFINED) {
		if (devargs->policy == RTE_DEV_WHITELISTED)
			bus->conf.scan_mode = RTE_BUS_SCAN_WHITELIST;
		else if (devargs->policy == RTE_DEV_BLACKLISTED)
			bus->conf.scan_mode = RTE_BUS_SCAN_BLACKLIST;
	}
	TAILQ_INSERT_TAIL(&devargs_list, devargs, next);
	return 0;

fail:
	if (devargs) {
		free(devargs->args);
		free(devargs);
	}

	return -1;
}

int __rte_experimental
rte_devargs_remove(const char *busname, const char *devname)
{
	struct rte_devargs *d;
	void *tmp;

	TAILQ_FOREACH_SAFE(d, &devargs_list, next, tmp) {
		if (strcmp(d->bus->name, busname) == 0 &&
		    strcmp(d->name, devname) == 0) {
			TAILQ_REMOVE(&devargs_list, d, next);
			free(d->args);
			free(d);
			return 0;
		}
	}
	return 1;
}

/* count the number of devices of a specified type */
__rte_experimental
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
__rte_experimental
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
__rte_experimental
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
