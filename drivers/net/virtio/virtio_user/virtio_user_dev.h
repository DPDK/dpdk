/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_DEV_H
#define _VIRTIO_USER_DEV_H

#include <limits.h>
#include "../virtio_pci.h"
#include "../virtio_ring.h"
#include "vhost.h"

struct virtio_user_dev {
	/* for vhost_user backend */
	int		vhostfd;

	/* for vhost_kernel backend */
	char		*ifname;
	int		*vhostfds;
	int		*tapfds;

	/* for both vhost_user and vhost_kernel */
	int		callfds[VIRTIO_MAX_VIRTQUEUES];
	int		kickfds[VIRTIO_MAX_VIRTQUEUES];
	int		mac_specified;
	uint32_t	max_queue_pairs;
	uint32_t	queue_pairs;
	uint32_t	queue_size;
	uint64_t	features; /* the negotiated features with driver,
				   * and will be sync with device
				   */
	uint64_t	device_features; /* supported features by device */
	uint8_t		status;
	uint16_t	port_id;
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	char		path[PATH_MAX];
	struct vring	vrings[VIRTIO_MAX_VIRTQUEUES];
	struct virtio_user_backend_ops *ops;
};

int is_vhost_user_by_type(const char *path);
int virtio_user_start_device(struct virtio_user_dev *dev);
int virtio_user_stop_device(struct virtio_user_dev *dev);
int virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
			 int cq, int queue_size, const char *mac, char **ifname);
void virtio_user_dev_uninit(struct virtio_user_dev *dev);
void virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx);
#endif
