/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <rte_malloc.h>
#include <rte_vfio.h>

#include "virtio_pci.h"
#include "virtio_api.h"
#include "virtqueue.h"
#include "virtio_logs.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))
#endif

struct virtio_pci_dev *
virtio_pci_dev_alloc(struct rte_pci_device *pci_dev)
{
	struct virtio_pci_dev *vpdev;
	struct virtio_hw *hw;
	int ret;

	vpdev = rte_zmalloc("virtio pci device", sizeof(*vpdev), RTE_CACHE_LINE_SIZE);
	if (vpdev == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vpdev memory");
		return NULL;
	}

	hw = &vpdev->hw;

	VTPCI_DEV(hw) = pci_dev;
	ret = virtio_pci_dev_init(pci_dev, vpdev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init virtio PCI device %s", pci_dev->device.name);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	vpdev->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);
	if (vpdev->vfio_dev_fd < 0) {
		PMD_INIT_LOG(ERR, "Failed to get vfio dev fd");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	/* Reset the device, so when device is used later, reset time is saved. */
	virtio_pci_dev_reset(vpdev);

	/* Tell the device that driver has noticed it. */
	virtio_pci_dev_set_status(vpdev, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the device that driver known how to drive it. */
	virtio_pci_dev_set_status(vpdev, VIRTIO_CONFIG_STATUS_DRIVER);

	hw->guest_features = VIRTIO_OPS(hw)->get_features(hw);
	PMD_INIT_LOG(DEBUG, "Guest_features is 0x%"PRIx64, hw->guest_features);
	return vpdev;

error:
	rte_free(vpdev);
	return NULL;
}

void
virtio_pci_dev_free(struct virtio_pci_dev *vpdev)
{
	struct virtio_hw *hw;

	hw = &vpdev->hw;
	if (VIRTIO_OPS(hw)->dev_close(hw))
		PMD_INIT_LOG(ERR, "Failed to close virtio device %s", VP_DEV_NAME(vpdev));
	rte_free(vpdev);
}

uint16_t
virtio_pci_dev_nr_vq_get(struct virtio_pci_dev *vpdev)
{
	struct virtio_hw *hw = &vpdev->hw;

	return VIRTIO_OPS(hw)->get_queue_num(hw);
}

void
virtio_pci_dev_queues_free(struct virtio_pci_dev *vpdev, uint16_t nr_vq)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct virtqueue *vq;
	uint16_t i;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;
		rte_free(vq);
		hw->vqs[i] = NULL;
	}

	rte_free(hw->vqs);
	hw->vqs = NULL;
}

int
virtio_pci_dev_queues_alloc(struct virtio_pci_dev *vpdev, uint16_t nr_vq)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct virtqueue *vq;
	uint16_t i;

	hw->vqs = rte_zmalloc(NULL, sizeof(vq) * nr_vq, 0);
	if (!hw->vqs) {
		PMD_INIT_LOG(ERR, "Failed to alloc virtio device(%s) queues", VP_DEV_NAME(vpdev));
		return -ENOMEM;
	}

	for (i = 0; i < nr_vq; i++) {
		vq = rte_zmalloc_socket(NULL, sizeof(*vq), RTE_CACHE_LINE_SIZE,
					VTPCI_DEV(hw)->device.numa_node);
		if (vq == NULL) {
			PMD_INIT_LOG(ERR, "Failed to alloc virtio queue");
			virtio_pci_dev_queues_free(vpdev, nr_vq);
			return -ENOMEM;
		}
		vq->hw = hw;
		vq->vq_queue_index = i;
		hw->vqs[i] = vq;
	}

	return 0;
}

uint16_t
virtio_pci_dev_queue_size_get(struct virtio_pci_dev *vpdev, uint16_t idx)
{
	struct virtio_hw *hw = &vpdev->hw;

	return VIRTIO_OPS(hw)->get_queue_size(hw, idx);
}

void
virtio_pci_dev_features_get(struct virtio_pci_dev *vpdev, uint64_t *features)
{
	struct virtio_hw *hw;

	hw = &vpdev->hw;
	*features = hw->guest_features;
}

uint64_t
virtio_pci_dev_features_set(struct virtio_pci_dev *vpdev, uint64_t features)
{
	struct virtio_hw *hw;

	hw = &vpdev->hw;
	return virtio_pci_dev_negotiate_features(hw, features);
}

int
virtio_pci_dev_notify_area_get(struct virtio_pci_dev *vpdev,
										   uint16_t qid, uint64_t *offset, uint64_t *size)
{
	struct virtio_hw *hw;
	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	int ret;

	reg.index = vpdev->notify_bar;
	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s can not get device region info: %s",
							VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	hw = &vpdev->hw;

	*offset = (uint8_t *)hw->vqs[qid]->notify_addr -
			(uint8_t *)VTPCI_DEV(hw)->mem_resource[vpdev->notify_bar].addr +
			reg.offset;
	//nofiy area should page size aligned
	*size = PAGE_SIZE;
	return 0;

}

int
virtio_pci_dev_queue_set(struct virtio_pci_dev *vpdev,
								   uint16_t qid, const struct virtio_pci_dev_vring_info *vring_info)
{
	struct virtio_hw *hw;
	struct virtqueue *hw_vq;
	unsigned int size;

	hw = &vpdev->hw;
	hw_vq = hw->vqs[qid];
	hw_vq->vq_ring_mem  = vring_info->desc;
	hw_vq->vq_avail_mem = vring_info->avail;
	hw_vq->vq_used_mem  = vring_info->used;
	hw_vq->vq_nentries  = vring_info->size;
	size = vring_size(hw, vring_info->size, VIRTIO_VRING_ALIGN);
	hw_vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_VRING_ALIGN);

	if (VIRTIO_OPS(hw)->setup_queue(hw, hw_vq) < 0) {
		PMD_INIT_LOG(ERR, "Setup_queue failed");
		return -EINVAL;
	}

	return 0;
}

void
virtio_pci_dev_queue_del(struct virtio_pci_dev *vpdev, uint16_t qid)
{
	struct virtio_hw *hw;
	struct virtqueue *hw_vq;

	hw = &vpdev->hw;
	hw_vq = hw->vqs[qid];

	VIRTIO_OPS(hw)->del_queue(hw, hw_vq);
}

int
virtio_pci_dev_interrupts_num_get(struct virtio_pci_dev *vpdev)
{
	struct vfio_irq_info irq_info;
	int ret;

	irq_info.argsz = sizeof(struct vfio_irq_info);
	irq_info.index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_info.count = 0;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq_info);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s get MSI-X num fail: %s",
						VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	return irq_info.count;
}

#define VFIO_FD_INVALID -1
int
virtio_pci_dev_interrupts_alloc(struct virtio_pci_dev *vpdev, int nvec)
{
	struct vfio_irq_set *irq_set;
	int ret, i;

	irq_set = rte_zmalloc(NULL, (sizeof(struct vfio_irq_set) + sizeof(int)*nvec), 0);
	if (irq_set == NULL) {
		PMD_INIT_LOG(ERR, "Dev %s malloc fail", VP_DEV_NAME(vpdev));
		return -ENOMEM;
	}
	irq_set->argsz = sizeof(struct vfio_irq_set) + sizeof(int)*nvec;
	irq_set->count = nvec;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	for (i = 0; i < nvec; i++)
		*((int *)&irq_set->data + i) = VFIO_FD_INVALID;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	rte_free(irq_set);
	if (ret)
		PMD_INIT_LOG(ERR, "Dev %s enabling MSI-X: %s", VP_DEV_NAME(vpdev), strerror(errno));

	return ret;
}

int
virtio_pci_dev_interrupts_free(struct virtio_pci_dev *vpdev)
{
	struct vfio_irq_set irq_set;
	int ret;

	irq_set.argsz = sizeof(struct vfio_irq_set);
	irq_set.count = 0;
	irq_set.flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set.index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set.start = 0;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, &irq_set);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s free MSI-X interrupts: %s", VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	return 0;
}

int
virtio_pci_dev_interrupt_enable(struct virtio_pci_dev *vpdev, int fd, int vec)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct vfio_irq_set *irq_set;
	int ret;

	irq_set = rte_zmalloc(NULL, (sizeof(struct vfio_irq_set) + sizeof(int)), 0);
	if (irq_set == NULL) {
		PMD_INIT_LOG(ERR, "Dev %s malloc fail", VP_DEV_NAME(vpdev));
		return -ENOMEM;
	}
	irq_set->argsz = sizeof(struct vfio_irq_set) + sizeof(int);
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = vec;
	*(int *)&irq_set->data = fd;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	rte_free(irq_set);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s enabling MSI-X: %s", VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	VIRTIO_OPS(hw)->intr_detect(hw);
	if (vpdev->msix_status != VIRTIO_MSIX_ENABLED) {
		PMD_INIT_LOG(ERR, "Dev %s MSI-X not enabled,status: %d",
							VP_DEV_NAME(vpdev), vpdev->msix_status);
		return -EINVAL;
	}

	if (vec == 0) {
		if (VIRTIO_OPS(hw)->set_config_irq(hw, 0) ==
				VIRTIO_MSI_NO_VECTOR) {
			PMD_INIT_LOG(ERR, "Failed to set config vector");
			return -EINVAL;
		}
	} else if (VIRTIO_OPS(hw)->set_queue_irq(hw, hw->vqs[vec-1], vec) ==
					 VIRTIO_MSI_NO_VECTOR) {
		PMD_INIT_LOG(ERR, "Failed to set queue %d vector", vec-1);
		return -EINVAL;
	}
	return 0;
}

int
virtio_pci_dev_interrupt_disable(struct virtio_pci_dev *vpdev, int vec)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct vfio_irq_set *irq_set;
	int ret;

	irq_set = rte_zmalloc(NULL, (sizeof(struct vfio_irq_set) + sizeof(int)), 0);
	if (irq_set == NULL) {
		PMD_INIT_LOG(ERR, "Dev %s malloc fail", VP_DEV_NAME(vpdev));
		return -ENOMEM;
	}
	irq_set->argsz = sizeof(struct vfio_irq_set) + sizeof(int);
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = vec;
	*(int *)&irq_set->data = VFIO_FD_INVALID;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	rte_free(irq_set);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s disabling MSI-X: %s", VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	if (vec == 0) {
		if (VIRTIO_OPS(hw)->set_config_irq(hw, VIRTIO_MSI_NO_VECTOR) !=
				VIRTIO_MSI_NO_VECTOR) {
			PMD_INIT_LOG(ERR, "Failed to unset config vector");
			return -EINVAL;
		}
	} else if (VIRTIO_OPS(hw)->set_queue_irq(hw, hw->vqs[vec-1], VIRTIO_MSI_NO_VECTOR) !=
					 VIRTIO_MSI_NO_VECTOR) {
		PMD_INIT_LOG(ERR, "Failed to unset queue %d vector", vec-1);
		return -EINVAL;
	}

	return 0;
}

void
virtio_pci_dev_set_status(struct virtio_pci_dev *vpdev, uint8_t status)
{
	struct virtio_hw *hw = &vpdev->hw;

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= VIRTIO_OPS(hw)->get_status(hw);

	VIRTIO_OPS(hw)->set_status(hw, status);
}

uint8_t
virtio_pci_dev_get_status(struct virtio_pci_dev *vpdev)
{
	struct virtio_hw *hw = &vpdev->hw;

	return VIRTIO_OPS(hw)->get_status(hw);
}

void
virtio_pci_dev_reset(struct virtio_pci_dev *vpdev)
{
	uint32_t retry = 0;
	struct virtio_hw *hw = &vpdev->hw;

	VIRTIO_OPS(hw)->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* Flush status write and wait device ready max 120 seconds. */
	while (VIRTIO_OPS(hw)->get_status(hw) != VIRTIO_CONFIG_STATUS_RESET) {
		if (retry++ > 120000) {
			PMD_INIT_LOG(WARNING, "vpdev %s  reset timeout", VP_DEV_NAME(vpdev));
			break;
		}
		if (!(retry % 1000))
			PMD_INIT_LOG(INFO, "vpdev %s  resetting", VP_DEV_NAME(vpdev));
		usleep(1000L);
	}
}

uint64_t
virtio_pci_dev_negotiate_features(struct virtio_hw *hw, uint64_t host_features)
{
	uint64_t features;

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & hw->guest_features;
	VIRTIO_OPS(hw)->set_features(hw, features);

	return features;
}

void
virtio_pci_dev_config_read(struct virtio_pci_dev *vpdev, size_t offset,
		      void *dst, int length)
{
	struct virtio_hw *hw = &vpdev->hw;

	VIRTIO_OPS(hw)->read_dev_cfg(hw, offset, dst, length);
}

void
virtio_pci_dev_config_write(struct virtio_pci_dev *vpdev, size_t offset,
		       const void *src, int length)
{
	struct virtio_hw *hw = &vpdev->hw;

	VIRTIO_OPS(hw)->write_dev_cfg(hw, offset, src, length);
}

uint8_t
virtio_pci_dev_isr_get(struct virtio_hw *hw)
{
	return VIRTIO_OPS(hw)->get_isr(hw);
}

RTE_LOG_REGISTER(virtio_pci_dev_logtype, pmd.virtio.dev, NOTICE);
