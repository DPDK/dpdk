/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell.
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
#include <pthread.h>

#include <rte_alarm.h>
#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_io.h>

#include "vhost.h"
#include "virtio_logs.h"
#include "cryptodev_pmd.h"
#include "virtio_crypto.h"
#include "virtio_cvq.h"
#include "virtio_user_dev.h"
#include "virtqueue.h"

#define VIRTIO_USER_MEM_EVENT_CLB_NAME "virtio_user_mem_event_clb"

const char * const crypto_virtio_user_backend_strings[] = {
	[VIRTIO_USER_BACKEND_UNKNOWN] = "VIRTIO_USER_BACKEND_UNKNOWN",
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
	uint64_t desc_addr, avail_addr, used_addr;
	struct vhost_vring_addr addr = {
		.index = queue_sel,
		.log_guest_addr = 0,
		.flags = 0, /* disable log */
	};

	if (queue_sel == dev->max_queue_pairs) {
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
		desc_addr = pq_vring->desc_iova;
		avail_addr = desc_addr + pq_vring->num * sizeof(struct vring_packed_desc);
		used_addr =  RTE_ALIGN_CEIL(avail_addr + sizeof(struct vring_packed_desc_event),
						VIRTIO_VRING_ALIGN);

		addr.desc_user_addr = desc_addr;
		addr.avail_user_addr = avail_addr;
		addr.used_user_addr = used_addr;
	} else {
		desc_addr = vring->desc_iova;
		avail_addr = desc_addr + vring->num * sizeof(struct vring_desc);
		used_addr = RTE_ALIGN_CEIL((uintptr_t)(&vring->avail->ring[vring->num]),
					VIRTIO_VRING_ALIGN);

		addr.desc_user_addr = desc_addr;
		addr.avail_user_addr = avail_addr;
		addr.used_user_addr = used_addr;
	}

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

	nr_vq = dev->max_queue_pairs;

	for (i = 0; i < nr_vq; i++)
		if (fn(dev, i) < 0)
			return -1;

	return 0;
}

int
crypto_virtio_user_dev_set_features(struct virtio_user_dev *dev)
{
	uint64_t features;
	int ret = -1;

	pthread_mutex_lock(&dev->mutex);

	/* Step 0: tell vhost to create queues */
	if (virtio_user_foreach_queue(dev, virtio_user_create_queue) < 0)
		goto error;

	features = dev->features;

	ret = dev->ops->set_features(dev, features);
	if (ret < 0)
		goto error;
	PMD_DRV_LOG(INFO, "(%s) set features: 0x%" PRIx64, dev->path, features);
error:
	pthread_mutex_unlock(&dev->mutex);

	return ret;
}

int
crypto_virtio_user_start_device(struct virtio_user_dev *dev)
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

	ret = virtio_user_kick_queue(dev, dev->max_queue_pairs);
	if (ret < 0)
		goto error;

	/* Step 4: enable queues */
	for (int i = 0; i < dev->max_queue_pairs; i++) {
		ret = dev->ops->enable_qp(dev, i, 1);
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

int crypto_virtio_user_stop_device(struct virtio_user_dev *dev)
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

	if (!dev->ops->get_config) {
		dev->max_queue_pairs = user_max_qp;
		return 0;
	}

	ret = dev->ops->get_config(dev, (uint8_t *)&dev->max_queue_pairs,
			offsetof(struct virtio_crypto_config, max_dataqueues),
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

static int
virtio_user_dev_init_cipher_services(struct virtio_user_dev *dev)
{
	struct virtio_crypto_config config;
	int ret;

	dev->crypto_services = RTE_BIT32(VIRTIO_CRYPTO_SERVICE_CIPHER);
	dev->cipher_algo = 0;
	dev->auth_algo = 0;
	dev->akcipher_algo = 0;

	if (!dev->ops->get_config)
		return 0;

	ret = dev->ops->get_config(dev, (uint8_t *)&config,	0, sizeof(config));
	if (ret) {
		PMD_DRV_LOG(ERR, "(%s) Failed to get crypto config from device", dev->path);
		return ret;
	}

	dev->crypto_services = config.crypto_services;
	dev->cipher_algo = ((uint64_t)config.cipher_algo_h << 32) |
						config.cipher_algo_l;
	dev->hash_algo = config.hash_algo;
	dev->auth_algo = ((uint64_t)config.mac_algo_h << 32) |
						config.mac_algo_l;
	dev->aead_algo = config.aead_algo;
	dev->akcipher_algo = config.akcipher_algo;
	return 0;
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
	case VIRTIO_USER_BACKEND_VHOST_VDPA:
		dev->ops = &virtio_crypto_ops_vdpa;
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

	nr_vrings = dev->max_queue_pairs + 1;

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
			nr_vrings * sizeof(*dev->qp_enabled), 0);
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

#define VIRTIO_USER_SUPPORTED_FEATURES   \
	(1ULL << VIRTIO_CRYPTO_SERVICE_CIPHER     | \
	 1ULL << VIRTIO_CRYPTO_SERVICE_HASH       | \
	 1ULL << VIRTIO_CRYPTO_SERVICE_AKCIPHER   | \
	 1ULL << VIRTIO_F_VERSION_1               | \
	 1ULL << VIRTIO_F_IN_ORDER                | \
	 1ULL << VIRTIO_F_RING_PACKED             | \
	 1ULL << VIRTIO_F_NOTIFICATION_DATA       | \
	 1ULL << VIRTIO_F_ORDER_PLATFORM)

int
crypto_virtio_user_dev_init(struct virtio_user_dev *dev, char *path, uint16_t queues,
			int queue_size, int server)
{
	uint64_t backend_features;

	pthread_mutex_init(&dev->mutex, NULL);
	strlcpy(dev->path, path, PATH_MAX);

	dev->started = 0;
	dev->queue_pairs = 1; /* mq disabled by default */
	dev->max_queue_pairs = queues; /* initialize to user requested value for kernel backend */
	dev->queue_size = queue_size;
	dev->is_server = server;
	dev->frontend_features = 0;
	dev->unsupported_features = 0;
	dev->backend_type = VIRTIO_USER_BACKEND_VHOST_VDPA;
	dev->hw.modern = 1;

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

	if (virtio_user_dev_init_max_queue_pairs(dev, queues)) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get max queue pairs", dev->path);
		goto destroy;
	}

	if (virtio_user_dev_init_cipher_services(dev)) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get cipher services", dev->path);
		goto destroy;
	}

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

	if (rte_mem_event_callback_register(VIRTIO_USER_MEM_EVENT_CLB_NAME,
				virtio_user_mem_event_cb, dev)) {
		if (rte_errno != ENOTSUP) {
			PMD_INIT_LOG(ERR, "(%s) Failed to register mem event callback",
					dev->path);
			goto notify_uninit;
		}
	}

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
crypto_virtio_user_dev_uninit(struct virtio_user_dev *dev)
{
	crypto_virtio_user_stop_device(dev);

	rte_mem_event_callback_unregister(VIRTIO_USER_MEM_EVENT_CLB_NAME, dev);

	virtio_user_dev_uninit_notify(dev);

	virtio_user_free_vrings(dev);

	if (dev->is_server)
		unlink(dev->path);

	dev->ops->destroy(dev);
}

#define CVQ_MAX_DATA_DESCS 32

int
crypto_virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status)
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
crypto_virtio_user_dev_update_status(struct virtio_user_dev *dev)
{
	int ret;
	uint8_t status;

	pthread_mutex_lock(&dev->mutex);

	ret = dev->ops->get_status(dev, &status);
	if (!ret) {
		dev->status = status;
		PMD_INIT_LOG(DEBUG, "Updated Device Status(0x%08x):"
			"\t-RESET: %u "
			"\t-ACKNOWLEDGE: %u "
			"\t-DRIVER: %u "
			"\t-DRIVER_OK: %u "
			"\t-FEATURES_OK: %u "
			"\t-DEVICE_NEED_RESET: %u "
			"\t-FAILED: %u",
			dev->status,
			(dev->status == VIRTIO_CONFIG_STATUS_RESET),
			!!(dev->status & VIRTIO_CONFIG_STATUS_ACK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_FEATURES_OK),
			!!(dev->status & VIRTIO_CONFIG_STATUS_DEV_NEED_RESET),
			!!(dev->status & VIRTIO_CONFIG_STATUS_FAILED));
	} else if (ret != -ENOTSUP) {
		PMD_INIT_LOG(ERR, "(%s) Failed to get backend status", dev->path);
	}

	pthread_mutex_unlock(&dev->mutex);
	return ret;
}

int
crypto_virtio_user_dev_update_link_state(struct virtio_user_dev *dev)
{
	if (dev->ops->update_link_state)
		return dev->ops->update_link_state(dev);

	return 0;
}
