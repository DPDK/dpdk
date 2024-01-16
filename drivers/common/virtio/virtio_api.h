/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_API_H_
#define _VIRTIO_API_H_

#define VIRTIO_API_FLAG
#include "virtio_pci.h"
#undef VIRTIO_API_FLAG

#include "virtio_util.h"

/* Status byte for guest to report progress. */
#define VIRTIO_CONFIG_STATUS_RESET		0x00
#define VIRTIO_CONFIG_STATUS_ACK		0x01
#define VIRTIO_CONFIG_STATUS_DRIVER		0x02
#define VIRTIO_CONFIG_STATUS_DRIVER_OK		0x04
#define VIRTIO_CONFIG_STATUS_FEATURES_OK	0x08
#define VIRTIO_CONFIG_STATUS_DEV_NEED_RESET	0x40
#define VIRTIO_CONFIG_STATUS_FAILED		0x80

#define VIRTIO_ARG_VDPA "vdpa"
#define VIRTIO_ARG_VM_UUID "vm_uuid"
#define VIRTIO_ARG_VDPA_VALUE_VF "1"
#define VIRTIO_ARG_VDPA_VALUE_PF "2"

#define VIRTIO_VDPA_PROBE_RESET_TIME_OUT 120000
#define VIRTIO_VDPA_REMOVE_RESET_TIME_OUT 3000

struct virtio_pci_dev_vring_info {
	uint64_t desc;
	uint64_t avail;
	uint64_t used;
	uint16_t size;
};

static inline void
virtio_pci_dev_queue_notify(struct virtio_pci_dev *vpdev, uint16_t qid)
{
	VIRTIO_OPS(&vpdev->hw)->notify_queue(&vpdev->hw, vpdev->hw.vqs[qid]);
}

struct virtio_dev_run_state_info {
	uint16_t last_avail_idx;
	uint16_t last_used_idx;
	bool flag;
} __rte_packed;

/* When dev_fd is -1, alloc with no device fd. When dev_fd is a valid devie file
 * descriptor, alloc with the device fd that needs to be restored
 */
__rte_internal
struct virtio_pci_dev *virtio_pci_dev_alloc(struct rte_pci_device *pci_dev, int dev_fd);
__rte_internal
uint16_t virtio_pci_dev_nr_vq_get(struct virtio_pci_dev *vpdev);
__rte_internal
int virtio_pci_dev_queues_alloc(struct virtio_pci_dev *vpdev, uint16_t nr_vq);
__rte_internal
void virtio_pci_dev_queues_free(struct virtio_pci_dev *vpdev, uint16_t nr_vq);
__rte_internal
uint16_t virtio_pci_dev_queue_size_get(struct virtio_pci_dev *vpdev, uint16_t idx);
__rte_internal
void virtio_pci_dev_free(struct virtio_pci_dev *vpdev);
__rte_internal
void virtio_pci_dev_features_get(struct virtio_pci_dev *vpdev, uint64_t *features);
__rte_internal
uint64_t virtio_pci_dev_features_set(struct virtio_pci_dev *vpdev, uint64_t features);
__rte_internal
uint64_t virtio_pci_dev_state_features_set(struct virtio_pci_dev *vpdev, uint64_t features, void *state);
__rte_internal
void virtio_pci_dev_state_features_get(struct virtio_pci_dev *vpdev, uint64_t *features);
__rte_internal
int virtio_pci_dev_queue_set(struct virtio_pci_dev *vpdev, uint16_t qid, const struct virtio_pci_dev_vring_info *vring_info);
__rte_internal
void virtio_pci_dev_queue_del(struct virtio_pci_dev *vpdev, uint16_t qid);
__rte_internal
void virtio_pci_dev_state_dev_status_set(void *state, uint8_t dev_status);
__rte_internal
void virtio_pci_dev_state_dump(struct virtio_pci_dev *vpdev, void *state, uint32_t state_size);
__rte_internal
void virtio_pci_dev_state_all_queues_disable(struct virtio_pci_dev *vpdev, void *state);
__rte_internal
int virtio_pci_dev_state_hw_idx_set(struct virtio_pci_dev *vpdev, uint16_t qid, uint16_t last_avail_idx, uint16_t last_used_idx, void *state);
__rte_internal
int virtio_pci_dev_state_hw_idx_get(void *state, uint32_t state_size, struct virtio_dev_run_state_info *hw_idx_info, int num_queues);
__rte_internal
int virtio_pci_dev_state_queue_set(struct virtio_pci_dev *vpdev, uint16_t qid, const struct virtio_pci_dev_vring_info *vring_info, void *state);
__rte_internal
void virtio_pci_dev_state_queue_del(struct virtio_pci_dev *vpdev, uint16_t qid, void *state);
__rte_internal
int virtio_pci_dev_state_interrupt_enable(struct virtio_pci_dev *vpdev, int fd, int vec, void *state);
__rte_internal
int virtio_pci_dev_state_interrupt_enable_only(struct virtio_pci_dev *vpdev, int vec, void *state);
__rte_internal
int virtio_pci_dev_state_interrupt_disable(struct virtio_pci_dev *vpdev, int vec, void *state);
__rte_internal
int virtio_pci_dev_interrupt_enable(struct virtio_pci_dev *vpdev, int fd, int vec);
__rte_internal
int virtio_pci_dev_interrupt_disable(struct virtio_pci_dev *vpdev, int vec);
__rte_internal
int virtio_pci_dev_interrupts_num_get(struct virtio_pci_dev *vpdev);
__rte_internal
int virtio_pci_dev_interrupts_alloc(struct virtio_pci_dev *vpdev, int nvec);
__rte_internal
int virtio_pci_dev_interrupts_free(struct virtio_pci_dev *vpdev);
__rte_internal
void virtio_pci_dev_set_status(struct virtio_pci_dev *vpdev, uint8_t status);
__rte_internal
uint8_t virtio_pci_dev_get_status(struct virtio_pci_dev *vpdev);
__rte_internal
int virtio_pci_dev_reset(struct virtio_pci_dev *vpdev, uint32_t time_out_ms);
__rte_internal
int virtio_pci_dev_notify_area_get(struct virtio_pci_dev *vpdev, uint16_t qid, uint64_t *offset, uint64_t *size);
__rte_internal
void virtio_pci_dev_config_read(struct virtio_pci_dev *vpdev, size_t offset, void *dst, int length);
__rte_internal
void virtio_pci_dev_config_write(struct virtio_pci_dev *vpdev, size_t offset, const void *src, int length);
__rte_internal
void virtio_pci_dev_state_config_read(struct virtio_pci_dev *vpdev, void *dst, int length, void *state);
__rte_internal
void virtio_pci_dev_state_config_write(struct virtio_pci_dev *vpdev, void *src, int length, void *state);
__rte_internal
int virtio_pci_dev_state_bar_copy(struct virtio_pci_dev *vpdev, void *state, int state_len);
__rte_internal
int virtio_pci_dev_state_size_get(struct virtio_pci_dev *vpdev);
__rte_internal
void virtio_pci_dev_state_num_queue_set(struct virtio_pci_dev *vpdev);
__rte_internal
uint8_t virtio_pci_dev_isr_get(struct virtio_pci_dev *vpdev);
const char * rte_vdpa_err_str_get(int32_t err);


#endif /* _VIRTIO_API_H_ */
