/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _VHOST_NET_CDEV_H_
#define _VHOST_NET_CDEV_H_
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <unistd.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <rte_log.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_dmadev.h>

#include "rte_vhost.h"
#include "vdpa_driver.h"

#include "rte_vhost_async.h"

/* Used to indicate that the device is running on a data core */
#define VIRTIO_DEV_RUNNING ((uint32_t)1 << 0)
/* Used to indicate that the device is ready to operate */
#define VIRTIO_DEV_READY ((uint32_t)1 << 1)
/* Used to indicate that the built-in vhost net device backend is enabled */
#define VIRTIO_DEV_BUILTIN_VIRTIO_NET ((uint32_t)1 << 2)
/* Used to indicate that the device has its own data path and configured */
#define VIRTIO_DEV_VDPA_CONFIGURED ((uint32_t)1 << 3)
/* Used to indicate that the feature negotiation failed */
#define VIRTIO_DEV_FEATURES_FAILED ((uint32_t)1 << 4)
/* Used to indicate that the virtio_net tx code should fill TX ol_flags */
#define VIRTIO_DEV_LEGACY_OL_FLAGS ((uint32_t)1 << 5)

/* Backend value set by guest. */
#define VIRTIO_DEV_STOPPED -1

#define BUF_VECTOR_MAX 256

#define VHOST_LOG_CACHE_NR 32

#define MAX_PKT_BURST 32

#define VHOST_MAX_ASYNC_IT (MAX_PKT_BURST)
#define VHOST_MAX_ASYNC_VEC 2048
#define VIRTIO_MAX_RX_PKTLEN 9728U
#define VHOST_DMA_MAX_COPY_COMPLETE ((VIRTIO_MAX_RX_PKTLEN / RTE_MBUF_DEFAULT_DATAROOM) \
		* MAX_PKT_BURST)

#define PACKED_DESC_ENQUEUE_USED_FLAG(w)	\
	((w) ? (VRING_DESC_F_AVAIL | VRING_DESC_F_USED | VRING_DESC_F_WRITE) : \
		VRING_DESC_F_WRITE)
#define PACKED_DESC_DEQUEUE_USED_FLAG(w)	\
	((w) ? (VRING_DESC_F_AVAIL | VRING_DESC_F_USED) : 0x0)
#define PACKED_DESC_SINGLE_DEQUEUE_FLAG (VRING_DESC_F_NEXT | \
					 VRING_DESC_F_INDIRECT)

#define PACKED_BATCH_SIZE (RTE_CACHE_LINE_SIZE / \
			    sizeof(struct vring_packed_desc))
#define PACKED_BATCH_MASK (PACKED_BATCH_SIZE - 1)

#ifdef VHOST_GCC_UNROLL_PRAGMA
#define vhost_for_each_try_unroll(iter, val, size) _Pragma("GCC unroll 4") \
	for (iter = val; iter < size; iter++)
#endif

#ifdef VHOST_CLANG_UNROLL_PRAGMA
#define vhost_for_each_try_unroll(iter, val, size) _Pragma("unroll 4") \
	for (iter = val; iter < size; iter++)
#endif

#ifdef VHOST_ICC_UNROLL_PRAGMA
#define vhost_for_each_try_unroll(iter, val, size) _Pragma("unroll (4)") \
	for (iter = val; iter < size; iter++)
#endif

#ifndef vhost_for_each_try_unroll
#define vhost_for_each_try_unroll(iter, val, num) \
	for (iter = val; iter < num; iter++)
#endif

/**
 * Structure contains buffer address, length and descriptor index
 * from vring to do scatter RX.
 */
struct buf_vector {
	uint64_t buf_iova;
	uint64_t buf_addr;
	uint32_t buf_len;
	uint32_t desc_idx;
};

/*
 * Structure contains the info for each batched memory copy.
 */
struct batch_copy_elem {
	void *dst;
	void *src;
	uint32_t len;
	uint64_t log_addr;
};

/*
 * Structure that contains the info for batched dirty logging.
 */
struct log_cache_entry {
	uint32_t offset;
	unsigned long val;
};

struct vring_used_elem_packed {
	uint16_t id;
	uint16_t flags;
	uint32_t len;
	uint32_t count;
};

/**
 * iovec
 */
struct vhost_iovec {
	void *src_addr;
	void *dst_addr;
	size_t len;
};

/**
 * iovec iterator
 */
struct vhost_iov_iter {
	/** pointer to the iovec array */
	struct vhost_iovec *iov;
	/** number of iovec in this iterator */
	unsigned long nr_segs;
};

struct async_dma_vchan_info {
	/* circular array to track if packet copy completes */
	bool **pkts_cmpl_flag_addr;

	/* max elements in 'pkts_cmpl_flag_addr' */
	uint16_t ring_size;
	/* ring index mask for 'pkts_cmpl_flag_addr' */
	uint16_t ring_mask;

	/**
	 * DMA virtual channel lock. Although it is able to bind DMA
	 * virtual channels to data plane threads, vhost control plane
	 * thread could call data plane functions too, thus causing
	 * DMA device contention.
	 *
	 * For example, in VM exit case, vhost control plane thread needs
	 * to clear in-flight packets before disable vring, but there could
	 * be anotther data plane thread is enqueuing packets to the same
	 * vring with the same DMA virtual channel. As dmadev PMD functions
	 * are lock-free, the control plane and data plane threads could
	 * operate the same DMA virtual channel at the same time.
	 */
	rte_spinlock_t dma_lock;
};

struct async_dma_info {
	struct async_dma_vchan_info *vchans;
	/* number of registered virtual channels */
	uint16_t nr_vchans;
};

extern struct async_dma_info dma_copy_track[RTE_DMADEV_DEFAULT_MAX];

/**
 * inflight async packet information
 */
struct async_inflight_info {
	struct rte_mbuf *mbuf;
	uint16_t descs; /* num of descs inflight */
	uint16_t nr_buffers; /* num of buffers inflight for packed ring */
};

struct vhost_async {
	struct vhost_iov_iter iov_iter[VHOST_MAX_ASYNC_IT];
	struct vhost_iovec iovec[VHOST_MAX_ASYNC_VEC];
	uint16_t iter_idx;
	uint16_t iovec_idx;

	/* data transfer status */
	struct async_inflight_info *pkts_info;
	/**
	 * Packet reorder array. "true" indicates that DMA device
	 * completes all copies for the packet.
	 *
	 * Note that this array could be written by multiple threads
	 * simultaneously. For example, in the case of thread0 and
	 * thread1 RX packets from NIC and then enqueue packets to
	 * vring0 and vring1 with own DMA device DMA0 and DMA1, it's
	 * possible for thread0 to get completed copies belonging to
	 * vring1 from DMA0, while thread0 is calling rte_vhost_poll
	 * _enqueue_completed() for vring0 and thread1 is calling
	 * rte_vhost_submit_enqueue_burst() for vring1. In this case,
	 * vq->access_lock cannot protect pkts_cmpl_flag of vring1.
	 *
	 * However, since offloading is per-packet basis, each packet
	 * flag will only be written by one thread. And single byte
	 * write is atomic, so no lock for pkts_cmpl_flag is needed.
	 */
	bool *pkts_cmpl_flag;
	uint16_t pkts_idx;
	uint16_t pkts_inflight_n;
	union {
		struct vring_used_elem  *descs_split;
		struct vring_used_elem_packed *buffers_packed;
	};
	union {
		uint16_t desc_idx_split;
		uint16_t buffer_idx_packed;
	};
	union {
		uint16_t last_desc_idx_split;
		uint16_t last_buffer_idx_packed;
	};
};

/**
 * Structure contains variables relevant to RX/TX virtqueues.
 */
struct vhost_virtqueue {
	union {
		struct vring_desc	*desc;
		struct vring_packed_desc   *desc_packed;
	};
	union {
		struct vring_avail	*avail;
		struct vring_packed_desc_event *driver_event;
	};
	union {
		struct vring_used	*used;
		struct vring_packed_desc_event *device_event;
	};
	uint16_t		size;

	uint16_t		last_avail_idx;
	uint16_t		last_used_idx;
	/* Last used index we notify to front end. */
	uint16_t		signalled_used;
	bool			signalled_used_valid;
#define VIRTIO_INVALID_EVENTFD		(-1)
#define VIRTIO_UNINITIALIZED_EVENTFD	(-2)

	bool			enabled;
	bool			access_ok;
	bool			ready;

	rte_spinlock_t		access_lock;


	union {
		struct vring_used_elem  *shadow_used_split;
		struct vring_used_elem_packed *shadow_used_packed;
	};
	uint16_t                shadow_used_idx;
	/* Record packed ring enqueue latest desc cache aligned index */
	uint16_t		shadow_aligned_idx;
	/* Record packed ring first dequeue desc index */
	uint16_t		shadow_last_used_idx;

	uint16_t		batch_copy_nb_elems;
	struct batch_copy_elem	*batch_copy_elems;
	int			numa_node;
	bool			used_wrap_counter;
	bool			avail_wrap_counter;

	/* Physical address of used ring, for logging */
	uint16_t		log_cache_nb_elem;
	uint64_t		log_guest_addr;
	struct log_cache_entry	*log_cache;

	rte_rwlock_t	iotlb_lock;
	rte_rwlock_t	iotlb_pending_lock;
	struct rte_mempool *iotlb_pool;
	TAILQ_HEAD(, vhost_iotlb_entry) iotlb_list;
	TAILQ_HEAD(, vhost_iotlb_entry) iotlb_pending_list;
	int				iotlb_cache_nr;

	/* Used to notify the guest (trigger interrupt) */
	int			callfd;
	/* Currently unused as polling mode is enabled */
	int			kickfd;

	/* inflight share memory info */
	union {
		struct rte_vhost_inflight_info_split *inflight_split;
		struct rte_vhost_inflight_info_packed *inflight_packed;
	};
	struct rte_vhost_resubmit_info *resubmit_inflight;
	uint64_t		global_counter;

	struct vhost_async	*async;

	int			notif_enable;
#define VIRTIO_UNINITIALIZED_NOTIF	(-1)

	struct vhost_vring_addr ring_addrs;
} __rte_cache_aligned;

/* Virtio device status as per Virtio specification */
#define VIRTIO_DEVICE_STATUS_RESET		0x00
#define VIRTIO_DEVICE_STATUS_ACK		0x01
#define VIRTIO_DEVICE_STATUS_DRIVER		0x02
#define VIRTIO_DEVICE_STATUS_DRIVER_OK		0x04
#define VIRTIO_DEVICE_STATUS_FEATURES_OK	0x08
#define VIRTIO_DEVICE_STATUS_DEV_NEED_RESET	0x40
#define VIRTIO_DEVICE_STATUS_FAILED		0x80

#define VHOST_MAX_VRING			0x100
#define VHOST_MAX_QUEUE_PAIRS		0x80

/* Declare IOMMU related bits for older kernels */
#ifndef VIRTIO_F_IOMMU_PLATFORM

#define VIRTIO_F_IOMMU_PLATFORM 33

struct vhost_iotlb_msg {
	__u64 iova;
	__u64 size;
	__u64 uaddr;
#define VHOST_ACCESS_RO      0x1
#define VHOST_ACCESS_WO      0x2
#define VHOST_ACCESS_RW      0x3
	__u8 perm;
#define VHOST_IOTLB_MISS           1
#define VHOST_IOTLB_UPDATE         2
#define VHOST_IOTLB_INVALIDATE     3
#define VHOST_IOTLB_ACCESS_FAIL    4
	__u8 type;
};

#define VHOST_IOTLB_MSG 0x1

struct vhost_msg {
	int type;
	union {
		struct vhost_iotlb_msg iotlb;
		__u8 padding[64];
	};
};
#endif

/*
 * Define virtio 1.0 for older kernels
 */
#ifndef VIRTIO_F_VERSION_1
 #define VIRTIO_F_VERSION_1 32
#endif

/* Declare packed ring related bits for older kernels */
#ifndef VIRTIO_F_RING_PACKED

#define VIRTIO_F_RING_PACKED 34

struct vring_packed_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t id;
	uint16_t flags;
};

struct vring_packed_desc_event {
	uint16_t off_wrap;
	uint16_t flags;
};
#endif

/*
 * Declare below packed ring defines unconditionally
 * as Kernel header might use different names.
 */
#define VRING_DESC_F_AVAIL	(1ULL << 7)
#define VRING_DESC_F_USED	(1ULL << 15)

#define VRING_EVENT_F_ENABLE 0x0
#define VRING_EVENT_F_DISABLE 0x1
#define VRING_EVENT_F_DESC 0x2

/*
 * Available and used descs are in same order
 */
#ifndef VIRTIO_F_IN_ORDER
#define VIRTIO_F_IN_ORDER      35
#endif

/* Features supported by this builtin vhost-user net driver. */
#define VIRTIO_NET_SUPPORTED_FEATURES ((1ULL << VIRTIO_NET_F_MRG_RXBUF) | \
				(1ULL << VIRTIO_F_ANY_LAYOUT) | \
				(1ULL << VIRTIO_NET_F_CTRL_VQ) | \
				(1ULL << VIRTIO_NET_F_CTRL_RX) | \
				(1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) | \
				(1ULL << VIRTIO_NET_F_MQ)      | \
				(1ULL << VIRTIO_F_VERSION_1)   | \
				(1ULL << VHOST_F_LOG_ALL)      | \
				(1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
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
				(1ULL << VIRTIO_RING_F_EVENT_IDX) | \
				(1ULL << VIRTIO_NET_F_MTU)  | \
				(1ULL << VIRTIO_F_IN_ORDER) | \
				(1ULL << VIRTIO_F_IOMMU_PLATFORM) | \
				(1ULL << VIRTIO_F_RING_PACKED))


struct guest_page {
	uint64_t guest_phys_addr;
	uint64_t host_iova;
	uint64_t host_user_addr;
	uint64_t size;
};

struct inflight_mem_info {
	int		fd;
	void		*addr;
	uint64_t	size;
};

/**
 * Device structure contains all configuration information relating
 * to the device.
 */
struct virtio_net {
	/* Frontend (QEMU) memory and memory region information */
	struct rte_vhost_memory	*mem;
	uint64_t		features;
	uint64_t		protocol_features;
	int			vid;
	uint32_t		flags;
	uint16_t		vhost_hlen;
	/* to tell if we need broadcast rarp packet */
	int16_t			broadcast_rarp;
	uint32_t		nr_vring;
	int			async_copy;

	int			extbuf;
	int			linearbuf;
	struct vhost_virtqueue	*virtqueue[VHOST_MAX_QUEUE_PAIRS * 2];
	struct inflight_mem_info *inflight_info;
#define IF_NAME_SZ (PATH_MAX > IFNAMSIZ ? PATH_MAX : IFNAMSIZ)
	char			ifname[IF_NAME_SZ];
	uint64_t		log_size;
	uint64_t		log_base;
	uint64_t		log_addr;
	struct rte_ether_addr	mac;
	uint16_t		mtu;
	uint8_t			status;

	struct rte_vhost_device_ops const *notify_ops;

	uint32_t		nr_guest_pages;
	uint32_t		max_guest_pages;
	struct guest_page       *guest_pages;

	int			slave_req_fd;
	rte_spinlock_t		slave_req_lock;

	int			postcopy_ufd;
	int			postcopy_listening;

	struct rte_vdpa_device *vdpa_dev;

	/* context data for the external message handlers */
	void			*extern_data;
	/* pre and post vhost user message handlers for the device */
	struct rte_vhost_user_extern_ops extern_ops;
} __rte_cache_aligned;

static __rte_always_inline bool
vq_is_packed(struct virtio_net *dev)
{
	return dev->features & (1ull << VIRTIO_F_RING_PACKED);
}

static inline bool
desc_is_avail(struct vring_packed_desc *desc, bool wrap_counter)
{
	uint16_t flags = __atomic_load_n(&desc->flags, __ATOMIC_ACQUIRE);

	return wrap_counter == !!(flags & VRING_DESC_F_AVAIL) &&
		wrap_counter != !!(flags & VRING_DESC_F_USED);
}

static inline void
vq_inc_last_used_packed(struct vhost_virtqueue *vq, uint16_t num)
{
	vq->last_used_idx += num;
	if (vq->last_used_idx >= vq->size) {
		vq->used_wrap_counter ^= 1;
		vq->last_used_idx -= vq->size;
	}
}

static inline void
vq_inc_last_avail_packed(struct vhost_virtqueue *vq, uint16_t num)
{
	vq->last_avail_idx += num;
	if (vq->last_avail_idx >= vq->size) {
		vq->avail_wrap_counter ^= 1;
		vq->last_avail_idx -= vq->size;
	}
}

void __vhost_log_cache_write(struct virtio_net *dev,
		struct vhost_virtqueue *vq,
		uint64_t addr, uint64_t len);
void __vhost_log_cache_write_iova(struct virtio_net *dev,
		struct vhost_virtqueue *vq,
		uint64_t iova, uint64_t len);
void __vhost_log_cache_sync(struct virtio_net *dev,
		struct vhost_virtqueue *vq);
void __vhost_log_write(struct virtio_net *dev, uint64_t addr, uint64_t len);
void __vhost_log_write_iova(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    uint64_t iova, uint64_t len);

static __rte_always_inline void
vhost_log_write(struct virtio_net *dev, uint64_t addr, uint64_t len)
{
	if (unlikely(dev->features & (1ULL << VHOST_F_LOG_ALL)))
		__vhost_log_write(dev, addr, len);
}

static __rte_always_inline void
vhost_log_cache_sync(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	if (unlikely(dev->features & (1ULL << VHOST_F_LOG_ALL)))
		__vhost_log_cache_sync(dev, vq);
}

static __rte_always_inline void
vhost_log_cache_write(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t addr, uint64_t len)
{
	if (unlikely(dev->features & (1ULL << VHOST_F_LOG_ALL)))
		__vhost_log_cache_write(dev, vq, addr, len);
}

static __rte_always_inline void
vhost_log_cache_used_vring(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t offset, uint64_t len)
{
	if (unlikely(dev->features & (1ULL << VHOST_F_LOG_ALL))) {
		if (unlikely(vq->log_guest_addr == 0))
			return;
		__vhost_log_cache_write(dev, vq, vq->log_guest_addr + offset,
					len);
	}
}

static __rte_always_inline void
vhost_log_used_vring(struct virtio_net *dev, struct vhost_virtqueue *vq,
		     uint64_t offset, uint64_t len)
{
	if (unlikely(dev->features & (1ULL << VHOST_F_LOG_ALL))) {
		if (unlikely(vq->log_guest_addr == 0))
			return;
		__vhost_log_write(dev, vq->log_guest_addr + offset, len);
	}
}

static __rte_always_inline void
vhost_log_cache_write_iova(struct virtio_net *dev, struct vhost_virtqueue *vq,
			   uint64_t iova, uint64_t len)
{
	if (likely(!(dev->features & (1ULL << VHOST_F_LOG_ALL))))
		return;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		__vhost_log_cache_write_iova(dev, vq, iova, len);
	else
		__vhost_log_cache_write(dev, vq, iova, len);
}

static __rte_always_inline void
vhost_log_write_iova(struct virtio_net *dev, struct vhost_virtqueue *vq,
			   uint64_t iova, uint64_t len)
{
	if (likely(!(dev->features & (1ULL << VHOST_F_LOG_ALL))))
		return;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		__vhost_log_write_iova(dev, vq, iova, len);
	else
		__vhost_log_write(dev, iova, len);
}

extern int vhost_config_log_level;
extern int vhost_data_log_level;

#define VHOST_LOG_CONFIG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, vhost_config_log_level,	\
		"VHOST_CONFIG: " fmt, ##args)

#define VHOST_LOG_DATA(level, fmt, args...) \
	(void)((RTE_LOG_ ## level <= RTE_LOG_DP_LEVEL) ?	\
	 rte_log(RTE_LOG_ ## level,  vhost_data_log_level,	\
		"VHOST_DATA : " fmt, ##args) :			\
	 0)

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VHOST_MAX_PRINT_BUFF 6072
#define PRINT_PACKET(device, addr, size, header) do { \
	char *pkt_addr = (char *)(addr); \
	unsigned int index; \
	char packet[VHOST_MAX_PRINT_BUFF]; \
	\
	if ((header)) \
		snprintf(packet, VHOST_MAX_PRINT_BUFF, "(%d) Header size %d: ", (device->vid), (size)); \
	else \
		snprintf(packet, VHOST_MAX_PRINT_BUFF, "(%d) Packet size %d: ", (device->vid), (size)); \
	for (index = 0; index < (size); index++) { \
		snprintf(packet + strnlen(packet, VHOST_MAX_PRINT_BUFF), VHOST_MAX_PRINT_BUFF - strnlen(packet, VHOST_MAX_PRINT_BUFF), \
			"%02hhx ", pkt_addr[index]); \
	} \
	snprintf(packet + strnlen(packet, VHOST_MAX_PRINT_BUFF), VHOST_MAX_PRINT_BUFF - strnlen(packet, VHOST_MAX_PRINT_BUFF), "\n"); \
	\
	VHOST_LOG_DATA(DEBUG, "%s", packet); \
} while (0)
#else
#define PRINT_PACKET(device, addr, size, header) do {} while (0)
#endif

extern struct virtio_net *vhost_devices[RTE_MAX_VHOST_DEVICE];

#define VHOST_BINARY_SEARCH_THRESH 256

static __rte_always_inline int guest_page_addrcmp(const void *p1,
						const void *p2)
{
	const struct guest_page *page1 = (const struct guest_page *)p1;
	const struct guest_page *page2 = (const struct guest_page *)p2;

	if (page1->guest_phys_addr > page2->guest_phys_addr)
		return 1;
	if (page1->guest_phys_addr < page2->guest_phys_addr)
		return -1;

	return 0;
}

static __rte_always_inline int guest_page_rangecmp(const void *p1, const void *p2)
{
	const struct guest_page *page1 = (const struct guest_page *)p1;
	const struct guest_page *page2 = (const struct guest_page *)p2;

	if (page1->guest_phys_addr >= page2->guest_phys_addr) {
		if (page1->guest_phys_addr < page2->guest_phys_addr + page2->size)
			return 0;
		else
			return 1;
	} else
		return -1;
}

static __rte_always_inline rte_iova_t
gpa_to_first_hpa(struct virtio_net *dev, uint64_t gpa,
	uint64_t gpa_size, uint64_t *hpa_size)
{
	uint32_t i;
	struct guest_page *page;
	struct guest_page key;

	*hpa_size = gpa_size;
	if (dev->nr_guest_pages >= VHOST_BINARY_SEARCH_THRESH) {
		key.guest_phys_addr = gpa;
		page = bsearch(&key, dev->guest_pages, dev->nr_guest_pages,
			       sizeof(struct guest_page), guest_page_rangecmp);
		if (page) {
			if (gpa + gpa_size <=
					page->guest_phys_addr + page->size) {
				return gpa - page->guest_phys_addr +
					page->host_iova;
			} else if (gpa < page->guest_phys_addr +
						page->size) {
				*hpa_size = page->guest_phys_addr +
					page->size - gpa;
				return gpa - page->guest_phys_addr +
					page->host_iova;
			}
		}
	} else {
		for (i = 0; i < dev->nr_guest_pages; i++) {
			page = &dev->guest_pages[i];

			if (gpa >= page->guest_phys_addr) {
				if (gpa + gpa_size <=
					page->guest_phys_addr + page->size) {
					return gpa - page->guest_phys_addr +
						page->host_iova;
				} else if (gpa < page->guest_phys_addr +
							page->size) {
					*hpa_size = page->guest_phys_addr +
						page->size - gpa;
					return gpa - page->guest_phys_addr +
						page->host_iova;
				}
			}
		}
	}

	*hpa_size = 0;
	return 0;
}

/* Convert guest physical address to host physical address */
static __rte_always_inline rte_iova_t
gpa_to_hpa(struct virtio_net *dev, uint64_t gpa, uint64_t size)
{
	rte_iova_t hpa;
	uint64_t hpa_size;

	hpa = gpa_to_first_hpa(dev, gpa, size, &hpa_size);
	return hpa_size == size ? hpa : 0;
}

static __rte_always_inline uint64_t
hva_to_gpa(struct virtio_net *dev, uint64_t vva, uint64_t len)
{
	struct rte_vhost_mem_region *r;
	uint32_t i;

	if (unlikely(!dev || !dev->mem))
		return 0;

	for (i = 0; i < dev->mem->nregions; i++) {
		r = &dev->mem->regions[i];

		if (vva >= r->host_user_addr &&
		    vva + len <  r->host_user_addr + r->size) {
			return r->guest_phys_addr + vva - r->host_user_addr;
		}
	}
	return 0;
}

static __rte_always_inline struct virtio_net *
get_device(int vid)
{
	struct virtio_net *dev = vhost_devices[vid];

	if (unlikely(!dev)) {
		VHOST_LOG_CONFIG(ERR,
			"(%d) device not found.\n", vid);
	}

	return dev;
}

int vhost_new_device(void);
void cleanup_device(struct virtio_net *dev, int destroy);
void reset_device(struct virtio_net *dev);
void vhost_destroy_device(int);
void vhost_destroy_device_notify(struct virtio_net *dev);

void cleanup_vq(struct vhost_virtqueue *vq, int destroy);
void cleanup_vq_inflight(struct virtio_net *dev, struct vhost_virtqueue *vq);
void free_vq(struct virtio_net *dev, struct vhost_virtqueue *vq);

int alloc_vring_queue(struct virtio_net *dev, uint32_t vring_idx);

void vhost_attach_vdpa_device(int vid, struct rte_vdpa_device *dev);

void vhost_set_ifname(int, const char *if_name, unsigned int if_len);
void vhost_setup_virtio_net(int vid, bool enable, bool legacy_ol_flags);
void vhost_enable_extbuf(int vid);
void vhost_enable_linearbuf(int vid);
int vhost_enable_guest_notification(struct virtio_net *dev,
		struct vhost_virtqueue *vq, int enable);

struct rte_vhost_device_ops const *vhost_driver_callback_get(const char *path);

/*
 * Backend-specific cleanup.
 *
 * TODO: fix it; we have one backend now
 */
void vhost_backend_cleanup(struct virtio_net *dev);

uint64_t __vhost_iova_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t iova, uint64_t *len, uint8_t perm);
void *vhost_alloc_copy_ind_table(struct virtio_net *dev,
			struct vhost_virtqueue *vq,
			uint64_t desc_addr, uint64_t desc_len);
int vring_translate(struct virtio_net *dev, struct vhost_virtqueue *vq);
uint64_t translate_log_addr(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t log_addr);
void vring_invalidate(struct virtio_net *dev, struct vhost_virtqueue *vq);

static __rte_always_inline uint64_t
vhost_iova_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t iova, uint64_t *len, uint8_t perm)
{
	if (!(dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)))
		return rte_vhost_va_from_guest_pa(dev->mem, iova, len);

	return __vhost_iova_to_vva(dev, vq, iova, len, perm);
}

#define vhost_avail_event(vr) \
	(*(volatile uint16_t*)&(vr)->used->ring[(vr)->size])
#define vhost_used_event(vr) \
	(*(volatile uint16_t*)&(vr)->avail->ring[(vr)->size])

/*
 * The following is used with VIRTIO_RING_F_EVENT_IDX.
 * Assuming a given event_idx value from the other size, if we have
 * just incremented index from old to new_idx, should we trigger an
 * event?
 */
static __rte_always_inline int
vhost_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
	return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
}

static __rte_always_inline void
vhost_vring_call_split(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	/* Flush used->idx update before we read avail->flags. */
	rte_atomic_thread_fence(__ATOMIC_SEQ_CST);

	/* Don't kick guest if we don't reach index specified by guest. */
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX)) {
		uint16_t old = vq->signalled_used;
		uint16_t new = vq->last_used_idx;
		bool signalled_used_valid = vq->signalled_used_valid;

		vq->signalled_used = new;
		vq->signalled_used_valid = true;

		VHOST_LOG_DATA(DEBUG, "%s: used_event_idx=%d, old=%d, new=%d\n",
			__func__,
			vhost_used_event(vq),
			old, new);

		if ((vhost_need_event(vhost_used_event(vq), new, old) &&
					(vq->callfd >= 0)) ||
				unlikely(!signalled_used_valid)) {
			eventfd_write(vq->callfd, (eventfd_t) 1);
			if (dev->notify_ops->guest_notified)
				dev->notify_ops->guest_notified(dev->vid);
		}
	} else {
		/* Kick the guest if necessary. */
		if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
				&& (vq->callfd >= 0)) {
			eventfd_write(vq->callfd, (eventfd_t)1);
			if (dev->notify_ops->guest_notified)
				dev->notify_ops->guest_notified(dev->vid);
		}
	}
}

static __rte_always_inline void
vhost_vring_call_packed(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t old, new, off, off_wrap;
	bool signalled_used_valid, kick = false;

	/* Flush used desc update. */
	rte_atomic_thread_fence(__ATOMIC_SEQ_CST);

	if (!(dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))) {
		if (vq->driver_event->flags !=
				VRING_EVENT_F_DISABLE)
			kick = true;
		goto kick;
	}

	old = vq->signalled_used;
	new = vq->last_used_idx;
	vq->signalled_used = new;
	signalled_used_valid = vq->signalled_used_valid;
	vq->signalled_used_valid = true;

	if (vq->driver_event->flags != VRING_EVENT_F_DESC) {
		if (vq->driver_event->flags != VRING_EVENT_F_DISABLE)
			kick = true;
		goto kick;
	}

	if (unlikely(!signalled_used_valid)) {
		kick = true;
		goto kick;
	}

	rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

	off_wrap = vq->driver_event->off_wrap;
	off = off_wrap & ~(1 << 15);

	if (new <= old)
		old -= vq->size;

	if (vq->used_wrap_counter != off_wrap >> 15)
		off -= vq->size;

	if (vhost_need_event(off, new, old))
		kick = true;
kick:
	if (kick) {
		eventfd_write(vq->callfd, (eventfd_t)1);
		if (dev->notify_ops->guest_notified)
			dev->notify_ops->guest_notified(dev->vid);
	}
}

static __rte_always_inline void
free_ind_table(void *idesc)
{
	rte_free(idesc);
}

static __rte_always_inline void
restore_mbuf(struct rte_mbuf *m)
{
	uint32_t mbuf_size, priv_size;

	while (m) {
		priv_size = rte_pktmbuf_priv_size(m->pool);
		mbuf_size = sizeof(struct rte_mbuf) + priv_size;
		/* start of buffer is after mbuf structure and priv data */

		m->buf_addr = (char *)m + mbuf_size;
		m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
		m = m->next;
	}
}

static __rte_always_inline bool
mbuf_is_consumed(struct rte_mbuf *m)
{
	while (m) {
		if (rte_mbuf_refcnt_read(m) > 1)
			return false;
		m = m->next;
	}

	return true;
}

#endif /* _VHOST_NET_CDEV_H_ */
