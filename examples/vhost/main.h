/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include <sys/queue.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_VHOST_PORT   RTE_LOGTYPE_USER3

struct device_statistics {
	uint64_t	tx;
	uint64_t	tx_total;
	rte_atomic64_t	rx_atomic;
	rte_atomic64_t	rx_total_atomic;
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
	struct device_statistics stats;
	TAILQ_ENTRY(vhost_dev) global_vdev_entry;
	TAILQ_ENTRY(vhost_dev) lcore_vdev_entry;
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

#endif /* _MAIN_H_ */
