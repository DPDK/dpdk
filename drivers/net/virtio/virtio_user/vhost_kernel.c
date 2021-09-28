/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <rte_memory.h>

#include "vhost.h"
#include "virtio_user_dev.h"
#include "vhost_kernel_tap.h"

struct vhost_kernel_data {
	int *vhostfds;
	int *tapfds;
};

struct vhost_memory_kernel {
	uint32_t nregions;
	uint32_t padding;
	struct vhost_memory_region regions[0];
};

/* vhost kernel ioctls */
#define VHOST_VIRTIO 0xAF
#define VHOST_GET_FEATURES _IOR(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_FEATURES _IOW(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_OWNER _IO(VHOST_VIRTIO, 0x01)
#define VHOST_RESET_OWNER _IO(VHOST_VIRTIO, 0x02)
#define VHOST_SET_MEM_TABLE _IOW(VHOST_VIRTIO, 0x03, struct vhost_memory_kernel)
#define VHOST_SET_LOG_BASE _IOW(VHOST_VIRTIO, 0x04, __u64)
#define VHOST_SET_LOG_FD _IOW(VHOST_VIRTIO, 0x07, int)
#define VHOST_SET_VRING_NUM _IOW(VHOST_VIRTIO, 0x10, struct vhost_vring_state)
#define VHOST_SET_VRING_ADDR _IOW(VHOST_VIRTIO, 0x11, struct vhost_vring_addr)
#define VHOST_SET_VRING_BASE _IOW(VHOST_VIRTIO, 0x12, struct vhost_vring_state)
#define VHOST_GET_VRING_BASE _IOWR(VHOST_VIRTIO, 0x12, struct vhost_vring_state)
#define VHOST_SET_VRING_KICK _IOW(VHOST_VIRTIO, 0x20, struct vhost_vring_file)
#define VHOST_SET_VRING_CALL _IOW(VHOST_VIRTIO, 0x21, struct vhost_vring_file)
#define VHOST_SET_VRING_ERR _IOW(VHOST_VIRTIO, 0x22, struct vhost_vring_file)
#define VHOST_NET_SET_BACKEND _IOW(VHOST_VIRTIO, 0x30, struct vhost_vring_file)

/* with below features, vhost kernel does not need to do the checksum and TSO,
 * these info will be passed to virtio_user through virtio net header.
 */
#define VHOST_KERNEL_GUEST_OFFLOADS_MASK	\
	((1ULL << VIRTIO_NET_F_GUEST_CSUM) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_TSO4) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_TSO6) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_ECN)  |	\
	 (1ULL << VIRTIO_NET_F_GUEST_UFO))

/* with below features, when flows from virtio_user to vhost kernel
 * (1) if flows goes up through the kernel networking stack, it does not need
 * to verify checksum, which can save CPU cycles;
 * (2) if flows goes through a Linux bridge and outside from an interface
 * (kernel driver), checksum and TSO will be done by GSO in kernel or even
 * offloaded into real physical device.
 */
#define VHOST_KERNEL_HOST_OFFLOADS_MASK		\
	((1ULL << VIRTIO_NET_F_HOST_TSO4) |	\
	 (1ULL << VIRTIO_NET_F_HOST_TSO6) |	\
	 (1ULL << VIRTIO_NET_F_CSUM))

static uint64_t max_regions = 64;

static void
get_vhost_kernel_max_regions(void)
{
	int fd;
	char buf[20] = {'\0'};

	fd = open("/sys/module/vhost/parameters/max_mem_regions", O_RDONLY);
	if (fd < 0)
		return;

	if (read(fd, buf, sizeof(buf) - 1) > 0)
		max_regions = strtoull(buf, NULL, 10);

	close(fd);
}

static int
vhost_kernel_ioctl(int fd, uint64_t request, void *arg)
{
	int ret;

	ret = ioctl(fd, request, arg);
	if (ret) {
		PMD_DRV_LOG(ERR, "Vhost-kernel ioctl %"PRIu64" failed (%s)",
				request, strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_kernel_set_owner(struct virtio_user_dev *dev)
{
	int ret;
	uint32_t i;
	struct vhost_kernel_data *data = dev->backend_data;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		if (data->vhostfds[i] < 0)
			continue;

		ret = vhost_kernel_ioctl(data->vhostfds[i], VHOST_SET_OWNER, NULL);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int
vhost_kernel_get_features(struct virtio_user_dev *dev, uint64_t *features)
{
	struct vhost_kernel_data *data = dev->backend_data;
	unsigned int tap_flags;
	int ret;

	ret = vhost_kernel_ioctl(data->vhostfds[0], VHOST_GET_FEATURES, features);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to get features");
		return -1;
	}

	ret = tap_get_flags(data->tapfds[0], &tap_flags);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to get TAP features");
		return -1;
	}

	/* with tap as the backend, all these features are supported
	 * but not claimed by vhost-net, so we add them back when
	 * reporting to upper layer.
	 */
	if (tap_flags & IFF_VNET_HDR) {
		*features |= VHOST_KERNEL_GUEST_OFFLOADS_MASK;
		*features |= VHOST_KERNEL_HOST_OFFLOADS_MASK;
	}

	/* vhost_kernel will not declare this feature, but it does
	 * support multi-queue.
	 */
	if (tap_flags & IFF_MULTI_QUEUE)
		*features |= (1ull << VIRTIO_NET_F_MQ);

	return 0;
}

static int
vhost_kernel_set_features(struct virtio_user_dev *dev, uint64_t features)
{
	struct vhost_kernel_data *data = dev->backend_data;
	uint32_t i;
	int ret;

	/* We don't need memory protection here */
	features &= ~(1ULL << VIRTIO_F_IOMMU_PLATFORM);
	/* VHOST kernel does not know about below flags */
	features &= ~VHOST_KERNEL_GUEST_OFFLOADS_MASK;
	features &= ~VHOST_KERNEL_HOST_OFFLOADS_MASK;
	features &= ~(1ULL << VIRTIO_NET_F_MQ);

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		if (data->vhostfds[i] < 0)
			continue;

		ret = vhost_kernel_ioctl(data->vhostfds[i], VHOST_SET_FEATURES, &features);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int
add_memseg_list(const struct rte_memseg_list *msl, void *arg)
{
	struct vhost_memory_kernel *vm = arg;
	struct vhost_memory_region *mr;
	void *start_addr;
	uint64_t len;

	if (msl->external)
		return 0;

	if (vm->nregions >= max_regions)
		return -1;

	start_addr = msl->base_va;
	len = msl->page_sz * msl->memseg_arr.len;

	mr = &vm->regions[vm->nregions++];

	mr->guest_phys_addr = (uint64_t)(uintptr_t)start_addr;
	mr->userspace_addr = (uint64_t)(uintptr_t)start_addr;
	mr->memory_size = len;
	mr->mmap_offset = 0; /* flags_padding */

	PMD_DRV_LOG(DEBUG, "index=%u addr=%p len=%" PRIu64,
			vm->nregions - 1, start_addr, len);

	return 0;
}

/* By default, vhost kernel module allows 64 regions, but DPDK may
 * have much more memory regions. Below function will treat each
 * contiguous memory space reserved by DPDK as one region.
 */
static int
vhost_kernel_set_memory_table(struct virtio_user_dev *dev)
{
	uint32_t i;
	struct vhost_kernel_data *data = dev->backend_data;
	struct vhost_memory_kernel *vm;
	int ret;

	vm = malloc(sizeof(struct vhost_memory_kernel) +
			max_regions *
			sizeof(struct vhost_memory_region));
	if (!vm)
		goto err;

	vm->nregions = 0;
	vm->padding = 0;

	/*
	 * The memory lock has already been taken by memory subsystem
	 * or virtio_user_start_device().
	 */
	ret = rte_memseg_list_walk_thread_unsafe(add_memseg_list, vm);
	if (ret < 0)
		goto err_free;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		if (data->vhostfds[i] < 0)
			continue;

		ret = vhost_kernel_ioctl(data->vhostfds[i], VHOST_SET_MEM_TABLE, vm);
		if (ret < 0)
			goto err_free;
	}

	free(vm);

	return 0;
err_free:
	free(vm);
err:
	PMD_DRV_LOG(ERR, "Failed to set memory table");
	return -1;
}

static int
vhost_kernel_set_vring(struct virtio_user_dev *dev, uint64_t req, struct vhost_vring_state *state)
{
	int ret, fd;
	unsigned int index = state->index;
	struct vhost_kernel_data *data = dev->backend_data;

	/* Convert from queue index to queue-pair & offset */
	fd = data->vhostfds[state->index / 2];
	state->index %= 2;

	ret = vhost_kernel_ioctl(fd, req, state);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set vring (request %" PRIu64 ")", req);
		return -1;
	}

	/* restore index back to queue index */
	state->index = index;

	return 0;
}

static int
vhost_kernel_set_vring_num(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_kernel_set_vring(dev, VHOST_SET_VRING_NUM, state);
}

static int
vhost_kernel_set_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_kernel_set_vring(dev, VHOST_SET_VRING_BASE, state);
}

static int
vhost_kernel_get_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_kernel_set_vring(dev, VHOST_GET_VRING_BASE, state);
}

static int
vhost_kernel_set_vring_file(struct virtio_user_dev *dev, uint64_t req,
		struct vhost_vring_file *file)
{
	int ret, fd;
	unsigned int index = file->index;
	struct vhost_kernel_data *data = dev->backend_data;

	/* Convert from queue index to queue-pair & offset */
	fd = data->vhostfds[file->index / 2];
	file->index %= 2;

	ret = vhost_kernel_ioctl(fd, req, file);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set vring file (request %" PRIu64 ")", req);
		return -1;
	}

	/* restore index back to queue index */
	file->index = index;

	return 0;
}

static int
vhost_kernel_set_vring_kick(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	return vhost_kernel_set_vring_file(dev, VHOST_SET_VRING_KICK, file);
}

static int
vhost_kernel_set_vring_call(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	return vhost_kernel_set_vring_file(dev, VHOST_SET_VRING_CALL, file);
}

static int
vhost_kernel_set_vring_addr(struct virtio_user_dev *dev, struct vhost_vring_addr *addr)
{
	int ret, fd;
	unsigned int index = addr->index;
	struct vhost_kernel_data *data = dev->backend_data;

	/* Convert from queue index to queue-pair & offset */
	fd = data->vhostfds[addr->index / 2];
	addr->index %= 2;

	ret = vhost_kernel_ioctl(fd, VHOST_SET_VRING_ADDR, addr);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set vring address");
		return -1;
	}

	/* restore index back to queue index */
	addr->index = index;

	return 0;
}

static int
vhost_kernel_get_status(struct virtio_user_dev *dev __rte_unused, uint8_t *status __rte_unused)
{
	return -ENOTSUP;
}

static int
vhost_kernel_set_status(struct virtio_user_dev *dev __rte_unused, uint8_t status __rte_unused)
{
	return -ENOTSUP;
}

/**
 * Set up environment to talk with a vhost kernel backend.
 *
 * @return
 *   - (-1) if fail to set up;
 *   - (>=0) if successful.
 */
static int
vhost_kernel_setup(struct virtio_user_dev *dev)
{
	struct vhost_kernel_data *data;
	unsigned int tap_features;
	unsigned int tap_flags;
	const char *ifname;
	uint32_t q, i;
	int vhostfd;

	if (tap_support_features(&tap_features) < 0)
		return -1;

	if ((tap_features & IFF_VNET_HDR) == 0) {
		PMD_INIT_LOG(ERR, "TAP does not support IFF_VNET_HDR");
		return -1;
	}

	data = malloc(sizeof(*data));
	if (!data) {
		PMD_INIT_LOG(ERR, "(%s) Failed to allocate Vhost-kernel data", dev->path);
		return -1;
	}

	data->vhostfds = malloc(dev->max_queue_pairs * sizeof(int));
	if (!data->vhostfds) {
		PMD_INIT_LOG(ERR, "(%s) Failed to allocate Vhost FDs", dev->path);
		goto err_data;
	}
	data->tapfds = malloc(dev->max_queue_pairs * sizeof(int));
	if (!data->tapfds) {
		PMD_INIT_LOG(ERR, "(%s) Failed to allocate TAP FDs", dev->path);
		goto err_vhostfds;
	}

	for (q = 0; q < dev->max_queue_pairs; ++q) {
		data->vhostfds[q] = -1;
		data->tapfds[q] = -1;
	}

	get_vhost_kernel_max_regions();

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		vhostfd = open(dev->path, O_RDWR);
		if (vhostfd < 0) {
			PMD_DRV_LOG(ERR, "fail to open %s, %s", dev->path, strerror(errno));
			goto err_tapfds;
		}
		data->vhostfds[i] = vhostfd;
	}

	ifname = dev->ifname != NULL ? dev->ifname : "tap%d";
	data->tapfds[0] = tap_open(ifname, (tap_features & IFF_MULTI_QUEUE) != 0);
	if (data->tapfds[0] < 0)
		goto err_tapfds;
	if (dev->ifname == NULL && tap_get_name(data->tapfds[0], &dev->ifname) < 0) {
		PMD_DRV_LOG(ERR, "fail to get tap name (%d)", data->tapfds[0]);
		goto err_tapfds;
	}
	if (tap_get_flags(data->tapfds[0], &tap_flags) < 0) {
		PMD_DRV_LOG(ERR, "fail to get tap flags for tap %s", dev->ifname);
		goto err_tapfds;
	}
	if ((tap_flags & IFF_MULTI_QUEUE) == 0 && dev->max_queue_pairs > 1) {
		PMD_DRV_LOG(ERR, "tap %s does not support multi queue", dev->ifname);
		goto err_tapfds;
	}

	for (i = 1; i < dev->max_queue_pairs; i++) {
		data->tapfds[i] = tap_open(dev->ifname, true);
		if (data->tapfds[i] < 0)
			goto err_tapfds;
	}

	dev->backend_data = data;

	return 0;

err_tapfds:
	for (i = 0; i < dev->max_queue_pairs; i++) {
		if (data->vhostfds[i] >= 0)
			close(data->vhostfds[i]);
		if (data->tapfds[i] >= 0)
			close(data->tapfds[i]);
	}

	free(data->tapfds);
err_vhostfds:
	free(data->vhostfds);
err_data:
	free(data);

	return -1;
}

static int
vhost_kernel_destroy(struct virtio_user_dev *dev)
{
	struct vhost_kernel_data *data = dev->backend_data;
	uint32_t i;

	if (!data)
		return 0;

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		if (data->vhostfds[i] >= 0)
			close(data->vhostfds[i]);
		if (data->tapfds[i] >= 0)
			close(data->tapfds[i]);
	}

	free(data->vhostfds);
	free(data->tapfds);
	free(data);
	dev->backend_data = NULL;

	return 0;
}

static int
vhost_kernel_set_backend(int vhostfd, int tapfd)
{
	struct vhost_vring_file f;

	f.fd = tapfd;
	f.index = 0;
	if (ioctl(vhostfd, VHOST_NET_SET_BACKEND, &f) < 0) {
		PMD_DRV_LOG(ERR, "VHOST_NET_SET_BACKEND fails, %s",
				strerror(errno));
		return -1;
	}

	f.index = 1;
	if (ioctl(vhostfd, VHOST_NET_SET_BACKEND, &f) < 0) {
		PMD_DRV_LOG(ERR, "VHOST_NET_SET_BACKEND fails, %s",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_kernel_enable_queue_pair(struct virtio_user_dev *dev,
			       uint16_t pair_idx,
			       int enable)
{
	struct vhost_kernel_data *data = dev->backend_data;
	int hdr_size;
	int vhostfd;
	int tapfd;

	if (dev->qp_enabled[pair_idx] == enable)
		return 0;

	vhostfd = data->vhostfds[pair_idx];
	tapfd = data->tapfds[pair_idx];

	if (!enable) {
		if (vhost_kernel_set_backend(vhostfd, -1) < 0) {
			PMD_DRV_LOG(ERR, "fail to set backend for vhost kernel");
			return -1;
		}
		dev->qp_enabled[pair_idx] = false;
		return 0;
	}

	if ((dev->features & (1ULL << VIRTIO_NET_F_MRG_RXBUF)) ||
	    (dev->features & (1ULL << VIRTIO_F_VERSION_1)))
		hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		hdr_size = sizeof(struct virtio_net_hdr);

	/* Set mac on tap only once when starting */
	if (!dev->started && pair_idx == 0 &&
			tap_set_mac(data->tapfds[pair_idx], dev->mac_addr) < 0)
		return -1;

	if (vhost_kernel_tap_setup(tapfd, hdr_size, dev->features) < 0) {
		PMD_DRV_LOG(ERR, "fail to setup tap for vhost kernel");
		return -1;
	}

	if (vhost_kernel_set_backend(vhostfd, tapfd) < 0) {
		PMD_DRV_LOG(ERR, "fail to set backend for vhost kernel");
		return -1;
	}

	dev->qp_enabled[pair_idx] = true;
	return 0;
}

static int
vhost_kernel_get_backend_features(uint64_t *features)
{
	*features = 0;

	return 0;
}

static int
vhost_kernel_update_link_state(struct virtio_user_dev *dev __rte_unused)
{
	/* Nothing to update (Maybe get TAP interface link state?) */
	return 0;
}

static int
vhost_kernel_get_intr_fd(struct virtio_user_dev *dev __rte_unused)
{
	/* No link state interrupt with Vhost-kernel */
	return -1;
}

struct virtio_user_backend_ops virtio_ops_kernel = {
	.setup = vhost_kernel_setup,
	.destroy = vhost_kernel_destroy,
	.get_backend_features = vhost_kernel_get_backend_features,
	.set_owner = vhost_kernel_set_owner,
	.get_features = vhost_kernel_get_features,
	.set_features = vhost_kernel_set_features,
	.set_memory_table = vhost_kernel_set_memory_table,
	.set_vring_num = vhost_kernel_set_vring_num,
	.set_vring_base = vhost_kernel_set_vring_base,
	.get_vring_base = vhost_kernel_get_vring_base,
	.set_vring_call = vhost_kernel_set_vring_call,
	.set_vring_kick = vhost_kernel_set_vring_kick,
	.set_vring_addr = vhost_kernel_set_vring_addr,
	.get_status = vhost_kernel_get_status,
	.set_status = vhost_kernel_set_status,
	.enable_qp = vhost_kernel_enable_queue_pair,
	.update_link_state = vhost_kernel_update_link_state,
	.get_intr_fd = vhost_kernel_get_intr_fd,
};
