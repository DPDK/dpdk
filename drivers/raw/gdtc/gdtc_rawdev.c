/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 ZTE Corporation
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_spinlock.h>
#include <rte_branch_prediction.h>

#include "gdtc_rawdev.h"

/*
 * User define:
 * ep_id-bit[15:12] vfunc_num-bit[11:4] func_num-bit[3:1] vfunc_active-bit0
 * host ep_id:5~8   zf ep_id:9
 */
#define ZXDH_GDMA_ZF_USER                       0x9000      /* ep4 pf0 */
#define ZXDH_GDMA_PF_NUM_SHIFT                  1
#define ZXDH_GDMA_VF_NUM_SHIFT                  4
#define ZXDH_GDMA_EP_ID_SHIFT                   12
#define ZXDH_GDMA_VF_EN                         1
#define ZXDH_GDMA_EPID_OFFSET                   5

/* Register offset */
#define ZXDH_GDMA_BASE_OFFSET                   0x100000
#define ZXDH_GDMA_EXT_ADDR_OFFSET               0x218
#define ZXDH_GDMA_CONTROL_OFFSET                0x230
#define ZXDH_GDMA_TC_CNT_OFFSET                 0x23c
#define ZXDH_GDMA_LLI_USER_OFFSET               0x228

#define ZXDH_GDMA_CHAN_FORCE_CLOSE              (1 << 31)

/* TC count & Error interrupt status register */
#define ZXDH_GDMA_SRC_LLI_ERR                   (1 << 16)
#define ZXDH_GDMA_SRC_DATA_ERR                  (1 << 17)
#define ZXDH_GDMA_DST_ADDR_ERR                  (1 << 18)
#define ZXDH_GDMA_ERR_STATUS                    (1 << 19)
#define ZXDH_GDMA_ERR_INTR_ENABLE               (1 << 20)
#define ZXDH_GDMA_TC_CNT_CLEAN                  (1)

#define ZXDH_GDMA_CHAN_SHIFT                    0x80
#define LOW32_MASK                              0xffffffff
#define LOW16_MASK                              0xffff

static int zxdh_gdma_queue_init(struct rte_rawdev *dev, uint16_t queue_id);
static int zxdh_gdma_queue_free(struct rte_rawdev *dev, uint16_t queue_id);

char zxdh_gdma_driver_name[] = "rawdev_zxdh_gdma";
char dev_name[] = "zxdh_gdma";

static inline struct zxdh_gdma_rawdev *
zxdh_gdma_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev->dev_private;
}

static inline struct zxdh_gdma_queue *
zxdh_gdma_get_queue(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct zxdh_gdma_rawdev *gdmadev = zxdh_gdma_rawdev_get_priv(dev);

	if (queue_id >= ZXDH_GDMA_TOTAL_CHAN_NUM) {
		ZXDH_PMD_LOG(ERR, "queue id %d is invalid", queue_id);
		return NULL;
	}

	return &(gdmadev->vqs[queue_id]);
}

static void
zxdh_gdma_write_reg(struct rte_rawdev *dev, uint16_t queue_id, uint32_t offset, uint32_t val)
{
	struct zxdh_gdma_rawdev *gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	uint32_t addr = 0;

	addr = offset + queue_id * ZXDH_GDMA_CHAN_SHIFT;
	*(uint32_t *)(gdmadev->base_addr + addr) = val;
}

static int
zxdh_gdma_rawdev_queue_setup(struct rte_rawdev *dev,
		uint16_t queue_id,
		rte_rawdev_obj_t queue_conf,
		size_t conf_size)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	struct zxdh_gdma_queue_config *qconfig = NULL;
	struct zxdh_gdma_rbp *rbp = NULL;
	uint16_t i = 0;
	uint8_t is_txq = 0;
	uint32_t src_user = 0;
	uint32_t dst_user = 0;

	if (dev == NULL)
		return -EINVAL;

	if ((queue_conf == NULL) || (conf_size != sizeof(struct zxdh_gdma_queue_config)))
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	qconfig = (struct zxdh_gdma_queue_config *)queue_conf;

	for (i = 0; i < ZXDH_GDMA_TOTAL_CHAN_NUM; i++) {
		if (gdmadev->vqs[i].enable == 0)
			break;
	}
	if (i >= ZXDH_GDMA_TOTAL_CHAN_NUM) {
		ZXDH_PMD_LOG(ERR, "Failed to setup queue, no avail queues");
		return -1;
	}
	queue_id = i;
	if (zxdh_gdma_queue_init(dev, queue_id) != 0) {
		ZXDH_PMD_LOG(ERR, "Failed to init queue");
		return -1;
	}
	queue = &(gdmadev->vqs[queue_id]);

	rbp = qconfig->rbp;
	if ((rbp->srbp != 0) && (rbp->drbp == 0)) {
		is_txq = 0;
		dst_user = ZXDH_GDMA_ZF_USER;
		src_user = ((rbp->spfid << ZXDH_GDMA_PF_NUM_SHIFT) |
				((rbp->sportid + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (rbp->svfid != 0)
			src_user |= (ZXDH_GDMA_VF_EN |
					((rbp->svfid - 1) << ZXDH_GDMA_VF_NUM_SHIFT));

		ZXDH_PMD_LOG(DEBUG, "rxq->qidx:%d setup src_user(ep:%d pf:%d vf:%d) success",
				queue_id, (uint8_t)rbp->sportid, (uint8_t)rbp->spfid,
				(uint8_t)rbp->svfid);
	} else if ((rbp->srbp == 0) && (rbp->drbp != 0)) {
		is_txq = 1;
		src_user = ZXDH_GDMA_ZF_USER;
		dst_user = ((rbp->dpfid << ZXDH_GDMA_PF_NUM_SHIFT) |
				((rbp->dportid + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (rbp->dvfid != 0)
			dst_user |= (ZXDH_GDMA_VF_EN |
					((rbp->dvfid - 1) << ZXDH_GDMA_VF_NUM_SHIFT));

		ZXDH_PMD_LOG(DEBUG, "txq->qidx:%d setup dst_user(ep:%d pf:%d vf:%d) success",
				queue_id, (uint8_t)rbp->dportid, (uint8_t)rbp->dpfid,
				(uint8_t)rbp->dvfid);
	} else {
		ZXDH_PMD_LOG(ERR, "Failed to setup queue, srbp/drbp is invalid");
		return -EINVAL;
	}
	queue->is_txq = is_txq;

	/* setup queue user info */
	queue->user = (src_user & LOW16_MASK) | (dst_user << 16);

	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_EXT_ADDR_OFFSET, queue->user);
	gdmadev->used_num++;

	return queue_id;
}

static const struct rte_rawdev_ops zxdh_gdma_rawdev_ops = {
	.queue_setup = zxdh_gdma_rawdev_queue_setup,
};

static int
zxdh_gdma_queue_init(struct rte_rawdev *dev, uint16_t queue_id)
{
	char name[RTE_MEMZONE_NAMESIZE];
	struct zxdh_gdma_queue *queue = NULL;
	const struct rte_memzone *mz = NULL;
	uint32_t size = 0;
	int ret = 0;

	queue = zxdh_gdma_get_queue(dev, queue_id);
	if (queue == NULL)
		return -EINVAL;

	queue->enable = 1;
	queue->vq_id  = queue_id;
	queue->flag   = 0;
	queue->tc_cnt = 0;

	/* Init sw_ring */
	queue->sw_ring.job = rte_calloc(NULL, queue->queue_size, sizeof(struct zxdh_gdma_job *), 0);
	if (queue->sw_ring.job == NULL) {
		ZXDH_PMD_LOG(ERR, "can not allocate sw_ring");
		ret = -ENOMEM;
		goto free_queue;
	}

	/* Cache up to size-1 job in the ring to prevent overwriting hardware prefetching */
	queue->sw_ring.free_cnt = queue->queue_size - 1;
	queue->sw_ring.deq_cnt  = 0;
	queue->sw_ring.pend_cnt = 0;
	queue->sw_ring.enq_idx  = 0;
	queue->sw_ring.deq_idx  = 0;
	queue->sw_ring.used_idx = 0;

	/* Init ring */
	snprintf(name, RTE_MEMZONE_NAMESIZE, "gdma_vq%d_ring", queue_id);
	size = ZXDH_GDMA_RING_SIZE * sizeof(struct zxdh_gdma_buff_desc);
	mz = rte_memzone_reserve_aligned(name, size, rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, size);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(name);
		if (mz == NULL) {
			ZXDH_PMD_LOG(ERR, "can not allocate ring %s", name);
			ret = -ENOMEM;
			goto free_queue;
		}
	}
	memset(mz->addr, 0, size);
	queue->ring.ring_mz   = mz;
	queue->ring.desc      = (struct zxdh_gdma_buff_desc *)(mz->addr);
	queue->ring.ring_mem  = mz->iova;
	queue->ring.avail_idx = 0;
	ZXDH_PMD_LOG(INFO, "queue%u ring phy addr:0x%"PRIx64" virt addr:%p",
			queue_id, mz->iova, mz->addr);

	/* Initialize the hardware channel */
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET,
			ZXDH_GDMA_CHAN_FORCE_CLOSE);
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_TC_CNT_OFFSET,
			ZXDH_GDMA_ERR_INTR_ENABLE | ZXDH_GDMA_ERR_STATUS | ZXDH_GDMA_TC_CNT_CLEAN);
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_LLI_USER_OFFSET,
			ZXDH_GDMA_ZF_USER);

	return 0;

free_queue:
	zxdh_gdma_queue_free(dev, queue_id);
	return ret;
}

static int
zxdh_gdma_queue_free(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint32_t val = 0;

	queue = zxdh_gdma_get_queue(dev, queue_id);
	if (queue == NULL)
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->used_num--;

	/* disable gdma channel */
	val = ZXDH_GDMA_CHAN_FORCE_CLOSE;
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET, val);

	queue->enable           = 0;
	queue->is_txq           = 0;
	queue->flag             = 0;
	queue->user             = 0;
	queue->tc_cnt           = 0;
	queue->ring.avail_idx   = 0;
	queue->sw_ring.free_cnt = 0;
	queue->sw_ring.deq_cnt  = 0;
	queue->sw_ring.pend_cnt = 0;
	queue->sw_ring.enq_idx  = 0;
	queue->sw_ring.deq_idx  = 0;
	queue->sw_ring.used_idx = 0;
	rte_free(queue->sw_ring.job);
	rte_memzone_free(queue->ring.ring_mz);

	return 0;
}

static int
zxdh_gdma_map_resource(struct rte_pci_device *dev)
{
	int fd = -1;
	char devname[PATH_MAX];
	void *mapaddr = NULL;
	struct rte_pci_addr *loc;

	loc = &dev->addr;
	snprintf(devname, sizeof(devname), "%s/" PCI_PRI_FMT "/resource0",
		rte_pci_get_sysfs_path(),
		loc->domain, loc->bus, loc->devid,
		loc->function);

		fd = open(devname, O_RDWR);
		if (fd < 0) {
			ZXDH_PMD_LOG(ERR, "Cannot open %s: %s", devname, strerror(errno));
			return -1;
		}

	/* Map the PCI memory resource of device */
	mapaddr = rte_mem_map(NULL, (size_t)dev->mem_resource[0].len,
			RTE_PROT_READ | RTE_PROT_WRITE,
			RTE_MAP_SHARED, fd, 0);
	if (mapaddr == NULL) {
		ZXDH_PMD_LOG(ERR, "cannot map resource(%d, 0x%zx): %s (%p)",
				fd, (size_t)dev->mem_resource[0].len,
				rte_strerror(rte_errno), mapaddr);
		close(fd);
		return -1;
	}

	close(fd);
	dev->mem_resource[0].addr = mapaddr;

	return 0;
}

static void
zxdh_gdma_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (rte_mem_unmap(requested_addr, size))
		ZXDH_PMD_LOG(ERR, "cannot mem unmap(%p, %#zx): %s",
				requested_addr, size, rte_strerror(rte_errno));
	else
		ZXDH_PMD_LOG(DEBUG, "PCI memory unmapped at %p", requested_addr);
}

static int
zxdh_gdma_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint8_t i = 0;
	int ret;

	if (pci_dev->mem_resource[0].phys_addr == 0) {
		ZXDH_PMD_LOG(ERR, "PCI bar0 resource is invalid");
		return -1;
	}

	ret = zxdh_gdma_map_resource(pci_dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Failed to mmap pci device(%s)", pci_dev->name);
		return -1;
	}
	ZXDH_PMD_LOG(INFO, "%s bar0 0x%"PRIx64" mapped at %p",
			pci_dev->name, pci_dev->mem_resource[0].phys_addr,
			pci_dev->mem_resource[0].addr);

	dev = rte_rawdev_pmd_allocate(dev_name, sizeof(struct zxdh_gdma_rawdev), rte_socket_id());
	if (dev == NULL) {
		ZXDH_PMD_LOG(ERR, "Unable to allocate gdma rawdev");
		goto err_out;
	}
	ZXDH_PMD_LOG(INFO, "Init %s on NUMA node %d, dev_id is %d",
			dev_name, rte_socket_id(), dev->dev_id);

	dev->dev_ops = &zxdh_gdma_rawdev_ops;
	dev->device = &pci_dev->device;
	dev->driver_name = zxdh_gdma_driver_name;
	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_STOPPED;
	gdmadev->rawdev = dev;
	gdmadev->queue_num = ZXDH_GDMA_TOTAL_CHAN_NUM;
	gdmadev->used_num = 0;
	gdmadev->base_addr = (uintptr_t)pci_dev->mem_resource[0].addr + ZXDH_GDMA_BASE_OFFSET;

	for (i = 0; i < ZXDH_GDMA_TOTAL_CHAN_NUM; i++) {
		queue = &(gdmadev->vqs[i]);
		queue->enable = 0;
		queue->queue_size = ZXDH_GDMA_QUEUE_SIZE;
		rte_spinlock_init(&(queue->enqueue_lock));
	}

	return 0;

err_out:
	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);
	return -1;
}

static int
zxdh_gdma_rawdev_remove(struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	int ret = 0;

	dev = rte_rawdev_pmd_get_named_dev(dev_name);
	if (dev == NULL)
		return -EINVAL;

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Device cleanup failed");
		return -1;
	}

	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);

	ZXDH_PMD_LOG(DEBUG, "rawdev %s remove done!", dev_name);

	return ret;
}

static const struct rte_pci_id zxdh_gdma_rawdev_map[] = {
	{ RTE_PCI_DEVICE(ZXDH_GDMA_VENDORID, ZXDH_GDMA_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver zxdh_gdma_rawdev_pmd = {
	.id_table = zxdh_gdma_rawdev_map,
	.drv_flags = 0,
	.probe = zxdh_gdma_rawdev_probe,
	.remove = zxdh_gdma_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_map);
RTE_LOG_REGISTER_DEFAULT(zxdh_gdma_rawdev_logtype, NOTICE);
