/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

/**
 * This work is largely based on the "vhost-user-blk" implementation by
 * SPDK(https://github.com/spdk/spdk).
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_string_fns.h>

#include "vhost_blk.h"
#include "blk_spec.h"

static void
vhost_strcpy_pad(void *dst, const char *src, size_t size, int pad)
{
	size_t len;

	len = strlen(src);
	if (len < size) {
		memcpy(dst, src, len);
		memset((char *)dst + len, pad, size - len);
	} else {
		memcpy(dst, src, size);
	}
}

static int
vhost_bdev_blk_readwrite(struct vhost_block_dev *bdev,
			  struct vhost_blk_task *task,
			  uint64_t lba_512, __rte_unused uint32_t xfer_len)
{
	uint32_t i;
	uint64_t offset;
	uint32_t nbytes = 0;

	offset = lba_512 * 512;

	/* iovs[0] is the head and iovs[iovs_cnt - 1] is the tail
	 * Middle is the data range
	 */
	for (i = 1; i < task->iovs_cnt - 1; i++) {
		if (task->dxfer_dir == BLK_DIR_TO_DEV)
			memcpy(bdev->data + offset, task->iovs[i].iov_base,
			       task->iovs[i].iov_len);
		else
			memcpy(task->iovs[i].iov_base, bdev->data + offset,
			       task->iovs[i].iov_len);
		offset += task->iovs[i].iov_len;
		nbytes += task->iovs[i].iov_len;
	}

	return nbytes;
}

int
vhost_bdev_process_blk_commands(struct vhost_block_dev *bdev,
				 struct vhost_blk_task *task)
{
	size_t used_len;

	if (unlikely(task->data_len > (bdev->blockcnt * bdev->blocklen))) {
		fprintf(stderr, "read or write beyond capacity\n");
		return VIRTIO_BLK_S_UNSUPP;
	}

	switch (task->req->type) {
	case VIRTIO_BLK_T_IN:
		if (unlikely(task->data_len == 0 ||
			(task->data_len & (512 - 1)) != 0)) {
			fprintf(stderr,
				"%s - passed IO buffer is not multiple of 512b"
				"(req_idx = %"PRIu16").\n",
				task->req->type ? "WRITE" : "READ",
				task->req_idx);
			return VIRTIO_BLK_S_UNSUPP;
		}

		task->dxfer_dir = BLK_DIR_FROM_DEV;
		vhost_bdev_blk_readwrite(bdev, task,
					 task->req->sector, task->data_len);
		break;
	case VIRTIO_BLK_T_OUT:
		if (unlikely(task->data_len == 0 ||
			(task->data_len & (512 - 1)) != 0)) {
			fprintf(stderr,
				"%s - passed IO buffer is not multiple of 512b"
				"(req_idx = %"PRIu16").\n",
				task->req->type ? "WRITE" : "READ",
				task->req_idx);
			return VIRTIO_BLK_S_UNSUPP;
		}

		task->dxfer_dir = BLK_DIR_TO_DEV;
		vhost_bdev_blk_readwrite(bdev, task,
					 task->req->sector, task->data_len);
		break;
	case VIRTIO_BLK_T_GET_ID:
		if (!task->iovs_cnt || task->data_len)
			return VIRTIO_BLK_S_UNSUPP;
		used_len = RTE_MIN((size_t)VIRTIO_BLK_ID_BYTES, task->data_len);
		vhost_strcpy_pad(task->iovs[0].iov_base,
				 bdev->product_name, used_len, ' ');
		break;
	default:
		fprintf(stderr, "unsupported cmd\n");
		return VIRTIO_BLK_S_UNSUPP;
	}

	return VIRTIO_BLK_S_OK;
}
