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

#ifndef _VHOST_NET_CDEV_H_
#define _VHOST_NET_CDEV_H_
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <unistd.h>
#include <linux/vhost.h>

#include <rte_log.h>

#include "rte_virtio_net.h"

/* Used to indicate that the device is running on a data core */
#define VIRTIO_DEV_RUNNING 1

/* Backend value set by guest. */
#define VIRTIO_DEV_STOPPED -1

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

/*
 * A structure to hold some fields needed in zero copy code path,
 * mainly for associating an mbuf with the right desc_idx.
 */
struct zcopy_mbuf {
	struct rte_mbuf *mbuf;
	uint32_t desc_idx;
	uint16_t in_use;

	TAILQ_ENTRY(zcopy_mbuf) next;
};
TAILQ_HEAD(zcopy_mbuf_list, zcopy_mbuf);

/**
 * Structure contains variables relevant to RX/TX virtqueues.
 */
struct vhost_virtqueue {
	struct vring_desc	*desc;
	struct vring_avail	*avail;
	struct vring_used	*used;
	uint32_t		size;

	uint16_t		last_avail_idx;
	uint16_t		last_used_idx;
#define VIRTIO_INVALID_EVENTFD		(-1)
#define VIRTIO_UNINITIALIZED_EVENTFD	(-2)

	/* Backend value to determine if device should started/stopped */
	int			backend;
	/* Used to notify the guest (trigger interrupt) */
	int			callfd;
	/* Currently unused as polling mode is enabled */
	int			kickfd;
	int			enabled;

	/* Physical address of used ring, for logging */
	uint64_t		log_guest_addr;

	uint16_t		nr_zmbuf;
	uint16_t		zmbuf_size;
	uint16_t		last_zmbuf_idx;
	struct zcopy_mbuf	*zmbufs;
	struct zcopy_mbuf_list	zmbuf_list;

	struct vring_used_elem  *shadow_used_ring;
	uint16_t                shadow_used_idx;
} __rte_cache_aligned;

/* Old kernels have no such macro defined */
#ifndef VIRTIO_NET_F_GUEST_ANNOUNCE
 #define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#endif


/*
 * Make an extra wrapper for VIRTIO_NET_F_MQ and
 * VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX as they are
 * introduced since kernel v3.8. This makes our
 * code buildable for older kernel.
 */
#ifdef VIRTIO_NET_F_MQ
 #define VHOST_MAX_QUEUE_PAIRS	VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX
 #define VHOST_SUPPORTS_MQ	(1ULL << VIRTIO_NET_F_MQ)
#else
 #define VHOST_MAX_QUEUE_PAIRS	1
 #define VHOST_SUPPORTS_MQ	0
#endif

/*
 * Define virtio 1.0 for older kernels
 */
#ifndef VIRTIO_F_VERSION_1
 #define VIRTIO_F_VERSION_1 32
#endif

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
	struct virtio_memory	*mem;
	uint64_t		features;
	uint64_t		protocol_features;
	int			vid;
	uint32_t		flags;
	uint16_t		vhost_hlen;
	/* to tell if we need broadcast rarp packet */
	rte_atomic16_t		broadcast_rarp;
	uint32_t		virt_qp_nb;
	int			dequeue_zero_copy;
	struct vhost_virtqueue	*virtqueue[VHOST_MAX_QUEUE_PAIRS * 2];
#define IF_NAME_SZ (PATH_MAX > IFNAMSIZ ? PATH_MAX : IFNAMSIZ)
	char			ifname[IF_NAME_SZ];
	uint64_t		log_size;
	uint64_t		log_base;
	uint64_t		log_addr;
	struct ether_addr	mac;

	uint32_t		nr_guest_pages;
	uint32_t		max_guest_pages;
	struct guest_page       *guest_pages;
} __rte_cache_aligned;

/**
 * Information relating to memory regions including offsets to
 * addresses in QEMUs memory file.
 */
struct virtio_memory_region {
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
struct virtio_memory {
	uint32_t nregions;
	struct virtio_memory_region regions[0];
};


/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER1

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VHOST_MAX_PRINT_BUFF 6072
#define LOG_LEVEL RTE_LOG_DEBUG
#define LOG_DEBUG(log_type, fmt, args...) RTE_LOG(DEBUG, log_type, fmt, ##args)
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
	LOG_DEBUG(VHOST_DATA, "%s", packet); \
} while (0)
#else
#define LOG_LEVEL RTE_LOG_INFO
#define LOG_DEBUG(log_type, fmt, args...) do {} while (0)
#define PRINT_PACKET(device, addr, size, header) do {} while (0)
#endif

extern uint64_t VHOST_FEATURES;
#define MAX_VHOST_DEVICE	1024
extern struct virtio_net *vhost_devices[MAX_VHOST_DEVICE];

/* Convert guest physical Address to host virtual address */
static inline uint64_t __attribute__((always_inline))
gpa_to_vva(struct virtio_net *dev, uint64_t gpa)
{
	struct virtio_memory_region *reg;
	uint32_t i;

	for (i = 0; i < dev->mem->nregions; i++) {
		reg = &dev->mem->regions[i];
		if (gpa >= reg->guest_phys_addr &&
		    gpa <  reg->guest_phys_addr + reg->size) {
			return gpa - reg->guest_phys_addr +
			       reg->host_user_addr;
		}
	}

	return 0;
}

/* Convert guest physical address to host physical address */
static inline phys_addr_t __attribute__((always_inline))
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

struct virtio_net_device_ops const *notify_ops;
struct virtio_net *get_device(int vid);

int vhost_new_device(void);
void cleanup_device(struct virtio_net *dev, int destroy);
void reset_device(struct virtio_net *dev);
void vhost_destroy_device(int);

int alloc_vring_queue_pair(struct virtio_net *dev, uint32_t qp_idx);

void vhost_set_ifname(int, const char *if_name, unsigned int if_len);
void vhost_enable_dequeue_zero_copy(int vid);

/*
 * Backend-specific cleanup.
 *
 * TODO: fix it; we have one backend now
 */
void vhost_backend_cleanup(struct virtio_net *dev);

#endif /* _VHOST_NET_CDEV_H_ */
