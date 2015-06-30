/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

/**
 * @file
 * Interface to vhost net
 */

#include <stdint.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_net.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <rte_memory.h>
#include <rte_mempool.h>

struct rte_mbuf;

#define VHOST_MEMORY_MAX_NREGIONS 8

/* Used to indicate that the device is running on a data core */
#define VIRTIO_DEV_RUNNING 1

/* Backend value set by guest. */
#define VIRTIO_DEV_STOPPED -1


/* Enum for virtqueue management. */
enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define BUF_VECTOR_MAX 256

/**
 * Structure contains buffer address, length and descriptor index
 * from vring to do scatter RX.
 */
struct buf_vector {
	uint64_t buf_addr;
	uint32_t buf_len;
	uint32_t desc_idx;
};

/**
 * Structure contains variables relevant to RX/TX virtqueues.
 */
struct vhost_virtqueue {
	struct vring_desc	*desc;			/**< Virtqueue descriptor ring. */
	struct vring_avail	*avail;			/**< Virtqueue available ring. */
	struct vring_used	*used;			/**< Virtqueue used ring. */
	uint32_t		size;			/**< Size of descriptor ring. */
	uint32_t		backend;		/**< Backend value to determine if device should started/stopped. */
	uint16_t		vhost_hlen;		/**< Vhost header length (varies depending on RX merge buffers. */
	volatile uint16_t	last_used_idx;		/**< Last index used on the available ring */
	volatile uint16_t	last_used_idx_res;	/**< Used for multiple devices reserving buffers. */
	eventfd_t		callfd;			/**< Used to notify the guest (trigger interrupt). */
	eventfd_t		kickfd;			/**< Currently unused as polling mode is enabled. */
	struct buf_vector	buf_vec[BUF_VECTOR_MAX];	/**< for scatter RX. */
} __rte_cache_aligned;

/**
 * Device structure contains all configuration information relating to the device.
 */
struct virtio_net {
	struct vhost_virtqueue	*virtqueue[VIRTIO_QNUM];	/**< Contains all virtqueue information. */
	struct virtio_memory	*mem;		/**< QEMU memory and memory region information. */
	uint64_t		features;	/**< Negotiated feature set. */
	uint64_t		device_fh;	/**< device identifier. */
	uint32_t		flags;		/**< Device flags. Only used to check if device is running on data core. */
#define IF_NAME_SZ (PATH_MAX > IFNAMSIZ ? PATH_MAX : IFNAMSIZ)
	char			ifname[IF_NAME_SZ];	/**< Name of the tap device or socket path. */
	void			*priv;		/**< private context */
} __rte_cache_aligned;

/**
 * Information relating to memory regions including offsets to addresses in QEMUs memory file.
 */
struct virtio_memory_regions {
	uint64_t	guest_phys_address;	/**< Base guest physical address of region. */
	uint64_t	guest_phys_address_end;	/**< End guest physical address of region. */
	uint64_t	memory_size;		/**< Size of region. */
	uint64_t	userspace_address;	/**< Base userspace address of region. */
	uint64_t	address_offset;		/**< Offset of region for address translation. */
};


/**
 * Memory structure includes region and mapping information.
 */
struct virtio_memory {
	uint64_t	base_address;	/**< Base QEMU userspace address of the memory file. */
	uint64_t	mapped_address;	/**< Mapped address of memory file base in our applications memory space. */
	uint64_t	mapped_size;	/**< Total size of memory file. */
	uint32_t	nregions;	/**< Number of memory regions. */
	struct virtio_memory_regions      regions[0]; /**< Memory region information. */
};

/**
 * Device operations to add/remove device.
 *
 * Make sure to set VIRTIO_DEV_RUNNING to the device flags in new_device and
 * remove it in destroy_device.
 *
 */
struct virtio_net_device_ops {
	int (*new_device)(struct virtio_net *);	/**< Add device. */
	void (*destroy_device)(volatile struct virtio_net *);	/**< Remove device. */
};

static inline uint16_t __attribute__((always_inline))
rte_vring_available_entries(struct virtio_net *dev, uint16_t queue_id)
{
	struct vhost_virtqueue *vq = dev->virtqueue[queue_id];
	return *(volatile uint16_t *)&vq->avail->idx - vq->last_used_idx_res;
}

/**
 * Function to convert guest physical addresses to vhost virtual addresses.
 * This is used to convert guest virtio buffer addresses.
 */
static inline uint64_t __attribute__((always_inline))
gpa_to_vva(struct virtio_net *dev, uint64_t guest_pa)
{
	struct virtio_memory_regions *region;
	uint32_t regionidx;
	uint64_t vhost_va = 0;

	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		region = &dev->mem->regions[regionidx];
		if ((guest_pa >= region->guest_phys_address) &&
			(guest_pa <= region->guest_phys_address_end)) {
			vhost_va = region->address_offset + guest_pa;
			break;
		}
	}
	return vhost_va;
}

/**
 *  Disable features in feature_mask. Returns 0 on success.
 */
int rte_vhost_feature_disable(uint64_t feature_mask);

/**
 *  Enable features in feature_mask. Returns 0 on success.
 */
int rte_vhost_feature_enable(uint64_t feature_mask);

/* Returns currently supported vhost features */
uint64_t rte_vhost_feature_get(void);

int rte_vhost_enable_guest_notification(struct virtio_net *dev, uint16_t queue_id, int enable);

/* Register vhost driver. dev_name could be different for multiple instance support. */
int rte_vhost_driver_register(const char *dev_name);

/* Unregister vhost driver. This is only meaningful to vhost user. */
int rte_vhost_driver_unregister(const char *dev_name);

/* Register callbacks. */
int rte_vhost_driver_callback_register(struct virtio_net_device_ops const * const);
/* Start vhost driver session blocking loop. */
int rte_vhost_driver_session_start(void);

/**
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtual device. A packet
 * count is returned to indicate the number of packets that were succesfully
 * added to the RX queue.
 * @param dev
 *  virtio-net device
 * @param queue_id
 *  virtio queue index in mq case
 * @param pkts
 *  array to contain packets to be enqueued
 * @param count
 *  packets num to be enqueued
 * @return
 *  num of packets enqueued
 */
uint16_t rte_vhost_enqueue_burst(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count);

/**
 * This function gets guest buffers from the virtio device TX virtqueue,
 * construct host mbufs, copies guest buffer content to host mbufs and
 * store them in pkts to be processed.
 * @param dev
 *  virtio-net device
 * @param queue_id
 *  virtio queue index in mq case
 * @param mbuf_pool
 *  mbuf_pool where host mbuf is allocated.
 * @param pkts
 *  array to contain packets to be dequeued
 * @param count
 *  packets num to be dequeued
 * @return
 *  num of packets dequeued
 */
uint16_t rte_vhost_dequeue_burst(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count);

#endif /* _VIRTIO_NET_H_ */
