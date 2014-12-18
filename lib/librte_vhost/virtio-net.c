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

#include <dirent.h>
#include <fuse/cuse_lowlevel.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/if_tun.h>
#include <linux/if.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_memory.h>
#include <rte_virtio_net.h>

#include "vhost-net-cdev.h"
#include "eventfd_link/eventfd_link.h"

/*
 * Device linked list structure for configuration.
 */
struct virtio_net_config_ll {
	struct virtio_net dev;			/* Virtio device.*/
	struct virtio_net_config_ll *next;	/* Next dev on linked list.*/
};

const char eventfd_cdev[] = "/dev/eventfd-link";

/* device ops to add/remove device to/from data core. */
static struct virtio_net_device_ops const *notify_ops;
/* root address of the linked list of managed virtio devices */
static struct virtio_net_config_ll *ll_root;

/* Features supported by this lib. */
#define VHOST_SUPPORTED_FEATURES ((1ULL << VIRTIO_NET_F_MRG_RXBUF) | \
				  (1ULL << VIRTIO_NET_F_CTRL_RX))
static uint64_t VHOST_FEATURES = VHOST_SUPPORTED_FEATURES;

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
 * Converts QEMU virtual address to Vhost virtual address. This function is
 * used to convert the ring addresses to our address space.
 */
static uint64_t
qva_to_vva(struct virtio_net *dev, uint64_t qemu_va)
{
	struct virtio_memory_regions *region;
	uint64_t vhost_va = 0;
	uint32_t regionidx = 0;

	/* Find the region where the address lives. */
	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		region = &dev->mem->regions[regionidx];
		if ((qemu_va >= region->userspace_address) &&
			(qemu_va <= region->userspace_address +
			region->memory_size)) {
			vhost_va = dev->mem->mapped_address + qemu_va -
					dev->mem->base_address;
			break;
		}
	}
	return vhost_va;
}

/*
 * Locate the file containing QEMU's memory space and
 * map it to our address space.
 */
static int
host_memory_map(struct virtio_net *dev, struct virtio_memory *mem,
	pid_t pid, uint64_t addr)
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
			"(%"PRIu64") Failed to open maps file for pid %d\n",
			dev->device_fh, pid);
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
			"(%"PRIu64") Failed to find memory file in pid %d maps file\n",
			dev->device_fh, pid);
		return -1;
	}

	/* Find the guest memory file among the process fds. */
	dp = opendir(procdir);
	if (dp == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Cannot open pid %d process directory\n",
			dev->device_fh, pid);
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
				"(%"PRIu64") Failed to resolve fd directory\n",
				dev->device_fh);
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
			"(%"PRIu64") Failed to find memory file for pid %d\n",
			dev->device_fh, pid);
		return -1;
	}
	/* Open the shared memory file and map the memory into this process. */
	fd = open(memfile, O_RDWR);

	if (fd == -1) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to open %s for pid %d\n",
			dev->device_fh, memfile, pid);
		return -1;
	}

	map = mmap(0, (size_t)procmap.len, PROT_READ|PROT_WRITE,
		MAP_POPULATE|MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Error mapping the file %s for pid %d\n",
			dev->device_fh, memfile, pid);
		return -1;
	}

	/* Store the memory address and size in the device data structure */
	mem->mapped_address = (uint64_t)(uintptr_t)map;
	mem->mapped_size = procmap.len;

	LOG_DEBUG(VHOST_CONFIG,
		"(%"PRIu64") Mem File: %s->%s - Size: %llu - VA: %p\n",
		dev->device_fh,
		memfile, resolved_path,
		(unsigned long long)mem->mapped_size, map);

	return 0;
}

/*
 * Retrieves an entry from the devices configuration linked list.
 */
static struct virtio_net_config_ll *
get_config_ll_entry(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* Loop through linked list until the device_fh is found. */
	while (ll_dev != NULL) {
		if (ll_dev->dev.device_fh == ctx.fh)
			return ll_dev;
		ll_dev = ll_dev->next;
	}

	return NULL;
}

/*
 * Searches the configuration core linked list and
 * retrieves the device if it exists.
 */
static struct virtio_net *
get_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev;

	ll_dev = get_config_ll_entry(ctx);

	if (ll_dev)
		return &ll_dev->dev;

	RTE_LOG(ERR, VHOST_CONFIG,
		"(%"PRIu64") Device not found in linked list.\n", ctx.fh);
	return NULL;
}

/*
 * Add entry containing a device to the device configuration linked list.
 */
static void
add_config_ll_entry(struct virtio_net_config_ll *new_ll_dev)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* If ll_dev == NULL then this is the first device so go to else */
	if (ll_dev) {
		/* If the 1st device_fh != 0 then we insert our device here. */
		if (ll_dev->dev.device_fh != 0) {
			new_ll_dev->dev.device_fh = 0;
			new_ll_dev->next = ll_dev;
			ll_root = new_ll_dev;
		} else {
			/*
			 * Increment through the ll until we find un unused
			 * device_fh. Insert the device at that entry.
			 */
			while ((ll_dev->next != NULL) &&
				(ll_dev->dev.device_fh ==
					(ll_dev->next->dev.device_fh - 1)))
				ll_dev = ll_dev->next;

			new_ll_dev->dev.device_fh = ll_dev->dev.device_fh + 1;
			new_ll_dev->next = ll_dev->next;
			ll_dev->next = new_ll_dev;
		}
	} else {
		ll_root = new_ll_dev;
		ll_root->dev.device_fh = 0;
	}

}

/*
 * Unmap any memory, close any file descriptors and
 * free any memory owned by a device.
 */
static void
cleanup_device(struct virtio_net *dev)
{
	/* Unmap QEMU memory file if mapped. */
	if (dev->mem) {
		munmap((void *)(uintptr_t)dev->mem->mapped_address,
			(size_t)dev->mem->mapped_size);
		free(dev->mem);
	}

	/* Close any event notifiers opened by device. */
	if (dev->virtqueue[VIRTIO_RXQ]->callfd)
		close((int)dev->virtqueue[VIRTIO_RXQ]->callfd);
	if (dev->virtqueue[VIRTIO_RXQ]->kickfd)
		close((int)dev->virtqueue[VIRTIO_RXQ]->kickfd);
	if (dev->virtqueue[VIRTIO_TXQ]->callfd)
		close((int)dev->virtqueue[VIRTIO_TXQ]->callfd);
	if (dev->virtqueue[VIRTIO_TXQ]->kickfd)
		close((int)dev->virtqueue[VIRTIO_TXQ]->kickfd);
}

/*
 * Release virtqueues and device memory.
 */
static void
free_device(struct virtio_net_config_ll *ll_dev)
{
	/* Free any malloc'd memory */
	free(ll_dev->dev.virtqueue[VIRTIO_RXQ]);
	free(ll_dev->dev.virtqueue[VIRTIO_TXQ]);
	free(ll_dev);
}

/*
 * Remove an entry from the device configuration linked list.
 */
static struct virtio_net_config_ll *
rm_config_ll_entry(struct virtio_net_config_ll *ll_dev,
	struct virtio_net_config_ll *ll_dev_last)
{
	/* First remove the device and then clean it up. */
	if (ll_dev == ll_root) {
		ll_root = ll_dev->next;
		cleanup_device(&ll_dev->dev);
		free_device(ll_dev);
		return ll_root;
	} else {
		if (likely(ll_dev_last != NULL)) {
			ll_dev_last->next = ll_dev->next;
			cleanup_device(&ll_dev->dev);
			free_device(ll_dev);
			return ll_dev_last->next;
		} else {
			cleanup_device(&ll_dev->dev);
			free_device(ll_dev);
			RTE_LOG(ERR, VHOST_CONFIG,
				"Remove entry from config_ll failed\n");
			return NULL;
		}
	}
}

/*
 *  Initialise all variables in device structure.
 */
static void
init_device(struct virtio_net *dev)
{
	uint64_t vq_offset;

	/*
	 * Virtqueues have already been malloced so
	 * we don't want to set them to NULL.
	 */
	vq_offset = offsetof(struct virtio_net, mem);

	/* Set everything to 0. */
	memset((void *)(uintptr_t)((uint64_t)(uintptr_t)dev + vq_offset), 0,
		(sizeof(struct virtio_net) - (size_t)vq_offset));
	memset(dev->virtqueue[VIRTIO_RXQ], 0, sizeof(struct vhost_virtqueue));
	memset(dev->virtqueue[VIRTIO_TXQ], 0, sizeof(struct vhost_virtqueue));

	/* Backends are set to -1 indicating an inactive device. */
	dev->virtqueue[VIRTIO_RXQ]->backend = VIRTIO_DEV_STOPPED;
	dev->virtqueue[VIRTIO_TXQ]->backend = VIRTIO_DEV_STOPPED;
}

/*
 * Function is called from the CUSE open function. The device structure is
 * initialised and a new entry is added to the device configuration linked
 * list.
 */
static int
new_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *new_ll_dev;
	struct vhost_virtqueue *virtqueue_rx, *virtqueue_tx;

	/* Setup device and virtqueues. */
	new_ll_dev = malloc(sizeof(struct virtio_net_config_ll));
	if (new_ll_dev == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for dev.\n",
			ctx.fh);
		return -1;
	}

	virtqueue_rx = malloc(sizeof(struct vhost_virtqueue));
	if (virtqueue_rx == NULL) {
		free(new_ll_dev);
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for rxq.\n",
			ctx.fh);
		return -1;
	}

	virtqueue_tx = malloc(sizeof(struct vhost_virtqueue));
	if (virtqueue_tx == NULL) {
		free(virtqueue_rx);
		free(new_ll_dev);
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for txq.\n",
			ctx.fh);
		return -1;
	}

	new_ll_dev->dev.virtqueue[VIRTIO_RXQ] = virtqueue_rx;
	new_ll_dev->dev.virtqueue[VIRTIO_TXQ] = virtqueue_tx;

	/* Initialise device and virtqueues. */
	init_device(&new_ll_dev->dev);

	new_ll_dev->next = NULL;

	/* Add entry to device configuration linked list. */
	add_config_ll_entry(new_ll_dev);

	return new_ll_dev->dev.device_fh;
}

/*
 * Function is called from the CUSE release function. This function will
 * cleanup the device and remove it from device configuration linked list.
 */
static void
destroy_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev_cur_ctx, *ll_dev_last = NULL;
	struct virtio_net_config_ll *ll_dev_cur = ll_root;

	/* Find the linked list entry for the device to be removed. */
	ll_dev_cur_ctx = get_config_ll_entry(ctx);
	while (ll_dev_cur != NULL) {
		/*
		 * If the device is found or
		 * a device that doesn't exist is found then it is removed.
		 */
		if (ll_dev_cur == ll_dev_cur_ctx) {
			/*
			 * If the device is running on a data core then call
			 * the function to remove it from the data core.
			 */
			if ((ll_dev_cur->dev.flags & VIRTIO_DEV_RUNNING))
				notify_ops->destroy_device(&(ll_dev_cur->dev));
			ll_dev_cur = rm_config_ll_entry(ll_dev_cur,
					ll_dev_last);
		} else {
			ll_dev_last = ll_dev_cur;
			ll_dev_cur = ll_dev_cur->next;
		}
	}
}

/*
 * Called from CUSE IOCTL: VHOST_SET_OWNER
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
static int
set_owner(struct vhost_device_ctx ctx)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_RESET_OWNER
 */
static int
reset_owner(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev;

	ll_dev = get_config_ll_entry(ctx);

	cleanup_device(&ll_dev->dev);
	init_device(&ll_dev->dev);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_FEATURES
 * The features that we support are requested.
 */
static int
get_features(struct vhost_device_ctx ctx, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* Send our supported features. */
	*pu = VHOST_FEATURES;
	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_FEATURES
 * We receive the negotiated features supported by us and the virtio device.
 */
static int
set_features(struct vhost_device_ctx ctx, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;
	if (*pu & ~VHOST_FEATURES)
		return -1;

	/* Store the negotiated feature list for the device. */
	dev->features = *pu;

	/* Set the vhost_hlen depending on if VIRTIO_NET_F_MRG_RXBUF is set. */
	if (dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF)) {
		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") Mergeable RX buffers enabled\n",
			dev->device_fh);
		dev->virtqueue[VIRTIO_RXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr_mrg_rxbuf);
		dev->virtqueue[VIRTIO_TXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") Mergeable RX buffers disabled\n",
			dev->device_fh);
		dev->virtqueue[VIRTIO_RXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr);
		dev->virtqueue[VIRTIO_TXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr);
	}
	return 0;
}


/*
 * Called from CUSE IOCTL: VHOST_SET_MEM_TABLE
 * This function creates and populates the memory structure for the device.
 * This includes storing offsets used to translate buffer addresses.
 */
static int
set_mem_table(struct vhost_device_ctx ctx, const void *mem_regions_addr,
	uint32_t nregions)
{
	struct virtio_net *dev;
	struct vhost_memory_region *mem_regions;
	struct virtio_memory *mem;
	uint64_t size = offsetof(struct vhost_memory, regions);
	uint32_t regionidx, valid_regions;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	if (dev->mem) {
		munmap((void *)(uintptr_t)dev->mem->mapped_address,
			(size_t)dev->mem->mapped_size);
		free(dev->mem);
	}

	/* Malloc the memory structure depending on the number of regions. */
	mem = calloc(1, sizeof(struct virtio_memory) +
		(sizeof(struct virtio_memory_regions) * nregions));
	if (mem == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for dev->mem.\n",
			dev->device_fh);
		return -1;
	}

	mem->nregions = nregions;

	mem_regions = (void *)(uintptr_t)
			((uint64_t)(uintptr_t)mem_regions_addr + size);

	for (regionidx = 0; regionidx < mem->nregions; regionidx++) {
		/* Populate the region structure for each region. */
		mem->regions[regionidx].guest_phys_address =
			mem_regions[regionidx].guest_phys_addr;
		mem->regions[regionidx].guest_phys_address_end =
			mem->regions[regionidx].guest_phys_address +
			mem_regions[regionidx].memory_size;
		mem->regions[regionidx].memory_size =
			mem_regions[regionidx].memory_size;
		mem->regions[regionidx].userspace_address =
			mem_regions[regionidx].userspace_addr;

		LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") REGION: %u - GPA: %p - QEMU VA: %p - SIZE (%"PRIu64")\n", dev->device_fh,
			regionidx,
			(void *)(uintptr_t)mem->regions[regionidx].guest_phys_address,
			(void *)(uintptr_t)mem->regions[regionidx].userspace_address,
			mem->regions[regionidx].memory_size);

		/*set the base address mapping*/
		if (mem->regions[regionidx].guest_phys_address == 0x0) {
			mem->base_address =
				mem->regions[regionidx].userspace_address;
			/* Map VM memory file */
			if (host_memory_map(dev, mem, ctx.pid,
				mem->base_address) != 0) {
				free(mem);
				return -1;
			}
		}
	}

	/* Check that we have a valid base address. */
	if (mem->base_address == 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "(%"PRIu64") Failed to find base address of qemu memory file.\n", dev->device_fh);
		free(mem);
		return -1;
	}

	/*
	 * Check if all of our regions have valid mappings.
	 * Usually one does not exist in the QEMU memory file.
	 */
	valid_regions = mem->nregions;
	for (regionidx = 0; regionidx < mem->nregions; regionidx++) {
		if ((mem->regions[regionidx].userspace_address <
			mem->base_address) ||
			(mem->regions[regionidx].userspace_address >
			(mem->base_address + mem->mapped_size)))
				valid_regions--;
	}

	/*
	 * If a region does not have a valid mapping,
	 * we rebuild our memory struct to contain only valid entries.
	 */
	if (valid_regions != mem->nregions) {
		LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") Not all memory regions exist in the QEMU mem file. Re-populating mem structure\n",
			dev->device_fh);

		/*
		 * Re-populate the memory structure with only valid regions.
		 * Invalid regions are over-written with memmove.
		 */
		valid_regions = 0;

		for (regionidx = mem->nregions; 0 != regionidx--;) {
			if ((mem->regions[regionidx].userspace_address <
				mem->base_address) ||
				(mem->regions[regionidx].userspace_address >
				(mem->base_address + mem->mapped_size))) {
				memmove(&mem->regions[regionidx],
					&mem->regions[regionidx + 1],
					sizeof(struct virtio_memory_regions) *
						valid_regions);
			} else {
				valid_regions++;
			}
		}
	}
	mem->nregions = valid_regions;
	dev->mem = mem;

	/*
	 * Calculate the address offset for each region.
	 * This offset is used to identify the vhost virtual address
	 * corresponding to a QEMU guest physical address.
	 */
	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		dev->mem->regions[regionidx].address_offset =
			dev->mem->regions[regionidx].userspace_address -
				dev->mem->base_address +
				dev->mem->mapped_address -
				dev->mem->regions[regionidx].guest_phys_address;

	}
	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_NUM
 * The virtio device sends us the size of the descriptor ring.
 */
static int
set_vring_num(struct vhost_device_ctx ctx, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->size = state->num;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_ADDR
 * The virtio device sends us the desc, used and avail ring addresses.
 * This function then converts these to our address space.
 */
static int
set_vring_addr(struct vhost_device_ctx ctx, struct vhost_vring_addr *addr)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* addr->index refers to the queue index. The txq 1, rxq is 0. */
	vq = dev->virtqueue[addr->index];

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	vq->desc = (struct vring_desc *)(uintptr_t)qva_to_vva(dev,
			addr->desc_user_addr);
	if (vq->desc == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find desc ring address.\n",
			dev->device_fh);
		return -1;
	}

	vq->avail = (struct vring_avail *)(uintptr_t)qva_to_vva(dev,
			addr->avail_user_addr);
	if (vq->avail == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find avail ring address.\n",
			dev->device_fh);
		return -1;
	}

	vq->used = (struct vring_used *)(uintptr_t)qva_to_vva(dev,
			addr->used_user_addr);
	if (vq->used == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find used ring address.\n",
			dev->device_fh);
		return -1;
	}

	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address desc: %p\n",
			dev->device_fh, vq->desc);
	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address avail: %p\n",
			dev->device_fh, vq->avail);
	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address used: %p\n",
			dev->device_fh, vq->used);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_BASE
 * The virtio device sends us the available ring last used index.
 */
static int
set_vring_base(struct vhost_device_ctx ctx, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->last_used_idx = state->num;
	dev->virtqueue[state->index]->last_used_idx_res = state->num;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_VRING_BASE
 * We send the virtio device our available ring last used index.
 */
static int
get_vring_base(struct vhost_device_ctx ctx, uint32_t index,
	struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	state->index = index;
	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	state->num = dev->virtqueue[state->index]->last_used_idx;

	return 0;
}

/*
 * This function uses the eventfd_link kernel module to copy an eventfd file
 * descriptor provided by QEMU in to our process space.
 */
static int
eventfd_copy(struct virtio_net *dev, struct eventfd_copy *eventfd_copy)
{
	int eventfd_link, ret;

	/* Open the character device to the kernel module. */
	eventfd_link = open(eventfd_cdev, O_RDWR);
	if (eventfd_link < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") eventfd_link module is not loaded\n",
			dev->device_fh);
		return -1;
	}

	/* Call the IOCTL to copy the eventfd. */
	ret = ioctl(eventfd_link, EVENTFD_COPY, eventfd_copy);
	close(eventfd_link);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") EVENTFD_COPY ioctl failed\n",
			dev->device_fh);
		return -1;
	}

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_CALL
 * The virtio device sends an eventfd to interrupt the guest. This fd gets
 * copied into our process space.
 */
static int
set_vring_call(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct eventfd_copy	eventfd_kick;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];

	if (vq->kickfd)
		close((int)vq->kickfd);

	/* Populate the eventfd_copy structure and call eventfd_copy. */
	vq->kickfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	eventfd_kick.source_fd = vq->kickfd;
	eventfd_kick.target_fd = file->fd;
	eventfd_kick.target_pid = ctx.pid;

	if (eventfd_copy(dev, &eventfd_kick))
		return -1;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_KICK
 * The virtio device sends an eventfd that it can use to notify us.
 * This fd gets copied into our process space.
 */
static int
set_vring_kick(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct eventfd_copy eventfd_call;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];

	if (vq->callfd)
		close((int)vq->callfd);

	/* Populate the eventfd_copy structure and call eventfd_copy. */
	vq->callfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	eventfd_call.source_fd = vq->callfd;
	eventfd_call.target_fd = file->fd;
	eventfd_call.target_pid = ctx.pid;

	if (eventfd_copy(dev, &eventfd_call))
		return -1;

	return 0;
}

/*
 * Function to get the tap device name from the provided file descriptor and
 * save it in the device structure.
 */
static int
get_ifname(struct virtio_net *dev, int tap_fd, int pid)
{
	struct eventfd_copy fd_tap;
	struct ifreq ifr;
	uint32_t size, ifr_size;
	int ret;

	fd_tap.source_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	fd_tap.target_fd = tap_fd;
	fd_tap.target_pid = pid;

	if (eventfd_copy(dev, &fd_tap))
		return -1;

	ret = ioctl(fd_tap.source_fd, TUNGETIFF, &ifr);

	if (close(fd_tap.source_fd) < 0)
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") fd close failed\n",
			dev->device_fh);

	if (ret >= 0) {
		ifr_size = strnlen(ifr.ifr_name, sizeof(ifr.ifr_name));
		size = ifr_size > sizeof(dev->ifname) ?
				sizeof(dev->ifname) : ifr_size;

		strncpy(dev->ifname, ifr.ifr_name, size);
	} else
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") TUNGETIFF ioctl failed\n",
			dev->device_fh);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_NET_SET_BACKEND
 * To complete device initialisation when the virtio driver is loaded,
 * we are provided with a valid fd for a tap device (not used by us).
 * If this happens then we can add the device to a data core.
 * When the virtio driver is removed we get fd=-1.
 * At that point we remove the device from the data core.
 * The device will still exist in the device configuration linked list.
 */
static int
set_backend(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[file->index]->backend = file->fd;

	/*
	 * If the device isn't already running and both backend fds are set,
	 * we add the device.
	 */
	if (!(dev->flags & VIRTIO_DEV_RUNNING)) {
		if (((int)dev->virtqueue[VIRTIO_TXQ]->backend != VIRTIO_DEV_STOPPED) &&
			((int)dev->virtqueue[VIRTIO_RXQ]->backend != VIRTIO_DEV_STOPPED)) {
			get_ifname(dev, file->fd, ctx.pid);
			return notify_ops->new_device(dev);
		}
	/* Otherwise we remove it. */
	} else
		if (file->fd == VIRTIO_DEV_STOPPED)
			notify_ops->destroy_device(dev);
	return 0;
}

/*
 * Function pointers are set for the device operations to allow CUSE to call
 * functions when an IOCTL, device_add or device_release is received.
 */
static const struct vhost_net_device_ops vhost_device_ops = {
	.new_device = new_device,
	.destroy_device = destroy_device,

	.get_features = get_features,
	.set_features = set_features,

	.set_mem_table = set_mem_table,

	.set_vring_num = set_vring_num,
	.set_vring_addr = set_vring_addr,
	.set_vring_base = set_vring_base,
	.get_vring_base = get_vring_base,

	.set_vring_kick = set_vring_kick,
	.set_vring_call = set_vring_call,

	.set_backend = set_backend,

	.set_owner = set_owner,
	.reset_owner = reset_owner,
};

/*
 * Called by main to setup callbacks when registering CUSE device.
 */
struct vhost_net_device_ops const *
get_virtio_net_callbacks(void)
{
	return &vhost_device_ops;
}

int rte_vhost_enable_guest_notification(struct virtio_net *dev,
	uint16_t queue_id, int enable)
{
	if (enable) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"guest notification isn't supported.\n");
		return -1;
	}

	dev->virtqueue[queue_id]->used->flags =
		enable ? 0 : VRING_USED_F_NO_NOTIFY;
	return 0;
}

uint64_t rte_vhost_feature_get(void)
{
	return VHOST_FEATURES;
}

int rte_vhost_feature_disable(uint64_t feature_mask)
{
	VHOST_FEATURES = VHOST_FEATURES & ~feature_mask;
	return 0;
}

int rte_vhost_feature_enable(uint64_t feature_mask)
{
	if ((feature_mask & VHOST_SUPPORTED_FEATURES) == feature_mask) {
		VHOST_FEATURES = VHOST_FEATURES | feature_mask;
		return 0;
	}
	return -1;
}

/*
 * Register ops so that we can add/remove device to data core.
 */
int
rte_vhost_driver_callback_register(struct virtio_net_device_ops const * const ops)
{
	notify_ops = ops;

	return 0;
}
