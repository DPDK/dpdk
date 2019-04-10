/* SPDX-License-Identifier: (BSD-3-Clause OR LGPL-2.1) */
/*
 * Copyright(c) 2007-2014 Intel Corporation.
 */

#ifndef _RTE_KNI_COMMON_H_
#define _RTE_KNI_COMMON_H_

#ifdef __KERNEL__
#include <linux/if.h>
#include <asm/barrier.h>
#define RTE_STD_C11
#else
#include <rte_common.h>
#include <rte_config.h>
#endif

/**
 * KNI name is part of memzone name.
 */
#define RTE_KNI_NAMESIZE 32

#define RTE_CACHE_LINE_MIN_SIZE 64

/*
 * Request id.
 */
enum rte_kni_req_id {
	RTE_KNI_REQ_UNKNOWN = 0,
	RTE_KNI_REQ_CHANGE_MTU,
	RTE_KNI_REQ_CFG_NETWORK_IF,
	RTE_KNI_REQ_CHANGE_MAC_ADDR,
	RTE_KNI_REQ_CHANGE_PROMISC,
	RTE_KNI_REQ_MAX,
};

/*
 * Structure for KNI request.
 */
struct rte_kni_request {
	uint32_t req_id;             /**< Request id */
	RTE_STD_C11
	union {
		uint32_t new_mtu;    /**< New MTU */
		uint8_t if_up;       /**< 1: interface up, 0: interface down */
		uint8_t mac_addr[6]; /**< MAC address for interface */
		uint8_t promiscusity;/**< 1: promisc mode enable, 0: disable */
	};
	int32_t result;               /**< Result for processing request */
} __attribute__((__packed__));

/*
 * Fifo struct mapped in a shared memory. It describes a circular buffer FIFO
 * Write and read should wrap around. Fifo is empty when write == read
 * Writing should never overwrite the read position
 */
struct rte_kni_fifo {
#ifdef RTE_USE_C11_MEM_MODEL
	unsigned write;              /**< Next position to be written*/
	unsigned read;               /**< Next position to be read */
#else
	volatile unsigned write;     /**< Next position to be written*/
	volatile unsigned read;      /**< Next position to be read */
#endif
	unsigned len;                /**< Circular buffer length */
	unsigned elem_size;          /**< Pointer size - for 32/64 bit OS */
	void *volatile buffer[];     /**< The buffer contains mbuf pointers */
};

/*
 * The kernel image of the rte_mbuf struct, with only the relevant fields.
 * Padding is necessary to assure the offsets of these fields
 */
struct rte_kni_mbuf {
	void *buf_addr __attribute__((__aligned__(RTE_CACHE_LINE_SIZE)));
	uint64_t buf_physaddr;
	uint16_t data_off;      /**< Start address of data in segment buffer. */
	char pad1[2];
	uint16_t nb_segs;       /**< Number of segments. */
	char pad4[2];
	uint64_t ol_flags;      /**< Offload features. */
	char pad2[4];
	uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */
	uint16_t data_len;      /**< Amount of data in segment buffer. */

	/* fields on second cache line */
	char pad3[8] __attribute__((__aligned__(RTE_CACHE_LINE_MIN_SIZE)));
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

	__extension__
	uint8_t force_bind : 1;       /**< Flag for kernel thread binding */

	/* mbuf size */
	unsigned mbuf_size;
	unsigned int mtu;
	uint8_t mac_addr[6];
};

#define KNI_DEVICE "kni"

#define RTE_KNI_IOCTL_TEST    _IOWR(0, 1, int)
#define RTE_KNI_IOCTL_CREATE  _IOWR(0, 2, struct rte_kni_device_info)
#define RTE_KNI_IOCTL_RELEASE _IOWR(0, 3, struct rte_kni_device_info)

#endif /* _RTE_KNI_COMMON_H_ */
