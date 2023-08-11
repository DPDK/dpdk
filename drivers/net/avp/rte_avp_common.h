/* SPDX-License-Identifier: (BSD-3-Clause OR LGPL-2.1)
 * Copyright(c) 2010-2013 Intel Corporation.
 * Copyright(c) 2014-2017 Wind River Systems, Inc.
 */

#ifndef _RTE_AVP_COMMON_H_
#define _RTE_AVP_COMMON_H_

#ifdef __KERNEL__
#include <linux/if.h>
#else
#include <stdint.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_memory.h>
#include <rte_ether.h>
#include <rte_atomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AVP name is part of network device name.
 */
#define RTE_AVP_NAMESIZE 32

/**
 * AVP alias is a user-defined value used for lookups from secondary
 * processes.  Typically, this is a UUID.
 */
#define RTE_AVP_ALIASSIZE 128

/*
 * Request id.
 */
enum rte_avp_req_id {
	RTE_AVP_REQ_UNKNOWN = 0,
	RTE_AVP_REQ_CHANGE_MTU,
	RTE_AVP_REQ_CFG_NETWORK_IF,
	RTE_AVP_REQ_CFG_DEVICE,
	RTE_AVP_REQ_SHUTDOWN_DEVICE,
	RTE_AVP_REQ_MAX,
};

/**@{ AVP device driver types */
#define RTE_AVP_DRIVER_TYPE_UNKNOWN 0
#define RTE_AVP_DRIVER_TYPE_DPDK 1
#define RTE_AVP_DRIVER_TYPE_KERNEL 2
#define RTE_AVP_DRIVER_TYPE_QEMU 3
/**@} */

/**@{ AVP device operational modes */
#define RTE_AVP_MODE_HOST 0 /**< AVP interface created in host */
#define RTE_AVP_MODE_GUEST 1 /**< AVP interface created for export to guest */
#define RTE_AVP_MODE_TRACE 2 /**< AVP interface created for packet tracing */
/**@} */

/*
 * Structure for AVP queue configuration query request/result
 */
struct rte_avp_device_config {
	uint64_t device_id;	/**< Unique system identifier */
	uint32_t driver_type; /**< Device Driver type */
	uint32_t driver_version; /**< Device Driver version */
	uint32_t features; /**< Negotiated features */
	uint16_t num_tx_queues;	/**< Number of active transmit queues */
	uint16_t num_rx_queues;	/**< Number of active receive queues */
	uint8_t if_up; /**< 1: interface up, 0: interface down */
} __rte_packed;

/*
 * Structure for AVP request.
 */
struct rte_avp_request {
	uint32_t req_id; /**< Request id */
	union {
		uint32_t new_mtu; /**< New MTU */
		uint8_t if_up;	/**< 1: interface up, 0: interface down */
	struct rte_avp_device_config config; /**< Queue configuration */
	};
	int32_t result;	/**< Result for processing request */
} __rte_packed;

/*
 * FIFO struct mapped in a shared memory. It describes a circular buffer FIFO
 * Write and read should wrap around. FIFO is empty when write == read
 * Writing should never overwrite the read position
 */
struct rte_avp_fifo {
	volatile unsigned int write; /**< Next position to be written*/
	volatile unsigned int read; /**< Next position to be read */
	unsigned int len; /**< Circular buffer length */
	unsigned int elem_size; /**< Pointer size - for 32/64 bit OS */
	void *volatile buffer[]; /**< The buffer contains mbuf pointers */
};


/*
 * AVP packet buffer header used to define the exchange of packet data.
 */
struct rte_avp_desc {
	uint64_t pad0;
	void *pkt_mbuf; /**< Reference to packet mbuf */
	uint8_t pad1[14];
	uint16_t ol_flags; /**< Offload features. */
	void *next;	/**< Reference to next buffer in chain */
	void *data;	/**< Start address of data in segment buffer. */
	uint16_t data_len; /**< Amount of data in segment buffer. */
	uint8_t nb_segs; /**< Number of segments */
	uint8_t pad2;
	uint16_t pkt_len; /**< Total pkt len: sum of all segment data_len. */
	uint32_t pad3;
	uint16_t vlan_tci; /**< VLAN Tag Control Identifier (CPU order). */
	uint32_t pad4;
} __rte_packed __rte_cache_aligned;


/**{ AVP device features */
#define RTE_AVP_FEATURE_VLAN_OFFLOAD (1 << 0) /**< Emulated HW VLAN offload */
/**@} */


/**@{ Offload feature flags */
#define RTE_AVP_TX_VLAN_PKT 0x0001 /**< TX packet is a 802.1q VLAN packet. */
#define RTE_AVP_RX_VLAN_PKT 0x0800 /**< RX packet is a 802.1q VLAN packet. */
/**@} */


/**@{ AVP PCI identifiers */
#define RTE_AVP_PCI_VENDOR_ID   0x1af4
#define RTE_AVP_PCI_DEVICE_ID   0x1110
/**@} */

/**@{ AVP PCI subsystem identifiers */
#define RTE_AVP_PCI_SUB_VENDOR_ID RTE_AVP_PCI_VENDOR_ID
#define RTE_AVP_PCI_SUB_DEVICE_ID 0x1104
/**@} */

/**@{ AVP PCI BAR definitions */
#define RTE_AVP_PCI_MMIO_BAR   0
#define RTE_AVP_PCI_MSIX_BAR   1
#define RTE_AVP_PCI_MEMORY_BAR 2
#define RTE_AVP_PCI_MEMMAP_BAR 4
#define RTE_AVP_PCI_DEVICE_BAR 5
#define RTE_AVP_PCI_MAX_BAR    6
/**@} */

/**@{ AVP PCI BAR name definitions */
#define RTE_AVP_MMIO_BAR_NAME   "avp-mmio"
#define RTE_AVP_MSIX_BAR_NAME   "avp-msix"
#define RTE_AVP_MEMORY_BAR_NAME "avp-memory"
#define RTE_AVP_MEMMAP_BAR_NAME "avp-memmap"
#define RTE_AVP_DEVICE_BAR_NAME "avp-device"
/**@} */

/**@{ AVP PCI MSI-X vectors */
#define RTE_AVP_MIGRATION_MSIX_VECTOR 0	/**< Migration interrupts */
#define RTE_AVP_MAX_MSIX_VECTORS 1
/**@} */

/**@} AVP Migration status/ack register values */
#define RTE_AVP_MIGRATION_NONE      0 /**< Migration never executed */
#define RTE_AVP_MIGRATION_DETACHED  1 /**< Device attached during migration */
#define RTE_AVP_MIGRATION_ATTACHED  2 /**< Device reattached during migration */
#define RTE_AVP_MIGRATION_ERROR     3 /**< Device failed to attach/detach */
/**@} */

/**@} AVP MMIO Register Offsets */
#define RTE_AVP_REGISTER_BASE 0
#define RTE_AVP_INTERRUPT_MASK_OFFSET (RTE_AVP_REGISTER_BASE + 0)
#define RTE_AVP_INTERRUPT_STATUS_OFFSET (RTE_AVP_REGISTER_BASE + 4)
#define RTE_AVP_MIGRATION_STATUS_OFFSET (RTE_AVP_REGISTER_BASE + 8)
#define RTE_AVP_MIGRATION_ACK_OFFSET (RTE_AVP_REGISTER_BASE + 12)
/**@} */

/**@} AVP Interrupt Status Mask */
#define RTE_AVP_MIGRATION_INTERRUPT_MASK (1 << 1)
#define RTE_AVP_APP_INTERRUPTS_MASK      0xFFFFFFFF
#define RTE_AVP_NO_INTERRUPTS_MASK       0
/**@} */

/*
 * Maximum number of memory regions to export
 */
#define RTE_AVP_MAX_MAPS  2048

/*
 * Description of a single memory region
 */
struct rte_avp_memmap {
	void *addr;
	rte_iova_t phys_addr;
	uint64_t length;
};

/*
 * AVP memory mapping validation marker
 */
#define RTE_AVP_MEMMAP_MAGIC 0x20131969

/**@{  AVP memory map versions */
#define RTE_AVP_MEMMAP_VERSION_1 1
#define RTE_AVP_MEMMAP_VERSION RTE_AVP_MEMMAP_VERSION_1
/**@} */

/*
 * Defines a list of memory regions exported from the host to the guest
 */
struct rte_avp_memmap_info {
	uint32_t magic; /**< Memory validation marker */
	uint32_t version; /**< Data format version */
	uint32_t nb_maps;
	struct rte_avp_memmap maps[RTE_AVP_MAX_MAPS];
};

/*
 * AVP device memory validation marker
 */
#define RTE_AVP_DEVICE_MAGIC 0x20131975

/**@{  AVP device map versions
 * WARNING:  do not change the format or names of these variables.  They are
 * automatically parsed from the build system to generate the SDK package
 * name.
 **/
#define RTE_AVP_RELEASE_VERSION_1 1
#define RTE_AVP_RELEASE_VERSION RTE_AVP_RELEASE_VERSION_1
#define RTE_AVP_MAJOR_VERSION_0 0
#define RTE_AVP_MAJOR_VERSION_1 1
#define RTE_AVP_MAJOR_VERSION_2 2
#define RTE_AVP_MAJOR_VERSION RTE_AVP_MAJOR_VERSION_2
#define RTE_AVP_MINOR_VERSION_0 0
#define RTE_AVP_MINOR_VERSION_1 1
#define RTE_AVP_MINOR_VERSION_13 13
#define RTE_AVP_MINOR_VERSION RTE_AVP_MINOR_VERSION_13
/**@} */


/**
 * Generates a 32-bit version number from the specified version number
 * components
 */
#define RTE_AVP_MAKE_VERSION(_release, _major, _minor) \
((((_release) & 0xffff) << 16) | (((_major) & 0xff) << 8) | ((_minor) & 0xff))


/**
 * Represents the current version of the AVP host driver
 * WARNING:  in the current development branch the host and guest driver
 * version should always be the same.  When patching guest features back to
 * GA releases the host version number should not be updated unless there was
 * an actual change made to the host driver.
 */
#define RTE_AVP_CURRENT_HOST_VERSION \
RTE_AVP_MAKE_VERSION(RTE_AVP_RELEASE_VERSION_1, \
		     RTE_AVP_MAJOR_VERSION_0, \
		     RTE_AVP_MINOR_VERSION_1)


/**
 * Represents the current version of the AVP guest drivers
 */
#define RTE_AVP_CURRENT_GUEST_VERSION \
RTE_AVP_MAKE_VERSION(RTE_AVP_RELEASE_VERSION_1, \
		     RTE_AVP_MAJOR_VERSION_2, \
		     RTE_AVP_MINOR_VERSION_13)

/**
 * Access AVP device version values
 */
#define RTE_AVP_GET_RELEASE_VERSION(_version) (((_version) >> 16) & 0xffff)
#define RTE_AVP_GET_MAJOR_VERSION(_version) (((_version) >> 8) & 0xff)
#define RTE_AVP_GET_MINOR_VERSION(_version) ((_version) & 0xff)
/**@}*/


/**
 * Remove the minor version number so that only the release and major versions
 * are used for comparisons.
 */
#define RTE_AVP_STRIP_MINOR_VERSION(_version) ((_version) >> 8)


/**
 * Defines the number of mbuf pools supported per device (1 per socket)
 */
#define RTE_AVP_MAX_MEMPOOLS 8

/*
 * Defines address translation parameters for each support mbuf pool
 */
struct rte_avp_mempool_info {
	void *addr;
	rte_iova_t phys_addr;
	uint64_t length;
};

/*
 * Struct used to create a AVP device. Passed to the kernel in IOCTL call or
 * via inter-VM shared memory when used in a guest.
 */
struct rte_avp_device_info {
	uint32_t magic;	/**< Memory validation marker */
	uint32_t version; /**< Data format version */

	char ifname[RTE_AVP_NAMESIZE];	/**< Network device name for AVP */

	rte_iova_t tx_phys;
	rte_iova_t rx_phys;
	rte_iova_t alloc_phys;
	rte_iova_t free_phys;

	uint32_t features; /**< Supported feature bitmap */
	uint8_t min_rx_queues; /**< Minimum supported receive/free queues */
	uint8_t num_rx_queues; /**< Recommended number of receive/free queues */
	uint8_t max_rx_queues; /**< Maximum supported receive/free queues */
	uint8_t min_tx_queues; /**< Minimum supported transmit/alloc queues */
	uint8_t num_tx_queues;
	/**< Recommended number of transmit/alloc queues */
	uint8_t max_tx_queues; /**< Maximum supported transmit/alloc queues */

	uint32_t tx_size; /**< Size of each transmit queue */
	uint32_t rx_size; /**< Size of each receive queue */
	uint32_t alloc_size; /**< Size of each alloc queue */
	uint32_t free_size;	/**< Size of each free queue */

	/* Used by Ethtool */
	rte_iova_t req_phys;
	rte_iova_t resp_phys;
	rte_iova_t sync_phys;
	void *sync_va;

	/* mbuf mempool (used when a single memory area is supported) */
	void *mbuf_va;
	rte_iova_t mbuf_phys;

	/* mbuf mempools */
	struct rte_avp_mempool_info pool[RTE_AVP_MAX_MEMPOOLS];

#ifdef __KERNEL__
	/* Ethernet info */
	char ethaddr[ETH_ALEN];
#else
	char ethaddr[RTE_ETHER_ADDR_LEN];
#endif

	uint8_t mode; /**< device mode, i.e guest, host, trace */

	/* mbuf size */
	unsigned int mbuf_size;

	/*
	 * unique id to differentiate between two instantiations of the same
	 * AVP device (i.e., the guest needs to know if the device has been
	 * deleted and recreated).
	 */
	uint64_t device_id;

	uint32_t max_rx_pkt_len; /**< Maximum receive unit size */
};

#define RTE_AVP_MAX_QUEUES 8 /**< Maximum number of queues per device */

/** Maximum number of chained mbufs in a packet */
#define RTE_AVP_MAX_MBUF_SEGMENTS 5

#define RTE_AVP_DEVICE "avp"

#define RTE_AVP_IOCTL_TEST    _IOWR(0, 1, int)
#define RTE_AVP_IOCTL_CREATE  _IOWR(0, 2, struct rte_avp_device_info)
#define RTE_AVP_IOCTL_RELEASE _IOWR(0, 3, struct rte_avp_device_info)
#define RTE_AVP_IOCTL_QUERY   _IOWR(0, 4, struct rte_avp_device_config)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_AVP_COMMON_H_ */
