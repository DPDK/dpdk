/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <uapi/linux/vduse.h>

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/virtio_net.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_eal_paging.h>
#include <rte_thread.h>

#include "fd_man.h"
#include "iotlb.h"
#include "vduse.h"
#include "vhost.h"
#include "virtio_net_ctrl.h"

#define VHOST_VDUSE_API_VERSION 0
#define VDUSE_CTRL_PATH "/dev/vduse/control"

struct vduse {
	struct fdset *fdset;
};

static struct vduse vduse;

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
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to get IOTLB entry for 0x%" PRIx64,
				iova);
		return -1;
	}

	fd = ret;

	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "New IOTLB entry:");
	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "\tIOVA: %" PRIx64 " - %" PRIx64,
			(uint64_t)entry.start, (uint64_t)entry.last);
	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "\toffset: %" PRIx64, (uint64_t)entry.offset);
	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "\tfd: %d", fd);
	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "\tperm: %x", entry.perm);

	size = entry.last - entry.start + 1;
	mmap_addr = mmap(0, size + entry.offset, entry.perm, MAP_SHARED, fd, 0);
	if (!mmap_addr) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Failed to mmap IOTLB entry for 0x%" PRIx64, iova);
		ret = -1;
		goto close_fd;
	}

	ret = fstat(fd, &stat);
	if (ret < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to get page size.");
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
vduse_control_queue_event(int fd, void *arg, int *close __rte_unused)
{
	struct virtio_net *dev = arg;
	uint64_t buf;
	int ret;

	ret = read(fd, &buf, sizeof(buf));
	if (ret < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to read control queue event: %s",
				strerror(errno));
		return;
	}

	VHOST_CONFIG_LOG(dev->ifname, DEBUG, "Control queue kicked");
	if (virtio_net_ctrl_handle(dev))
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to handle ctrl request");
}

static void
vduse_vring_setup(struct virtio_net *dev, unsigned int index, bool reconnect)
{
	struct vhost_virtqueue *vq = dev->virtqueue[index];
	struct vhost_vring_addr *ra = &vq->ring_addrs;
	struct vduse_vq_info vq_info;
	struct vduse_vq_eventfd vq_efd;
	int ret;

	vq_info.index = index;
	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_GET_INFO, &vq_info);
	if (ret) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to get VQ %u info: %s",
				index, strerror(errno));
		return;
	}

	if (reconnect) {
		vq->last_avail_idx = vq->reconnect_log->last_avail_idx;
		vq->last_used_idx = vq->reconnect_log->last_avail_idx;
	} else {
		vq->last_avail_idx = vq_info.split.avail_index;
		vq->last_used_idx = vq_info.split.avail_index;
	}
	vq->size = vq_info.num;
	vq->ready = true;
	vq->enabled = vq_info.ready;
	ra->desc_user_addr = vq_info.desc_addr;
	ra->avail_user_addr = vq_info.driver_addr;
	ra->used_user_addr = vq_info.device_addr;
	VHOST_CONFIG_LOG(dev->ifname, INFO, "VQ %u info:", index);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tnum: %u", vq_info.num);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tdesc_addr: %llx",
			(unsigned long long)vq_info.desc_addr);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tdriver_addr: %llx",
			(unsigned long long)vq_info.driver_addr);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tdevice_addr: %llx",
			(unsigned long long)vq_info.device_addr);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tavail_idx: %u", vq->last_avail_idx);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tused_idx: %u", vq->last_used_idx);
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tready: %u", vq_info.ready);
	vq->kickfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (vq->kickfd < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to init kickfd for VQ %u: %s",
				index, strerror(errno));
		vq->kickfd = VIRTIO_INVALID_EVENTFD;
		return;
	}
	VHOST_CONFIG_LOG(dev->ifname, INFO, "\tkick fd: %d", vq->kickfd);

	vq->shadow_used_split = rte_malloc_socket(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE, 0);
	vq->batch_copy_elems = rte_malloc_socket(NULL,
				vq->size * sizeof(struct batch_copy_elem),
				RTE_CACHE_LINE_SIZE, 0);

	rte_rwlock_write_lock(&vq->access_lock);
	vhost_user_iotlb_rd_lock(vq);
	if (vring_translate(dev, vq))
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to translate vring %d addresses",
				index);

	if (vhost_enable_guest_notification(dev, vq, 0))
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Failed to disable guest notifications on vring %d",
				index);
	vhost_user_iotlb_rd_unlock(vq);
	rte_rwlock_write_unlock(&vq->access_lock);

	vq_efd.index = index;
	vq_efd.fd = vq->kickfd;

	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
	if (ret) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to setup kickfd for VQ %u: %s",
				index, strerror(errno));
		close(vq->kickfd);
		vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
		return;
	}

	if (vq == dev->cvq) {
		ret = fdset_add(vduse.fdset, vq->kickfd, vduse_control_queue_event, NULL, dev);
		if (ret) {
			VHOST_CONFIG_LOG(dev->ifname, ERR,
					"Failed to setup kickfd handler for VQ %u: %s",
					index, strerror(errno));
			vq_efd.fd = VDUSE_EVENTFD_DEASSIGN;
			ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
			close(vq->kickfd);
			vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
		}
		vhost_enable_guest_notification(dev, vq, 1);
		VHOST_CONFIG_LOG(dev->ifname, INFO, "Ctrl queue event handler installed");
	}
}

static void
vduse_vring_cleanup(struct virtio_net *dev, unsigned int index)
{
	struct vhost_virtqueue *vq = dev->virtqueue[index];
	struct vduse_vq_eventfd vq_efd;
	int ret;

	if (vq == dev->cvq && vq->kickfd >= 0)
		fdset_del(vduse.fdset, vq->kickfd);

	vq_efd.index = index;
	vq_efd.fd = VDUSE_EVENTFD_DEASSIGN;

	ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP_KICKFD, &vq_efd);
	if (ret)
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to cleanup kickfd for VQ %u: %s",
				index, strerror(errno));

	close(vq->kickfd);
	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;

	rte_rwlock_write_lock(&vq->access_lock);
	vring_invalidate(dev, vq);
	rte_rwlock_write_unlock(&vq->access_lock);

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

/*
 * Tests show that virtqueues get ready at the first retry at worst,
 * but let's be on the safe side and allow more retries.
 */
#define VDUSE_VQ_READY_POLL_MAX_RETRIES 100

static int
vduse_wait_for_virtqueues_ready(struct virtio_net *dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < dev->nr_vring; i++) {
		int retry_count = 0;

		while (retry_count < VDUSE_VQ_READY_POLL_MAX_RETRIES) {
			struct vduse_vq_info vq_info = { 0 };

			vq_info.index = i;
			ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_GET_INFO, &vq_info);
			if (ret) {
				VHOST_CONFIG_LOG(dev->ifname, ERR,
					"Failed to get VQ %u info while polling ready state: %s",
					i, strerror(errno));
				return -1;
			}

			if (vq_info.ready) {
				VHOST_CONFIG_LOG(dev->ifname, DEBUG,
					"VQ %u is ready after %u retries", i, retry_count);
				break;
			}

			retry_count++;
			usleep(1000);
		}

		if (retry_count >= VDUSE_VQ_READY_POLL_MAX_RETRIES) {
			VHOST_CONFIG_LOG(dev->ifname, ERR,
				"VQ %u ready state polling timeout after %u retries",
				i, VDUSE_VQ_READY_POLL_MAX_RETRIES);
			return -1;
		}
	}

	VHOST_CONFIG_LOG(dev->ifname, INFO, "All virtqueues are ready after polling");
	return 0;
}

static void
vduse_device_start(struct virtio_net *dev, bool reconnect)
{
	unsigned int i, ret;

	VHOST_CONFIG_LOG(dev->ifname, INFO, "Starting device...");

	dev->notify_ops = vhost_driver_callback_get(dev->ifname);
	if (!dev->notify_ops) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Failed to get callback ops for driver");
		return;
	}

	ret = ioctl(dev->vduse_dev_fd, VDUSE_DEV_GET_FEATURES, &dev->features);
	if (ret) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to get features: %s",
				strerror(errno));
		return;
	}

	if (reconnect && dev->features != dev->reconnect_log->features) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Mismatch between reconnect file features 0x%" PRIx64 " & device features 0x%" PRIx64,
				dev->reconnect_log->features, dev->features);
		return;
	}

	dev->reconnect_log->features = dev->features;

	VHOST_CONFIG_LOG(dev->ifname, INFO, "Negotiated Virtio features: 0x%" PRIx64,
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
		vduse_vring_setup(dev, i, reconnect);

	dev->flags |= VIRTIO_DEV_READY;

	if (!dev->notify_ops->new_device ||
		dev->notify_ops->new_device(dev->vid) == 0)
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

	VHOST_CONFIG_LOG(dev->ifname, INFO, "Stopping device...");

	vhost_destroy_device_notify(dev);

	dev->flags &= ~VIRTIO_DEV_READY;

	for (i = 0; i < dev->nr_vring; i++)
		vduse_vring_cleanup(dev, i);

	vhost_user_iotlb_flush_all(dev);
}

static void
vduse_events_handler(int fd, void *arg, int *close __rte_unused)
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
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to read request: %s",
				strerror(errno));
		return;
	} else if (ret < (int)sizeof(req)) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Incomplete to read request %d", ret);
		return;
	}

	VHOST_CONFIG_LOG(dev->ifname, INFO, "New request: %s (%u)",
			vduse_req_id_to_str(req.type), req.type);

	switch (req.type) {
	case VDUSE_GET_VQ_STATE:
		vq = dev->virtqueue[req.vq_state.index];
		VHOST_CONFIG_LOG(dev->ifname, INFO, "\tvq index: %u, avail_index: %u",
				req.vq_state.index, vq->last_avail_idx);
		resp.vq_state.split.avail_index = vq->last_avail_idx;
		resp.result = VDUSE_REQ_RESULT_OK;
		break;
	case VDUSE_SET_STATUS:
		VHOST_CONFIG_LOG(dev->ifname, INFO, "\tnew status: 0x%08x",
				req.s.status);
		old_status = dev->status;
		dev->status = req.s.status;
		dev->reconnect_log->status = dev->status;
		resp.result = VDUSE_REQ_RESULT_OK;
		break;
	case VDUSE_UPDATE_IOTLB:
		VHOST_CONFIG_LOG(dev->ifname, INFO, "\tIOVA range: %" PRIx64 " - %" PRIx64,
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
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to write response %s",
				strerror(errno));
		return;
	}

	if ((old_status ^ dev->status) & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
		if (dev->status & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
			/* Poll virtqueues ready states before starting device */
			ret = vduse_wait_for_virtqueues_ready(dev);
			if (ret < 0) {
				VHOST_CONFIG_LOG(dev->ifname, ERR,
					"Failed to wait for virtqueues ready, aborting device start");
				return;
			}
			vduse_device_start(dev, false);
		} else {
			vduse_device_stop(dev);
		}
	}

	VHOST_CONFIG_LOG(dev->ifname, INFO, "Request %s (%u) handled successfully",
			vduse_req_id_to_str(req.type), req.type);
}

static char vduse_reconnect_dir[PATH_MAX];
static bool vduse_reconnect_path_set;

static int
vduse_reconnect_path_init(void)
{
	const char *directory;
	int ret;

	if (vduse_reconnect_path_set == true)
		return 0;

	/* from RuntimeDirectory= see systemd.exec */
	directory = getenv("RUNTIME_DIRECTORY");
	if (directory == NULL) {
		/*
		 * Used standard convention defined in
		 * XDG Base Directory Specification and
		 * Filesystem Hierarchy Standard.
		 */
		if (getuid() == 0)
			directory = "/var/run";
		else
			directory = getenv("XDG_RUNTIME_DIR") ? : "/tmp";
	}

	ret = snprintf(vduse_reconnect_dir, sizeof(vduse_reconnect_dir), "%s/vduse",
			directory);
	if (ret < 0 || ret == sizeof(vduse_reconnect_dir)) {
		VHOST_CONFIG_LOG("vduse", ERR, "Error creating VDUSE reconnect path name");
		return -1;
	}

	ret = mkdir(vduse_reconnect_dir, 0700);
	if (ret < 0 && errno != EEXIST) {
		VHOST_CONFIG_LOG("vduse", ERR, "Error creating '%s': %s",
				vduse_reconnect_dir, strerror(errno));
		return -1;
	}

	VHOST_CONFIG_LOG("vduse", INFO, "Created VDUSE reconnect directory in %s",
			vduse_reconnect_dir);

	vduse_reconnect_path_set = true;

	return 0;
}

static int
vduse_reconnect_log_map(struct virtio_net *dev, bool create)
{
	char reco_file[PATH_MAX];
	int fd, ret;
	const char *name = dev->ifname + strlen("/dev/vduse/");

	if (vduse_reconnect_path_init() < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to initialize reconnect path");
		return -1;
	}

	ret = snprintf(reco_file, sizeof(reco_file), "%s/%s", vduse_reconnect_dir, name);
	if (ret < 0 || ret == sizeof(reco_file)) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to create vduse reconnect path name");
		return -1;
	}

	if (create) {
		fd = open(reco_file, O_CREAT | O_EXCL | O_RDWR, 0600);
		if (fd < 0) {
			if (errno == EEXIST) {
				VHOST_CONFIG_LOG(dev->ifname, ERR, "Reconnect file %s exists but not the device",
						reco_file);
			} else {
				VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to open reconnect file %s (%s)",
						reco_file, strerror(errno));
			}
			return -1;
		}

		ret = ftruncate(fd, sizeof(*dev->reconnect_log));
		if (ret < 0) {
			VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to truncate reconnect file %s (%s)",
					reco_file, strerror(errno));
			goto out_close;
		}
	} else {
		fd = open(reco_file, O_RDWR, 0600);
		if (fd < 0) {
			if (errno == ENOENT)
				VHOST_CONFIG_LOG(dev->ifname, ERR, "Missing reconnect file (%s)", reco_file);
			else
				VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to open reconnect file %s (%s)",
						reco_file, strerror(errno));
			return -1;
		}
	}

	dev->reconnect_log = mmap(NULL, sizeof(*dev->reconnect_log), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0);
	if (dev->reconnect_log == MAP_FAILED) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to mmap reconnect file %s (%s)",
				reco_file, strerror(errno));
		ret = -1;
		goto out_close;
	}
	ret = 0;

out_close:
	close(fd);

	return ret;
}

static int
vduse_reconnect_log_check(struct virtio_net *dev, uint64_t features, uint32_t total_queues)
{
	if (dev->reconnect_log->version != VHOST_RECONNECT_VERSION) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Version mismatch between backend (0x%x) & reconnection file (0x%x)",
				VHOST_RECONNECT_VERSION, dev->reconnect_log->version);
		return -1;
	}

	if ((dev->reconnect_log->features & features) != dev->reconnect_log->features) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Features mismatch between backend (0x%" PRIx64 ") & reconnection file (0x%" PRIx64 ")",
				features, dev->reconnect_log->features);
		return -1;
	}

	if (dev->reconnect_log->nr_vrings != total_queues) {
		VHOST_CONFIG_LOG(dev->ifname, ERR,
				"Queues number mismatch between backend (%u) and reconnection file (%u)",
				total_queues, dev->reconnect_log->nr_vrings);
		return -1;
	}

	return 0;
}

static void
vduse_reconnect_handler(int fd __rte_unused, void *arg, int *close)
{
	struct virtio_net *dev = arg;

	vduse_device_start(dev, true);

	*close = 1;
}

static int
vduse_reconnect_start_device(struct virtio_net *dev)
{
	int fd, ret;

	/*
	 * Make vduse_device_start() being executed in the same
	 * context for both reconnection and fresh startup.
	 */
	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to create reconnect efd: %s",
				strerror(errno));
		ret = -1;
		goto out_err;
	}

	ret = fdset_add(vduse.fdset, fd, vduse_reconnect_handler, NULL, dev);
	if (ret) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to add reconnect efd %d to vduse fdset",
				fd);
		goto out_err_close;
	}

	ret = eventfd_write(fd, (eventfd_t)1);
	if (ret < 0) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to write to reconnect eventfd");
		goto out_err_fdset;
	}

	return 0;

out_err_fdset:
	fdset_del(vduse.fdset, fd);
out_err_close:
	close(fd);
out_err:
	return ret;
}

int
vduse_device_create(const char *path, bool compliant_ol_flags)
{
	int control_fd, dev_fd, vid, ret;
	uint32_t i, max_queue_pairs, total_queues;
	struct virtio_net *dev;
	struct virtio_net_config vnet_config = {{ 0 }};
	uint64_t ver = VHOST_VDUSE_API_VERSION;
	uint64_t features;
	const char *name = path + strlen("/dev/vduse/");
	bool reconnect = false;

	if (vduse.fdset == NULL) {
		vduse.fdset = fdset_init("vduse-evt");
		if (vduse.fdset == NULL) {
			VHOST_CONFIG_LOG(path, ERR, "failed to init VDUSE fdset");
			return -1;
		}
	}

	control_fd = open(VDUSE_CTRL_PATH, O_RDWR);
	if (control_fd < 0) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to open %s: %s",
				VDUSE_CTRL_PATH, strerror(errno));
		return -1;
	}

	if (ioctl(control_fd, VDUSE_SET_API_VERSION, &ver)) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to set API version: %" PRIu64 ": %s",
				ver, strerror(errno));
		ret = -1;
		goto out_ctrl_close;
	}

	ret = rte_vhost_driver_get_features(path, &features);
	if (ret < 0) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to get backend features");
		goto out_ctrl_close;
	}

	ret = rte_vhost_driver_get_queue_num(path, &max_queue_pairs);
	if (ret < 0) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to get max queue pairs");
		goto out_ctrl_close;
	}

	VHOST_CONFIG_LOG(path, INFO, "VDUSE max queue pairs: %u", max_queue_pairs);
	total_queues = max_queue_pairs * 2;

	if (max_queue_pairs == 1)
		features &= ~(RTE_BIT64(VIRTIO_NET_F_CTRL_VQ) | RTE_BIT64(VIRTIO_NET_F_MQ));
	else
		total_queues += 1; /* Includes ctrl queue */

	dev_fd = open(path, O_RDWR);
	if (dev_fd >= 0) {
		VHOST_CONFIG_LOG(name, INFO, "Device already exists, reconnecting...");
		reconnect = true;
	} else if (errno == ENOENT) {
		struct vduse_dev_config *dev_config;

		dev_config = malloc(offsetof(struct vduse_dev_config, config) +
				sizeof(vnet_config));
		if (!dev_config) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to allocate VDUSE config");
			ret = -1;
			goto out_ctrl_close;
		}

		vnet_config.max_virtqueue_pairs = max_queue_pairs;
		memset(dev_config, 0, sizeof(struct vduse_dev_config));

		rte_strscpy(dev_config->name, name, VDUSE_NAME_MAX - 1);
		dev_config->device_id = VIRTIO_ID_NET;
		dev_config->vendor_id = 0;
		dev_config->features = features;
		dev_config->vq_num = total_queues;
		dev_config->vq_align = rte_mem_page_size();
		dev_config->config_size = sizeof(struct virtio_net_config);
		memcpy(dev_config->config, &vnet_config, sizeof(vnet_config));

		ret = ioctl(control_fd, VDUSE_CREATE_DEV, dev_config);
		free(dev_config);
		dev_config = NULL;
		if (ret < 0) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to create VDUSE device: %s",
					strerror(errno));
			goto out_ctrl_close;
		}

		dev_fd = open(path, O_RDWR);
		if (dev_fd < 0) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to open newly created device %s: %s",
					path, strerror(errno));
			ret = -1;
			goto out_ctrl_close;
		}
	} else {
		VHOST_CONFIG_LOG(name, ERR, "Failed to open device %s: %s",
				path, strerror(errno));
		ret = -1;
		goto out_ctrl_close;
	}

	ret = fcntl(dev_fd, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to set chardev as non-blocking: %s",
				strerror(errno));
		goto out_dev_close;
	}

	vid = vhost_new_device(&vduse_backend_ops);
	if (vid < 0) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to create new Vhost device");
		ret = -1;
		goto out_dev_close;
	}

	dev = get_device(vid);
	if (!dev) {
		ret = -1;
		goto out_dev_destroy;
	}

	strncpy(dev->ifname, path, IF_NAME_SZ - 1);
	dev->vduse_ctrl_fd = control_fd;
	dev->vduse_dev_fd = dev_fd;

	ret = vduse_reconnect_log_map(dev, !reconnect);
	if (ret < 0)
		goto out_dev_destroy;

	if (reconnect) {
		ret = vduse_reconnect_log_check(dev, features, total_queues);
		if (ret < 0)
			goto out_log_unmap;

		dev->status = dev->reconnect_log->status;
	} else {
		dev->reconnect_log->version = VHOST_RECONNECT_VERSION;
		dev->reconnect_log->nr_vrings = total_queues;
		memcpy(&dev->reconnect_log->config, &vnet_config, sizeof(vnet_config));
	}

	vhost_setup_virtio_net(dev->vid, true, compliant_ol_flags, true, true);

	for (i = 0; i < total_queues; i++) {
		struct vduse_vq_config vq_cfg = { 0 };
		struct vhost_virtqueue *vq;

		ret = alloc_vring_queue(dev, i);
		if (ret) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to alloc vring %d metadata", i);
			goto out_log_unmap;
		}

		vq = dev->virtqueue[i];
		vq->reconnect_log = &dev->reconnect_log->vring[i];

		if (reconnect)
			continue;

		vq_cfg.index = i;
		vq_cfg.max_size = 1024;

		ret = ioctl(dev->vduse_dev_fd, VDUSE_VQ_SETUP, &vq_cfg);
		if (ret) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to set-up VQ %d", i);
			goto out_log_unmap;
		}
	}

	dev->cvq = dev->virtqueue[max_queue_pairs * 2];

	ret = fdset_add(vduse.fdset, dev->vduse_dev_fd, vduse_events_handler, NULL, dev);
	if (ret) {
		VHOST_CONFIG_LOG(name, ERR, "Failed to add fd %d to vduse fdset",
				dev->vduse_dev_fd);
		goto out_log_unmap;
	}

	if (reconnect && dev->status & VIRTIO_DEVICE_STATUS_DRIVER_OK)  {
		ret = vduse_reconnect_start_device(dev);
		if (ret)
			goto out_log_unmap;
	}

	return 0;

out_log_unmap:
	munmap(dev->reconnect_log, sizeof(*dev->reconnect_log));
out_dev_destroy:
	vhost_destroy_device(vid);
out_dev_close:
	if (dev_fd >= 0)
		close(dev_fd);
	ioctl(control_fd, VDUSE_DESTROY_DEV, name);
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

	if (dev->reconnect_log)
		munmap(dev->reconnect_log, sizeof(*dev->reconnect_log));

	vduse_device_stop(dev);

	fdset_del(vduse.fdset, dev->vduse_dev_fd);

	if (dev->vduse_dev_fd >= 0) {
		close(dev->vduse_dev_fd);
		dev->vduse_dev_fd = -1;
	}

	if (dev->vduse_ctrl_fd >= 0) {
		char reconnect_file[PATH_MAX];

		ret = ioctl(dev->vduse_ctrl_fd, VDUSE_DESTROY_DEV, name);
		if (ret) {
			VHOST_CONFIG_LOG(name, ERR, "Failed to destroy VDUSE device: %s",
					strerror(errno));
		} else {
			/*
			 * VDUSE device was no more attached to the vDPA bus,
			 * so we can remove the reconnect file.
			 */
			ret = snprintf(reconnect_file, sizeof(reconnect_file), "%s/%s",
					vduse_reconnect_dir, name);
			if (ret < 0 || ret == sizeof(reconnect_file))
				VHOST_CONFIG_LOG(name, ERR,
						"Failed to create vduse reconnect path name");
			else
				unlink(reconnect_file);
		}

		close(dev->vduse_ctrl_fd);
		dev->vduse_ctrl_fd = -1;
	}

	vhost_destroy_device(vid);

	return 0;
}
