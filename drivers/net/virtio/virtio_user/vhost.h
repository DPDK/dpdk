/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_VHOST_H
#define _VIRTIO_USER_VHOST_H

#include <stdint.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include <rte_errno.h>

#include "../virtio.h"
#include "../virtio_logs.h"
#include "../virtqueue.h"

struct vhost_vring_state {
	unsigned int index;
	unsigned int num;
};

struct vhost_vring_file {
	unsigned int index;
	int fd;
};

struct vhost_vring_addr {
	unsigned int index;
	/* Option flags. */
	unsigned int flags;
	/* Flag values: */
	/* Whether log address is valid. If set enables logging. */
#define VHOST_VRING_F_LOG 0

	/* Start of array of descriptors (virtually contiguous) */
	uint64_t desc_user_addr;
	/* Used structure address. Must be 32 bit aligned */
	uint64_t used_user_addr;
	/* Available structure address. Must be 16 bit aligned */
	uint64_t avail_user_addr;
	/* Logging support. */
	/* Log writes to used structure, at offset calculated from specified
	 * address. Address must be 32 bit aligned.
	 */
	uint64_t log_guest_addr;
};

#ifndef VHOST_BACKEND_F_IOTLB_MSG_V2
#define VHOST_BACKEND_F_IOTLB_MSG_V2 1
#endif

#ifndef VHOST_BACKEND_F_IOTLB_BATCH
#define VHOST_BACKEND_F_IOTLB_BATCH 2
#endif

struct vhost_memory_region {
	uint64_t guest_phys_addr;
	uint64_t memory_size; /* bytes */
	uint64_t userspace_addr;
	uint64_t mmap_offset;
};

struct virtio_user_dev;

struct virtio_user_backend_ops {
	int (*setup)(struct virtio_user_dev *dev);
	int (*destroy)(struct virtio_user_dev *dev);
	int (*get_backend_features)(uint64_t *features);
	int (*set_owner)(struct virtio_user_dev *dev);
	int (*get_features)(struct virtio_user_dev *dev, uint64_t *features);
	int (*set_features)(struct virtio_user_dev *dev, uint64_t features);
	int (*set_memory_table)(struct virtio_user_dev *dev);
	int (*set_vring_num)(struct virtio_user_dev *dev, struct vhost_vring_state *state);
	int (*set_vring_base)(struct virtio_user_dev *dev, struct vhost_vring_state *state);
	int (*get_vring_base)(struct virtio_user_dev *dev, struct vhost_vring_state *state);
	int (*set_vring_call)(struct virtio_user_dev *dev, struct vhost_vring_file *file);
	int (*set_vring_kick)(struct virtio_user_dev *dev, struct vhost_vring_file *file);
	int (*set_vring_addr)(struct virtio_user_dev *dev, struct vhost_vring_addr *addr);
	int (*get_status)(struct virtio_user_dev *dev, uint8_t *status);
	int (*set_status)(struct virtio_user_dev *dev, uint8_t status);
	int (*get_config)(struct virtio_user_dev *dev, uint8_t *data, uint32_t off, uint32_t len);
	int (*set_config)(struct virtio_user_dev *dev, const uint8_t *data, uint32_t off,
			uint32_t len);
	int (*enable_qp)(struct virtio_user_dev *dev, uint16_t pair_idx, int enable);
	int (*dma_map)(struct virtio_user_dev *dev, void *addr, uint64_t iova, size_t len);
	int (*dma_unmap)(struct virtio_user_dev *dev, void *addr, uint64_t iova, size_t len);
	int (*update_link_state)(struct virtio_user_dev *dev);
	int (*server_disconnect)(struct virtio_user_dev *dev);
	int (*server_reconnect)(struct virtio_user_dev *dev);
	int (*get_intr_fd)(struct virtio_user_dev *dev);
};

extern struct virtio_user_backend_ops virtio_ops_user;
extern struct virtio_user_backend_ops virtio_ops_kernel;
extern struct virtio_user_backend_ops virtio_ops_vdpa;

#endif
