/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_DEV_H
#define _VIRTIO_USER_DEV_H

#include <limits.h>
#include <stdbool.h>
#include "../virtio_pci.h"
#include "../virtio_ring.h"

enum virtio_user_backend_type {
	VIRTIO_USER_BACKEND_UNKNOWN,
	VIRTIO_USER_BACKEND_VHOST_USER,
	VIRTIO_USER_BACKEND_VHOST_KERNEL,
	VIRTIO_USER_BACKEND_VHOST_VDPA,
};

struct virtio_user_queue {
	uint16_t used_idx;
	bool avail_wrap_counter;
	bool used_wrap_counter;
};

struct virtio_user_dev {
	enum virtio_user_backend_type backend_type;
	/* for vhost_user backend */
	int		vhostfd;
	int		listenfd;   /* listening fd */
	bool		is_server;  /* server or client mode */

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
	uint64_t	frontend_features; /* enabled frontend features */
	uint64_t	unsupported_features; /* unsupported features mask */
	uint64_t	protocol_features; /* negotiated protocol features */
	uint8_t		status;
	uint16_t	net_status;
	uint16_t	port_id;
	uint8_t		mac_addr[RTE_ETHER_ADDR_LEN];
	char		path[PATH_MAX];
	union {
		struct vring		vrings[VIRTIO_MAX_VIRTQUEUES];
		struct vring_packed	packed_vrings[VIRTIO_MAX_VIRTQUEUES];
	};
	struct virtio_user_queue packed_queues[VIRTIO_MAX_VIRTQUEUES];
	bool		qp_enabled[VIRTIO_MAX_VIRTQUEUE_PAIRS];

	struct virtio_user_backend_ops *ops;
	pthread_mutex_t	mutex;
	bool		started;
};

int virtio_user_dev_set_features(struct virtio_user_dev *dev);
int virtio_user_start_device(struct virtio_user_dev *dev);
int virtio_user_stop_device(struct virtio_user_dev *dev);
int virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
			 int cq, int queue_size, const char *mac, char **ifname,
			 int server, int mrg_rxbuf, int in_order,
			 int packed_vq,
			 enum virtio_user_backend_type backend_type);
void virtio_user_dev_uninit(struct virtio_user_dev *dev);
void virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx);
void virtio_user_handle_cq_packed(struct virtio_user_dev *dev,
				  uint16_t queue_idx);
uint8_t virtio_user_handle_mq(struct virtio_user_dev *dev, uint16_t q_pairs);
int virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status);
int virtio_user_dev_update_status(struct virtio_user_dev *dev);
extern const char * const virtio_user_backend_strings[];
#endif
