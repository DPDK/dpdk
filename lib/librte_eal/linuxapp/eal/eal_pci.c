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

#include "rte_pci_dev_ids.h"
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

struct uio_map {
	void *addr;
	uint64_t offset;
	uint64_t size;
	uint64_t phaddr;
};

/*
 * For multi-process we need to reproduce all PCI mappings in secondary
 * processes, so save them in a tailq.
 */
struct uio_resource {
	TAILQ_ENTRY(uio_resource) next;

	struct rte_pci_addr pci_addr;
	char path[PATH_MAX];
	size_t nb_maps;
	struct uio_map maps[PCI_MAX_RESOURCE];
};

TAILQ_HEAD(uio_res_list, uio_resource);

static struct uio_res_list *uio_res_list = NULL;
static int pci_parse_sysfs_value(const char *filename, uint64_t *val);

/* forward prototype of function called in pci_switch_module below */
static int pci_uio_map_resource(struct rte_pci_device *dev);

#ifdef RTE_EAL_UNBIND_PORTS
#define PROC_MODULES "/proc/modules"

#define IGB_UIO_NAME "igb_uio"

#define UIO_DRV_PATH  "/sys/bus/pci/drivers/%s"

/* maximum time to wait that /dev/uioX appears */
#define UIO_DEV_WAIT_TIMEOUT 3 /* seconds */

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
		RTE_LOG(ERR, EAL, "Cannot open "PROC_MODULES": %s\n", 
				strerror(errno));
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
	return -1;
}

/* bind a PCI to the kernel module driver */
static int
pci_bind_device(struct rte_pci_device *dev, char dr_path[])
{
	FILE *f;
	int n;
	char buf[BUFSIZ];
	char dev_bind[PATH_MAX];
	struct rte_pci_addr *loc = &dev->addr;

	n = rte_snprintf(dev_bind, sizeof(dev_bind), "%s/bind", dr_path);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf device bind path\n");
		return -1;
	}

	f = fopen(dev_bind, "w");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "Cannot open %s\n", dev_bind);
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

	fclose(f);
	return 0;
}

static int
pci_uio_bind_device(struct rte_pci_device *dev, const char *module_name)
{
	FILE *f;
	int n;
	char buf[BUFSIZ];
	char uio_newid[PATH_MAX];
	char uio_bind[PATH_MAX];

	n = rte_snprintf(uio_newid, sizeof(uio_newid), UIO_DRV_PATH "/new_id", module_name);
	if ((n < 0) || (n >= (int)sizeof(uio_newid))) {
		RTE_LOG(ERR, EAL, "Cannot rte_snprintf uio_newid name\n");
		return -1;
	}

	n = rte_snprintf(uio_bind, sizeof(uio_bind), UIO_DRV_PATH, module_name);
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

	pci_bind_device(dev, uio_bind);
	return 0;
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

	f = fopen(filename, "w");
	if (f == NULL) /* device was not bound */
		return 0;

	n = rte_snprintf(buf, sizeof(buf), PCI_PRI_FMT "\n",
	             loc->domain, loc->bus, loc->devid, loc->function);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		RTE_LOG(ERR, EAL, "%s(): rte_snprintf failed\n", __func__);
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
pci_switch_module(struct rte_pci_driver *dr, struct rte_pci_device *dev,
		int uio_status, const char *module_name)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* check that our driver is loaded */
		if (uio_status != 0 &&
				(uio_status = pci_uio_check_module(module_name)) != 0)
			rte_exit(EXIT_FAILURE, "The %s module is required by the "
					"%s driver\n", module_name, dr->name);

		/* unbind current driver, bind ours */
		if (pci_unbind_kernel_driver(dev) < 0)
			return -1;
		if (pci_uio_bind_device(dev, module_name) < 0)
			return -1;
	}
	/* map the NIC resources */
	if (pci_uio_map_resource(dev) < 0)
		return -1;

	return 0;
}

#endif /* ifdef EAL_UNBIND_PORTS */

/* map a particular resource from a file */
static void *
pci_map_resource(struct rte_pci_device *dev, void *requested_addr, 
		const char *devname, off_t offset, size_t size)
{
	int fd;
	void *mapaddr;

#ifdef RTE_EAL_UNBIND_PORTS
	/*
	 * open devname, and mmap it: it can take some time to
	 * appear, so we wait some time before returning an error
	 */
	unsigned n;
	fd = dev->intr_handle.fd;
	for (n = 0; n < UIO_DEV_WAIT_TIMEOUT*10 && fd < 0; n++) {
		errno = 0;
		if ((fd = open(devname, O_RDWR)) < 0 && errno != ENOENT)
			break;
		usleep(100000);
	}
#else
	/*
	 * open devname, to mmap it
	 */
	fd = open(devname, O_RDWR);
#endif
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s\n", 
			devname, strerror(errno));
		goto fail;
	}

	/* Map the PCI memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, offset);
	if (mapaddr == MAP_FAILED ||
			(requested_addr != NULL && mapaddr != requested_addr)) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap(%s(%d), %p, 0x%lx, 0x%lx):"
			" %s (%p)\n", __func__, devname, fd, requested_addr, 
			(unsigned long)size, (unsigned long)offset,
			strerror(errno), mapaddr);
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

	RTE_LOG(DEBUG, EAL, "  PCI memory mapped at %p\n", mapaddr);

	return mapaddr;

fail:
	dev->intr_handle.fd = -1;
	dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;

	return NULL;
}

#define OFF_MAX              ((uint64_t)(off_t)-1)
static ssize_t
pci_uio_get_mappings(const char *devname, struct uio_map maps[], size_t nb_maps)
{
	size_t i;
	char dirname[PATH_MAX];
	char filename[PATH_MAX];
	uint64_t offset, size;

	for (i = 0; i != nb_maps; i++) {
 
		/* check if map directory exists */
		rte_snprintf(dirname, sizeof(dirname), 
			"%s/maps/map%u", devname, i);
 
		if (access(dirname, F_OK) != 0)
			break;
 
		/* get mapping offset */
		rte_snprintf(filename, sizeof(filename),
			"%s/offset", dirname);
		if (pci_parse_sysfs_value(filename, &offset) < 0) {
			RTE_LOG(ERR, EAL,
				"%s(): cannot parse offset of %s\n",
				__func__, dirname);
			return (-1);
		}
 
		/* get mapping size */
		rte_snprintf(filename, sizeof(filename),
			"%s/size", dirname);
		if (pci_parse_sysfs_value(filename, &size) < 0) {
			RTE_LOG(ERR, EAL,
				"%s(): cannot parse size of %s\n",
				__func__, dirname);
			return (-1);
		}
 
		/* get mapping physical address */
		rte_snprintf(filename, sizeof(filename),
			"%s/addr", dirname);
		if (pci_parse_sysfs_value(filename, &maps[i].phaddr) < 0) {
			RTE_LOG(ERR, EAL,
				"%s(): cannot parse addr of %s\n",
				__func__, dirname);
			return (-1);
		}

		if ((offset > OFF_MAX) || (size > SIZE_MAX)) {
			RTE_LOG(ERR, EAL,
				"%s(): offset/size exceed system max value\n",
				__func__); 
			return (-1);
		}

		maps[i].offset = offset;
		maps[i].size = size;
        }
	return (i);
}

static int
pci_uio_map_secondary(struct rte_pci_device *dev)
{
        size_t i;
        struct uio_resource *uio_res;
 
	TAILQ_FOREACH(uio_res, uio_res_list, next) {
 
		/* skip this element if it doesn't match our PCI address */
		if (memcmp(&uio_res->pci_addr, &dev->addr, sizeof(dev->addr)))
			continue;
                
		for (i = 0; i != uio_res->nb_maps; i++) {
			if (pci_map_resource(dev, uio_res->maps[i].addr,
					uio_res->path,
					(off_t)uio_res->maps[i].offset,
					(size_t)uio_res->maps[i].size) != 
					uio_res->maps[i].addr) {
				RTE_LOG(ERR, EAL,
					"Cannot mmap device resource\n");
				return (-1);
			}
		}
		return (0);
	}

	RTE_LOG(ERR, EAL, "Cannot find resource for device\n");
	return -1;
}

/* map the PCI resource of a PCI device in virtual memory */
static int
pci_uio_map_resource(struct rte_pci_device *dev)
{
	int i, j;
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];
	char filename[PATH_MAX];
	char dirname2[PATH_MAX];
	char devname[PATH_MAX]; /* contains the /dev/uioX */
	void *mapaddr;
	unsigned uio_num;
	unsigned long start,size;
	uint64_t phaddr;
	uint64_t offset;
	uint64_t pagesz;
	ssize_t nb_maps;
	struct rte_pci_addr *loc = &dev->addr;
	struct uio_resource *uio_res;
	struct uio_map *maps;

	dev->intr_handle.fd = -1;

	/* secondary processes - use already recorded details */
	if ((rte_eal_process_type() != RTE_PROC_PRIMARY) &&
		(dev->id.vendor_id != PCI_VENDOR_ID_QUMRANET))
		return (pci_uio_map_secondary(dev));

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */

	rte_snprintf(dirname, sizeof(dirname),
	         SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/uio",
	         loc->domain, loc->bus, loc->devid, loc->function);

	dir = opendir(dirname);
	if (dir == NULL) {
		/* retry with the parent directory */
		rte_snprintf(dirname, sizeof(dirname),
		         SYSFS_PCI_DEVICES "/" PCI_PRI_FMT,
		         loc->domain, loc->bus, loc->devid, loc->function);
		dir = opendir(dirname);

		if (dir == NULL) {
			RTE_LOG(ERR, EAL, "Cannot opendir %s\n", dirname);
			return -1;
		}
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;
		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1; 
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + shortprefix_len)) {
			rte_snprintf(dirname2, sizeof(dirname2),
				 "%s/uio%u", dirname, uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			rte_snprintf(dirname2, sizeof(dirname2),
				 "%s/uio:uio%u", dirname, uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL) {
		RTE_LOG(WARNING, EAL, "  "PCI_PRI_FMT" not managed by UIO driver, "
				"skipping\n", loc->domain, loc->bus, loc->devid, loc->function);
		return -1;
	}

	if(dev->id.vendor_id == PCI_VENDOR_ID_QUMRANET) {
		/* get portio size */
		rte_snprintf(filename, sizeof(filename),
			 "%s/portio/port0/size", dirname2);
		if (eal_parse_sysfs_value(filename, &size) < 0) {
			RTE_LOG(ERR, EAL, "%s(): cannot parse size\n",
				__func__);
			return -1;
		}

		/* get portio start */
		rte_snprintf(filename, sizeof(filename),
			 "%s/portio/port0/start", dirname2);
		if (eal_parse_sysfs_value(filename, &start) < 0) {
			RTE_LOG(ERR, EAL, "%s(): cannot parse portio start\n",
				__func__);
			return -1;
		}
		dev->mem_resource[0].addr = (void *)(uintptr_t)start;
		dev->mem_resource[0].len =  (uint64_t)size;
		RTE_LOG(DEBUG, EAL, "PCI Port IO found start=0x%lx with size=0x%lx\n", start, size);
		/* rte_virtio_pmd does not need any other bar even if available */
		return (0);
	}
	
	/* allocate the mapping details for secondary processes*/
	if ((uio_res = rte_zmalloc("UIO_RES", sizeof (*uio_res), 0)) == NULL) {
		RTE_LOG(ERR, EAL,
			"%s(): cannot store uio mmap details\n", __func__);
		return (-1);
	}

	rte_snprintf(devname, sizeof(devname), "/dev/uio%u", uio_num);
	rte_snprintf(uio_res->path, sizeof(uio_res->path), "%s", devname);
	memcpy(&uio_res->pci_addr, &dev->addr, sizeof(uio_res->pci_addr));

	/* collect info about device mappings */
	if ((nb_maps = pci_uio_get_mappings(dirname2, uio_res->maps,
			sizeof (uio_res->maps) / sizeof (uio_res->maps[0])))
			< 0)
		return (nb_maps);
 
	uio_res->nb_maps = nb_maps;

	/* Map all BARs */
	pagesz = sysconf(_SC_PAGESIZE);
 
	maps = uio_res->maps;
	for (i = 0; i != PCI_MAX_RESOURCE; i++) {
    
		/* skip empty BAR */
		if ((phaddr = dev->mem_resource[i].phys_addr) == 0)
			continue;
 
		for (j = 0; j != nb_maps && (phaddr != maps[j].phaddr ||
				dev->mem_resource[i].len != maps[j].size);
				j++)
			;
 
		/* if matching map is found, then use it */
		if (j != nb_maps) {
			offset = j * pagesz;
			if (maps[j].addr != NULL ||
					(mapaddr = pci_map_resource(dev,
					NULL, devname, (off_t)offset,
					(size_t)maps[j].size)) == NULL) {
				return (-1);
			}
 
			maps[j].addr = mapaddr;
			maps[j].offset = offset;
			dev->mem_resource[i].addr = mapaddr;
		}
	}

	TAILQ_INSERT_TAIL(uio_res_list, uio_res, next);

	return (0);
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

/* 
 * parse a sysfs file containing one integer value 
 * different to the eal version, as it needs to work with 64-bit values
 */ 
static int 
pci_parse_sysfs_value(const char *filename, uint64_t *val) 
{
        FILE *f;
        char buf[BUFSIZ];
        char *end = NULL;
 
        f = fopen(filename, "r");
        if (f == NULL) {
                RTE_LOG(ERR, EAL, "%s(): cannot open sysfs value %s\n",
                        __func__, filename);
                return -1;
        }
 
        if (fgets(buf, sizeof(buf), f) == NULL) {
                RTE_LOG(ERR, EAL, "%s(): cannot read sysfs value %s\n",
                        __func__, filename);
                fclose(f);
                return -1;
        }
        *val = strtoull(buf, &end, 0);
        if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
                RTE_LOG(ERR, EAL, "%s(): cannot parse sysfs value %s\n",
                                __func__, filename);
                fclose(f);
                return -1;
        }
        fclose(f);
        return 0;
}

/* Compare two PCI device addresses. */
static int
pci_addr_comparison(struct rte_pci_addr *addr, struct rte_pci_addr *addr2)
{
	uint64_t dev_addr = (addr->domain << 24) + (addr->bus << 16) + (addr->devid << 8) + addr->function;
	uint64_t dev_addr2 = (addr2->domain << 24) + (addr2->bus << 16) + (addr2->devid << 8) + addr2->function;

	if (dev_addr > dev_addr2)
		return 1;
	else
		return 0;
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

	/* get max_vfs */
	dev->max_vfs = 0;
	rte_snprintf(filename, sizeof(filename), "%s/max_vfs", dirname);
	if (!access(filename, F_OK) && 
	    eal_parse_sysfs_value(filename, &tmp) == 0) {
		dev->max_vfs = (uint16_t)tmp;
	}

	/* get numa node */
	rte_snprintf(filename, sizeof(filename), "%s/numa_node",
		 dirname);
	if (access(filename, R_OK) != 0) {
		/* if no NUMA support just set node to 0 */
		dev->numa_node = -1;
	} else {
		if (eal_parse_sysfs_value(filename, &tmp) < 0) {
			free(dev);
			return -1;
		}
		dev->numa_node = tmp;
	}

	/* parse resources */
	rte_snprintf(filename, sizeof(filename), "%s/resource", dirname);
	if (pci_parse_sysfs_resource(filename, dev) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse resource\n", __func__);
		free(dev);
		return -1;
	}

	/* device is valid, add in list (sorted) */
	if (TAILQ_EMPTY(&device_list)) {
		TAILQ_INSERT_TAIL(&device_list, dev, next);
	}	
	else {
		struct rte_pci_device *dev2 = NULL;

		TAILQ_FOREACH(dev2, &device_list, next) {
			if (pci_addr_comparison(&dev->addr, &dev2->addr))
				continue;
			else {
				TAILQ_INSERT_BEFORE(dev2, dev, next);
				return 0;
			}
		}
		TAILQ_INSERT_TAIL(&device_list, dev, next);
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

/*
 * If vendor/device ID match, call the devinit() function of the
 * driver.
 */
int
rte_eal_pci_probe_one_driver(struct rte_pci_driver *dr, struct rte_pci_device *dev)
{
	struct rte_pci_id *id_table;
#ifdef RTE_EAL_UNBIND_PORTS
	const char *module_name = NULL;
	int uio_status = -1;

	if (dr->drv_flags & RTE_PCI_DRV_NEED_IGB_UIO)
		module_name = IGB_UIO_NAME;
#endif

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
		if (dev->blacklisted) {
			RTE_LOG(DEBUG, EAL, "  Device is blacklisted, not initializing\n");
			return 0;
		}

#ifdef RTE_EAL_UNBIND_PORTS
		/* Unbind PCI devices if needed */
		if (module_name != NULL)
			if (pci_switch_module(dr, dev, uio_status, module_name) < 0)
				return -1;
#else
		/* just map the NIC resources */
		if (pci_uio_map_resource(dev) < 0)
			return -1;
#endif

		/* We always should have BAR0 mapped */
		if (rte_eal_process_type() == RTE_PROC_PRIMARY && 
			dev->mem_resource[0].addr == NULL) {
			RTE_LOG(ERR, EAL,
				"%s(): BAR0 is not mapped\n",
				__func__);
			return (-1);
		}
 
		/* reference driver structure */
		dev->driver = dr;

		/* call the driver devinit() function */
		return dr->devinit(dr, dev);
	}
	/* return positive value if driver is not found */
	return 1;
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
