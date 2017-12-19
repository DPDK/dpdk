/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <sys/queue.h>

#include <rte_ether.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_VHOST_PORT   RTE_LOGTYPE_USER3

enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define MAX_PKT_BURST 32		/* Max burst size for RX/TX */

struct device_statistics {
	uint64_t	tx;
	uint64_t	tx_total;
	rte_atomic64_t	rx_atomic;
	rte_atomic64_t	rx_total_atomic;
};

struct vhost_queue {
	struct rte_vhost_vring	vr;
	uint16_t		last_avail_idx;
	uint16_t		last_used_idx;
};

struct vhost_dev {
	/**< Number of memory regions for gpa to hpa translation. */
	uint32_t nregions_hpa;
	/**< Device MAC address (Obtained on first TX packet). */
	struct ether_addr mac_address;
	/**< RX VMDQ queue number. */
	uint16_t vmdq_rx_q;
	/**< Vlan tag assigned to the pool */
	uint32_t vlan_tag;
	/**< Data core that the device is added to. */
	uint16_t coreid;
	/**< A device is set as ready if the MAC address has been set. */
	volatile uint8_t ready;
	/**< Device is marked for removal from the data core. */
	volatile uint8_t remove;

	int vid;
	uint64_t features;
	size_t hdr_len;
	uint16_t nr_vrings;
	struct rte_vhost_memory *mem;
	struct device_statistics stats;
	TAILQ_ENTRY(vhost_dev) global_vdev_entry;
	TAILQ_ENTRY(vhost_dev) lcore_vdev_entry;

#define MAX_QUEUE_PAIRS	4
	struct vhost_queue queues[MAX_QUEUE_PAIRS * 2];
} __rte_cache_aligned;

TAILQ_HEAD(vhost_dev_tailq_list, vhost_dev);


#define REQUEST_DEV_REMOVAL	1
#define ACK_DEV_REMOVAL		0

/*
 * Structure containing data core specific information.
 */
struct lcore_info {
	uint32_t		device_num;

	/* Flag to synchronize device removal. */
	volatile uint8_t	dev_removal_flag;

	struct vhost_dev_tailq_list vdev_list;
};

/* we implement non-extra virtio net features */
#define VIRTIO_NET_FEATURES	0

void vs_vhost_net_setup(struct vhost_dev *dev);
void vs_vhost_net_remove(struct vhost_dev *dev);
uint16_t vs_enqueue_pkts(struct vhost_dev *dev, uint16_t queue_id,
			 struct rte_mbuf **pkts, uint32_t count);

uint16_t vs_dequeue_pkts(struct vhost_dev *dev, uint16_t queue_id,
			 struct rte_mempool *mbuf_pool,
			 struct rte_mbuf **pkts, uint16_t count);
#endif /* _MAIN_H_ */
