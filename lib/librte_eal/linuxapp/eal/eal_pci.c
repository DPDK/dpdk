/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <dirent.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "eal_filesystem.h"
#include "eal_private.h"
#include "eal_pci_init.h"

/**
 * @file
 * PCI probing under linux
 *
 * This code is used to simulate a PCI probe by parsing information in sysfs.
 * When a registered device matches a driver, it is then initialized with
 * IGB_UIO driver (or doesn't initialize, if the device wasn't bound to it).
 */

/* unbind kernel driver for this device */
int
pci_unbind_kernel_driver(struct rte_pci_device *dev)
{
	int n;
	FILE *f;
	char filename[PATH_MAX];
	char buf[BUFSIZ];
	struct rte_pci_addr *loc = &dev->addr;

	/* open /sys/bus/pci/devices/AAAA:BB:CC.D/driver */
	snprintf(filename, sizeof(filename),
		"%s/" PCI_PRI_FMT "/driver/unbind", pci_get_sysfs_path(),
		loc->domain, loc->bus, loc->devid, loc->function);

	f = fopen(filename, "w");
	if (f == NULL) /* device was not bound */
		return 0;

	n = snprintf(buf, sizeof(buf), PCI_PRI_FMT "\n",
	             loc->domain, loc->bus, loc->devid, loc->function);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "%s(): snprintf failed\n", __func__);
		goto error;
	}
	if (fwrite(buf, n, 1, f) == 0) {
		RTE_LOG(ERR, EAL, "%s(): could not write to %s\n", __func__,
				filename);
		goto error;
	}

	fclose(f);
	return 0;

error:
	fclose(f);
	return -1;
}

static int
pci_get_kernel_driver_by_path(const char *filename, char *dri_name)
{
	int count;
	char path[PATH_MAX];
	char *name;

	if (!filename || !dri_name)
		return -1;

	count = readlink(filename, path, PATH_MAX);
	if (count >= PATH_MAX)
		return -1;

	/* For device does not have a driver */
	if (count < 0)
		return 1;

	path[count] = '\0';

	name = strrchr(path, '/');
	if (name) {
		strncpy(dri_name, name + 1, strlen(name + 1) + 1);
		return 0;
	}

	return -1;
}

/* Map pci device */
int
rte_eal_pci_map_device(struct rte_pci_device *dev)
{
	int ret = -1;

	/* try mapping the NIC resources using VFIO if it exists */
	switch (dev->kdrv) {
	case RTE_KDRV_VFIO:
#ifdef VFIO_PRESENT
		if (pci_vfio_is_enabled())
			ret = pci_vfio_map_resource(dev);
#endif
		break;
	case RTE_KDRV_IGB_UIO:
	case RTE_KDRV_UIO_GENERIC:
		/* map resources for devices that use uio */
		ret = pci_uio_map_resource(dev);
		break;
	default:
		RTE_LOG(DEBUG, EAL,
			"  Not managed by a supported kernel driver, skipped\n");
		ret = 1;
		break;
	}

	return ret;
}

/* Unmap pci device */
void
rte_eal_pci_unmap_device(struct rte_pci_device *dev)
{
	/* try unmapping the NIC resources using VFIO if it exists */
	switch (dev->kdrv) {
	case RTE_KDRV_VFIO:
		RTE_LOG(ERR, EAL, "Hotplug doesn't support vfio yet\n");
		break;
	case RTE_KDRV_IGB_UIO:
	case RTE_KDRV_UIO_GENERIC:
		/* unmap resources for devices that use uio */
		pci_uio_unmap_resource(dev);
		break;
	default:
		RTE_LOG(DEBUG, EAL,
			"  Not managed by a supported kernel driver, skipped\n");
		break;
	}
}

void *
pci_find_max_end_va(void)
{
	const struct rte_memseg *seg = rte_eal_get_physmem_layout();
	const struct rte_memseg *last = seg;
	unsigned i = 0;

	for (i = 0; i < RTE_MAX_MEMSEG; i++, seg++) {
		if (seg->addr == NULL)
			break;

		if (seg->addr > last->addr)
			last = seg;

	}
	return RTE_PTR_ADD(last->addr, last->len);
}

/* parse one line of the "resource" sysfs file (note that the 'line'
 * string is modified)
 */
int
pci_parse_one_sysfs_resource(char *line, size_t len, uint64_t *phys_addr,
	uint64_t *end_addr, uint64_t *flags)
{
	union pci_resource_info {
		struct {
			char *phys_addr;
			char *end_addr;
			char *flags;
		};
		char *ptrs[PCI_RESOURCE_FMT_NVAL];
	} res_info;

	if (rte_strsplit(line, len, res_info.ptrs, 3, ' ') != 3) {
		RTE_LOG(ERR, EAL,
			"%s(): bad resource format\n", __func__);
		return -1;
	}
	errno = 0;
	*phys_addr = strtoull(res_info.phys_addr, NULL, 16);
	*end_addr = strtoull(res_info.end_addr, NULL, 16);
	*flags = strtoull(res_info.flags, NULL, 16);
	if (errno != 0) {
		RTE_LOG(ERR, EAL,
			"%s(): bad resource format\n", __func__);
		return -1;
	}

	return 0;
}

/* parse the "resource" sysfs file */
static int
pci_parse_sysfs_resource(const char *filename, struct rte_pci_device *dev)
{
	FILE *f;
	char buf[BUFSIZ];
	int i;
	uint64_t phys_addr, end_addr, flags;

	f = fopen(filename, "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "Cannot open sysfs resource\n");
		return -1;
	}

	for (i = 0; i<PCI_MAX_RESOURCE; i++) {

		if (fgets(buf, sizeof(buf), f) == NULL) {
			RTE_LOG(ERR, EAL,
				"%s(): cannot read resource\n", __func__);
			goto error;
		}
		if (pci_parse_one_sysfs_resource(buf, sizeof(buf), &phys_addr,
				&end_addr, &flags) < 0)
			goto error;

		if (flags & IORESOURCE_MEM) {
			dev->mem_resource[i].phys_addr = phys_addr;
			dev->mem_resource[i].len = end_addr - phys_addr + 1;
			/* not mapped for now */
			dev->mem_resource[i].addr = NULL;
		}
	}
	fclose(f);
	return 0;

error:
	fclose(f);
	return -1;
}

/* Scan one pci sysfs entry, and fill the devices list from it. */
static int
pci_scan_one(const char *dirname, uint16_t domain, uint8_t bus,
	     uint8_t devid, uint8_t function)
{
	char filename[PATH_MAX];
	unsigned long tmp;
	struct rte_pci_device *dev;
	char driver[PATH_MAX];
	int ret;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return -1;

	memset(dev, 0, sizeof(*dev));
	dev->addr.domain = domain;
	dev->addr.bus = bus;
	dev->addr.devid = devid;
	dev->addr.function = function;

	/* get vendor id */
	snprintf(filename, sizeof(filename), "%s/vendor", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.vendor_id = (uint16_t)tmp;

	/* get device id */
	snprintf(filename, sizeof(filename), "%s/device", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.device_id = (uint16_t)tmp;

	/* get subsystem_vendor id */
	snprintf(filename, sizeof(filename), "%s/subsystem_vendor",
		 dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.subsystem_vendor_id = (uint16_t)tmp;

	/* get subsystem_device id */
	snprintf(filename, sizeof(filename), "%s/subsystem_device",
		 dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.subsystem_device_id = (uint16_t)tmp;

	/* get class_id */
	snprintf(filename, sizeof(filename), "%s/class",
		 dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	/* the least 24 bits are valid: class, subclass, program interface */
	dev->id.class_id = (uint32_t)tmp & RTE_CLASS_ANY_ID;

	/* get max_vfs */
	dev->max_vfs = 0;
	snprintf(filename, sizeof(filename), "%s/max_vfs", dirname);
	if (!access(filename, F_OK) &&
	    eal_parse_sysfs_value(filename, &tmp) == 0)
		dev->max_vfs = (uint16_t)tmp;
	else {
		/* for non igb_uio driver, need kernel version >= 3.8 */
		snprintf(filename, sizeof(filename),
			 "%s/sriov_numvfs", dirname);
		if (!access(filename, F_OK) &&
		    eal_parse_sysfs_value(filename, &tmp) == 0)
			dev->max_vfs = (uint16_t)tmp;
	}

	/* get numa node */
	snprintf(filename, sizeof(filename), "%s/numa_node",
		 dirname);
	if (access(filename, R_OK) != 0) {
		/* if no NUMA support, set default to 0 */
		dev->numa_node = 0;
	} else {
		if (eal_parse_sysfs_value(filename, &tmp) < 0) {
			free(dev);
			return -1;
		}
		dev->numa_node = tmp;
	}

	/* parse resources */
	snprintf(filename, sizeof(filename), "%s/resource", dirname);
	if (pci_parse_sysfs_resource(filename, dev) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse resource\n", __func__);
		free(dev);
		return -1;
	}

	/* parse driver */
	snprintf(filename, sizeof(filename), "%s/driver", dirname);
	ret = pci_get_kernel_driver_by_path(filename, driver);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Fail to get kernel driver\n");
		free(dev);
		return -1;
	}

	if (!ret) {
		if (!strcmp(driver, "vfio-pci"))
			dev->kdrv = RTE_KDRV_VFIO;
		else if (!strcmp(driver, "igb_uio"))
			dev->kdrv = RTE_KDRV_IGB_UIO;
		else if (!strcmp(driver, "uio_pci_generic"))
			dev->kdrv = RTE_KDRV_UIO_GENERIC;
		else
			dev->kdrv = RTE_KDRV_UNKNOWN;
	} else
		dev->kdrv = RTE_KDRV_NONE;

	/* device is valid, add in list (sorted) */
	if (TAILQ_EMPTY(&pci_device_list)) {
		TAILQ_INSERT_TAIL(&pci_device_list, dev, next);
	} else {
		struct rte_pci_device *dev2;
		int ret;

		TAILQ_FOREACH(dev2, &pci_device_list, next) {
			ret = rte_eal_compare_pci_addr(&dev->addr, &dev2->addr);
			if (ret > 0)
				continue;

			if (ret < 0) {
				TAILQ_INSERT_BEFORE(dev2, dev, next);
			} else { /* already registered */
				dev2->kdrv = dev->kdrv;
				dev2->max_vfs = dev->max_vfs;
				memmove(dev2->mem_resource, dev->mem_resource,
					sizeof(dev->mem_resource));
				free(dev);
			}
			return 0;
		}
		TAILQ_INSERT_TAIL(&pci_device_list, dev, next);
	}

	return 0;
}

/*
 * split up a pci address into its constituent parts.
 */
static int
parse_pci_addr_format(const char *buf, int bufsize, uint16_t *domain,
		uint8_t *bus, uint8_t *devid, uint8_t *function)
{
	/* first split on ':' */
	union splitaddr {
		struct {
			char *domain;
			char *bus;
			char *devid;
			char *function;
		};
		char *str[PCI_FMT_NVAL]; /* last element-separator is "." not ":" */
	} splitaddr;

	char *buf_copy = strndup(buf, bufsize);
	if (buf_copy == NULL)
		return -1;

	if (rte_strsplit(buf_copy, bufsize, splitaddr.str, PCI_FMT_NVAL, ':')
			!= PCI_FMT_NVAL - 1)
		goto error;
	/* final split is on '.' between devid and function */
	splitaddr.function = strchr(splitaddr.devid,'.');
	if (splitaddr.function == NULL)
		goto error;
	*splitaddr.function++ = '\0';

	/* now convert to int values */
	errno = 0;
	*domain = (uint16_t)strtoul(splitaddr.domain, NULL, 16);
	*bus = (uint8_t)strtoul(splitaddr.bus, NULL, 16);
	*devid = (uint8_t)strtoul(splitaddr.devid, NULL, 16);
	*function = (uint8_t)strtoul(splitaddr.function, NULL, 10);
	if (errno != 0)
		goto error;

	free(buf_copy); /* free the copy made with strdup */
	return 0;
error:
	free(buf_copy);
	return -1;
}

/*
 * Scan the content of the PCI bus, and the devices in the devices
 * list
 */
int
rte_eal_pci_scan(void)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];
	uint16_t domain;
	uint8_t bus, devid, function;

	dir = opendir(pci_get_sysfs_path());
	if (dir == NULL) {
		RTE_LOG(ERR, EAL, "%s(): opendir failed: %s\n",
			__func__, strerror(errno));
		return -1;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		if (parse_pci_addr_format(e->d_name, sizeof(e->d_name), &domain,
				&bus, &devid, &function) != 0)
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s",
				pci_get_sysfs_path(), e->d_name);
		if (pci_scan_one(dirname, domain, bus, devid, function) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

/* Read PCI config space. */
int rte_eal_pci_read_config(const struct rte_pci_device *device,
			    void *buf, size_t len, off_t offset)
{
	const struct rte_intr_handle *intr_handle = &device->intr_handle;

	switch (intr_handle->type) {
	case RTE_INTR_HANDLE_UIO:
	case RTE_INTR_HANDLE_UIO_INTX:
		return pci_uio_read_config(intr_handle, buf, len, offset);

#ifdef VFIO_PRESENT
	case RTE_INTR_HANDLE_VFIO_MSIX:
	case RTE_INTR_HANDLE_VFIO_MSI:
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		return pci_vfio_read_config(intr_handle, buf, len, offset);
#endif
	default:
		RTE_LOG(ERR, EAL,
			"Unknown handle type of fd %d\n",
					intr_handle->fd);
		return -1;
	}
}

/* Write PCI config space. */
int rte_eal_pci_write_config(const struct rte_pci_device *device,
			     const void *buf, size_t len, off_t offset)
{
	const struct rte_intr_handle *intr_handle = &device->intr_handle;

	switch (intr_handle->type) {
	case RTE_INTR_HANDLE_UIO:
	case RTE_INTR_HANDLE_UIO_INTX:
		return pci_uio_write_config(intr_handle, buf, len, offset);

#ifdef VFIO_PRESENT
	case RTE_INTR_HANDLE_VFIO_MSIX:
	case RTE_INTR_HANDLE_VFIO_MSI:
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		return pci_vfio_write_config(intr_handle, buf, len, offset);
#endif
	default:
		RTE_LOG(ERR, EAL,
			"Unknown handle type of fd %d\n",
					intr_handle->fd);
		return -1;
	}
}

#if defined(RTE_ARCH_X86)
static int
pci_ioport_map(struct rte_pci_device *dev, int bar __rte_unused,
	       struct rte_pci_ioport *p)
{
	uint16_t start, end;
	FILE *fp;
	char *line = NULL;
	char pci_id[16];
	int found = 0;
	size_t linesz;

	snprintf(pci_id, sizeof(pci_id), PCI_PRI_FMT,
		 dev->addr.domain, dev->addr.bus,
		 dev->addr.devid, dev->addr.function);

	fp = fopen("/proc/ioports", "r");
	if (fp == NULL) {
		RTE_LOG(ERR, EAL, "%s(): can't open ioports\n", __func__);
		return -1;
	}

	while (getdelim(&line, &linesz, '\n', fp) > 0) {
		char *ptr = line;
		char *left;
		int n;

		n = strcspn(ptr, ":");
		ptr[n] = 0;
		left = &ptr[n + 1];

		while (*left && isspace(*left))
			left++;

		if (!strncmp(left, pci_id, strlen(pci_id))) {
			found = 1;

			while (*ptr && isspace(*ptr))
				ptr++;

			sscanf(ptr, "%04hx-%04hx", &start, &end);

			break;
		}
	}

	free(line);
	fclose(fp);

	if (!found)
		return -1;

	dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
	p->base = start;
	RTE_LOG(DEBUG, EAL, "PCI Port IO found start=0x%x\n", start);

	return 0;
}
#endif

int
rte_eal_pci_ioport_map(struct rte_pci_device *dev, int bar,
		       struct rte_pci_ioport *p)
{
	int ret = -1;

	switch (dev->kdrv) {
#ifdef VFIO_PRESENT
	case RTE_KDRV_VFIO:
		if (pci_vfio_is_enabled())
			ret = pci_vfio_ioport_map(dev, bar, p);
		break;
#endif
	case RTE_KDRV_IGB_UIO:
		ret = pci_uio_ioport_map(dev, bar, p);
		break;
	case RTE_KDRV_UIO_GENERIC:
#if defined(RTE_ARCH_X86)
		ret = pci_ioport_map(dev, bar, p);
#else
		ret = pci_uio_ioport_map(dev, bar, p);
#endif
		break;
	case RTE_KDRV_NONE:
#if defined(RTE_ARCH_X86)
		ret = pci_ioport_map(dev, bar, p);
#endif
		break;
	default:
		break;
	}

	if (!ret)
		p->dev = dev;

	return ret;
}

void
rte_eal_pci_ioport_read(struct rte_pci_ioport *p,
			void *data, size_t len, off_t offset)
{
	switch (p->dev->kdrv) {
#ifdef VFIO_PRESENT
	case RTE_KDRV_VFIO:
		pci_vfio_ioport_read(p, data, len, offset);
		break;
#endif
	case RTE_KDRV_IGB_UIO:
		pci_uio_ioport_read(p, data, len, offset);
		break;
	case RTE_KDRV_UIO_GENERIC:
		pci_uio_ioport_read(p, data, len, offset);
		break;
	case RTE_KDRV_NONE:
#if defined(RTE_ARCH_X86)
		pci_uio_ioport_read(p, data, len, offset);
#endif
		break;
	default:
		break;
	}
}

void
rte_eal_pci_ioport_write(struct rte_pci_ioport *p,
			 const void *data, size_t len, off_t offset)
{
	switch (p->dev->kdrv) {
#ifdef VFIO_PRESENT
	case RTE_KDRV_VFIO:
		pci_vfio_ioport_write(p, data, len, offset);
		break;
#endif
	case RTE_KDRV_IGB_UIO:
		pci_uio_ioport_write(p, data, len, offset);
		break;
	case RTE_KDRV_UIO_GENERIC:
		pci_uio_ioport_write(p, data, len, offset);
		break;
	case RTE_KDRV_NONE:
#if defined(RTE_ARCH_X86)
		pci_uio_ioport_write(p, data, len, offset);
#endif
		break;
	default:
		break;
	}
}

int
rte_eal_pci_ioport_unmap(struct rte_pci_ioport *p)
{
	int ret = -1;

	switch (p->dev->kdrv) {
#ifdef VFIO_PRESENT
	case RTE_KDRV_VFIO:
		if (pci_vfio_is_enabled())
			ret = pci_vfio_ioport_unmap(p);
		break;
#endif
	case RTE_KDRV_IGB_UIO:
		ret = pci_uio_ioport_unmap(p);
		break;
	case RTE_KDRV_UIO_GENERIC:
#if defined(RTE_ARCH_X86)
		ret = 0;
#else
		ret = pci_uio_ioport_unmap(p);
#endif
		break;
	case RTE_KDRV_NONE:
#if defined(RTE_ARCH_X86)
		ret = 0;
#endif
		break;
	default:
		break;
	}

	return ret;
}

/* Init the PCI EAL subsystem */
int
rte_eal_pci_init(void)
{
	TAILQ_INIT(&pci_driver_list);
	TAILQ_INIT(&pci_device_list);

	/* for debug purposes, PCI can be disabled */
	if (internal_config.no_pci)
		return 0;

	if (rte_eal_pci_scan() < 0) {
		RTE_LOG(ERR, EAL, "%s(): Cannot scan PCI bus\n", __func__);
		return -1;
	}

	return 0;
}
