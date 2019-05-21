/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <rte_ether.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_VHOST_PORT   RTE_LOGTYPE_USER3

/* State of virtio device. */
#define DEVICE_MAC_LEARNING	0
#define DEVICE_RX		1
#define DEVICE_SAFE_REMOVE	2

#define MAX_QUEUES 512

/* Max burst size for RX/TX */
#define MAX_PKT_BURST 32

/* Max number of devices. Limited by the application. */
#define MAX_DEVICES 64

enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

/* Per-device statistics struct */
struct device_statistics {
	uint64_t tx_total;
	rte_atomic64_t rx_total_atomic;
	uint64_t rx_total;
	uint64_t tx;
	rte_atomic64_t rx_atomic;
	/**< Bad inner IP csum for tunneling pkt */
	rte_atomic64_t rx_bad_ip_csum;
	/**< Bad inner L4 csum for tunneling pkt */
	rte_atomic64_t rx_bad_l4_csum;
} __rte_cache_aligned;

/**
 * Device linked list structure for data path.
 */
struct vhost_dev {
	int vid;
	/**< Number of memory regions for gpa to hpa translation. */
	uint32_t nregions_hpa;
	/**< Memory region information for gpa to hpa translation. */
	struct virtio_memory_regions_hpa *regions_hpa;
	/**< Device MAC address (Obtained on first TX packet). */
	struct rte_ether_addr mac_address;
	/**< RX queue number. */
	uint16_t rx_q;
	/**< Data core that the device is added to. */
	uint16_t coreid;
	/**< A device is set as ready if the MAC address has been set. */
	volatile uint8_t ready;
	/**< Device is marked for removal from the data core. */
	volatile uint8_t remove;
} __rte_cache_aligned;

/**
 * Structure containing data core specific information.
 */
struct lcore_ll_info {
	/**< Pointer to head in free linked list. */
	struct virtio_net_data_ll *ll_root_free;
	/**< Pointer to head of used linked list. */
	struct virtio_net_data_ll *ll_root_used;
	/**< Number of devices on lcore. */
	uint32_t device_num;
	/**< Flag to synchronize device removal. */
	volatile uint8_t dev_removal_flag;
};

struct lcore_info {
	/**< Pointer to data core specific lcore_ll_info struct */
	struct lcore_ll_info	*lcore_ll;
};

struct virtio_net_data_ll {
	/**< Pointer to device created by configuration core. */
	struct vhost_dev            *vdev;
	/**< Pointer to next device in linked list. */
	struct virtio_net_data_ll   *next;
};

uint32_t
virtio_dev_rx(int vid, struct rte_mbuf **pkts, uint32_t count);

#endif /* _MAIN_H_ */
