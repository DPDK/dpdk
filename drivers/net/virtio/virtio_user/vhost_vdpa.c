/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Red Hat Inc.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_memory.h>

#include "vhost.h"
#include "virtio_user_dev.h"

struct vhost_vdpa_data {
	int vhostfd;
	uint64_t protocol_features;
};

#define VHOST_VDPA_SUPPORTED_BACKEND_FEATURES		\
	(1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2	|	\
	1ULL << VHOST_BACKEND_F_IOTLB_BATCH)

/* vhost kernel & vdpa ioctls */
#define VHOST_VIRTIO 0xAF
#define VHOST_GET_FEATURES _IOR(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_FEATURES _IOW(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_OWNER _IO(VHOST_VIRTIO, 0x01)
#define VHOST_RESET_OWNER _IO(VHOST_VIRTIO, 0x02)
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
#define VHOST_VDPA_GET_DEVICE_ID _IOR(VHOST_VIRTIO, 0x70, __u32)
#define VHOST_VDPA_GET_STATUS _IOR(VHOST_VIRTIO, 0x71, __u8)
#define VHOST_VDPA_SET_STATUS _IOW(VHOST_VIRTIO, 0x72, __u8)
#define VHOST_VDPA_GET_CONFIG _IOR(VHOST_VIRTIO, 0x73, struct vhost_vdpa_config)
#define VHOST_VDPA_SET_CONFIG _IOW(VHOST_VIRTIO, 0x74, struct vhost_vdpa_config)
#define VHOST_VDPA_SET_VRING_ENABLE _IOW(VHOST_VIRTIO, 0x75, struct vhost_vring_state)
#define VHOST_SET_BACKEND_FEATURES _IOW(VHOST_VIRTIO, 0x25, __u64)
#define VHOST_GET_BACKEND_FEATURES _IOR(VHOST_VIRTIO, 0x26, __u64)

/* no alignment requirement */
struct vhost_iotlb_msg {
	uint64_t iova;
	uint64_t size;
	uint64_t uaddr;
#define VHOST_ACCESS_RO      0x1
#define VHOST_ACCESS_WO      0x2
#define VHOST_ACCESS_RW      0x3
	uint8_t perm;
#define VHOST_IOTLB_MISS           1
#define VHOST_IOTLB_UPDATE         2
#define VHOST_IOTLB_INVALIDATE     3
#define VHOST_IOTLB_ACCESS_FAIL    4
#define VHOST_IOTLB_BATCH_BEGIN    5
#define VHOST_IOTLB_BATCH_END      6
	uint8_t type;
};

#define VHOST_IOTLB_MSG_V2 0x2

struct vhost_vdpa_config {
	uint32_t off;
	uint32_t len;
	uint8_t buf[];
};

struct vhost_msg {
	uint32_t type;
	uint32_t reserved;
	union {
		struct vhost_iotlb_msg iotlb;
		uint8_t padding[64];
	};
};


static int
vhost_vdpa_ioctl(int fd, uint64_t request, void *arg)
{
	int ret;

	ret = ioctl(fd, request, arg);
	if (ret) {
		PMD_DRV_LOG(ERR, "Vhost-vDPA ioctl %"PRIu64" failed (%s)",
				request, strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_set_owner(struct virtio_user_dev *dev)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_OWNER, NULL);
}

static int
vhost_vdpa_get_protocol_features(struct virtio_user_dev *dev, uint64_t *features)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_GET_BACKEND_FEATURES, features);
}

static int
vhost_vdpa_set_protocol_features(struct virtio_user_dev *dev, uint64_t features)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_BACKEND_FEATURES, &features);
}

static int
vhost_vdpa_get_features(struct virtio_user_dev *dev, uint64_t *features)
{
	struct vhost_vdpa_data *data = dev->backend_data;
	int ret;

	ret = vhost_vdpa_ioctl(data->vhostfd, VHOST_GET_FEATURES, features);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get features");
		return -1;
	}

	if (*features & 1ULL << VIRTIO_NET_F_CTRL_VQ)
		dev->hw_cvq = true;

	/* Negotiated vDPA backend features */
	ret = vhost_vdpa_get_protocol_features(dev, &data->protocol_features);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to get backend features");
		return -1;
	}

	data->protocol_features &= VHOST_VDPA_SUPPORTED_BACKEND_FEATURES;

	ret = vhost_vdpa_set_protocol_features(dev, data->protocol_features);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set backend features");
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_set_features(struct virtio_user_dev *dev, uint64_t features)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	/* WORKAROUND */
	features |= 1ULL << VIRTIO_F_IOMMU_PLATFORM;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_FEATURES, &features);
}

static int
vhost_vdpa_iotlb_batch_begin(struct virtio_user_dev *dev)
{
	struct vhost_vdpa_data *data = dev->backend_data;
	struct vhost_msg msg = {};

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_BATCH)))
		return 0;

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_BATCH_BEGIN;

	if (write(data->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
		PMD_DRV_LOG(ERR, "Failed to send IOTLB batch begin (%s)",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_iotlb_batch_end(struct virtio_user_dev *dev)
{
	struct vhost_vdpa_data *data = dev->backend_data;
	struct vhost_msg msg = {};

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_BATCH)))
		return 0;

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_BATCH_END;

	if (write(data->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
		PMD_DRV_LOG(ERR, "Failed to send IOTLB batch end (%s)",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_dma_map(struct virtio_user_dev *dev, void *addr,
				  uint64_t iova, size_t len)
{
	struct vhost_vdpa_data *data = dev->backend_data;
	struct vhost_msg msg = {};

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_UPDATE;
	msg.iotlb.iova = iova;
	msg.iotlb.uaddr = (uint64_t)(uintptr_t)addr;
	msg.iotlb.size = len;
	msg.iotlb.perm = VHOST_ACCESS_RW;

	PMD_DRV_LOG(DEBUG, "%s: iova: 0x%" PRIx64 ", addr: %p, len: 0x%zx",
			__func__, iova, addr, len);

	if (write(data->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
		PMD_DRV_LOG(ERR, "Failed to send IOTLB update (%s)",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_dma_unmap(struct virtio_user_dev *dev, __rte_unused void *addr,
				  uint64_t iova, size_t len)
{
	struct vhost_vdpa_data *data = dev->backend_data;
	struct vhost_msg msg = {};

	if (!(data->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_INVALIDATE;
	msg.iotlb.iova = iova;
	msg.iotlb.size = len;

	PMD_DRV_LOG(DEBUG, "%s: iova: 0x%" PRIx64 ", len: 0x%zx",
			__func__, iova, len);

	if (write(data->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
		PMD_DRV_LOG(ERR, "Failed to send IOTLB invalidate (%s)",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_dma_map_batch(struct virtio_user_dev *dev, void *addr,
				  uint64_t iova, size_t len)
{
	int ret;

	if (vhost_vdpa_iotlb_batch_begin(dev) < 0)
		return -1;

	ret = vhost_vdpa_dma_map(dev, addr, iova, len);

	if (vhost_vdpa_iotlb_batch_end(dev) < 0)
		return -1;

	return ret;
}

static int
vhost_vdpa_dma_unmap_batch(struct virtio_user_dev *dev, void *addr,
				  uint64_t iova, size_t len)
{
	int ret;

	if (vhost_vdpa_iotlb_batch_begin(dev) < 0)
		return -1;

	ret = vhost_vdpa_dma_unmap(dev, addr, iova, len);

	if (vhost_vdpa_iotlb_batch_end(dev) < 0)
		return -1;

	return ret;
}

static int
vhost_vdpa_map_contig(const struct rte_memseg_list *msl,
		const struct rte_memseg *ms, size_t len, void *arg)
{
	struct virtio_user_dev *dev = arg;

	if (msl->external)
		return 0;

	return vhost_vdpa_dma_map(dev, ms->addr, ms->iova, len);
}

static int
vhost_vdpa_map(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg)
{
	struct virtio_user_dev *dev = arg;

	/* skip external memory that isn't a heap */
	if (msl->external && !msl->heap)
		return 0;

	/* skip any segments with invalid IOVA addresses */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	/* if IOVA mode is VA, we've already mapped the internal segments */
	if (!msl->external && rte_eal_iova_mode() == RTE_IOVA_VA)
		return 0;

	return vhost_vdpa_dma_map(dev, ms->addr, ms->iova, ms->len);
}

static int
vhost_vdpa_set_memory_table(struct virtio_user_dev *dev)
{
	int ret;

	if (vhost_vdpa_iotlb_batch_begin(dev) < 0)
		return -1;

	vhost_vdpa_dma_unmap(dev, NULL, 0, SIZE_MAX);

	if (rte_eal_iova_mode() == RTE_IOVA_VA) {
		/* with IOVA as VA mode, we can get away with mapping contiguous
		 * chunks rather than going page-by-page.
		 */
		ret = rte_memseg_contig_walk_thread_unsafe(
				vhost_vdpa_map_contig, dev);
		if (ret)
			goto batch_end;
		/* we have to continue the walk because we've skipped the
		 * external segments during the config walk.
		 */
	}
	ret = rte_memseg_walk_thread_unsafe(vhost_vdpa_map, dev);

batch_end:
	if (vhost_vdpa_iotlb_batch_end(dev) < 0)
		return -1;

	return ret;
}

static int
vhost_vdpa_set_vring_enable(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_VDPA_SET_VRING_ENABLE, state);
}

static int
vhost_vdpa_set_vring_num(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_VRING_NUM, state);
}

static int
vhost_vdpa_set_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_VRING_BASE, state);
}

static int
vhost_vdpa_get_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_GET_VRING_BASE, state);
}

static int
vhost_vdpa_set_vring_call(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_VRING_CALL, file);
}

static int
vhost_vdpa_set_vring_kick(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_VRING_KICK, file);
}

static int
vhost_vdpa_set_vring_addr(struct virtio_user_dev *dev, struct vhost_vring_addr *addr)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_SET_VRING_ADDR, addr);
}

static int
vhost_vdpa_get_status(struct virtio_user_dev *dev, uint8_t *status)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_VDPA_GET_STATUS, status);
}

static int
vhost_vdpa_set_status(struct virtio_user_dev *dev, uint8_t status)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	return vhost_vdpa_ioctl(data->vhostfd, VHOST_VDPA_SET_STATUS, &status);
}

static int
vhost_vdpa_get_config(struct virtio_user_dev *dev, uint8_t *data, uint32_t off, uint32_t len)
{
	struct vhost_vdpa_data *vdpa_data = dev->backend_data;
	struct vhost_vdpa_config *config;
	int ret = 0;

	config = malloc(sizeof(*config) + len);
	if (!config) {
		PMD_DRV_LOG(ERR, "Failed to allocate vDPA config data");
		return -1;
	}

	config->off = off;
	config->len = len;

	ret = vhost_vdpa_ioctl(vdpa_data->vhostfd, VHOST_VDPA_GET_CONFIG, config);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get vDPA config (offset 0x%x, len 0x%x)", off, len);
		ret = -1;
		goto out;
	}

	memcpy(data, config->buf, len);
out:
	free(config);

	return ret;
}

static int
vhost_vdpa_set_config(struct virtio_user_dev *dev, const uint8_t *data, uint32_t off, uint32_t len)
{
	struct vhost_vdpa_data *vdpa_data = dev->backend_data;
	struct vhost_vdpa_config *config;
	int ret = 0;

	config = malloc(sizeof(*config) + len);
	if (!config) {
		PMD_DRV_LOG(ERR, "Failed to allocate vDPA config data");
		return -1;
	}

	config->off = off;
	config->len = len;

	memcpy(config->buf, data, len);

	ret = vhost_vdpa_ioctl(vdpa_data->vhostfd, VHOST_VDPA_SET_CONFIG, config);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to set vDPA config (offset 0x%x, len 0x%x)", off, len);
		ret = -1;
	}

	free(config);

	return ret;
}

/**
 * Set up environment to talk with a vhost vdpa backend.
 *
 * @return
 *   - (-1) if fail to set up;
 *   - (>=0) if successful.
 */
static int
vhost_vdpa_setup(struct virtio_user_dev *dev)
{
	struct vhost_vdpa_data *data;
	uint32_t did = (uint32_t)-1;

	data = malloc(sizeof(*data));
	if (!data) {
		PMD_DRV_LOG(ERR, "(%s) Faidle to allocate backend data", dev->path);
		return -1;
	}

	data->vhostfd = open(dev->path, O_RDWR);
	if (data->vhostfd < 0) {
		PMD_DRV_LOG(ERR, "Failed to open %s: %s",
				dev->path, strerror(errno));
		free(data);
		return -1;
	}

	if (ioctl(data->vhostfd, VHOST_VDPA_GET_DEVICE_ID, &did) < 0 ||
			did != VIRTIO_ID_NETWORK) {
		PMD_DRV_LOG(ERR, "Invalid vdpa device ID: %u", did);
		close(data->vhostfd);
		free(data);
		return -1;
	}

	dev->backend_data = data;

	return 0;
}

static int
vhost_vdpa_destroy(struct virtio_user_dev *dev)
{
	struct vhost_vdpa_data *data = dev->backend_data;

	if (!data)
		return 0;

	close(data->vhostfd);

	free(data);
	dev->backend_data = NULL;

	return 0;
}

static int
vhost_vdpa_cvq_enable(struct virtio_user_dev *dev, int enable)
{
	struct vhost_vring_state state = {
		.index = dev->max_queue_pairs * 2,
		.num   = enable,
	};

	return vhost_vdpa_set_vring_enable(dev, &state);
}

static int
vhost_vdpa_enable_queue_pair(struct virtio_user_dev *dev,
			       uint16_t pair_idx,
			       int enable)
{
	int i;

	if (dev->qp_enabled[pair_idx] == enable)
		return 0;

	for (i = 0; i < 2; ++i) {
		struct vhost_vring_state state = {
			.index = pair_idx * 2 + i,
			.num   = enable,
		};

		if (vhost_vdpa_set_vring_enable(dev, &state))
			return -1;
	}

	dev->qp_enabled[pair_idx] = enable;

	return 0;
}

static int
vhost_vdpa_get_backend_features(uint64_t *features)
{
	*features = 0;

	return 0;
}

static int
vhost_vdpa_update_link_state(struct virtio_user_dev *dev __rte_unused)
{
	/* Nothing to update (for now?) */
	return 0;
}

static int
vhost_vdpa_get_intr_fd(struct virtio_user_dev *dev __rte_unused)
{
	/* No link state interrupt with Vhost-vDPA */
	return -1;
}

struct virtio_user_backend_ops virtio_ops_vdpa = {
	.setup = vhost_vdpa_setup,
	.destroy = vhost_vdpa_destroy,
	.get_backend_features = vhost_vdpa_get_backend_features,
	.set_owner = vhost_vdpa_set_owner,
	.get_features = vhost_vdpa_get_features,
	.set_features = vhost_vdpa_set_features,
	.set_memory_table = vhost_vdpa_set_memory_table,
	.set_vring_num = vhost_vdpa_set_vring_num,
	.set_vring_base = vhost_vdpa_set_vring_base,
	.get_vring_base = vhost_vdpa_get_vring_base,
	.set_vring_call = vhost_vdpa_set_vring_call,
	.set_vring_kick = vhost_vdpa_set_vring_kick,
	.set_vring_addr = vhost_vdpa_set_vring_addr,
	.get_status = vhost_vdpa_get_status,
	.set_status = vhost_vdpa_set_status,
	.get_config = vhost_vdpa_get_config,
	.set_config = vhost_vdpa_set_config,
	.cvq_enable = vhost_vdpa_cvq_enable,
	.enable_qp = vhost_vdpa_enable_queue_pair,
	.dma_map = vhost_vdpa_dma_map_batch,
	.dma_unmap = vhost_vdpa_dma_unmap_batch,
	.update_link_state = vhost_vdpa_update_link_state,
	.get_intr_fd = vhost_vdpa_get_intr_fd,
};
