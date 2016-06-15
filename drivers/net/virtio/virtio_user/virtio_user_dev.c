/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "vhost.h"
#include "virtio_user_dev.h"
#include "../virtio_ethdev.h"

static int
virtio_user_kick_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	int callfd, kickfd;
	struct vhost_vring_file file;
	struct vhost_vring_state state;
	struct vring *vring = &dev->vrings[queue_sel];
	struct vhost_vring_addr addr = {
		.index = queue_sel,
		.desc_user_addr = (uint64_t)(uintptr_t)vring->desc,
		.avail_user_addr = (uint64_t)(uintptr_t)vring->avail,
		.used_user_addr = (uint64_t)(uintptr_t)vring->used,
		.log_guest_addr = 0,
		.flags = 0, /* disable log */
	};

	/* May use invalid flag, but some backend leverages kickfd and callfd as
	 * criteria to judge if dev is alive. so finally we use real event_fd.
	 */
	callfd = eventfd(0, O_CLOEXEC | O_NONBLOCK);
	if (callfd < 0) {
		PMD_DRV_LOG(ERR, "callfd error, %s\n", strerror(errno));
		return -1;
	}
	kickfd = eventfd(0, O_CLOEXEC | O_NONBLOCK);
	if (kickfd < 0) {
		close(callfd);
		PMD_DRV_LOG(ERR, "kickfd error, %s\n", strerror(errno));
		return -1;
	}

	/* Of all per virtqueue MSGs, make sure VHOST_SET_VRING_CALL come
	 * firstly because vhost depends on this msg to allocate virtqueue
	 * pair.
	 */
	file.index = queue_sel;
	file.fd = callfd;
	vhost_user_sock(dev->vhostfd, VHOST_USER_SET_VRING_CALL, &file);
	dev->callfds[queue_sel] = callfd;

	state.index = queue_sel;
	state.num = vring->num;
	vhost_user_sock(dev->vhostfd, VHOST_USER_SET_VRING_NUM, &state);

	state.num = 0; /* no reservation */
	vhost_user_sock(dev->vhostfd, VHOST_USER_SET_VRING_BASE, &state);

	vhost_user_sock(dev->vhostfd, VHOST_USER_SET_VRING_ADDR, &addr);

	/* Of all per virtqueue MSGs, make sure VHOST_USER_SET_VRING_KICK comes
	 * lastly because vhost depends on this msg to judge if
	 * virtio is ready.
	 */
	file.fd = kickfd;
	vhost_user_sock(dev->vhostfd, VHOST_USER_SET_VRING_KICK, &file);
	dev->kickfds[queue_sel] = kickfd;

	return 0;
}

int
virtio_user_start_device(struct virtio_user_dev *dev)
{
	uint64_t features;
	uint32_t i, queue_sel;
	int ret;

	/* construct memory region inside each implementation */
	ret = vhost_user_sock(dev->vhostfd, VHOST_USER_SET_MEM_TABLE, NULL);
	if (ret < 0)
		goto error;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		queue_sel = 2 * i + VTNET_SQ_RQ_QUEUE_IDX;
		if (virtio_user_kick_queue(dev, queue_sel) < 0) {
			PMD_DRV_LOG(INFO, "kick rx vq fails: %u", i);
			goto error;
		}
	}
	for (i = 0; i < dev->max_queue_pairs; ++i) {
		queue_sel = 2 * i + VTNET_SQ_TQ_QUEUE_IDX;
		if (virtio_user_kick_queue(dev, queue_sel) < 0) {
			PMD_DRV_LOG(INFO, "kick tx vq fails: %u", i);
			goto error;
		}
	}

	/* After setup all virtqueues, we need to set_features so that
	 * these features can be set into each virtqueue in vhost side.
	 * And before that, make sure VIRTIO_NET_F_MAC is stripped.
	 */
	features = dev->features;
	features &= ~(1ull << VIRTIO_NET_F_MAC);
	ret = vhost_user_sock(dev->vhostfd, VHOST_USER_SET_FEATURES, &features);
	if (ret < 0)
		goto error;
	PMD_DRV_LOG(INFO, "set features: %" PRIx64, features);

	return 0;
error:
	/* TODO: free resource here or caller to check */
	return -1;
}

int virtio_user_stop_device(struct virtio_user_dev *dev)
{
	return vhost_user_sock(dev->vhostfd, VHOST_USER_RESET_OWNER, NULL);
}

static inline void
parse_mac(struct virtio_user_dev *dev, const char *mac)
{
	int i, r;
	uint32_t tmp[ETHER_ADDR_LEN];

	if (!mac)
		return;

	r = sscanf(mac, "%x:%x:%x:%x:%x:%x", &tmp[0],
			&tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	if (r == ETHER_ADDR_LEN) {
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			dev->mac_addr[i] = (uint8_t)tmp[i];
		dev->mac_specified = 1;
	} else {
		/* ignore the wrong mac, use random mac */
		PMD_DRV_LOG(ERR, "wrong format of mac: %s", mac);
	}
}

int
virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
		     int cq, int queue_size, const char *mac)
{
	strncpy(dev->path, path, PATH_MAX);
	dev->max_queue_pairs = queues;
	dev->queue_pairs = 1; /* mq disabled by default */
	dev->queue_size = queue_size;
	dev->mac_specified = 0;
	parse_mac(dev, mac);
	dev->vhostfd = -1;
	/* TODO: cq */
	RTE_SET_USED(cq);

	dev->vhostfd = vhost_user_setup(dev->path);
	if (dev->vhostfd < 0) {
		PMD_INIT_LOG(ERR, "backend set up fails");
		return -1;
	}
	if (vhost_user_sock(dev->vhostfd, VHOST_USER_SET_OWNER, NULL) < 0) {
		PMD_INIT_LOG(ERR, "set_owner fails: %s", strerror(errno));
		return -1;
	}

	if (vhost_user_sock(dev->vhostfd, VHOST_USER_GET_FEATURES,
			    &dev->features) < 0) {
		PMD_INIT_LOG(ERR, "get_features failed: %s", strerror(errno));
		return -1;
	}
	if (dev->mac_specified)
		dev->features |= (1ull << VIRTIO_NET_F_MAC);
	/* disable it until we support CQ */
	dev->features &= ~(1ull << VIRTIO_NET_F_CTRL_VQ);
	dev->features &= ~(1ull << VIRTIO_NET_F_CTRL_RX);

	return 0;
}

void
virtio_user_dev_uninit(struct virtio_user_dev *dev)
{
	uint32_t i;

	for (i = 0; i < dev->max_queue_pairs * 2; ++i) {
		close(dev->callfds[i]);
		close(dev->kickfds[i]);
	}

	close(dev->vhostfd);
}
