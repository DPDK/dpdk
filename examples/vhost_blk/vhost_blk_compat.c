/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _VHOST_BLK_COMPAT_H_
#define _VHOST_BLK_COMPAT_H_

#include <sys/uio.h>
#include <stdint.h>
#include <linux/virtio_blk.h>
#include <linux/virtio_ring.h>

#include <rte_vhost.h>
#include "vhost_blk.h"
#include "blk_spec.h"

#define VHOST_MAX_VQUEUES	256
#define SPDK_VHOST_MAX_VQ_SIZE	1024

#define VHOST_USER_GET_CONFIG	24
#define VHOST_USER_SET_CONFIG	25

static int
vhost_blk_get_config(struct vhost_block_dev *bdev, uint8_t *config,
			  uint32_t len)
{
	struct virtio_blk_config blkcfg;
	uint32_t blk_size;
	uint64_t blkcnt;

	if (bdev == NULL) {
		/* We can't just return -1 here as this GET_CONFIG message might
		 * be caused by a QEMU VM reboot. Returning -1 will indicate an
		 * error to QEMU, who might then decide to terminate itself.
		 * We don't want that. A simple reboot shouldn't break the
		 * system.
		 *
		 * Presenting a block device with block size 0 and block count 0
		 * doesn't cause any problems on QEMU side and the virtio-pci
		 * device is even still available inside the VM, but there will
		 * be no block device created for it - the kernel drivers will
		 * silently reject it.
		 */
		blk_size = 0;
		blkcnt = 0;
	} else {
		blk_size = bdev->blocklen;
		blkcnt = bdev->blockcnt;
	}

	memset(&blkcfg, 0, sizeof(blkcfg));
	blkcfg.blk_size = blk_size;
	/* minimum I/O size in blocks */
	blkcfg.min_io_size = 1;
	/* expressed in 512 Bytes sectors */
	blkcfg.capacity = (blkcnt * blk_size) / 512;
	/* QEMU can overwrite this value when started */
	blkcfg.num_queues = VHOST_MAX_VQUEUES;

	fprintf(stdout, "block device:blk_size = %d, blkcnt = %"PRIx64"\n",
		blk_size, blkcnt);

	memcpy(config, &blkcfg, RTE_MIN(len, sizeof(blkcfg)));

	return 0;
}

static enum rte_vhost_msg_result
extern_vhost_pre_msg_handler(int vid, void *_msg)
{
	char path[PATH_MAX];
	struct vhost_blk_ctrlr *ctrlr;
	struct vhost_user_msg *msg = _msg;
	int ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Cannot get socket name\n");
		return -1;
	}

	ctrlr = vhost_blk_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		return -1;
	}

	switch ((int)msg->request) {
	case VHOST_USER_GET_VRING_BASE:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_SET_VRING_ADDR:
	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_MEM_TABLE:
		break;
	case VHOST_USER_GET_CONFIG: {
		int rc = 0;

		rc = vhost_blk_get_config(ctrlr->bdev,
					  msg->payload.cfg.region,
					  msg->payload.cfg.size);
		if (rc != 0)
			msg->size = 0;

		return RTE_VHOST_MSG_RESULT_REPLY;
	}
	case VHOST_USER_SET_CONFIG:
	default:
		break;
	}

	return RTE_VHOST_MSG_RESULT_NOT_HANDLED;
}

static enum rte_vhost_msg_result
extern_vhost_post_msg_handler(int vid, void *_msg)
{
	char path[PATH_MAX];
	struct vhost_blk_ctrlr *ctrlr;
	struct vhost_user_msg *msg = _msg;
	int ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Cannot get socket name\n");
		return -1;
	}

	ctrlr = vhost_blk_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		return -1;
	}

	switch (msg->request) {
	case VHOST_USER_SET_FEATURES:
	case VHOST_USER_SET_VRING_KICK:
	default:
		break;
	}

	return RTE_VHOST_MSG_RESULT_NOT_HANDLED;
}

struct rte_vhost_user_extern_ops g_extern_vhost_ops = {
	.pre_msg_handle = extern_vhost_pre_msg_handler,
	.post_msg_handle = extern_vhost_post_msg_handler,
};

void
vhost_session_install_rte_compat_hooks(uint32_t vid)
{
	int rc;

	rc = rte_vhost_extern_callback_register(vid, &g_extern_vhost_ops, NULL);
	if (rc != 0)
		fprintf(stderr,
			"rte_vhost_extern_callback_register() failed for vid = %d\n",
			vid);
}

void
vhost_dev_install_rte_compat_hooks(const char *path)
{
	uint64_t protocol_features = 0;

	rte_vhost_driver_get_protocol_features(path, &protocol_features);
	protocol_features |= (1ULL << VHOST_USER_PROTOCOL_F_CONFIG);
	protocol_features |= (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD);
	rte_vhost_driver_set_protocol_features(path, protocol_features);
}

#endif
