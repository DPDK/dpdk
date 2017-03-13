/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_memory.h>
#include <rte_eal_memconfig.h>

#include "vhost.h"
#include "virtio_user_dev.h"
#include "vhost_kernel_tap.h"

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

static uint64_t vhost_req_user_to_kernel[] = {
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
};

/* By default, vhost kernel module allows 64 regions, but DPDK allows
 * 256 segments. As a relief, below function merges those virtually
 * adjacent memsegs into one region.
 */
static struct vhost_memory_kernel *
prepare_vhost_memory_kernel(void)
{
	uint32_t i, j, k = 0;
	struct rte_memseg *seg;
	struct vhost_memory_region *mr;
	struct vhost_memory_kernel *vm;

	vm = malloc(sizeof(struct vhost_memory_kernel) +
		    max_regions *
		    sizeof(struct vhost_memory_region));
	if (!vm)
		return NULL;

	for (i = 0; i < RTE_MAX_MEMSEG; ++i) {
		seg = &rte_eal_get_configuration()->mem_config->memseg[i];
		if (!seg->addr)
			break;

		int new_region = 1;

		for (j = 0; j < k; ++j) {
			mr = &vm->regions[j];

			if (mr->userspace_addr + mr->memory_size ==
			    (uint64_t)(uintptr_t)seg->addr) {
				mr->memory_size += seg->len;
				new_region = 0;
				break;
			}

			if ((uint64_t)(uintptr_t)seg->addr + seg->len ==
			    mr->userspace_addr) {
				mr->guest_phys_addr =
					(uint64_t)(uintptr_t)seg->addr;
				mr->userspace_addr =
					(uint64_t)(uintptr_t)seg->addr;
				mr->memory_size += seg->len;
				new_region = 0;
				break;
			}
		}

		if (new_region == 0)
			continue;

		mr = &vm->regions[k++];
		/* use vaddr here! */
		mr->guest_phys_addr = (uint64_t)(uintptr_t)seg->addr;
		mr->userspace_addr = (uint64_t)(uintptr_t)seg->addr;
		mr->memory_size = seg->len;
		mr->mmap_offset = 0;

		if (k >= max_regions) {
			free(vm);
			return NULL;
		}
	}

	vm->nregions = k;
	vm->padding = 0;
	return vm;
}

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

static int
tap_supporte_mq(void)
{
	int tapfd;
	unsigned int tap_features;

	tapfd = open(PATH_NET_TUN, O_RDWR);
	if (tapfd < 0) {
		PMD_DRV_LOG(ERR, "fail to open %s: %s",
			    PATH_NET_TUN, strerror(errno));
		return -1;
	}

	if (ioctl(tapfd, TUNGETFEATURES, &tap_features) == -1) {
		PMD_DRV_LOG(ERR, "TUNGETFEATURES failed: %s", strerror(errno));
		close(tapfd);
		return -1;
	}

	close(tapfd);
	return tap_features & IFF_MULTI_QUEUE;
}

static int
vhost_kernel_ioctl(struct virtio_user_dev *dev,
		   enum vhost_user_request req,
		   void *arg)
{
	int ret = -1;
	unsigned int i;
	uint64_t req_kernel;
	struct vhost_memory_kernel *vm = NULL;
	int vhostfd;
	unsigned int queue_sel;

	PMD_DRV_LOG(INFO, "%s", vhost_msg_strings[req]);

	req_kernel = vhost_req_user_to_kernel[req];

	if (req_kernel == VHOST_SET_MEM_TABLE) {
		vm = prepare_vhost_memory_kernel();
		if (!vm)
			return -1;
		arg = (void *)vm;
	}

	if (req_kernel == VHOST_SET_FEATURES) {
		/* We don't need memory protection here */
		*(uint64_t *)arg &= ~(1ULL << VIRTIO_F_IOMMU_PLATFORM);

		/* VHOST kernel does not know about below flags */
		*(uint64_t *)arg &= ~VHOST_KERNEL_GUEST_OFFLOADS_MASK;
		*(uint64_t *)arg &= ~VHOST_KERNEL_HOST_OFFLOADS_MASK;

		*(uint64_t *)arg &= ~(1ULL << VIRTIO_NET_F_MQ);
	}

	switch (req_kernel) {
	case VHOST_SET_VRING_NUM:
	case VHOST_SET_VRING_ADDR:
	case VHOST_SET_VRING_BASE:
	case VHOST_GET_VRING_BASE:
	case VHOST_SET_VRING_KICK:
	case VHOST_SET_VRING_CALL:
		queue_sel = *(unsigned int *)arg;
		vhostfd = dev->vhostfds[queue_sel / 2];
		*(unsigned int *)arg = queue_sel % 2;
		PMD_DRV_LOG(DEBUG, "vhostfd=%d, index=%u",
			    vhostfd, *(unsigned int *)arg);
		break;
	default:
		vhostfd = -1;
	}
	if (vhostfd == -1) {
		for (i = 0; i < dev->max_queue_pairs; ++i) {
			if (dev->vhostfds[i] < 0)
				continue;

			ret = ioctl(dev->vhostfds[i], req_kernel, arg);
			if (ret < 0)
				break;
		}
	} else {
		ret = ioctl(vhostfd, req_kernel, arg);
	}

	if (!ret && req_kernel == VHOST_GET_FEATURES) {
		/* with tap as the backend, all these features are supported
		 * but not claimed by vhost-net, so we add them back when
		 * reporting to upper layer.
		 */
		*((uint64_t *)arg) |= VHOST_KERNEL_GUEST_OFFLOADS_MASK;
		*((uint64_t *)arg) |= VHOST_KERNEL_HOST_OFFLOADS_MASK;

		/* vhost_kernel will not declare this feature, but it does
		 * support multi-queue.
		 */
		if (tap_supporte_mq())
			*(uint64_t *)arg |= (1ull << VIRTIO_NET_F_MQ);
	}

	if (vm)
		free(vm);

	if (ret < 0)
		PMD_DRV_LOG(ERR, "%s failed: %s",
			    vhost_msg_strings[req], strerror(errno));

	return ret;
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
	int vhostfd;
	uint32_t i;

	get_vhost_kernel_max_regions();

	for (i = 0; i < dev->max_queue_pairs; ++i) {
		vhostfd = open(dev->path, O_RDWR);
		if (vhostfd < 0) {
			PMD_DRV_LOG(ERR, "fail to open %s, %s",
				    dev->path, strerror(errno));
			return -1;
		}

		dev->vhostfds[i] = vhostfd;
	}

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
	int hdr_size;
	int vhostfd;
	int tapfd;
	int req_mq = (dev->max_queue_pairs > 1);

	vhostfd = dev->vhostfds[pair_idx];

	if (!enable) {
		if (dev->tapfds[pair_idx] >= 0) {
			close(dev->tapfds[pair_idx]);
			dev->tapfds[pair_idx] = -1;
		}
		return vhost_kernel_set_backend(vhostfd, -1);
	} else if (dev->tapfds[pair_idx] >= 0) {
		return 0;
	}

	if ((dev->features & (1ULL << VIRTIO_NET_F_MRG_RXBUF)) ||
	    (dev->features & (1ULL << VIRTIO_F_VERSION_1)))
		hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		hdr_size = sizeof(struct virtio_net_hdr);

	tapfd = vhost_kernel_open_tap(&dev->ifname, hdr_size, req_mq);
	if (tapfd < 0) {
		PMD_DRV_LOG(ERR, "fail to open tap for vhost kernel");
		return -1;
	}

	if (vhost_kernel_set_backend(vhostfd, tapfd) < 0) {
		PMD_DRV_LOG(ERR, "fail to set backend for vhost kernel");
		close(tapfd);
		return -1;
	}

	dev->tapfds[pair_idx] = tapfd;
	return 0;
}

struct virtio_user_backend_ops ops_kernel = {
	.setup = vhost_kernel_setup,
	.send_request = vhost_kernel_ioctl,
	.enable_qp = vhost_kernel_enable_queue_pair
};
