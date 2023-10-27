/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include <pthread.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nfp_common_pci.h>
#include <nfp_dev.h>
#include <rte_vfio.h>
#include <vdpa_driver.h>

#include "nfp_vdpa_core.h"
#include "nfp_vdpa_log.h"

#define NFP_VDPA_DRIVER_NAME nfp_vdpa

#define MSIX_IRQ_SET_BUF_LEN (sizeof(struct vfio_irq_set) + \
		sizeof(int) * (NFP_VDPA_MAX_QUEUES * 2 + 1))

struct nfp_vdpa_dev {
	struct rte_pci_device *pci_dev;
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_hw hw;

	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	int iommu_group;

	rte_thread_t tid;    /**< Thread for notify relay */
	int epoll_fd;

	int vid;
	uint16_t max_queues;
	RTE_ATOMIC(uint32_t) started;
	RTE_ATOMIC(uint32_t) dev_attached;
	RTE_ATOMIC(uint32_t) running;
	rte_spinlock_t lock;

	/** Eventfd for used ring interrupt */
	int intr_fd[NFP_VDPA_MAX_QUEUES * 2];
};

struct nfp_vdpa_dev_node {
	TAILQ_ENTRY(nfp_vdpa_dev_node) next;
	struct nfp_vdpa_dev *device;
};

TAILQ_HEAD(vdpa_dev_list_head, nfp_vdpa_dev_node);

static struct vdpa_dev_list_head vdpa_dev_list =
	TAILQ_HEAD_INITIALIZER(vdpa_dev_list);

static pthread_mutex_t vdpa_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct nfp_vdpa_dev_node *
nfp_vdpa_find_node_by_vdev(struct rte_vdpa_device *vdev)
{
	bool found = false;
	struct nfp_vdpa_dev_node *node;

	pthread_mutex_lock(&vdpa_list_lock);

	TAILQ_FOREACH(node, &vdpa_dev_list, next) {
		if (vdev == node->device->vdev) {
			found = true;
			break;
		}
	}

	pthread_mutex_unlock(&vdpa_list_lock);

	if (found)
		return node;

	return NULL;
}

static struct nfp_vdpa_dev_node *
nfp_vdpa_find_node_by_pdev(struct rte_pci_device *pdev)
{
	bool found = false;
	struct nfp_vdpa_dev_node *node;

	pthread_mutex_lock(&vdpa_list_lock);

	TAILQ_FOREACH(node, &vdpa_dev_list, next) {
		if (pdev == node->device->pci_dev) {
			found = true;
			break;
		}
	}

	pthread_mutex_unlock(&vdpa_list_lock);

	if (found)
		return node;

	return NULL;
}

static int
nfp_vdpa_vfio_setup(struct nfp_vdpa_dev *device)
{
	int ret;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = {0};
	struct rte_pci_device *pci_dev = device->pci_dev;

	rte_pci_unmap_device(pci_dev);

	rte_pci_device_name(&pci_dev->addr, dev_name, RTE_DEV_NAME_MAX_LEN);
	rte_vfio_get_group_num(rte_pci_get_sysfs_path(), dev_name,
			&device->iommu_group);

	device->vfio_container_fd = rte_vfio_container_create();
	if (device->vfio_container_fd < 0)
		return -1;

	device->vfio_group_fd = rte_vfio_container_group_bind(
			device->vfio_container_fd, device->iommu_group);
	if (device->vfio_group_fd < 0)
		goto container_destroy;

	DRV_VDPA_LOG(DEBUG, "container_fd=%d, group_fd=%d,\n",
			device->vfio_container_fd, device->vfio_group_fd);

	ret = rte_pci_map_device(pci_dev);
	if (ret != 0)
		goto group_unbind;

	device->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);

	return 0;

group_unbind:
	rte_vfio_container_group_unbind(device->vfio_container_fd, device->iommu_group);
container_destroy:
	rte_vfio_container_destroy(device->vfio_container_fd);

	return -1;
}

static void
nfp_vdpa_vfio_teardown(struct nfp_vdpa_dev *device)
{
	rte_pci_unmap_device(device->pci_dev);
	rte_vfio_container_group_unbind(device->vfio_container_fd, device->iommu_group);
	rte_vfio_container_destroy(device->vfio_container_fd);
}

static int
nfp_vdpa_dma_do_unmap(struct rte_vhost_memory *mem,
		uint32_t times,
		int vfio_container_fd)
{
	uint32_t i;
	int ret = 0;
	struct rte_vhost_mem_region *region;

	for (i = 0; i < times; i++) {
		region = &mem->regions[i];

		ret = rte_vfio_container_dma_unmap(vfio_container_fd,
				region->host_user_addr, region->guest_phys_addr,
				region->size);
		if (ret < 0) {
			/* Here should not return, even error happened. */
			DRV_VDPA_LOG(ERR, "DMA unmap failed. Times: %u", i);
		}
	}

	return ret;
}

static int
nfp_vdpa_dma_do_map(struct rte_vhost_memory *mem,
		uint32_t times,
		int vfio_container_fd)
{
	int ret;
	uint32_t i;
	struct rte_vhost_mem_region *region;

	for (i = 0; i < times; i++) {
		region = &mem->regions[i];

		ret = rte_vfio_container_dma_map(vfio_container_fd,
				region->host_user_addr, region->guest_phys_addr,
				region->size);
		if (ret < 0) {
			DRV_VDPA_LOG(ERR, "DMA map failed.");
			nfp_vdpa_dma_do_unmap(mem, i, vfio_container_fd);
			return ret;
		}
	}

	return 0;
}

static int
nfp_vdpa_dma_map(struct nfp_vdpa_dev *device,
		bool do_map)
{
	int ret;
	int vfio_container_fd;
	struct rte_vhost_memory *mem = NULL;

	ret = rte_vhost_get_mem_table(device->vid, &mem);
	if (ret < 0) {
		DRV_VDPA_LOG(ERR, "Failed to get memory layout.");
		return ret;
	}

	vfio_container_fd = device->vfio_container_fd;
	DRV_VDPA_LOG(DEBUG, "vfio_container_fd %d", vfio_container_fd);

	if (do_map)
		ret = nfp_vdpa_dma_do_map(mem, mem->nregions, vfio_container_fd);
	else
		ret = nfp_vdpa_dma_do_unmap(mem, mem->nregions, vfio_container_fd);

	free(mem);

	return ret;
}

static uint64_t
nfp_vdpa_qva_to_gpa(int vid,
		uint64_t qva)
{
	int ret;
	uint32_t i;
	uint64_t gpa = 0;
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *region;

	ret = rte_vhost_get_mem_table(vid, &mem);
	if (ret < 0) {
		DRV_VDPA_LOG(ERR, "Failed to get memory layout.");
		return gpa;
	}

	for (i = 0; i < mem->nregions; i++) {
		region = &mem->regions[i];

		if (qva >= region->host_user_addr &&
				qva < region->host_user_addr + region->size) {
			gpa = qva - region->host_user_addr + region->guest_phys_addr;
			break;
		}
	}

	free(mem);

	return gpa;
}

static int
nfp_vdpa_start(struct nfp_vdpa_dev *device)
{
	int ret;
	int vid;
	uint16_t i;
	uint64_t gpa;
	struct rte_vhost_vring vring;
	struct nfp_vdpa_hw *vdpa_hw = &device->hw;

	vid = device->vid;
	vdpa_hw->nr_vring = rte_vhost_get_vring_num(vid);

	ret = rte_vhost_get_negotiated_features(vid, &vdpa_hw->req_features);
	if (ret != 0)
		return ret;

	for (i = 0; i < vdpa_hw->nr_vring; i++) {
		ret = rte_vhost_get_vhost_vring(vid, i, &vring);
		if (ret != 0)
			return ret;

		gpa = nfp_vdpa_qva_to_gpa(vid, (uint64_t)(uintptr_t)vring.desc);
		if (gpa == 0) {
			DRV_VDPA_LOG(ERR, "Fail to get GPA for descriptor ring.");
			return -1;
		}

		vdpa_hw->vring[i].desc = gpa;

		gpa = nfp_vdpa_qva_to_gpa(vid, (uint64_t)(uintptr_t)vring.avail);
		if (gpa == 0) {
			DRV_VDPA_LOG(ERR, "Fail to get GPA for available ring.");
			return -1;
		}

		vdpa_hw->vring[i].avail = gpa;

		gpa = nfp_vdpa_qva_to_gpa(vid, (uint64_t)(uintptr_t)vring.used);
		if (gpa == 0) {
			DRV_VDPA_LOG(ERR, "Fail to get GPA for used ring.");
			return -1;
		}

		vdpa_hw->vring[i].used = gpa;

		vdpa_hw->vring[i].size = vring.size;

		ret = rte_vhost_get_vring_base(vid, i,
				&vdpa_hw->vring[i].last_avail_idx,
				&vdpa_hw->vring[i].last_used_idx);
		if (ret != 0)
			return ret;
	}

	return nfp_vdpa_hw_start(&device->hw, vid);
}

static void
nfp_vdpa_stop(struct nfp_vdpa_dev *device)
{
	int vid;
	uint32_t i;
	struct nfp_vdpa_hw *vdpa_hw = &device->hw;

	nfp_vdpa_hw_stop(vdpa_hw);

	vid = device->vid;
	for (i = 0; i < vdpa_hw->nr_vring; i++)
		rte_vhost_set_vring_base(vid, i,
				vdpa_hw->vring[i].last_avail_idx,
				vdpa_hw->vring[i].last_used_idx);
}

static int
nfp_vdpa_enable_vfio_intr(struct nfp_vdpa_dev *device)
{
	int ret;
	uint16_t i;
	int *fd_ptr;
	uint16_t nr_vring;
	struct vfio_irq_set *irq_set;
	struct rte_vhost_vring vring;
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];

	nr_vring = rte_vhost_get_vring_num(device->vid);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = nr_vring + 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	fd_ptr = (int *)&irq_set->data;
	fd_ptr[RTE_INTR_VEC_ZERO_OFFSET] = rte_intr_fd_get(device->pci_dev->intr_handle);

	for (i = 0; i < nr_vring; i++)
		device->intr_fd[i] = -1;

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(device->vid, i, &vring);
		fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = vring.callfd;
	}

	ret = ioctl(device->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret != 0) {
		DRV_VDPA_LOG(ERR, "Error enabling MSI-X interrupts.");
		return -EIO;
	}

	return 0;
}

static int
nfp_vdpa_disable_vfio_intr(struct nfp_vdpa_dev *device)
{
	int ret;
	struct vfio_irq_set *irq_set;
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(device->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret != 0) {
		DRV_VDPA_LOG(ERR, "Error disabling MSI-X interrupts.");
		return -EIO;
	}

	return 0;
}

static void
nfp_vdpa_read_kickfd(int kickfd)
{
	int bytes;
	uint64_t buf;

	for (;;) {
		bytes = read(kickfd, &buf, 8);
		if (bytes >= 0)
			break;

		if (errno != EINTR && errno != EWOULDBLOCK &&
				errno != EAGAIN) {
			DRV_VDPA_LOG(ERR, "Error reading kickfd");
			break;
		}
	}
}

static int
nfp_vdpa_notify_epoll_ctl(uint32_t queue_num,
		struct nfp_vdpa_dev *device)
{
	int ret;
	uint32_t qid;

	for (qid = 0; qid < queue_num; qid++) {
		struct epoll_event ev;
		struct rte_vhost_vring vring;

		ev.events = EPOLLIN | EPOLLPRI;
		rte_vhost_get_vhost_vring(device->vid, qid, &vring);
		ev.data.u64 = qid | (uint64_t)vring.kickfd << 32;
		ret = epoll_ctl(device->epoll_fd, EPOLL_CTL_ADD, vring.kickfd, &ev);
		if (ret < 0) {
			DRV_VDPA_LOG(ERR, "Epoll add error for queue %d", qid);
			return ret;
		}
	}

	return 0;
}

static int
nfp_vdpa_notify_epoll_wait(uint32_t queue_num,
		struct nfp_vdpa_dev *device)
{
	int i;
	int fds;
	int kickfd;
	uint32_t qid;
	struct epoll_event events[NFP_VDPA_MAX_QUEUES * 2];

	for (;;) {
		fds = epoll_wait(device->epoll_fd, events, queue_num, -1);
		if (fds < 0) {
			if (errno == EINTR)
				continue;

			DRV_VDPA_LOG(ERR, "Epoll wait fail");
			return -EACCES;
		}

		for (i = 0; i < fds; i++) {
			qid = events[i].data.u32;
			kickfd = (uint32_t)(events[i].data.u64 >> 32);

			nfp_vdpa_read_kickfd(kickfd);
			nfp_vdpa_notify_queue(&device->hw, qid);
		}
	}

	return 0;
}

static uint32_t
nfp_vdpa_notify_relay(void *arg)
{
	int ret;
	int epoll_fd;
	uint32_t queue_num;
	struct nfp_vdpa_dev *device = arg;

	epoll_fd = epoll_create(NFP_VDPA_MAX_QUEUES * 2);
	if (epoll_fd < 0) {
		DRV_VDPA_LOG(ERR, "failed to create epoll instance.");
		return 1;
	}

	device->epoll_fd = epoll_fd;

	queue_num = rte_vhost_get_vring_num(device->vid);

	ret = nfp_vdpa_notify_epoll_ctl(queue_num, device);
	if (ret != 0)
		goto notify_exit;

	ret = nfp_vdpa_notify_epoll_wait(queue_num, device);
	if (ret != 0)
		goto notify_exit;

	return 0;

notify_exit:
	close(device->epoll_fd);
	device->epoll_fd = -1;

	return 1;
}

static int
nfp_vdpa_setup_notify_relay(struct nfp_vdpa_dev *device)
{
	int ret;
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];

	snprintf(name, sizeof(name), "nfp-noti%d", device->vid);
	ret = rte_thread_create_internal_control(&device->tid, name,
			nfp_vdpa_notify_relay, (void *)device);
	if (ret != 0) {
		DRV_VDPA_LOG(ERR, "Failed to create notify relay pthread.");
		return -1;
	}

	return 0;
}

static void
nfp_vdpa_unset_notify_relay(struct nfp_vdpa_dev *device)
{
	if (device->tid.opaque_id != 0) {
		pthread_cancel((pthread_t)device->tid.opaque_id);
		rte_thread_join(device->tid, NULL);
		device->tid.opaque_id = 0;
	}

	if (device->epoll_fd >= 0) {
		close(device->epoll_fd);
		device->epoll_fd = -1;
	}
}

static int
update_datapath(struct nfp_vdpa_dev *device)
{
	int ret;

	rte_spinlock_lock(&device->lock);

	if ((rte_atomic_load_explicit(&device->running, rte_memory_order_relaxed) == 0) &&
			(rte_atomic_load_explicit(&device->started,
					rte_memory_order_relaxed) != 0) &&
			(rte_atomic_load_explicit(&device->dev_attached,
					rte_memory_order_relaxed) != 0)) {
		ret = nfp_vdpa_dma_map(device, true);
		if (ret != 0)
			goto unlock_exit;

		ret = nfp_vdpa_enable_vfio_intr(device);
		if (ret != 0)
			goto dma_map_rollback;

		ret = nfp_vdpa_start(device);
		if (ret != 0)
			goto disable_vfio_intr;

		ret = nfp_vdpa_setup_notify_relay(device);
		if (ret != 0)
			goto vdpa_stop;

		rte_atomic_store_explicit(&device->running, 1, rte_memory_order_relaxed);
	} else if ((rte_atomic_load_explicit(&device->running, rte_memory_order_relaxed) != 0) &&
			((rte_atomic_load_explicit(&device->started,
					rte_memory_order_relaxed) != 0) ||
			(rte_atomic_load_explicit(&device->dev_attached,
					rte_memory_order_relaxed) != 0))) {
		nfp_vdpa_unset_notify_relay(device);

		nfp_vdpa_stop(device);

		ret = nfp_vdpa_disable_vfio_intr(device);
		if (ret != 0)
			goto unlock_exit;

		ret = nfp_vdpa_dma_map(device, false);
		if (ret != 0)
			goto unlock_exit;

		rte_atomic_store_explicit(&device->running, 0, rte_memory_order_relaxed);
	}

	rte_spinlock_unlock(&device->lock);
	return 0;

vdpa_stop:
	nfp_vdpa_stop(device);
disable_vfio_intr:
	nfp_vdpa_disable_vfio_intr(device);
dma_map_rollback:
	nfp_vdpa_dma_map(device, false);
unlock_exit:
	rte_spinlock_unlock(&device->lock);
	return ret;
}

static int
nfp_vdpa_dev_config(int vid)
{
	int ret;
	struct nfp_vdpa_dev *device;
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_dev_node *node;

	vdev = rte_vhost_get_vdpa_device(vid);
	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	device = node->device;
	device->vid = vid;
	rte_atomic_store_explicit(&device->dev_attached, 1, rte_memory_order_relaxed);
	update_datapath(device);

	ret = rte_vhost_host_notifier_ctrl(vid, RTE_VHOST_QUEUE_ALL, true);
	if (ret != 0)
		DRV_VDPA_LOG(INFO, "vDPA (%s): software relay is used.",
				vdev->device->name);

	return 0;
}

static int
nfp_vdpa_dev_close(int vid)
{
	struct nfp_vdpa_dev *device;
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_dev_node *node;

	vdev = rte_vhost_get_vdpa_device(vid);
	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	device = node->device;
	rte_atomic_store_explicit(&device->dev_attached, 0, rte_memory_order_relaxed);
	update_datapath(device);

	return 0;
}

static int
nfp_vdpa_get_vfio_group_fd(int vid)
{
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_dev_node *node;

	vdev = rte_vhost_get_vdpa_device(vid);
	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	return node->device->vfio_group_fd;
}

static int
nfp_vdpa_get_vfio_device_fd(int vid)
{
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_dev_node *node;

	vdev = rte_vhost_get_vdpa_device(vid);
	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	return node->device->vfio_dev_fd;
}

static int
nfp_vdpa_get_notify_area(int vid,
		int qid,
		uint64_t *offset,
		uint64_t *size)
{
	int ret;
	struct nfp_vdpa_dev *device;
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_dev_node *node;
	struct vfio_region_info region = {
		.argsz = sizeof(region)
	};

	vdev = rte_vhost_get_vdpa_device(vid);
	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR,  "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	device = node->device;
	region.index = device->hw.notify_region;

	ret = ioctl(device->vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &region);
	if (ret != 0) {
		DRV_VDPA_LOG(ERR, "Get not get device region info.");
		return -EIO;
	}

	*offset = nfp_vdpa_get_queue_notify_offset(&device->hw, qid) + region.offset;
	*size = NFP_VDPA_NOTIFY_ADDR_INTERVAL;

	return 0;
}

static int
nfp_vdpa_get_queue_num(struct rte_vdpa_device *vdev,
		uint32_t *queue_num)
{
	struct nfp_vdpa_dev_node *node;

	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	*queue_num = node->device->max_queues;

	return 0;
}

static int
nfp_vdpa_get_vdpa_features(struct rte_vdpa_device *vdev,
		uint64_t *features)
{
	struct nfp_vdpa_dev_node *node;

	node = nfp_vdpa_find_node_by_vdev(vdev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR,  "Invalid vDPA device: %p", vdev);
		return -ENODEV;
	}

	*features = node->device->hw.features;

	return 0;
}

static int
nfp_vdpa_get_protocol_features(struct rte_vdpa_device *vdev __rte_unused,
		uint64_t *features)
{
	*features = 1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD |
			1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK |
			1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ |
			1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD |
			1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER;

	return 0;
}

static int
nfp_vdpa_set_features(int32_t vid)
{
	DRV_VDPA_LOG(DEBUG, "Start vid=%d", vid);
	return 0;
}

static int
nfp_vdpa_set_vring_state(int vid,
		int vring,
		int state)
{
	DRV_VDPA_LOG(DEBUG, "Start vid=%d, vring=%d, state=%d", vid, vring, state);
	return 0;
}

struct rte_vdpa_dev_ops nfp_vdpa_ops = {
	.get_queue_num = nfp_vdpa_get_queue_num,
	.get_features = nfp_vdpa_get_vdpa_features,
	.get_protocol_features = nfp_vdpa_get_protocol_features,
	.dev_conf = nfp_vdpa_dev_config,
	.dev_close = nfp_vdpa_dev_close,
	.set_vring_state = nfp_vdpa_set_vring_state,
	.set_features = nfp_vdpa_set_features,
	.get_vfio_group_fd = nfp_vdpa_get_vfio_group_fd,
	.get_vfio_device_fd = nfp_vdpa_get_vfio_device_fd,
	.get_notify_area = nfp_vdpa_get_notify_area,
};

static int
nfp_vdpa_pci_probe(struct rte_pci_device *pci_dev)
{
	int ret;
	struct nfp_vdpa_dev *device;
	struct nfp_vdpa_dev_node *node;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	node = calloc(1, sizeof(*node));
	if (node == NULL)
		return -ENOMEM;

	device = calloc(1, sizeof(*device));
	if (device == NULL)
		goto free_node;

	device->pci_dev = pci_dev;

	ret = nfp_vdpa_vfio_setup(device);
	if (ret != 0)
		goto free_device;

	ret = nfp_vdpa_hw_init(&device->hw, pci_dev);
	if (ret != 0)
		goto vfio_teardown;

	device->max_queues = NFP_VDPA_MAX_QUEUES;

	device->vdev = rte_vdpa_register_device(&pci_dev->device, &nfp_vdpa_ops);
	if (device->vdev == NULL) {
		DRV_VDPA_LOG(ERR, "Failed to register device %s", pci_dev->name);
		goto vfio_teardown;
	}

	node->device = device;
	pthread_mutex_lock(&vdpa_list_lock);
	TAILQ_INSERT_TAIL(&vdpa_dev_list, node, next);
	pthread_mutex_unlock(&vdpa_list_lock);

	rte_spinlock_init(&device->lock);
	rte_atomic_store_explicit(&device->started, 1, rte_memory_order_relaxed);
	update_datapath(device);

	return 0;

vfio_teardown:
	nfp_vdpa_vfio_teardown(device);
free_device:
	free(device);
free_node:
	free(node);

	return -1;
}

static int
nfp_vdpa_pci_remove(struct rte_pci_device *pci_dev)
{
	struct nfp_vdpa_dev *device;
	struct nfp_vdpa_dev_node *node;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	node = nfp_vdpa_find_node_by_pdev(pci_dev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid device: %s", pci_dev->name);
		return -ENODEV;
	}

	device = node->device;

	rte_atomic_store_explicit(&device->started, 0, rte_memory_order_relaxed);
	update_datapath(device);

	pthread_mutex_lock(&vdpa_list_lock);
	TAILQ_REMOVE(&vdpa_dev_list, node, next);
	pthread_mutex_unlock(&vdpa_list_lock);

	rte_vdpa_unregister_device(device->vdev);
	nfp_vdpa_vfio_teardown(device);

	free(device);
	free(node);

	return 0;
}

static const struct rte_pci_id pci_id_nfp_vdpa_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
				PCI_DEVICE_ID_NFP6000_VF_NIC)
	},
	{
		.vendor_id = 0,
	},
};

static struct nfp_class_driver nfp_vdpa = {
	.drv_class = NFP_CLASS_VDPA,
	.name = RTE_STR(NFP_VDPA_DRIVER_NAME),
	.id_table = pci_id_nfp_vdpa_map,
	.drv_flags =  RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = nfp_vdpa_pci_probe,
	.remove = nfp_vdpa_pci_remove,
};

RTE_INIT(nfp_vdpa_init)
{
	nfp_class_driver_register(&nfp_vdpa);
}

RTE_PMD_REGISTER_PCI_TABLE(NFP_VDPA_DRIVER_NAME, pci_id_nfp_vdpa_map);
RTE_PMD_REGISTER_KMOD_DEP(NFP_VDPA_DRIVER_NAME, "* vfio-pci");
