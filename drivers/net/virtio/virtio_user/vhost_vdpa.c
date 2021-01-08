/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Red Hat Inc.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_memory.h>

#include "vhost.h"
#include "virtio_user_dev.h"

/* vhost kernel & vdpa ioctls */
#define VHOST_VIRTIO 0xAF
#define VHOST_GET_FEATURES _IOR(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_FEATURES _IOW(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_OWNER _IO(VHOST_VIRTIO, 0x01)
#define VHOST_RESET_OWNER _IO(VHOST_VIRTIO, 0x02)
#define VHOST_SET_MEM_TABLE _IOW(VHOST_VIRTIO, 0x03, void *)
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
#define VHOST_VDPA_SET_VRING_ENABLE	_IOW(VHOST_VIRTIO, 0x75, \
					     struct vhost_vring_state)
#define VHOST_SET_BACKEND_FEATURES _IOW(VHOST_VIRTIO, 0x25, __u64)
#define VHOST_GET_BACKEND_FEATURES _IOR(VHOST_VIRTIO, 0x26, __u64)

static uint64_t vhost_req_user_to_vdpa[] = {
	[VHOST_USER_SET_OWNER] = VHOST_SET_OWNER,
	[VHOST_USER_RESET_OWNER] = VHOST_RESET_OWNER,
	[VHOST_USER_SET_FEATURES] = VHOST_SET_FEATURES,
	[VHOST_USER_GET_FEATURES] = VHOST_GET_FEATURES,
	[VHOST_USER_SET_VRING_CALL] = VHOST_SET_VRING_CALL,
	[VHOST_USER_SET_VRING_NUM] = VHOST_SET_VRING_NUM,
	[VHOST_USER_SET_VRING_BASE] = VHOST_SET_VRING_BASE,
	[VHOST_USER_GET_VRING_BASE] = VHOST_GET_VRING_BASE,
	[VHOST_USER_SET_VRING_ADDR] = VHOST_SET_VRING_ADDR,
	[VHOST_USER_SET_VRING_KICK] = VHOST_SET_VRING_KICK,
	[VHOST_USER_SET_MEM_TABLE] = VHOST_SET_MEM_TABLE,
	[VHOST_USER_SET_STATUS] = VHOST_VDPA_SET_STATUS,
	[VHOST_USER_GET_STATUS] = VHOST_VDPA_GET_STATUS,
	[VHOST_USER_SET_VRING_ENABLE] = VHOST_VDPA_SET_VRING_ENABLE,
	[VHOST_USER_GET_PROTOCOL_FEATURES] = VHOST_GET_BACKEND_FEATURES,
	[VHOST_USER_SET_PROTOCOL_FEATURES] = VHOST_SET_BACKEND_FEATURES,
};

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

struct vhost_msg {
	uint32_t type;
	uint32_t reserved;
	union {
		struct vhost_iotlb_msg iotlb;
		uint8_t padding[64];
	};
};

static int
vhost_vdpa_iotlb_batch_begin(struct virtio_user_dev *dev)
{
	struct vhost_msg msg = {};

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_BATCH)))
		return 0;

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_BATCH_BEGIN;

	if (write(dev->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
		PMD_DRV_LOG(ERR, "Failed to send IOTLB batch begin (%s)",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_vdpa_iotlb_batch_end(struct virtio_user_dev *dev)
{
	struct vhost_msg msg = {};

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_BATCH)))
		return 0;

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_BATCH_END;

	if (write(dev->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
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
	struct vhost_msg msg = {};

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_UPDATE;
	msg.iotlb.iova = iova;
	msg.iotlb.uaddr = (uint64_t)(uintptr_t)addr;
	msg.iotlb.size = len;
	msg.iotlb.perm = VHOST_ACCESS_RW;

	if (write(dev->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
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
	struct vhost_msg msg = {};

	if (!(dev->protocol_features & (1ULL << VHOST_BACKEND_F_IOTLB_MSG_V2))) {
		PMD_DRV_LOG(ERR, "IOTLB_MSG_V2 not supported by the backend.");
		return -1;
	}

	msg.type = VHOST_IOTLB_MSG_V2;
	msg.iotlb.type = VHOST_IOTLB_INVALIDATE;
	msg.iotlb.iova = iova;
	msg.iotlb.size = len;

	if (write(dev->vhostfd, &msg, sizeof(msg)) != sizeof(msg)) {
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
vhost_vdpa_dma_map_all(struct virtio_user_dev *dev)
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

/* with below features, vhost vdpa does not need to do the checksum and TSO,
 * these info will be passed to virtio_user through virtio net header.
 */
#define VHOST_VDPA_GUEST_OFFLOADS_MASK	\
	((1ULL << VIRTIO_NET_F_GUEST_CSUM) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_TSO4) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_TSO6) |	\
	 (1ULL << VIRTIO_NET_F_GUEST_ECN)  |	\
	 (1ULL << VIRTIO_NET_F_GUEST_UFO))

#define VHOST_VDPA_HOST_OFFLOADS_MASK		\
	((1ULL << VIRTIO_NET_F_HOST_TSO4) |	\
	 (1ULL << VIRTIO_NET_F_HOST_TSO6) |	\
	 (1ULL << VIRTIO_NET_F_CSUM))

static int
vhost_vdpa_ioctl(struct virtio_user_dev *dev,
		   enum vhost_user_request req,
		   void *arg)
{
	int ret = -1;
	uint64_t req_vdpa;

	PMD_DRV_LOG(INFO, "%s", vhost_msg_strings[req]);

	req_vdpa = vhost_req_user_to_vdpa[req];

	if (req_vdpa == VHOST_SET_MEM_TABLE)
		return vhost_vdpa_dma_map_all(dev);

	if (req_vdpa == VHOST_SET_FEATURES) {
		/* WORKAROUND */
		*(uint64_t *)arg |= 1ULL << VIRTIO_F_IOMMU_PLATFORM;

		/* Multiqueue not supported for now */
		*(uint64_t *)arg &= ~(1ULL << VIRTIO_NET_F_MQ);
	}

	switch (req_vdpa) {
	case VHOST_SET_VRING_NUM:
	case VHOST_SET_VRING_ADDR:
	case VHOST_SET_VRING_BASE:
	case VHOST_GET_VRING_BASE:
	case VHOST_SET_VRING_KICK:
	case VHOST_SET_VRING_CALL:
		PMD_DRV_LOG(DEBUG, "vhostfd=%d, index=%u",
			    dev->vhostfd, *(unsigned int *)arg);
		break;
	default:
		break;
	}

	ret = ioctl(dev->vhostfd, req_vdpa, arg);
	if (ret < 0)
		PMD_DRV_LOG(ERR, "%s failed: %s",
			    vhost_msg_strings[req], strerror(errno));

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
	uint32_t did = (uint32_t)-1;

	dev->vhostfd = open(dev->path, O_RDWR);
	if (dev->vhostfd < 0) {
		PMD_DRV_LOG(ERR, "Failed to open %s: %s\n",
				dev->path, strerror(errno));
		return -1;
	}

	if (ioctl(dev->vhostfd, VHOST_VDPA_GET_DEVICE_ID, &did) < 0 ||
			did != VIRTIO_ID_NETWORK) {
		PMD_DRV_LOG(ERR, "Invalid vdpa device ID: %u\n", did);
		return -1;
	}

	return 0;
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

		if (vhost_vdpa_ioctl(dev, VHOST_USER_SET_VRING_ENABLE, &state))
			return -1;
	}

	dev->qp_enabled[pair_idx] = enable;

	return 0;
}

struct virtio_user_backend_ops virtio_ops_vdpa = {
	.setup = vhost_vdpa_setup,
	.send_request = vhost_vdpa_ioctl,
	.enable_qp = vhost_vdpa_enable_queue_pair,
	.dma_map = vhost_vdpa_dma_map_batch,
	.dma_unmap = vhost_vdpa_dma_unmap_batch,
};
