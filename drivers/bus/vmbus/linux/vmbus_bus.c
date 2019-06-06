/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <rte_eal.h>
#include <rte_uuid.h>
#include <rte_tailq.h>
#include <rte_log.h>
#include <rte_devargs.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_bus_vmbus.h>

#include "eal_filesystem.h"
#include "private.h"

/** Pathname of VMBUS devices directory. */
#define SYSFS_VMBUS_DEVICES "/sys/bus/vmbus/devices"

/*
 * GUID associated with network devices
 * {f8615163-df3e-46c5-913f-f2d2f965ed0e}
 */
static const rte_uuid_t vmbus_nic_uuid = {
	0xf8, 0x61, 0x51, 0x63,
	0xdf, 0x3e,
	0x46, 0xc5,
	0x91, 0x3f,
	0xf2, 0xd2, 0xf9, 0x65, 0xed, 0xe
};

extern struct rte_vmbus_bus rte_vmbus_bus;

/* Read sysfs file to get UUID */
static int
parse_sysfs_uuid(const char *filename, rte_uuid_t uu)
{
	char buf[BUFSIZ];
	char *cp, *in = buf;
	FILE *f;

	f = fopen(filename, "r");
	if (f == NULL) {
		VMBUS_LOG(ERR, "cannot open sysfs value %s: %s",
			  filename, strerror(errno));
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		VMBUS_LOG(ERR, "cannot read sysfs value %s",
				filename);
		fclose(f);
		return -1;
	}
	fclose(f);

	cp = strchr(buf, '\n');
	if (cp)
		*cp = '\0';

	/* strip { } notation */
	if (buf[0] == '{') {
		in = buf + 1;
		cp = strchr(in, '}');
		if (cp)
			*cp = '\0';
	}

	if (rte_uuid_parse(in, uu) < 0) {
		VMBUS_LOG(ERR, "%s %s not a valid UUID",
			filename, buf);
		return -1;
	}

	return 0;
}

static int
get_sysfs_string(const char *filename, char *buf, size_t buflen)
{
	char *cp;
	FILE *f;

	f = fopen(filename, "r");
	if (f == NULL) {
		VMBUS_LOG(ERR, "cannot open sysfs value %s:%s",
			  filename, strerror(errno));
		return -1;
	}

	if (fgets(buf, buflen, f) == NULL) {
		VMBUS_LOG(ERR, "cannot read sysfs value %s",
				filename);
		fclose(f);
		return -1;
	}
	fclose(f);

	/* remove trailing newline */
	cp = memchr(buf, '\n', buflen);
	if (cp)
		*cp = '\0';

	return 0;
}

static int
vmbus_get_uio_dev(const struct rte_vmbus_device *dev,
		  char *dstbuf, size_t buflen)
{
	char dirname[PATH_MAX];
	unsigned int uio_num;
	struct dirent *e;
	DIR *dir;

	/* Assume recent kernel where uio is in uio/uioX */
	snprintf(dirname, sizeof(dirname),
		 SYSFS_VMBUS_DEVICES "/%s/uio", dev->device.name);

	dir = opendir(dirname);
	if (dir == NULL)
		return -1; /* Not a UIO device */

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		const int prefix_len = 3;
		char *endptr;

		if (strncmp(e->d_name, "uio", prefix_len) != 0)
			continue;

		/* try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + prefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + prefix_len)) {
			snprintf(dstbuf, buflen, "%s/uio%u", dirname, uio_num);
			break;
		}
	}
	closedir(dir);

	if (e == NULL)
		return -1;

	return uio_num;
}

/* Check map names with kernel names */
static const char *map_names[VMBUS_MAX_RESOURCE] = {
	[HV_TXRX_RING_MAP] = "txrx_rings",
	[HV_INT_PAGE_MAP]  = "int_page",
	[HV_MON_PAGE_MAP]  = "monitor_page",
	[HV_RECV_BUF_MAP]  = "recv:",
	[HV_SEND_BUF_MAP]  = "send:",
};


/* map the resources of a vmbus device in virtual memory */
int
rte_vmbus_map_device(struct rte_vmbus_device *dev)
{
	char uioname[PATH_MAX], filename[PATH_MAX];
	char dirname[PATH_MAX], mapname[64];
	int i;

	dev->uio_num = vmbus_get_uio_dev(dev, uioname, sizeof(uioname));
	if (dev->uio_num < 0) {
		VMBUS_LOG(DEBUG, "Not managed by UIO driver, skipped");
		return 1;
	}

	/* Extract resource value */
	for (i = 0; i < VMBUS_MAX_RESOURCE; i++) {
		struct rte_mem_resource *res = &dev->resource[i];
		unsigned long len, gpad = 0;
		char *cp;

		snprintf(dirname, sizeof(dirname),
			 "%s/maps/map%d", uioname, i);

		snprintf(filename, sizeof(filename),
			 "%s/name", dirname);

		if (get_sysfs_string(filename, mapname, sizeof(mapname)) < 0) {
			VMBUS_LOG(ERR, "could not read %s", filename);
			return -1;
		}

		if (strncmp(map_names[i], mapname, strlen(map_names[i])) != 0) {
			VMBUS_LOG(ERR,
				"unexpected resource %s (expected %s)",
				mapname, map_names[i]);
			return -1;
		}

		snprintf(filename, sizeof(filename),
			 "%s/size", dirname);
		if (eal_parse_sysfs_value(filename, &len) < 0) {
			VMBUS_LOG(ERR,
				"could not read %s", filename);
			return -1;
		}
		res->len = len;

		/* both send and receive buffers have gpad in name */
		cp = memchr(mapname, ':', sizeof(mapname));
		if (cp)
			gpad = strtoul(cp+1, NULL, 0);

		/* put the GPAD value in physical address */
		res->phys_addr = gpad;
	}

	return vmbus_uio_map_resource(dev);
}

void
rte_vmbus_unmap_device(struct rte_vmbus_device *dev)
{
	vmbus_uio_unmap_resource(dev);
}

/* Scan one vmbus sysfs entry, and fill the devices list from it. */
static int
vmbus_scan_one(const char *name)
{
	struct rte_vmbus_device *dev, *dev2;
	char filename[PATH_MAX];
	char dirname[PATH_MAX];
	unsigned long tmp;

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL)
		return -1;

	dev->device.bus = &rte_vmbus_bus.bus;
	dev->device.name = strdup(name);
	if (!dev->device.name)
		goto error;

	/* sysfs base directory
	 *   /sys/bus/vmbus/devices/7a08391f-f5a0-4ac0-9802-d13fd964f8df
	 * or on older kernel
	 *   /sys/bus/vmbus/devices/vmbus_1
	 */
	snprintf(dirname, sizeof(dirname), "%s/%s",
		 SYSFS_VMBUS_DEVICES, name);

	/* get device class  */
	snprintf(filename, sizeof(filename), "%s/class_id", dirname);
	if (parse_sysfs_uuid(filename, dev->class_id) < 0)
		goto error;

	/* skip non-network devices */
	if (rte_uuid_compare(dev->class_id, vmbus_nic_uuid) != 0) {
		free(dev);
		return 0;
	}

	/* get device id */
	snprintf(filename, sizeof(filename), "%s/device_id", dirname);
	if (parse_sysfs_uuid(filename, dev->device_id) < 0)
		goto error;

	/* get relid */
	snprintf(filename, sizeof(filename), "%s/id", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0)
		goto error;
	dev->relid = tmp;

	/* get monitor id */
	snprintf(filename, sizeof(filename), "%s/monitor_id", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0)
		goto error;
	dev->monitor_id = tmp;

	/* get numa node (if present) */
	snprintf(filename, sizeof(filename), "%s/numa_node",
		 dirname);

	if (access(filename, R_OK) == 0) {
		if (eal_parse_sysfs_value(filename, &tmp) < 0)
			goto error;
		dev->device.numa_node = tmp;
	} else {
		/* if no NUMA support, set default to 0 */
		dev->device.numa_node = SOCKET_ID_ANY;
	}

	dev->device.devargs = vmbus_devargs_lookup(dev);

	/* device is valid, add in list (sorted) */
	VMBUS_LOG(DEBUG, "Adding vmbus device %s", name);

	TAILQ_FOREACH(dev2, &rte_vmbus_bus.device_list, next) {
		int ret;

		ret = rte_uuid_compare(dev->device_id, dev2->device_id);
		if (ret > 0)
			continue;

		if (ret < 0) {
			vmbus_insert_device(dev2, dev);
		} else { /* already registered */
			VMBUS_LOG(NOTICE,
				"%s already registered", name);
			free(dev);
		}
		return 0;
	}

	vmbus_add_device(dev);
	return 0;
error:
	VMBUS_LOG(DEBUG, "failed");

	free(dev);
	return -1;
}

/*
 * Scan the content of the vmbus, and the devices in the devices list
 */
int
rte_vmbus_scan(void)
{
	struct dirent *e;
	DIR *dir;

	dir = opendir(SYSFS_VMBUS_DEVICES);
	if (dir == NULL) {
		if (errno == ENOENT)
			return 0;

		VMBUS_LOG(ERR, "opendir %s failed: %s",
			  SYSFS_VMBUS_DEVICES, strerror(errno));
		return -1;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		if (vmbus_scan_one(e->d_name) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

void rte_vmbus_irq_mask(struct rte_vmbus_device *device)
{
	vmbus_uio_irq_control(device, 1);
}

void rte_vmbus_irq_unmask(struct rte_vmbus_device *device)
{
	vmbus_uio_irq_control(device, 0);
}

int rte_vmbus_irq_read(struct rte_vmbus_device *device)
{
	return vmbus_uio_irq_read(device);
}
