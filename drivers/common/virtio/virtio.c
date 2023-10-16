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
#include "virtio_pci_state.h"

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
	virtio_pci_dev_reset(vpdev, VIRTIO_VDPA_PROBE_RESET_TIME_OUT);

	/* Tell the device that driver has noticed it. */
	virtio_pci_dev_set_status(vpdev, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the device that driver known how to drive it. */
	virtio_pci_dev_set_status(vpdev, VIRTIO_CONFIG_STATUS_DRIVER);

	virtio_pci_dev_state_num_queue_set(vpdev);

	hw->device_features = VIRTIO_OPS(hw)->get_features(hw);
	PMD_INIT_LOG(DEBUG, "device_features is 0x%"PRIx64, hw->device_features);
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

	if (hw == NULL || hw->vqs == NULL)
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
	*features = VIRTIO_OPS(hw)->get_features(hw);
}

uint64_t
virtio_pci_dev_features_set(struct virtio_pci_dev *vpdev, uint64_t features)
{
	struct virtio_hw *hw;
	uint64_t hw_features;

	hw = &vpdev->hw;
	hw_features = VIRTIO_OPS(hw)->get_features(hw);
	features &= hw_features;
	VIRTIO_OPS(hw)->set_features(hw, features);
	hw->guest_features = features;
	return features;
}

void
virtio_pci_dev_state_features_get(struct virtio_pci_dev *vpdev, uint64_t *features)
{
	struct virtio_hw *hw;

	hw = &vpdev->hw;
	*features = hw->device_features;
}

uint64_t
virtio_pci_dev_state_features_set(struct virtio_pci_dev *vpdev, uint64_t features, void *state)
{
	struct virtio_hw *hw;
	uint64_t hw_features;
	struct virtio_dev_common_state *state_info = state;

	hw = &vpdev->hw;
	hw_features = hw->device_features;
	features &= hw_features;

	state_info->common_cfg.driver_feature = rte_cpu_to_le_64(features);

	hw->guest_features = features;
	return features;
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
void
virtio_pci_dev_state_all_queues_disable(struct virtio_pci_dev *vpdev, void *state)
{
	struct virtio_hw *hw;
	struct virtio_dev_queue_info *q_info;
	uint16_t qid;

	hw = &vpdev->hw;
	q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);

	for(qid = 0; qid < hw->num_queues; qid++) {
		q_info[qid].q_cfg.queue_enable = 0;
	}
}

void
virtio_pci_dev_state_dev_status_set(void *state, uint8_t dev_status)
{
	struct virtio_dev_common_state *state_info = state;

	state_info->common_cfg.device_status = dev_status;
}
void
virtio_pci_dev_state_dump(struct virtio_pci_dev *vpdev, void *state, uint32_t state_size)
{
	struct virtio_dev_state_hdr *hdr;
	struct virtio_hw *hw;
	struct virtio_field_hdr *f_hdr;
	struct virtio_dev_split_q_run_state *tmp_hw_idx;
	struct virtio_pci_state_common_cfg *common_cfg;
	struct virtio_dev_q_cfg *tmp_q_cfg;
	uint32_t field_cnt, *tmp, *tmp_start;

	hw = &vpdev->hw;

	hdr = state;
	field_cnt = rte_le_to_cpu_32(hdr->virtio_field_count);

	PMD_DUMP_LOG(INFO, "--------------state dump start--------------\r\n");
	PMD_DUMP_LOG(INFO, "state field cnt:%d size:%d\r\n", field_cnt, state_size);
	f_hdr = (struct virtio_field_hdr *)(hdr + 1);
	while(field_cnt) {
		switch (rte_le_to_cpu_32(f_hdr->type)) {
			case VIRTIO_DEV_PCI_COMMON_CFG:
				common_cfg = (struct virtio_pci_state_common_cfg *)(f_hdr + 1);
				PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_PCI_COMMON_CFG, size:%d bytes \n", f_hdr->size);
				if (f_hdr->size < sizeof(struct virtio_pci_state_common_cfg)) {
					PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_PCI_COMMON_CFG is truncated\n");
					break;
				}
				PMD_DUMP_LOG(INFO, ">>> dev_ftr_sel: %d dev_ftrs: 0x%lx drv_ftr_sel: %d drv_ftrs: 0x%lx msi_x: 0x%0x num_queues: %d queue_select: %d status: 0x%0x config_gen: %d\n",
									rte_le_to_cpu_32(common_cfg->device_feature_select),
									rte_le_to_cpu_64(common_cfg->device_feature),
									rte_le_to_cpu_32(common_cfg->driver_feature_select),
									rte_le_to_cpu_64(common_cfg->driver_feature),
									rte_le_to_cpu_16(common_cfg->msix_config),
									rte_le_to_cpu_16(common_cfg->num_queues),
									rte_le_to_cpu_16(common_cfg->queue_select),
									common_cfg->device_status,
									common_cfg->config_generation);
				break;
			case VIRTIO_DEV_SPLIT_Q_RUN_STATE:
				tmp_hw_idx = (struct virtio_dev_split_q_run_state *)(f_hdr + 1);
				PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_SPLIT_Q_RUN_STATE, size:%d bytes \n", f_hdr->size);
				if (f_hdr->size < sizeof(struct virtio_dev_split_q_run_state)) {
					PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_SPLIT_Q_RUN_STATE is truncated\n");
					break;
				}
				PMD_DUMP_LOG(INFO, ">>> qid:%d hw_avail_idx: %d hw_used_idx: %d\r\n",
									rte_le_to_cpu_16(tmp_hw_idx->queue_index),
									rte_le_to_cpu_16(tmp_hw_idx->last_avail_idx),
									rte_le_to_cpu_16(tmp_hw_idx->last_used_idx));
				break;
			case VIRTIO_DEV_QUEUE_CFG:
				tmp_q_cfg = (struct virtio_dev_q_cfg *)(f_hdr + 1);
				PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_QUEUE_CFG, size:%d bytes \n", f_hdr->size);
				if (f_hdr->size < sizeof(struct virtio_dev_q_cfg)) {
					PMD_DUMP_LOG(INFO, ">> VIRTIO_DEV_QUEUE_CFG is truncated\n");
					break;
				}
				PMD_DUMP_LOG(INFO, ">>> qid:%d size: %d msix: %d enable: %d notify_offset: %d desc 0x%lx driver 0x%lx device 0x%lx notify data: %d reset: %d\r\n",
									rte_le_to_cpu_16(tmp_q_cfg->queue_index),
									rte_le_to_cpu_16(tmp_q_cfg->queue_size),
									rte_le_to_cpu_16(tmp_q_cfg->queue_msix_vector),
									rte_le_to_cpu_16(tmp_q_cfg->queue_enable),
									rte_le_to_cpu_16(tmp_q_cfg->queue_notify_off),
									rte_le_to_cpu_64(tmp_q_cfg->queue_desc),
									rte_le_to_cpu_64(tmp_q_cfg->queue_driver),
									rte_le_to_cpu_64(tmp_q_cfg->queue_device),
									rte_le_to_cpu_16(tmp_q_cfg->queue_notify_data),
									rte_le_to_cpu_16(tmp_q_cfg->queue_reset));
				break;
			case VIRTIO_DEV_CFG_SPACE:
				hw->virtio_dev_sp_ops->dev_cfg_dump(f_hdr);
				break;
			default:
				PMD_DUMP_LOG(INFO, ">> %d type, size:%d bytes \n", rte_le_to_cpu_32(f_hdr->type), f_hdr->size);
				tmp_start = (uint32_t *)(f_hdr + 1);
				tmp = tmp_start;
				if (((uint8_t *)tmp_start + f_hdr->size) > ((uint8_t *)state + state_size)) {
					PMD_DUMP_LOG(ERR, "Pointer f_hdr exceed!\r\n");
					return;
				}
				PMD_DUMP_LOG(INFO, ">>>");
				while ((tmp - tmp_start) < f_hdr->size) {
					PMD_DUMP_LOG(INFO, " %x", *tmp);
					tmp++;
				}
				PMD_DUMP_LOG(INFO, "\n");
				break;
		}
		f_hdr = (struct virtio_field_hdr *)((uint8_t *)(f_hdr + 1) + rte_le_to_cpu_32(f_hdr->size));
		field_cnt--;

		if ((uint8_t *)f_hdr > ((uint8_t *)state + state_size)) {
			PMD_DUMP_LOG(ERR, "Pointer exceed!\r\n");
			return;
		}
	}

	PMD_DUMP_LOG(INFO, "--------------state dump end--------------\r\n");

}
int
virtio_pci_dev_state_hw_idx_get(void *state, uint32_t state_size,
											struct virtio_dev_run_state_info *hw_idx_info,
											int num_queues)
{
	struct virtio_dev_state_hdr *hdr;
	struct virtio_field_hdr *f_hdr;
	struct virtio_dev_split_q_run_state *tmp_hw_idx;
	uint16_t qid;
	uint32_t field_cnt;

	hdr = state;
	field_cnt = rte_le_to_cpu_32(hdr->virtio_field_count);

	f_hdr = (struct virtio_field_hdr *)(hdr + 1);
	while(field_cnt) {
		if (rte_le_to_cpu_32(f_hdr->type) == VIRTIO_DEV_SPLIT_Q_RUN_STATE) {
			if (f_hdr->size < sizeof(struct virtio_dev_split_q_run_state)) {
				PMD_INIT_LOG(ERR, "State is truncated, size: %d \n", f_hdr->size);
				return -EINVAL;
			}

			tmp_hw_idx = (struct virtio_dev_split_q_run_state *)(f_hdr + 1);
			qid = rte_le_to_cpu_16(tmp_hw_idx->queue_index);
			if (qid < num_queues) {
				hw_idx_info[qid].flag = true;
				hw_idx_info[qid].last_avail_idx = rte_le_to_cpu_16(tmp_hw_idx->last_avail_idx);
				hw_idx_info[qid].last_used_idx = rte_le_to_cpu_16(tmp_hw_idx->last_used_idx);
			}
		}
		f_hdr = (struct virtio_field_hdr *)((uint8_t *)(f_hdr + 1) + rte_le_to_cpu_32(f_hdr->size));
		if (((uint8_t *)f_hdr - (uint8_t *)state) > state_size) {
			PMD_INIT_LOG(ERR, "TLV exceed state size, tlv:%p state:%p size:%d\n", f_hdr, state, state_size);
			return -EINVAL;
		}
		field_cnt--;
	}

	return 0;
}

int
virtio_pci_dev_state_hw_idx_set(struct virtio_pci_dev *vpdev, uint16_t qid, uint16_t last_avail_idx,
											uint16_t last_used_idx, void *state)
{
	struct virtio_hw *hw;
	struct virtio_dev_queue_info *q_info;

	hw = &vpdev->hw;

	q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);
	q_info[qid].q_run_state.last_avail_idx = rte_cpu_to_le_16(last_avail_idx);
	q_info[qid].q_run_state.last_used_idx = rte_cpu_to_le_16(last_used_idx);

	return 0;
}

int
virtio_pci_dev_state_queue_set(struct virtio_pci_dev *vpdev,
								   uint16_t qid, const struct virtio_pci_dev_vring_info *vring_info, void *state)
{
	struct virtio_hw *hw;
	struct virtqueue *hw_vq;
	struct virtio_dev_queue_info *q_info;
	unsigned int size;

	hw = &vpdev->hw;
	hw_vq = hw->vqs[qid];
	hw_vq->vq_ring_mem  = vring_info->desc;
	hw_vq->vq_avail_mem = vring_info->avail;
	hw_vq->vq_used_mem  = vring_info->used;
	hw_vq->vq_nentries  = vring_info->size;
	size = vring_size(hw, vring_info->size, VIRTIO_VRING_ALIGN);
	hw_vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_VRING_ALIGN);

	q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);
	q_info[qid].q_cfg.queue_size = rte_cpu_to_le_16(vring_info->size);
	q_info[qid].q_cfg.queue_enable = rte_cpu_to_le_16(1);
	q_info[qid].q_cfg.queue_desc = rte_cpu_to_le_64(vring_info->desc);
	q_info[qid].q_cfg.queue_driver = rte_cpu_to_le_64(vring_info->avail);
	q_info[qid].q_cfg.queue_device = rte_cpu_to_le_64(vring_info->used);

	return 0;
}

void
virtio_pci_dev_state_queue_del(struct virtio_pci_dev *vpdev, uint16_t qid, void *state)
{
	struct virtio_hw *hw;
	struct virtio_dev_queue_info *q_info;

	hw = &vpdev->hw;
	q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);
	q_info[qid].q_cfg.queue_enable = rte_cpu_to_le_16(0);
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
virtio_pci_dev_state_interrupt_enable(struct virtio_pci_dev *vpdev, int fd, int vec, void *state)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct vfio_irq_set *irq_set;
	struct virtio_dev_queue_info *q_info;
	struct virtio_dev_common_state *state_info;
	int ret, *data;

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
	data = (int *)&irq_set->data;
	*data = fd;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	rte_free(irq_set);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s enabling MSI-X: %s", VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	if (vec == 0) {
		state_info = state;
		state_info->common_cfg.msix_config = 0;
	} else {
		q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);
		q_info[vec-1].q_cfg.queue_msix_vector = rte_cpu_to_le_16(vec);
	}
	return 0;
}

int
virtio_pci_dev_state_interrupt_disable(struct virtio_pci_dev *vpdev, int vec, void *state)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct vfio_irq_set *irq_set;
	struct virtio_dev_queue_info *q_info;
	struct virtio_dev_common_state *state_info;
	int ret, *data;

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
	data = (int *)&irq_set->data;
	*data = VFIO_FD_INVALID;

	ret = ioctl(vpdev->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	rte_free(irq_set);
	if (ret) {
		PMD_INIT_LOG(ERR, "Dev %s disabling MSI-X: %s", VP_DEV_NAME(vpdev), strerror(errno));
		return ret;
	}

	if (vec == 0) {
		state_info = state;
		state_info->common_cfg.msix_config = rte_cpu_to_le_16(VIRTIO_MSI_NO_VECTOR);
	} else {
		q_info = hw->virtio_dev_sp_ops->get_queue_offset(state);
		q_info[vec-1].q_cfg.queue_msix_vector = rte_cpu_to_le_16(VIRTIO_MSI_NO_VECTOR);
	}

	return 0;
}

int
virtio_pci_dev_interrupt_enable(struct virtio_pci_dev *vpdev, int fd, int vec)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct vfio_irq_set *irq_set;
	int ret, *data;

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
	data = (int *)&irq_set->data;
	*data = fd;

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
	int ret, *data;

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
	data = (int *)&irq_set->data;
	*data = VFIO_FD_INVALID;

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
virtio_pci_dev_reset(struct virtio_pci_dev *vpdev, uint32_t time_out_ms)
{
	uint32_t retry = 0;
	struct virtio_hw *hw = &vpdev->hw;

	VIRTIO_OPS(hw)->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* Flush status write and wait device ready max 120 seconds. */
	while (VIRTIO_OPS(hw)->get_status(hw) != VIRTIO_CONFIG_STATUS_RESET) {
		if (retry++ > time_out_ms) {
			PMD_INIT_LOG(WARNING, "vpdev %s  reset %d ms timeout", VP_DEV_NAME(vpdev), time_out_ms);
			break;
		}
		if (!(retry % 1000))
			PMD_INIT_LOG(INFO, "vpdev %s  resetting", VP_DEV_NAME(vpdev));
		usleep(1000L);
	}
}

void
virtio_pci_dev_state_config_read(struct virtio_pci_dev *vpdev, void *dst, int length, void *state)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct virtio_dev_common_state *state_info = state;
	uint16_t dev_cfg_len;

	dev_cfg_len = hw->virtio_dev_sp_ops->get_dev_cfg_size();

	if (length != dev_cfg_len) {
		PMD_INIT_LOG(INFO, "vpdev %s copy len %d != cfg len %d use min", VP_DEV_NAME(vpdev), length, dev_cfg_len);
	}
	rte_memcpy(dst, state_info + 1, RTE_MIN(length, dev_cfg_len));

	PMD_INIT_LOG(INFO, "vpdev %s dev cfg copy len: %d", VP_DEV_NAME(vpdev), RTE_MIN(length, dev_cfg_len));
}

void
virtio_pci_dev_state_config_write(struct virtio_pci_dev *vpdev, void *src, int length, void *state)
{
	struct virtio_hw *hw = &vpdev->hw;
	struct virtio_dev_common_state *state_info = state;
	uint16_t dev_cfg_len;

	dev_cfg_len = hw->virtio_dev_sp_ops->get_dev_cfg_size();

	if (length != dev_cfg_len) {
		PMD_INIT_LOG(INFO, "vpdev %s write len %d != cfg len %d use min", VP_DEV_NAME(vpdev), length, dev_cfg_len);
	}
	rte_memcpy(state_info + 1, src, RTE_MIN(length, dev_cfg_len));

	PMD_INIT_LOG(INFO, "vpdev %s dev cfg copy len: %d", VP_DEV_NAME(vpdev), RTE_MIN(length, dev_cfg_len));
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
virtio_pci_dev_isr_get(struct virtio_pci_dev *vpdev)
{
	struct virtio_hw *hw = &vpdev->hw;

	return VIRTIO_OPS(hw)->get_isr(hw);
}

RTE_LOG_REGISTER(virtio_pci_dev_logtype, pmd.virtio.dev, NOTICE);
