/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _RTE_VHOST_H_
#define _RTE_VHOST_H_

/**
 * @file
 * Interface to vhost-user
 */

#include <stdint.h>
#include <sys/eventfd.h>

#include <rte_memory.h>
#include <rte_mempool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* These are not C++-aware. */
#include <linux/vhost.h>
#include <linux/virtio_ring.h>

#define RTE_VHOST_USER_CLIENT		(1ULL << 0)
#define RTE_VHOST_USER_NO_RECONNECT	(1ULL << 1)
#define RTE_VHOST_USER_DEQUEUE_ZERO_COPY	(1ULL << 2)
#define RTE_VHOST_USER_IOMMU_SUPPORT	(1ULL << 3)

/** Protocol features. */
#ifndef VHOST_USER_PROTOCOL_F_MQ
#define VHOST_USER_PROTOCOL_F_MQ	0
#endif

#ifndef VHOST_USER_PROTOCOL_F_LOG_SHMFD
#define VHOST_USER_PROTOCOL_F_LOG_SHMFD	1
#endif

#ifndef VHOST_USER_PROTOCOL_F_RARP
#define VHOST_USER_PROTOCOL_F_RARP	2
#endif

#ifndef VHOST_USER_PROTOCOL_F_REPLY_ACK
#define VHOST_USER_PROTOCOL_F_REPLY_ACK	3
#endif

#ifndef VHOST_USER_PROTOCOL_F_NET_MTU
#define VHOST_USER_PROTOCOL_F_NET_MTU	4
#endif

#ifndef VHOST_USER_PROTOCOL_F_SLAVE_REQ
#define VHOST_USER_PROTOCOL_F_SLAVE_REQ	5
#endif

#ifndef VHOST_USER_PROTOCOL_F_CRYPTO_SESSION
#define VHOST_USER_PROTOCOL_F_CRYPTO_SESSION 7
#endif

/** Indicate whether protocol features negotiation is supported. */
#ifndef VHOST_USER_F_PROTOCOL_FEATURES
#define VHOST_USER_F_PROTOCOL_FEATURES	30
#endif

/**
 * Information relating to memory regions including offsets to
 * addresses in QEMUs memory file.
 */
struct rte_vhost_mem_region {
	uint64_t guest_phys_addr;
	uint64_t guest_user_addr;
	uint64_t host_user_addr;
	uint64_t size;
	void	 *mmap_addr;
	uint64_t mmap_size;
	int fd;
};

/**
 * Memory structure includes region and mapping information.
 */
struct rte_vhost_memory {
	uint32_t nregions;
	struct rte_vhost_mem_region regions[];
};

struct rte_vhost_vring {
	struct vring_desc	*desc;
	struct vring_avail	*avail;
	struct vring_used	*used;
	uint64_t		log_guest_addr;

	/** Deprecated, use rte_vhost_vring_call() instead. */
	int			callfd;

	int			kickfd;
	uint16_t		size;
};

/**
 * Device and vring operations.
 */
struct vhost_device_ops {
	int (*new_device)(int vid);		/**< Add device. */
	void (*destroy_device)(int vid);	/**< Remove device. */

	int (*vring_state_changed)(int vid, uint16_t queue_id, int enable);	/**< triggered when a vring is enabled or disabled */

	/**
	 * Features could be changed after the feature negotiation.
	 * For example, VHOST_F_LOG_ALL will be set/cleared at the
	 * start/end of live migration, respectively. This callback
	 * is used to inform the application on such change.
	 */
	int (*features_changed)(int vid, uint64_t features);

	int (*new_connection)(int vid);
	void (*destroy_connection)(int vid);

	void *reserved[2]; /**< Reserved for future extension */
};

/**
 * Convert guest physical address to host virtual address
 *
 * This function is deprecated because unsafe.
 * New rte_vhost_va_from_guest_pa() should be used instead to ensure
 * guest physical ranges are fully and contiguously mapped into
 * process virtual address space.
 *
 * @param mem
 *  the guest memory regions
 * @param gpa
 *  the guest physical address for querying
 * @return
 *  the host virtual address on success, 0 on failure
 */
__rte_deprecated
static __rte_always_inline uint64_t
rte_vhost_gpa_to_vva(struct rte_vhost_memory *mem, uint64_t gpa)
{
	struct rte_vhost_mem_region *reg;
	uint32_t i;

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];
		if (gpa >= reg->guest_phys_addr &&
		    gpa <  reg->guest_phys_addr + reg->size) {
			return gpa - reg->guest_phys_addr +
			       reg->host_user_addr;
		}
	}

	return 0;
}

/**
 * Convert guest physical address to host virtual address safely
 *
 * This variant of rte_vhost_gpa_to_vva() takes care all the
 * requested length is mapped and contiguous in process address
 * space.
 *
 * @param mem
 *  the guest memory regions
 * @param gpa
 *  the guest physical address for querying
 * @param len
 *  the size of the requested area to map, updated with actual size mapped
 * @return
 *  the host virtual address on success, 0 on failure
 */
static __rte_always_inline uint64_t
rte_vhost_va_from_guest_pa(struct rte_vhost_memory *mem,
						   uint64_t gpa, uint64_t *len)
{
	struct rte_vhost_mem_region *r;
	uint32_t i;

	for (i = 0; i < mem->nregions; i++) {
		r = &mem->regions[i];
		if (gpa >= r->guest_phys_addr &&
		    gpa <  r->guest_phys_addr + r->size) {

			if (unlikely(*len > r->guest_phys_addr + r->size - gpa))
				*len = r->guest_phys_addr + r->size - gpa;

			return gpa - r->guest_phys_addr +
			       r->host_user_addr;
		}
	}
	*len = 0;

	return 0;
}

#define RTE_VHOST_NEED_LOG(features)	((features) & (1ULL << VHOST_F_LOG_ALL))

/**
 * Log the memory write start with given address.
 *
 * This function only need be invoked when the live migration starts.
 * Therefore, we won't need call it at all in the most of time. For
 * making the performance impact be minimum, it's suggested to do a
 * check before calling it:
 *
 *        if (unlikely(RTE_VHOST_NEED_LOG(features)))
 *                rte_vhost_log_write(vid, addr, len);
 *
 * @param vid
 *  vhost device ID
 * @param addr
 *  the starting address for write
 * @param len
 *  the length to write
 */
void rte_vhost_log_write(int vid, uint64_t addr, uint64_t len);

/**
 * Log the used ring update start at given offset.
 *
 * Same as rte_vhost_log_write, it's suggested to do a check before
 * calling it:
 *
 *        if (unlikely(RTE_VHOST_NEED_LOG(features)))
 *                rte_vhost_log_used_vring(vid, vring_idx, offset, len);
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  the vring index
 * @param offset
 *  the offset inside the used ring
 * @param len
 *  the length to write
 */
void rte_vhost_log_used_vring(int vid, uint16_t vring_idx,
			      uint64_t offset, uint64_t len);

int rte_vhost_enable_guest_notification(int vid, uint16_t queue_id, int enable);

/**
 * Register vhost driver. path could be different for multiple
 * instance support.
 */
int rte_vhost_driver_register(const char *path, uint64_t flags);

/* Unregister vhost driver. This is only meaningful to vhost user. */
int rte_vhost_driver_unregister(const char *path);

/**
 * Set the vdpa device id, enforce single connection per socket
 *
 * @param path
 *  The vhost-user socket file path
 * @param did
 *  Device id
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_driver_attach_vdpa_device(const char *path, int did);

/**
 * Unset the vdpa device id
 *
 * @param path
 *  The vhost-user socket file path
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_driver_detach_vdpa_device(const char *path);

/**
 * Get the device id
 *
 * @param path
 *  The vhost-user socket file path
 * @return
 *  Device id, -1 on failure
 */
int __rte_experimental
rte_vhost_driver_get_vdpa_device_id(const char *path);

/**
 * Set the feature bits the vhost-user driver supports.
 *
 * @param path
 *  The vhost-user socket file path
 * @param features
 *  Supported features
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_driver_set_features(const char *path, uint64_t features);

/**
 * Enable vhost-user driver features.
 *
 * Note that
 * - the param features should be a subset of the feature bits provided
 *   by rte_vhost_driver_set_features().
 * - it must be invoked before vhost-user negotiation starts.
 *
 * @param path
 *  The vhost-user socket file path
 * @param features
 *  Features to enable
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_driver_enable_features(const char *path, uint64_t features);

/**
 * Disable vhost-user driver features.
 *
 * The two notes at rte_vhost_driver_enable_features() also apply here.
 *
 * @param path
 *  The vhost-user socket file path
 * @param features
 *  Features to disable
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_driver_disable_features(const char *path, uint64_t features);

/**
 * Get the feature bits before feature negotiation.
 *
 * @param path
 *  The vhost-user socket file path
 * @param features
 *  A pointer to store the queried feature bits
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_driver_get_features(const char *path, uint64_t *features);

/**
 * Get the protocol feature bits before feature negotiation.
 *
 * @param path
 *  The vhost-user socket file path
 * @param protocol_features
 *  A pointer to store the queried protocol feature bits
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_driver_get_protocol_features(const char *path,
		uint64_t *protocol_features);

/**
 * Get the queue number bits before feature negotiation.
 *
 * @param path
 *  The vhost-user socket file path
 * @param queue_num
 *  A pointer to store the queried queue number bits
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_driver_get_queue_num(const char *path, uint32_t *queue_num);

/**
 * Get the feature bits after negotiation
 *
 * @param vid
 *  Vhost device ID
 * @param features
 *  A pointer to store the queried feature bits
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_get_negotiated_features(int vid, uint64_t *features);

/* Register callbacks. */
int rte_vhost_driver_callback_register(const char *path,
	struct vhost_device_ops const * const ops);

/**
 *
 * Start the vhost-user driver.
 *
 * This function triggers the vhost-user negotiation.
 *
 * @param path
 *  The vhost-user socket file path
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_driver_start(const char *path);

/**
 * Get the MTU value of the device if set in QEMU.
 *
 * @param vid
 *  virtio-net device ID
 * @param mtu
 *  The variable to store the MTU value
 *
 * @return
 *  0: success
 *  -EAGAIN: device not yet started
 *  -ENOTSUP: device does not support MTU feature
 */
int rte_vhost_get_mtu(int vid, uint16_t *mtu);

/**
 * Get the numa node from which the virtio net device's memory
 * is allocated.
 *
 * @param vid
 *  vhost device ID
 *
 * @return
 *  The numa node, -1 on failure
 */
int rte_vhost_get_numa_node(int vid);

/**
 * @deprecated
 * Get the number of queues the device supports.
 *
 * Note this function is deprecated, as it returns a queue pair number,
 * which is vhost specific. Instead, rte_vhost_get_vring_num should
 * be used.
 *
 * @param vid
 *  vhost device ID
 *
 * @return
 *  The number of queues, 0 on failure
 */
__rte_deprecated
uint32_t rte_vhost_get_queue_num(int vid);

/**
 * Get the number of vrings the device supports.
 *
 * @param vid
 *  vhost device ID
 *
 * @return
 *  The number of vrings, 0 on failure
 */
uint16_t rte_vhost_get_vring_num(int vid);

/**
 * Get the virtio net device's ifname, which is the vhost-user socket
 * file path.
 *
 * @param vid
 *  vhost device ID
 * @param buf
 *  The buffer to stored the queried ifname
 * @param len
 *  The length of buf
 *
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_get_ifname(int vid, char *buf, size_t len);

/**
 * Get how many avail entries are left in the queue
 *
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  virtio queue index
 *
 * @return
 *  num of avail entires left
 */
uint16_t rte_vhost_avail_entries(int vid, uint16_t queue_id);

struct rte_mbuf;
struct rte_mempool;
/**
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtual device. A packet
 * count is returned to indicate the number of packets that were successfully
 * added to the RX queue.
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  virtio queue index in mq case
 * @param pkts
 *  array to contain packets to be enqueued
 * @param count
 *  packets num to be enqueued
 * @return
 *  num of packets enqueued
 */
uint16_t rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count);

/**
 * This function gets guest buffers from the virtio device TX virtqueue,
 * construct host mbufs, copies guest buffer content to host mbufs and
 * store them in pkts to be processed.
 * @param vid
 *  vhost device ID
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
uint16_t rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count);

/**
 * Get guest mem table: a list of memory regions.
 *
 * An rte_vhost_vhost_memory object will be allocated internaly, to hold the
 * guest memory regions. Application should free it at destroy_device()
 * callback.
 *
 * @param vid
 *  vhost device ID
 * @param mem
 *  To store the returned mem regions
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_get_mem_table(int vid, struct rte_vhost_memory **mem);

/**
 * Get guest vring info, including the vring address, vring size, etc.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param vring
 *  the structure to hold the requested vring info
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_get_vhost_vring(int vid, uint16_t vring_idx,
			      struct rte_vhost_vring *vring);

/**
 * Notify the guest that used descriptors have been added to the vring.  This
 * function acts as a memory barrier.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_vring_call(int vid, uint16_t vring_idx);

/**
 * Get vhost RX queue avail count.
 *
 * @param vid
 *  vhost device ID
 * @param qid
 *  virtio queue index in mq case
 * @return
 *  num of desc available
 */
uint32_t rte_vhost_rx_queue_count(int vid, uint16_t qid);

/**
 * Get log base and log size of the vhost device
 *
 * @param vid
 *  vhost device ID
 * @param log_base
 *  vhost log base
 * @param log_size
 *  vhost log size
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_get_log_base(int vid, uint64_t *log_base, uint64_t *log_size);

/**
 * Get last_avail/used_idx of the vhost virtqueue
 *
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  vhost queue index
 * @param last_avail_idx
 *  vhost last_avail_idx to get
 * @param last_used_idx
 *  vhost last_used_idx to get
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_get_vring_base(int vid, uint16_t queue_id,
		uint16_t *last_avail_idx, uint16_t *last_used_idx);

/**
 * Set last_avail/used_idx of the vhost virtqueue
 *
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  vhost queue index
 * @param last_avail_idx
 *  last_avail_idx to set
 * @param last_used_idx
 *  last_used_idx to set
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_set_vring_base(int vid, uint16_t queue_id,
		uint16_t last_avail_idx, uint16_t last_used_idx);

/**
 * Get vdpa device id for vhost device.
 *
 * @param vid
 *  vhost device id
 * @return
 *  device id
 */
int __rte_experimental
rte_vhost_get_vdpa_device_id(int vid);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VHOST_H_ */
