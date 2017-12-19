/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _VHOST_SCSI_H_
#define _VHOST_SCSI_H_

#include <sys/uio.h>
#include <stdint.h>
#include <linux/virtio_scsi.h>
#include <linux/virtio_ring.h>

#include <rte_vhost.h>

struct vhost_scsi_queue {
	struct rte_vhost_vring vq;
	uint16_t last_avail_idx;
	uint16_t last_used_idx;
};

#define NUM_OF_SCSI_QUEUES 3

struct vhost_block_dev {
	/** ID for vhost library. */
	int vid;
	/** Queues for the block device */
	struct vhost_scsi_queue queues[NUM_OF_SCSI_QUEUES];
	/** Unique name for this block device. */
	char name[64];

	/** Unique product name for this kind of block device. */
	char product_name[256];

	/** Size in bytes of a logical block for the backend */
	uint32_t blocklen;

	/** Number of blocks */
	uint64_t blockcnt;

	/** write cache enabled, not used at the moment */
	int write_cache;

	/** use memory as disk storage space */
	uint8_t *data;
};

struct vhost_scsi_ctrlr {
	/** Only support 1 LUN for the example */
	struct vhost_block_dev *bdev;
	/** VM memory region */
	struct rte_vhost_memory *mem;
} __rte_cache_aligned;

#define VHOST_SCSI_MAX_IOVS 128

enum scsi_data_dir {
	SCSI_DIR_NONE = 0,
	SCSI_DIR_TO_DEV = 1,
	SCSI_DIR_FROM_DEV = 2,
};

struct vhost_scsi_task {
	int req_idx;
	uint32_t dxfer_dir;
	uint32_t data_len;
	struct virtio_scsi_cmd_req *req;
	struct virtio_scsi_cmd_resp *resp;
	struct iovec iovs[VHOST_SCSI_MAX_IOVS];
	uint32_t iovs_cnt;
	struct vring_desc *desc;
	struct rte_vhost_vring *vq;
	struct vhost_block_dev *bdev;
	struct vhost_scsi_ctrlr *ctrlr;
};

int vhost_bdev_process_scsi_commands(struct vhost_block_dev *bdev,
				     struct vhost_scsi_task *task);

#endif /* _VHOST_SCSI_H_ */
