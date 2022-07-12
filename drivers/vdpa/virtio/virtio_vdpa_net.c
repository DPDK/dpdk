/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <rte_common.h>
#include <rte_vhost.h>
#include "rte_vf_rpc.h"
#include "virtio_api.h"
#include "virtio_vdpa.h"

#define VIRTIO_VDPA_NET_PROTOCOL_FEATURES \
				((1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_STATUS) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK))

extern int virtio_vdpa_logtype;
#define NET_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_logtype, \
		"VIRTIO VDPA NET %s(): " fmt "\n", __func__, ##args)

static void
virtio_vdpa_net_vhost_feature_get(uint64_t *features)
{
	*features = VIRTIO_VDPA_NET_PROTOCOL_FEATURES;
}

static int
virtio_vdpa_net_dirty_desc_get(int vid, int qix, uint64_t *desc_addr, uint32_t *write_len)
{
	struct rte_vhost_vring vq;
	uint32_t desc_id;
	int ret;

	ret = rte_vhost_get_vhost_vring(vid, qix, &vq);
	if (ret) {
		NET_LOG(ERR, "VID: %d qix:%d fail get vhost ring", vid, qix);
		return -ENODEV;
	}

	desc_id = vq.used->ring[(vq.used->idx -1) & (vq.size -1)].id;
	*desc_addr = vq.desc[desc_id].addr;
	*write_len = RTE_MIN(vq.used->ring[(vq.used->idx -1) & (vq.size -1)].len, vq.desc[desc_id].len);

	return 0;
}

struct virtio_vdpa_device_callback virtio_vdpa_net_callback = {
	.vhost_feature_get = virtio_vdpa_net_vhost_feature_get,
	.dirty_desc_get = virtio_vdpa_net_dirty_desc_get,
};

