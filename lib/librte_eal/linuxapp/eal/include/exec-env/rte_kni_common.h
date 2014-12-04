/*-
 *   This file is provided under a dual BSD/LGPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GNU LESSER GENERAL PUBLIC LICENSE
 *
 *   Copyright(c) 2007-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2.1 of the GNU Lesser General Public License
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *   Contact Information:
 *   Intel Corporation
 *
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _RTE_KNI_COMMON_H_
#define _RTE_KNI_COMMON_H_

#ifdef __KERNEL__
#include <linux/if.h>
#endif

/**
 * KNI name is part of memzone name.
 */
#define RTE_KNI_NAMESIZE 32

#ifndef RTE_CACHE_LINE_SIZE
#define RTE_CACHE_LINE_SIZE 64       /**< Cache line size. */
#endif

/*
 * Request id.
 */
enum rte_kni_req_id {
	RTE_KNI_REQ_UNKNOWN = 0,
	RTE_KNI_REQ_CHANGE_MTU,
	RTE_KNI_REQ_CFG_NETWORK_IF,
	RTE_KNI_REQ_MAX,
};

/*
 * Structure for KNI request.
 */
struct rte_kni_request {
	uint32_t req_id;             /**< Request id */
	union {
		uint32_t new_mtu;    /**< New MTU */
		uint8_t if_up;       /**< 1: interface up, 0: interface down */
	};
	int32_t result;               /**< Result for processing request */
} __attribute__((__packed__));

/*
 * Fifo struct mapped in a shared memory. It describes a circular buffer FIFO
 * Write and read should wrap around. Fifo is empty when write == read
 * Writing should never overwrite the read position
 */
struct rte_kni_fifo {
	volatile unsigned write;     /**< Next position to be written*/
	volatile unsigned read;      /**< Next position to be read */
	unsigned len;                /**< Circular buffer length */
	unsigned elem_size;          /**< Pointer size - for 32/64 bit OS */
	void * volatile buffer[0];   /**< The buffer contains mbuf pointers */
};

/*
 * The kernel image of the rte_mbuf struct, with only the relevant fields.
 * Padding is necessary to assure the offsets of these fields
 */
struct rte_kni_mbuf {
	void *buf_addr __attribute__((__aligned__(RTE_CACHE_LINE_SIZE)));
	char pad0[10];
	uint16_t data_off;      /**< Start address of data in segment buffer. */
	char pad1[4];
	uint64_t ol_flags;      /**< Offload features. */
	char pad2[2];
	uint16_t data_len;      /**< Amount of data in segment buffer. */
	uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */

	/* fields on second cache line */
	char pad3[8] __attribute__((__aligned__(RTE_CACHE_LINE_SIZE)));
	void *pool;
	void *next;
};

/*
 * Struct used to create a KNI device. Passed to the kernel in IOCTL call
 */

struct rte_kni_device_info {
	char name[RTE_KNI_NAMESIZE];  /**< Network device name for KNI */

	phys_addr_t tx_phys;
	phys_addr_t rx_phys;
	phys_addr_t alloc_phys;
	phys_addr_t free_phys;

	/* Used by Ethtool */
	phys_addr_t req_phys;
	phys_addr_t resp_phys;
	phys_addr_t sync_phys;
	void * sync_va;

	/* mbuf mempool */
	void * mbuf_va;
	phys_addr_t mbuf_phys;

	/* PCI info */
	uint16_t vendor_id;           /**< Vendor ID or PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or PCI_ANY_ID. */
	uint8_t bus;                  /**< Device bus */
	uint8_t devid;                /**< Device ID */
	uint8_t function;             /**< Device function. */

	uint16_t group_id;            /**< Group ID */
	uint32_t core_id;             /**< core ID to bind for kernel thread */

	uint8_t force_bind : 1;       /**< Flag for kernel thread binding */

	/* mbuf size */
	unsigned mbuf_size;
};

#define KNI_DEVICE "kni"

#define RTE_KNI_IOCTL_TEST    _IOWR(0, 1, int)
#define RTE_KNI_IOCTL_CREATE  _IOWR(0, 2, struct rte_kni_device_info)
#define RTE_KNI_IOCTL_RELEASE _IOWR(0, 3, struct rte_kni_device_info)

#endif /* _RTE_KNI_COMMON_H_ */
