/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#ifndef _RTE_ETHDEV_H_
#define _RTE_ETHDEV_H_

/**
 * @file
 *
 * RTE Ethernet Device API
 *
 * The Ethernet Device API is composed of two parts:
 *
 * - The application-oriented Ethernet API that includes functions to setup
 *   an Ethernet device (configure it, setup its RX and TX queues and start it),
 *   to get its MAC address, the speed and the status of its physical link,
 *   to receive and to transmit packets, and so on.
 *
 * - The driver-oriented Ethernet API that exports a function allowing
 *   an Ethernet Poll Mode Driver (PMD) to simultaneously register itself as
 *   an Ethernet device driver and as a PCI driver for a set of matching PCI
 *   [Ethernet] devices classes.
 *
 * By default, all the functions of the Ethernet Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on
 * different logical cores to work on the same target object.  For instance,
 * the receive function of a PMD cannot be invoked in parallel on two logical
 * cores to poll the same RX queue [of the same port]. Of course, this function
 * can be invoked in parallel by different logical cores on different RX queues.
 * It is the responsibility of the upper level application to enforce this rule.
 *
 * If needed, parallel accesses by multiple logical cores to shared queues
 * shall be explicitly protected by dedicated inline lock-aware functions
 * built on top of their corresponding lock-free functions of the PMD API.
 *
 * In all functions of the Ethernet API, the Ethernet device is
 * designated by an integer >= 0 named the device port identifier.
 *
 * At the Ethernet driver level, Ethernet devices are represented by a generic
 * data structure of type *rte_eth_dev*.
 *
 * Ethernet devices are dynamically registered during the PCI probing phase
 * performed at EAL initialization time.
 * When an Ethernet device is being probed, an *rte_eth_dev* structure and
 * a new port identifier are allocated for that device. Then, the eth_dev_init()
 * function supplied by the Ethernet driver matching the probed PCI
 * device is invoked to properly initialize the device.
 *
 * The role of the device init function consists of resetting the hardware,
 * checking access to Non-volatile Memory (NVM), reading the MAC address
 * from NVM etc.
 *
 * If the device init operation is successful, the correspondence between
 * the port identifier assigned to the new device and its associated
 * *rte_eth_dev* structure is effectively registered.
 * Otherwise, both the *rte_eth_dev* structure and the port identifier are
 * freed.
 *
 * The functions exported by the application Ethernet API to setup a device
 * designated by its port identifier must be invoked in the following order:
 *     - rte_eth_dev_configure()
 *     - rte_eth_tx_queue_setup()
 *     - rte_eth_rx_queue_setup()
 *     - rte_eth_dev_start()
 *
 * Then, the network application can invoke, in any order, the functions
 * exported by the Ethernet API to get the MAC address of a given device, to
 * get the speed and the status of a device physical link, to receive/transmit
 * [burst of] packets, and so on.
 *
 * If the application wants to change the configuration (i.e. call
 * rte_eth_dev_configure(), rte_eth_tx_queue_setup(), or
 * rte_eth_rx_queue_setup()), it must call rte_eth_dev_stop() first to stop the
 * device and then do the reconfiguration before calling rte_eth_dev_start()
 * again. The tramsit and receive functions should not be invoked when the
 * device is stopped.
 *
 * Please note that some configuration is not stored between calls to
 * rte_eth_dev_stop()/rte_eth_dev_start(). The following configuration will
 * be retained:
 *
 *     - flow control settings
 *     - receive mode configuration (promiscuous mode, hardware checksum mode,
 *       RSS/VMDQ settings etc.)
 *     - VLAN filtering configuration
 *     - MAC addresses supplied to MAC address array
 *     - flow director filtering mode (but not filtering rules)
 *
 * Any other configuration will not be stored and will need to be re-entered
 * after a call to rte_eth_dev_start().
 *
 *
 * Finally, a network application can close an Ethernet device by invoking the
 * rte_eth_dev_close() function.
 *
 * Each function of the application Ethernet API invokes a specific function
 * of the PMD that controls the target device designated by its port
 * identifier.
 * For this purpose, all device-specific functions of an Ethernet driver are
 * supplied through a set of pointers contained in a generic structure of type
 * *eth_dev_ops*.
 * The address of the *eth_dev_ops* structure is stored in the *rte_eth_dev*
 * structure by the device init function of the Ethernet driver, which is
 * invoked during the PCI probing phase, as explained earlier.
 *
 * In other words, each function of the Ethernet API simply retrieves the
 * *rte_eth_dev* structure associated with the device port identifier and
 * performs an indirect invocation of the corresponding driver function
 * supplied in the *eth_dev_ops* structure of the *rte_eth_dev* structure.
 *
 * For performance reasons, the address of the burst-oriented RX and TX
 * functions of the Ethernet driver are not contained in the *eth_dev_ops*
 * structure. Instead, they are directly stored at the beginning of the
 * *rte_eth_dev* structure to avoid an extra indirect memory access during
 * their invocation.
 *
 * RTE ethernet device drivers do not use interrupts for transmitting or
 * receiving. Instead, Ethernet drivers export Poll-Mode receive and transmit
 * functions to applications.
 * Both receive and transmit functions are packet-burst oriented to minimize
 * their cost per packet through the following optimizations:
 *
 * - Sharing among multiple packets the incompressible cost of the
 *   invocation of receive/transmit functions.
 *
 * - Enabling receive/transmit functions to take advantage of burst-oriented
 *   hardware features (L1 cache, prefetch instructions, NIC head/tail
 *   registers) to minimize the number of CPU cycles per packet, for instance,
 *   by avoiding useless read memory accesses to ring descriptors, or by
 *   systematically using arrays of pointers that exactly fit L1 cache line
 *   boundaries and sizes.
 *
 * The burst-oriented receive function does not provide any error notification,
 * to avoid the corresponding overhead. As a hint, the upper-level application
 * might check the status of the device link once being systematically returned
 * a 0 value by the receive function of the driver for a given number of tries.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_log.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_mbuf.h>
#include "rte_ether.h"

/**
 * A structure used to retrieve statistics for an Ethernet port.
 */
struct rte_eth_stats {
	uint64_t ipackets;  /**< Total number of successfully received packets. */
	uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
	uint64_t ibytes;    /**< Total number of successfully received bytes. */
	uint64_t obytes;    /**< Total number of successfully transmitted bytes. */
	uint64_t ierrors;   /**< Total number of erroneous received packets. */
	uint64_t oerrors;   /**< Total number of failed transmitted packets. */
	uint64_t imcasts;   /**< Total number of multicast received packets. */
	uint64_t rx_nombuf; /**< Total number of RX mbuf allocation failures. */
	uint64_t fdirmatch; /**< Total number of RX packets matching a filter. */
	uint64_t fdirmiss;  /**< Total number of RX packets not matching any filter. */
};

/**
 * A structure used to retrieve link-level information of an Ethernet port.
 */
struct rte_eth_link {
	uint16_t link_speed;      /**< ETH_LINK_SPEED_[10, 100, 1000, 10000] */
	uint16_t link_duplex;     /**< ETH_LINK_[HALF_DUPLEX, FULL_DUPLEX] */
	uint8_t  link_status : 1; /**< 1 -> link up, 0 -> link down */
}__attribute__((aligned(8)));     /**< aligned for atomic64 read/write */

#define ETH_LINK_SPEED_AUTONEG  0       /**< Auto-negotiate link speed. */
#define ETH_LINK_SPEED_10       10      /**< 10 megabits/second. */
#define ETH_LINK_SPEED_100      100     /**< 100 megabits/second. */
#define ETH_LINK_SPEED_1000     1000    /**< 1 gigabits/second. */
#define ETH_LINK_SPEED_10000    10000   /**< 10 gigabits/second. */

#define ETH_LINK_AUTONEG_DUPLEX 0       /**< Auto-negotiate duplex. */
#define ETH_LINK_HALF_DUPLEX    1       /**< Half-duplex connection. */
#define ETH_LINK_FULL_DUPLEX    2       /**< Full-duplex connection. */

/**
 * A structure used to configure the ring threshold registers of an RX/TX
 * queue for an Ethernet port.
 */
struct rte_eth_thresh {
	uint8_t pthresh; /**< Ring prefetch threshold. */
	uint8_t hthresh; /**< Ring host threshold. */
	uint8_t wthresh; /**< Ring writeback threshold. */
};

/**
 *  A set of values to identify what method is to be used to route
 *  packets to multiple queues.
 */
enum rte_eth_rx_mq_mode {
	ETH_RSS     = 0,     /**< Default to RSS mode */
	ETH_VMDQ_DCB         /**< Use VMDQ+DCB to route traffic to queues */
};

/**
 * A structure used to configure the RX features of an Ethernet port.
 */
struct rte_eth_rxmode {
	/** The multi-queue packet distribution mode to be used, e.g. RSS. */
	enum rte_eth_rx_mq_mode mq_mode;
	uint32_t max_rx_pkt_len;    /**< Only used if jumbo_frame enabled. */
	uint16_t split_hdr_size;    /**< hdr buf size (header_split enabled).*/
	uint8_t header_split   : 1, /**< Header Split enable. */
		hw_ip_checksum : 1, /**< IP/UDP/TCP checksum offload enable. */
		hw_vlan_filter : 1, /**< VLAN filter enable. */
		jumbo_frame    : 1, /**< Jumbo Frame Receipt enable. */
		hw_strip_crc   : 1; /**< Enable CRC stripping by hardware. */
};

/**
 * A structure used to configure the Receive Side Scaling (RSS) feature
 * of an Ethernet port.
 * If not NULL, the *rss_key* pointer of the *rss_conf* structure points
 * to an array of 40 bytes holding the RSS key to use for hashing specific
 * header fields of received packets.
 * Otherwise, a default random hash key is used by the device driver.
 *
 * The *rss_hf* field of the *rss_conf* structure indicates the different
 * types of IPv4/IPv6 packets to which the RSS hashing must be applied.
 * Supplying an *rss_hf* equal to zero disables the RSS feature.
 */
struct rte_eth_rss_conf {
	uint8_t  *rss_key;   /**< If not NULL, 40-byte hash key. */
	uint16_t rss_hf;     /**< Hash functions to apply - see below. */
};

#define ETH_RSS_IPV4        0x0001 /**< IPv4 packet. */
#define ETH_RSS_IPV4_TCP    0x0002 /**< IPv4/TCP packet. */
#define ETH_RSS_IPV6        0x0004 /**< IPv6 packet. */
#define ETH_RSS_IPV6_EX     0x0008 /**< IPv6 packet with extension headers.*/
#define ETH_RSS_IPV6_TCP    0x0010 /**< IPv6/TCP packet. */
#define ETH_RSS_IPV6_TCP_EX 0x0020 /**< IPv6/TCP with extension headers. */
/* Intel RSS extensions to UDP packets */
#define ETH_RSS_IPV4_UDP    0x0040 /**< IPv4/UDP packet. */
#define ETH_RSS_IPV6_UDP    0x0080 /**< IPv6/UDP packet. */
#define ETH_RSS_IPV6_UDP_EX 0x0100 /**< IPv6/UDP with extension headers. */

/* Definitions used for VMDQ and DCB functionality */
#define ETH_VMDQ_MAX_VLAN_FILTERS   64 /**< Maximum nb. of VMDQ vlan filters. */
#define ETH_DCB_NUM_USER_PRIORITIES 8  /**< Maximum nb. of DCB priorities. */
#define ETH_VMDQ_DCB_NUM_QUEUES     128 /**< Maximum nb. of VMDQ DCB queues. */

/**
 * This enum indicates the possible number of queue pools
 * in VMDQ+DCB configurations.
 */
enum rte_eth_nb_pools {
	ETH_16_POOLS = 16, /**< 16 pools with DCB. */
	ETH_32_POOLS = 32  /**< 32 pools with DCB. */
};

/**
 * A structure used to configure the VMDQ+DCB feature
 * of an Ethernet port.
 *
 * Using this feature, packets are routed to a pool of queues, based
 * on the vlan id in the vlan tag, and then to a specific queue within
 * that pool, using the user priority vlan tag field.
 *
 * A default pool may be used, if desired, to route all traffic which
 * does not match the vlan filter rules.
 */
struct rte_eth_vmdq_dcb_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	struct {
		uint16_t vlan_id; /**< The vlan id of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet rx */
	} pool_map[ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq vlan pool maps. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Selects a queue in a pool */
};

/**
 * A structure used to configure the TX features of an Ethernet port.
 * For future extensions.
 */
struct rte_eth_txmode {
};

/**
 * A structure used to configure an RX ring of an Ethernet port.
 */
struct rte_eth_rxconf {
	struct rte_eth_thresh rx_thresh; /**< RX ring threshold registers. */
	uint16_t rx_free_thresh; /**< Drives the freeing of RX descriptors. */
};

/**
 * A structure used to configure a TX ring of an Ethernet port.
 */
struct rte_eth_txconf {
	struct rte_eth_thresh tx_thresh; /**< TX ring threshold registers. */
	uint16_t tx_rs_thresh; /**< Drives the setting of RS bit on TXDs. */
	uint16_t tx_free_thresh; /**< Drives the freeing of TX buffers. */
};

/**
 * This enum indicates the flow control mode
 */
enum rte_eth_fc_mode {
	RTE_FC_NONE = 0, /**< Disable flow control. */
	RTE_FC_RX_PAUSE, /**< RX pause frame, enable flowctrl on TX side. */
	RTE_FC_TX_PAUSE, /**< TX pause frame, enable flowctrl on RX side. */
	RTE_FC_FULL      /**< Enable flow control on both side. */
};

/**
 * A structure used to configure Ethernet flow control parameter.
 * These parameters will be configured into the register of the NIC.
 * Please refer to the corresponding data sheet for proper value.
 */
struct rte_eth_fc_conf {
	uint32_t high_water;  /**< High threshold value to trigger XOFF */
	uint32_t low_water;   /**< Low threshold value to trigger XON */
	uint16_t pause_time;  /**< Pause quota in the Pause frame */
	uint16_t send_xon;    /**< Is XON frame need be sent */
	enum rte_eth_fc_mode mode;  /**< Link flow control mode */
};

/**
 *  Flow Director setting modes: none (default), signature or perfect.
 */
enum rte_fdir_mode {
	RTE_FDIR_MODE_NONE      = 0, /**< Disable FDIR support. */
	RTE_FDIR_MODE_SIGNATURE,     /**< Enable FDIR signature filter mode. */
	RTE_FDIR_MODE_PERFECT,       /**< Enable FDIR perfect filter mode. */
};

/**
 *  Memory space that can be configured to store Flow Director filters
 *  in the board memory.
 */
enum rte_fdir_pballoc_type {
	RTE_FDIR_PBALLOC_64K = 0,  /**< 64k. */
	RTE_FDIR_PBALLOC_128K,     /**< 128k. */
	RTE_FDIR_PBALLOC_256K,     /**< 256k. */
};

/**
 *  Select report mode of FDIR hash information in RX descriptors.
 */
enum rte_fdir_status_mode {
	RTE_FDIR_NO_REPORT_STATUS = 0, /**< Never report FDIR hash. */
	RTE_FDIR_REPORT_STATUS, /**< Only report FDIR hash for matching pkts. */
	RTE_FDIR_REPORT_STATUS_ALWAYS, /**< Always report FDIR hash. */
};

/**
 * A structure used to configure the Flow Director (FDIR) feature
 * of an Ethernet port.
 *
 * If mode is RTE_FDIR_DISABLE, the pballoc value is ignored.
 */
struct rte_fdir_conf {
	enum rte_fdir_mode mode; /**< Flow Director mode. */
	enum rte_fdir_pballoc_type pballoc; /**< Space for FDIR filters. */
	enum rte_fdir_status_mode status;  /**< How to report FDIR hash. */
	/* Offset of flexbytes field in RX packets (in 16-bit word units). */
	uint8_t flexbytes_offset;
	/* RX queue of packets matching a "drop" filter in perfect mode. */
	uint8_t drop_queue;
};

/**
 *  Possible l4type of FDIR filters.
 */
enum rte_l4type {
	RTE_FDIR_L4TYPE_NONE = 0,       /**< Nnoe. */
	RTE_FDIR_L4TYPE_UDP,            /**< UDP. */
	RTE_FDIR_L4TYPE_TCP,            /**< TCP. */
	RTE_FDIR_L4TYPE_SCTP,     	/**< SCTP. */
};

/**
 *  Select IPv4 or IPv6 FDIR filters.
 */
enum rte_iptype {
	RTE_FDIR_IPTYPE_IPV4 = 0,     /**< IPv4. */
	RTE_FDIR_IPTYPE_IPV6 ,        /**< IPv6. */
};

/**
 *  A structure used to define a FDIR packet filter.
 */
struct rte_fdir_filter {
	uint16_t flex_bytes; /**< Flex bytes value to match. */
	uint16_t vlan_id; /**< VLAN ID value to match, 0 otherwise. */
	uint16_t port_src; /**< Source port to match, 0 otherwise. */
	uint16_t port_dst; /**< Destination port to match, 0 otherwise. */
	union {
		uint32_t ipv4_addr; /**< IPv4 source address to match. */
		uint32_t ipv6_addr[4]; /**< IPv6 source address to match. */
	} ip_src; /**< IPv4/IPv6 source address to match (union of above). */
	union {
		uint32_t ipv4_addr; /**< IPv4 destination address to match. */
		uint32_t ipv6_addr[4]; /**< IPv6 destination address to match */
	} ip_dst; /**< IPv4/IPv6 destination address to match (union of above). */
	enum rte_l4type l4type; /**< l4type to match: NONE/UDP/TCP/SCTP. */
	enum rte_iptype iptype; /**< IP packet type to match: IPv4 or IPv6. */
};

/**
 *  A structure used to configure FDIR masks that are used by the device
 *  to match the various fields of RX packet headers.
 */
struct rte_fdir_masks {
	/** When set to 1, packet l4type is not relevant in filters, and
	   source and destination port masks must be set to zero. */
	uint8_t only_ip_flow;
	uint8_t vlan_id; /**< If set to 1, vlan_id is relevant in filters. */
	uint8_t vlan_prio; /**< If set to 1, vlan_prio is relevant in filters. */
	uint8_t flexbytes; /**< If set to 1, flexbytes is relevant in filters. */
	/** Mask of Destination IPv4 Address. All bits set to 1 define the
	   relevant bits to use in the destination address of an IPv4 packet
	   when matching it against FDIR filters. */
	uint32_t dst_ipv4_mask;
	/** Mask of Source IPv4 Address. All bits set to 1 define
	   the relevant bits to use in the source address of an IPv4 packet
	   when matching it against FDIR filters. */
	uint32_t src_ipv4_mask;
	/** Mask of Source IPv6 Address. All bits set to 1 define the
	   relevant BYTES to use in the source address of an IPv6 packet
	   when matching it against FDIR filters. */
	uint16_t src_ipv6_mask;
	/** Mask of Source Port. All bits set to 1 define the relevant
	   bits to use in the source port of an IP packets when matching it
	   against FDIR filters. */
	uint16_t src_port_mask;
	/** Mask of Destination Port. All bits set to 1 define the relevant
	   bits to use in the destination port of an IP packet when matching it
	   against FDIR filters. */
	uint16_t dst_port_mask;
};

/**
 *  A structure used to report the status of the flow director filters in use.
 */
struct rte_eth_fdir {
	/** Number of filters with collision indication. */
	uint16_t collision;
	/** Number of free (non programmed) filters. */
	uint16_t free;
	/** The Lookup hash value of the added filter that updated the value
	   of the MAXLEN field */
	uint16_t maxhash;
	/** Longest linked list of filters in the table. */
	uint8_t maxlen;
	/** Number of added filters. */
	uint64_t add;
	/** Number of removed filters. */
	uint64_t remove;
	/** Number of failed added filters (no more space in device). */
	uint64_t f_add;
	/** Number of failed removed filters. */
	uint64_t f_remove;
};

/**
 * A structure used to enable/disable specific device interrupts.
 */
struct rte_intr_conf {
	/** enable/disable lsc interrupt. 0 (default) - disable, 1 enable */
	uint16_t lsc;
};

/**
 * A structure used to configure an Ethernet port.
 * Depending upon the RX multi-queue mode, extra advanced
 * configuration settings may be needed.
 */
struct rte_eth_conf {
	uint16_t link_speed;
	/**< ETH_LINK_SPEED_10[0|00|000], or 0 for autonegotation */
	uint16_t link_duplex;
	/**< ETH_LINK_[HALF_DUPLEX|FULL_DUPLEX], or 0 for autonegotation */
	struct rte_eth_rxmode rxmode; /**< Port RX configuration. */
	struct rte_eth_txmode txmode; /**< Port TX configuration. */
	union {
		struct rte_eth_rss_conf rss_conf; /**< Port RSS configuration */
		struct rte_eth_vmdq_dcb_conf vmdq_dcb_conf;
		/**< Port vmdq+dcb configuration. */
	} rx_adv_conf; /**< Port RX filtering configuration (union). */
	struct rte_fdir_conf fdir_conf; /**< FDIR configuration. */
	struct rte_intr_conf intr_conf; /**< Interrupt mode configuration. */
};

/**
 * A structure used to retrieve the contextual information of
 * an Ethernet device, such as the controlling driver of the device,
 * its PCI context, etc...
 */
struct rte_eth_dev_info {
	struct rte_pci_device *pci_dev; /**< Device PCI information. */
	const char *driver_name; /**< Device Driver name. */
	uint32_t min_rx_bufsize; /**< Minimum size of RX buffer. */
	uint32_t max_rx_pktlen; /**< Maximum configurable length of RX pkt. */
	uint16_t max_rx_queues; /**< Maximum number of RX queues. */
	uint16_t max_tx_queues; /**< Maximum number of TX queues. */
	uint32_t max_mac_addrs; /**< Maximum number of MAC addresses. */
};

struct rte_eth_dev;
struct igb_rx_queue;
struct igb_tx_queue;

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);

/*
 * Definitions of all functions exported by an Ethernet driver through the
 * the generic structure of type *eth_dev_ops* supplied in the *rte_eth_dev*
 * structure associated with an Ethernet device.
 */

typedef int  (*eth_dev_configure_t)(struct rte_eth_dev *dev, uint16_t nb_rx_q,
				    uint16_t nb_tx_q);
/**< Ethernet device configuration. */

typedef int  (*eth_dev_start_t)(struct rte_eth_dev *dev);
/**< Function used to start a configured Ethernet device. */

typedef void (*eth_dev_stop_t)(struct rte_eth_dev *dev);
/**< Function used to stop a configured Ethernet device. */

typedef void (*eth_dev_close_t)(struct rte_eth_dev *dev);
/**< @internal Function used to close a configured Ethernet device. */

typedef void (*eth_promiscuous_enable_t)(struct rte_eth_dev *dev);
/**< Function used to enable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_promiscuous_disable_t)(struct rte_eth_dev *dev);
/**< Function used to disable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_allmulticast_enable_t)(struct rte_eth_dev *dev);
/**< Enable the receipt of all multicast packets by an Ethernet device. */

typedef void (*eth_allmulticast_disable_t)(struct rte_eth_dev *dev);
/**< Disable the receipt of all multicast packets by an Ethernet device. */

typedef int (*eth_link_update_t)(struct rte_eth_dev *dev,
				int wait_to_complete);
/**< Get link speed, duplex mode and state (up/down) of an Ethernet device. */

typedef void (*eth_stats_get_t)(struct rte_eth_dev *dev,
				struct rte_eth_stats *igb_stats);
/**< Get global I/O statistics of an Ethernet device. */

typedef void (*eth_stats_reset_t)(struct rte_eth_dev *dev);
/**< Reset global I/O statistics of an Ethernet device to 0. */

typedef void (*eth_dev_infos_get_t)(struct rte_eth_dev *dev,
				    struct rte_eth_dev_info *dev_info);
/**< Get specific informations of an Ethernet device. */

typedef int (*eth_rx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id,
				    uint16_t nb_rx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_rxconf *rx_conf,
				    struct rte_mempool *mb_pool);
/**< Set up a receive queue of an Ethernet device. */

typedef int (*eth_tx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t tx_queue_id,
				    uint16_t nb_tx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_txconf *tx_conf);
/**< Setup a transmit queue of an Ethernet device. */

typedef void (*vlan_filter_set_t)(struct rte_eth_dev *dev,
				  uint16_t vlan_id,
				  int on);
/**< Enable/Disable filtering of a VLAN Tag Identifier by an Ethernet device. */

typedef uint16_t (*eth_rx_burst_t)(struct igb_rx_queue *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);
/**< Retrieve input packets from a receive queue of an Ethernet device. */

typedef uint16_t (*eth_tx_burst_t)(struct igb_tx_queue *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< Send output packets on a transmit queue of an Ethernet device. */

typedef int (*fdir_add_signature_filter_t)(struct rte_eth_dev *dev,
					   struct rte_fdir_filter *fdir_ftr,
					   uint8_t rx_queue);
/**< Setup a new signature filter rule on an Ethernet device */

typedef int (*fdir_update_signature_filter_t)(struct rte_eth_dev *dev,
					      struct rte_fdir_filter *fdir_ftr,
					      uint8_t rx_queue);
/**< Update a signature filter rule on an Ethernet device */

typedef int (*fdir_remove_signature_filter_t)(struct rte_eth_dev *dev,
					      struct rte_fdir_filter *fdir_ftr);
/**< Remove a  signature filter rule on an Ethernet device */

typedef void (*fdir_infos_get_t)(struct rte_eth_dev *dev,
				 struct rte_eth_fdir *fdir);
/**< Get information about fdir status */

typedef int (*fdir_add_perfect_filter_t)(struct rte_eth_dev *dev,
					 struct rte_fdir_filter *fdir_ftr,
					 uint16_t soft_id, uint8_t rx_queue,
					 uint8_t drop);
/**< Setup a new perfect filter rule on an Ethernet device */

typedef int (*fdir_update_perfect_filter_t)(struct rte_eth_dev *dev,
					    struct rte_fdir_filter *fdir_ftr,
					    uint16_t soft_id, uint8_t rx_queue,
					    uint8_t drop);
/**< Update a perfect filter rule on an Ethernet device */

typedef int (*fdir_remove_perfect_filter_t)(struct rte_eth_dev *dev,
					    struct rte_fdir_filter *fdir_ftr,
					    uint16_t soft_id);
/**< Remove a perfect filter rule on an Ethernet device */

typedef int (*fdir_set_masks_t)(struct rte_eth_dev *dev,
				struct rte_fdir_masks *fdir_masks);
/**< Setup flow director masks on an Ethernet device */

typedef int (*flow_ctrl_set_t)(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
/**< Setup flow control parameter on an Ethernet device */

typedef int (*eth_dev_led_on_t)(struct rte_eth_dev *dev);
/**<  Turn on SW controllable LED on an Ethernet device */

typedef int (*eth_dev_led_off_t)(struct rte_eth_dev *dev);
/**<  Turn off SW controllable LED on an Ethernet device */

typedef void (*eth_mac_addr_remove_t)(struct rte_eth_dev *dev, uint32_t index);
/**< Remove MAC address from receive address register */

typedef void (*eth_mac_addr_add_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr,
				  uint32_t index,
				  uint32_t vmdq);
/**< Set a MAC address into Receive Address Address Register */

/**
 * A structure containing the functions exported by an Ethernet driver.
 */
struct eth_dev_ops {
	eth_dev_configure_t        dev_configure; /**< Configure device. */
	eth_dev_start_t            dev_start;     /**< Start device. */
	eth_dev_stop_t             dev_stop;      /**< Stop device. */
	eth_dev_close_t            dev_close;     /**< Close device. */
	eth_promiscuous_enable_t   promiscuous_enable; /**< Promiscuous ON. */
	eth_promiscuous_disable_t  promiscuous_disable;/**< Promiscuous OFF. */
	eth_allmulticast_enable_t  allmulticast_enable;/**< RX multicast ON. */
	eth_allmulticast_disable_t allmulticast_disable;/**< RX multicast OF. */
	eth_link_update_t          link_update;   /**< Get device link state. */
	eth_stats_get_t            stats_get;     /**< Get device statistics. */
	eth_stats_reset_t          stats_reset;   /**< Reset device statistics. */
	eth_dev_infos_get_t        dev_infos_get; /**< Get device info. */
	vlan_filter_set_t          vlan_filter_set; /**< Filter VLAN on/off. */
	eth_rx_queue_setup_t       rx_queue_setup;/**< Set up device RX queue.*/
	eth_tx_queue_setup_t       tx_queue_setup;/**< Set up device TX queue.*/
	eth_dev_led_on_t           dev_led_on;    /**< Turn on LED. */
	eth_dev_led_off_t          dev_led_off;   /**< Turn off LED. */
	flow_ctrl_set_t            flow_ctrl_set; /**< Setup flow control. */
	eth_mac_addr_remove_t      mac_addr_remove; /**< Remove MAC address */
	eth_mac_addr_add_t         mac_addr_add;  /**< Add a MAC address */

	/** Add a signature filter. */
	fdir_add_signature_filter_t fdir_add_signature_filter;
	/** Update a signature filter. */
	fdir_update_signature_filter_t fdir_update_signature_filter;
	/** Remove a signature filter. */
	fdir_remove_signature_filter_t fdir_remove_signature_filter;
	/** Get information about FDIR status. */
	fdir_infos_get_t fdir_infos_get;
	/** Add a perfect filter. */
	fdir_add_perfect_filter_t fdir_add_perfect_filter;
	/** Update a perfect filter. */
	fdir_update_perfect_filter_t fdir_update_perfect_filter;
	/** Remove a perfect filter. */
	fdir_remove_perfect_filter_t fdir_remove_perfect_filter;
	/** Setup masks for FDIR filtering. */
	fdir_set_masks_t fdir_set_masks;
};

/**
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
	struct rte_eth_dev_data *data;  /**< Pointer to device data */
	const struct eth_driver *driver;/**< Driver for this device */
	struct eth_dev_ops *dev_ops;    /**< Functions exported by PMD */
	struct rte_pci_device *pci_dev; /**< PCI info. supplied by probing */
	struct rte_eth_dev_cb_list callbacks; /**< User application callbacks */
};

/**
 * The data part, with no function pointers, associated with each ethernet device.
 *
 * This structure is safe to place in shared memory to be common among different
 * processes in a multi-process configuration.
 */
struct rte_eth_dev_data {
	struct igb_rx_queue **rx_queues; /**< Array of pointers to RX queues. */
	struct igb_tx_queue **tx_queues; /**< Array of pointers to TX queues. */
	uint16_t nb_rx_queues; /**< Number of RX queues. */
	uint16_t nb_tx_queues; /**< Number of TX queues. */

	void *dev_private;              /**< PMD-specific private data */

	struct rte_eth_link dev_link;
	/**< Link-level information & status */

	struct rte_eth_conf dev_conf;   /**< Configuration applied to device. */
	uint16_t max_frame_size;        /**< Default is ETHER_MAX_LEN (1518). */

	uint64_t rx_mbuf_alloc_failed; /**< RX ring mbuf allocation failures. */
	struct ether_addr* mac_addrs;/**< Device Ethernet Link address. */
	uint8_t port_id;           /**< Device [external] port identifier. */
	uint8_t promiscuous   : 1, /**< RX promiscuous mode ON(1) / OFF(0). */
		scattered_rx : 1,  /**< RX of scattered packets is ON(1) / OFF(0) */
		all_multicast : 1, /**< RX all multicast mode ON(1) / OFF(0). */
		dev_started : 1;   /**< Device state: STARTED(1) / STOPPED(0). */
};

/**
 * The pool of *rte_eth_dev* structures. The size of the pool
 * is configured at compile-time in the <rte_ethdev.c> file.
 */
extern struct rte_eth_dev rte_eth_devices[];

/**
 * Get the total number of Ethernet devices that have been successfully
 * initialized by the [matching] Ethernet driver during the PCI probing phase.
 * All devices whose port identifier is in the range
 * [0,  rte_eth_dev_count() - 1] can be operated on by network applications.
 *
 * @return
 *   - The total number of usable Ethernet devices.
 */
extern uint8_t rte_eth_dev_count(void);

struct eth_driver;
/**
 * Initialization function of an Ethernet driver invoked for each matching
 * Ethernet PCI device detected during the PCI probing phase.
 *
 * @param eth_drv
 *   The pointer to the [matching] Ethernet driver structure supplied by
 *   the PMD when it registered itself.
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 *   associated with the matching device and which have been [automatically]
 *   allocated in the *rte_eth_devices* array.
 *   The *eth_dev* structure is supplied to the driver initialization function
 *   with the following fields already initialized:
 *
 *   - *pci_dev*: Holds the pointers to the *rte_pci_device* structure which
 *     contains the generic PCI information of the matching device.
 *
 *   - *dev_private*: Holds a pointer to the device private data structure.
 *
 *   - *max_frame_size*: Contains the default Ethernet maximum frame length
 *     (1518).
 *
 *   - *port_id*: Contains the port index of the device (actually the index
 *     of the *eth_dev* structure in the *rte_eth_devices* array).
 *
 * @return
 *   - 0: Success, the device is properly initialized by the driver.
 *        In particular, the driver MUST have set up the *dev_ops* pointer
 *        of the *eth_dev* structure.
 *   - <0: Error code of the device initialization failure.
 */
typedef int (*eth_dev_init_t)(struct eth_driver  *eth_drv,
			      struct rte_eth_dev *eth_dev);

/**
 * The structure associated with a PMD Ethernet driver.
 *
 * Each Ethernet driver acts as a PCI driver and is represented by a generic
 * *eth_driver* structure that holds:
 *
 * - An *rte_pci_driver* structure (which must be the first field).
 *
 * - The *eth_dev_init* function invoked for each matching PCI device.
 *
 * - The size of the private data to allocate for each matching device.
 */
struct eth_driver {
	struct rte_pci_driver pci_drv;    /**< The PMD is also a PCI driver. */
	eth_dev_init_t eth_dev_init;      /**< Device init function. */
	unsigned int dev_private_size;    /**< Size of device private data. */
};

/**
 * A function invoked by the initialization function of an Ethernet driver
 * to simultaneously register itself as a PCI driver and as an Ethernet
 * Poll Mode Driver (PMD).
 *
 * @param eth_drv
 *   The pointer to the *eth_driver* structure associated with
 *   the Ethernet driver.
 */
extern void rte_eth_driver_register(struct eth_driver *eth_drv);

/**
 * The initialization function of the driver for
 * Intel(r) IGB Gigabit Ethernet Controller devices.
 * This function is invoked once at EAL start time.
 * @return
 *   0 on success
 */
extern int rte_igb_pmd_init(void);

/**
 * The initialization function of the driver for
 * Intel(r) EM Gigabit Ethernet Controller devices.
 * This function is invoked once at EAL start time.
 * @return
 *   0 on success
 */
extern int rte_em_pmd_init(void);

/**
 * The initialization function of the driver for 1Gbps Intel IGB_VF
 * Ethernet devices.
 * Invoked once at EAL start time.
 * @return
 *   0 on success
 */
extern int rte_igbvf_pmd_init(void);

/**
 * The initialization function of the driver for 10Gbps Intel IXGBE
 * Ethernet devices.
 * Invoked once at EAL start time.
 * @return
 *   0 on success
 */
extern int rte_ixgbe_pmd_init(void);

/**
 * The initialization function of the driver for 10Gbps Intel IXGBE_VF
 * Ethernet devices.
 * Invoked once at EAL start time.
 * @return
 *   0 on success
 */
extern int rte_ixgbevf_pmd_init(void);

/**
 * The initialization function of *all* supported and enabled drivers.
 * Right now, the following PMDs are supported:
 *  - igb
 *  - igbvf
 *  - em
 *  - ixgbe
 *  - ixgbevf
 * This function is invoked once at EAL start time.
 * @return
 *   0 on success.
 *   Error code of the device initialization failure,
 *   -ENODEV if there are no drivers available
 *   (e.g. if all driver config options are = n).
 */
static inline
int rte_pmd_init_all(void)
{
	int ret = -ENODEV;

#ifdef RTE_LIBRTE_IGB_PMD
	if ((ret = rte_igb_pmd_init()) != 0) {
		RTE_LOG(ERR, PMD, "Cannot init igb PMD\n");
		return (ret);
	}
	if ((ret = rte_igbvf_pmd_init()) != 0) {
		RTE_LOG(ERR, PMD, "Cannot init igbvf PMD\n");
		return (ret);
	}
#endif /* RTE_LIBRTE_IGB_PMD */

#ifdef RTE_LIBRTE_EM_PMD
	if ((ret = rte_em_pmd_init()) != 0) {
		RTE_LOG(ERR, PMD, "Cannot init em PMD\n");
		return (ret);
	}
#endif /* RTE_LIBRTE_EM_PMD */

#ifdef RTE_LIBRTE_IXGBE_PMD
	if ((ret = rte_ixgbe_pmd_init()) != 0) {
		RTE_LOG(ERR, PMD, "Cannot init ixgbe PMD\n");
		return (ret);
	}
	if ((ret = rte_ixgbevf_pmd_init()) != 0) {
		RTE_LOG(ERR, PMD, "Cannot init ixgbevf PMD\n");
		return (ret);
	}
#endif /* RTE_LIBRTE_IXGBE_PMD */

	if (ret == -ENODEV)
		RTE_LOG(ERR, PMD, "No PMD(s) are configured\n");
	return (ret);
}

/**
 * Configure an Ethernet device.
 * This function must be invoked first before any other function in the
 * Ethernet API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param port_id
 *   The port identifier of the Ethernet device to configure.
 * @param nb_rx_queue
 *   The number of receive queues to set up for the Ethernet device.
 * @param nb_tx_queue
 *   The number of transmit queues to set up for the Ethernet device.
 * @param eth_conf
 *   The pointer to the configuration data to be used for the Ethernet device.
 *   The *rte_eth_conf* structure includes:
 *     -  the hardware offload features to activate, with dedicated fields for
 *        each statically configurable offload hardware feature provided by
 *        Ethernet devices, such as IP checksum or VLAN tag stripping for
 *        example.
 *     - the Receive Side Scaling (RSS) configuration when using multiple RX
 *         queues per port.
 *
 *   Embedding all configuration information in a single data structure
 *   is the more flexible method that allows the addition of new features
 *   without changing the syntax of the API.
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
extern int rte_eth_dev_configure(uint8_t port_id,
				 uint16_t nb_rx_queue,
				 uint16_t nb_tx_queue,
				 const struct rte_eth_conf *eth_conf);

/**
 * Allocate and set up a receive queue for an Ethernet device.
 *
 * The function allocates a contiguous block of memory for *nb_rx_desc*
 * receive descriptors from a memory zone associated with *socket_id*
 * and initializes each receive descriptor with a network buffer allocated
 * from the memory pool *mb_pool*.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rx_queue_id
 *   The index of the receive queue to set up.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param nb_rx_desc
 *   The number of receive descriptors to allocate for the receive ring.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 *   The value can be *SOCKET_ID_ANY* if there is no NUMA constraint for
 *   the DMA memory allocated for the receive descriptors of the ring.
 * @param rx_conf
 *   The pointer to the configuration data to be used for the receive queue.
 *   The *rx_conf* structure contains an *rx_thresh* structure with the values
 *   of the Prefetch, Host, and Write-Back threshold registers of the receive
 *   ring.
 * @param mb_pool
 *   The pointer to the memory pool from which to allocate *rte_mbuf* network
 *   memory buffers to populate each descriptor of the receive ring.
 * @return
 *   - 0: Success, receive queue correctly set up.
 *   - -EINVAL: The size of network buffers which can be allocated from the
 *      memory pool does not fit the various buffer sizes allowed by the
 *      device controller.
 *   - -ENOMEM: Unable to allocate the receive ring descriptors or to
 *      allocate network memory buffers from the memory pool when
 *      initializing receive descriptors.
 */
extern int rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
				  uint16_t nb_rx_desc, unsigned int socket_id,
				  const struct rte_eth_rxconf *rx_conf,
				  struct rte_mempool *mb_pool);

/**
 * Allocate and set up a transmit queue for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tx_queue_id
 *   The index of the transmit queue to set up.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param nb_tx_desc
 *   The number of transmit descriptors to allocate for the transmit ring.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 *   Its value can be *SOCKET_ID_ANY* if there is no NUMA constraint for
 *   the DMA memory allocated for the transmit descriptors of the ring.
 * @param tx_conf
 *   The pointer to the configuration data to be used for the transmit queue.
 *   The *tx_conf* structure contains the following data:
 *   - The *tx_thresh* structure with the values of the Prefetch, Host, and
 *     Write-Back threshold registers of the transmit ring.
 *     When setting Write-Back threshold to the value greater then zero,
 *     *tx_rs_thresh* value should be explicitly set to one.
 *   - The *tx_free_thresh* value indicates the [minimum] number of network
 *     buffers that must be pending in the transmit ring to trigger their
 *     [implicit] freeing by the driver transmit function.
 *   - The *tx_rs_thresh* value indicates the [minimum] number of transmit
 *     descriptors that must be pending in the transmit ring before setting the
 *     RS bit on a descriptor by the driver transmit function.
 *     The *tx_rs_thresh* value should be less or equal then
 *     *tx_free_thresh* value, and both of them should be less then
 *     *nb_tx_desc* - 3.
 *
 *     Note that setting *tx_free_thresh* or *tx_rs_thresh* value to 0 forces
 *     the transmit function to use default values.
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -ENOMEM: Unable to allocate the transmit ring descriptors.
 */
extern int rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
				  uint16_t nb_tx_desc, unsigned int socket_id,
				  const struct rte_eth_txconf *tx_conf);

/**
 * Start an Ethernet device.
 *
 * The device start step is the last one and consists of setting the configured
 * offload features and in starting the transmit and the receive units of the
 * device.
 * On success, all basic functions exported by the Ethernet API (link status,
 * receive/transmit, and so on) can be invoked.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - 0: Success, Ethernet device started.
 *   - <0: Error code of the driver device start function.
 */
extern int rte_eth_dev_start(uint8_t port_id);

/**
 * Stop an Ethernet device. The device can be restarted with a call to
 * rte_eth_dev_start()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_dev_stop(uint8_t port_id);

/**
 * Close an Ethernet device. The device cannot be restarted!
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_dev_close(uint8_t port_id);

/**
 * Enable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_promiscuous_enable(uint8_t port_id);

/**
 * Disable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_promiscuous_disable(uint8_t port_id);

/**
 * Return the value of promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (1) if promiscuous is enabled
 *   - (0) if promiscuous is disabled.
 *   - (-1) on error
 */
extern int rte_eth_promiscuous_get(uint8_t port_id);

/**
 * Enable the receipt of any multicast frame by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_allmulticast_enable(uint8_t port_id);

/**
 * Disable the receipt of all multicast frames by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_allmulticast_disable(uint8_t port_id);

/**
 * Return the value of allmulticast mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (1) if allmulticast is enabled
 *   - (0) if allmulticast is disabled.
 *   - (-1) on error
 */
extern int rte_eth_allmulticast_get(uint8_t port_id);

/**
 * Retrieve the status (ON/OFF), the speed (in Mbps) and the mode (HALF-DUPLEX
 * or FULL-DUPLEX) of the physical link of an Ethernet device. It might need
 * to wait up to 9 seconds in it.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param link
 *   A pointer to an *rte_eth_link* structure to be filled with
 *   the status, the speed and the mode of the Ethernet device link.
 */
extern void rte_eth_link_get(uint8_t port_id, struct rte_eth_link *link);

/**
 * Retrieve the status (ON/OFF), the speed (in Mbps) and the mode (HALF-DUPLEX
 * or FULL-DUPLEX) of the physical link of an Ethernet device. It is a no-wait
 * version of rte_eth_link_get().
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param link
 *   A pointer to an *rte_eth_link* structure to be filled with
 *   the status, the speed and the mode of the Ethernet device link.
 */
extern void rte_eth_link_get_nowait(uint8_t port_id,
				struct rte_eth_link *link);

/**
 * Retrieve the general I/O statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param stats
 *   A pointer to a structure of type *rte_eth_stats* to be filled with
 *   the values of device counters for the following set of statistics:
 *   - *ipackets* with the total of successfully received packets.
 *   - *opackets* with the total of successfully transmitted packets.
 *   - *ibytes*   with the total of successfully received bytes.
 *   - *obytes*   with the total of successfully transmitted bytes.
 *   - *ierrors*  with the total of erroneous received packets.
 *   - *oerrors*  with the total of failed transmitted packets.
 */
extern void rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats);

/**
 * Reset the general I/O statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern void rte_eth_stats_reset(uint8_t port_id);

/**
 * Retrieve the Ethernet address of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   A pointer to a structure of type *ether_addr* to be filled with
 *   the Ethernet address of the Ethernet device.
 */
extern void rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr);

/**
 * Retrieve the contextual information of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dev_info
 *   A pointer to a structure of type *rte_eth_dev_info* to be filled with
 *   the contextual information of the Ethernet device.
 */
extern void rte_eth_dev_info_get(uint8_t port_id,
				 struct rte_eth_dev_info *dev_info);

/**
 * Enable/Disable hardware filtering by an Ethernet device of received
 * VLAN packets tagged with a given VLAN Tag Identifier.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vlan_id
 *   The VLAN Tag Identifier whose filtering must be enabled or disabled.
 * @param on
 *   If > 0, enable VLAN filtering of VLAN packets tagged with *vlan_id*.
 *   Otherwise, disable VLAN filtering of VLAN packets tagged with *vlan_id*.
 * @return
 *   - (0) if successful.
 *   - (-ENOSUP) if hardware-assisted VLAN filtering not configured.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if VLAN filtering on *port_id* disabled.
 *   - (-EINVAL) if *vlan_id* > 4095.
 */
extern int rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id, int on);

/**
 *
 * Retrieve a burst of input packets from a receive queue of an Ethernet
 * device. The retrieved packets are stored in *rte_mbuf* structures whose
 * pointers are supplied in the *rx_pkts* array.
 *
 * The rte_eth_rx_burst() function loops, parsing the RX ring of the
 * receive queue, up to *nb_pkts* packets, and for each completed RX
 * descriptor in the ring, it performs the following operations:
 *
 * - Initialize the *rte_mbuf* data structure associated with the
 *   RX descriptor according to the information provided by the NIC into
 *   that RX descriptor.
 *
 * - Store the *rte_mbuf* data structure into the next entry of the
 *   *rx_pkts* array.
 *
 * - Replenish the RX descriptor with a new *rte_mbuf* buffer
 *   allocated from the memory pool associated with the receive queue at
 *   initialization time.
 *
 * When retrieving an input packet that was scattered by the controller
 * into multiple receive descriptors, the rte_eth_rx_burst() function
 * appends the associated *rte_mbuf* buffers to the first buffer of the
 * packet.
 *
 * The rte_eth_rx_burst() function returns the number of packets
 * actually retrieved, which is the number of *rte_mbuf* data structures
 * effectively supplied into the *rx_pkts* array.
 * A return value equal to *nb_pkts* indicates that the RX queue contained
 * at least *rx_pkts* packets, and this is likely to signify that other
 * received packets remain in the input queue. Applications implementing
 * a "retrieve as much received packets as possible" policy can check this
 * specific case and keep invoking the rte_eth_rx_burst() function until
 * a value less than *nb_pkts* is returned.
 *
 * This receive method has the following advantages:
 *
 * - It allows a run-to-completion network stack engine to retrieve and
 *   to immediately process received packets in a fast burst-oriented
 *   approach, avoiding the overhead of unnecessary intermediate packet
 *   queue/dequeue operations.
 *
 * - Conversely, it also allows an asynchronous-oriented processing
 *   method to retrieve bursts of received packets and to immediately
 *   queue them for further parallel processing by another logical core,
 *   for instance. However, instead of having received packets being
 *   individually queued by the driver, this approach allows the invoker
 *   of the rte_eth_rx_burst() function to queue a burst of retrieved
 *   packets at a time and therefore dramatically reduce the cost of
 *   enqueue/dequeue operations per packet.
 *
 * - It allows the rte_eth_rx_burst() function of the driver to take
 *   advantage of burst-oriented hardware features (CPU cache,
 *   prefetch instructions, and so on) to minimize the number of CPU
 *   cycles per packet.
 *
 * To summarize, the proposed receive API enables many
 * burst-oriented optimizations in both synchronous and asynchronous
 * packet processing environments with no overhead in both cases.
 *
 * The rte_eth_rx_burst() function does not provide any error
 * notification to avoid the corresponding overhead. As a hint, the
 * upper-level application might check the status of the device link once
 * being systematically returned a 0 value for a given number of tries.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue from which to retrieve input packets.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param rx_pkts
 *   The address of an array of pointers to *rte_mbuf* structures that
 *   must be large enough to store *nb_pkts* pointers in it.
 * @param nb_pkts
 *   The maximum number of packets to retrieve.
 * @return
 *   The number of packets actually retrieved, which is the number
 *   of pointers to *rte_mbuf* structures effectively supplied to the
 *   *rx_pkts* array.
 */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
extern uint16_t rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
				 struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
#else
static inline uint16_t
rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev;

	dev = &rte_eth_devices[port_id];
	return (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id], rx_pkts, nb_pkts);
}
#endif

/**
 * Send a burst of output packets on a transmit queue of an Ethernet device.
 *
 * The rte_eth_tx_burst() function is invoked to transmit output packets
 * on the output queue *queue_id* of the Ethernet device designated by its
 * *port_id*.
 * The *nb_pkts* parameter is the number of packets to send which are
 * supplied in the *tx_pkts* array of *rte_mbuf* structures.
 * The rte_eth_tx_burst() function loops, sending *nb_pkts* packets,
 * up to the number of transmit descriptors available in the TX ring of the
 * transmit queue.
 * For each packet to send, the rte_eth_tx_burst() function performs
 * the following operations:
 *
 * - Pick up the next available descriptor in the transmit ring.
 *
 * - Free the network buffer previously sent with that descriptor, if any.
 *
 * - Initialize the transmit descriptor with the information provided
 *   in the *rte_mbuf data structure.
 *
 * In the case of a segmented packet composed of a list of *rte_mbuf* buffers,
 * the rte_eth_tx_burst() function uses several transmit descriptors
 * of the ring.
 *
 * The rte_eth_tx_burst() function returns the number of packets it
 * actually sent. A return value equal to *nb_pkts* means that all packets
 * have been sent, and this is likely to signify that other output packets
 * could be immediately transmitted again. Applications that implement a
 * "send as many packets to transmit as possible" policy can check this
 * specific case and keep invoking the rte_eth_tx_burst() function until
 * a value less than *nb_pkts* is returned.
 *
 * It is the responsibility of the rte_eth_tx_burst() function to
 * transparently free the memory buffers of packets previously sent.
 * This feature is driven by the *tx_free_thresh* value supplied to the
 * rte_eth_dev_configure() function at device configuration time.
 * When the number of previously sent packets reached the "minimum transmit
 * packets to free" threshold, the rte_eth_tx_burst() function must
 * [attempt to] free the *rte_mbuf*  buffers of those packets whose
 * transmission was effectively completed.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param tx_pkts
 *   The address of an array of *nb_pkts* pointers to *rte_mbuf* structures
 *   which contain the output packets.
 * @param nb_pkts
 *   The maximum number of packets to transmit.
 * @return
 *   The number of output packets actually stored in transmit descriptors of
 *   the transmit ring. The return value can be less than the value of the
 *   *tx_pkts* parameter when the transmit ring is full or has been filled up.
 */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
extern uint16_t rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
				 struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
#else
static inline uint16_t
rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev;

	dev = &rte_eth_devices[port_id];
	return (*dev->tx_pkt_burst)(dev->data->tx_queues[queue_id], tx_pkts, nb_pkts);
}
#endif

/**
 * Setup a new signature filter rule on an Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_filter
 *   The pointer to the fdir filter structure describing the signature filter
 *   rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   signature filter defined in fdir_filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the FDIR mode is not configured in signature mode
 *               on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_add_signature_filter(uint8_t port_id,
					  struct rte_fdir_filter *fdir_filter,
					  uint8_t rx_queue);

/**
 * Update a signature filter rule on an Ethernet device.
 * If the rule doesn't exits, it is created.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_ftr
 *   The pointer to the structure describing the signature filter rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   signature filter defined in fdir_ftr.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in signature mode
 *     on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_update_signature_filter(uint8_t port_id,
					     struct rte_fdir_filter *fdir_ftr,
					     uint8_t rx_queue);

/**
 * Remove a signature filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_ftr
 *   The pointer to the structure describing the signature filter rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in signature mode
 *     on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_remove_signature_filter(uint8_t port_id,
					     struct rte_fdir_filter *fdir_ftr);

/**
 * Retrieve the flow director information of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir
 *   A pointer to a structure of type *rte_eth_dev_fdir* to be filled with
 *   the flow director information of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured on *port_id*.
 */
int rte_eth_dev_fdir_get_infos(uint8_t port_id, struct rte_eth_fdir *fdir);

/**
 * Add a new perfect filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_filter
 *   The pointer to the structure describing the perfect filter rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 *   IPv6 are not supported.
 * @param soft_id
 *    The 16-bit value supplied in the field hash.fdir.id of mbuf for RX
 *    packets matching the perfect filter.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   perfect filter defined in fdir_filter.
 * @param drop
 *    If drop is set to 1, matching RX packets are stored into the RX drop
 *    queue defined in the rte_fdir_conf.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in perfect mode
 *               on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_add_perfect_filter(uint8_t port_id,
					struct rte_fdir_filter *fdir_filter,
					uint16_t soft_id, uint8_t rx_queue,
					uint8_t drop);

/**
 * Update a perfect filter rule on an Ethernet device.
 * If the rule doesn't exits, it is created.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_filter
 *   The pointer to the structure describing the perfect filter rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 *   IPv6 are not supported.
 * @param soft_id
 *    The 16-bit value supplied in the field hash.fdir.id of mbuf for RX
 *    packets matching the perfect filter.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   perfect filter defined in fdir_filter.
 * @param drop
 *    If drop is set to 1, matching RX packets are stored into the RX drop
 *    queue defined in the rte_fdir_conf.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in perfect mode
 *      on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_update_perfect_filter(uint8_t port_id,
					   struct rte_fdir_filter *fdir_filter,
					   uint16_t soft_id, uint8_t rx_queue,
					   uint8_t drop);

/**
 * Remove a perfect filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_filter
 *   The pointer to the structure describing the perfect filter rule.
 *   The *rte_fdir_filter* structure includes the values of the different fields
 *   to match: source and destination IP addresses, vlan id, flexbytes, source
 *   and destination ports, and so on.
 *   IPv6 are not supported.
 * @param soft_id
 *    The soft_id value provided when adding/updating the removed filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in perfect mode
 *      on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct.
 */
int rte_eth_dev_fdir_remove_perfect_filter(uint8_t port_id,
					   struct rte_fdir_filter *fdir_filter,
					   uint16_t soft_id);
/**
 * Configure globally the masks for flow director mode for an Ethernet device.
 * For example, the device can match packets with only the first 24 bits of
 * the IPv4 source address.
 *
 * The following fields can be masked: IPv4 addresses and L4 port numbers.
 * The following fields can be either enabled or disabled completely for the
 * matching functionality: VLAN ID tag; VLAN Priority + CFI bit; Flexible 2-byte
 * tuple.
 * IPv6 masks are not supported.
 *
 * All filters must comply with the masks previously configured.
 * For example, with a mask equal to 255.255.255.0 for the source IPv4 address,
 * all IPv4 filters must be created with a source IPv4 address that fits the
 * "X.X.X.0" format.
 *
 * This function flushes all filters that have been previously added in
 * the device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fdir_mask
 *   The pointer to the fdir mask structure describing relevant headers fields
 *   and relevant bits to use when matching packets addresses and ports.
 *   IPv6 masks are not supported.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if the flow director mode is not configured in perfect
 *      mode on *port_id*.
 *   - (-EINVAL) if the fdir_filter information is not correct
 */
int rte_eth_dev_fdir_set_masks(uint8_t port_id,
			       struct rte_fdir_masks *fdir_mask);

/**
 * The eth device event type for interrupt, and maybe others in the future.
 */
enum rte_eth_event_type {
	RTE_ETH_EVENT_UNKNOWN,  /**< unknown event type */
	RTE_ETH_EVENT_INTR_LSC, /**< lsc interrupt event */
	RTE_ETH_EVENT_MAX       /**< max value of this enum */
};

typedef void (*rte_eth_dev_cb_fn)(uint8_t port_id, \
		enum rte_eth_event_type event, void *cb_arg);
/**< user application callback to be registered for interrupts */



/**
 * Register a callback function for specific port id.
 *
 * @param port_id
 *  Port id.
 * @param event
 *  Event interested.
 * @param cb_fn
 *  User supplied callback function to be called.
 * @param cb_arg
 *  Pointer to the parameters for the registered callback.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_eth_dev_callback_register(uint8_t port_id,
			enum rte_eth_event_type event,
		rte_eth_dev_cb_fn cb_fn, void *cb_arg);

/**
 * Unregister a callback function for specific port id.
 *
 * @param port_id
 *  Port id.
 * @param event
 *  Event interested.
 * @param cb_fn
 *  User supplied callback function to be called.
 * @param cb_arg
 *  Pointer to the parameters for the registered callback. -1 means to
 *  remove all for the same callback address and same event.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_eth_dev_callback_unregister(uint8_t port_id,
			enum rte_eth_event_type event,
		rte_eth_dev_cb_fn cb_fn, void *cb_arg);

/**
 * @internal Executes all the user application registered callbacks for
 * the specific device. It is for DPDK internal user only. User
 * application should not call it directly.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param event
 *  Eth device interrupt event type.
 *
 * @return
 *  void
 */
void _rte_eth_dev_callback_process(struct rte_eth_dev *dev,
				enum rte_eth_event_type event);

/**
 * Turn on the LED on the Ethernet device.
 * This function turns on the LED on the Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support
 *     that operation.
 *   - (-ENODEV) if *port_id* invalid.
 */
int  rte_eth_led_on(uint8_t port_id);

/**
 * Turn off the LED on the Ethernet device.
 * This function turns off the LED on the Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support
 *     that operation.
 *   - (-ENODEV) if *port_id* invalid.
 */
int  rte_eth_led_off(uint8_t port_id);

/**
 * Configure the Ethernet link flow control for Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fc_conf
 *   The pointer to the structure of the flow control parameters.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow director mode.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EINVAL)  if bad parameter
 *   - (-EIO)     if flow control setup failure
 */
int rte_eth_dev_flow_ctrl_set(uint8_t port_id,
				struct rte_eth_fc_conf *fc_conf);

/**
 * Add a MAC address to an internal array of addresses used to enable whitelist
 * filtering to accept packets only if the destination MAC address matches.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   The MAC address to add.
 * @param pool
 *   VMDq pool index to associate address with (if VMDq is enabled). If VMDq is
 *   not enabled, this should be set to 0.
 * @return
 *   - (0) if successfully added or *mac_addr" was already added.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port* is invalid.
 *   - (-ENOSPC) if no more MAC addresses can be added.
 *   - (-EINVAL) if MAC address is invalid.
 */
int rte_eth_dev_mac_addr_add(uint8_t port, struct ether_addr *mac_addr,
				uint32_t pool);

/**
 * Remove a MAC address from the internal array of addresses.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   MAC address to remove.
 * @return
 *   - (0) if successful, or *mac_addr* didn't exist.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EADDRINUSE) if attempting to remove the default MAC address
 */
int rte_eth_dev_mac_addr_remove(uint8_t port, struct ether_addr *mac_addr);


#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_H_ */
