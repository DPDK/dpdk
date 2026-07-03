/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_DEV_H
#define _VIRTIO_USER_DEV_H

#include <limits.h>
#include <stdbool.h>

#include "../virtio.h"
#include "../virtio_ring.h"

#include <rte_eal.h>

/* Max eventfds shareable over the MP channel (bounded by SCM_RIGHTS). */
#define VIRTIO_USER_MAX_EVENTFDS RTE_MP_MAX_FD_NUM

/* Each queue contributes one kick fd and one call fd to the MP channel. */
#define VIRTIO_USER_EVENTFDS_PER_QUEUE 2

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
	struct virtio_hw hw;
	enum virtio_user_backend_type backend_type;
	bool		is_server;  /* server or client mode */

	int		*callfds;
	int		*kickfds;
	int		mac_specified;
	uint16_t	max_queue_pairs;
	uint16_t	queue_pairs;
	uint32_t	queue_size;
	uint64_t	features; /* the negotiated features with driver,
				   * and will be sync with device
				   */
	uint64_t	device_features; /* supported features by device */
	uint64_t	frontend_features; /* enabled frontend features */
	uint64_t	unsupported_features; /* unsupported features mask */
	uint8_t		status;
	uint16_t	net_status;
	uint8_t		mac_addr[RTE_ETHER_ADDR_LEN];
	char		path[PATH_MAX];
	char		*ifname;

	union {
		void			*ptr;
		struct vring		*split;
		struct vring_packed	*packed;
	} vrings;

	struct virtio_user_queue *packed_queues;
	bool		*qp_enabled;

	struct virtio_user_backend_ops *ops;
	pthread_mutex_t	mutex;
	bool		started;

	bool			hw_cvq;
	struct virtqueue	*scvq;

	void *backend_data;

	uint16_t **notify_area;
};

int virtio_user_dev_set_features(struct virtio_user_dev *dev);
int virtio_user_start_device(struct virtio_user_dev *dev);
int virtio_user_stop_device(struct virtio_user_dev *dev);
int virtio_user_dev_init(struct virtio_user_dev *dev, char *path, uint16_t queues,
			 int cq, int queue_size, const char *mac, char **ifname,
			 int server, int mrg_rxbuf, int in_order,
			 int packed_vq,
			 enum virtio_user_backend_type backend_type);
void virtio_user_dev_uninit(struct virtio_user_dev *dev);
void virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx);
int virtio_user_dev_create_shadow_cvq(struct virtio_user_dev *dev, struct virtqueue *vq);
void virtio_user_dev_destroy_shadow_cvq(struct virtio_user_dev *dev);
int virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status);
int virtio_user_dev_update_status(struct virtio_user_dev *dev);
int virtio_user_dev_update_link_state(struct virtio_user_dev *dev);
int virtio_user_dev_set_mac(struct virtio_user_dev *dev);
int virtio_user_dev_get_mac(struct virtio_user_dev *dev);
int virtio_user_dev_get_rss_config(struct virtio_user_dev *dev, void *dst, size_t offset,
				   int length);
void virtio_user_dev_delayed_disconnect_handler(void *param);
int virtio_user_dev_server_reconnect(struct virtio_user_dev *dev);

/**
 * Collect a primary device's kick/call eventfds for sharing with a
 * secondary process over the multiprocess channel.
 *
 * @param dev
 *   Pointer to the virtio_user device (primary).
 * @param fds
 *   Output array, must hold at least VIRTIO_USER_MAX_EVENTFDS elements.
 * @return
 *   Number of fds written on success, negative errno on error.
 */
int virtio_user_get_eventfds_from_dev(struct virtio_user_dev *dev,
				     int fds[VIRTIO_USER_MAX_EVENTFDS]);

extern const char * const virtio_user_backend_strings[];
#endif
