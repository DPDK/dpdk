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
#include <rte_rwlock.h>
#include <rte_malloc.h>

#include "rte_vhost.h"
#include "rte_vdpa.h"

/* Used to indicate that the device is running on a data core */
#define VIRTIO_DEV_RUNNING 1
/* Used to indicate that the device is ready to operate */
#define VIRTIO_DEV_READY 2
/* Used to indicate that the built-in vhost net device backend is enabled */
#define VIRTIO_DEV_BUILTIN_VIRTIO_NET 4
/* Used to indicate that the device has its own data path and configured */
#define VIRTIO_DEV_VDPA_CONFIGURED 8

/* Backend value set by guest. */
#define VIRTIO_DEV_STOPPED -1

#define BUF_VECTOR_MAX 256

#define VHOST_LOG_CACHE_NR 32

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
 * A structure to hold some fields needed in zero copy code path,
 * mainly for associating an mbuf with the right desc_idx.
 */
struct zcopy_mbuf {
	struct rte_mbuf *mbuf;
	uint32_t desc_idx;
	uint16_t desc_count;
	uint16_t in_use;

	TAILQ_ENTRY(zcopy_mbuf) next;
};
TAILQ_HEAD(zcopy_mbuf_list, zcopy_mbuf);

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
	uint32_t len;
	uint32_t count;
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
	uint32_t		size;

	uint16_t		last_avail_idx;
	uint16_t		last_used_idx;
	/* Last used index we notify to front end. */
	uint16_t		signalled_used;
	bool			signalled_used_valid;
#define VIRTIO_INVALID_EVENTFD		(-1)
#define VIRTIO_UNINITIALIZED_EVENTFD	(-2)

	/* Backend value to determine if device should started/stopped */
	int			backend;
	int			enabled;
	int			access_ok;
	rte_spinlock_t		access_lock;

	/* Used to notify the guest (trigger interrupt) */
	int			callfd;
	/* Currently unused as polling mode is enabled */
	int			kickfd;

	/* Physical address of used ring, for logging */
	uint64_t		log_guest_addr;

	uint16_t		nr_zmbuf;
	uint16_t		zmbuf_size;
	uint16_t		last_zmbuf_idx;
	struct zcopy_mbuf	*zmbufs;
	struct zcopy_mbuf_list	zmbuf_list;

	union {
		struct vring_used_elem  *shadow_used_split;
		struct vring_used_elem_packed *shadow_used_packed;
	};
	uint16_t                shadow_used_idx;
	struct vhost_vring_addr ring_addrs;

	struct batch_copy_elem	*batch_copy_elems;
	uint16_t		batch_copy_nb_elems;
	bool			used_wrap_counter;
	bool			avail_wrap_counter;

	struct log_cache_entry log_cache[VHOST_LOG_CACHE_NR];
	uint16_t log_cache_nb_elem;

	rte_rwlock_t	iotlb_lock;
	rte_rwlock_t	iotlb_pending_lock;
	struct rte_mempool *iotlb_pool;
	TAILQ_HEAD(, vhost_iotlb_entry) iotlb_list;
	int				iotlb_cache_nr;
	TAILQ_HEAD(, vhost_iotlb_entry) iotlb_pending_list;
} __rte_cache_aligned;

/* Old kernels have no such macros defined */
#ifndef VIRTIO_NET_F_GUEST_ANNOUNCE
 #define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#endif

#ifndef VIRTIO_NET_F_MQ
 #define VIRTIO_NET_F_MQ		22
#endif

#define VHOST_MAX_VRING			0x100
#define VHOST_MAX_QUEUE_PAIRS		0x80

#ifndef VIRTIO_NET_F_MTU
 #define VIRTIO_NET_F_MTU 3
#endif

#ifndef VIRTIO_F_ANY_LAYOUT
 #define VIRTIO_F_ANY_LAYOUT		27
#endif

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
	uint64_t host_phys_addr;
	uint64_t size;
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
	rte_atomic16_t		broadcast_rarp;
	uint32_t		nr_vring;
	int			dequeue_zero_copy;
	struct vhost_virtqueue	*virtqueue[VHOST_MAX_QUEUE_PAIRS * 2];
#define IF_NAME_SZ (PATH_MAX > IFNAMSIZ ? PATH_MAX : IFNAMSIZ)
	char			ifname[IF_NAME_SZ];
	uint64_t		log_size;
	uint64_t		log_base;
	uint64_t		log_addr;
	struct ether_addr	mac;
	uint16_t		mtu;

	struct vhost_device_ops const *notify_ops;

	uint32_t		nr_guest_pages;
	uint32_t		max_guest_pages;
	struct guest_page       *guest_pages;

	int			slave_req_fd;
	rte_spinlock_t		slave_req_lock;

	int			postcopy_ufd;
	int			postcopy_listening;

	/*
	 * Device id to identify a specific backend device.
	 * It's set to -1 for the default software implementation.
	 */
	int			vdpa_dev_id;

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
	uint16_t flags = *((volatile uint16_t *) &desc->flags);

	return wrap_counter == !!(flags & VRING_DESC_F_AVAIL) &&
		wrap_counter != !!(flags & VRING_DESC_F_USED);
}

#define VHOST_LOG_PAGE	4096

/*
 * Atomically set a bit in memory.
 */
static __rte_always_inline void
vhost_set_bit(unsigned int nr, volatile uint8_t *addr)
{
#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION < 70100)
	/*
	 * __sync_ built-ins are deprecated, but __atomic_ ones
	 * are sub-optimized in older GCC versions.
	 */
	__sync_fetch_and_or_1(addr, (1U << nr));
#else
	__atomic_fetch_or(addr, (1U << nr), __ATOMIC_RELAXED);
#endif
}

static __rte_always_inline void
vhost_log_page(uint8_t *log_base, uint64_t page)
{
	vhost_set_bit(page % 8, &log_base[page / 8]);
}

static __rte_always_inline void
vhost_log_write(struct virtio_net *dev, uint64_t addr, uint64_t len)
{
	uint64_t page;

	if (likely(((dev->features & (1ULL << VHOST_F_LOG_ALL)) == 0) ||
		   !dev->log_base || !len))
		return;

	if (unlikely(dev->log_size <= ((addr + len - 1) / VHOST_LOG_PAGE / 8)))
		return;

	/* To make sure guest memory updates are committed before logging */
	rte_smp_wmb();

	page = addr / VHOST_LOG_PAGE;
	while (page * VHOST_LOG_PAGE < addr + len) {
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);
		page += 1;
	}
}

static __rte_always_inline void
vhost_log_cache_sync(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	unsigned long *log_base;
	int i;

	if (likely(((dev->features & (1ULL << VHOST_F_LOG_ALL)) == 0) ||
		   !dev->log_base))
		return;

	rte_smp_wmb();

	log_base = (unsigned long *)(uintptr_t)dev->log_base;

	for (i = 0; i < vq->log_cache_nb_elem; i++) {
		struct log_cache_entry *elem = vq->log_cache + i;

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION < 70100)
		/*
		 * '__sync' builtins are deprecated, but '__atomic' ones
		 * are sub-optimized in older GCC versions.
		 */
		__sync_fetch_and_or(log_base + elem->offset, elem->val);
#else
		__atomic_fetch_or(log_base + elem->offset, elem->val,
				__ATOMIC_RELAXED);
#endif
	}

	rte_smp_wmb();

	vq->log_cache_nb_elem = 0;
}

static __rte_always_inline void
vhost_log_cache_page(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t page)
{
	uint32_t bit_nr = page % (sizeof(unsigned long) << 3);
	uint32_t offset = page / (sizeof(unsigned long) << 3);
	int i;

	for (i = 0; i < vq->log_cache_nb_elem; i++) {
		struct log_cache_entry *elem = vq->log_cache + i;

		if (elem->offset == offset) {
			elem->val |= (1UL << bit_nr);
			return;
		}
	}

	if (unlikely(i >= VHOST_LOG_CACHE_NR)) {
		/*
		 * No more room for a new log cache entry,
		 * so write the dirty log map directly.
		 */
		rte_smp_wmb();
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);

		return;
	}

	vq->log_cache[i].offset = offset;
	vq->log_cache[i].val = (1UL << bit_nr);
	vq->log_cache_nb_elem++;
}

static __rte_always_inline void
vhost_log_cache_write(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t addr, uint64_t len)
{
	uint64_t page;

	if (likely(((dev->features & (1ULL << VHOST_F_LOG_ALL)) == 0) ||
		   !dev->log_base || !len))
		return;

	if (unlikely(dev->log_size <= ((addr + len - 1) / VHOST_LOG_PAGE / 8)))
		return;

	page = addr / VHOST_LOG_PAGE;
	while (page * VHOST_LOG_PAGE < addr + len) {
		vhost_log_cache_page(dev, vq, page);
		page += 1;
	}
}

static __rte_always_inline void
vhost_log_cache_used_vring(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t offset, uint64_t len)
{
	vhost_log_cache_write(dev, vq, vq->log_guest_addr + offset, len);
}

static __rte_always_inline void
vhost_log_used_vring(struct virtio_net *dev, struct vhost_virtqueue *vq,
		     uint64_t offset, uint64_t len)
{
	vhost_log_write(dev, vq->log_guest_addr + offset, len);
}

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER1

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VHOST_MAX_PRINT_BUFF 6072
#define VHOST_LOG_DEBUG(log_type, fmt, args...) \
	RTE_LOG(DEBUG, log_type, fmt, ##args)
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
	VHOST_LOG_DEBUG(VHOST_DATA, "%s", packet); \
} while (0)
#else
#define VHOST_LOG_DEBUG(log_type, fmt, args...) do {} while (0)
#define PRINT_PACKET(device, addr, size, header) do {} while (0)
#endif

extern uint64_t VHOST_FEATURES;
#define MAX_VHOST_DEVICE	1024
extern struct virtio_net *vhost_devices[MAX_VHOST_DEVICE];

/* Convert guest physical address to host physical address */
static __rte_always_inline rte_iova_t
gpa_to_hpa(struct virtio_net *dev, uint64_t gpa, uint64_t size)
{
	uint32_t i;
	struct guest_page *page;

	for (i = 0; i < dev->nr_guest_pages; i++) {
		page = &dev->guest_pages[i];

		if (gpa >= page->guest_phys_addr &&
		    gpa + size < page->guest_phys_addr + page->size) {
			return gpa - page->guest_phys_addr +
			       page->host_phys_addr;
		}
	}

	return 0;
}

static __rte_always_inline struct virtio_net *
get_device(int vid)
{
	struct virtio_net *dev = vhost_devices[vid];

	if (unlikely(!dev)) {
		RTE_LOG(ERR, VHOST_CONFIG,
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
void free_vq(struct virtio_net *dev, struct vhost_virtqueue *vq);

int alloc_vring_queue(struct virtio_net *dev, uint32_t vring_idx);

void vhost_attach_vdpa_device(int vid, int did);

void vhost_set_ifname(int, const char *if_name, unsigned int if_len);
void vhost_enable_dequeue_zero_copy(int vid);
void vhost_set_builtin_virtio_net(int vid, bool enable);

struct vhost_device_ops const *vhost_driver_callback_get(const char *path);

/*
 * Backend-specific cleanup.
 *
 * TODO: fix it; we have one backend now
 */
void vhost_backend_cleanup(struct virtio_net *dev);

uint64_t __vhost_iova_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t iova, uint64_t *len, uint8_t perm);
int vring_translate(struct virtio_net *dev, struct vhost_virtqueue *vq);
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
	rte_smp_mb();

	/* Don't kick guest if we don't reach index specified by guest. */
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX)) {
		uint16_t old = vq->signalled_used;
		uint16_t new = vq->last_used_idx;
		bool signalled_used_valid = vq->signalled_used_valid;

		vq->signalled_used = new;
		vq->signalled_used_valid = true;

		VHOST_LOG_DEBUG(VHOST_DATA, "%s: used_event_idx=%d, old=%d, new=%d\n",
			__func__,
			vhost_used_event(vq),
			old, new);

		if ((vhost_need_event(vhost_used_event(vq), new, old) &&
					(vq->callfd >= 0)) ||
				unlikely(!signalled_used_valid))
			eventfd_write(vq->callfd, (eventfd_t) 1);
	} else {
		/* Kick the guest if necessary. */
		if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
				&& (vq->callfd >= 0))
			eventfd_write(vq->callfd, (eventfd_t)1);
	}
}

static __rte_always_inline void
vhost_vring_call_packed(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t old, new, off, off_wrap;
	bool signalled_used_valid, kick = false;

	/* Flush used desc update. */
	rte_smp_mb();

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

	rte_smp_rmb();

	off_wrap = vq->driver_event->off_wrap;
	off = off_wrap & ~(1 << 15);

	if (new <= old)
		old -= vq->size;

	if (vq->used_wrap_counter != off_wrap >> 15)
		off -= vq->size;

	if (vhost_need_event(off, new, old))
		kick = true;
kick:
	if (kick)
		eventfd_write(vq->callfd, (eventfd_t)1);
}

static __rte_always_inline void *
alloc_copy_ind_table(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t desc_addr, uint64_t desc_len)
{
	void *idesc;
	uint64_t src, dst;
	uint64_t len, remain = desc_len;

	idesc = rte_malloc(__func__, desc_len, 0);
	if (unlikely(!idesc))
		return 0;

	dst = (uint64_t)(uintptr_t)idesc;

	while (remain) {
		len = remain;
		src = vhost_iova_to_vva(dev, vq, desc_addr, &len,
				VHOST_ACCESS_RO);
		if (unlikely(!src || !len)) {
			rte_free(idesc);
			return 0;
		}

		rte_memcpy((void *)(uintptr_t)dst, (void *)(uintptr_t)src, len);

		remain -= len;
		dst += len;
		desc_addr += len;
	}

	return idesc;
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

static __rte_always_inline void
put_zmbuf(struct zcopy_mbuf *zmbuf)
{
	zmbuf->in_use = 0;
}

#endif /* _VHOST_NET_CDEV_H_ */
