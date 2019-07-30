/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_VHOST_H
#define _VIRTIO_USER_VHOST_H

#include <stdint.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include "../virtio_pci.h"
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

enum vhost_user_request {
	VHOST_USER_NONE = 0,
	VHOST_USER_GET_FEATURES = 1,
	VHOST_USER_SET_FEATURES = 2,
	VHOST_USER_SET_OWNER = 3,
	VHOST_USER_RESET_OWNER = 4,
	VHOST_USER_SET_MEM_TABLE = 5,
	VHOST_USER_SET_LOG_BASE = 6,
	VHOST_USER_SET_LOG_FD = 7,
	VHOST_USER_SET_VRING_NUM = 8,
	VHOST_USER_SET_VRING_ADDR = 9,
	VHOST_USER_SET_VRING_BASE = 10,
	VHOST_USER_GET_VRING_BASE = 11,
	VHOST_USER_SET_VRING_KICK = 12,
	VHOST_USER_SET_VRING_CALL = 13,
	VHOST_USER_SET_VRING_ERR = 14,
	VHOST_USER_GET_PROTOCOL_FEATURES = 15,
	VHOST_USER_SET_PROTOCOL_FEATURES = 16,
	VHOST_USER_GET_QUEUE_NUM = 17,
	VHOST_USER_SET_VRING_ENABLE = 18,
	VHOST_USER_MAX
};

extern const char * const vhost_msg_strings[VHOST_USER_MAX];

struct vhost_memory_region {
	uint64_t guest_phys_addr;
	uint64_t memory_size; /* bytes */
	uint64_t userspace_addr;
	uint64_t mmap_offset;
};

struct virtio_user_dev;

struct virtio_user_backend_ops {
	int (*setup)(struct virtio_user_dev *dev);
	int (*send_request)(struct virtio_user_dev *dev,
			    enum vhost_user_request req,
			    void *arg);
	int (*enable_qp)(struct virtio_user_dev *dev,
			 uint16_t pair_idx,
			 int enable);
};

extern struct virtio_user_backend_ops virtio_ops_user;
extern struct virtio_user_backend_ops virtio_ops_kernel;

#endif
