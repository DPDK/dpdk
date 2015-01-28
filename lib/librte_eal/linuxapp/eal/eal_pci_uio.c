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
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/pci_regs.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "rte_pci_dev_ids.h"
#include "eal_filesystem.h"
#include "eal_pci_init.h"

void *pci_map_addr = NULL;


#define OFF_MAX              ((uint64_t)(off_t)-1)
static int
pci_uio_get_mappings(struct rte_pci_device *dev,
			struct pci_map maps[], int nb_maps)
{
	struct rte_pci_addr *loc = &dev->addr;
	int i = 0;
	char filename[PATH_MAX];
	unsigned long long start_addr, end_addr, flags;
	FILE *f;

	snprintf(filename, sizeof(filename),
		SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/resource",
		loc->domain, loc->bus, loc->devid, loc->function);

	f = fopen(filename, "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL,
		"%s(): cannot open sysfs %s\n",
		__func__, filename);
		return -1;
	}

	while (fscanf(f, "%llx %llx %llx", &start_addr,
			&end_addr, &flags) == 3 && i < nb_maps) {
		if (flags & IORESOURCE_MEM) {
			maps[i].offset = 0x0;
			maps[i].size = end_addr - start_addr + 1;
			maps[i].phaddr = start_addr;
			i++;
		}
	}
	fclose(f);

	return i;
}

static int
pci_uio_set_bus_master(int dev_fd)
{
	uint16_t reg;
	int ret;

	ret = pread(dev_fd, &reg, sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		RTE_LOG(ERR, EAL,
			"Cannot read command from PCI config space!\n");
		return -1;
	}

	/* return if bus mastering is already on */
	if (reg & PCI_COMMAND_MASTER)
		return 0;

	reg |= PCI_COMMAND_MASTER;

	ret = pwrite(dev_fd, &reg, sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		RTE_LOG(ERR, EAL,
			"Cannot write command to PCI config space!\n");
		return -1;
	}

	return 0;
}

static int
pci_uio_map_secondary(struct rte_pci_device *dev)
{
	int fd, i;
	struct mapped_pci_resource *uio_res;

	TAILQ_FOREACH(uio_res, pci_res_list, next) {

		/* skip this element if it doesn't match our PCI address */
		if (memcmp(&uio_res->pci_addr, &dev->addr, sizeof(dev->addr)))
			continue;

		for (i = 0; i != uio_res->nb_maps; i++) {
			/* ignore mappings unused in primary process */
			if (uio_res->maps[i].addr == NULL)
				continue;

			/*
			 * open devname, to mmap it
			 */
			fd = open(uio_res->path, O_RDWR);
			if (fd < 0) {
				RTE_LOG(ERR, EAL, "Cannot open %s: %s\n",
					uio_res->path, strerror(errno));
				return -1;
			}

			if (pci_map_resource(uio_res->maps[i].addr, fd,
					     (off_t)uio_res->maps[i].offset,
					     (size_t)uio_res->maps[i].size, 0)
			    != uio_res->maps[i].addr) {
				RTE_LOG(ERR, EAL,
					"Cannot mmap device resource\n");
				close(fd);
				return -1;
			}
			/* fd is not needed in slave process, close it */
			close(fd);
		}
		return 0;
	}

	RTE_LOG(ERR, EAL, "Cannot find resource for device\n");
	return 1;
}

static int
pci_mknod_uio_dev(const char *sysfs_uio_path, unsigned uio_num)
{
	FILE *f;
	char filename[PATH_MAX];
	int ret;
	unsigned major, minor;
	dev_t dev;

	/* get the name of the sysfs file that contains the major and minor
	 * of the uio device and read its content */
	snprintf(filename, sizeof(filename), "%s/dev", sysfs_uio_path);

	f = fopen(filename, "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "%s(): cannot open sysfs to get major:minor\n",
			__func__);
		return -1;
	}

	ret = fscanf(f, "%u:%u", &major, &minor);
	if (ret != 2) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse sysfs to get major:minor\n",
			__func__);
		fclose(f);
		return -1;
	}
	fclose(f);

	/* create the char device "mknod /dev/uioX c major minor" */
	snprintf(filename, sizeof(filename), "/dev/uio%u", uio_num);
	dev = makedev(major, minor);
	ret = mknod(filename, S_IFCHR | S_IRUSR | S_IWUSR, dev);
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "%s(): mknod() failed %s\n",
			__func__, strerror(errno));
		return -1;
	}

	return ret;
}

/*
 * Return the uioX char device used for a pci device. On success, return
 * the UIO number and fill dstbuf string with the path of the device in
 * sysfs. On error, return a negative value. In this case dstbuf is
 * invalid.
 */
static int
pci_get_uio_dev(struct rte_pci_device *dev, char *dstbuf,
			   unsigned int buflen)
{
	struct rte_pci_addr *loc = &dev->addr;
	unsigned int uio_num;
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */

	snprintf(dirname, sizeof(dirname),
			SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/uio",
			loc->domain, loc->bus, loc->devid, loc->function);

	dir = opendir(dirname);
	if (dir == NULL) {
		/* retry with the parent directory */
		snprintf(dirname, sizeof(dirname),
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
			snprintf(dstbuf, buflen, "%s/uio%u", dirname, uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			snprintf(dstbuf, buflen, "%s/uio:uio%u", dirname, uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL)
		return -1;

	/* create uio device if we've been asked to */
	if (internal_config.create_uio_dev &&
			pci_mknod_uio_dev(dstbuf, uio_num) < 0)
		RTE_LOG(WARNING, EAL, "Cannot create /dev/uio%u\n", uio_num);

	return uio_num;
}

/* map the PCI resource of a PCI device in virtual memory */
int
pci_uio_map_resource(struct rte_pci_device *dev)
{
	int i, j;
	char dirname[PATH_MAX];
	char cfgname[PATH_MAX];
	char devname[PATH_MAX]; /* contains the /dev/uioX */
	void *mapaddr;
	int uio_num;
	uint64_t phaddr;
	uint64_t offset;
	uint64_t pagesz;
	int nb_maps;
	struct rte_pci_addr *loc = &dev->addr;
	struct mapped_pci_resource *uio_res;
	struct pci_map *maps;

	dev->intr_handle.fd = -1;
	dev->intr_handle.uio_cfg_fd = -1;
	dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;

	/* secondary processes - use already recorded details */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return pci_uio_map_secondary(dev);

	/* find uio resource */
	uio_num = pci_get_uio_dev(dev, dirname, sizeof(dirname));
	if (uio_num < 0) {
		RTE_LOG(WARNING, EAL, "  "PCI_PRI_FMT" not managed by UIO driver, "
				"skipping\n", loc->domain, loc->bus, loc->devid, loc->function);
		return 1;
	}
	snprintf(devname, sizeof(devname), "/dev/uio%u", uio_num);

	/* save fd if in primary process */
	dev->intr_handle.fd = open(devname, O_RDWR);
	if (dev->intr_handle.fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s\n",
			devname, strerror(errno));
		return -1;
	}
	dev->intr_handle.type = RTE_INTR_HANDLE_UIO;

	snprintf(cfgname, sizeof(cfgname),
			"/sys/class/uio/uio%u/device/config", uio_num);
	dev->intr_handle.uio_cfg_fd = open(cfgname, O_RDWR);
	if (dev->intr_handle.uio_cfg_fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s\n",
			cfgname, strerror(errno));
		return -1;
	}

	/* update devname for mmap  */
	snprintf(devname, sizeof(devname),
		SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/resource%d",
		loc->domain, loc->bus, loc->devid, loc->function, 0);

	/* set bus master that is not done by uio_pci_generic */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (pci_uio_set_bus_master(dev->intr_handle.uio_cfg_fd)) {
			RTE_LOG(ERR, EAL, "Cannot set up bus mastering!\n");
			return -1;
		}
	}

	/* allocate the mapping details for secondary processes*/
	uio_res = rte_zmalloc("UIO_RES", sizeof(*uio_res), 0);
	if (uio_res == NULL) {
		RTE_LOG(ERR, EAL,
			"%s(): cannot store uio mmap details\n", __func__);
		return -1;
	}

	snprintf(uio_res->path, sizeof(uio_res->path), "%s", devname);
	memcpy(&uio_res->pci_addr, &dev->addr, sizeof(uio_res->pci_addr));

	/* collect info about device mappings */
	nb_maps = pci_uio_get_mappings(dev, uio_res->maps,
					RTE_DIM(uio_res->maps));
	if (nb_maps < 0) {
		rte_free(uio_res);
		return nb_maps;
	}
	uio_res->nb_maps = nb_maps;

	/* Map all BARs */
	pagesz = sysconf(_SC_PAGESIZE);

	maps = uio_res->maps;
	for (i = 0; i != PCI_MAX_RESOURCE; i++) {
		int fd;

		/* skip empty BAR */
		phaddr = dev->mem_resource[i].phys_addr;
		if (phaddr == 0)
			continue;

		for (j = 0; j != nb_maps && (phaddr != maps[j].phaddr ||
				dev->mem_resource[i].len != maps[j].size);
				j++)
			;

		/* if matching map is found, then use it */
		if (j != nb_maps) {
			int fail = 0;
			offset = j * pagesz;

			/*
			 * open devname, to mmap it
			 */
			fd = open(devname, O_RDWR);
			if (fd < 0) {
				RTE_LOG(ERR, EAL, "Cannot open %s: %s\n",
					devname, strerror(errno));
				return -1;
			}

			if (maps[j].addr != NULL)
				fail = 1;
			else {
				/* try mapping somewhere close to the end of hugepages */
				if (pci_map_addr == NULL)
					pci_map_addr = pci_find_max_end_va();

				mapaddr = pci_map_resource(pci_map_addr, fd, (off_t)offset,
						(size_t)maps[j].size, 0);
				if (mapaddr == MAP_FAILED)
					fail = 1;

				pci_map_addr = RTE_PTR_ADD(mapaddr, (size_t) maps[j].size);
			}

			if (fail) {
				rte_free(uio_res);
				close(fd);
				return -1;
			}
			close(fd);

			maps[j].addr = mapaddr;
			maps[j].offset = offset;
			dev->mem_resource[i].addr = mapaddr;
		}
	}

	TAILQ_INSERT_TAIL(pci_res_list, uio_res, next);

	return 0;
}
