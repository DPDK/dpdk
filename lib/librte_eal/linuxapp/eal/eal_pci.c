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
#include <sys/mman.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "rte_pci_dev_ids.h"
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
static int
pci_unbind_kernel_driver(struct rte_pci_device *dev)
{
	int n;
	FILE *f;
	char filename[PATH_MAX];
	char buf[BUFSIZ];
	struct rte_pci_addr *loc = &dev->addr;

	/* open /sys/bus/pci/devices/AAAA:BB:CC.D/driver */
	snprintf(filename, sizeof(filename),
	         SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/driver/unbind",
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


/* map a particular resource from a file */
void *
pci_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		 int additional_flags)
{
	void *mapaddr;

	/* Map the PCI memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap(%d, %p, 0x%lx, 0x%lx): %s (%p)\n",
			__func__, fd, requested_addr,
			(unsigned long)size, (unsigned long)offset,
			strerror(errno), mapaddr);
	} else {
		RTE_LOG(DEBUG, EAL, "  PCI memory mapped at %p\n", mapaddr);
	}

	return mapaddr;
}

/* unmap a particular resource */
void
pci_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (munmap(requested_addr, size)) {
		RTE_LOG(ERR, EAL, "%s(): cannot munmap(%p, 0x%lx): %s\n",
			__func__, requested_addr, (unsigned long)size,
			strerror(errno));
	} else
		RTE_LOG(DEBUG, EAL, "  PCI memory unmapped at %p\n",
				requested_addr);
}

/* parse the "resource" sysfs file */
static int
pci_parse_sysfs_resource(const char *filename, struct rte_pci_device *dev)
{
	FILE *f;
	char buf[BUFSIZ];
	union pci_resource_info {
		struct {
			char *phys_addr;
			char *end_addr;
			char *flags;
		};
		char *ptrs[PCI_RESOURCE_FMT_NVAL];
	} res_info;
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

		if (rte_strsplit(buf, sizeof(buf), res_info.ptrs, 3, ' ') != 3) {
			RTE_LOG(ERR, EAL,
				"%s(): bad resource format\n", __func__);
			goto error;
		}
		errno = 0;
		phys_addr = strtoull(res_info.phys_addr, NULL, 16);
		end_addr = strtoull(res_info.end_addr, NULL, 16);
		flags = strtoull(res_info.flags, NULL, 16);
		if (errno != 0) {
			RTE_LOG(ERR, EAL,
				"%s(): bad resource format\n", __func__);
			goto error;
		}

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
		/* if no NUMA support just set node to -1 */
		dev->numa_node = -1;
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
	if (!ret) {
		if (!strcmp(driver, "vfio-pci"))
			dev->kdrv = RTE_KDRV_VFIO;
		else if (!strcmp(driver, "igb_uio"))
			dev->kdrv = RTE_KDRV_IGB_UIO;
		else if (!strcmp(driver, "uio_pci_generic"))
			dev->kdrv = RTE_KDRV_UIO_GENERIC;
		else
			dev->kdrv = RTE_KDRV_UNKNOWN;
	} else if (ret < 0) {
		RTE_LOG(ERR, EAL, "Fail to get kernel driver\n");
		free(dev);
		return -1;
	} else
		dev->kdrv = RTE_KDRV_UNKNOWN;

	/* device is valid, add in list (sorted) */
	if (TAILQ_EMPTY(&pci_device_list)) {
		TAILQ_INSERT_TAIL(&pci_device_list, dev, next);
	}
	else {
		struct rte_pci_device *dev2 = NULL;
		int ret;

		TAILQ_FOREACH(dev2, &pci_device_list, next) {
			ret = rte_eal_compare_pci_addr(&dev->addr, &dev2->addr);
			if (ret > 0)
				continue;
			else if (ret < 0) {
				TAILQ_INSERT_BEFORE(dev2, dev, next);
				return 0;
			} else { /* already registered */
				dev2->kdrv = dev->kdrv;
				dev2->max_vfs = dev->max_vfs;
				memmove(dev2->mem_resource,
					dev->mem_resource,
					sizeof(dev->mem_resource));
				free(dev);
				return 0;
			}
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

	dir = opendir(SYSFS_PCI_DEVICES);
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

		snprintf(dirname, sizeof(dirname), "%s/%s", SYSFS_PCI_DEVICES,
			 e->d_name);
		if (pci_scan_one(dirname, domain, bus, devid, function) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

#ifdef RTE_PCI_CONFIG
static int
pci_config_extended_tag(struct rte_pci_device *dev)
{
	struct rte_pci_addr *loc = &dev->addr;
	char filename[PATH_MAX];
	char buf[BUFSIZ];
	FILE *f;

	/* not configured, let it as is */
	if (strncmp(RTE_PCI_EXTENDED_TAG, "on", 2) != 0 &&
		strncmp(RTE_PCI_EXTENDED_TAG, "off", 3) != 0)
		return 0;

	snprintf(filename, sizeof(filename),
		SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/" "extended_tag",
		loc->domain, loc->bus, loc->devid, loc->function);
	f = fopen(filename, "rw+");
	if (!f)
		return -1;

	fgets(buf, sizeof(buf), f);
	if (strncmp(RTE_PCI_EXTENDED_TAG, "on", 2) == 0) {
		/* enable Extended Tag*/
		if (strncmp(buf, "on", 2) != 0) {
			fseek(f, 0, SEEK_SET);
			fputs("on", f);
		}
	} else {
		/* disable Extended Tag */
		if (strncmp(buf, "off", 3) != 0) {
			fseek(f, 0, SEEK_SET);
			fputs("off", f);
		}
	}
	fclose(f);

	return 0;
}

static int
pci_config_max_read_request_size(struct rte_pci_device *dev)
{
	struct rte_pci_addr *loc = &dev->addr;
	char filename[PATH_MAX];
	char buf[BUFSIZ], param[BUFSIZ];
	FILE *f;
	/* size can be 128, 256, 512, 1024, 2048, 4096 */
	uint32_t max_size = RTE_PCI_MAX_READ_REQUEST_SIZE;

	/* not configured, let it as is */
	if (!max_size)
		return 0;

	snprintf(filename, sizeof(filename),
		SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/" "max_read_request_size",
			loc->domain, loc->bus, loc->devid, loc->function);
	f = fopen(filename, "rw+");
	if (!f)
		return -1;

	fgets(buf, sizeof(buf), f);
	snprintf(param, sizeof(param), "%d", max_size);

	/* check if the size to be set is the same as current */
	if (strcmp(buf, param) == 0) {
		fclose(f);
		return 0;
	}
	fseek(f, 0, SEEK_SET);
	fputs(param, f);
	fclose(f);

	return 0;
}

static void
pci_config_space_set(struct rte_pci_device *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	/* configure extended tag */
	pci_config_extended_tag(dev);

	/* configure max read request size */
	pci_config_max_read_request_size(dev);
}
#endif

static int
pci_map_device(struct rte_pci_device *dev)
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
		RTE_LOG(DEBUG, EAL, "  Not managed by a supported kernel driver,"
			" skipped\n");
		ret = 1;
		break;
	}

	return ret;
}

#ifdef RTE_LIBRTE_EAL_HOTPLUG
static void
pci_unmap_device(struct rte_pci_device *dev)
{
	if (dev == NULL)
		return;

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
		RTE_LOG(DEBUG, EAL, "  Not managed by a supported kernel driver,"
			" skipped\n");
		break;
	}
}
#endif /* RTE_LIBRTE_EAL_HOTPLUG */

/*
 * If vendor/device ID match, call the devinit() function of the
 * driver.
 */
int
rte_eal_pci_probe_one_driver(struct rte_pci_driver *dr, struct rte_pci_device *dev)
{
	int ret;
	struct rte_pci_id *id_table;

	for (id_table = dr->id_table ; id_table->vendor_id != 0; id_table++) {

		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != dev->id.vendor_id &&
				id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != dev->id.device_id &&
				id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id != dev->id.subsystem_vendor_id &&
				id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id != dev->id.subsystem_device_id &&
				id_table->subsystem_device_id != PCI_ANY_ID)
			continue;

		struct rte_pci_addr *loc = &dev->addr;

		RTE_LOG(DEBUG, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
				loc->domain, loc->bus, loc->devid, loc->function,
				dev->numa_node);

		RTE_LOG(DEBUG, EAL, "  probe driver: %x:%x %s\n", dev->id.vendor_id,
				dev->id.device_id, dr->name);

		/* no initialization when blacklisted, return without error */
		if (dev->devargs != NULL &&
			dev->devargs->type == RTE_DEVTYPE_BLACKLISTED_PCI) {
			RTE_LOG(DEBUG, EAL, "  Device is blacklisted, not initializing\n");
			return 1;
		}

		if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) {
#ifdef RTE_PCI_CONFIG
			/*
			 * Set PCIe config space for high performance.
			 * Return value can be ignored.
			 */
			pci_config_space_set(dev);
#endif
			/* map resources for devices that use igb_uio */
			ret = pci_map_device(dev);
			if (ret != 0)
				return ret;
		} else if (dr->drv_flags & RTE_PCI_DRV_FORCE_UNBIND &&
		           rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* unbind current driver */
			if (pci_unbind_kernel_driver(dev) < 0)
				return -1;
		}

		/* reference driver structure */
		dev->driver = dr;

		/* call the driver devinit() function */
		return dr->devinit(dr, dev);
	}
	/* return positive value if driver is not found */
	return 1;
}

#ifdef RTE_LIBRTE_EAL_HOTPLUG
/*
 * If vendor/device ID match, call the devuninit() function of the
 * driver.
 */
int
rte_eal_pci_close_one_driver(struct rte_pci_driver *dr,
		struct rte_pci_device *dev)
{
	struct rte_pci_id *id_table;

	if ((dr == NULL) || (dev == NULL))
		return -EINVAL;

	for (id_table = dr->id_table ; id_table->vendor_id != 0; id_table++) {

		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != dev->id.vendor_id &&
		    id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != dev->id.device_id &&
		    id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != PCI_ANY_ID)
			continue;

		struct rte_pci_addr *loc = &dev->addr;

		RTE_LOG(DEBUG, EAL,
				"PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
				loc->domain, loc->bus, loc->devid,
				loc->function, dev->numa_node);

		RTE_LOG(DEBUG, EAL, "  remove driver: %x:%x %s\n",
				dev->id.vendor_id, dev->id.device_id,
				dr->name);

		/* call the driver devuninit() function */
		if (dr->devuninit && (dr->devuninit(dev) < 0))
			return -1;	/* negative value is an error */

		/* clear driver structure */
		dev->driver = NULL;

		if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING)
			/* unmap resources for devices that use igb_uio */
			pci_unmap_device(dev);

		return 0;
	}
	/* return positive value if driver is not found */
	return 1;
}
#else /* RTE_LIBRTE_EAL_HOTPLUG */
int
rte_eal_pci_close_one_driver(struct rte_pci_driver *dr __rte_unused,
		struct rte_pci_device *dev __rte_unused)
{
	RTE_LOG(ERR, EAL, "Hotplug support isn't enabled\n");
	return -1;
}
#endif /* RTE_LIBRTE_EAL_HOTPLUG */

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
#ifdef VFIO_PRESENT
	pci_vfio_enable();

	if (pci_vfio_is_enabled()) {

		/* if we are primary process, create a thread to communicate with
		 * secondary processes. the thread will use a socket to wait for
		 * requests from secondary process to send open file descriptors,
		 * because VFIO does not allow multiple open descriptors on a group or
		 * VFIO container.
		 */
		if (internal_config.process_type == RTE_PROC_PRIMARY &&
				pci_vfio_mp_sync_setup() < 0)
			return -1;
	}
#endif
	return 0;
}
