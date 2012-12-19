/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_common.h>
#include <rte_launch.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_debug.h>

#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_private.h"

/**
 * @file
 * PCI probing under linux
 *
 * This code is used to simulate a PCI probe by parsing information in
 * sysfs. Moreover, when a registered driver matches a device, the
 * kernel driver currently using it is unloaded and replaced by
 * igb_uio module, which is a very minimal userland driver for Intel
 * network card, only providing access to PCI BAR to applications, and
 * enabling bus master.
 */


#define PROC_MODULES "/proc/modules"

#define IGB_UIO_NAME "igb_uio"

#define UIO_NEWID "/sys/bus/pci/drivers/%s/new_id"
#define UIO_BIND  "/sys/bus/pci/drivers/%s/bind"

/* maximum time to wait that /dev/uioX appears */
#define UIO_DEV_WAIT_TIMEOUT 3 /* seconds */

/*
 * For multi-process we need to reproduce all PCI mappings in secondary
 * processes, so save them in a tailq.
 */
struct uio_resource {
	TAILQ_ENTRY(uio_resource) next;

	struct rte_pci_addr pci_addr;
	void *addr;
	char path[PATH_MAX];
	unsigned long size;
	unsigned long offset;
};

TAILQ_HEAD(uio_res_list, uio_resource);

static struct uio_res_list *uio_res_list = NULL;

/*
 * Check that a kernel module is loaded. Returns 0 on success, or if the
 * parameter is NULL, or -1 if the module is not loaded.
 */
static int
pci_uio_check_module(const char *module_name)
{
	FILE *f;
	unsigned i;
	char buf[BUFSIZ];

	if (module_name == NULL)
		return 0;

	f = fopen(PROC_MODULES, "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "Cannot open "PROC_MODULES"\n");
		return -1;
	}

	while(fgets(buf, sizeof(buf), f) != NULL) {

		for (i = 0; i < sizeof(buf) && buf[i] != '\0'; i++) {
			if (isspace(buf[i]))
			    buf[i] = '\0';
		}

		if (strncmp(buf, module_name, sizeof(buf)) == 0) {
			fclose(f);
			return 0;
		}
	}
	fclose(f);
	RTE_LOG(ERR, EAL, "Cannot find %s in "PROC_MODULES"\n", module_name);
	return -1;
}

/* bind a PCI to the kernel module driver */
static int
pci_uio_bind_device(struct rte_pci_device *dev, const char *module_name)
{
	FILE *f;
	int n;
	char buf[BUFSIZ];
	char uio_newid[PATH_MAX];
	char uio_bind[PATH_MAX];
	struct rte_pci_addr *loc = &dev->addr;

	RTE_LOG(DEBUG, EAL, "bind PCI device "PCI_PRI_FMT" to %s driver\n",
	        loc->domain, loc->bus, loc->devid, loc->function, module_name);

	n = rte_snprintf(uio_newid, sizeof(uio_newid), UIO_NEWID, module_name);
	if ((n < 0) || (n >= (int)sizeof(uio_newid))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf uio_newid name\n");
		return -1;
	}
	n = rte_snprintf(uio_bind, sizeof(uio_bind), UIO_BIND, module_name);
	if ((n < 0) || (n >= (int)sizeof(uio_bind))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf uio_bind name\n");
		return -1;
	}

	n = rte_snprintf(buf, sizeof(buf), "%x %x\n",
			dev->id.vendor_id, dev->id.device_id);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf vendor_id/device_id\n");
		return -1;
	}

	f = fopen(uio_newid, "w");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "Cannot open %s\n", uio_newid);
		return -1;
	}
	if (fwrite(buf, n, 1, f) == 0) {
		fclose(f);
		return -1;
	}
	fclose(f);

	f = fopen(uio_bind, "w");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "Cannot open %s\n", uio_bind);
		return -1;
	}
	n = rte_snprintf(buf, sizeof(buf), PCI_PRI_FMT "\n",
	                 loc->domain, loc->bus, loc->devid, loc->function);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf PCI infos\n");
		fclose(f);
		return -1;
	}
	if (fwrite(buf, n, 1, f) == 0) {
		fclose(f);
		return -1;
	}

	RTE_LOG(DEBUG, EAL, "Device bound\n");

	fclose(f);
	return 0;
}

/* map a particular resource from a file */
static void *
pci_map_resource(struct rte_pci_device *dev, void *requested_addr, const char *devname,
		unsigned long offset, unsigned long size)
{
	unsigned n;
	int fd;
	void *mapaddr;

	/*
	 * open devname, and mmap it: it can take some time to
	 * appear, so we wait some time before returning an error
	 */
	for (n=0; n<UIO_DEV_WAIT_TIMEOUT*10; n++) {
		fd = open(devname, O_RDWR);
		if (fd >= 0)
			break;
		if (errno != ENOENT)
			break;
		usleep(100000);
	}
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s\n", devname, strerror(errno));
		goto fail;
	}

	/* Map the PCI memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, offset);
	if (mapaddr == MAP_FAILED ||
			(requested_addr != NULL && mapaddr != requested_addr)) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap %s: %s\n", __func__,
			devname, strerror(errno));
		close(fd);
		goto fail;
	}
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* save fd if in primary process */
		dev->intr_handle.fd = fd;
		dev->intr_handle.type = RTE_INTR_HANDLE_UIO;
	} else {
		/* fd is not needed in slave process, close it */
		dev->intr_handle.fd = -1;
		dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
		close(fd);
	}

	RTE_LOG(DEBUG, EAL, "PCI memory mapped at %p\n", mapaddr);

	return mapaddr;

fail:
	dev->intr_handle.fd = -1;
	dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;

	return NULL;
}
/* map the PCI resource of a PCI device in virtual memory */
static int
pci_uio_map_resource(struct rte_pci_device *dev)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];
	char dirname2[PATH_MAX];
	char filename[PATH_MAX];
	char devname[PATH_MAX]; /* contains the /dev/uioX */
	void *mapaddr;
	unsigned uio_num;
	unsigned long size, offset;
	struct rte_pci_addr *loc = &dev->addr;
	struct uio_resource *uio_res;

	RTE_LOG(DEBUG, EAL, "map PCI resource for device "PCI_PRI_FMT"\n",
	        loc->domain, loc->bus, loc->devid, loc->function);

	/* secondary processes - use already recorded details */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {

			TAILQ_FOREACH(uio_res, uio_res_list, next) {
				/* skip this element if it doesn't match our PCI address */
				if (memcmp(&uio_res->pci_addr, &dev->addr, sizeof(dev->addr)))
					continue;

				if (pci_map_resource(dev, uio_res->addr, uio_res->path, \
						uio_res->offset, uio_res->size) == uio_res->addr)
					return 0;
				else {
					RTE_LOG(ERR, EAL, "Cannot mmap device resource\n");
					return -1;
				}
			}
			RTE_LOG(ERR, EAL, "Cannot find resource for device\n");
			return -1;
	}

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */

	rte_snprintf(dirname, sizeof(dirname),
	         "/sys/bus/pci/devices/" PCI_PRI_FMT "/uio",
	         loc->domain, loc->bus, loc->devid, loc->function);

	dir = opendir(dirname);
	if (dir == NULL) {
		/* retry with the parent directory */
		rte_snprintf(dirname, sizeof(dirname),
		         "/sys/bus/pci/devices/" PCI_PRI_FMT,
		         loc->domain, loc->bus, loc->devid, loc->function);
		dir = opendir(dirname);

		if (dir == NULL) {
			RTE_LOG(ERR, EAL, "Cannot opendir %s\n", dirname);
			return -1;
		}
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		int shortprefix_len = sizeof("uio") - 1; /* format could be uio%d ...*/
		int longprefix_len = sizeof("uio:uio") - 1; /* ... or uio:uio%d */
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != e->d_name) {
			rte_snprintf(dirname2, sizeof(dirname2),
				 "%s/uio%u", dirname, uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != e->d_name) {
			rte_snprintf(dirname2, sizeof(dirname2),
				 "%s/uio:uio%u", dirname, uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL)
		return 0;

	/* get mapping offset */
	rte_snprintf(filename, sizeof(filename),
		 "%s/maps/map0/offset", dirname2);
	if (eal_parse_sysfs_value(filename, &offset) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse offset\n",
			__func__);
		return -1;
	}

	/* get mapping size */
	rte_snprintf(filename, sizeof(filename),
		 "%s/maps/map0/size", dirname2);
	if (eal_parse_sysfs_value(filename, &size) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse size\n",
			__func__);
		return -1;
	}

	/* open and mmap /dev/uioX */
	rte_snprintf(devname, sizeof(devname), "/dev/uio%u", uio_num);
	mapaddr = pci_map_resource(dev, NULL, devname, offset, size);
	if (mapaddr == NULL)
		return -1;
	dev->mem_resource.addr = mapaddr;

	/* save the mapping details for secondary processes*/
	uio_res = rte_malloc("UIO_RES", sizeof(*uio_res), 0);
	if (uio_res == NULL){
		RTE_LOG(ERR, EAL, "%s(): cannot store uio mmap details\n", __func__);
		return -1;
	}
	uio_res->addr = mapaddr;
	uio_res->offset = offset;
	uio_res->size = size;
	rte_snprintf(uio_res->path, sizeof(uio_res->path), "%s", devname);
	memcpy(&uio_res->pci_addr, &dev->addr, sizeof(uio_res->pci_addr));

	TAILQ_INSERT_TAIL(uio_res_list, uio_res, next);

	return 0;
}

/* parse the "resource" sysfs file */
#define IORESOURCE_MEM  0x00000200

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
			RTE_LOG(ERR, EAL, "%s(): cannot read resource\n", __func__);
			goto error;
		}

		if (rte_strsplit(buf, sizeof(buf), res_info.ptrs, 3, ' ') != 3) {
			RTE_LOG(ERR, EAL, "%s(): bad resource format\n", __func__);
			goto error;
		}
		errno = 0;
		phys_addr = strtoull(res_info.phys_addr, NULL, 16);
		end_addr = strtoull(res_info.end_addr, NULL, 16);
		flags = strtoull(res_info.flags, NULL, 16);
		if (errno != 0) {
			RTE_LOG(ERR, EAL, "%s(): bad resource format\n", __func__);
			goto error;
		}

		if (flags & IORESOURCE_MEM) {
			dev->mem_resource.phys_addr = phys_addr;
			dev->mem_resource.len = end_addr - phys_addr + 1;
			dev->mem_resource.addr = NULL; /* not mapped for now */
			break;
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

	dev = malloc(sizeof(*dev));
	if (dev == NULL) {
		return -1;
	}

	memset(dev, 0, sizeof(*dev));
	dev->addr.domain = domain;
	dev->addr.bus = bus;
	dev->addr.devid = devid;
	dev->addr.function = function;

	/* get vendor id */
	rte_snprintf(filename, sizeof(filename), "%s/vendor", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.vendor_id = (uint16_t)tmp;

	/* get device id */
	rte_snprintf(filename, sizeof(filename), "%s/device", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.device_id = (uint16_t)tmp;

	/* get subsystem_vendor id */
	rte_snprintf(filename, sizeof(filename), "%s/subsystem_vendor",
		 dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.subsystem_vendor_id = (uint16_t)tmp;

	/* get subsystem_device id */
	rte_snprintf(filename, sizeof(filename), "%s/subsystem_device",
		 dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		free(dev);
		return -1;
	}
	dev->id.subsystem_device_id = (uint16_t)tmp;

	/* parse resources */
	rte_snprintf(filename, sizeof(filename), "%s/resource", dirname);
	if (pci_parse_sysfs_resource(filename, dev) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse resource\n", __func__);
		free(dev);
		return -1;
	}

	/* device is valid, add in list */
	TAILQ_INSERT_TAIL(&device_list, dev, next);

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
	*domain = (uint8_t)strtoul(splitaddr.domain, NULL, 16);
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
static int
pci_scan(void)
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

		rte_snprintf(dirname, sizeof(dirname), "%s/%s", SYSFS_PCI_DEVICES,
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
	rte_snprintf(filename, sizeof(filename),
	         SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/driver/unbind",
	         loc->domain, loc->bus, loc->devid, loc->function);

	RTE_LOG(DEBUG, EAL, "unbind kernel driver %s\n", filename);

	f = fopen(filename, "w");
	if (f == NULL) /* device was not bound */
		return 0;

	n = rte_snprintf(buf, sizeof(buf), PCI_PRI_FMT "\n",
	             loc->domain, loc->bus, loc->devid, loc->function);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "%s(): rte_snprintf failed\n", __func__);
		goto error;
	}
	if (fwrite(buf, n, 1, f) == 0)
		goto error;

	fclose(f);
	return 0;

error:
	fclose(f);
	return -1;
}

/*
 * If vendor/device ID match, call the devinit() function of the
 * driver.
 */
int
rte_eal_pci_probe_one_driver(struct rte_pci_driver *dr, struct rte_pci_device *dev)
{
	struct rte_pci_id *id_table;
	const char *module_name = NULL;
	int ret;

	if (dr->drv_flags & RTE_PCI_DRV_NEED_IGB_UIO)
		module_name = IGB_UIO_NAME;

	ret = pci_uio_check_module(module_name);
	if (ret != 0)
		rte_exit(1, "The %s module is required by the %s driver\n",
				module_name, dr->name);

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

		RTE_LOG(DEBUG, EAL, "probe driver: %x:%x %s\n",
		dev->id.vendor_id, dev->id.device_id, dr->name);

		/* Unbind PCI devices if needed */
		if (module_name != NULL) {
			if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* unbind current driver, bind ours */
				if (pci_unbind_kernel_driver(dev) < 0)
					return -1;
				if (pci_uio_bind_device(dev, module_name) < 0)
					return -1;
			}
			/* map the NIC resources */
			if (pci_uio_map_resource(dev) < 0)
				return -1;
		}
		/* call the driver devinit() function */
		return dr->devinit(dr, dev);

	}
	return -1;
}

/* Init the PCI EAL subsystem */
int
rte_eal_pci_init(void)
{
	TAILQ_INIT(&driver_list);
	TAILQ_INIT(&device_list);
	uio_res_list = RTE_TAILQ_RESERVE_BY_IDX(RTE_TAILQ_PCI, uio_res_list);

	/* for debug purposes, PCI can be disabled */
	if (internal_config.no_pci)
		return 0;

	if (pci_scan() < 0) {
		RTE_LOG(ERR, EAL, "%s(): Cannot scan PCI bus\n", __func__);
		return -1;
	}
	return 0;
}
