/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>


#include <linux/vduse.h>
#include <linux/virtio_net.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <rte_common.h>

#include "vduse.h"
#include "vhost.h"

#define VHOST_VDUSE_API_VERSION 0
#define VDUSE_CTRL_PATH "/dev/vduse/control"

#define VDUSE_NET_SUPPORTED_FEATURES ((1ULL << VIRTIO_NET_F_MRG_RXBUF) | \
				(1ULL << VIRTIO_F_ANY_LAYOUT) | \
				(1ULL << VIRTIO_F_VERSION_1)   | \
				(1ULL << VIRTIO_NET_F_GSO) | \
				(1ULL << VIRTIO_NET_F_HOST_TSO4) | \
				(1ULL << VIRTIO_NET_F_HOST_TSO6) | \
				(1ULL << VIRTIO_NET_F_HOST_UFO) | \
				(1ULL << VIRTIO_NET_F_HOST_ECN) | \
				(1ULL << VIRTIO_NET_F_CSUM)    | \
				(1ULL << VIRTIO_NET_F_GUEST_CSUM) | \
				(1ULL << VIRTIO_NET_F_GUEST_TSO4) | \
				(1ULL << VIRTIO_NET_F_GUEST_TSO6) | \
				(1ULL << VIRTIO_NET_F_GUEST_UFO) | \
				(1ULL << VIRTIO_NET_F_GUEST_ECN) | \
				(1ULL << VIRTIO_RING_F_INDIRECT_DESC) | \
				(1ULL << VIRTIO_F_IN_ORDER) | \
				(1ULL << VIRTIO_F_IOMMU_PLATFORM))

static struct vhost_backend_ops vduse_backend_ops = {
};

int
vduse_device_create(const char *path)
{
	int control_fd, dev_fd, vid, ret;
	uint32_t i;
	struct virtio_net *dev;
	uint64_t ver = VHOST_VDUSE_API_VERSION;
	struct vduse_dev_config *dev_config = NULL;
	const char *name = path + strlen("/dev/vduse/");

	control_fd = open(VDUSE_CTRL_PATH, O_RDWR);
	if (control_fd < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to open %s: %s\n",
				VDUSE_CTRL_PATH, strerror(errno));
		return -1;
	}

	if (ioctl(control_fd, VDUSE_SET_API_VERSION, &ver)) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to set API version: %" PRIu64 ": %s\n",
				ver, strerror(errno));
		ret = -1;
		goto out_ctrl_close;
	}

	dev_config = malloc(offsetof(struct vduse_dev_config, config));
	if (!dev_config) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to allocate VDUSE config\n");
		ret = -1;
		goto out_ctrl_close;
	}

	memset(dev_config, 0, sizeof(struct vduse_dev_config));

	strncpy(dev_config->name, name, VDUSE_NAME_MAX - 1);
	dev_config->device_id = VIRTIO_ID_NET;
	dev_config->vendor_id = 0;
	dev_config->features = VDUSE_NET_SUPPORTED_FEATURES;
	dev_config->vq_num = 2;
	dev_config->vq_align = sysconf(_SC_PAGE_SIZE);
	dev_config->config_size = 0;

	ret = ioctl(control_fd, VDUSE_CREATE_DEV, dev_config);
	if (ret < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to create VDUSE device: %s\n",
				strerror(errno));
		goto out_free;
	}

	dev_fd = open(path, O_RDWR);
	if (dev_fd < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to open device %s: %s\n",
				path, strerror(errno));
		ret = -1;
		goto out_dev_close;
	}

	ret = fcntl(dev_fd, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to set chardev as non-blocking: %s\n",
				strerror(errno));
		goto out_dev_close;
	}

	vid = vhost_new_device(&vduse_backend_ops);
	if (vid < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to create new Vhost device\n");
		ret = -1;
		goto out_dev_close;
	}

	dev = get_device(vid);
	if (!dev) {
		ret = -1;
		goto out_dev_close;
	}

	strncpy(dev->ifname, path, IF_NAME_SZ - 1);
	dev->vduse_ctrl_fd = control_fd;
	dev->vduse_dev_fd = dev_fd;
	vhost_setup_virtio_net(dev->vid, true, true, true, true);

	for (i = 0; i < 2; i++) {
		struct vduse_vq_config vq_cfg = { 0 };

		ret = alloc_vring_queue(dev, i);
		if (ret) {
			VHOST_LOG_CONFIG(name, ERR, "Failed to alloc vring %d metadata\n", i);
			goto out_dev_destroy;
		}

		vq_cfg.index = i;
		vq_cfg.max_size = 1024;

		ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP, &vq_cfg);
		if (ret) {
			VHOST_LOG_CONFIG(name, ERR, "Failed to set-up VQ %d\n", i);
			goto out_dev_destroy;
		}
	}

	free(dev_config);

	return 0;

out_dev_destroy:
	vhost_destroy_device(vid);
out_dev_close:
	if (dev_fd >= 0)
		close(dev_fd);
	ioctl(control_fd, VDUSE_DESTROY_DEV, name);
out_free:
	free(dev_config);
out_ctrl_close:
	close(control_fd);

	return ret;
}

int
vduse_device_destroy(const char *path)
{
	const char *name = path + strlen("/dev/vduse/");
	struct virtio_net *dev;
	int vid, ret;

	for (vid = 0; vid < RTE_MAX_VHOST_DEVICE; vid++) {
		dev = vhost_devices[vid];

		if (dev == NULL)
			continue;

		if (!strcmp(path, dev->ifname))
			break;
	}

	if (vid == RTE_MAX_VHOST_DEVICE)
		return -1;

	if (dev->vduse_dev_fd >= 0) {
		close(dev->vduse_dev_fd);
		dev->vduse_dev_fd = -1;
	}

	if (dev->vduse_ctrl_fd >= 0) {
		ret = ioctl(dev->vduse_ctrl_fd, VDUSE_DESTROY_DEV, name);
		if (ret)
			VHOST_LOG_CONFIG(name, ERR, "Failed to destroy VDUSE device: %s\n",
					strerror(errno));
		close(dev->vduse_ctrl_fd);
		dev->vduse_ctrl_fd = -1;
	}

	vhost_destroy_device(vid);

	return 0;
}
