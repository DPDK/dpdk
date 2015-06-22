/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <rte_ether.h>

#ifdef DEBUG
#define LOG_LEVEL RTE_LOG_DEBUG
#define LOG_DEBUG(log_type, fmt, args...) RTE_LOG(DEBUG, log_type, fmt, ##args)
#else
#define LOG_LEVEL RTE_LOG_INFO
#define LOG_DEBUG(log_type, fmt, args...) do {} while (0)
#endif

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
	/**< Pointer to device created by vhost lib. */
	struct virtio_net      *dev;
	/**< Number of memory regions for gpa to hpa translation. */
	uint32_t nregions_hpa;
	/**< Memory region information for gpa to hpa translation. */
	struct virtio_memory_regions_hpa *regions_hpa;
	/**< Device MAC address (Obtained on first TX packet). */
	struct ether_addr mac_address;
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
virtio_dev_rx(struct virtio_net *dev, struct rte_mbuf **pkts, uint32_t count);

#endif /* _MAIN_H_ */
