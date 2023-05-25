/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _RTE_VHOST_H_
#define _RTE_VHOST_H_

/**
 * @file
 * Interface to vhost-user
 */

#include <stdbool.h>
#include <stdint.h>
#include <sys/eventfd.h>

#include <rte_compat.h>
#include <rte_memory.h>
#include <rte_mempool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
/* These are not C++-aware. */
#include <linux/vhost.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_net.h>
#endif

#define RTE_VHOST_USER_CLIENT		(1ULL << 0)
#define RTE_VHOST_USER_NO_RECONNECT	(1ULL << 1)
#define RTE_VHOST_USER_RESERVED_1	(1ULL << 2)
#define RTE_VHOST_USER_IOMMU_SUPPORT	(1ULL << 3)
#define RTE_VHOST_USER_POSTCOPY_SUPPORT		(1ULL << 4)
/* support mbuf with external buffer attached */
#define RTE_VHOST_USER_EXTBUF_SUPPORT	(1ULL << 5)
/* support only linear buffers (no chained mbufs) */
#define RTE_VHOST_USER_LINEARBUF_SUPPORT	(1ULL << 6)
#define RTE_VHOST_USER_ASYNC_COPY	(1ULL << 7)
#define RTE_VHOST_USER_NET_COMPLIANT_OL_FLAGS	(1ULL << 8)
#define RTE_VHOST_USER_NET_STATS_ENABLE	(1ULL << 9)

/* Features. */
#ifndef VIRTIO_NET_F_GUEST_ANNOUNCE
 #define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#endif

#ifndef VIRTIO_NET_F_MQ
 #define VIRTIO_NET_F_MQ		22
#endif

#ifndef VIRTIO_NET_F_MTU
 #define VIRTIO_NET_F_MTU 3
#endif

#ifndef VIRTIO_F_ANY_LAYOUT
 #define VIRTIO_F_ANY_LAYOUT		27
#endif

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

#ifndef VHOST_USER_PROTOCOL_F_BACKEND_REQ
#define VHOST_USER_PROTOCOL_F_BACKEND_REQ	5
#endif

#ifndef VHOST_USER_PROTOCOL_F_CRYPTO_SESSION
#define VHOST_USER_PROTOCOL_F_CRYPTO_SESSION 7
#endif

#ifndef VHOST_USER_PROTOCOL_F_PAGEFAULT
#define VHOST_USER_PROTOCOL_F_PAGEFAULT 8
#endif

#ifndef VHOST_USER_PROTOCOL_F_CONFIG
#define VHOST_USER_PROTOCOL_F_CONFIG 9
#endif

#ifndef VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD
#define VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD 10
#endif

#ifndef VHOST_USER_PROTOCOL_F_HOST_NOTIFIER
#define VHOST_USER_PROTOCOL_F_HOST_NOTIFIER 11
#endif

#ifndef VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD
#define VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD 12
#endif

#ifndef VHOST_USER_PROTOCOL_F_STATUS
#define VHOST_USER_PROTOCOL_F_STATUS 16
#endif

/** Indicate whether protocol features negotiation is supported. */
#ifndef VHOST_USER_F_PROTOCOL_FEATURES
#define VHOST_USER_F_PROTOCOL_FEATURES	30
#endif

#define RTE_MAX_VHOST_DEVICE	1024

#define RTE_VHOST_VDPA_DEVICE_TYPE_NET 0
#define RTE_VHOST_VDPA_DEVICE_TYPE_BLK 1

struct rte_vdpa_device;

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

struct rte_vhost_inflight_desc_split {
	uint8_t inflight;
	uint8_t padding[5];
	uint16_t next;
	uint64_t counter;
};

struct rte_vhost_inflight_info_split {
	uint64_t features;
	uint16_t version;
	uint16_t desc_num;
	uint16_t last_inflight_io;
	uint16_t used_idx;
	struct rte_vhost_inflight_desc_split desc[];
};

struct rte_vhost_inflight_desc_packed {
	uint8_t inflight;
	uint8_t padding;
	uint16_t next;
	uint16_t last;
	uint16_t num;
	uint64_t counter;
	uint16_t id;
	uint16_t flags;
	uint32_t len;
	uint64_t addr;
};

struct rte_vhost_inflight_info_packed {
	uint64_t features;
	uint16_t version;
	uint16_t desc_num;
	uint16_t free_head;
	uint16_t old_free_head;
	uint16_t used_idx;
	uint16_t old_used_idx;
	uint8_t used_wrap_counter;
	uint8_t old_used_wrap_counter;
	uint8_t padding[7];
	struct rte_vhost_inflight_desc_packed desc[];
};

struct rte_vhost_resubmit_desc {
	uint16_t index;
	uint64_t counter;
};

struct rte_vhost_resubmit_info {
	struct rte_vhost_resubmit_desc *resubmit_list;
	uint16_t resubmit_num;
};

struct rte_vhost_ring_inflight {
	union {
		struct rte_vhost_inflight_info_split *inflight_split;
		struct rte_vhost_inflight_info_packed *inflight_packed;
	};

	struct rte_vhost_resubmit_info *resubmit_inflight;
};

struct rte_vhost_vring {
	union {
		struct vring_desc *desc;
		struct vring_packed_desc *desc_packed;
	};
	union {
		struct vring_avail *avail;
		struct vring_packed_desc_event *driver_event;
	};
	union {
		struct vring_used *used;
		struct vring_packed_desc_event *device_event;
	};
	uint64_t		log_guest_addr;

	/** Deprecated, use rte_vhost_vring_call() instead. */
	int			callfd;

	int			kickfd;
	uint16_t		size;
};

/**
 * Possible results of the vhost user message handling callbacks
 */
enum rte_vhost_msg_result {
	/* Message handling failed */
	RTE_VHOST_MSG_RESULT_ERR = -1,
	/* Message handling successful */
	RTE_VHOST_MSG_RESULT_OK =  0,
	/* Message handling successful and reply prepared */
	RTE_VHOST_MSG_RESULT_REPLY =  1,
	/* Message not handled */
	RTE_VHOST_MSG_RESULT_NOT_HANDLED,
};

/**
 * Function prototype for the vhost backend to handle specific vhost user
 * messages.
 *
 * @param vid
 *  vhost device id
 * @param msg
 *  Message pointer.
 * @return
 *  RTE_VHOST_MSG_RESULT_OK on success,
 *  RTE_VHOST_MSG_RESULT_REPLY on success with reply,
 *  RTE_VHOST_MSG_RESULT_ERR on failure,
 *  RTE_VHOST_MSG_RESULT_NOT_HANDLED if message was not handled.
 */
typedef enum rte_vhost_msg_result (*rte_vhost_msg_handle)(int vid, void *msg);

/**
 * Optional vhost user message handlers.
 */
struct rte_vhost_user_extern_ops {
	/* Called prior to the frontend message handling. */
	rte_vhost_msg_handle pre_msg_handle;
	/* Called after the frontend message handling. */
	rte_vhost_msg_handle post_msg_handle;
};

/**
 * Device and vring operations.
 */
struct rte_vhost_device_ops {
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

	/**
	 * This callback gets called each time a guest gets notified
	 * about waiting packets. This is the interrupt handling through
	 * the eventfd_write(callfd), which can be used for counting these
	 * "slow" syscalls.
	 */
	void (*guest_notified)(int vid);

	/**
	 * If this callback is registered, notification to the guest can
	 * be handled by the front-end calling rte_vhost_notify_guest().
	 * If it's not handled, 'false' should be returned. This can be used
	 * to remove the "slow" eventfd_write() syscall from the datapath.
	 */
	bool (*guest_notify)(int vid, uint16_t queue_id);
};

/**
 * Power monitor condition.
 */
struct rte_vhost_power_monitor_cond {
	/**< Address to monitor for changes */
	volatile void *addr;
	/**< If the `mask` is non-zero, location pointed
	 *   to by `addr` will be read and masked, then
	 *   compared with this value.
	 */
	uint64_t val;
	/**< 64-bit mask to extract value read from `addr` */
	uint64_t mask;
	/**< Data size (in bytes) that will be read from the
	 *   monitored memory location (`addr`).
	 */
	uint8_t size;
	/**< If 1, and masked value that read from 'addr' equals
	 *   'val', the driver should skip core sleep. If 0, and
	 *  masked value that read from 'addr' does not equal 'val',
	 *  the driver should skip core sleep.
	 */
	uint8_t match;
};

/** Maximum name length for the statistics counters */
#define RTE_VHOST_STATS_NAME_SIZE 64

/**
 * Vhost virtqueue statistics structure
 *
 * This structure is used by rte_vhost_vring_stats_get() to provide
 * virtqueue statistics to the calling application.
 * It maps a name ID, corresponding to an index in the array returned
 * by rte_vhost_vring_stats_get_names(), to a statistic value.
 */
struct rte_vhost_stat {
	uint64_t id;    /**< The index in xstats name array. */
	uint64_t value; /**< The statistic counter value. */
};

/**
 * Vhost virtqueue statistic name element
 *
 * This structure is used by rte_vhost_vring_stats_get_names() to
 * provide virtqueue statistics names to the calling application.
 */
struct rte_vhost_stat_name {
	char name[RTE_VHOST_STATS_NAME_SIZE]; /**< The statistic name. */
};

/**
 * Convert guest physical address to host virtual address
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
 *  the starting address for write (in guest physical address space)
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
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Inject the offloaded interrupt into the vhost device's queue.
 * @see guest_notify vhost device operation
 *
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  virtio queue index
 */
__rte_experimental
void rte_vhost_notify_guest(int vid, uint16_t queue_id);

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
 * @param dev
 *  vDPA device pointer
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_driver_attach_vdpa_device(const char *path,
		struct rte_vdpa_device *dev);

/**
 * Unset the vdpa device id
 *
 * @param path
 *  The vhost-user socket file path
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_driver_detach_vdpa_device(const char *path);

/**
 * Get the device id
 *
 * @param path
 *  The vhost-user socket file path
 * @return
 *  vDPA device pointer, NULL on failure
 */
struct rte_vdpa_device *
rte_vhost_driver_get_vdpa_device(const char *path);

/**
 * Get the device type of the vdpa device.
 *
 * @param path
 *  The vhost-user socket file path
 * @param type
 *  the device type of the vdpa device
 * @return
 *  0 on success, -1 on failure
 */
__rte_experimental
int
rte_vhost_driver_get_vdpa_dev_type(const char *path, uint32_t *type);

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
 * Set the protocol feature bits before feature negotiation.
 *
 * @param path
 *  The vhost-user socket file path
 * @param protocol_features
 *  Supported protocol features
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_driver_set_protocol_features(const char *path,
		uint64_t protocol_features);

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
int
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
int
rte_vhost_driver_get_queue_num(const char *path, uint32_t *queue_num);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Set the maximum number of queue pairs supported by the device.
 *
 * @param path
 *  The vhost-user socket file path
 * @param max_queue_pairs
 *  The maximum number of queue pairs
 * @return
 *  0 on success, -1 on failure
 */
__rte_experimental
int
rte_vhost_driver_set_max_queue_num(const char *path, uint32_t max_queue_pairs);

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

/**
 * Get the protocol feature bits after negotiation
 *
 * @param vid
 *  Vhost device ID
 * @param protocol_features
 *  A pointer to store the queried protocol feature bits
 * @return
 *  0 on success, -1 on failure
 */
__rte_experimental
int
rte_vhost_get_negotiated_protocol_features(int vid,
					   uint64_t *protocol_features);

/* Register callbacks. */
int rte_vhost_driver_callback_register(const char *path,
	struct rte_vhost_device_ops const * const ops);

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
 *  num of avail entries left
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
 * An rte_vhost_vhost_memory object will be allocated internally, to hold the
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
 * Get guest inflight vring info, including inflight ring and resubmit list.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param vring
 *  the structure to hold the requested inflight vring info
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_get_vhost_ring_inflight(int vid, uint16_t vring_idx,
	struct rte_vhost_ring_inflight *vring);

/**
 * Set split inflight descriptor.
 *
 * This function save descriptors that has been consumed in available
 * ring
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param idx
 *  inflight entry index
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_set_inflight_desc_split(int vid, uint16_t vring_idx,
	uint16_t idx);

/**
 * Set packed inflight descriptor and get corresponding inflight entry
 *
 * This function save descriptors that has been consumed
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param head
 *  head of descriptors
 * @param last
 *  last of descriptors
 * @param inflight_entry
 *  corresponding inflight entry
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_set_inflight_desc_packed(int vid, uint16_t vring_idx,
	uint16_t head, uint16_t last, uint16_t *inflight_entry);

/**
 * Save the head of list that the last batch of used descriptors.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param idx
 *  descriptor entry index
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_set_last_inflight_io_split(int vid,
	uint16_t vring_idx, uint16_t idx);

/**
 * Update the inflight free_head, used_idx and used_wrap_counter.
 *
 * This function will update status first before updating descriptors
 * to used
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param head
 *  head of descriptors
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_set_last_inflight_io_packed(int vid,
	uint16_t vring_idx, uint16_t head);

/**
 * Clear the split inflight status.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param last_used_idx
 *  last used idx of used ring
 * @param idx
 *  inflight entry index
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_clr_inflight_desc_split(int vid, uint16_t vring_idx,
	uint16_t last_used_idx, uint16_t idx);

/**
 * Clear the packed inflight status.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @param head
 *  inflight entry index
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_clr_inflight_desc_packed(int vid, uint16_t vring_idx,
	uint16_t head);

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
 * Notify the guest that used descriptors have been added to the vring.  This
 * function acts as a memory barrier.  This function will return -EAGAIN when
 * vq's access lock is held by other thread, user should try again later.
 *
 * @param vid
 *  vhost device ID
 * @param vring_idx
 *  vring index
 * @return
 *  0 on success, -1 on failure, -EAGAIN for another retry
 */
__rte_experimental
int rte_vhost_vring_call_nonblock(int vid, uint16_t vring_idx);

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
 * Get power monitor address of the vhost device
 *
 * @param vid
 *  vhost device ID
 * @param queue_id
 *  vhost queue ID
 * @param pmc
 *  power monitor condition
 * @return
 *  0 on success, -1 on failure
 */
__rte_experimental
int
rte_vhost_get_monitor_addr(int vid, uint16_t queue_id,
		struct rte_vhost_power_monitor_cond *pmc);

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
int
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
int
rte_vhost_get_vring_base(int vid, uint16_t queue_id,
		uint16_t *last_avail_idx, uint16_t *last_used_idx);

/**
 * Get last_avail/last_used of the vhost virtqueue
 *
 * This function is designed for the reconnection and it's specific for
 * the packed ring as we can get the two parameters from the inflight
 * queueregion
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
int
rte_vhost_get_vring_base_from_inflight(int vid,
	uint16_t queue_id, uint16_t *last_avail_idx, uint16_t *last_used_idx);

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
int
rte_vhost_set_vring_base(int vid, uint16_t queue_id,
		uint16_t last_avail_idx, uint16_t last_used_idx);

/**
 * Register external message handling callbacks
 *
 * @param vid
 *  vhost device ID
 * @param ops
 *  virtio external callbacks to register
 * @param ctx
 *  additional context passed to the callbacks
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vhost_extern_callback_register(int vid,
		struct rte_vhost_user_extern_ops const * const ops, void *ctx);

/**
 * Get vdpa device id for vhost device.
 *
 * @param vid
 *  vhost device id
 * @return
 *  vDPA device pointer on success, NULL on failure
 */
struct rte_vdpa_device *
rte_vhost_get_vdpa_device(int vid);

/**
 * Notify the guest that should get virtio configuration space from backend.
 *
 * @param vid
 *  vhost device ID
 * @param need_reply
 *  wait for the frontend response the status of this operation
 * @return
 *  0 on success, < 0 on failure
 */
__rte_experimental
int
rte_vhost_backend_config_change(int vid, bool need_reply);

/**
 * Retrieve names of statistics of a Vhost virtqueue.
 *
 * There is an assumption that 'stat_names' and 'stats' arrays are matched
 * by array index: stats_names[i].name => stats[i].value
 *
 * @param vid
 *   vhost device ID
 * @param queue_id
 *   vhost queue index
 * @param name
 *   array of at least size elements to be filled.
 *   If set to NULL, the function returns the required number of elements.
 * @param size
 *   The number of elements in stats_names array.
 * @return
 *  - Success if greater than 0 and lower or equal to *size*. The return value
 *    indicates the number of elements filled in the *names* array.
 *  - Failure if greater than *size*. The return value indicates the number of
 *    elements the *names* array that should be given to succeed.
 *  - Failure if lower than 0. The device ID or queue ID is invalid or
 +    statistics collection is not enabled.
 */
int
rte_vhost_vring_stats_get_names(int vid, uint16_t queue_id,
		struct rte_vhost_stat_name *name, unsigned int size);

/**
 * Retrieve statistics of a Vhost virtqueue.
 *
 * There is an assumption that 'stat_names' and 'stats' arrays are matched
 * by array index: stats_names[i].name => stats[i].value
 *
 * @param vid
 *   vhost device ID
 * @param queue_id
 *   vhost queue index
 * @param stats
 *   A pointer to a table of structure of type rte_vhost_stat to be filled with
 *   virtqueue statistics ids and values.
 * @param n
 *   The number of elements in stats array.
 * @return
 *  - Success if greater than 0 and lower or equal to *n*. The return value
 *    indicates the number of elements filled in the *stats* array.
 *  - Failure if greater than *n*. The return value indicates the number of
 *    elements the *stats* array that should be given to succeed.
 *  - Failure if lower than 0. The device ID or queue ID is invalid, or
 *    statistics collection is not enabled.
 */
int
rte_vhost_vring_stats_get(int vid, uint16_t queue_id,
		struct rte_vhost_stat *stats, unsigned int n);

/**
 * Reset statistics of a Vhost virtqueue.
 *
 * @param vid
 *   vhost device ID
 * @param queue_id
 *   vhost queue index
 * @return
 *  - Success if 0. Statistics have been reset.
 *  - Failure if lower than 0. The device ID or queue ID is invalid, or
 *    statistics collection is not enabled.
 */
int
rte_vhost_vring_stats_reset(int vid, uint16_t queue_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VHOST_H_ */
