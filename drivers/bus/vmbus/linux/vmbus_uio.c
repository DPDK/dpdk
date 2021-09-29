/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_memory.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_bus_vmbus.h>
#include <rte_string_fns.h>

#include "private.h"

/** Pathname of VMBUS devices directory. */
#define SYSFS_VMBUS_DEVICES "/sys/bus/vmbus/devices"

static void *vmbus_map_addr;

/* Control interrupts */
void vmbus_uio_irq_control(struct rte_vmbus_device *dev, int32_t onoff)
{
	if (write(dev->intr_handle.fd, &onoff, sizeof(onoff)) < 0) {
		VMBUS_LOG(ERR, "cannot write to %d:%s",
			dev->intr_handle.fd, strerror(errno));
	}
}

int vmbus_uio_irq_read(struct rte_vmbus_device *dev)
{
	int32_t count;
	int cc;

	cc = read(dev->intr_handle.fd, &count, sizeof(count));
	if (cc < (int)sizeof(count)) {
		if (cc < 0) {
			VMBUS_LOG(ERR, "IRQ read failed %s",
				  strerror(errno));
			return -errno;
		}
		VMBUS_LOG(ERR, "can't read IRQ count");
		return -EINVAL;
	}

	return count;
}

void
vmbus_uio_free_resource(struct rte_vmbus_device *dev,
		struct mapped_vmbus_resource *uio_res)
{
	rte_free(uio_res);

	if (dev->intr_handle.uio_cfg_fd >= 0) {
		close(dev->intr_handle.uio_cfg_fd);
		dev->intr_handle.uio_cfg_fd = -1;
	}

	if (dev->intr_handle.fd >= 0) {
		close(dev->intr_handle.fd);
		dev->intr_handle.fd = -1;
		dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
	}
}

int
vmbus_uio_alloc_resource(struct rte_vmbus_device *dev,
			 struct mapped_vmbus_resource **uio_res)
{
	char devname[PATH_MAX]; /* contains the /dev/uioX */

	/* save fd if in primary process */
	snprintf(devname, sizeof(devname), "/dev/uio%u", dev->uio_num);
	dev->intr_handle.fd = open(devname, O_RDWR);
	if (dev->intr_handle.fd < 0) {
		VMBUS_LOG(ERR, "Cannot open %s: %s",
			devname, strerror(errno));
		goto error;
	}
	dev->intr_handle.type = RTE_INTR_HANDLE_UIO_INTX;

	/* allocate the mapping details for secondary processes*/
	*uio_res = rte_zmalloc("UIO_RES", sizeof(**uio_res), 0);
	if (*uio_res == NULL) {
		VMBUS_LOG(ERR, "cannot store uio mmap details");
		goto error;
	}

	strlcpy((*uio_res)->path, devname, PATH_MAX);
	rte_uuid_copy((*uio_res)->id, dev->device_id);

	return 0;

error:
	vmbus_uio_free_resource(dev, *uio_res);
	return -1;
}

static int
find_max_end_va(const struct rte_memseg_list *msl, void *arg)
{
	size_t sz = msl->memseg_arr.len * msl->page_sz;
	void *end_va = RTE_PTR_ADD(msl->base_va, sz);
	void **max_va = arg;

	if (*max_va < end_va)
		*max_va = end_va;
	return 0;
}

/*
 * TODO: this should be part of memseg api.
 *       code is duplicated from PCI.
 */
static void *
vmbus_find_max_end_va(void)
{
	void *va = NULL;

	rte_memseg_list_walk(find_max_end_va, &va);
	return va;
}

int
vmbus_uio_map_resource_by_index(struct rte_vmbus_device *dev, int idx,
				struct mapped_vmbus_resource *uio_res,
				int flags)
{
	size_t size = dev->resource[idx].len;
	struct vmbus_map *maps = uio_res->maps;
	void *mapaddr;
	off_t offset;
	int fd;

	/* devname for mmap  */
	fd = open(uio_res->path, O_RDWR);
	if (fd < 0) {
		VMBUS_LOG(ERR, "Cannot open %s: %s",
			  uio_res->path, strerror(errno));
		return -1;
	}

	/* try mapping somewhere close to the end of hugepages */
	if (vmbus_map_addr == NULL)
		vmbus_map_addr = vmbus_find_max_end_va();

	/* offset is special in uio it indicates which resource */
	offset = idx * PAGE_SIZE;

	mapaddr = vmbus_map_resource(vmbus_map_addr, fd, offset, size, flags);
	close(fd);

	if (mapaddr == MAP_FAILED)
		return -1;

	dev->resource[idx].addr = mapaddr;
	vmbus_map_addr = RTE_PTR_ADD(mapaddr, size);

	/* Record result of successful mapping for use by secondary */
	maps[idx].addr = mapaddr;
	maps[idx].size = size;

	return 0;
}

static int vmbus_uio_map_primary(struct vmbus_channel *chan,
				 void **ring_buf, uint32_t *ring_size)
{
	struct mapped_vmbus_resource *uio_res;

	uio_res = vmbus_uio_find_resource(chan->device);
	if (!uio_res) {
		VMBUS_LOG(ERR, "can not find resources!");
		return -ENOMEM;
	}

	if (uio_res->nb_maps < VMBUS_MAX_RESOURCE) {
		VMBUS_LOG(ERR, "VMBUS: only %u resources found!",
			  uio_res->nb_maps);
		return -EINVAL;
	}

	*ring_size = uio_res->maps[HV_TXRX_RING_MAP].size / 2;
	*ring_buf  = uio_res->maps[HV_TXRX_RING_MAP].addr;
	return 0;
}

static int vmbus_uio_map_subchan(const struct rte_vmbus_device *dev,
				 const struct vmbus_channel *chan,
				 void **ring_buf, uint32_t *ring_size)
{
	char ring_path[PATH_MAX];
	size_t file_size;
	struct stat sb;
	void *mapaddr;
	int fd;
	struct mapped_vmbus_resource *uio_res;
	int channel_idx;

	uio_res = vmbus_uio_find_resource(dev);
	if (!uio_res) {
		VMBUS_LOG(ERR, "can not find resources for mapping subchan");
		return -ENOMEM;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (uio_res->nb_subchannels >= UIO_MAX_SUBCHANNEL) {
			VMBUS_LOG(ERR,
				"exceeding max subchannels UIO_MAX_SUBCHANNEL(%d)",
				UIO_MAX_SUBCHANNEL);
			VMBUS_LOG(ERR, "Change UIO_MAX_SUBCHANNEL and recompile");
			return -ENOMEM;
		}
	} else {
		for (channel_idx = 0; channel_idx < uio_res->nb_subchannels;
		     channel_idx++)
			if (uio_res->subchannel_maps[channel_idx].relid ==
					chan->relid)
				break;
		if (channel_idx == uio_res->nb_subchannels) {
			VMBUS_LOG(ERR,
				"couldn't find sub channel %d from shared mapping in primary",
				chan->relid);
			return -ENOMEM;
		}
		vmbus_map_addr = uio_res->subchannel_maps[channel_idx].addr;
	}

	snprintf(ring_path, sizeof(ring_path),
		 "%s/%s/channels/%u/ring",
		 SYSFS_VMBUS_DEVICES, dev->device.name,
		 chan->relid);

	fd = open(ring_path, O_RDWR);
	if (fd < 0) {
		VMBUS_LOG(ERR, "Cannot open %s: %s",
			  ring_path, strerror(errno));
		return -errno;
	}

	if (fstat(fd, &sb) < 0) {
		VMBUS_LOG(ERR, "Cannot state %s: %s",
			  ring_path, strerror(errno));
		close(fd);
		return -errno;
	}
	file_size = sb.st_size;

	if (file_size == 0 || (file_size & (PAGE_SIZE - 1))) {
		VMBUS_LOG(ERR, "incorrect size %s: %zu",
			  ring_path, file_size);

		close(fd);
		return -EINVAL;
	}

	mapaddr = vmbus_map_resource(vmbus_map_addr, fd,
				     0, file_size, 0);
	close(fd);

	if (mapaddr == MAP_FAILED)
		return -EIO;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {

		/* Add this mapping to uio_res for use by secondary */
		uio_res->subchannel_maps[uio_res->nb_subchannels].relid =
			chan->relid;
		uio_res->subchannel_maps[uio_res->nb_subchannels].addr =
			mapaddr;
		uio_res->subchannel_maps[uio_res->nb_subchannels].size =
			file_size;
		uio_res->nb_subchannels++;

		vmbus_map_addr = RTE_PTR_ADD(mapaddr, file_size);
	} else {
		if (mapaddr != vmbus_map_addr) {
			VMBUS_LOG(ERR, "failed to map channel %d to addr %p",
					chan->relid, mapaddr);
			vmbus_unmap_resource(mapaddr, file_size);
			return -EIO;
		}
	}

	*ring_size = file_size / 2;
	*ring_buf = mapaddr;

	return 0;
}

int vmbus_uio_map_rings(struct vmbus_channel *chan)
{
	const struct rte_vmbus_device *dev = chan->device;
	uint32_t ring_size;
	void *ring_buf;
	int ret;

	/* Primary channel */
	if (chan->subchannel_id == 0)
		ret = vmbus_uio_map_primary(chan, &ring_buf, &ring_size);
	else
		ret = vmbus_uio_map_subchan(dev, chan, &ring_buf, &ring_size);

	if (ret)
		return ret;

	vmbus_br_setup(&chan->txbr, ring_buf, ring_size);
	vmbus_br_setup(&chan->rxbr, (char *)ring_buf + ring_size, ring_size);
	return 0;
}

static int vmbus_uio_sysfs_read(const char *dir, const char *name,
				unsigned long *val, unsigned long max_range)
{
	char path[PATH_MAX];
	FILE *f;
	int ret;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	f = fopen(path, "r");
	if (!f) {
		VMBUS_LOG(ERR, "can't open %s:%s",
			  path, strerror(errno));
		return -errno;
	}

	if (fscanf(f, "%lu", val) != 1)
		ret = -EIO;
	else if (*val > max_range)
		ret = -ERANGE;
	else
		ret = 0;
	fclose(f);

	return ret;
}

static bool vmbus_uio_ring_present(const struct rte_vmbus_device *dev,
				   uint32_t relid)
{
	char ring_path[PATH_MAX];

	/* Check if kernel has subchannel sysfs files */
	snprintf(ring_path, sizeof(ring_path),
		 "%s/%s/channels/%u/ring",
		 SYSFS_VMBUS_DEVICES, dev->device.name, relid);

	return access(ring_path, R_OK|W_OK) == 0;
}

bool vmbus_uio_subchannels_supported(const struct rte_vmbus_device *dev,
				     const struct vmbus_channel *chan)
{
	return vmbus_uio_ring_present(dev, chan->relid);
}

static bool vmbus_isnew_subchannel(struct vmbus_channel *primary,
				   unsigned long id)
{
	const struct vmbus_channel *c;

	STAILQ_FOREACH(c, &primary->subchannel_list, next) {
		if (c->relid == id)
			return false;
	}
	return true;
}

int vmbus_uio_get_subchan(struct vmbus_channel *primary,
			  struct vmbus_channel **subchan)
{
	const struct rte_vmbus_device *dev = primary->device;
	char chan_path[PATH_MAX], subchan_path[PATH_MAX];
	struct dirent *ent;
	DIR *chan_dir;
	int err;

	snprintf(chan_path, sizeof(chan_path),
		 "%s/%s/channels",
		 SYSFS_VMBUS_DEVICES, dev->device.name);

	chan_dir = opendir(chan_path);
	if (!chan_dir) {
		VMBUS_LOG(ERR, "cannot open %s: %s",
			  chan_path, strerror(errno));
		return -errno;
	}

	while ((ent = readdir(chan_dir))) {
		unsigned long relid, subid, monid;
		char *endp;

		if (ent->d_name[0] == '.')
			continue;

		errno = 0;
		relid = strtoul(ent->d_name, &endp, 0);
		if (*endp || errno != 0 || relid > UINT16_MAX) {
			VMBUS_LOG(NOTICE, "not a valid channel relid: %s",
				  ent->d_name);
			continue;
		}

		if (!vmbus_isnew_subchannel(primary, relid)) {
			VMBUS_LOG(DEBUG, "skip already found channel: %lu",
				  relid);
			continue;
		}

		if (!vmbus_uio_ring_present(dev, relid)) {
			VMBUS_LOG(DEBUG, "ring mmap not found (yet) for: %lu",
				  relid);
			continue;
		}

		snprintf(subchan_path, sizeof(subchan_path), "%s/%lu",
			 chan_path, relid);
		err = vmbus_uio_sysfs_read(subchan_path, "subchannel_id",
					   &subid, UINT16_MAX);
		if (err) {
			VMBUS_LOG(NOTICE, "no subchannel_id in %s:%s",
				  subchan_path, strerror(-err));
			goto fail;
		}

		if (subid == 0)
			continue;	/* skip primary channel */

		err = vmbus_uio_sysfs_read(subchan_path, "monitor_id",
					   &monid, UINT8_MAX);
		if (err) {
			VMBUS_LOG(NOTICE, "no monitor_id in %s:%s",
				  subchan_path, strerror(-err));
			goto fail;
		}

		err = vmbus_chan_create(dev, relid, subid, monid, subchan);
		if (err) {
			VMBUS_LOG(ERR, "subchannel setup failed");
			goto fail;
		}
		break;
	}
	closedir(chan_dir);

	return (ent == NULL) ? -ENOENT : 0;
fail:
	closedir(chan_dir);
	return err;
}
