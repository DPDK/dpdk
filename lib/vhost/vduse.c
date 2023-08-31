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
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_thread.h>

#include "fd_man.h"
#include "iotlb.h"
#include "vduse.h"
#include "vhost.h"
#include "virtio_net_ctrl.h"

#define VHOST_VDUSE_API_VERSION 0
#define VDUSE_CTRL_PATH "/dev/vduse/control"

struct vduse {
	struct fdset fdset;
};

static struct vduse vduse = {
	.fdset = {
		.fd = { [0 ... MAX_FDS - 1] = {-1, NULL, NULL, NULL, 0} },
		.fd_mutex = PTHREAD_MUTEX_INITIALIZER,
		.fd_pooling_mutex = PTHREAD_MUTEX_INITIALIZER,
		.num = 0
	},
};

static bool vduse_events_thread;

static const char * const vduse_reqs_str[] = {
	"VDUSE_GET_VQ_STATE",
	"VDUSE_SET_STATUS",
	"VDUSE_UPDATE_IOTLB",
};

#define vduse_req_id_to_str(id) \
	(id < RTE_DIM(vduse_reqs_str) ? \
	vduse_reqs_str[id] : "Unknown")

static int
vduse_inject_irq(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	return ioctl(dev->vduse_dev_fd, VDUSE_VQ_INJECT_IRQ, &vq->index);
}

static void
vduse_iotlb_remove_notify(uint64_t addr, uint64_t offset, uint64_t size)
{
	munmap((void *)(uintptr_t)addr, offset + size);
}

static int
vduse_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm __rte_unused)
{
	struct vduse_iotlb_entry entry;
	uint64_t size, page_size;
	struct stat stat;
	void *mmap_addr;
	int fd, ret;

	entry.start = iova;
	entry.last = iova + 1;

	ret = ioctl(dev->vduse_dev_fd, VDUSE_IOTLB_GET_FD, &entry);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to get IOTLB entry for 0x%" PRIx64 "\n",
				iova);
		return -1;
	}

	fd = ret;

	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "New IOTLB entry:\n");
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "\tIOVA: %" PRIx64 " - %" PRIx64 "\n",
			(uint64_t)entry.start, (uint64_t)entry.last);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "\toffset: %" PRIx64 "\n", (uint64_t)entry.offset);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "\tfd: %d\n", fd);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "\tperm: %x\n", entry.perm);

	size = entry.last - entry.start + 1;
	mmap_addr = mmap(0, size + entry.offset, entry.perm, MAP_SHARED, fd, 0);
	if (!mmap_addr) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
				"Failed to mmap IOTLB entry for 0x%" PRIx64 "\n", iova);
		ret = -1;
		goto close_fd;
	}

	ret = fstat(fd, &stat);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to get page size.\n");
		munmap(mmap_addr, entry.offset + size);
		goto close_fd;
	}
	page_size = (uint64_t)stat.st_blksize;

	vhost_user_iotlb_cache_insert(dev, entry.start, (uint64_t)(uintptr_t)mmap_addr,
		entry.offset, size, page_size, entry.perm);

	ret = 0;
close_fd:
	close(fd);

	return ret;
}

static struct vhost_backend_ops vduse_backend_ops = {
	.iotlb_miss = vduse_iotlb_miss,
	.iotlb_remove_notify = vduse_iotlb_remove_notify,
	.inject_irq = vduse_inject_irq,
};

static void
vduse_control_queue_event(int fd, void *arg, int *remove __rte_unused)
{
	struct virtio_net *dev = arg;
	uint64_t buf;
	int ret;

	ret = read(fd, &buf, sizeof(buf));
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to read control queue event: %s\n",
				strerror(errno));
		return;
	}

	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "Control queue kicked\n");
	if (virtio_net_ctrl_handle(dev))
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to handle ctrl request\n");
}

static void
vduse_vring_setup(struct virtio_net *dev, unsigned int index)
{
	struct vhost_virtqueue *vq = dev->virtqueue[index];
	struct vhost_vring_addr *ra = &vq->ring_addrs;
	struct vduse_vq_info vq_info;
	struct vduse_vq_eventfd vq_efd;
	int ret;

	vq_info.index = index;
	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_GET_INFO, &vq_info);
	if (ret) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to get VQ %u info: %s\n",
				index, strerror(errno));
		return;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO, "VQ %u info:\n", index);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tnum: %u\n", vq_info.num);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tdesc_addr: %llx\n",
			(unsigned long long)vq_info.desc_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tdriver_addr: %llx\n",
			(unsigned long long)vq_info.driver_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tdevice_addr: %llx\n",
			(unsigned long long)vq_info.device_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tavail_idx: %u\n", vq_info.split.avail_index);
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tready: %u\n", vq_info.ready);

	vq->last_avail_idx = vq_info.split.avail_index;
	vq->size = vq_info.num;
	vq->ready = true;
	vq->enabled = vq_info.ready;
	ra->desc_user_addr = vq_info.desc_addr;
	ra->avail_user_addr = vq_info.driver_addr;
	ra->used_user_addr = vq_info.device_addr;

	vq->kickfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (vq->kickfd < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to init kickfd for VQ %u: %s\n",
				index, strerror(errno));
		vq->kickfd = VIRTIO_INVALID_EVENTFD;
		return;
	}
	VHOST_LOG_CONFIG(dev->ifname, INFO, "\tkick fd: %d\n", vq->kickfd);

	vq->shadow_used_split = rte_malloc_socket(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE, 0);
	vq->batch_copy_elems = rte_malloc_socket(NULL,
				vq->size * sizeof(struct batch_copy_elem),
				RTE_CACHE_LINE_SIZE, 0);

	vhost_user_iotlb_rd_lock(vq);
	if (vring_translate(dev, vq))
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to translate vring %d addresses\n",
				index);

	if (vhost_enable_guest_notification(dev, vq, 0))
		VHOST_LOG_CONFIG(dev->ifname, ERR,
				"Failed to disable guest notifications on vring %d\n",
				index);
	vhost_user_iotlb_rd_unlock(vq);

	vq_efd.index = index;
	vq_efd.fd = vq->kickfd;

	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
	if (ret) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to setup kickfd for VQ %u: %s\n",
				index, strerror(errno));
		close(vq->kickfd);
		vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
		return;
	}

	if (vq == dev->cvq) {
		ret = fdset_add(&vduse.fdset, vq->kickfd, vduse_control_queue_event, NULL, dev);
		if (ret) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
					"Failed to setup kickfd handler for VQ %u: %s\n",
					index, strerror(errno));
			vq_efd.fd = VDUSE_EVENTFD_DEASSIGN;
			ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
			close(vq->kickfd);
			vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
		}
		fdset_pipe_notify(&vduse.fdset);
		vhost_enable_guest_notification(dev, vq, 1);
		VHOST_LOG_CONFIG(dev->ifname, INFO, "Ctrl queue event handler installed\n");
	}
}

static void
vduse_vring_cleanup(struct virtio_net *dev, unsigned int index)
{
	struct vhost_virtqueue *vq = dev->virtqueue[index];
	struct vduse_vq_eventfd vq_efd;
	int ret;

	if (vq == dev->cvq && vq->kickfd >= 0) {
		fdset_del(&vduse.fdset, vq->kickfd);
		fdset_pipe_notify(&vduse.fdset);
	}

	vq_efd.index = index;
	vq_efd.fd = VDUSE_EVENTFD_DEASSIGN;

	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
	if (ret)
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to cleanup kickfd for VQ %u: %s\n",
				index, strerror(errno));

	close(vq->kickfd);
	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;

	vring_invalidate(dev, vq);

	rte_free(vq->batch_copy_elems);
	vq->batch_copy_elems = NULL;

	rte_free(vq->shadow_used_split);
	vq->shadow_used_split = NULL;

	vq->enabled = false;
	vq->ready = false;
	vq->size = 0;
	vq->last_used_idx = 0;
	vq->last_avail_idx = 0;
}

static void
vduse_device_start(struct virtio_net *dev)
{
	unsigned int i, ret;

	VHOST_LOG_CONFIG(dev->ifname, INFO, "Starting device...\n");

	dev->notify_ops = vhost_driver_callback_get(dev->ifname);
	if (!dev->notify_ops) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
				"Failed to get callback ops for driver\n");
		return;
	}

	ret = ioctl(dev->vduse_dev_fd, VDUSE_DEV_GET_FEATURES, &dev->features);
	if (ret) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to get features: %s\n",
				strerror(errno));
		return;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO, "Negotiated Virtio features: 0x%" PRIx64 "\n",
		dev->features);

	if (dev->features &
		((1ULL << VIRTIO_NET_F_MRG_RXBUF) |
		 (1ULL << VIRTIO_F_VERSION_1) |
		 (1ULL << VIRTIO_F_RING_PACKED))) {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr);
	}

	for (i = 0; i < dev->nr_vring; i++)
		vduse_vring_setup(dev, i);

	dev->flags |= VIRTIO_DEV_READY;

	if (dev->notify_ops->new_device(dev->vid) == 0)
		dev->flags |= VIRTIO_DEV_RUNNING;

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq == dev->cvq)
			continue;

		if (dev->notify_ops->vring_state_changed)
			dev->notify_ops->vring_state_changed(dev->vid, i, vq->enabled);
	}
}

static void
vduse_device_stop(struct virtio_net *dev)
{
	unsigned int i;

	VHOST_LOG_CONFIG(dev->ifname, INFO, "Stopping device...\n");

	vhost_destroy_device_notify(dev);

	dev->flags &= ~VIRTIO_DEV_READY;

	for (i = 0; i < dev->nr_vring; i++)
		vduse_vring_cleanup(dev, i);

	vhost_user_iotlb_flush_all(dev);
}

static void
vduse_events_handler(int fd, void *arg, int *remove __rte_unused)
{
	struct virtio_net *dev = arg;
	struct vduse_dev_request req;
	struct vduse_dev_response resp;
	struct vhost_virtqueue *vq;
	uint8_t old_status = dev->status;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = read(fd, &req, sizeof(req));
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to read request: %s\n",
				strerror(errno));
		return;
	} else if (ret < (int)sizeof(req)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Incomplete to read request %d\n", ret);
		return;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO, "New request: %s (%u)\n",
			vduse_req_id_to_str(req.type), req.type);

	switch (req.type) {
	case VDUSE_GET_VQ_STATE:
		vq = dev->virtqueue[req.vq_state.index];
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\tvq index: %u, avail_index: %u\n",
				req.vq_state.index, vq->last_avail_idx);
		resp.vq_state.split.avail_index = vq->last_avail_idx;
		resp.result = VDUSE_REQ_RESULT_OK;
		break;
	case VDUSE_SET_STATUS:
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\tnew status: 0x%08x\n",
				req.s.status);
		old_status = dev->status;
		dev->status = req.s.status;
		resp.result = VDUSE_REQ_RESULT_OK;
		break;
	case VDUSE_UPDATE_IOTLB:
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\tIOVA range: %" PRIx64 " - %" PRIx64 "\n",
				(uint64_t)req.iova.start, (uint64_t)req.iova.last);
		vhost_user_iotlb_cache_remove(dev, req.iova.start,
				req.iova.last - req.iova.start + 1);
		resp.result = VDUSE_REQ_RESULT_OK;
		break;
	default:
		resp.result = VDUSE_REQ_RESULT_FAILED;
		break;
	}

	resp.request_id = req.request_id;

	ret = write(dev->vduse_dev_fd, &resp, sizeof(resp));
	if (ret != sizeof(resp)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to write response %s\n",
				strerror(errno));
		return;
	}

	if ((old_status ^ dev->status) & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
		if (dev->status & VIRTIO_DEVICE_STATUS_DRIVER_OK)
			vduse_device_start(dev);
		else
			vduse_device_stop(dev);
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO, "Request %s (%u) handled successfully\n",
			vduse_req_id_to_str(req.type), req.type);
}

int
vduse_device_create(const char *path, bool compliant_ol_flags)
{
	int control_fd, dev_fd, vid, ret;
	rte_thread_t fdset_tid;
	uint32_t i, max_queue_pairs, total_queues;
	struct virtio_net *dev;
	struct virtio_net_config vnet_config = {{ 0 }};
	uint64_t ver = VHOST_VDUSE_API_VERSION;
	uint64_t features;
	struct vduse_dev_config *dev_config = NULL;
	const char *name = path + strlen("/dev/vduse/");

	/* If first device, create events dispatcher thread */
	if (vduse_events_thread == false) {
		/**
		 * create a pipe which will be waited by poll and notified to
		 * rebuild the wait list of poll.
		 */
		if (fdset_pipe_init(&vduse.fdset) < 0) {
			VHOST_LOG_CONFIG(path, ERR, "failed to create pipe for vduse fdset\n");
			return -1;
		}

		ret = rte_thread_create_internal_control(&fdset_tid, "vduse-evt",
				fdset_event_dispatch, &vduse.fdset);
		if (ret != 0) {
			VHOST_LOG_CONFIG(path, ERR, "failed to create vduse fdset handling thread\n");
			fdset_pipe_uninit(&vduse.fdset);
			return -1;
		}

		vduse_events_thread = true;
	}

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

	dev_config = malloc(offsetof(struct vduse_dev_config, config) +
			sizeof(vnet_config));
	if (!dev_config) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to allocate VDUSE config\n");
		ret = -1;
		goto out_ctrl_close;
	}

	ret = rte_vhost_driver_get_features(path, &features);
	if (ret < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to get backend features\n");
		goto out_free;
	}

	ret = rte_vhost_driver_get_queue_num(path, &max_queue_pairs);
	if (ret < 0) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to get max queue pairs\n");
		goto out_free;
	}

	VHOST_LOG_CONFIG(path, INFO, "VDUSE max queue pairs: %u\n", max_queue_pairs);
	total_queues = max_queue_pairs * 2;

	if (max_queue_pairs == 1)
		features &= ~(RTE_BIT64(VIRTIO_NET_F_CTRL_VQ) | RTE_BIT64(VIRTIO_NET_F_MQ));
	else
		total_queues += 1; /* Includes ctrl queue */

	vnet_config.max_virtqueue_pairs = max_queue_pairs;
	memset(dev_config, 0, sizeof(struct vduse_dev_config));

	strncpy(dev_config->name, name, VDUSE_NAME_MAX - 1);
	dev_config->device_id = VIRTIO_ID_NET;
	dev_config->vendor_id = 0;
	dev_config->features = features;
	dev_config->vq_num = total_queues;
	dev_config->vq_align = sysconf(_SC_PAGE_SIZE);
	dev_config->config_size = sizeof(struct virtio_net_config);
	memcpy(dev_config->config, &vnet_config, sizeof(vnet_config));

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
	vhost_setup_virtio_net(dev->vid, true, compliant_ol_flags, true, true);

	for (i = 0; i < total_queues; i++) {
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

	dev->cvq = dev->virtqueue[max_queue_pairs * 2];

	ret = fdset_add(&vduse.fdset, dev->vduse_dev_fd, vduse_events_handler, NULL, dev);
	if (ret) {
		VHOST_LOG_CONFIG(name, ERR, "Failed to add fd %d to vduse fdset\n",
				dev->vduse_dev_fd);
		goto out_dev_destroy;
	}
	fdset_pipe_notify(&vduse.fdset);

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

	vduse_device_stop(dev);

	fdset_del(&vduse.fdset, dev->vduse_dev_fd);
	fdset_pipe_notify(&vduse.fdset);

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
