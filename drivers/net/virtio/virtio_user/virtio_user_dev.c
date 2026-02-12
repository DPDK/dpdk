/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rte_alarm.h>
#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_io.h>

#include "vhost.h"
#include "virtio.h"
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
virtio_user_uninit_notify_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	if (dev->kickfds[queue_sel] >= 0) {
		close(dev->kickfds[queue_sel]);
		dev->kickfds[queue_sel] = -1;
	}

	if (dev->callfds[queue_sel] >= 0) {
		close(dev->callfds[queue_sel]);
		dev->callfds[queue_sel] = -1;
	}

	return 0;
}

static int
virtio_user_init_notify_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	/* May use invalid flag, but some backend uses kickfd and
	 * callfd as criteria to judge if dev is alive. so finally we
	 * use real event_fd.
	 */
	dev->callfds[queue_sel] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (dev->callfds[queue_sel] < 0) {
		PMD_DRV_LOG(ERR, "(%s) Failed to setup callfd for queue %u: %s",
				dev->path, queue_sel, strerror(errno));
		return -1;
	}
	dev->kickfds[queue_sel] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (dev->kickfds[queue_sel] < 0) {
		PMD_DRV_LOG(ERR, "(%s) Failed to setup kickfd for queue %u: %s",
				dev->path, queue_sel, strerror(errno));
		return -1;
	}

	return 0;
}

static int
virtio_user_destroy_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	struct vhost_vring_state state;
	int ret;

	state.index = queue_sel;
	ret = dev->ops->get_vring_base(dev, &state);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "(%s) Failed to destroy queue %u", dev->path, queue_sel);
		return -1;
	}

	return 0;
}

static int
virtio_user_create_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	/* Of all per virtqueue MSGs, make sure VHOST_SET_VRING_CALL come
	 * firstly because vhost depends on this msg to allocate virtqueue
	 * pair.
	 */
	struct vhost_vring_file file;
	int ret;

	file.index = queue_sel;
	file.fd = dev->callfds[queue_sel];
	ret = dev->ops->set_vring_call(dev, &file);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to create queue %u", dev->path, queue_sel);
		return -1;
	}

	return 0;
}

static int
virtio_user_kick_queue(struct virtio_user_dev *dev, uint32_t queue_sel)
{
	int ret;
	struct vhost_vring_file file;
	struct vhost_vring_state state;
	struct vring *vring = &dev->vrings.split[queue_sel];
	struct vring_packed *pq_vring = &dev->vrings.packed[queue_sel];
	uint64_t desc_addr, desc_iova_addr, avail_addr, used_addr;
	struct vhost_vring_addr addr = {
		.index = queue_sel,
		.log_guest_addr = 0,
		.flags = 0, /* disable log */
	};

	if (queue_sel == dev->max_queue_pairs * 2) {
		if (!dev->scvq) {
			PMD_INIT_LOG(ERR, "(%s) Shadow control queue expected but missing",
					dev->path);
			goto err;
		}

		/* Use shadow control queue information */
		vring = &dev->scvq->vq_split.ring;
		pq_vring = &dev->scvq->vq_packed.ring;
	}

	if (dev->features & (1ULL << VIRTIO_F_RING_PACKED)) {
		desc_iova_addr = pq_vring->desc_iova;
		desc_addr = (uint64_t)(uintptr_t)pq_vring->desc;
		avail_addr = (uint64_t)(uintptr_t)pq_vring->driver;
		used_addr = (uint64_t)(uintptr_t)pq_vring->device;
	} else {
		desc_iova_addr = vring->desc_iova;
		desc_addr = (uint64_t)(uintptr_t)vring->desc;
		avail_addr = (uint64_t)(uintptr_t)vring->avail;
		used_addr = (uint64_t)(uintptr_t)vring->used;
	}

	addr.desc_user_addr = desc_iova_addr;
	addr.avail_user_addr = (desc_iova_addr - desc_addr) + avail_addr;
	addr.used_user_addr = (desc_iova_addr - desc_addr) + used_addr;

	state.index = queue_sel;
	state.num = vring->num;
	ret = dev->ops->set_vring_num(dev, &state);
	if (ret < 0)
		goto err;

	state.index = queue_sel;
	state.num = 0; /* no reservation */
	if (dev->features & (1ULL << VIRTIO_F_RING_PACKED))
		state.num |= (1 << 15);
	ret = dev->ops->set_vring_base(dev, &state);
	if (ret < 0)
		goto err;

	ret = dev->ops->set_vring_addr(dev, &addr);
	if (ret < 0)
		goto err;

	/* Of all per virtqueue MSGs, make sure VHOST_USER_SET_VRING_KICK comes
	 * lastly because vhost depends on this msg to judge if
	 * virtio is ready.
	 */
	file.index = queue_sel;
	file.fd = dev->kickfds[queue_sel];
	ret = dev->ops->set_vring_kick(dev, &file);
	if (ret < 0)
		goto err;

	return 0;
err:
	PMD_INIT_LOG(ERR, "(%s) Failed to kick queue %u", dev->path, queue_sel);

	return -1;
}

static int
virtio_user_foreach_queue(struct virtio_user_dev *dev,
			int (*fn)(struct virtio_user_dev *, uint32_t))
{
	uint32_t i, nr_vq;

	nr_vq = dev->max_queue_pairs * 2;
	if (dev->hw_cvq)
		nr_vq++;

	for (i = 0; i < nr_vq; i++)
		if (fn(dev, i) < 0)
			return -1;

	return 0;
}

int
virtio_user_dev_set_features(struct virtio_user_dev *dev)
{
	uint64_t features;
	int ret = -1;

	pthread_mutex_lock(&dev->mutex);

	/* Step 0: tell vhost to create queues */
	if (virtio_user_foreach_queue(dev, virtio_user_create_queue) < 0)
		goto error;

	features = dev->features;

	/* Strip VIRTIO_NET_F_MAC, as MAC address is handled in vdev init */
	features &= ~(1ull << VIRTIO_NET_F_MAC);
	/* Strip VIRTIO_NET_F_CTRL_VQ if the devices does not really support control VQ */
	if (!dev->hw_cvq)
		features &= ~(1ull << VIRTIO_NET_F_CTRL_VQ);
	features &= ~(1ull << VIRTIO_NET_F_STATUS);
	ret = dev->ops->set_features(dev, features);
	if (ret < 0)
		goto error;
	PMD_DRV_LOG(INFO, "(%s) set features: 0x%" PRIx64, dev->path, features);
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

	/* Step 2: share memory regions */
	ret = dev->ops->set_memory_table(dev);
	if (ret < 0)
		goto error;

	/* Step 3: kick queues */
	ret = virtio_user_foreach_queue(dev, virtio_user_kick_queue);
	if (ret < 0)
		goto error;

	/* Step 4: enable queues
	 * we enable the 1st queue pair by default.
	 */
	ret = dev->ops->enable_qp(dev, 0, 1);
	if (ret < 0)
		goto error;

	if (dev->scvq) {
		ret = dev->ops->cvq_enable(dev, 1);
		if (ret < 0)
			goto error;
	}

	dev->started = true;

	pthread_mutex_unlock(&dev->mutex);
	rte_mcfg_mem_read_unlock();

	return 0;
error:
	pthread_mutex_unlock(&dev->mutex);
	rte_mcfg_mem_read_unlock();

	PMD_INIT_LOG(ERR, "(%s) Failed to start device", dev->path);

	/* TODO: free resource here or caller to check */
	return -1;
}

int virtio_user_stop_device(struct virtio_user_dev *dev)
{
	uint32_t i;
	int ret;

	pthread_mutex_lock(&dev->mutex);
	if (!dev->started)
		goto out;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		ret = dev->ops->enable_qp(dev, i, 0);
		if (ret < 0)
			goto err;
	}

	if (dev->scvq) {
		ret = dev->ops->cvq_enable(dev, 0);
		if (ret < 0)
			goto err;
	}

	/* Stop the backend. */
	if (virtio_user_foreach_queue(dev, virtio_user_destroy_queue) < 0)
		goto err;

	dev->started = false;

out:
	pthread_mutex_unlock(&dev->mutex);

	return 0;
err:
	pthread_mutex_unlock(&dev->mutex);

	PMD_INIT_LOG(ERR, "(%s) Failed to stop device", dev->path);

	return -1;
}

static int
virtio_user_dev_init_max_queue_pairs(struct virtio_user_dev *dev, uint32_t user_max_qp)
{
	int ret;

	if (!(dev->device_features & (1ULL << VIRTIO_NET_F_MQ))) {
		dev->max_queue_pairs = 1;
		return 0;
	}

	if (!dev->ops->get_config) {
		dev->max_queue_pairs = user_max_qp;
		return 0;
	}

	ret = dev->ops->get_config(dev, (uint8_t *)&dev->max_queue_pairs,
			offsetof(struct virtio_net_config, max_virtqueue_pairs),
			sizeof(uint16_t));
	if (ret) {
		/*
		 * We need to know the max queue pair from the device so that
		 * the control queue gets the right index.
		 */
		dev->max_queue_pairs = 1;
		PMD_DRV_LOG(ERR, "(%s) Failed to get max queue pairs from device", dev->path);

		return ret;
	}

	return 0;
}

int
virtio_user_dev_get_rss_config(struct virtio_user_dev *dev, void *dst, size_t offset, int length)
{
	int ret = 0;

	if (!(dev->device_features & (1ULL << VIRTIO_NET_F_RSS)))
		return -ENOTSUP;

	if (!dev->ops->get_config)
		return -ENOTSUP;

	ret = dev->ops->get_config(dev, dst, offset, length);
	if (ret)
		PMD_DRV_LOG(ERR, "(%s) Failed to get rss config in device", dev->path);

	return ret;
}

int
virtio_user_dev_set_mac(struct virtio_user_dev *dev)
{
	int ret = 0;

	if (!(dev->device_features & (1ULL << VIRTIO_NET_F_MAC)))
		return -ENOTSUP;

	if (!dev->ops->set_config)
		return -ENOTSUP;

	ret = dev->ops->set_config(dev, dev->mac_addr,
			offsetof(struct virtio_net_config, mac),
			RTE_ETHER_ADDR_LEN);
	if (ret)
		PMD_DRV_LOG(ERR, "(%s) Failed to set MAC address in device", dev->path);

	return ret;
}

int
virtio_user_dev_get_mac(struct virtio_user_dev *dev)
{
	int ret = 0;

	if (!(dev->device_features & (1ULL << VIRTIO_NET_F_MAC)))
		return -ENOTSUP;

	if (!dev->ops->get_config)
		return -ENOTSUP;

	ret = dev->ops->get_config(dev, dev->mac_addr,
			offsetof(struct virtio_net_config, mac),
			RTE_ETHER_ADDR_LEN);
	if (ret)
		PMD_DRV_LOG(ERR, "(%s) Failed to get MAC address from device", dev->path);

	return ret;
}

static void
virtio_user_dev_init_mac(struct virtio_user_dev *dev, const char *mac)
{
	struct rte_ether_addr cmdline_mac;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	if (mac && rte_ether_unformat_addr(mac, &cmdline_mac) == 0) {
		/*
		 * MAC address was passed from command-line, try to store
		 * it in the device if it supports it. Otherwise try to use
		 * the device one.
		 */
		memcpy(dev->mac_addr, &cmdline_mac, RTE_ETHER_ADDR_LEN);
		dev->mac_specified = 1;

		/* Setting MAC may fail, continue to get the device one in this case */
		virtio_user_dev_set_mac(dev);
		ret = virtio_user_dev_get_mac(dev);
		if (ret == -ENOTSUP)
			goto out;

		if (memcmp(&cmdline_mac, dev->mac_addr, RTE_ETHER_ADDR_LEN))
			PMD_DRV_LOG(INFO, "(%s) Device MAC update failed", dev->path);
	} else {
		ret = virtio_user_dev_get_mac(dev);
		if (ret) {
			PMD_DRV_LOG(ERR, "(%s) No valid MAC in devargs or device, use random",
					dev->path);
			return;
		}

		dev->mac_specified = 1;
	}
out:
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE,
			(struct rte_ether_addr *)dev->mac_addr);
	PMD_DRV_LOG(INFO, "(%s) MAC %s specified", dev->path, buf);
}

static int
virtio_user_dev_init_notify(struct virtio_user_dev *dev)
{

	if (virtio_user_foreach_queue(dev, virtio_user_init_notify_queue) < 0)
		goto err;

	if (dev->device_features & (1ULL << VIRTIO_F_NOTIFICATION_DATA))
		if (dev->ops->map_notification_area &&
				dev->ops->map_notification_area(dev))
			goto err;

	return 0;
err:
	virtio_user_foreach_queue(dev, virtio_user_uninit_notify_queue);

	return -1;
}

static void
virtio_user_dev_uninit_notify(struct virtio_user_dev *dev)
{
	virtio_user_foreach_queue(dev, virtio_user_uninit_notify_queue);

	if (dev->ops->unmap_notification_area && dev->notify_area)
		dev->ops->unmap_notification_area(dev);
}

static int
virtio_user_fill_intr_handle(struct virtio_user_dev *dev)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->hw.port_id];

	if (eth_dev->intr_handle == NULL) {
		eth_dev->intr_handle =
			rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
		if (eth_dev->intr_handle == NULL) {
			PMD_DRV_LOG(ERR, "(%s) failed to allocate intr_handle", dev->path);
			return -1;
		}
	}

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		if (rte_intr_efds_index_set(eth_dev->intr_handle, i,
				dev->callfds[2 * i + VTNET_SQ_RQ_QUEUE_IDX]))
			return -rte_errno;
	}

	if (rte_intr_nb_efd_set(eth_dev->intr_handle, dev->max_queue_pairs))
		return -rte_errno;

	if (rte_intr_max_intr_set(eth_dev->intr_handle,
			dev->max_queue_pairs + 1))
		return -rte_errno;

	if (rte_intr_type_set(eth_dev->intr_handle, RTE_INTR_HANDLE_VDEV))
		return -rte_errno;

	/* For virtio vdev, no need to read counter for clean */
	if (rte_intr_efd_counter_size_set(eth_dev->intr_handle, 0))
		return -rte_errno;

	if (rte_intr_fd_set(eth_dev->intr_handle, dev->ops->get_intr_fd(dev)))
		return -rte_errno;

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
	int ret = 0;

	/* ignore externally allocated memory */
	msl = rte_mem_virt2memseg_list(addr);
	if (msl->external)
		return;

	pthread_mutex_lock(&dev->mutex);

	if (dev->started == false)
		goto exit;

	/* Step 1: pause the active queues */
	for (i = 0; i < dev->queue_pairs; i++) {
		ret = dev->ops->enable_qp(dev, i, 0);
		if (ret < 0)
			goto exit;
	}

	/* Step 2: update memory regions */
	ret = dev->ops->set_memory_table(dev);
	if (ret < 0)
		goto exit;

	/* Step 3: resume the active queues */
	for (i = 0; i < dev->queue_pairs; i++) {
		ret = dev->ops->enable_qp(dev, i, 1);
		if (ret < 0)
			goto exit;
	}

exit:
	pthread_mutex_unlock(&dev->mutex);

	if (ret < 0)
		PMD_DRV_LOG(ERR, "(%s) Failed to update memory table", dev->path);
}

static int
virtio_user_dev_setup(struct virtio_user_dev *dev)
{
	if (dev->is_server) {
		if (dev->backend_type != VIRTIO_USER_BACKEND_VHOST_USER) {
			PMD_DRV_LOG(ERR, "Server mode only supports vhost-user!");
			return -1;
		}
	}

	switch (dev->backend_type) {
	case VIRTIO_USER_BACKEND_VHOST_USER:
		dev->ops = &virtio_ops_user;
		break;
	case VIRTIO_USER_BACKEND_VHOST_KERNEL:
		dev->ops = &virtio_ops_kernel;
		break;
	case VIRTIO_USER_BACKEND_VHOST_VDPA:
		dev->ops = &virtio_ops_vdpa;
		break;
	default:
		PMD_DRV_LOG(ERR, "(%s) Unknown backend type", dev->path);
		return -1;
	}

	if (dev->ops->setup(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to setup backend", dev->path);
		return -1;
	}

	return 0;
}

static int
virtio_user_alloc_vrings(struct virtio_user_dev *dev)
{
	int i, size, nr_vrings;
	bool packed_ring = !!(dev->device_features & (1ull << VIRTIO_F_RING_PACKED));

	nr_vrings = dev->max_queue_pairs * 2;
	if (dev->frontend_features & (1ull << VIRTIO_NET_F_CTRL_VQ))
		nr_vrings++;

	dev->callfds = rte_zmalloc("virtio_user_dev", nr_vrings * sizeof(*dev->callfds), 0);
	if (!dev->callfds) {
		PMD_INIT_LOG(ERR, "(%s) Failed to alloc callfds", dev->path);
		return -1;
	}

	dev->kickfds = rte_zmalloc("virtio_user_dev", nr_vrings * sizeof(*dev->kickfds), 0);
	if (!dev->kickfds) {
		PMD_INIT_LOG(ERR, "(%s) Failed to alloc kickfds", dev->path);
		goto free_callfds;
	}

	for (i = 0; i < nr_vrings; i++) {
		dev->callfds[i] = -1;
		dev->kickfds[i] = -1;
	}

	if (packed_ring)
		size = sizeof(*dev->vrings.packed);
	else
		size = sizeof(*dev->vrings.split);
	dev->vrings.ptr = rte_zmalloc("virtio_user_dev", nr_vrings * size, 0);
	if (!dev->vrings.ptr) {
		PMD_INIT_LOG(ERR, "(%s) Failed to alloc vrings metadata", dev->path);
		goto free_kickfds;
	}

	if (packed_ring) {
		dev->packed_queues = rte_zmalloc("virtio_user_dev",
				nr_vrings * sizeof(*dev->packed_queues), 0);
		if (!dev->packed_queues) {
			PMD_INIT_LOG(ERR, "(%s) Failed to alloc packed queues metadata",
					dev->path);
			goto free_vrings;
		}
	}

	dev->qp_enabled = rte_zmalloc("virtio_user_dev",
			dev->max_queue_pairs * sizeof(*dev->qp_enabled), 0);
	if (!dev->qp_enabled) {
		PMD_INIT_LOG(ERR, "(%s) Failed to alloc QP enable states", dev->path);
		goto free_packed_queues;
	}

	return 0;

free_packed_queues:
	rte_free(dev->packed_queues);
	dev->packed_queues = NULL;
free_vrings:
	rte_free(dev->vrings.ptr);
	dev->vrings.ptr = NULL;
free_kickfds:
	rte_free(dev->kickfds);
	dev->kickfds = NULL;
free_callfds:
	rte_free(dev->callfds);
	dev->callfds = NULL;

	return -1;
}

static void
virtio_user_free_vrings(struct virtio_user_dev *dev)
{
	rte_free(dev->qp_enabled);
	dev->qp_enabled = NULL;
	rte_free(dev->packed_queues);
	dev->packed_queues = NULL;
	rte_free(dev->vrings.ptr);
	dev->vrings.ptr = NULL;
	rte_free(dev->kickfds);
	dev->kickfds = NULL;
	rte_free(dev->callfds);
	dev->callfds = NULL;
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
	 1ULL << VIRTIO_F_NOTIFICATION_DATA	|	\
	 1ULL << VIRTIO_F_ORDER_PLATFORM        |       \
	 1ULL << VIRTIO_NET_F_HASH_REPORT       |       \
	 1ULL << VIRTIO_NET_F_RSS)

int
virtio_user_dev_init(struct virtio_user_dev *dev, char *path, uint16_t queues,
		     int cq, int queue_size, const char *mac, char **ifname,
		     int server, int mrg_rxbuf, int in_order, int packed_vq,
		     enum virtio_user_backend_type backend_type)
{
	uint64_t backend_features;

	pthread_mutex_init(&dev->mutex, NULL);
	strlcpy(dev->path, path, PATH_MAX);

	dev->started = 0;
	dev->queue_pairs = 1; /* mq disabled by default */
	dev->max_queue_pairs = queues; /* initialize to user requested value for kernel backend */
	dev->queue_size = queue_size;
	dev->is_server = server;
	dev->mac_specified = 0;
	dev->frontend_features = 0;
	dev->unsupported_features = 0;
	dev->backend_type = backend_type;
	dev->ifname = *ifname;

	if (virtio_user_dev_setup(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) backend set up fails", dev->path);
		return -1;
	}

	if (dev->ops->set_owner(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to set backend owner", dev->path);
		goto destroy;
	}

	if (dev->ops->get_backend_features(&backend_features) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get backend features", dev->path);
		goto destroy;
	}

	dev->unsupported_features = ~(VIRTIO_USER_SUPPORTED_FEATURES | backend_features);

	if (dev->ops->get_features(dev, &dev->device_features) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get device features", dev->path);
		goto destroy;
	}

	virtio_user_dev_init_mac(dev, mac);

	if (virtio_user_dev_init_max_queue_pairs(dev, queues))
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_MQ);

	if (dev->max_queue_pairs > 1 || dev->hw_cvq)
		cq = 1;

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
		/* Except for vDPA, the device does not really need to know
		 * anything about CQ, so if necessary, we just claim to support
		 * control queue.
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

	dev->frontend_features &= ~dev->unsupported_features;
	dev->device_features &= ~dev->unsupported_features;

	if (virtio_user_alloc_vrings(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to allocate vring metadata", dev->path);
		goto destroy;
	}

	if (virtio_user_dev_init_notify(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to init notifiers", dev->path);
		goto free_vrings;
	}

	if (virtio_user_fill_intr_handle(dev) < 0) {
		PMD_INIT_LOG(ERR, "(%s) Failed to init interrupt handler", dev->path);
		goto notify_uninit;
	}

	if (rte_mem_event_callback_register(VIRTIO_USER_MEM_EVENT_CLB_NAME,
				virtio_user_mem_event_cb, dev)) {
		if (rte_errno != ENOTSUP) {
			PMD_INIT_LOG(ERR, "(%s) Failed to register mem event callback",
					dev->path);
			goto notify_uninit;
		}
	}

	*ifname = NULL;
	return 0;

notify_uninit:
	virtio_user_dev_uninit_notify(dev);
free_vrings:
	virtio_user_free_vrings(dev);
destroy:
	dev->ops->destroy(dev);

	return -1;
}

void
virtio_user_dev_uninit(struct virtio_user_dev *dev)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->hw.port_id];

	rte_intr_instance_free(eth_dev->intr_handle);
	eth_dev->intr_handle = NULL;

	virtio_user_stop_device(dev);

	rte_mem_event_callback_unregister(VIRTIO_USER_MEM_EVENT_CLB_NAME, dev);

	virtio_user_dev_uninit_notify(dev);

	virtio_user_free_vrings(dev);

	free(dev->ifname);

	if (dev->is_server)
		unlink(dev->path);

	dev->ops->destroy(dev);
}

static uint8_t
virtio_user_handle_mq(struct virtio_user_dev *dev, uint16_t q_pairs)
{
	uint16_t i;
	uint8_t ret = 0;

	if (q_pairs > dev->max_queue_pairs) {
		PMD_INIT_LOG(ERR, "(%s) multi-q config %u, but only %u supported",
			     dev->path, q_pairs, dev->max_queue_pairs);
		return -1;
	}

	for (i = 0; i < q_pairs; ++i)
		ret |= dev->ops->enable_qp(dev, i, 1);
	for (i = q_pairs; i < dev->max_queue_pairs; ++i)
		ret |= dev->ops->enable_qp(dev, i, 0);

	dev->queue_pairs = q_pairs;

	return ret;
}

#define CVQ_MAX_DATA_DESCS 32

static inline void *
virtio_user_iova2virt(struct virtio_user_dev *dev, rte_iova_t iova)
{
	if (rte_eal_iova_mode() == RTE_IOVA_VA || dev->hw.use_va)
		return (void *)(uintptr_t)iova;
	else
		return rte_mem_iova2virt(iova);
}

static uint32_t
virtio_user_handle_ctrl_msg_split(struct virtio_user_dev *dev, struct vring *vring,
			    uint16_t idx_hdr)
{
	struct virtio_net_ctrl_hdr *hdr;
	virtio_net_ctrl_ack status = ~0;
	uint16_t i, idx_data, idx_status;
	uint32_t n_descs = 0;
	int dlen[CVQ_MAX_DATA_DESCS], nb_dlen = 0;

	/* locate desc for header, data, and status */
	idx_data = vring->desc[idx_hdr].next;
	n_descs++;

	i = idx_data;
	while (vring->desc[i].flags == VRING_DESC_F_NEXT) {
		dlen[nb_dlen++] = vring->desc[i].len;
		i = vring->desc[i].next;
		n_descs++;
	}

	/* locate desc for status */
	idx_status = i;
	n_descs++;

	hdr = virtio_user_iova2virt(dev, vring->desc[idx_hdr].addr);
	if (hdr->class == VIRTIO_NET_CTRL_MQ &&
	    hdr->cmd == VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET) {
		uint16_t queues, *addr;

		addr = virtio_user_iova2virt(dev, vring->desc[idx_data].addr);
		queues = *addr;
		status = virtio_user_handle_mq(dev, queues);
	} else if (hdr->class == VIRTIO_NET_CTRL_MQ && hdr->cmd == VIRTIO_NET_CTRL_MQ_RSS_CONFIG) {
		struct virtio_net_ctrl_rss *rss;

		rss = virtio_user_iova2virt(dev, vring->desc[idx_data].addr);
		status = virtio_user_handle_mq(dev, rss->max_tx_vq);
	} else if (hdr->class == VIRTIO_NET_CTRL_RX  ||
		   hdr->class == VIRTIO_NET_CTRL_MAC ||
		   hdr->class == VIRTIO_NET_CTRL_VLAN) {
		status = 0;
	}

	if (!status && dev->scvq)
		status = virtio_send_command(&dev->scvq->cq,
				(struct virtio_pmd_ctrl *)hdr, dlen, nb_dlen);

	/* Update status */
	*(virtio_net_ctrl_ack *)virtio_user_iova2virt(dev, vring->desc[idx_status].addr) = status;

	return n_descs;
}

static inline int
desc_is_avail(struct vring_packed_desc *desc, bool wrap_counter)
{
	uint16_t flags = rte_atomic_load_explicit(&desc->flags, rte_memory_order_acquire);

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
	int dlen[CVQ_MAX_DATA_DESCS], nb_dlen = 0;

	/* locate desc for header, data, and status */
	idx_data = idx_hdr + 1;
	if (idx_data >= dev->queue_size)
		idx_data -= dev->queue_size;

	n_descs++;

	idx_status = idx_data;
	while (vring->desc[idx_status].flags & VRING_DESC_F_NEXT) {
		dlen[nb_dlen++] = vring->desc[idx_status].len;
		idx_status++;
		if (idx_status >= dev->queue_size)
			idx_status -= dev->queue_size;
		n_descs++;
	}

	hdr = virtio_user_iova2virt(dev, vring->desc[idx_hdr].addr);
	if (hdr->class == VIRTIO_NET_CTRL_MQ &&
	    hdr->cmd == VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET) {
		uint16_t queues, *addr;

		addr = virtio_user_iova2virt(dev, vring->desc[idx_data].addr);
		queues = *addr;
		status = virtio_user_handle_mq(dev, queues);
	} else if (hdr->class == VIRTIO_NET_CTRL_MQ && hdr->cmd == VIRTIO_NET_CTRL_MQ_RSS_CONFIG) {
		struct virtio_net_ctrl_rss *rss;

		rss = virtio_user_iova2virt(dev, vring->desc[idx_data].addr);
		status = virtio_user_handle_mq(dev, rss->max_tx_vq);
	} else if (hdr->class == VIRTIO_NET_CTRL_RX  ||
		   hdr->class == VIRTIO_NET_CTRL_MAC ||
		   hdr->class == VIRTIO_NET_CTRL_VLAN) {
		status = 0;
	}

	if (!status && dev->scvq)
		status = virtio_send_command(&dev->scvq->cq,
				(struct virtio_pmd_ctrl *)hdr, dlen, nb_dlen);

	/* Update status */
	*(virtio_net_ctrl_ack *)virtio_user_iova2virt(dev, vring->desc[idx_status].addr) = status;

	/* Update used descriptor */
	vring->desc[idx_hdr].id = vring->desc[idx_status].id;
	vring->desc[idx_hdr].len = sizeof(status);

	return n_descs;
}

static void
virtio_user_handle_cq_packed(struct virtio_user_dev *dev, uint16_t queue_idx)
{
	struct virtio_user_queue *vq = &dev->packed_queues[queue_idx];
	struct vring_packed *vring = &dev->vrings.packed[queue_idx];
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

		rte_atomic_store_explicit(&vring->desc[vq->used_idx].flags, flags,
				 rte_memory_order_release);

		vq->used_idx += n_descs;
		if (vq->used_idx >= dev->queue_size) {
			vq->used_idx -= dev->queue_size;
			vq->used_wrap_counter ^= 1;
		}
	}
}

static void
virtio_user_handle_cq_split(struct virtio_user_dev *dev, uint16_t queue_idx)
{
	uint16_t avail_idx, desc_idx;
	struct vring_used_elem *uep;
	uint32_t n_descs;
	struct vring *vring = &dev->vrings.split[queue_idx];

	/* Consume avail ring, using used ring idx as first one */
	while (rte_atomic_load_explicit(&vring->used->idx, rte_memory_order_relaxed)
	       != vring->avail->idx) {
		avail_idx = rte_atomic_load_explicit(&vring->used->idx, rte_memory_order_relaxed)
			    & (vring->num - 1);
		desc_idx = vring->avail->ring[avail_idx];

		n_descs = virtio_user_handle_ctrl_msg_split(dev, vring, desc_idx);

		/* Update used ring */
		uep = &vring->used->ring[avail_idx];
		uep->id = desc_idx;
		uep->len = n_descs;

		rte_atomic_fetch_add_explicit(&vring->used->idx, 1, rte_memory_order_relaxed);
	}
}

void
virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx)
{
	if (virtio_with_packed_queue(&dev->hw))
		virtio_user_handle_cq_packed(dev, queue_idx);
	else
		virtio_user_handle_cq_split(dev, queue_idx);
}

static void
virtio_user_control_queue_notify(struct virtqueue *vq, void *cookie)
{
	struct virtio_user_dev *dev = cookie;
	uint64_t notify_data = 1;

	if (!dev->notify_area) {
		if (write(dev->kickfds[vq->vq_queue_index], &notify_data, sizeof(notify_data)) < 0)
			PMD_DRV_LOG(ERR, "failed to kick backend: %s",
				    strerror(errno));
		return;
	} else if (!virtio_with_feature(&dev->hw, VIRTIO_F_NOTIFICATION_DATA)) {
		rte_write16(vq->vq_queue_index, vq->notify_addr);
		return;
	}

	if (virtio_with_packed_queue(&dev->hw)) {
		/* Bit[0:15]: vq queue index
		 * Bit[16:30]: avail index
		 * Bit[31]: avail wrap counter
		 */
		notify_data = ((uint32_t)(!!(vq->vq_packed.cached_flags &
				VRING_PACKED_DESC_F_AVAIL)) << 31) |
				((uint32_t)vq->vq_avail_idx << 16) |
				vq->vq_queue_index;
	} else {
		/* Bit[0:15]: vq queue index
		 * Bit[16:31]: avail index
		 */
		notify_data = ((uint32_t)vq->vq_avail_idx << 16) |
				vq->vq_queue_index;
	}
	rte_write32(notify_data, vq->notify_addr);
}

int
virtio_user_dev_create_shadow_cvq(struct virtio_user_dev *dev, struct virtqueue *vq)
{
	char name[VIRTQUEUE_MAX_NAME_SZ];
	struct virtqueue *scvq;

	snprintf(name, sizeof(name), "port%d_shadow_cvq", vq->hw->port_id);
	scvq = virtqueue_alloc(&dev->hw, vq->vq_queue_index, vq->vq_nentries,
			VTNET_CQ, SOCKET_ID_ANY, name);
	if (!scvq) {
		PMD_INIT_LOG(ERR, "(%s) Failed to alloc shadow control vq", dev->path);
		return -ENOMEM;
	}

	scvq->cq.notify_queue = &virtio_user_control_queue_notify;
	scvq->cq.notify_cookie = dev;
	scvq->notify_addr = vq->notify_addr;
	dev->scvq = scvq;

	return 0;
}

void
virtio_user_dev_destroy_shadow_cvq(struct virtio_user_dev *dev)
{
	if (!dev->scvq)
		return;

	virtqueue_free(dev->scvq);
	dev->scvq = NULL;
}

int
virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status)
{
	int ret;

	pthread_mutex_lock(&dev->mutex);
	dev->status = status;
	ret = dev->ops->set_status(dev, status);
	if (ret && ret != -ENOTSUP)
		PMD_INIT_LOG(ERR, "(%s) Failed to set backend status", dev->path);

	pthread_mutex_unlock(&dev->mutex);
	return ret;
}

int
virtio_user_dev_update_status(struct virtio_user_dev *dev)
{
	int ret;
	uint8_t status;

	pthread_mutex_lock(&dev->mutex);

	ret = dev->ops->get_status(dev, &status);
	if (!ret) {
		dev->status = status;
		PMD_INIT_LOG(DEBUG, "Updated Device Status(0x%08x):",
			dev->status);
		PMD_INIT_LOG(DEBUG, "\t-RESET: %u",
			(dev->status == VIRTIO_CONFIG_STATUS_RESET));
		PMD_INIT_LOG(DEBUG, "\t-ACKNOWLEDGE: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_ACK));
		PMD_INIT_LOG(DEBUG, "\t-DRIVER: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER));
		PMD_INIT_LOG(DEBUG, "\t-DRIVER_OK: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK));
		PMD_INIT_LOG(DEBUG, "\t-FEATURES_OK: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_FEATURES_OK));
		PMD_INIT_LOG(DEBUG, "\t-DEVICE_NEED_RESET: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_DEV_NEED_RESET));
		PMD_INIT_LOG(DEBUG, "\t-FAILED: %u",
			!!(dev->status & VIRTIO_CONFIG_STATUS_FAILED));
	} else if (ret != -ENOTSUP) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get backend status", dev->path);
	}

	pthread_mutex_unlock(&dev->mutex);
	return ret;
}

int
virtio_user_dev_update_link_state(struct virtio_user_dev *dev)
{
	if (dev->ops->update_link_state)
		return dev->ops->update_link_state(dev);

	return 0;
}

static void
virtio_user_dev_reset_queues_packed(struct rte_eth_dev *eth_dev)
{
	struct virtio_user_dev *dev = eth_dev->data->dev_private;
	struct virtio_hw *hw = &dev->hw;
	struct virtnet_rx *rxvq;
	struct virtnet_tx *txvq;
	uint16_t i;

	/* Add lock to avoid queue contention. */
	rte_spinlock_lock(&hw->state_lock);
	hw->started = 0;

	/*
	 * Waiting for datapath to complete before resetting queues.
	 * 1 ms should be enough for the ongoing Tx/Rx function to finish.
	 */
	rte_delay_ms(1);

	/* Vring reset for each Tx queue and Rx queue. */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxvq = eth_dev->data->rx_queues[i];
		virtqueue_rxvq_reset_packed(virtnet_rxq_to_vq(rxvq));
		virtio_dev_rx_queue_setup_finish(eth_dev, i);
	}

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		txvq = eth_dev->data->tx_queues[i];
		virtqueue_txvq_reset_packed(virtnet_txq_to_vq(txvq));
	}

	hw->started = 1;
	rte_spinlock_unlock(&hw->state_lock);
}

void
virtio_user_dev_delayed_disconnect_handler(void *param)
{
	struct virtio_user_dev *dev = param;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->hw.port_id];

	if (rte_intr_disable(eth_dev->intr_handle) < 0) {
		PMD_DRV_LOG(ERR, "interrupt disable failed");
		return;
	}
	PMD_DRV_LOG(DEBUG, "Unregistering intr fd: %d",
		    rte_intr_fd_get(eth_dev->intr_handle));
	if (rte_intr_callback_unregister(eth_dev->intr_handle,
					 virtio_interrupt_handler,
					 eth_dev) != 1)
		PMD_DRV_LOG(ERR, "interrupt unregister failed");

	if (dev->is_server) {
		if (dev->ops->server_disconnect)
			dev->ops->server_disconnect(dev);

		rte_intr_fd_set(eth_dev->intr_handle,
			dev->ops->get_intr_fd(dev));

		PMD_DRV_LOG(DEBUG, "Registering intr fd: %d",
			    rte_intr_fd_get(eth_dev->intr_handle));

		if (rte_intr_callback_register(eth_dev->intr_handle,
					       virtio_interrupt_handler,
					       eth_dev))
			PMD_DRV_LOG(ERR, "interrupt register failed");

		if (rte_intr_enable(eth_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return;
		}
	}
}

static void
virtio_user_dev_delayed_intr_reconfig_handler(void *param)
{
	struct virtio_user_dev *dev = param;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->hw.port_id];

	PMD_DRV_LOG(DEBUG, "Unregistering intr fd: %d",
		    rte_intr_fd_get(eth_dev->intr_handle));

	if (rte_intr_callback_unregister(eth_dev->intr_handle,
					 virtio_interrupt_handler,
					 eth_dev) != 1)
		PMD_DRV_LOG(ERR, "interrupt unregister failed");

	rte_intr_fd_set(eth_dev->intr_handle, dev->ops->get_intr_fd(dev));

	PMD_DRV_LOG(DEBUG, "Registering intr fd: %d",
		    rte_intr_fd_get(eth_dev->intr_handle));

	if (rte_intr_callback_register(eth_dev->intr_handle,
				       virtio_interrupt_handler, eth_dev))
		PMD_DRV_LOG(ERR, "interrupt register failed");

	if (rte_intr_enable(eth_dev->intr_handle) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");
}

int
virtio_user_dev_server_reconnect(struct virtio_user_dev *dev)
{
	int ret, old_status;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->hw.port_id];
	struct virtio_hw *hw = &dev->hw;

	if (!dev->ops->server_reconnect) {
		PMD_DRV_LOG(ERR, "(%s) Missing server reconnect callback", dev->path);
		return -1;
	}

	if (dev->ops->server_reconnect(dev)) {
		PMD_DRV_LOG(ERR, "(%s) Reconnect callback call failed", dev->path);
		return -1;
	}

	old_status = dev->status;

	virtio_reset(hw);

	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);

	if (dev->ops->get_features(dev, &dev->device_features) < 0) {
		PMD_INIT_LOG(ERR, "get_features failed: %s",
			     strerror(errno));
		return -1;
	}

	/* unmask vhost-user unsupported features */
	dev->device_features &= ~(dev->unsupported_features);

	dev->features &= (dev->device_features | dev->frontend_features);

	/* For packed ring, resetting queues is required in reconnection. */
	if (virtio_with_packed_queue(hw) &&
	   (old_status & VIRTIO_CONFIG_STATUS_DRIVER_OK)) {
		PMD_INIT_LOG(NOTICE, "Packets on the fly will be dropped"
				" when packed ring reconnecting.");
		virtio_user_dev_reset_queues_packed(eth_dev);
	}

	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_FEATURES_OK);

	/* Start the device */
	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
	if (!dev->started)
		return -1;

	if (dev->queue_pairs > 1) {
		ret = virtio_user_handle_mq(dev, dev->queue_pairs);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Fails to enable multi-queue pairs!");
			return -1;
		}
	}
	if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC) {
		if (rte_intr_disable(eth_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt disable failed");
			return -1;
		}
		/*
		 * This function can be called from the interrupt handler, so
		 * we can't unregister interrupt handler here.  Setting
		 * alarm to do that later.
		 */
		rte_eal_alarm_set(1,
			virtio_user_dev_delayed_intr_reconfig_handler,
			(void *)dev);
	}
	PMD_INIT_LOG(NOTICE, "server mode virtio-user reconnection succeeds!");
	return 0;
}
