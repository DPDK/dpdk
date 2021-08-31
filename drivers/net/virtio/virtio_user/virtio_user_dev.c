/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>

#include "vhost.h"
#include "virtio_user_dev.h"
#include "../virtio_ethdev.h"

#define VIRTIO_USER_MEM_EVENT_CLB_NAME "virtio_user_mem_event_clb"

const char * const virtio_user_backend_strings[] = {
	[VIRTIO_USER_BACKEND_UNKNOWN] = "VIRTIO_USER_BACKEND_UNKNOWN",
	[VIRTIO_USER_BACKEND_VHOST_USER] = "VHOST_USER",
	[VIRTIO_USER_BACKEND_VHOST_KERNEL] = "VHOST_NET",
	[VIRTIO_USER_BACKEND_VHOST_VDPA] = "VHOST_VDPA",
};

static int
virtio_user_create_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	/* Of all per virtqueue MSGs, make sure VHOST_SET_VRING_CALL come
	 * firstly because vhost depends on this msg to allocate virtqueue
	 * pair.
	 */
	struct vhost_vring_file file;

	file.index = queue_sel;
	file.fd = dev->callfds[queue_sel];
	dev->ops->send_request(dev, VHOST_USER_SET_VRING_CALL, &file);

	return 0;
}

static int
virtio_user_kick_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	struct vhost_vring_file file;
	struct vhost_vring_state state;
	struct vring *vring = &dev->vrings[queue_sel];
	struct vring_packed *pq_vring = &dev->packed_vrings[queue_sel];
	struct vhost_vring_addr addr = {
		.index = queue_sel,
		.log_guest_addr = 0,
		.flags = 0, /* disable log */
	};

	if (dev->features & (1ULL << VIRTIO_F_RING_PACKED)) {
		addr.desc_user_addr =
			(uint64_t)(uintptr_t)pq_vring->desc;
		addr.avail_user_addr =
			(uint64_t)(uintptr_t)pq_vring->driver;
		addr.used_user_addr =
			(uint64_t)(uintptr_t)pq_vring->device;
	} else {
		addr.desc_user_addr = (uint64_t)(uintptr_t)vring->desc;
		addr.avail_user_addr = (uint64_t)(uintptr_t)vring->avail;
		addr.used_user_addr = (uint64_t)(uintptr_t)vring->used;
	}

	state.index = queue_sel;
	state.num = vring->num;
	dev->ops->send_request(dev, VHOST_USER_SET_VRING_NUM, &state);

	state.index = queue_sel;
	state.num = 0; /* no reservation */
	if (dev->features & (1ULL << VIRTIO_F_RING_PACKED))
		state.num |= (1 << 15);
	dev->ops->send_request(dev, VHOST_USER_SET_VRING_BASE, &state);

	dev->ops->send_request(dev, VHOST_USER_SET_VRING_ADDR, &addr);

	/* Of all per virtqueue MSGs, make sure VHOST_USER_SET_VRING_KICK comes
	 * lastly because vhost depends on this msg to judge if
	 * virtio is ready.
	 */
	file.index = queue_sel;
	file.fd = dev->kickfds[queue_sel];
	dev->ops->send_request(dev, VHOST_USER_SET_VRING_KICK, &file);

	return 0;
}

static int
virtio_user_queue_setup(struct virtio_user_dev *dev,
			int (*fn)(struct virtio_user_dev *, uint32_t))
{
	uint32_t i, queue_sel;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		queue_sel = 2 * i + VTNET_SQ_RQ_QUEUE_IDX;
		if (fn(dev, queue_sel) < 0) {
			PMD_DRV_LOG(INFO, "setup rx vq fails: %u", i);
			return -1;
		}
	}
	for (i = 0; i < dev->max_queue_pairs; ++i) {
		queue_sel = 2 * i + VTNET_SQ_TQ_QUEUE_IDX;
		if (fn(dev, queue_sel) < 0) {
			PMD_DRV_LOG(INFO, "setup tx vq fails: %u", i);
			return -1;
		}
	}

	return 0;
}

int
virtio_user_dev_set_features(struct virtio_user_dev *dev)
{
	uint64_t features;
	int ret = -1;

	pthread_mutex_lock(&dev->mutex);

	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER &&
			dev->vhostfd < 0)
		goto error;

	/* Step 0: tell vhost to create queues */
	if (virtio_user_queue_setup(dev, virtio_user_create_queue) < 0)
		goto error;

	features = dev->features;

	/* Strip VIRTIO_NET_F_MAC, as MAC address is handled in vdev init */
	features &= ~(1ull << VIRTIO_NET_F_MAC);
	/* Strip VIRTIO_NET_F_CTRL_VQ, as devices do not really need to know */
	features &= ~(1ull << VIRTIO_NET_F_CTRL_VQ);
	features &= ~(1ull << VIRTIO_NET_F_STATUS);
	ret = dev->ops->send_request(dev, VHOST_USER_SET_FEATURES, &features);
	if (ret < 0)
		goto error;
	PMD_DRV_LOG(INFO, "set features: %" PRIx64, features);
error:
	pthread_mutex_unlock(&dev->mutex);

	return ret;
}

int
virtio_user_start_device(struct virtio_user_dev *dev)
{
	int ret;

	/*
	 * XXX workaround!
	 *
	 * We need to make sure that the locks will be
	 * taken in the correct order to avoid deadlocks.
	 *
	 * Before releasing this lock, this thread should
	 * not trigger any memory hotplug events.
	 *
	 * This is a temporary workaround, and should be
	 * replaced when we get proper supports from the
	 * memory subsystem in the future.
	 */
	rte_mcfg_mem_read_lock();
	pthread_mutex_lock(&dev->mutex);

	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER &&
			dev->vhostfd < 0)
		goto error;

	/* Step 2: share memory regions */
	ret = dev->ops->send_request(dev, VHOST_USER_SET_MEM_TABLE, NULL);
	if (ret < 0)
		goto error;

	/* Step 3: kick queues */
	if (virtio_user_queue_setup(dev, virtio_user_kick_queue) < 0)
		goto error;

	/* Step 4: enable queues
	 * we enable the 1st queue pair by default.
	 */
	dev->ops->enable_qp(dev, 0, 1);

	dev->started = true;
	pthread_mutex_unlock(&dev->mutex);
	rte_mcfg_mem_read_unlock();

	return 0;
error:
	pthread_mutex_unlock(&dev->mutex);
	rte_mcfg_mem_read_unlock();
	/* TODO: free resource here or caller to check */
	return -1;
}

int virtio_user_stop_device(struct virtio_user_dev *dev)
{
	struct vhost_vring_state state;
	uint32_t i;
	int error = 0;

	pthread_mutex_lock(&dev->mutex);
	if (!dev->started)
		goto out;

	for (i = 0; i < dev->max_queue_pairs; ++i)
		dev->ops->enable_qp(dev, i, 0);

	/* Stop the backend. */
	for (i = 0; i < dev->max_queue_pairs * 2; ++i) {
		state.index = i;
		if (dev->ops->send_request(dev, VHOST_USER_GET_VRING_BASE,
					   &state) < 0) {
			PMD_DRV_LOG(ERR, "get_vring_base failed, index=%u\n",
				    i);
			error = -1;
			goto out;
		}
	}

	dev->started = false;
out:
	pthread_mutex_unlock(&dev->mutex);

	return error;
}

static inline void
parse_mac(struct virtio_user_dev *dev, const char *mac)
{
	struct rte_ether_addr tmp;

	if (!mac)
		return;

	if (rte_ether_unformat_addr(mac, &tmp) == 0) {
		memcpy(dev->mac_addr, &tmp, RTE_ETHER_ADDR_LEN);
		dev->mac_specified = 1;
	} else {
		/* ignore the wrong mac, use random mac */
		PMD_DRV_LOG(ERR, "wrong format of mac: %s", mac);
	}
}

static int
virtio_user_dev_init_notify(struct virtio_user_dev *dev)
{
	uint32_t i, j;
	int callfd;
	int kickfd;

	for (i = 0; i < VIRTIO_MAX_VIRTQUEUES; ++i) {
		if (i >= dev->max_queue_pairs * 2) {
			dev->kickfds[i] = -1;
			dev->callfds[i] = -1;
			continue;
		}

		/* May use invalid flag, but some backend uses kickfd and
		 * callfd as criteria to judge if dev is alive. so finally we
		 * use real event_fd.
		 */
		callfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
		if (callfd < 0) {
			PMD_DRV_LOG(ERR, "callfd error, %s", strerror(errno));
			break;
		}
		kickfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
		if (kickfd < 0) {
			close(callfd);
			PMD_DRV_LOG(ERR, "kickfd error, %s", strerror(errno));
			break;
		}
		dev->callfds[i] = callfd;
		dev->kickfds[i] = kickfd;
	}

	if (i < VIRTIO_MAX_VIRTQUEUES) {
		for (j = 0; j < i; ++j) {
			close(dev->callfds[j]);
			close(dev->kickfds[j]);
		}

		return -1;
	}

	return 0;
}

static int
virtio_user_fill_intr_handle(struct virtio_user_dev *dev)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->port_id];

	if (!eth_dev->intr_handle) {
		eth_dev->intr_handle = malloc(sizeof(*eth_dev->intr_handle));
		if (!eth_dev->intr_handle) {
			PMD_DRV_LOG(ERR, "fail to allocate intr_handle");
			return -1;
		}
		memset(eth_dev->intr_handle, 0, sizeof(*eth_dev->intr_handle));
	}

	for (i = 0; i < dev->max_queue_pairs; ++i)
		eth_dev->intr_handle->efds[i] = dev->callfds[2 * i];
	eth_dev->intr_handle->nb_efd = dev->max_queue_pairs;
	eth_dev->intr_handle->max_intr = dev->max_queue_pairs + 1;
	eth_dev->intr_handle->type = RTE_INTR_HANDLE_VDEV;
	/* For virtio vdev, no need to read counter for clean */
	eth_dev->intr_handle->efd_counter_size = 0;
	eth_dev->intr_handle->fd = -1;
	if (dev->vhostfd >= 0)
		eth_dev->intr_handle->fd = dev->vhostfd;
	else if (dev->is_server)
		eth_dev->intr_handle->fd = dev->listenfd;

	return 0;
}

static void
virtio_user_mem_event_cb(enum rte_mem_event type __rte_unused,
			 const void *addr,
			 size_t len __rte_unused,
			 void *arg)
{
	struct virtio_user_dev *dev = arg;
	struct rte_memseg_list *msl;
	uint16_t i;

	/* ignore externally allocated memory */
	msl = rte_mem_virt2memseg_list(addr);
	if (msl->external)
		return;

	pthread_mutex_lock(&dev->mutex);

	if (dev->started == false)
		goto exit;

	/* Step 1: pause the active queues */
	for (i = 0; i < dev->queue_pairs; i++)
		dev->ops->enable_qp(dev, i, 0);

	/* Step 2: update memory regions */
	dev->ops->send_request(dev, VHOST_USER_SET_MEM_TABLE, NULL);

	/* Step 3: resume the active queues */
	for (i = 0; i < dev->queue_pairs; i++)
		dev->ops->enable_qp(dev, i, 1);

exit:
	pthread_mutex_unlock(&dev->mutex);
}

static int
virtio_user_dev_setup(struct virtio_user_dev *dev)
{
	uint32_t q;

	dev->vhostfd = -1;
	dev->vhostfds = NULL;
	dev->tapfds = NULL;

	if (dev->is_server) {
		if (dev->backend_type != VIRTIO_USER_BACKEND_VHOST_USER) {
			PMD_DRV_LOG(ERR, "Server mode only supports vhost-user!");
			return -1;
		}
		dev->ops = &virtio_ops_user;
	} else {
		if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER) {
			dev->ops = &virtio_ops_user;
		} else if (dev->backend_type ==
					VIRTIO_USER_BACKEND_VHOST_KERNEL) {
			dev->ops = &virtio_ops_kernel;

			dev->vhostfds = malloc(dev->max_queue_pairs *
					       sizeof(int));
			dev->tapfds = malloc(dev->max_queue_pairs *
					     sizeof(int));
			if (!dev->vhostfds || !dev->tapfds) {
				PMD_INIT_LOG(ERR, "Failed to malloc");
				return -1;
			}

			for (q = 0; q < dev->max_queue_pairs; ++q) {
				dev->vhostfds[q] = -1;
				dev->tapfds[q] = -1;
			}
		} else if (dev->backend_type ==
				VIRTIO_USER_BACKEND_VHOST_VDPA) {
			dev->ops = &virtio_ops_vdpa;
		} else {
			PMD_DRV_LOG(ERR, "Unknown backend type");
			return -1;
		}
	}

	if (dev->ops->setup(dev) < 0)
		return -1;

	if (virtio_user_dev_init_notify(dev) < 0)
		return -1;

	if (virtio_user_fill_intr_handle(dev) < 0)
		return -1;

	return 0;
}

/* Use below macro to filter features from vhost backend */
#define VIRTIO_USER_SUPPORTED_FEATURES			\
	(1ULL << VIRTIO_NET_F_MAC		|	\
	 1ULL << VIRTIO_NET_F_STATUS		|	\
	 1ULL << VIRTIO_NET_F_MQ		|	\
	 1ULL << VIRTIO_NET_F_CTRL_MAC_ADDR	|	\
	 1ULL << VIRTIO_NET_F_CTRL_VQ		|	\
	 1ULL << VIRTIO_NET_F_CTRL_RX		|	\
	 1ULL << VIRTIO_NET_F_CTRL_VLAN		|	\
	 1ULL << VIRTIO_NET_F_CSUM		|	\
	 1ULL << VIRTIO_NET_F_HOST_TSO4		|	\
	 1ULL << VIRTIO_NET_F_HOST_TSO6		|	\
	 1ULL << VIRTIO_NET_F_MRG_RXBUF		|	\
	 1ULL << VIRTIO_RING_F_INDIRECT_DESC	|	\
	 1ULL << VIRTIO_NET_F_GUEST_CSUM	|	\
	 1ULL << VIRTIO_NET_F_GUEST_TSO4	|	\
	 1ULL << VIRTIO_NET_F_GUEST_TSO6	|	\
	 1ULL << VIRTIO_F_IN_ORDER		|	\
	 1ULL << VIRTIO_F_VERSION_1		|	\
	 1ULL << VIRTIO_F_RING_PACKED		|	\
	 1ULL << VHOST_USER_F_PROTOCOL_FEATURES)

#define VHOST_USER_SUPPORTED_PROTOCOL_FEATURES		\
	(1ULL << VHOST_USER_PROTOCOL_F_MQ |		\
	 1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK |	\
	 1ULL << VHOST_USER_PROTOCOL_F_STATUS)

#define VHOST_VDPA_SUPPORTED_PROTOCOL_FEATURES		\
	(1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2	|	\
	1ULL << VHOST_BACKEND_F_IOTLB_BATCH)
int
virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
		     int cq, int queue_size, const char *mac, char **ifname,
		     int server, int mrg_rxbuf, int in_order, int packed_vq,
		     enum virtio_user_backend_type backend_type)
{
	uint64_t protocol_features = 0;

	pthread_mutex_init(&dev->mutex, NULL);
	strlcpy(dev->path, path, PATH_MAX);
	dev->started = 0;
	dev->max_queue_pairs = queues;
	dev->queue_pairs = 1; /* mq disabled by default */
	dev->queue_size = queue_size;
	dev->is_server = server;
	dev->mac_specified = 0;
	dev->frontend_features = 0;
	dev->unsupported_features = ~VIRTIO_USER_SUPPORTED_FEATURES;
	dev->backend_type = backend_type;

	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER)
		dev->protocol_features = VHOST_USER_SUPPORTED_PROTOCOL_FEATURES;
	else if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_VDPA)
		dev->protocol_features = VHOST_VDPA_SUPPORTED_PROTOCOL_FEATURES;

	parse_mac(dev, mac);

	if (*ifname) {
		dev->ifname = *ifname;
		*ifname = NULL;
	}

	if (virtio_user_dev_setup(dev) < 0) {
		PMD_INIT_LOG(ERR, "backend set up fails");
		return -1;
	}

	if (dev->backend_type != VIRTIO_USER_BACKEND_VHOST_USER)
		dev->unsupported_features |=
			(1ULL << VHOST_USER_F_PROTOCOL_FEATURES);

	if (!dev->is_server) {
		if (dev->ops->send_request(dev, VHOST_USER_SET_OWNER,
					   NULL) < 0) {
			PMD_INIT_LOG(ERR, "set_owner fails: %s",
				     strerror(errno));
			return -1;
		}

		if (dev->ops->send_request(dev, VHOST_USER_GET_FEATURES,
					   &dev->device_features) < 0) {
			PMD_INIT_LOG(ERR, "get_features failed: %s",
				     strerror(errno));
			return -1;
		}


		if ((dev->device_features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)) ||
				(dev->backend_type == VIRTIO_USER_BACKEND_VHOST_VDPA)) {
			if (dev->ops->send_request(dev,
					VHOST_USER_GET_PROTOCOL_FEATURES,
					&protocol_features))
				return -1;

			dev->protocol_features &= protocol_features;

			if (dev->ops->send_request(dev,
					VHOST_USER_SET_PROTOCOL_FEATURES,
					&dev->protocol_features))
				return -1;

			if (!(dev->protocol_features &
					(1ULL << VHOST_USER_PROTOCOL_F_MQ)))
				dev->unsupported_features |=
					(1ull << VIRTIO_NET_F_MQ);
		}
	} else {
		/* We just pretend vhost-user can support all these features.
		 * Note that this could be problematic that if some feature is
		 * negotiated but not supported by the vhost-user which comes
		 * later.
		 */
		dev->device_features = VIRTIO_USER_SUPPORTED_FEATURES;

		/* We cannot assume VHOST_USER_PROTOCOL_F_STATUS is supported
		 * until it's negotiated
		 */
		dev->protocol_features &=
			~(1ULL << VHOST_USER_PROTOCOL_F_STATUS);
	}



	if (!mrg_rxbuf)
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_MRG_RXBUF);

	if (!in_order)
		dev->unsupported_features |= (1ull << VIRTIO_F_IN_ORDER);

	if (!packed_vq)
		dev->unsupported_features |= (1ull << VIRTIO_F_RING_PACKED);

	if (dev->mac_specified)
		dev->frontend_features |= (1ull << VIRTIO_NET_F_MAC);
	else
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_MAC);

	if (cq) {
		/* device does not really need to know anything about CQ,
		 * so if necessary, we just claim to support CQ
		 */
		dev->frontend_features |= (1ull << VIRTIO_NET_F_CTRL_VQ);
	} else {
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_CTRL_VQ);
		/* Also disable features that depend on VIRTIO_NET_F_CTRL_VQ */
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_CTRL_RX);
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_CTRL_VLAN);
		dev->unsupported_features |=
			(1ull << VIRTIO_NET_F_GUEST_ANNOUNCE);
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_MQ);
		dev->unsupported_features |=
			(1ull << VIRTIO_NET_F_CTRL_MAC_ADDR);
	}

	/* The backend will not report this feature, we add it explicitly */
	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER)
		dev->frontend_features |= (1ull << VIRTIO_NET_F_STATUS);

	/*
	 * Device features =
	 *     (frontend_features | backend_features) & ~unsupported_features;
	 */
	dev->device_features |= dev->frontend_features;
	dev->device_features &= ~dev->unsupported_features;

	if (rte_mem_event_callback_register(VIRTIO_USER_MEM_EVENT_CLB_NAME,
				virtio_user_mem_event_cb, dev)) {
		if (rte_errno != ENOTSUP) {
			PMD_INIT_LOG(ERR, "Failed to register mem event"
					" callback\n");
			return -1;
		}
	}

	return 0;
}

void
virtio_user_dev_uninit(struct virtio_user_dev *dev)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->port_id];

	if (eth_dev->intr_handle) {
		free(eth_dev->intr_handle);
		eth_dev->intr_handle = NULL;
	}

	virtio_user_stop_device(dev);

	rte_mem_event_callback_unregister(VIRTIO_USER_MEM_EVENT_CLB_NAME, dev);

	for (i = 0; i < dev->max_queue_pairs * 2; ++i) {
		close(dev->callfds[i]);
		close(dev->kickfds[i]);
	}

	if (dev->vhostfd >= 0)
		close(dev->vhostfd);

	if (dev->is_server && dev->listenfd >= 0) {
		close(dev->listenfd);
		dev->listenfd = -1;
	}

	if (dev->vhostfds) {
		for (i = 0; i < dev->max_queue_pairs; ++i) {
			close(dev->vhostfds[i]);
			if (dev->tapfds[i] >= 0)
				close(dev->tapfds[i]);
		}
		free(dev->vhostfds);
		free(dev->tapfds);
	}

	free(dev->ifname);

	if (dev->is_server)
		unlink(dev->path);
}

uint8_t
virtio_user_handle_mq(struct virtio_user_dev *dev, uint16_t q_pairs)
{
	uint16_t i;
	uint8_t ret = 0;

	if (q_pairs > dev->max_queue_pairs) {
		PMD_INIT_LOG(ERR, "multi-q config %u, but only %u supported",
			     q_pairs, dev->max_queue_pairs);
		return -1;
	}

	/* Server mode can't enable queue pairs if vhostfd is invalid,
	 * always return 0 in this case.
	 */
	if (!dev->is_server || dev->vhostfd >= 0) {
		for (i = 0; i < q_pairs; ++i)
			ret |= dev->ops->enable_qp(dev, i, 1);
		for (i = q_pairs; i < dev->max_queue_pairs; ++i)
			ret |= dev->ops->enable_qp(dev, i, 0);
	}
	dev->queue_pairs = q_pairs;

	return ret;
}

static uint32_t
virtio_user_handle_ctrl_msg(struct virtio_user_dev *dev, struct vring *vring,
			    uint16_t idx_hdr)
{
	struct virtio_net_ctrl_hdr *hdr;
	virtio_net_ctrl_ack status = ~0;
	uint16_t i, idx_data, idx_status;
	uint32_t n_descs = 0;

	/* locate desc for header, data, and status */
	idx_data = vring->desc[idx_hdr].next;
	n_descs++;

	i = idx_data;
	while (vring->desc[i].flags == VRING_DESC_F_NEXT) {
		i = vring->desc[i].next;
		n_descs++;
	}

	/* locate desc for status */
	idx_status = i;
	n_descs++;

	hdr = (void *)(uintptr_t)vring->desc[idx_hdr].addr;
	if (hdr->class == VIRTIO_NET_CTRL_MQ &&
	    hdr->cmd == VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET) {
		uint16_t queues;

		queues = *(uint16_t *)(uintptr_t)vring->desc[idx_data].addr;
		status = virtio_user_handle_mq(dev, queues);
	} else if (hdr->class == VIRTIO_NET_CTRL_RX  ||
		   hdr->class == VIRTIO_NET_CTRL_MAC ||
		   hdr->class == VIRTIO_NET_CTRL_VLAN) {
		status = 0;
	}

	/* Update status */
	*(virtio_net_ctrl_ack *)(uintptr_t)vring->desc[idx_status].addr = status;

	return n_descs;
}

static inline int
desc_is_avail(struct vring_packed_desc *desc, bool wrap_counter)
{
	uint16_t flags = __atomic_load_n(&desc->flags, __ATOMIC_ACQUIRE);

	return wrap_counter == !!(flags & VRING_PACKED_DESC_F_AVAIL) &&
		wrap_counter != !!(flags & VRING_PACKED_DESC_F_USED);
}

static uint32_t
virtio_user_handle_ctrl_msg_packed(struct virtio_user_dev *dev,
				   struct vring_packed *vring,
				   uint16_t idx_hdr)
{
	struct virtio_net_ctrl_hdr *hdr;
	virtio_net_ctrl_ack status = ~0;
	uint16_t idx_data, idx_status;
	/* initialize to one, header is first */
	uint32_t n_descs = 1;

	/* locate desc for header, data, and status */
	idx_data = idx_hdr + 1;
	if (idx_data >= dev->queue_size)
		idx_data -= dev->queue_size;

	n_descs++;

	idx_status = idx_data;
	while (vring->desc[idx_status].flags & VRING_DESC_F_NEXT) {
		idx_status++;
		if (idx_status >= dev->queue_size)
			idx_status -= dev->queue_size;
		n_descs++;
	}

	hdr = (void *)(uintptr_t)vring->desc[idx_hdr].addr;
	if (hdr->class == VIRTIO_NET_CTRL_MQ &&
	    hdr->cmd == VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET) {
		uint16_t queues;

		queues = *(uint16_t *)(uintptr_t)
				vring->desc[idx_data].addr;
		status = virtio_user_handle_mq(dev, queues);
	} else if (hdr->class == VIRTIO_NET_CTRL_RX  ||
		   hdr->class == VIRTIO_NET_CTRL_MAC ||
		   hdr->class == VIRTIO_NET_CTRL_VLAN) {
		status = 0;
	}

	/* Update status */
	*(virtio_net_ctrl_ack *)(uintptr_t)
		vring->desc[idx_status].addr = status;

	/* Update used descriptor */
	vring->desc[idx_hdr].id = vring->desc[idx_status].id;
	vring->desc[idx_hdr].len = sizeof(status);

	return n_descs;
}

void
virtio_user_handle_cq_packed(struct virtio_user_dev *dev, uint16_t queue_idx)
{
	struct virtio_user_queue *vq = &dev->packed_queues[queue_idx];
	struct vring_packed *vring = &dev->packed_vrings[queue_idx];
	uint16_t n_descs, flags;

	/* Perform a load-acquire barrier in desc_is_avail to
	 * enforce the ordering between desc flags and desc
	 * content.
	 */
	while (desc_is_avail(&vring->desc[vq->used_idx],
			     vq->used_wrap_counter)) {

		n_descs = virtio_user_handle_ctrl_msg_packed(dev, vring,
				vq->used_idx);

		flags = VRING_DESC_F_WRITE;
		if (vq->used_wrap_counter)
			flags |= VRING_PACKED_DESC_F_AVAIL_USED;

		__atomic_store_n(&vring->desc[vq->used_idx].flags, flags,
				 __ATOMIC_RELEASE);

		vq->used_idx += n_descs;
		if (vq->used_idx >= dev->queue_size) {
			vq->used_idx -= dev->queue_size;
			vq->used_wrap_counter ^= 1;
		}
	}
}

void
virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx)
{
	uint16_t avail_idx, desc_idx;
	struct vring_used_elem *uep;
	uint32_t n_descs;
	struct vring *vring = &dev->vrings[queue_idx];

	/* Consume avail ring, using used ring idx as first one */
	while (__atomic_load_n(&vring->used->idx, __ATOMIC_RELAXED)
	       != vring->avail->idx) {
		avail_idx = __atomic_load_n(&vring->used->idx, __ATOMIC_RELAXED)
			    & (vring->num - 1);
		desc_idx = vring->avail->ring[avail_idx];

		n_descs = virtio_user_handle_ctrl_msg(dev, vring, desc_idx);

		/* Update used ring */
		uep = &vring->used->ring[avail_idx];
		uep->id = desc_idx;
		uep->len = n_descs;

		__atomic_add_fetch(&vring->used->idx, 1, __ATOMIC_RELAXED);
	}
}

int
virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status)
{
	int ret;
	uint64_t arg = status;

	pthread_mutex_lock(&dev->mutex);
	dev->status = status;
	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER)
		ret = dev->ops->send_request(dev,
				VHOST_USER_SET_STATUS, &arg);
	else if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_VDPA)
		ret = dev->ops->send_request(dev,
				VHOST_USER_SET_STATUS, &status);
	else
		ret = -ENOTSUP;

	if (ret && ret != -ENOTSUP) {
		PMD_INIT_LOG(ERR, "VHOST_USER_SET_STATUS failed (%d): %s", ret,
			     strerror(errno));
	}

	pthread_mutex_unlock(&dev->mutex);
	return ret;
}

int
virtio_user_dev_update_status(struct virtio_user_dev *dev)
{
	uint64_t ret;
	uint8_t status;
	int err;

	pthread_mutex_lock(&dev->mutex);
	if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_USER) {
		err = dev->ops->send_request(dev, VHOST_USER_GET_STATUS, &ret);
		if (!err && ret > UINT8_MAX) {
			PMD_INIT_LOG(ERR, "Invalid VHOST_USER_GET_STATUS "
					"response 0x%" PRIx64 "\n", ret);
			err = -1;
			goto error;
		}

		status = ret;
	} else if (dev->backend_type == VIRTIO_USER_BACKEND_VHOST_VDPA) {
		err = dev->ops->send_request(dev, VHOST_USER_GET_STATUS,
				&status);
	} else {
		err = -ENOTSUP;
	}

	if (!err) {
		dev->status = status;
		PMD_INIT_LOG(DEBUG, "Updated Device Status(0x%08x):\n"
			"\t-RESET: %u\n"
			"\t-ACKNOWLEDGE: %u\n"
			"\t-DRIVER: %u\n"
			"\t-DRIVER_OK: %u\n"
			"\t-FEATURES_OK: %u\n"
			"\t-DEVICE_NEED_RESET: %u\n"
			"\t-FAILED: %u\n",
			dev->status,
			(dev->status == VIRTIO_CONFIG_STATUS_RESET),
			!!(dev->status & VIRTIO_CONFIG_STATUS_ACK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_FEATURES_OK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DEV_NEED_RESET),
			!!(dev->status & VIRTIO_CONFIG_STATUS_FAILED));
	} else if (err != -ENOTSUP) {
		PMD_INIT_LOG(ERR, "VHOST_USER_GET_STATUS failed (%d): %s", err,
			     strerror(errno));
	}

error:
	pthread_mutex_unlock(&dev->mutex);
	return err;
}
