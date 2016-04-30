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

#include <stdint.h>
#include <dirent.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <fuse/cuse_lowlevel.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <errno.h>

#include <rte_log.h>

#include "rte_virtio_net.h"
#include "vhost-net.h"
#include "virtio-net-cdev.h"
#include "eventfd_copy.h"

/* Line size for reading maps file. */
static const uint32_t BUFSIZE = PATH_MAX;

/* Size of prot char array in procmap. */
#define PROT_SZ 5

/* Number of elements in procmap struct. */
#define PROCMAP_SZ 8

/* Structure containing information gathered from maps file. */
struct procmap {
	uint64_t va_start;	/* Start virtual address in file. */
	uint64_t len;		/* Size of file. */
	uint64_t pgoff;		/* Not used. */
	uint32_t maj;		/* Not used. */
	uint32_t min;		/* Not used. */
	uint32_t ino;		/* Not used. */
	char prot[PROT_SZ];	/* Not used. */
	char fname[PATH_MAX];	/* File name. */
};

/*
 * Locate the file containing QEMU's memory space and
 * map it to our address space.
 */
static int
host_memory_map(pid_t pid, uint64_t addr,
	uint64_t *mapped_address, uint64_t *mapped_size)
{
	struct dirent *dptr = NULL;
	struct procmap procmap;
	DIR *dp = NULL;
	int fd;
	int i;
	char memfile[PATH_MAX];
	char mapfile[PATH_MAX];
	char procdir[PATH_MAX];
	char resolved_path[PATH_MAX];
	char *path = NULL;
	FILE *fmap;
	void *map;
	uint8_t found = 0;
	char line[BUFSIZE];
	char dlm[] = "-   :   ";
	char *str, *sp, *in[PROCMAP_SZ];
	char *end = NULL;

	/* Path where mem files are located. */
	snprintf(procdir, PATH_MAX, "/proc/%u/fd/", pid);
	/* Maps file used to locate mem file. */
	snprintf(mapfile, PATH_MAX, "/proc/%u/maps", pid);

	fmap = fopen(mapfile, "r");
	if (fmap == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to open maps file for pid %d\n",
			pid);
		return -1;
	}

	/* Read through maps file until we find out base_address. */
	while (fgets(line, BUFSIZE, fmap) != 0) {
		str = line;
		errno = 0;
		/* Split line into fields. */
		for (i = 0; i < PROCMAP_SZ; i++) {
			in[i] = strtok_r(str, &dlm[i], &sp);
			if ((in[i] == NULL) || (errno != 0)) {
				fclose(fmap);
				return -1;
			}
			str = NULL;
		}

		/* Convert/Copy each field as needed. */
		procmap.va_start = strtoull(in[0], &end, 16);
		if ((in[0] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.len = strtoull(in[1], &end, 16);
		if ((in[1] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.pgoff = strtoull(in[3], &end, 16);
		if ((in[3] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.maj = strtoul(in[4], &end, 16);
		if ((in[4] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.min = strtoul(in[5], &end, 16);
		if ((in[5] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.ino = strtoul(in[6], &end, 16);
		if ((in[6] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		memcpy(&procmap.prot, in[2], PROT_SZ);
		memcpy(&procmap.fname, in[7], PATH_MAX);

		if (procmap.va_start == addr) {
			procmap.len = procmap.len - procmap.va_start;
			found = 1;
			break;
		}
	}
	fclose(fmap);

	if (!found) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find memory file in pid %d maps file\n",
			pid);
		return -1;
	}

	/* Find the guest memory file among the process fds. */
	dp = opendir(procdir);
	if (dp == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Cannot open pid %d process directory\n",
			pid);
		return -1;
	}

	found = 0;

	/* Read the fd directory contents. */
	while (NULL != (dptr = readdir(dp))) {
		snprintf(memfile, PATH_MAX, "/proc/%u/fd/%s",
				pid, dptr->d_name);
		path = realpath(memfile, resolved_path);
		if ((path == NULL) && (strlen(resolved_path) == 0)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to resolve fd directory\n");
			closedir(dp);
			return -1;
		}
		if (strncmp(resolved_path, procmap.fname,
			strnlen(procmap.fname, PATH_MAX)) == 0) {
			found = 1;
			break;
		}
	}

	closedir(dp);

	if (found == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find memory file for pid %d\n",
			pid);
		return -1;
	}
	/* Open the shared memory file and map the memory into this process. */
	fd = open(memfile, O_RDWR);

	if (fd == -1) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to open %s for pid %d\n",
			memfile, pid);
		return -1;
	}

	map = mmap(0, (size_t)procmap.len, PROT_READ|PROT_WRITE,
			MAP_POPULATE|MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Error mapping the file %s for pid %d\n",
			memfile, pid);
		return -1;
	}

	/* Store the memory address and size in the device data structure */
	*mapped_address = (uint64_t)(uintptr_t)map;
	*mapped_size = procmap.len;

	LOG_DEBUG(VHOST_CONFIG,
		"Mem File: %s->%s - Size: %llu - VA: %p\n",
		memfile, resolved_path,
		(unsigned long long)*mapped_size, map);

	return 0;
}

int
cuse_set_mem_table(struct vhost_cuse_device_ctx ctx,
	const struct vhost_memory *mem_regions_addr, uint32_t nregions)
{
	uint64_t size = offsetof(struct vhost_memory, regions);
	uint32_t idx, valid_regions;
	struct virtio_memory_regions *pregion;
	struct vhost_memory_region *mem_regions = (void *)(uintptr_t)
		((uint64_t)(uintptr_t)mem_regions_addr + size);
	uint64_t base_address = 0, mapped_address, mapped_size;
	struct virtio_net *dev;

	dev = get_device(ctx.vid);
	if (dev == NULL)
		return -1;

	if (dev->mem && dev->mem->mapped_address) {
		munmap((void *)(uintptr_t)dev->mem->mapped_address,
			(size_t)dev->mem->mapped_size);
		free(dev->mem);
		dev->mem = NULL;
	}

	dev->mem = calloc(1, sizeof(struct virtio_memory) +
		sizeof(struct virtio_memory_regions) * nregions);
	if (dev->mem == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to allocate memory for dev->mem\n",
			dev->vid);
		return -1;
	}

	pregion = &dev->mem->regions[0];

	for (idx = 0; idx < nregions; idx++) {
		pregion[idx].guest_phys_address =
			mem_regions[idx].guest_phys_addr;
		pregion[idx].guest_phys_address_end =
			pregion[idx].guest_phys_address +
			mem_regions[idx].memory_size;
		pregion[idx].memory_size =
			mem_regions[idx].memory_size;
		pregion[idx].userspace_address =
			mem_regions[idx].userspace_addr;

		LOG_DEBUG(VHOST_CONFIG,
			"REGION: %u - GPA: %p - QVA: %p - SIZE (%"PRIu64")\n",
			idx,
			(void *)(uintptr_t)pregion[idx].guest_phys_address,
			(void *)(uintptr_t)pregion[idx].userspace_address,
			pregion[idx].memory_size);

		/*set the base address mapping*/
		if (pregion[idx].guest_phys_address == 0x0) {
			base_address =
				pregion[idx].userspace_address;
			/* Map VM memory file */
			if (host_memory_map(ctx.pid, base_address,
				&mapped_address, &mapped_size) != 0) {
				free(dev->mem);
				dev->mem = NULL;
				return -1;
			}
			dev->mem->mapped_address = mapped_address;
			dev->mem->base_address = base_address;
			dev->mem->mapped_size = mapped_size;
		}
	}

	/* Check that we have a valid base address. */
	if (base_address == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find base address of qemu memory file.\n");
		free(dev->mem);
		dev->mem = NULL;
		return -1;
	}

	valid_regions = nregions;
	for (idx = 0; idx < nregions; idx++) {
		if ((pregion[idx].userspace_address < base_address) ||
			(pregion[idx].userspace_address >
			(base_address + mapped_size)))
			valid_regions--;
	}


	if (valid_regions != nregions) {
		valid_regions = 0;
		for (idx = nregions; 0 != idx--; ) {
			if ((pregion[idx].userspace_address < base_address) ||
			(pregion[idx].userspace_address >
			(base_address + mapped_size))) {
				memmove(&pregion[idx], &pregion[idx + 1],
					sizeof(struct virtio_memory_regions) *
					valid_regions);
			} else
				valid_regions++;
		}
	}

	for (idx = 0; idx < valid_regions; idx++) {
		pregion[idx].address_offset =
			mapped_address - base_address +
			pregion[idx].userspace_address -
			pregion[idx].guest_phys_address;
	}
	dev->mem->nregions = valid_regions;

	return 0;
}

/*
 * Function to get the tap device name from the provided file descriptor and
 * save it in the device structure.
 */
static int
get_ifname(int vid, int tap_fd, int pid)
{
	int fd_tap;
	struct ifreq ifr;
	uint32_t ifr_size;
	int ret;

	fd_tap = eventfd_copy(tap_fd, pid);
	if (fd_tap < 0)
		return -1;

	ret = ioctl(fd_tap, TUNGETIFF, &ifr);

	if (close(fd_tap) < 0)
		RTE_LOG(ERR, VHOST_CONFIG, "(%d) fd close failed\n", vid);

	if (ret >= 0) {
		ifr_size = strnlen(ifr.ifr_name, sizeof(ifr.ifr_name));
		vhost_set_ifname(vid, ifr.ifr_name, ifr_size);
	} else
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) TUNGETIFF ioctl failed\n", vid);

	return 0;
}

int
cuse_set_backend(struct vhost_cuse_device_ctx ctx,
		 struct vhost_vring_file *file)
{
	struct virtio_net *dev;

	dev = get_device(ctx.vid);
	if (dev == NULL)
		return -1;

	if (!(dev->flags & VIRTIO_DEV_RUNNING) && file->fd != VIRTIO_DEV_STOPPED)
		get_ifname(ctx.vid, file->fd, ctx.pid);

	return vhost_set_backend(ctx.vid, file);
}

void
vhost_backend_cleanup(struct virtio_net *dev)
{
	/* Unmap QEMU memory file if mapped. */
	if (dev->mem) {
		munmap((void *)(uintptr_t)dev->mem->mapped_address,
			(size_t)dev->mem->mapped_size);
		free(dev->mem);
		dev->mem = NULL;
	}
}
