/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <rte_eal.h>
#include <rte_tailq.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_bus_vmbus.h>

#include "private.h"

static struct rte_tailq_elem vmbus_tailq = {
	.name = "VMBUS_RESOURCE_LIST",
};
EAL_REGISTER_TAILQ(vmbus_tailq)

struct mapped_vmbus_resource *
vmbus_uio_find_resource(const struct rte_vmbus_device *dev)
{
	struct mapped_vmbus_resource *uio_res;
	struct mapped_vmbus_res_list *uio_res_list =
			RTE_TAILQ_CAST(vmbus_tailq.head, mapped_vmbus_res_list);

	if (dev == NULL)
		return NULL;

	TAILQ_FOREACH(uio_res, uio_res_list, next) {
		if (rte_uuid_compare(uio_res->id, dev->device_id) == 0)
			return uio_res;
	}
	return NULL;
}

static int
vmbus_uio_map_secondary(struct rte_vmbus_device *dev)
{
	struct mapped_vmbus_resource *uio_res;
	struct vmbus_channel *chan;
	int fd, i;

	uio_res = vmbus_uio_find_resource(dev);
	if (!uio_res) {
		VMBUS_LOG(ERR,  "Cannot find resource for device");
		return -1;
	}

	/* open /dev/uioX */
	fd = open(uio_res->path, O_RDWR);
	if (fd < 0) {
		VMBUS_LOG(ERR, "Cannot open %s: %s",
			  uio_res->path, strerror(errno));
		return -1;
	}

	for (i = 0; i != uio_res->nb_maps; i++) {
		void *mapaddr;
		off_t offset = i * rte_mem_page_size();

		mapaddr = vmbus_map_resource(uio_res->maps[i].addr,
					     fd, offset,
					     uio_res->maps[i].size, 0);

		if (mapaddr == uio_res->maps[i].addr) {
			dev->resource[i].addr = mapaddr;
			continue;	/* successful map */
		}

		if (mapaddr == MAP_FAILED)
			VMBUS_LOG(ERR,
				  "mmap resource %d in secondary failed", i);
		else {
			VMBUS_LOG(ERR,
				  "mmap resource %d address mismatch", i);
			vmbus_unmap_resource(mapaddr, uio_res->maps[i].size);
		}

		close(fd);
		return -1;
	}

	/* fd is not needed in secondary process, close it */
	close(fd);

	/* Create and map primary channel */
	if (vmbus_chan_create(dev, dev->relid, 0,
					dev->monitor_id, &dev->primary)) {
		VMBUS_LOG(ERR, "cannot create primary channel");
		goto failed_primary;
	}

	/* Create and map sub channels */
	for (i = 0; i < uio_res->nb_subchannels; i++) {
		if (rte_vmbus_subchan_open(dev->primary, &chan)) {
			VMBUS_LOG(ERR,
				"failed to create subchannel at index %d", i);
			goto failed_secondary;
		}
	}

	return 0;

failed_secondary:
	while (!STAILQ_EMPTY(&dev->primary->subchannel_list)) {
		chan = STAILQ_FIRST(&dev->primary->subchannel_list);
		vmbus_unmap_resource(chan->txbr.vbr, chan->txbr.dsize * 2);
		rte_vmbus_chan_close(chan);
	}
	rte_vmbus_chan_close(dev->primary);

failed_primary:
	for (i = 0; i != uio_res->nb_maps; i++) {
		vmbus_unmap_resource(
				uio_res->maps[i].addr, uio_res->maps[i].size);
	}

	return -1;
}

static int
vmbus_uio_map_primary(struct rte_vmbus_device *dev)
{
	int i, ret;
	struct mapped_vmbus_resource *uio_res = NULL;
	struct mapped_vmbus_res_list *uio_res_list =
		RTE_TAILQ_CAST(vmbus_tailq.head, mapped_vmbus_res_list);

	/* allocate uio resource */
	ret = vmbus_uio_alloc_resource(dev, &uio_res);
	if (ret)
		return ret;

	/* Map the resources */
	for (i = 0; i < VMBUS_MAX_RESOURCE; i++) {
		/* stop at empty BAR */
		if (dev->resource[i].len == 0)
			break;

		ret = vmbus_uio_map_resource_by_index(dev, i, uio_res, 0);
		if (ret)
			goto error;
	}

	uio_res->nb_maps = i;

	TAILQ_INSERT_TAIL(uio_res_list, uio_res, next);

	return 0;
error:
	while (--i >= 0) {
		vmbus_unmap_resource(uio_res->maps[i].addr,
				(size_t)uio_res->maps[i].size);
	}
	vmbus_uio_free_resource(dev, uio_res);
	return -1;
}

/* map the VMBUS resource of a VMBUS device in virtual memory */
int
vmbus_uio_map_resource(struct rte_vmbus_device *dev)
{
	struct mapped_vmbus_resource *uio_res;
	int ret;

	/* TODO: handle rescind */
	if (rte_intr_fd_set(dev->intr_handle, -1))
		return -1;

	if (rte_intr_dev_fd_set(dev->intr_handle, -1))
		return -1;

	if (rte_intr_type_set(dev->intr_handle, RTE_INTR_HANDLE_UNKNOWN))
		return -1;

	/* secondary processes - use already recorded details */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		ret = vmbus_uio_map_secondary(dev);
	else
		ret = vmbus_uio_map_primary(dev);

	if (ret != 0)
		return ret;

	uio_res = vmbus_uio_find_resource(dev);
	if (!uio_res) {
		VMBUS_LOG(ERR, "can not find resources!");
		return -EIO;
	}

	if (uio_res->nb_maps <= HV_MON_PAGE_MAP) {
		VMBUS_LOG(ERR, "VMBUS: only %u resources found!",
			uio_res->nb_maps);
		return -EINVAL;
	}

	dev->int_page = (uint32_t *)((char *)uio_res->maps[HV_INT_PAGE_MAP].addr
				     + (rte_mem_page_size() >> 1));
	dev->monitor_page = uio_res->maps[HV_MON_PAGE_MAP].addr;
	return 0;
}

static void
vmbus_uio_unmap(struct mapped_vmbus_resource *uio_res)
{
	int i;

	if (uio_res == NULL)
		return;

	for (i = 0; i < uio_res->nb_subchannels; i++) {
		vmbus_unmap_resource(uio_res->subchannel_maps[i].addr,
				uio_res->subchannel_maps[i].size);
	}

	for (i = 0; i != uio_res->nb_maps; i++) {
		vmbus_unmap_resource(uio_res->maps[i].addr,
				     (size_t)uio_res->maps[i].size);
	}
}

/* unmap the VMBUS resource of a VMBUS device in virtual memory */
void
vmbus_uio_unmap_resource(struct rte_vmbus_device *dev)
{
	struct mapped_vmbus_resource *uio_res;
	struct mapped_vmbus_res_list *uio_res_list =
			RTE_TAILQ_CAST(vmbus_tailq.head, mapped_vmbus_res_list);

	if (dev == NULL)
		return;

	/* find an entry for the device */
	uio_res = vmbus_uio_find_resource(dev);
	if (uio_res == NULL)
		return;

	/* secondary processes - just free maps */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		vmbus_uio_unmap(uio_res);
		rte_free(dev->primary);
		return;
	}

	TAILQ_REMOVE(uio_res_list, uio_res, next);

	/* unmap all resources */
	vmbus_uio_unmap(uio_res);

	/* free uio resource */
	rte_free(uio_res);

	/* close fd if in primary process */
	if (rte_intr_fd_get(dev->intr_handle) >= 0)
		close(rte_intr_fd_get(dev->intr_handle));

	if (rte_intr_dev_fd_get(dev->intr_handle) >= 0) {
		close(rte_intr_dev_fd_get(dev->intr_handle));
		rte_intr_dev_fd_set(dev->intr_handle, -1);
	}

	rte_intr_fd_set(dev->intr_handle, -1);
	rte_intr_type_set(dev->intr_handle, RTE_INTR_HANDLE_UNKNOWN);
}
