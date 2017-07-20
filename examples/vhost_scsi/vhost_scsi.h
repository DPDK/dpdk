/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
