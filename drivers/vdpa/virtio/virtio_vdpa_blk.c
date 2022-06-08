/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <unistd.h>
#include <rte_common.h>
#include <rte_vhost.h>
#include "virtio_vdpa.h"

#define VIRTIO_VDPA_BLK_PROTOCOL_FEATURES \
				((1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_CONFIG) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_STATUS) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK))

static void
virtio_vdpa_blk_vhost_feature_get(uint64_t *features)
{
	*features = VIRTIO_VDPA_BLK_PROTOCOL_FEATURES;
}

struct virtio_vdpa_device_callback virtio_vdpa_blk_callback = {
	.vhost_feature_get = virtio_vdpa_blk_vhost_feature_get,
};

