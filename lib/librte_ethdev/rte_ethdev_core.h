/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_ETHDEV_CORE_H_
#define _RTE_ETHDEV_CORE_H_

#include <pthread.h>
#include <sys/types.h>

/**
 * @file
 *
 * RTE Ethernet Device internal header.
 *
 * This header contains internal data types. But they are still part of the
 * public API because they are used by inline functions in the published API.
 *
 * Applications should not use these directly.
 *
 */

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);

struct rte_eth_dev;

typedef uint16_t (*eth_rx_burst_t)(void *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);
/**< @internal Retrieve input packets from a receive queue of an Ethernet device. */

typedef uint16_t (*eth_tx_burst_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< @internal Send output packets on a transmit queue of an Ethernet device. */

typedef uint16_t (*eth_tx_prep_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< @internal Prepare output packets on a transmit queue of an Ethernet device. */


typedef uint32_t (*eth_rx_queue_count_t)(struct rte_eth_dev *dev,
					 uint16_t rx_queue_id);
/**< @internal Get number of used descriptors on a receive queue. */

typedef int (*eth_rx_descriptor_done_t)(void *rxq, uint16_t offset);
/**< @internal Check DD bit of specific RX descriptor */

typedef int (*eth_rx_descriptor_status_t)(void *rxq, uint16_t offset);
/**< @internal Check the status of a Rx descriptor */

typedef int (*eth_tx_descriptor_status_t)(void *txq, uint16_t offset);
/**< @internal Check the status of a Tx descriptor */


/**
 * @internal
 * Structure used to hold information about the callbacks to be called for a
 * queue on RX and TX.
 */
struct rte_eth_rxtx_callback {
	struct rte_eth_rxtx_callback *next;
	union{
		rte_rx_callback_fn rx;
		rte_tx_callback_fn tx;
	} fn;
	void *param;
};

/**
 * @internal
 * The generic data structure associated with each ethernet device.
 *
 * Pointers to burst-oriented packet receive and transmit functions are
 * located at the beginning of the structure, along with the pointer to
 * where all the data elements for the particular device are stored in shared
 * memory. This split allows the function pointer and driver data to be per-
 * process, while the actual configuration data for the device is shared.
 */
struct rte_eth_dev {
	eth_rx_burst_t rx_pkt_burst; /**< Pointer to PMD receive function. */
	eth_tx_burst_t tx_pkt_burst; /**< Pointer to PMD transmit function. */
	eth_tx_prep_t tx_pkt_prepare; /**< Pointer to PMD transmit prepare function. */

	eth_rx_queue_count_t       rx_queue_count; /**< Get the number of used RX descriptors. */
	eth_rx_descriptor_done_t   rx_descriptor_done;   /**< Check rxd DD bit. */
	eth_rx_descriptor_status_t rx_descriptor_status; /**< Check the status of a Rx descriptor. */
	eth_tx_descriptor_status_t tx_descriptor_status; /**< Check the status of a Tx descriptor. */

	/**
	 * Next two fields are per-device data but *data is shared between
	 * primary and secondary processes and *process_private is per-process
	 * private. The second one is managed by PMDs if necessary.
	 */
	struct rte_eth_dev_data *data;  /**< Pointer to device data. */
	void *process_private; /**< Pointer to per-process device data. */
	const struct eth_dev_ops *dev_ops; /**< Functions exported by PMD */
	struct rte_device *device; /**< Backing device */
	struct rte_intr_handle *intr_handle; /**< Device interrupt handle */
	/** User application callbacks for NIC interrupts */
	struct rte_eth_dev_cb_list link_intr_cbs;
	/**
	 * User-supplied functions called from rx_burst to post-process
	 * received packets before passing them to the user
	 */
	struct rte_eth_rxtx_callback *post_rx_burst_cbs[RTE_MAX_QUEUES_PER_PORT];
	/**
	 * User-supplied functions called from tx_burst to pre-process
	 * received packets before passing them to the driver for transmission.
	 */
	struct rte_eth_rxtx_callback *pre_tx_burst_cbs[RTE_MAX_QUEUES_PER_PORT];
	enum rte_eth_dev_state state; /**< Flag indicating the port state */
	void *security_ctx; /**< Context for security ops */

	uint64_t reserved_64s[4]; /**< Reserved for future fields */
	void *reserved_ptrs[4];   /**< Reserved for future fields */
} __rte_cache_aligned;

struct rte_eth_dev_sriov;
struct rte_eth_dev_owner;

/**
 * @internal
 * The data part, with no function pointers, associated with each ethernet device.
 *
 * This structure is safe to place in shared memory to be common among different
 * processes in a multi-process configuration.
 */
struct rte_eth_dev_data {
	char name[RTE_ETH_NAME_MAX_LEN]; /**< Unique identifier name */

	void **rx_queues; /**< Array of pointers to RX queues. */
	void **tx_queues; /**< Array of pointers to TX queues. */
	uint16_t nb_rx_queues; /**< Number of RX queues. */
	uint16_t nb_tx_queues; /**< Number of TX queues. */

	struct rte_eth_dev_sriov sriov;    /**< SRIOV data */

	void *dev_private;
			/**< PMD-specific private data.
			 *   @see rte_eth_dev_release_port()
			 */

	struct rte_eth_link dev_link;   /**< Link-level information & status. */
	struct rte_eth_conf dev_conf;   /**< Configuration applied to device. */
	uint16_t mtu;                   /**< Maximum Transmission Unit. */
	uint32_t min_rx_buf_size;
			/**< Common RX buffer size handled by all queues. */

	uint64_t rx_mbuf_alloc_failed; /**< RX ring mbuf allocation failures. */
	struct rte_ether_addr *mac_addrs;
			/**< Device Ethernet link address.
			 *   @see rte_eth_dev_release_port()
			 */
	uint64_t mac_pool_sel[ETH_NUM_RECEIVE_MAC_ADDR];
			/**< Bitmap associating MAC addresses to pools. */
	struct rte_ether_addr *hash_mac_addrs;
			/**< Device Ethernet MAC addresses of hash filtering.
			 *   @see rte_eth_dev_release_port()
			 */
	uint16_t port_id;           /**< Device [external] port identifier. */

	__extension__
	uint8_t promiscuous   : 1, /**< RX promiscuous mode ON(1) / OFF(0). */
		scattered_rx : 1,  /**< RX of scattered packets is ON(1) / OFF(0) */
		all_multicast : 1, /**< RX all multicast mode ON(1) / OFF(0). */
		dev_started : 1,   /**< Device state: STARTED(1) / STOPPED(0). */
		lro         : 1;   /**< RX LRO is ON(1) / OFF(0) */
	uint8_t rx_queue_state[RTE_MAX_QUEUES_PER_PORT];
		/**< Queues state: HAIRPIN(2) / STARTED(1) / STOPPED(0). */
	uint8_t tx_queue_state[RTE_MAX_QUEUES_PER_PORT];
		/**< Queues state: HAIRPIN(2) / STARTED(1) / STOPPED(0). */
	uint32_t dev_flags;             /**< Capabilities. */
	int numa_node;                  /**< NUMA node connection. */
	struct rte_vlan_filter_conf vlan_filter_conf;
			/**< VLAN filter configuration. */
	struct rte_eth_dev_owner owner; /**< The port owner. */
	uint16_t representor_id;
			/**< Switch-specific identifier.
			 *   Valid if RTE_ETH_DEV_REPRESENTOR in dev_flags.
			 */

	pthread_mutex_t flow_ops_mutex; /**< rte_flow ops mutex. */
	uint64_t reserved_64s[4]; /**< Reserved for future fields */
	void *reserved_ptrs[4];   /**< Reserved for future fields */
} __rte_cache_aligned;

/**
 * @internal
 * The pool of *rte_eth_dev* structures. The size of the pool
 * is configured at compile-time in the <rte_ethdev.c> file.
 */
extern struct rte_eth_dev rte_eth_devices[];

#endif /* _RTE_ETHDEV_CORE_H_ */
