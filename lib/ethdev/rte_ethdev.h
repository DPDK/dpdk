/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
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
 *   an Ethernet device (configure it, setup its Rx and Tx queues and start it),
 *   to get its MAC address, the speed and the status of its physical link,
 *   to receive and to transmit packets, and so on.
 *
 * - The driver-oriented Ethernet API that exports functions allowing
 *   an Ethernet Poll Mode Driver (PMD) to allocate an Ethernet device instance,
 *   create memzone for HW rings and process registered callbacks, and so on.
 *   PMDs should include ethdev_driver.h instead of this header.
 *
 * By default, all the functions of the Ethernet Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on
 * different logical cores to work on the same target object.  For instance,
 * the receive function of a PMD cannot be invoked in parallel on two logical
 * cores to poll the same Rx queue [of the same port]. Of course, this function
 * can be invoked in parallel by different logical cores on different Rx queues.
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
 * again. The transmit and receive functions should not be invoked when the
 * device or the queue is stopped.
 *
 * Please note that some configuration is not stored between calls to
 * rte_eth_dev_stop()/rte_eth_dev_start(). The following configuration will
 * be retained:
 *
 *     - MTU
 *     - flow control settings
 *     - receive mode configuration (promiscuous mode, all-multicast mode,
 *       hardware checksum mode, RSS/VMDq settings etc.)
 *     - VLAN filtering configuration
 *     - default MAC address
 *     - MAC addresses supplied to MAC address array
 *     - flow director filtering mode (but not filtering rules)
 *     - NIC queue statistics mappings
 *
 * The following configuration may be retained or not
 * depending on the device capabilities:
 *
 *     - flow rules
 *     - flow-related shared objects, e.g. indirect actions
 *
 * Any other configuration will not be stored and will need to be re-entered
 * before a call to rte_eth_dev_start().
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
 * For performance reasons, the address of the burst-oriented Rx and Tx
 * functions of the Ethernet driver are not contained in the *eth_dev_ops*
 * structure. Instead, they are directly stored at the beginning of the
 * *rte_eth_dev* structure to avoid an extra indirect memory access during
 * their invocation.
 *
 * RTE Ethernet device drivers do not use interrupts for transmitting or
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

/* Use this macro to check if LRO API is supported */
#define RTE_ETHDEV_HAS_LRO_SUPPORT

/* Alias RTE_LIBRTE_ETHDEV_DEBUG for backward compatibility. */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
#define RTE_ETHDEV_DEBUG_RX
#define RTE_ETHDEV_DEBUG_TX
#endif

#include <rte_cman.h>
#include <rte_compat.h>
#include <rte_log.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_bitops.h>
#include <rte_errno.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_power_intrinsics.h>

#include "rte_ethdev_trace_fp.h"
#include "rte_dev_info.h"

extern int rte_eth_dev_logtype;

#define RTE_ETHDEV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, rte_eth_dev_logtype, "" __VA_ARGS__)

struct rte_mbuf;

/**
 * Initializes a device iterator.
 *
 * This iterator allows accessing a list of devices matching some devargs.
 *
 * @param iter
 *   Device iterator handle initialized by the function.
 *   The fields bus_str and cls_str might be dynamically allocated,
 *   and could be freed by calling rte_eth_iterator_cleanup().
 *
 * @param devargs
 *   Device description string.
 *
 * @return
 *   0 on successful initialization, negative otherwise.
 */
int rte_eth_iterator_init(struct rte_dev_iterator *iter, const char *devargs);

/**
 * Iterates on devices with devargs filter.
 * The ownership is not checked.
 *
 * The next port ID is returned, and the iterator is updated.
 *
 * @param iter
 *   Device iterator handle initialized by rte_eth_iterator_init().
 *   Some fields bus_str and cls_str might be freed when no more port is found,
 *   by calling rte_eth_iterator_cleanup().
 *
 * @return
 *   A port ID if found, RTE_MAX_ETHPORTS otherwise.
 */
uint16_t rte_eth_iterator_next(struct rte_dev_iterator *iter);

/**
 * Free some allocated fields of the iterator.
 *
 * This function is automatically called by rte_eth_iterator_next()
 * on the last iteration (i.e. when no more matching port is found).
 *
 * It is safe to call this function twice; it will do nothing more.
 *
 * @param iter
 *   Device iterator handle initialized by rte_eth_iterator_init().
 *   The fields bus_str and cls_str are freed if needed.
 */
void rte_eth_iterator_cleanup(struct rte_dev_iterator *iter);

/**
 * Macro to iterate over all ethdev ports matching some devargs.
 *
 * If a break is done before the end of the loop,
 * the function rte_eth_iterator_cleanup() must be called.
 *
 * @param id
 *   Iterated port ID of type uint16_t.
 * @param devargs
 *   Device parameters input as string of type char*.
 * @param iter
 *   Iterator handle of type struct rte_dev_iterator, used internally.
 */
#define RTE_ETH_FOREACH_MATCHING_DEV(id, devargs, iter) \
	for (rte_eth_iterator_init(iter, devargs), \
	     id = rte_eth_iterator_next(iter); \
	     id != RTE_MAX_ETHPORTS; \
	     id = rte_eth_iterator_next(iter))

/**
 * A structure used to retrieve statistics for an Ethernet port.
 * Not all statistics fields in struct rte_eth_stats are supported
 * by any type of network interface card (NIC). If any statistics
 * field is not supported, its value is 0.
 * All byte-related statistics do not include Ethernet FCS regardless
 * of whether these bytes have been delivered to the application
 * (see RTE_ETH_RX_OFFLOAD_KEEP_CRC).
 */
struct rte_eth_stats {
	uint64_t ipackets;  /**< Total number of successfully received packets. */
	uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
	uint64_t ibytes;    /**< Total number of successfully received bytes. */
	uint64_t obytes;    /**< Total number of successfully transmitted bytes. */
	/**
	 * Total of Rx packets dropped by the HW,
	 * because there are no available buffer (i.e. Rx queues are full).
	 */
	uint64_t imissed;
	uint64_t ierrors;   /**< Total number of erroneous received packets. */
	uint64_t oerrors;   /**< Total number of failed transmitted packets. */
	uint64_t rx_nombuf; /**< Total number of Rx mbuf allocation failures. */
	/* Queue stats are limited to max 256 queues */
	/** Total number of queue Rx packets. */
	uint64_t q_ipackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/** Total number of queue Tx packets. */
	uint64_t q_opackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/** Total number of successfully received queue bytes. */
	uint64_t q_ibytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/** Total number of successfully transmitted queue bytes. */
	uint64_t q_obytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/** Total number of queue packets received that are dropped. */
	uint64_t q_errors[RTE_ETHDEV_QUEUE_STAT_CNTRS];
};

/**@{@name Link speed capabilities
 * Device supported speeds bitmap flags
 */
#define RTE_ETH_LINK_SPEED_AUTONEG 0             /**< Autonegotiate (all speeds) */
#define RTE_ETH_LINK_SPEED_FIXED   RTE_BIT32(0)  /**< Disable autoneg (fixed speed) */
#define RTE_ETH_LINK_SPEED_10M_HD  RTE_BIT32(1)  /**<  10 Mbps half-duplex */
#define RTE_ETH_LINK_SPEED_10M     RTE_BIT32(2)  /**<  10 Mbps full-duplex */
#define RTE_ETH_LINK_SPEED_100M_HD RTE_BIT32(3)  /**< 100 Mbps half-duplex */
#define RTE_ETH_LINK_SPEED_100M    RTE_BIT32(4)  /**< 100 Mbps full-duplex */
#define RTE_ETH_LINK_SPEED_1G      RTE_BIT32(5)  /**<   1 Gbps */
#define RTE_ETH_LINK_SPEED_2_5G    RTE_BIT32(6)  /**< 2.5 Gbps */
#define RTE_ETH_LINK_SPEED_5G      RTE_BIT32(7)  /**<   5 Gbps */
#define RTE_ETH_LINK_SPEED_10G     RTE_BIT32(8)  /**<  10 Gbps */
#define RTE_ETH_LINK_SPEED_20G     RTE_BIT32(9)  /**<  20 Gbps */
#define RTE_ETH_LINK_SPEED_25G     RTE_BIT32(10) /**<  25 Gbps */
#define RTE_ETH_LINK_SPEED_40G     RTE_BIT32(11) /**<  40 Gbps */
#define RTE_ETH_LINK_SPEED_50G     RTE_BIT32(12) /**<  50 Gbps */
#define RTE_ETH_LINK_SPEED_56G     RTE_BIT32(13) /**<  56 Gbps */
#define RTE_ETH_LINK_SPEED_100G    RTE_BIT32(14) /**< 100 Gbps */
#define RTE_ETH_LINK_SPEED_200G    RTE_BIT32(15) /**< 200 Gbps */
#define RTE_ETH_LINK_SPEED_400G    RTE_BIT32(16) /**< 400 Gbps */
/**@}*/

/**@{@name Link speed
 * Ethernet numeric link speeds in Mbps
 */
#define RTE_ETH_SPEED_NUM_NONE         0 /**< Not defined */
#define RTE_ETH_SPEED_NUM_10M         10 /**<  10 Mbps */
#define RTE_ETH_SPEED_NUM_100M       100 /**< 100 Mbps */
#define RTE_ETH_SPEED_NUM_1G        1000 /**<   1 Gbps */
#define RTE_ETH_SPEED_NUM_2_5G      2500 /**< 2.5 Gbps */
#define RTE_ETH_SPEED_NUM_5G        5000 /**<   5 Gbps */
#define RTE_ETH_SPEED_NUM_10G      10000 /**<  10 Gbps */
#define RTE_ETH_SPEED_NUM_20G      20000 /**<  20 Gbps */
#define RTE_ETH_SPEED_NUM_25G      25000 /**<  25 Gbps */
#define RTE_ETH_SPEED_NUM_40G      40000 /**<  40 Gbps */
#define RTE_ETH_SPEED_NUM_50G      50000 /**<  50 Gbps */
#define RTE_ETH_SPEED_NUM_56G      56000 /**<  56 Gbps */
#define RTE_ETH_SPEED_NUM_100G    100000 /**< 100 Gbps */
#define RTE_ETH_SPEED_NUM_200G    200000 /**< 200 Gbps */
#define RTE_ETH_SPEED_NUM_400G    400000 /**< 400 Gbps */
#define RTE_ETH_SPEED_NUM_UNKNOWN UINT32_MAX /**< Unknown */
/**@}*/

/**
 * A structure used to retrieve link-level information of an Ethernet port.
 */
__extension__
struct rte_eth_link {
	uint32_t link_speed;        /**< RTE_ETH_SPEED_NUM_ */
	uint16_t link_duplex  : 1;  /**< RTE_ETH_LINK_[HALF/FULL]_DUPLEX */
	uint16_t link_autoneg : 1;  /**< RTE_ETH_LINK_[AUTONEG/FIXED] */
	uint16_t link_status  : 1;  /**< RTE_ETH_LINK_[DOWN/UP] */
} __rte_aligned(8);      /**< aligned for atomic64 read/write */

/**@{@name Link negotiation
 * Constants used in link management.
 */
#define RTE_ETH_LINK_HALF_DUPLEX 0 /**< Half-duplex connection (see link_duplex). */
#define RTE_ETH_LINK_FULL_DUPLEX 1 /**< Full-duplex connection (see link_duplex). */
#define RTE_ETH_LINK_DOWN        0 /**< Link is down (see link_status). */
#define RTE_ETH_LINK_UP          1 /**< Link is up (see link_status). */
#define RTE_ETH_LINK_FIXED       0 /**< No autonegotiation (see link_autoneg). */
#define RTE_ETH_LINK_AUTONEG     1 /**< Autonegotiated (see link_autoneg). */
#define RTE_ETH_LINK_MAX_STR_LEN 40 /**< Max length of default link string. */
/**@}*/

/**
 * A structure used to configure the ring threshold registers of an Rx/Tx
 * queue for an Ethernet port.
 */
struct rte_eth_thresh {
	uint8_t pthresh; /**< Ring prefetch threshold. */
	uint8_t hthresh; /**< Ring host threshold. */
	uint8_t wthresh; /**< Ring writeback threshold. */
};

/**@{@name Multi-queue mode
 * @see rte_eth_conf.rxmode.mq_mode.
 */
#define RTE_ETH_MQ_RX_RSS_FLAG  RTE_BIT32(0) /**< Enable RSS. @see rte_eth_rss_conf */
#define RTE_ETH_MQ_RX_DCB_FLAG  RTE_BIT32(1) /**< Enable DCB. */
#define RTE_ETH_MQ_RX_VMDQ_FLAG RTE_BIT32(2) /**< Enable VMDq. */
/**@}*/

/**
 *  A set of values to identify what method is to be used to route
 *  packets to multiple queues.
 */
enum rte_eth_rx_mq_mode {
	/** None of DCB, RSS or VMDq mode */
	RTE_ETH_MQ_RX_NONE = 0,

	/** For Rx side, only RSS is on */
	RTE_ETH_MQ_RX_RSS = RTE_ETH_MQ_RX_RSS_FLAG,
	/** For Rx side,only DCB is on. */
	RTE_ETH_MQ_RX_DCB = RTE_ETH_MQ_RX_DCB_FLAG,
	/** Both DCB and RSS enable */
	RTE_ETH_MQ_RX_DCB_RSS = RTE_ETH_MQ_RX_RSS_FLAG | RTE_ETH_MQ_RX_DCB_FLAG,

	/** Only VMDq, no RSS nor DCB */
	RTE_ETH_MQ_RX_VMDQ_ONLY = RTE_ETH_MQ_RX_VMDQ_FLAG,
	/** RSS mode with VMDq */
	RTE_ETH_MQ_RX_VMDQ_RSS = RTE_ETH_MQ_RX_RSS_FLAG | RTE_ETH_MQ_RX_VMDQ_FLAG,
	/** Use VMDq+DCB to route traffic to queues */
	RTE_ETH_MQ_RX_VMDQ_DCB = RTE_ETH_MQ_RX_VMDQ_FLAG | RTE_ETH_MQ_RX_DCB_FLAG,
	/** Enable both VMDq and DCB in VMDq */
	RTE_ETH_MQ_RX_VMDQ_DCB_RSS = RTE_ETH_MQ_RX_RSS_FLAG | RTE_ETH_MQ_RX_DCB_FLAG |
				 RTE_ETH_MQ_RX_VMDQ_FLAG,
};

/**
 * A set of values to identify what method is to be used to transmit
 * packets using multi-TCs.
 */
enum rte_eth_tx_mq_mode {
	RTE_ETH_MQ_TX_NONE    = 0,  /**< It is in neither DCB nor VT mode. */
	RTE_ETH_MQ_TX_DCB,          /**< For Tx side,only DCB is on. */
	RTE_ETH_MQ_TX_VMDQ_DCB,     /**< For Tx side,both DCB and VT is on. */
	RTE_ETH_MQ_TX_VMDQ_ONLY,    /**< Only VT on, no DCB */
};

/**
 * A structure used to configure the Rx features of an Ethernet port.
 */
struct rte_eth_rxmode {
	/** The multi-queue packet distribution mode to be used, e.g. RSS. */
	enum rte_eth_rx_mq_mode mq_mode;
	uint32_t mtu;  /**< Requested MTU. */
	/** Maximum allowed size of LRO aggregated packet. */
	uint32_t max_lro_pkt_size;
	/**
	 * Per-port Rx offloads to be set using RTE_ETH_RX_OFFLOAD_* flags.
	 * Only offloads set on rx_offload_capa field on rte_eth_dev_info
	 * structure are allowed to be set.
	 */
	uint64_t offloads;

	uint64_t reserved_64s[2]; /**< Reserved for future fields */
	void *reserved_ptrs[2];   /**< Reserved for future fields */
};

/**
 * VLAN types to indicate if it is for single VLAN, inner VLAN or outer VLAN.
 * Note that single VLAN is treated the same as inner VLAN.
 */
enum rte_vlan_type {
	RTE_ETH_VLAN_TYPE_UNKNOWN = 0,
	RTE_ETH_VLAN_TYPE_INNER, /**< Inner VLAN. */
	RTE_ETH_VLAN_TYPE_OUTER, /**< Single VLAN, or outer VLAN. */
	RTE_ETH_VLAN_TYPE_MAX,
};

/**
 * A structure used to describe a VLAN filter.
 * If the bit corresponding to a VID is set, such VID is on.
 */
struct rte_vlan_filter_conf {
	uint64_t ids[64];
};

/**
 * A structure used to configure the Receive Side Scaling (RSS) feature
 * of an Ethernet port.
 * If not NULL, the *rss_key* pointer of the *rss_conf* structure points
 * to an array holding the RSS key to use for hashing specific header
 * fields of received packets. The length of this array should be indicated
 * by *rss_key_len* below. Otherwise, a default random hash key is used by
 * the device driver.
 *
 * The *rss_key_len* field of the *rss_conf* structure indicates the length
 * in bytes of the array pointed by *rss_key*. To be compatible, this length
 * will be checked in i40e only. Others assume 40 bytes to be used as before.
 *
 * The *rss_hf* field of the *rss_conf* structure indicates the different
 * types of IPv4/IPv6 packets to which the RSS hashing must be applied.
 * Supplying an *rss_hf* equal to zero disables the RSS feature.
 */
struct rte_eth_rss_conf {
	uint8_t *rss_key;    /**< If not NULL, 40-byte hash key. */
	uint8_t rss_key_len; /**< hash key length in bytes. */
	uint64_t rss_hf;     /**< Hash functions to apply - see below. */
};

/*
 * A packet can be identified by hardware as different flow types. Different
 * NIC hardware may support different flow types.
 * Basically, the NIC hardware identifies the flow type as deep protocol as
 * possible, and exclusively. For example, if a packet is identified as
 * 'RTE_ETH_FLOW_NONFRAG_IPV4_TCP', it will not be any of other flow types,
 * though it is an actual IPV4 packet.
 */
#define RTE_ETH_FLOW_UNKNOWN             0
#define RTE_ETH_FLOW_RAW                 1
#define RTE_ETH_FLOW_IPV4                2
#define RTE_ETH_FLOW_FRAG_IPV4           3
#define RTE_ETH_FLOW_NONFRAG_IPV4_TCP    4
#define RTE_ETH_FLOW_NONFRAG_IPV4_UDP    5
#define RTE_ETH_FLOW_NONFRAG_IPV4_SCTP   6
#define RTE_ETH_FLOW_NONFRAG_IPV4_OTHER  7
#define RTE_ETH_FLOW_IPV6                8
#define RTE_ETH_FLOW_FRAG_IPV6           9
#define RTE_ETH_FLOW_NONFRAG_IPV6_TCP   10
#define RTE_ETH_FLOW_NONFRAG_IPV6_UDP   11
#define RTE_ETH_FLOW_NONFRAG_IPV6_SCTP  12
#define RTE_ETH_FLOW_NONFRAG_IPV6_OTHER 13
#define RTE_ETH_FLOW_L2_PAYLOAD         14
#define RTE_ETH_FLOW_IPV6_EX            15
#define RTE_ETH_FLOW_IPV6_TCP_EX        16
#define RTE_ETH_FLOW_IPV6_UDP_EX        17
/** Consider device port number as a flow differentiator */
#define RTE_ETH_FLOW_PORT               18
#define RTE_ETH_FLOW_VXLAN              19 /**< VXLAN protocol based flow */
#define RTE_ETH_FLOW_GENEVE             20 /**< GENEVE protocol based flow */
#define RTE_ETH_FLOW_NVGRE              21 /**< NVGRE protocol based flow */
#define RTE_ETH_FLOW_VXLAN_GPE          22 /**< VXLAN-GPE protocol based flow */
#define RTE_ETH_FLOW_GTPU               23 /**< GTPU protocol based flow */
#define RTE_ETH_FLOW_MAX                24

/*
 * Below macros are defined for RSS offload types, they can be used to
 * fill rte_eth_rss_conf.rss_hf or rte_flow_action_rss.types.
 */
#define RTE_ETH_RSS_IPV4               RTE_BIT64(2)
#define RTE_ETH_RSS_FRAG_IPV4          RTE_BIT64(3)
#define RTE_ETH_RSS_NONFRAG_IPV4_TCP   RTE_BIT64(4)
#define RTE_ETH_RSS_NONFRAG_IPV4_UDP   RTE_BIT64(5)
#define RTE_ETH_RSS_NONFRAG_IPV4_SCTP  RTE_BIT64(6)
#define RTE_ETH_RSS_NONFRAG_IPV4_OTHER RTE_BIT64(7)
#define RTE_ETH_RSS_IPV6               RTE_BIT64(8)
#define RTE_ETH_RSS_FRAG_IPV6          RTE_BIT64(9)
#define RTE_ETH_RSS_NONFRAG_IPV6_TCP   RTE_BIT64(10)
#define RTE_ETH_RSS_NONFRAG_IPV6_UDP   RTE_BIT64(11)
#define RTE_ETH_RSS_NONFRAG_IPV6_SCTP  RTE_BIT64(12)
#define RTE_ETH_RSS_NONFRAG_IPV6_OTHER RTE_BIT64(13)
#define RTE_ETH_RSS_L2_PAYLOAD         RTE_BIT64(14)
#define RTE_ETH_RSS_IPV6_EX            RTE_BIT64(15)
#define RTE_ETH_RSS_IPV6_TCP_EX        RTE_BIT64(16)
#define RTE_ETH_RSS_IPV6_UDP_EX        RTE_BIT64(17)
#define RTE_ETH_RSS_PORT               RTE_BIT64(18)
#define RTE_ETH_RSS_VXLAN              RTE_BIT64(19)
#define RTE_ETH_RSS_GENEVE             RTE_BIT64(20)
#define RTE_ETH_RSS_NVGRE              RTE_BIT64(21)
#define RTE_ETH_RSS_GTPU               RTE_BIT64(23)
#define RTE_ETH_RSS_ETH                RTE_BIT64(24)
#define RTE_ETH_RSS_S_VLAN             RTE_BIT64(25)
#define RTE_ETH_RSS_C_VLAN             RTE_BIT64(26)
#define RTE_ETH_RSS_ESP                RTE_BIT64(27)
#define RTE_ETH_RSS_AH                 RTE_BIT64(28)
#define RTE_ETH_RSS_L2TPV3             RTE_BIT64(29)
#define RTE_ETH_RSS_PFCP               RTE_BIT64(30)
#define RTE_ETH_RSS_PPPOE              RTE_BIT64(31)
#define RTE_ETH_RSS_ECPRI              RTE_BIT64(32)
#define RTE_ETH_RSS_MPLS               RTE_BIT64(33)
#define RTE_ETH_RSS_IPV4_CHKSUM        RTE_BIT64(34)

/**
 * The RTE_ETH_RSS_L4_CHKSUM works on checksum field of any L4 header.
 * It is similar to RTE_ETH_RSS_PORT that they don't specify the specific type of
 * L4 header. This macro is defined to replace some specific L4 (TCP/UDP/SCTP)
 * checksum type for constructing the use of RSS offload bits.
 *
 * Due to above reason, some old APIs (and configuration) don't support
 * RTE_ETH_RSS_L4_CHKSUM. The rte_flow RSS API supports it.
 *
 * For the case that checksum is not used in an UDP header,
 * it takes the reserved value 0 as input for the hash function.
 */
#define RTE_ETH_RSS_L4_CHKSUM          RTE_BIT64(35)

#define RTE_ETH_RSS_L2TPV2             RTE_BIT64(36)

/*
 * We use the following macros to combine with above RTE_ETH_RSS_* for
 * more specific input set selection. These bits are defined starting
 * from the high end of the 64 bits.
 * Note: If we use above RTE_ETH_RSS_* without SRC/DST_ONLY, it represents
 * both SRC and DST are taken into account. If SRC_ONLY and DST_ONLY of
 * the same level are used simultaneously, it is the same case as none of
 * them are added.
 */
#define RTE_ETH_RSS_L3_SRC_ONLY        RTE_BIT64(63)
#define RTE_ETH_RSS_L3_DST_ONLY        RTE_BIT64(62)
#define RTE_ETH_RSS_L4_SRC_ONLY        RTE_BIT64(61)
#define RTE_ETH_RSS_L4_DST_ONLY        RTE_BIT64(60)
#define RTE_ETH_RSS_L2_SRC_ONLY        RTE_BIT64(59)
#define RTE_ETH_RSS_L2_DST_ONLY        RTE_BIT64(58)

/*
 * Only select IPV6 address prefix as RSS input set according to
 * https://tools.ietf.org/html/rfc6052
 * Must be combined with RTE_ETH_RSS_IPV6, RTE_ETH_RSS_NONFRAG_IPV6_UDP,
 * RTE_ETH_RSS_NONFRAG_IPV6_TCP, RTE_ETH_RSS_NONFRAG_IPV6_SCTP.
 */
#define RTE_ETH_RSS_L3_PRE32           RTE_BIT64(57)
#define RTE_ETH_RSS_L3_PRE40           RTE_BIT64(56)
#define RTE_ETH_RSS_L3_PRE48           RTE_BIT64(55)
#define RTE_ETH_RSS_L3_PRE56           RTE_BIT64(54)
#define RTE_ETH_RSS_L3_PRE64           RTE_BIT64(53)
#define RTE_ETH_RSS_L3_PRE96           RTE_BIT64(52)

/*
 * Use the following macros to combine with the above layers
 * to choose inner and outer layers or both for RSS computation.
 * Bits 50 and 51 are reserved for this.
 */

/**
 * level 0, requests the default behavior.
 * Depending on the packet type, it can mean outermost, innermost,
 * anything in between or even no RSS.
 * It basically stands for the innermost encapsulation level RSS
 * can be performed on according to PMD and device capabilities.
 */
#define RTE_ETH_RSS_LEVEL_PMD_DEFAULT  (UINT64_C(0) << 50)

/**
 * level 1, requests RSS to be performed on the outermost packet
 * encapsulation level.
 */
#define RTE_ETH_RSS_LEVEL_OUTERMOST    (UINT64_C(1) << 50)

/**
 * level 2, requests RSS to be performed on the specified inner packet
 * encapsulation level, from outermost to innermost (lower to higher values).
 */
#define RTE_ETH_RSS_LEVEL_INNERMOST    (UINT64_C(2) << 50)
#define RTE_ETH_RSS_LEVEL_MASK         (UINT64_C(3) << 50)

#define RTE_ETH_RSS_LEVEL(rss_hf) ((rss_hf & RTE_ETH_RSS_LEVEL_MASK) >> 50)

/**
 * For input set change of hash filter, if SRC_ONLY and DST_ONLY of
 * the same level are used simultaneously, it is the same case as
 * none of them are added.
 *
 * @param rss_hf
 *   RSS types with SRC/DST_ONLY.
 * @return
 *   RSS types.
 */
static inline uint64_t
rte_eth_rss_hf_refine(uint64_t rss_hf)
{
	if ((rss_hf & RTE_ETH_RSS_L3_SRC_ONLY) && (rss_hf & RTE_ETH_RSS_L3_DST_ONLY))
		rss_hf &= ~(RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);

	if ((rss_hf & RTE_ETH_RSS_L4_SRC_ONLY) && (rss_hf & RTE_ETH_RSS_L4_DST_ONLY))
		rss_hf &= ~(RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);

	return rss_hf;
}

#define RTE_ETH_RSS_IPV6_PRE32 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE32)

#define RTE_ETH_RSS_IPV6_PRE40 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE40)

#define RTE_ETH_RSS_IPV6_PRE48 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE48)

#define RTE_ETH_RSS_IPV6_PRE56 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE56)

#define RTE_ETH_RSS_IPV6_PRE64 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE64)

#define RTE_ETH_RSS_IPV6_PRE96 ( \
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_L3_PRE96)

#define RTE_ETH_RSS_IPV6_PRE32_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE32)

#define RTE_ETH_RSS_IPV6_PRE40_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE40)

#define RTE_ETH_RSS_IPV6_PRE48_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE48)

#define RTE_ETH_RSS_IPV6_PRE56_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE56)

#define RTE_ETH_RSS_IPV6_PRE64_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE64)

#define RTE_ETH_RSS_IPV6_PRE96_UDP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_L3_PRE96)

#define RTE_ETH_RSS_IPV6_PRE32_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE32)

#define RTE_ETH_RSS_IPV6_PRE40_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE40)

#define RTE_ETH_RSS_IPV6_PRE48_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE48)

#define RTE_ETH_RSS_IPV6_PRE56_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE56)

#define RTE_ETH_RSS_IPV6_PRE64_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE64)

#define RTE_ETH_RSS_IPV6_PRE96_TCP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_L3_PRE96)

#define RTE_ETH_RSS_IPV6_PRE32_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE32)

#define RTE_ETH_RSS_IPV6_PRE40_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE40)

#define RTE_ETH_RSS_IPV6_PRE48_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE48)

#define RTE_ETH_RSS_IPV6_PRE56_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE56)

#define RTE_ETH_RSS_IPV6_PRE64_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE64)

#define RTE_ETH_RSS_IPV6_PRE96_SCTP ( \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
		RTE_ETH_RSS_L3_PRE96)

#define RTE_ETH_RSS_IP ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_FRAG_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_FRAG_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_IPV6_EX)

#define RTE_ETH_RSS_UDP ( \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_IPV6_UDP_EX)

#define RTE_ETH_RSS_TCP ( \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_IPV6_TCP_EX)

#define RTE_ETH_RSS_SCTP ( \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define RTE_ETH_RSS_TUNNEL ( \
	RTE_ETH_RSS_VXLAN  | \
	RTE_ETH_RSS_GENEVE | \
	RTE_ETH_RSS_NVGRE)

#define RTE_ETH_RSS_VLAN ( \
	RTE_ETH_RSS_S_VLAN  | \
	RTE_ETH_RSS_C_VLAN)

/** Mask of valid RSS hash protocols */
#define RTE_ETH_RSS_PROTO_MASK ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_FRAG_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_FRAG_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_L2_PAYLOAD | \
	RTE_ETH_RSS_IPV6_EX | \
	RTE_ETH_RSS_IPV6_TCP_EX | \
	RTE_ETH_RSS_IPV6_UDP_EX | \
	RTE_ETH_RSS_PORT  | \
	RTE_ETH_RSS_VXLAN | \
	RTE_ETH_RSS_GENEVE | \
	RTE_ETH_RSS_NVGRE | \
	RTE_ETH_RSS_MPLS)

/*
 * Definitions used for redirection table entry size.
 * Some RSS RETA sizes may not be supported by some drivers, check the
 * documentation or the description of relevant functions for more details.
 */
#define RTE_ETH_RSS_RETA_SIZE_64  64
#define RTE_ETH_RSS_RETA_SIZE_128 128
#define RTE_ETH_RSS_RETA_SIZE_256 256
#define RTE_ETH_RSS_RETA_SIZE_512 512
#define RTE_ETH_RETA_GROUP_SIZE   64

/**@{@name VMDq and DCB maximums */
#define RTE_ETH_VMDQ_MAX_VLAN_FILTERS   64 /**< Maximum nb. of VMDq VLAN filters. */
#define RTE_ETH_DCB_NUM_USER_PRIORITIES 8  /**< Maximum nb. of DCB priorities. */
#define RTE_ETH_VMDQ_DCB_NUM_QUEUES     128 /**< Maximum nb. of VMDq DCB queues. */
#define RTE_ETH_DCB_NUM_QUEUES          128 /**< Maximum nb. of DCB queues. */
/**@}*/

/**@{@name DCB capabilities */
#define RTE_ETH_DCB_PG_SUPPORT      RTE_BIT32(0) /**< Priority Group(ETS) support. */
#define RTE_ETH_DCB_PFC_SUPPORT     RTE_BIT32(1) /**< Priority Flow Control support. */
/**@}*/

/**@{@name VLAN offload bits */
#define RTE_ETH_VLAN_STRIP_OFFLOAD   0x0001 /**< VLAN Strip  On/Off */
#define RTE_ETH_VLAN_FILTER_OFFLOAD  0x0002 /**< VLAN Filter On/Off */
#define RTE_ETH_VLAN_EXTEND_OFFLOAD  0x0004 /**< VLAN Extend On/Off */
#define RTE_ETH_QINQ_STRIP_OFFLOAD   0x0008 /**< QINQ Strip On/Off */

#define RTE_ETH_VLAN_STRIP_MASK      0x0001 /**< VLAN Strip  setting mask */
#define RTE_ETH_VLAN_FILTER_MASK     0x0002 /**< VLAN Filter  setting mask*/
#define RTE_ETH_VLAN_EXTEND_MASK     0x0004 /**< VLAN Extend  setting mask*/
#define RTE_ETH_QINQ_STRIP_MASK      0x0008 /**< QINQ Strip  setting mask */
#define RTE_ETH_VLAN_ID_MAX          0x0FFF /**< VLAN ID is in lower 12 bits*/
/**@}*/

/* Definitions used for receive MAC address */
#define RTE_ETH_NUM_RECEIVE_MAC_ADDR   128 /**< Maximum nb. of receive mac addr. */

/* Definitions used for unicast hash */
#define RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY 128 /**< Maximum nb. of UC hash array. */

/**@{@name VMDq Rx mode
 * @see rte_eth_vmdq_rx_conf.rx_mode
 */
/** Accept untagged packets. */
#define RTE_ETH_VMDQ_ACCEPT_UNTAG      RTE_BIT32(0)
/** Accept packets in multicast table. */
#define RTE_ETH_VMDQ_ACCEPT_HASH_MC    RTE_BIT32(1)
/** Accept packets in unicast table. */
#define RTE_ETH_VMDQ_ACCEPT_HASH_UC    RTE_BIT32(2)
/** Accept broadcast packets. */
#define RTE_ETH_VMDQ_ACCEPT_BROADCAST  RTE_BIT32(3)
/** Multicast promiscuous. */
#define RTE_ETH_VMDQ_ACCEPT_MULTICAST  RTE_BIT32(4)
/**@}*/

/**
 * A structure used to configure 64 entries of Redirection Table of the
 * Receive Side Scaling (RSS) feature of an Ethernet port. To configure
 * more than 64 entries supported by hardware, an array of this structure
 * is needed.
 */
struct rte_eth_rss_reta_entry64 {
	/** Mask bits indicate which entries need to be updated/queried. */
	uint64_t mask;
	/** Group of 64 redirection table entries. */
	uint16_t reta[RTE_ETH_RETA_GROUP_SIZE];
};

/**
 * This enum indicates the possible number of traffic classes
 * in DCB configurations
 */
enum rte_eth_nb_tcs {
	RTE_ETH_4_TCS = 4, /**< 4 TCs with DCB. */
	RTE_ETH_8_TCS = 8  /**< 8 TCs with DCB. */
};

/**
 * This enum indicates the possible number of queue pools
 * in VMDq configurations.
 */
enum rte_eth_nb_pools {
	RTE_ETH_8_POOLS = 8,    /**< 8 VMDq pools. */
	RTE_ETH_16_POOLS = 16,  /**< 16 VMDq pools. */
	RTE_ETH_32_POOLS = 32,  /**< 32 VMDq pools. */
	RTE_ETH_64_POOLS = 64   /**< 64 VMDq pools. */
};

/* This structure may be extended in future. */
struct rte_eth_dcb_rx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[RTE_ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_vmdq_dcb_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools. */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[RTE_ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_dcb_tx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs. */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[RTE_ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_vmdq_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq mode, 64 pools. */
};

/**
 * A structure used to configure the VMDq+DCB feature
 * of an Ethernet port.
 *
 * Using this feature, packets are routed to a pool of queues, based
 * on the VLAN ID in the VLAN tag, and then to a specific queue within
 * that pool, using the user priority VLAN tag field.
 *
 * A default pool may be used, if desired, to route all traffic which
 * does not match the VLAN filter rules.
 */
struct rte_eth_vmdq_dcb_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	struct {
		uint16_t vlan_id; /**< The VLAN ID of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet Rx */
	} pool_map[RTE_ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq VLAN pool maps. */
	/** Selects a queue in a pool */
	uint8_t dcb_tc[RTE_ETH_DCB_NUM_USER_PRIORITIES];
};

/**
 * A structure used to configure the VMDq feature of an Ethernet port when
 * not combined with the DCB feature.
 *
 * Using this feature, packets are routed to a pool of queues. By default,
 * the pool selection is based on the MAC address, the VLAN ID in the
 * VLAN tag as specified in the pool_map array.
 * Passing the RTE_ETH_VMDQ_ACCEPT_UNTAG in the rx_mode field allows pool
 * selection using only the MAC address. MAC address to pool mapping is done
 * using the rte_eth_dev_mac_addr_add function, with the pool parameter
 * corresponding to the pool ID.
 *
 * Queue selection within the selected pool will be done using RSS when
 * it is enabled or revert to the first queue of the pool if not.
 *
 * A default pool may be used, if desired, to route all traffic which
 * does not match the VLAN filter rules or any pool MAC address.
 */
struct rte_eth_vmdq_rx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq only mode, 8 or 64 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t enable_loop_back; /**< Enable VT loop back */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	uint32_t rx_mode; /**< Flags from RTE_ETH_VMDQ_ACCEPT_* */
	struct {
		uint16_t vlan_id; /**< The VLAN ID of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet Rx */
	} pool_map[RTE_ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq VLAN pool maps. */
};

/**
 * A structure used to configure the Tx features of an Ethernet port.
 */
struct rte_eth_txmode {
	enum rte_eth_tx_mq_mode mq_mode; /**< Tx multi-queues mode. */
	/**
	 * Per-port Tx offloads to be set using RTE_ETH_TX_OFFLOAD_* flags.
	 * Only offloads set on tx_offload_capa field on rte_eth_dev_info
	 * structure are allowed to be set.
	 */
	uint64_t offloads;

	uint16_t pvid;
	__extension__
	uint8_t /** If set, reject sending out tagged pkts */
		hw_vlan_reject_tagged : 1,
		/** If set, reject sending out untagged pkts */
		hw_vlan_reject_untagged : 1,
		/** If set, enable port based VLAN insertion */
		hw_vlan_insert_pvid : 1;

	uint64_t reserved_64s[2]; /**< Reserved for future fields */
	void *reserved_ptrs[2];   /**< Reserved for future fields */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * A structure used to configure an Rx packet segment to split.
 *
 * If RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT flag is set in offloads field,
 * the PMD will split the received packets into multiple segments
 * according to the specification in the description array:
 *
 * - The first network buffer will be allocated from the memory pool,
 *   specified in the first array element, the second buffer, from the
 *   pool in the second element, and so on.
 *
 * - The proto_hdrs in the elements define the split position of
 *   received packets.
 *
 * - The offsets from the segment description elements specify
 *   the data offset from the buffer beginning except the first mbuf.
 *   The first segment offset is added with RTE_PKTMBUF_HEADROOM.
 *
 * - The lengths in the elements define the maximal data amount
 *   being received to each segment. The receiving starts with filling
 *   up the first mbuf data buffer up to specified length. If the
 *   there are data remaining (packet is longer than buffer in the first
 *   mbuf) the following data will be pushed to the next segment
 *   up to its own length, and so on.
 *
 * - If the length in the segment description element is zero
 *   the actual buffer size will be deduced from the appropriate
 *   memory pool properties.
 *
 * - If there is not enough elements to describe the buffer for entire
 *   packet of maximal length the following parameters will be used
 *   for the all remaining segments:
 *     - pool from the last valid element
 *     - the buffer size from this pool
 *     - zero offset
 *
 * - Length based buffer split:
 *     - mp, length, offset should be configured.
 *     - The proto_hdr field must be 0.
 *
 * - Protocol header based buffer split:
 *     - mp, offset, proto_hdr should be configured.
 *     - The length field must be 0.
 *     - The proto_hdr field in the last segment should be 0.
 *
 * - When protocol header split is enabled, NIC may receive packets
 *   which do not match all the protocol headers within the Rx segments.
 *   At this point, NIC will have two possible split behaviors according to
 *   matching results, one is exact match, another is longest match.
 *   The split result of NIC must belong to one of them.
 *   The exact match means NIC only do split when the packets exactly match all
 *   the protocol headers in the segments.
 *   Otherwise, the whole packet will be put into the last valid mempool.
 *   The longest match means NIC will do split until packets mismatch
 *   the protocol header in the segments.
 *   The rest will be put into the last valid pool.
 */
struct rte_eth_rxseg_split {
	struct rte_mempool *mp; /**< Memory pool to allocate segment from. */
	uint16_t length; /**< Segment data length, configures split point. */
	uint16_t offset; /**< Data offset from beginning of mbuf data buffer. */
	/**
	 * proto_hdr defines a bit mask of the protocol sequence as RTE_PTYPE_*.
	 * The last RTE_PTYPE* in the mask indicates the split position.
	 *
	 * If one protocol header is defined to split packets into two segments,
	 * for non-tunneling packets, the complete protocol sequence should be defined.
	 * For tunneling packets, for simplicity, only the tunnel and inner part of
	 * complete protocol sequence is required.
	 * If several protocol headers are defined to split packets into multi-segments,
	 * the repeated parts of adjacent segments should be omitted.
	 */
	uint32_t proto_hdr;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * A common structure used to describe Rx packet segment properties.
 */
union rte_eth_rxseg {
	/* The settings for buffer split offload. */
	struct rte_eth_rxseg_split split;
	/* The other features settings should be added here. */
};

/**
 * A structure used to configure an Rx ring of an Ethernet port.
 */
struct rte_eth_rxconf {
	struct rte_eth_thresh rx_thresh; /**< Rx ring threshold registers. */
	uint16_t rx_free_thresh; /**< Drives the freeing of Rx descriptors. */
	uint8_t rx_drop_en; /**< Drop packets if no descriptors are available. */
	uint8_t rx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
	uint16_t rx_nseg; /**< Number of descriptions in rx_seg array. */
	/**
	 * Share group index in Rx domain and switch domain.
	 * Non-zero value to enable Rx queue share, zero value disable share.
	 * PMD is responsible for Rx queue consistency checks to avoid member
	 * port's configuration contradict to each other.
	 */
	uint16_t share_group;
	uint16_t share_qid; /**< Shared Rx queue ID in group */
	/**
	 * Per-queue Rx offloads to be set using RTE_ETH_RX_OFFLOAD_* flags.
	 * Only offloads set on rx_queue_offload_capa or rx_offload_capa
	 * fields on rte_eth_dev_info structure are allowed to be set.
	 */
	uint64_t offloads;
	/**
	 * Points to the array of segment descriptions for an entire packet.
	 * Array elements are properties for consecutive Rx segments.
	 *
	 * The supported capabilities of receiving segmentation is reported
	 * in rte_eth_dev_info.rx_seg_capa field.
	 */
	union rte_eth_rxseg *rx_seg;

	/**
	 * Array of mempools to allocate Rx buffers from.
	 *
	 * This provides support for multiple mbuf pools per Rx queue.
	 * The capability is reported in device info via positive
	 * max_rx_mempools.
	 *
	 * It could be useful for more efficient usage of memory when an
	 * application creates different mempools to steer the specific
	 * size of the packet.
	 *
	 * If many mempools are specified, packets received using Rx
	 * burst may belong to any provided mempool. From ethdev user point
	 * of view it is undefined how PMD/NIC chooses mempool for a packet.
	 *
	 * If Rx scatter is enabled, a packet may be delivered using a chain
	 * of mbufs obtained from single mempool or multiple mempools based
	 * on the NIC implementation.
	 */
	struct rte_mempool **rx_mempools;
	uint16_t rx_nmempool; /** < Number of Rx mempools */

	uint64_t reserved_64s[2]; /**< Reserved for future fields */
	void *reserved_ptrs[2];   /**< Reserved for future fields */
};

/**
 * A structure used to configure a Tx ring of an Ethernet port.
 */
struct rte_eth_txconf {
	struct rte_eth_thresh tx_thresh; /**< Tx ring threshold registers. */
	uint16_t tx_rs_thresh; /**< Drives the setting of RS bit on TXDs. */
	uint16_t tx_free_thresh; /**< Start freeing Tx buffers if there are
				      less free descriptors than this value. */

	uint8_t tx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
	/**
	 * Per-queue Tx offloads to be set  using RTE_ETH_TX_OFFLOAD_* flags.
	 * Only offloads set on tx_queue_offload_capa or tx_offload_capa
	 * fields on rte_eth_dev_info structure are allowed to be set.
	 */
	uint64_t offloads;

	uint64_t reserved_64s[2]; /**< Reserved for future fields */
	void *reserved_ptrs[2];   /**< Reserved for future fields */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to return the Tx or Rx hairpin queue capabilities.
 */
struct rte_eth_hairpin_queue_cap {
	/**
	 * When set, PMD supports placing descriptors and/or data buffers
	 * in dedicated device memory.
	 */
	uint32_t locked_device_memory:1;

	/**
	 * When set, PMD supports placing descriptors and/or data buffers
	 * in host memory managed by DPDK.
	 */
	uint32_t rte_memory:1;

	uint32_t reserved:30; /**< Reserved for future fields */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to return the hairpin capabilities that are supported.
 */
struct rte_eth_hairpin_cap {
	/** The max number of hairpin queues (different bindings). */
	uint16_t max_nb_queues;
	/** Max number of Rx queues to be connected to one Tx queue. */
	uint16_t max_rx_2_tx;
	/** Max number of Tx queues to be connected to one Rx queue. */
	uint16_t max_tx_2_rx;
	uint16_t max_nb_desc; /**< The max num of descriptors. */
	struct rte_eth_hairpin_queue_cap rx_cap; /**< Rx hairpin queue capabilities. */
	struct rte_eth_hairpin_queue_cap tx_cap; /**< Tx hairpin queue capabilities. */
};

#define RTE_ETH_MAX_HAIRPIN_PEERS 32

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to hold hairpin peer data.
 */
struct rte_eth_hairpin_peer {
	uint16_t port; /**< Peer port. */
	uint16_t queue; /**< Peer queue. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to configure hairpin binding.
 */
struct rte_eth_hairpin_conf {
	uint32_t peer_count:16; /**< The number of peers. */

	/**
	 * Explicit Tx flow rule mode.
	 * One hairpin pair of queues should have the same attribute.
	 *
	 * - When set, the user should be responsible for inserting the hairpin
	 *   Tx part flows and removing them.
	 * - When clear, the PMD will try to handle the Tx part of the flows,
	 *   e.g., by splitting one flow into two parts.
	 */
	uint32_t tx_explicit:1;

	/**
	 * Manually bind hairpin queues.
	 * One hairpin pair of queues should have the same attribute.
	 *
	 * - When set, to enable hairpin, the user should call the hairpin bind
	 *   function after all the queues are set up properly and the ports are
	 *   started. Also, the hairpin unbind function should be called
	 *   accordingly before stopping a port that with hairpin configured.
	 * - When cleared, the PMD will try to enable the hairpin with the queues
	 *   configured automatically during port start.
	 */
	uint32_t manual_bind:1;

	/**
	 * Use locked device memory as a backing storage.
	 *
	 * - When set, PMD will attempt place descriptors and/or data buffers
	 *   in dedicated device memory.
	 * - When cleared, PMD will use default memory type as a backing storage.
	 *   Please refer to PMD documentation for details.
	 *
	 * API user should check if PMD supports this configuration flag using
	 * @see rte_eth_dev_hairpin_capability_get.
	 */
	uint32_t use_locked_device_memory:1;

	/**
	 * Use DPDK memory as backing storage.
	 *
	 * - When set, PMD will attempt place descriptors and/or data buffers
	 *   in host memory managed by DPDK.
	 * - When cleared, PMD will use default memory type as a backing storage.
	 *   Please refer to PMD documentation for details.
	 *
	 * API user should check if PMD supports this configuration flag using
	 * @see rte_eth_dev_hairpin_capability_get.
	 */
	uint32_t use_rte_memory:1;

	/**
	 * Force usage of hairpin memory configuration.
	 *
	 * - When set, PMD will attempt to use specified memory settings.
	 *   If resource allocation fails, then hairpin queue allocation
	 *   will result in an error.
	 * - When clear, PMD will attempt to use specified memory settings.
	 *   If resource allocation fails, then PMD will retry
	 *   allocation with default configuration.
	 */
	uint32_t force_memory:1;

	uint32_t reserved:11; /**< Reserved bits. */

	struct rte_eth_hairpin_peer peers[RTE_ETH_MAX_HAIRPIN_PEERS];
};

/**
 * A structure contains information about HW descriptor ring limitations.
 */
struct rte_eth_desc_lim {
	uint16_t nb_max;   /**< Max allowed number of descriptors. */
	uint16_t nb_min;   /**< Min allowed number of descriptors. */
	uint16_t nb_align; /**< Number of descriptors should be aligned to. */

	/**
	 * Max allowed number of segments per whole packet.
	 *
	 * - For TSO packet this is the total number of data descriptors allowed
	 *   by device.
	 *
	 * @see nb_mtu_seg_max
	 */
	uint16_t nb_seg_max;

	/**
	 * Max number of segments per one MTU.
	 *
	 * - For non-TSO packet, this is the maximum allowed number of segments
	 *   in a single transmit packet.
	 *
	 * - For TSO packet each segment within the TSO may span up to this
	 *   value.
	 *
	 * @see nb_seg_max
	 */
	uint16_t nb_mtu_seg_max;
};

/**
 * This enum indicates the flow control mode
 */
enum rte_eth_fc_mode {
	RTE_ETH_FC_NONE = 0, /**< Disable flow control. */
	RTE_ETH_FC_RX_PAUSE, /**< Rx pause frame, enable flowctrl on Tx side. */
	RTE_ETH_FC_TX_PAUSE, /**< Tx pause frame, enable flowctrl on Rx side. */
	RTE_ETH_FC_FULL      /**< Enable flow control on both side. */
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
	uint8_t mac_ctrl_frame_fwd; /**< Forward MAC control frames */
	uint8_t autoneg;      /**< Use Pause autoneg */
};

/**
 * A structure used to configure Ethernet priority flow control parameter.
 * These parameters will be configured into the register of the NIC.
 * Please refer to the corresponding data sheet for proper value.
 */
struct rte_eth_pfc_conf {
	struct rte_eth_fc_conf fc; /**< General flow control parameter. */
	uint8_t priority;          /**< VLAN User Priority. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to retrieve information of queue based PFC.
 */
struct rte_eth_pfc_queue_info {
	/**
	 * Maximum supported traffic class as per PFC (802.1Qbb) specification.
	 */
	uint8_t tc_max;
	/** PFC queue mode capabilities. */
	enum rte_eth_fc_mode mode_capa;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * A structure used to configure Ethernet priority flow control parameters for
 * ethdev queues.
 *
 * rte_eth_pfc_queue_conf::rx_pause structure shall be used to configure given
 * tx_qid with corresponding tc. When ethdev device receives PFC frame with
 * rte_eth_pfc_queue_conf::rx_pause::tc, traffic will be paused on
 * rte_eth_pfc_queue_conf::rx_pause::tx_qid for that tc.
 *
 * rte_eth_pfc_queue_conf::tx_pause structure shall be used to configure given
 * rx_qid. When rx_qid is congested, PFC frames are generated with
 * rte_eth_pfc_queue_conf::rx_pause::tc and
 * rte_eth_pfc_queue_conf::rx_pause::pause_time to the peer.
 */
struct rte_eth_pfc_queue_conf {
	enum rte_eth_fc_mode mode; /**< Link flow control mode */

	struct {
		uint16_t tx_qid; /**< Tx queue ID */
		/** Traffic class as per PFC (802.1Qbb) spec. The value must be
		 * in the range [0, rte_eth_pfc_queue_info::tx_max - 1]
		 */
		uint8_t tc;
	} rx_pause; /* Valid when (mode == FC_RX_PAUSE || mode == FC_FULL) */

	struct {
		uint16_t pause_time; /**< Pause quota in the Pause frame */
		uint16_t rx_qid;     /**< Rx queue ID */
		/** Traffic class as per PFC (802.1Qbb) spec. The value must be
		 * in the range [0, rte_eth_pfc_queue_info::tx_max - 1]
		 */
		uint8_t tc;
	} tx_pause; /* Valid when (mode == FC_TX_PAUSE || mode == FC_FULL) */
};

/**
 * Tunnel type for device-specific classifier configuration.
 * @see rte_eth_udp_tunnel
 */
enum rte_eth_tunnel_type {
	RTE_ETH_TUNNEL_TYPE_NONE = 0,
	RTE_ETH_TUNNEL_TYPE_VXLAN,
	RTE_ETH_TUNNEL_TYPE_GENEVE,
	RTE_ETH_TUNNEL_TYPE_TEREDO,
	RTE_ETH_TUNNEL_TYPE_NVGRE,
	RTE_ETH_TUNNEL_TYPE_IP_IN_GRE,
	RTE_ETH_L2_TUNNEL_TYPE_E_TAG,
	RTE_ETH_TUNNEL_TYPE_VXLAN_GPE,
	RTE_ETH_TUNNEL_TYPE_ECPRI,
	RTE_ETH_TUNNEL_TYPE_MAX,
};

/* Deprecated API file for rte_eth_dev_filter_* functions */
#include "rte_eth_ctrl.h"

/**
 * UDP tunneling configuration.
 *
 * Used to configure the classifier of a device,
 * associating an UDP port with a type of tunnel.
 *
 * Some NICs may need such configuration to properly parse a tunnel
 * with any standard or custom UDP port.
 */
struct rte_eth_udp_tunnel {
	uint16_t udp_port; /**< UDP port used for the tunnel. */
	uint8_t prot_type; /**< Tunnel type. @see rte_eth_tunnel_type */
};

/**
 * A structure used to enable/disable specific device interrupts.
 */
struct rte_eth_intr_conf {
	/** enable/disable lsc interrupt. 0 (default) - disable, 1 enable */
	uint32_t lsc:1;
	/** enable/disable rxq interrupt. 0 (default) - disable, 1 enable */
	uint32_t rxq:1;
	/** enable/disable rmv interrupt. 0 (default) - disable, 1 enable */
	uint32_t rmv:1;
};

#define rte_intr_conf rte_eth_intr_conf

/**
 * A structure used to configure an Ethernet port.
 * Depending upon the Rx multi-queue mode, extra advanced
 * configuration settings may be needed.
 */
struct rte_eth_conf {
	uint32_t link_speeds; /**< bitmap of RTE_ETH_LINK_SPEED_XXX of speeds to be
				used. RTE_ETH_LINK_SPEED_FIXED disables link
				autonegotiation, and a unique speed shall be
				set. Otherwise, the bitmap defines the set of
				speeds to be advertised. If the special value
				RTE_ETH_LINK_SPEED_AUTONEG (0) is used, all speeds
				supported are advertised. */
	struct rte_eth_rxmode rxmode; /**< Port Rx configuration. */
	struct rte_eth_txmode txmode; /**< Port Tx configuration. */
	uint32_t lpbk_mode; /**< Loopback operation mode. By default the value
			         is 0, meaning the loopback mode is disabled.
				 Read the datasheet of given Ethernet controller
				 for details. The possible values of this field
				 are defined in implementation of each driver. */
	struct {
		struct rte_eth_rss_conf rss_conf; /**< Port RSS configuration */
		/** Port VMDq+DCB configuration. */
		struct rte_eth_vmdq_dcb_conf vmdq_dcb_conf;
		/** Port DCB Rx configuration. */
		struct rte_eth_dcb_rx_conf dcb_rx_conf;
		/** Port VMDq Rx configuration. */
		struct rte_eth_vmdq_rx_conf vmdq_rx_conf;
	} rx_adv_conf; /**< Port Rx filtering configuration. */
	union {
		/** Port VMDq+DCB Tx configuration. */
		struct rte_eth_vmdq_dcb_tx_conf vmdq_dcb_tx_conf;
		/** Port DCB Tx configuration. */
		struct rte_eth_dcb_tx_conf dcb_tx_conf;
		/** Port VMDq Tx configuration. */
		struct rte_eth_vmdq_tx_conf vmdq_tx_conf;
	} tx_adv_conf; /**< Port Tx DCB configuration (union). */
	/** Currently,Priority Flow Control(PFC) are supported,if DCB with PFC
	    is needed,and the variable must be set RTE_ETH_DCB_PFC_SUPPORT. */
	uint32_t dcb_capability_en;
	struct rte_eth_intr_conf intr_conf; /**< Interrupt mode configuration. */
};

/**
 * Rx offload capabilities of a device.
 */
#define RTE_ETH_RX_OFFLOAD_VLAN_STRIP       RTE_BIT64(0)
#define RTE_ETH_RX_OFFLOAD_IPV4_CKSUM       RTE_BIT64(1)
#define RTE_ETH_RX_OFFLOAD_UDP_CKSUM        RTE_BIT64(2)
#define RTE_ETH_RX_OFFLOAD_TCP_CKSUM        RTE_BIT64(3)
#define RTE_ETH_RX_OFFLOAD_TCP_LRO          RTE_BIT64(4)
#define RTE_ETH_RX_OFFLOAD_QINQ_STRIP       RTE_BIT64(5)
#define RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM RTE_BIT64(6)
#define RTE_ETH_RX_OFFLOAD_MACSEC_STRIP     RTE_BIT64(7)
#define RTE_ETH_RX_OFFLOAD_VLAN_FILTER      RTE_BIT64(9)
#define RTE_ETH_RX_OFFLOAD_VLAN_EXTEND      RTE_BIT64(10)
#define RTE_ETH_RX_OFFLOAD_SCATTER          RTE_BIT64(13)
/**
 * Timestamp is set by the driver in RTE_MBUF_DYNFIELD_TIMESTAMP_NAME
 * and RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME is set in ol_flags.
 * The mbuf field and flag are registered when the offload is configured.
 */
#define RTE_ETH_RX_OFFLOAD_TIMESTAMP        RTE_BIT64(14)
#define RTE_ETH_RX_OFFLOAD_SECURITY         RTE_BIT64(15)
#define RTE_ETH_RX_OFFLOAD_KEEP_CRC         RTE_BIT64(16)
#define RTE_ETH_RX_OFFLOAD_SCTP_CKSUM       RTE_BIT64(17)
#define RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM  RTE_BIT64(18)
#define RTE_ETH_RX_OFFLOAD_RSS_HASH         RTE_BIT64(19)
#define RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT     RTE_BIT64(20)

#define RTE_ETH_RX_OFFLOAD_CHECKSUM (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM | \
				 RTE_ETH_RX_OFFLOAD_UDP_CKSUM | \
				 RTE_ETH_RX_OFFLOAD_TCP_CKSUM)
#define RTE_ETH_RX_OFFLOAD_VLAN (RTE_ETH_RX_OFFLOAD_VLAN_STRIP | \
			     RTE_ETH_RX_OFFLOAD_VLAN_FILTER | \
			     RTE_ETH_RX_OFFLOAD_VLAN_EXTEND | \
			     RTE_ETH_RX_OFFLOAD_QINQ_STRIP)

/*
 * If new Rx offload capabilities are defined, they also must be
 * mentioned in rte_rx_offload_names in rte_ethdev.c file.
 */

/**
 * Tx offload capabilities of a device.
 */
#define RTE_ETH_TX_OFFLOAD_VLAN_INSERT      RTE_BIT64(0)
#define RTE_ETH_TX_OFFLOAD_IPV4_CKSUM       RTE_BIT64(1)
#define RTE_ETH_TX_OFFLOAD_UDP_CKSUM        RTE_BIT64(2)
#define RTE_ETH_TX_OFFLOAD_TCP_CKSUM        RTE_BIT64(3)
#define RTE_ETH_TX_OFFLOAD_SCTP_CKSUM       RTE_BIT64(4)
#define RTE_ETH_TX_OFFLOAD_TCP_TSO          RTE_BIT64(5)
#define RTE_ETH_TX_OFFLOAD_UDP_TSO          RTE_BIT64(6)
#define RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM RTE_BIT64(7)  /**< Used for tunneling packet. */
#define RTE_ETH_TX_OFFLOAD_QINQ_INSERT      RTE_BIT64(8)
#define RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO    RTE_BIT64(9)  /**< Used for tunneling packet. */
#define RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO      RTE_BIT64(10) /**< Used for tunneling packet. */
#define RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO     RTE_BIT64(11) /**< Used for tunneling packet. */
#define RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO   RTE_BIT64(12) /**< Used for tunneling packet. */
#define RTE_ETH_TX_OFFLOAD_MACSEC_INSERT    RTE_BIT64(13)
/**
 * Multiple threads can invoke rte_eth_tx_burst() concurrently on the same
 * Tx queue without SW lock.
 */
#define RTE_ETH_TX_OFFLOAD_MT_LOCKFREE      RTE_BIT64(14)
/** Device supports multi segment send. */
#define RTE_ETH_TX_OFFLOAD_MULTI_SEGS       RTE_BIT64(15)
/**
 * Device supports optimization for fast release of mbufs.
 * When set application must guarantee that per-queue all mbufs comes from
 * the same mempool and has refcnt = 1.
 */
#define RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE   RTE_BIT64(16)
#define RTE_ETH_TX_OFFLOAD_SECURITY         RTE_BIT64(17)
/**
 * Device supports generic UDP tunneled packet TSO.
 * Application must set RTE_MBUF_F_TX_TUNNEL_UDP and other mbuf fields required
 * for tunnel TSO.
 */
#define RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO      RTE_BIT64(18)
/**
 * Device supports generic IP tunneled packet TSO.
 * Application must set RTE_MBUF_F_TX_TUNNEL_IP and other mbuf fields required
 * for tunnel TSO.
 */
#define RTE_ETH_TX_OFFLOAD_IP_TNL_TSO       RTE_BIT64(19)
/** Device supports outer UDP checksum */
#define RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM  RTE_BIT64(20)
/**
 * Device sends on time read from RTE_MBUF_DYNFIELD_TIMESTAMP_NAME
 * if RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME is set in ol_flags.
 * The mbuf field and flag are registered when the offload is configured.
 */
#define RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP RTE_BIT64(21)
/*
 * If new Tx offload capabilities are defined, they also must be
 * mentioned in rte_tx_offload_names in rte_ethdev.c file.
 */

/**@{@name Device capabilities
 * Non-offload capabilities reported in rte_eth_dev_info.dev_capa.
 */
/** Device supports Rx queue setup after device started. */
#define RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP RTE_BIT64(0)
/** Device supports Tx queue setup after device started. */
#define RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP RTE_BIT64(1)
/**
 * Device supports shared Rx queue among ports within Rx domain and
 * switch domain. Mbufs are consumed by shared Rx queue instead of
 * each queue. Multiple groups are supported by share_group of Rx
 * queue configuration. Shared Rx queue is identified by PMD using
 * share_qid of Rx queue configuration. Polling any port in the group
 * receive packets of all member ports, source port identified by
 * mbuf->port field.
 */
#define RTE_ETH_DEV_CAPA_RXQ_SHARE              RTE_BIT64(2)
/** Device supports keeping flow rules across restart. */
#define RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP         RTE_BIT64(3)
/** Device supports keeping shared flow objects across restart. */
#define RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP RTE_BIT64(4)
/**@}*/

/*
 * Fallback default preferred Rx/Tx port parameters.
 * These are used if an application requests default parameters
 * but the PMD does not provide preferred values.
 */
#define RTE_ETH_DEV_FALLBACK_RX_RINGSIZE 512
#define RTE_ETH_DEV_FALLBACK_TX_RINGSIZE 512
#define RTE_ETH_DEV_FALLBACK_RX_NBQUEUES 1
#define RTE_ETH_DEV_FALLBACK_TX_NBQUEUES 1

/**
 * Preferred Rx/Tx port parameters.
 * There are separate instances of this structure for transmission
 * and reception respectively.
 */
struct rte_eth_dev_portconf {
	uint16_t burst_size; /**< Device-preferred burst size */
	uint16_t ring_size; /**< Device-preferred size of queue rings */
	uint16_t nb_queues; /**< Device-preferred number of queues */
};

/**
 * Default values for switch domain ID when ethdev does not support switch
 * domain definitions.
 */
#define RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID	(UINT16_MAX)

/**
 * Ethernet device associated switch information
 */
struct rte_eth_switch_info {
	const char *name;	/**< switch name */
	uint16_t domain_id;	/**< switch domain ID */
	/**
	 * Mapping to the devices physical switch port as enumerated from the
	 * perspective of the embedded interconnect/switch. For SR-IOV enabled
	 * device this may correspond to the VF_ID of each virtual function,
	 * but each driver should explicitly define the mapping of switch
	 * port identifier to that physical interconnect/switch
	 */
	uint16_t port_id;
	/**
	 * Shared Rx queue sub-domain boundary. Only ports in same Rx domain
	 * and switch domain can share Rx queue. Valid only if device advertised
	 * RTE_ETH_DEV_CAPA_RXQ_SHARE capability.
	 */
	uint16_t rx_domain;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * Ethernet device Rx buffer segmentation capabilities.
 */
struct rte_eth_rxseg_capa {
	__extension__
	uint32_t multi_pools:1; /**< Supports receiving to multiple pools.*/
	uint32_t offset_allowed:1; /**< Supports buffer offsets. */
	uint32_t offset_align_log2:4; /**< Required offset alignment. */
	uint16_t max_nseg; /**< Maximum amount of segments to split. */
	uint16_t reserved; /**< Reserved field. */
};

/**
 * Ethernet device information
 */

/**
 * Ethernet device representor port type.
 */
enum rte_eth_representor_type {
	RTE_ETH_REPRESENTOR_NONE, /**< not a representor. */
	RTE_ETH_REPRESENTOR_VF,   /**< representor of Virtual Function. */
	RTE_ETH_REPRESENTOR_SF,   /**< representor of Sub Function. */
	RTE_ETH_REPRESENTOR_PF,   /**< representor of Physical Function. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this enumeration may change without prior notice.
 *
 * Ethernet device error handling mode.
 */
enum rte_eth_err_handle_mode {
	/** No error handling modes are supported. */
	RTE_ETH_ERROR_HANDLE_MODE_NONE,
	/** Passive error handling, after the PMD detects that a reset is required,
	 * the PMD reports @see RTE_ETH_EVENT_INTR_RESET event,
	 * and the application invokes @see rte_eth_dev_reset to recover the port.
	 */
	RTE_ETH_ERROR_HANDLE_MODE_PASSIVE,
	/** Proactive error handling, after the PMD detects that a reset is required,
	 * the PMD reports @see RTE_ETH_EVENT_ERR_RECOVERING event,
	 * do recovery internally, and finally reports the recovery result event
	 * (@see RTE_ETH_EVENT_RECOVERY_*).
	 */
	RTE_ETH_ERROR_HANDLE_MODE_PROACTIVE,
};

/**
 * A structure used to retrieve the contextual information of
 * an Ethernet device, such as the controlling driver of the
 * device, etc...
 */
struct rte_eth_dev_info {
	struct rte_device *device; /**< Generic device information */
	const char *driver_name; /**< Device Driver name. */
	unsigned int if_index; /**< Index to bound host interface, or 0 if none.
		Use if_indextoname() to translate into an interface name. */
	uint16_t min_mtu;	/**< Minimum MTU allowed */
	uint16_t max_mtu;	/**< Maximum MTU allowed */
	const uint32_t *dev_flags; /**< Device flags */
	uint32_t min_rx_bufsize; /**< Minimum size of Rx buffer. */
	uint32_t max_rx_pktlen; /**< Maximum configurable length of Rx pkt. */
	/** Maximum configurable size of LRO aggregated packet. */
	uint32_t max_lro_pkt_size;
	uint16_t max_rx_queues; /**< Maximum number of Rx queues. */
	uint16_t max_tx_queues; /**< Maximum number of Tx queues. */
	uint32_t max_mac_addrs; /**< Maximum number of MAC addresses. */
	/** Maximum number of hash MAC addresses for MTA and UTA. */
	uint32_t max_hash_mac_addrs;
	uint16_t max_vfs; /**< Maximum number of VFs. */
	uint16_t max_vmdq_pools; /**< Maximum number of VMDq pools. */
	struct rte_eth_rxseg_capa rx_seg_capa; /**< Segmentation capability.*/
	/** All Rx offload capabilities including all per-queue ones */
	uint64_t rx_offload_capa;
	/** All Tx offload capabilities including all per-queue ones */
	uint64_t tx_offload_capa;
	/** Device per-queue Rx offload capabilities. */
	uint64_t rx_queue_offload_capa;
	/** Device per-queue Tx offload capabilities. */
	uint64_t tx_queue_offload_capa;
	/** Device redirection table size, the total number of entries. */
	uint16_t reta_size;
	uint8_t hash_key_size; /**< Hash key size in bytes */
	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;
	struct rte_eth_rxconf default_rxconf; /**< Default Rx configuration */
	struct rte_eth_txconf default_txconf; /**< Default Tx configuration */
	uint16_t vmdq_queue_base; /**< First queue ID for VMDq pools. */
	uint16_t vmdq_queue_num;  /**< Queue number for VMDq pools. */
	uint16_t vmdq_pool_base;  /**< First ID of VMDq pools. */
	struct rte_eth_desc_lim rx_desc_lim;  /**< Rx descriptors limits */
	struct rte_eth_desc_lim tx_desc_lim;  /**< Tx descriptors limits */
	uint32_t speed_capa;  /**< Supported speeds bitmap (RTE_ETH_LINK_SPEED_). */
	/** Configured number of Rx/Tx queues */
	uint16_t nb_rx_queues; /**< Number of Rx queues. */
	uint16_t nb_tx_queues; /**< Number of Tx queues. */
	/**
	 * Maximum number of Rx mempools supported per Rx queue.
	 *
	 * Value greater than 0 means that the driver supports Rx queue
	 * mempools specification via rx_conf->rx_mempools.
	 */
	uint16_t max_rx_mempools;
	/** Rx parameter recommendations */
	struct rte_eth_dev_portconf default_rxportconf;
	/** Tx parameter recommendations */
	struct rte_eth_dev_portconf default_txportconf;
	/** Generic device capabilities (RTE_ETH_DEV_CAPA_). */
	uint64_t dev_capa;
	/**
	 * Switching information for ports on a device with a
	 * embedded managed interconnect/switch.
	 */
	struct rte_eth_switch_info switch_info;
	/** Supported error handling mode. */
	enum rte_eth_err_handle_mode err_handle_mode;

	uint64_t reserved_64s[2]; /**< Reserved for future fields */
	void *reserved_ptrs[2];   /**< Reserved for future fields */
};

/**@{@name Rx/Tx queue states */
#define RTE_ETH_QUEUE_STATE_STOPPED 0 /**< Queue stopped. */
#define RTE_ETH_QUEUE_STATE_STARTED 1 /**< Queue started. */
#define RTE_ETH_QUEUE_STATE_HAIRPIN 2 /**< Queue used for hairpin. */
/**@}*/

/**
 * Ethernet device Rx queue information structure.
 * Used to retrieve information about configured queue.
 */
struct rte_eth_rxq_info {
	struct rte_mempool *mp;     /**< mempool used by that queue. */
	struct rte_eth_rxconf conf; /**< queue config parameters. */
	uint8_t scattered_rx;       /**< scattered packets Rx supported. */
	uint8_t queue_state;        /**< one of RTE_ETH_QUEUE_STATE_*. */
	uint16_t nb_desc;           /**< configured number of RXDs. */
	uint16_t rx_buf_size;       /**< hardware receive buffer size. */
	/**
	 * Available Rx descriptors threshold defined as percentage
	 * of Rx queue size. If number of available descriptors is lower,
	 * the event RTE_ETH_EVENT_RX_AVAIL_THESH is generated.
	 * Value 0 means that the threshold monitoring is disabled.
	 */
	uint8_t avail_thresh;
} __rte_cache_min_aligned;

/**
 * Ethernet device Tx queue information structure.
 * Used to retrieve information about configured queue.
 */
struct rte_eth_txq_info {
	struct rte_eth_txconf conf; /**< queue config parameters. */
	uint16_t nb_desc;           /**< configured number of TXDs. */
	uint8_t queue_state;        /**< one of RTE_ETH_QUEUE_STATE_*. */
} __rte_cache_min_aligned;

/* Generic Burst mode flag definition, values can be ORed. */

/**
 * If the queues have different burst mode description, this bit will be set
 * by PMD, then the application can iterate to retrieve burst description for
 * all other queues.
 */
#define RTE_ETH_BURST_FLAG_PER_QUEUE RTE_BIT64(0)

/**
 * Ethernet device Rx/Tx queue packet burst mode information structure.
 * Used to retrieve information about packet burst mode setting.
 */
struct rte_eth_burst_mode {
	uint64_t flags; /**< The ORed values of RTE_ETH_BURST_FLAG_xxx */

#define RTE_ETH_BURST_MODE_INFO_SIZE 1024 /**< Maximum size for information */
	char info[RTE_ETH_BURST_MODE_INFO_SIZE]; /**< burst mode information */
};

/** Maximum name length for extended statistics counters */
#define RTE_ETH_XSTATS_NAME_SIZE 64

/**
 * An Ethernet device extended statistic structure
 *
 * This structure is used by rte_eth_xstats_get() to provide
 * statistics that are not provided in the generic *rte_eth_stats*
 * structure.
 * It maps a name ID, corresponding to an index in the array returned
 * by rte_eth_xstats_get_names(), to a statistic value.
 */
struct rte_eth_xstat {
	uint64_t id;        /**< The index in xstats name array. */
	uint64_t value;     /**< The statistic counter value. */
};

/**
 * A name element for extended statistics.
 *
 * An array of this structure is returned by rte_eth_xstats_get_names().
 * It lists the names of extended statistics for a PMD. The *rte_eth_xstat*
 * structure references these names by their array index.
 *
 * The xstats should follow a common naming scheme.
 * Some names are standardized in rte_stats_strings.
 * Examples:
 *     - rx_missed_errors
 *     - tx_q3_bytes
 *     - tx_size_128_to_255_packets
 */
struct rte_eth_xstat_name {
	char name[RTE_ETH_XSTATS_NAME_SIZE]; /**< The statistic name. */
};

#define RTE_ETH_DCB_NUM_TCS    8
#define RTE_ETH_MAX_VMDQ_POOL  64

/**
 * A structure used to get the information of queue and
 * TC mapping on both Tx and Rx paths.
 */
struct rte_eth_dcb_tc_queue_mapping {
	/** Rx queues assigned to tc per Pool */
	struct {
		uint16_t base;
		uint16_t nb_queue;
	} tc_rxq[RTE_ETH_MAX_VMDQ_POOL][RTE_ETH_DCB_NUM_TCS];
	/** Rx queues assigned to tc per Pool */
	struct {
		uint16_t base;
		uint16_t nb_queue;
	} tc_txq[RTE_ETH_MAX_VMDQ_POOL][RTE_ETH_DCB_NUM_TCS];
};

/**
 * A structure used to get the information of DCB.
 * It includes TC UP mapping and queue TC mapping.
 */
struct rte_eth_dcb_info {
	uint8_t nb_tcs;        /**< number of TCs */
	uint8_t prio_tc[RTE_ETH_DCB_NUM_USER_PRIORITIES]; /**< Priority to tc */
	uint8_t tc_bws[RTE_ETH_DCB_NUM_TCS]; /**< Tx BW percentage for each TC */
	/** Rx queues assigned to tc */
	struct rte_eth_dcb_tc_queue_mapping tc_queue;
};

/**
 * This enum indicates the possible Forward Error Correction (FEC) modes
 * of an ethdev port.
 */
enum rte_eth_fec_mode {
	RTE_ETH_FEC_NOFEC = 0,      /**< FEC is off */
	RTE_ETH_FEC_AUTO,	    /**< FEC autonegotiation modes */
	RTE_ETH_FEC_BASER,          /**< FEC using common algorithm */
	RTE_ETH_FEC_RS,             /**< FEC using RS algorithm */
	RTE_ETH_FEC_LLRS,           /**< FEC using LLRS algorithm */
};

/* Translate from FEC mode to FEC capa */
#define RTE_ETH_FEC_MODE_TO_CAPA(x) RTE_BIT32(x)

/* This macro indicates FEC capa mask */
#define RTE_ETH_FEC_MODE_CAPA_MASK(x) RTE_BIT32(RTE_ETH_FEC_ ## x)

/* A structure used to get capabilities per link speed */
struct rte_eth_fec_capa {
	uint32_t speed; /**< Link speed (see RTE_ETH_SPEED_NUM_*) */
	uint32_t capa;  /**< FEC capabilities bitmask */
};

#define RTE_ETH_ALL RTE_MAX_ETHPORTS

/* Macros to check for valid port */
#define RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, retval) do { \
	if (!rte_eth_dev_is_valid_port(port_id)) { \
		RTE_ETHDEV_LOG(ERR, "Invalid port_id=%u\n", port_id); \
		return retval; \
	} \
} while (0)

#define RTE_ETH_VALID_PORTID_OR_RET(port_id) do { \
	if (!rte_eth_dev_is_valid_port(port_id)) { \
		RTE_ETHDEV_LOG(ERR, "Invalid port_id=%u\n", port_id); \
		return; \
	} \
} while (0)

/**
 * Function type used for Rx packet processing packet callbacks.
 *
 * The callback function is called on Rx with a burst of packets that have
 * been received on the given port and queue.
 *
 * @param port_id
 *   The Ethernet port on which Rx is being performed.
 * @param queue
 *   The queue on the Ethernet port which is being used to receive the packets.
 * @param pkts
 *   The burst of packets that have just been received.
 * @param nb_pkts
 *   The number of packets in the burst pointed to by "pkts".
 * @param max_pkts
 *   The max number of packets that can be stored in the "pkts" array.
 * @param user_param
 *   The arbitrary user parameter passed in by the application when the callback
 *   was originally configured.
 * @return
 *   The number of packets returned to the user.
 */
typedef uint16_t (*rte_rx_callback_fn)(uint16_t port_id, uint16_t queue,
	struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
	void *user_param);

/**
 * Function type used for Tx packet processing packet callbacks.
 *
 * The callback function is called on Tx with a burst of packets immediately
 * before the packets are put onto the hardware queue for transmission.
 *
 * @param port_id
 *   The Ethernet port on which Tx is being performed.
 * @param queue
 *   The queue on the Ethernet port which is being used to transmit the packets.
 * @param pkts
 *   The burst of packets that are about to be transmitted.
 * @param nb_pkts
 *   The number of packets in the burst pointed to by "pkts".
 * @param user_param
 *   The arbitrary user parameter passed in by the application when the callback
 *   was originally configured.
 * @return
 *   The number of packets to be written to the NIC.
 */
typedef uint16_t (*rte_tx_callback_fn)(uint16_t port_id, uint16_t queue,
	struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param);

/**
 * Possible states of an ethdev port.
 */
enum rte_eth_dev_state {
	/** Device is unused before being probed. */
	RTE_ETH_DEV_UNUSED = 0,
	/** Device is attached when allocated in probing. */
	RTE_ETH_DEV_ATTACHED,
	/** Device is in removed state when plug-out is detected. */
	RTE_ETH_DEV_REMOVED,
};

struct rte_eth_dev_sriov {
	uint8_t active;               /**< SRIOV is active with 16, 32 or 64 pools */
	uint8_t nb_q_per_pool;        /**< Rx queue number per pool */
	uint16_t def_vmdq_idx;        /**< Default pool num used for PF */
	uint16_t def_pool_q_idx;      /**< Default pool queue start reg index */
};
#define RTE_ETH_DEV_SRIOV(dev)         ((dev)->data->sriov)

#define RTE_ETH_NAME_MAX_LEN RTE_DEV_NAME_MAX_LEN

#define RTE_ETH_DEV_NO_OWNER 0

#define RTE_ETH_MAX_OWNER_NAME_LEN 64

struct rte_eth_dev_owner {
	uint64_t id; /**< The owner unique identifier. */
	char name[RTE_ETH_MAX_OWNER_NAME_LEN]; /**< The owner name. */
};

/**@{@name Device flags
 * Flags internally saved in rte_eth_dev_data.dev_flags
 * and reported in rte_eth_dev_info.dev_flags.
 */
/** PMD supports thread-safe flow operations */
#define RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE  RTE_BIT32(0)
/** Device supports link state interrupt */
#define RTE_ETH_DEV_INTR_LSC              RTE_BIT32(1)
/** Device is a bonding member */
#define RTE_ETH_DEV_BONDING_MEMBER        RTE_BIT32(2)
#define RTE_ETH_DEV_BONDED_SLAVE RTE_DEPRECATED(RTE_ETH_DEV_BONDED_SLAVE) RTE_ETH_DEV_BONDING_MEMBER
/** Device supports device removal interrupt */
#define RTE_ETH_DEV_INTR_RMV              RTE_BIT32(3)
/** Device is port representor */
#define RTE_ETH_DEV_REPRESENTOR           RTE_BIT32(4)
/** Device does not support MAC change after started */
#define RTE_ETH_DEV_NOLIVE_MAC_ADDR       RTE_BIT32(5)
/**
 * Queue xstats filled automatically by ethdev layer.
 * PMDs filling the queue xstats themselves should not set this flag
 */
#define RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS RTE_BIT32(6)
/**@}*/

/**
 * Iterates over valid ethdev ports owned by a specific owner.
 *
 * @param port_id
 *   The ID of the next possible valid owned port.
 * @param	owner_id
 *  The owner identifier.
 *  RTE_ETH_DEV_NO_OWNER means iterate over all valid ownerless ports.
 * @return
 *   Next valid port ID owned by owner_id, RTE_MAX_ETHPORTS if there is none.
 */
uint64_t rte_eth_find_next_owned_by(uint16_t port_id,
		const uint64_t owner_id);

/**
 * Macro to iterate over all enabled ethdev ports owned by a specific owner.
 */
#define RTE_ETH_FOREACH_DEV_OWNED_BY(p, o) \
	for (p = rte_eth_find_next_owned_by(0, o); \
	     (unsigned int)p < (unsigned int)RTE_MAX_ETHPORTS; \
	     p = rte_eth_find_next_owned_by(p + 1, o))

/**
 * Iterates over valid ethdev ports.
 *
 * @param port_id
 *   The ID of the next possible valid port.
 * @return
 *   Next valid port ID, RTE_MAX_ETHPORTS if there is none.
 */
uint16_t rte_eth_find_next(uint16_t port_id);

/**
 * Macro to iterate over all enabled and ownerless ethdev ports.
 */
#define RTE_ETH_FOREACH_DEV(p) \
	RTE_ETH_FOREACH_DEV_OWNED_BY(p, RTE_ETH_DEV_NO_OWNER)

/**
 * Iterates over ethdev ports of a specified device.
 *
 * @param port_id_start
 *   The ID of the next possible valid port.
 * @param parent
 *   The generic device behind the ports to iterate.
 * @return
 *   Next port ID of the device, possibly port_id_start,
 *   RTE_MAX_ETHPORTS if there is none.
 */
uint16_t
rte_eth_find_next_of(uint16_t port_id_start,
		const struct rte_device *parent);

/**
 * Macro to iterate over all ethdev ports of a specified device.
 *
 * @param port_id
 *   The ID of the matching port being iterated.
 * @param parent
 *   The rte_device pointer matching the iterated ports.
 */
#define RTE_ETH_FOREACH_DEV_OF(port_id, parent) \
	for (port_id = rte_eth_find_next_of(0, parent); \
		port_id < RTE_MAX_ETHPORTS; \
		port_id = rte_eth_find_next_of(port_id + 1, parent))

/**
 * Iterates over sibling ethdev ports (i.e. sharing the same rte_device).
 *
 * @param port_id_start
 *   The ID of the next possible valid sibling port.
 * @param ref_port_id
 *   The ID of a reference port to compare rte_device with.
 * @return
 *   Next sibling port ID, possibly port_id_start or ref_port_id itself,
 *   RTE_MAX_ETHPORTS if there is none.
 */
uint16_t
rte_eth_find_next_sibling(uint16_t port_id_start, uint16_t ref_port_id);

/**
 * Macro to iterate over all ethdev ports sharing the same rte_device
 * as the specified port.
 * Note: the specified reference port is part of the loop iterations.
 *
 * @param port_id
 *   The ID of the matching port being iterated.
 * @param ref_port_id
 *   The ID of the port being compared.
 */
#define RTE_ETH_FOREACH_DEV_SIBLING(port_id, ref_port_id) \
	for (port_id = rte_eth_find_next_sibling(0, ref_port_id); \
		port_id < RTE_MAX_ETHPORTS; \
		port_id = rte_eth_find_next_sibling(port_id + 1, ref_port_id))

/**
 * Get a new unique owner identifier.
 * An owner identifier is used to owns Ethernet devices by only one DPDK entity
 * to avoid multiple management of device by different entities.
 *
 * @param	owner_id
 *   Owner identifier pointer.
 * @return
 *   Negative errno value on error, 0 on success.
 */
int rte_eth_dev_owner_new(uint64_t *owner_id);

/**
 * Set an Ethernet device owner.
 *
 * @param	port_id
 *  The identifier of the port to own.
 * @param	owner
 *  The owner pointer.
 * @return
 *  Negative errno value on error, 0 on success.
 */
int rte_eth_dev_owner_set(const uint16_t port_id,
		const struct rte_eth_dev_owner *owner);

/**
 * Unset Ethernet device owner to make the device ownerless.
 *
 * @param	port_id
 *  The identifier of port to make ownerless.
 * @param	owner_id
 *  The owner identifier.
 * @return
 *  0 on success, negative errno value on error.
 */
int rte_eth_dev_owner_unset(const uint16_t port_id,
		const uint64_t owner_id);

/**
 * Remove owner from all Ethernet devices owned by a specific owner.
 *
 * @param	owner_id
 *  The owner identifier.
 * @return
 *  0 on success, negative errno value on error.
 */
int rte_eth_dev_owner_delete(const uint64_t owner_id);

/**
 * Get the owner of an Ethernet device.
 *
 * @param	port_id
 *  The port identifier.
 * @param	owner
 *  The owner structure pointer to fill.
 * @return
 *  0 on success, negative errno value on error..
 */
int rte_eth_dev_owner_get(const uint16_t port_id,
		struct rte_eth_dev_owner *owner);

/**
 * Get the number of ports which are usable for the application.
 *
 * These devices must be iterated by using the macro
 * ``RTE_ETH_FOREACH_DEV`` or ``RTE_ETH_FOREACH_DEV_OWNED_BY``
 * to deal with non-contiguous ranges of devices.
 *
 * @return
 *   The count of available Ethernet devices.
 */
uint16_t rte_eth_dev_count_avail(void);

/**
 * Get the total number of ports which are allocated.
 *
 * Some devices may not be available for the application.
 *
 * @return
 *   The total count of Ethernet devices.
 */
uint16_t rte_eth_dev_count_total(void);

/**
 * Convert a numerical speed in Mbps to a bitmap flag that can be used in
 * the bitmap link_speeds of the struct rte_eth_conf
 *
 * @param speed
 *   Numerical speed value in Mbps
 * @param duplex
 *   RTE_ETH_LINK_[HALF/FULL]_DUPLEX (only for 10/100M speeds)
 * @return
 *   0 if the speed cannot be mapped
 */
uint32_t rte_eth_speed_bitflag(uint32_t speed, int duplex);

/**
 * Get RTE_ETH_RX_OFFLOAD_* flag name.
 *
 * @param offload
 *   Offload flag.
 * @return
 *   Offload name or 'UNKNOWN' if the flag cannot be recognised.
 */
const char *rte_eth_dev_rx_offload_name(uint64_t offload);

/**
 * Get RTE_ETH_TX_OFFLOAD_* flag name.
 *
 * @param offload
 *   Offload flag.
 * @return
 *   Offload name or 'UNKNOWN' if the flag cannot be recognised.
 */
const char *rte_eth_dev_tx_offload_name(uint64_t offload);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get RTE_ETH_DEV_CAPA_* flag name.
 *
 * @param capability
 *   Capability flag.
 * @return
 *   Capability name or 'UNKNOWN' if the flag cannot be recognized.
 */
__rte_experimental
const char *rte_eth_dev_capability_name(uint64_t capability);

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
 *        The Rx offload bitfield API is obsolete and will be deprecated.
 *        Applications should set the ignore_bitfield_offloads bit on *rxmode*
 *        structure and use offloads field to set per-port offloads instead.
 *     -  Any offloading set in eth_conf->[rt]xmode.offloads must be within
 *        the [rt]x_offload_capa returned from rte_eth_dev_info_get().
 *        Any type of device supported offloading set in the input argument
 *        eth_conf->[rt]xmode.offloads to rte_eth_dev_configure() is enabled
 *        on all queues and it can't be disabled in rte_eth_[rt]x_queue_setup()
 *     -  the Receive Side Scaling (RSS) configuration when using multiple Rx
 *        queues per port. Any RSS hash function set in eth_conf->rss_conf.rss_hf
 *        must be within the flow_type_rss_offloads provided by drivers via
 *        rte_eth_dev_info_get() API.
 *
 *   Embedding all configuration information in a single data structure
 *   is the more flexible method that allows the addition of new features
 *   without changing the syntax of the API.
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_queue,
		uint16_t nb_tx_queue, const struct rte_eth_conf *eth_conf);

/**
 * Check if an Ethernet device was physically removed.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   1 when the Ethernet device is removed, otherwise 0.
 */
int
rte_eth_dev_is_removed(uint16_t port_id);

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
 *   NULL value is allowed, in which case default Rx configuration
 *   will be used.
 *   The *rx_conf* structure contains an *rx_thresh* structure with the values
 *   of the Prefetch, Host, and Write-Back threshold registers of the receive
 *   ring.
 *   In addition it contains the hardware offloads features to activate using
 *   the RTE_ETH_RX_OFFLOAD_* flags.
 *   If an offloading set in rx_conf->offloads
 *   hasn't been set in the input argument eth_conf->rxmode.offloads
 *   to rte_eth_dev_configure(), it is a new added offloading, it must be
 *   per-queue type and it is enabled for the queue.
 *   No need to repeat any bit in rx_conf->offloads which has already been
 *   enabled in rte_eth_dev_configure() at port level. An offloading enabled
 *   at port level can't be disabled at queue level.
 *   The configuration structure also contains the pointer to the array
 *   of the receiving buffer segment descriptions, see rx_seg and rx_nseg
 *   fields, this extended configuration might be used by split offloads like
 *   RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT. If mb_pool is not NULL,
 *   the extended configuration fields must be set to NULL and zero.
 * @param mb_pool
 *   The pointer to the memory pool from which to allocate *rte_mbuf* network
 *   memory buffers to populate each descriptor of the receive ring. There are
 *   two options to provide Rx buffer configuration:
 *   - single pool:
 *     mb_pool is not NULL, rx_conf.rx_nseg is 0.
 *   - multiple segments description:
 *     mb_pool is NULL, rx_conf.rx_seg is not NULL, rx_conf.rx_nseg is not 0.
 *     Taken only if flag RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT is set in offloads.
 *
 * @return
 *   - 0: Success, receive queue correctly set up.
 *   - -EIO: if device is removed.
 *   - -ENODEV: if *port_id* is invalid.
 *   - -EINVAL: The memory pool pointer is null or the size of network buffers
 *      which can be allocated from this memory pool does not fit the various
 *      buffer sizes allowed by the device controller.
 *   - -ENOMEM: Unable to allocate the receive ring descriptors or to
 *      allocate network memory buffers from the memory pool when
 *      initializing receive descriptors.
 */
int rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Allocate and set up a hairpin receive queue for an Ethernet device.
 *
 * The function set up the selected queue to be used in hairpin.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rx_queue_id
 *   The index of the receive queue to set up.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param nb_rx_desc
 *   The number of receive descriptors to allocate for the receive ring.
 *   0 means the PMD will use default value.
 * @param conf
 *   The pointer to the hairpin configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOMEM) if unable to allocate the resources.
 */
__rte_experimental
int rte_eth_rx_hairpin_queue_setup
	(uint16_t port_id, uint16_t rx_queue_id, uint16_t nb_rx_desc,
	 const struct rte_eth_hairpin_conf *conf);

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
 *   NULL value is allowed, in which case default Tx configuration
 *   will be used.
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
 *   - The *offloads* member contains Tx offloads to be enabled.
 *     If an offloading set in tx_conf->offloads
 *     hasn't been set in the input argument eth_conf->txmode.offloads
 *     to rte_eth_dev_configure(), it is a new added offloading, it must be
 *     per-queue type and it is enabled for the queue.
 *     No need to repeat any bit in tx_conf->offloads which has already been
 *     enabled in rte_eth_dev_configure() at port level. An offloading enabled
 *     at port level can't be disabled at queue level.
 *
 *     Note that setting *tx_free_thresh* or *tx_rs_thresh* value to 0 forces
 *     the transmit function to use default values.
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -ENOMEM: Unable to allocate the transmit ring descriptors.
 */
int rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Allocate and set up a transmit hairpin queue for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tx_queue_id
 *   The index of the transmit queue to set up.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param nb_tx_desc
 *   The number of transmit descriptors to allocate for the transmit ring.
 *   0 to set default PMD value.
 * @param conf
 *   The hairpin configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOMEM) if unable to allocate the resources.
 */
__rte_experimental
int rte_eth_tx_hairpin_queue_setup
	(uint16_t port_id, uint16_t tx_queue_id, uint16_t nb_tx_desc,
	 const struct rte_eth_hairpin_conf *conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get all the hairpin peer Rx / Tx ports of the current port.
 * The caller should ensure that the array is large enough to save the ports
 * list.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param peer_ports
 *   Pointer to the array to store the peer ports list.
 * @param len
 *   Length of the array to store the port identifiers.
 * @param direction
 *   Current port to peer port direction
 *   positive - current used as Tx to get all peer Rx ports.
 *   zero - current used as Rx to get all peer Tx ports.
 *
 * @return
 *   - (0 or positive) actual peer ports number.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENODEV) if *port_id* invalid
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - Others detailed errors from PMDs.
 */
__rte_experimental
int rte_eth_hairpin_get_peer_ports(uint16_t port_id, uint16_t *peer_ports,
				   size_t len, uint32_t direction);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Bind all hairpin Tx queues of one port to the Rx queues of the peer port.
 * It is only allowed to call this function after all hairpin queues are
 * configured properly and the devices are in started state.
 *
 * @param tx_port
 *   The identifier of the Tx port.
 * @param rx_port
 *   The identifier of peer Rx port.
 *   RTE_MAX_ETHPORTS is allowed for the traversal of all devices.
 *   Rx port ID could have the same value as Tx port ID.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if Tx port ID is invalid.
 *   - (-EBUSY) if device is not in started state.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - Others detailed errors from PMDs.
 */
__rte_experimental
int rte_eth_hairpin_bind(uint16_t tx_port, uint16_t rx_port);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Unbind all hairpin Tx queues of one port from the Rx queues of the peer port.
 * This should be called before closing the Tx or Rx devices, if the bind
 * function is called before.
 * After unbinding the hairpin ports pair, it is allowed to bind them again.
 * Changing queues configuration should be after stopping the device(s).
 *
 * @param tx_port
 *   The identifier of the Tx port.
 * @param rx_port
 *   The identifier of peer Rx port.
 *   RTE_MAX_ETHPORTS is allowed for traversal of all devices.
 *   Rx port ID could have the same value as Tx port ID.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if Tx port ID is invalid.
 *   - (-EBUSY) if device is in stopped state.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - Others detailed errors from PMDs.
 */
__rte_experimental
int rte_eth_hairpin_unbind(uint16_t tx_port, uint16_t rx_port);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 *  Get the number of aggregated ports of the DPDK port (specified with port_id).
 *  It is used when multiple ports are aggregated into a single one.
 *
 *  For the regular physical port doesn't have aggregated ports,
 *  the number of aggregated ports is reported as 0.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (>=0) the number of aggregated port if success.
 */
__rte_experimental
int rte_eth_dev_count_aggr_ports(uint16_t port_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 *  Map a Tx queue with an aggregated port of the DPDK port (specified with port_id).
 *  When multiple ports are aggregated into a single one,
 *  it allows to choose which port to use for Tx via a queue.
 *
 *  The application should use rte_eth_dev_map_aggr_tx_affinity()
 *  after rte_eth_dev_configure(), rte_eth_tx_queue_setup(), and
 *  before rte_eth_dev_start().
 *
 * @param port_id
 *   The identifier of the port used in rte_eth_tx_burst().
 * @param tx_queue_id
 *   The index of the transmit queue used in rte_eth_tx_burst().
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param affinity
 *   The number of the aggregated port.
 *   Value 0 means no affinity and traffic could be routed to any aggregated port.
 *   The first aggregated port is number 1 and so on.
 *   The maximum number is given by rte_eth_dev_count_aggr_ports().
 *
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
__rte_experimental
int rte_eth_dev_map_aggr_tx_affinity(uint16_t port_id, uint16_t tx_queue_id,
				     uint8_t affinity);

/**
 * Return the NUMA socket to which an Ethernet device is connected
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @return
 *   - The NUMA socket ID which the Ethernet device is connected to.
 *   - -1 (which translates to SOCKET_ID_ANY) if the socket could not be
 *     determined. rte_errno is then set to:
 *     - EINVAL is the port_id is invalid,
 *     - 0 is the socket could not be determined,
 */
int rte_eth_dev_socket_id(uint16_t port_id);

/**
 * Check if port_id of device is attached
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @return
 *   - 0 if port is out of range or not attached
 *   - 1 if device is attached
 */
int rte_eth_dev_is_valid_port(uint16_t port_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Check if Rx queue is valid.
 * If the queue has been setup, it is considered valid.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue.
 * @return
 *   - -ENODEV: if port_id is invalid.
 *   - -EINVAL: if queue_id is out of range or queue has not been setup.
 *   - 0 if Rx queue is valid.
 */
__rte_experimental
int rte_eth_rx_queue_is_valid(uint16_t port_id, uint16_t queue_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Check if Tx queue is valid.
 * If the queue has been setup, it is considered valid.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue.
 * @return
 *   - -ENODEV: if port_id is invalid.
 *   - -EINVAL: if queue_id is out of range or queue has not been setup.
 *   - 0 if Tx queue is valid.
 */
__rte_experimental
int rte_eth_tx_queue_is_valid(uint16_t port_id, uint16_t queue_id);

/**
 * Start specified Rx queue of a port. It is used when rx_deferred_start
 * flag of the specified queue is true.
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the Rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the receive queue is started.
 *   - -ENODEV: if *port_id* is invalid.
 *   - -EINVAL: The queue_id out of range or belong to hairpin.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function not supported in PMD.
 */
int rte_eth_dev_rx_queue_start(uint16_t port_id, uint16_t rx_queue_id);

/**
 * Stop specified Rx queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the Rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the receive queue is stopped.
 *   - -ENODEV: if *port_id* is invalid.
 *   - -EINVAL: The queue_id out of range or belong to hairpin.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function not supported in PMD.
 */
int rte_eth_dev_rx_queue_stop(uint16_t port_id, uint16_t rx_queue_id);

/**
 * Start Tx for specified queue of a port. It is used when tx_deferred_start
 * flag of the specified queue is true.
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the Tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is started.
 *   - -ENODEV: if *port_id* is invalid.
 *   - -EINVAL: The queue_id out of range or belong to hairpin.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function not supported in PMD.
 */
int rte_eth_dev_tx_queue_start(uint16_t port_id, uint16_t tx_queue_id);

/**
 * Stop specified Tx queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the Tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is stopped.
 *   - -ENODEV: if *port_id* is invalid.
 *   - -EINVAL: The queue_id out of range or belong to hairpin.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function not supported in PMD.
 */
int rte_eth_dev_tx_queue_stop(uint16_t port_id, uint16_t tx_queue_id);

/**
 * Start an Ethernet device.
 *
 * The device start step is the last one and consists of setting the configured
 * offload features and in starting the transmit and the receive units of the
 * device.
 *
 * Device RTE_ETH_DEV_NOLIVE_MAC_ADDR flag causes MAC address to be set before
 * PMD port start callback function is invoked.
 *
 * On success, all basic functions exported by the Ethernet API (link status,
 * receive/transmit, and so on) can be invoked.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - 0: Success, Ethernet device started.
 *   - -EAGAIN: If start operation must be retried.
 *   - <0: Error code of the driver device start function.
 */
int rte_eth_dev_start(uint16_t port_id);

/**
 * Stop an Ethernet device. The device can be restarted with a call to
 * rte_eth_dev_start()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - 0: Success, Ethernet device stopped.
 *   - -EBUSY: If stopping the port is not allowed in current state.
 *   - <0: Error code of the driver device stop function.
 */
int rte_eth_dev_stop(uint16_t port_id);

/**
 * Link up an Ethernet device.
 *
 * Set device link up will re-enable the device Rx/Tx
 * functionality after it is previously set device linked down.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - 0: Success, Ethernet device linked up.
 *   - <0: Error code of the driver device link up function.
 */
int rte_eth_dev_set_link_up(uint16_t port_id);

/**
 * Link down an Ethernet device.
 * The device Rx/Tx functionality will be disabled if success,
 * and it can be re-enabled with a call to
 * rte_eth_dev_set_link_up()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
int rte_eth_dev_set_link_down(uint16_t port_id);

/**
 * Close a stopped Ethernet device. The device cannot be restarted!
 * The function frees all port resources.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - Zero if the port is closed successfully.
 *   - Negative if something went wrong.
 */
int rte_eth_dev_close(uint16_t port_id);

/**
 * Reset a Ethernet device and keep its port ID.
 *
 * When a port has to be reset passively, the DPDK application can invoke
 * this function. For example when a PF is reset, all its VFs should also
 * be reset. Normally a DPDK application can invoke this function when
 * RTE_ETH_EVENT_INTR_RESET event is detected, but can also use it to start
 * a port reset in other circumstances.
 *
 * When this function is called, it first stops the port and then calls the
 * PMD specific dev_uninit( ) and dev_init( ) to return the port to initial
 * state, in which no Tx and Rx queues are setup, as if the port has been
 * reset and not started. The port keeps the port ID it had before the
 * function call.
 *
 * After calling rte_eth_dev_reset( ), the application should use
 * rte_eth_dev_configure( ), rte_eth_rx_queue_setup( ),
 * rte_eth_tx_queue_setup( ), and rte_eth_dev_start( )
 * to reconfigure the device as appropriate.
 *
 * Note: To avoid unexpected behavior, the application should stop calling
 * Tx and Rx functions before calling rte_eth_dev_reset( ). For thread
 * safety, all these controlling functions should be called from the same
 * thread.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-ENOTSUP) if hardware doesn't support this function.
 *   - (-EPERM) if not ran from the primary process.
 *   - (-EIO) if re-initialisation failed or device is removed.
 *   - (-ENOMEM) if the reset failed due to OOM.
 *   - (-EAGAIN) if the reset temporarily failed and should be retried later.
 */
int rte_eth_dev_reset(uint16_t port_id);

/**
 * Enable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for promiscuous_enable() does not exist
 *     for the device.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_promiscuous_enable(uint16_t port_id);

/**
 * Disable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for promiscuous_disable() does not exist
 *     for the device.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_promiscuous_disable(uint16_t port_id);

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
int rte_eth_promiscuous_get(uint16_t port_id);

/**
 * Enable the receipt of any multicast frame by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for allmulticast_enable() does not exist
 *     for the device.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_allmulticast_enable(uint16_t port_id);

/**
 * Disable the receipt of all multicast frames by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for allmulticast_disable() does not exist
 *     for the device.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_allmulticast_disable(uint16_t port_id);

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
int rte_eth_allmulticast_get(uint16_t port_id);

/**
 * Retrieve the link status (up/down), the duplex mode (half/full),
 * the negotiation (auto/fixed), and if available, the speed (Mbps).
 *
 * It might need to wait up to 9 seconds.
 * @see rte_eth_link_get_nowait.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param link
 *   Link information written back.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if the function is not supported in PMD.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_link_get(uint16_t port_id, struct rte_eth_link *link);

/**
 * Retrieve the link status (up/down), the duplex mode (half/full),
 * the negotiation (auto/fixed), and if available, the speed (Mbps).
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param link
 *   Link information written back.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if the function is not supported in PMD.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link *link);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * The function converts a link_speed to a string. It handles all special
 * values like unknown or none speed.
 *
 * @param link_speed
 *   link_speed of rte_eth_link struct
 * @return
 *   Link speed in textual format. It's pointer to immutable memory.
 *   No free is required.
 */
__rte_experimental
const char *rte_eth_link_speed_to_str(uint32_t link_speed);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * The function converts a rte_eth_link struct representing a link status to
 * a string.
 *
 * @param str
 *   A pointer to a string to be filled with textual representation of
 *   device status. At least RTE_ETH_LINK_MAX_STR_LEN bytes should be allocated to
 *   store default link status text.
 * @param len
 *   Length of available memory at 'str' string.
 * @param eth_link
 *   Link status returned by rte_eth_link_get function
 * @return
 *   Number of bytes written to str array or -EINVAL if bad parameter.
 */
__rte_experimental
int rte_eth_link_to_str(char *str, size_t len,
			const struct rte_eth_link *eth_link);

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
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
int rte_eth_stats_get(uint16_t port_id, struct rte_eth_stats *stats);

/**
 * Reset the general I/O statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if device notified to reset stats.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (<0): Error code of the driver stats reset function.
 */
int rte_eth_stats_reset(uint16_t port_id);

/**
 * Retrieve names of extended statistics of an Ethernet device.
 *
 * There is an assumption that 'xstat_names' and 'xstats' arrays are matched
 * by array index:
 *  xstats_names[i].name => xstats[i].value
 *
 * And the array index is same with id field of 'struct rte_eth_xstat':
 *  xstats[i].id == i
 *
 * This assumption makes key-value pair matching less flexible but simpler.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param xstats_names
 *   An rte_eth_xstat_name array of at least *size* elements to
 *   be filled. If set to NULL, the function returns the required number
 *   of elements.
 * @param size
 *   The size of the xstats_names array (number of elements).
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - A negative value on error (invalid port ID).
 */
int rte_eth_xstats_get_names(uint16_t port_id,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size);

/**
 * Retrieve extended statistics of an Ethernet device.
 *
 * There is an assumption that 'xstat_names' and 'xstats' arrays are matched
 * by array index:
 *  xstats_names[i].name => xstats[i].value
 *
 * And the array index is same with id field of 'struct rte_eth_xstat':
 *  xstats[i].id == i
 *
 * This assumption makes key-value pair matching less flexible but simpler.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param xstats
 *   A pointer to a table of structure of type *rte_eth_xstat*
 *   to be filled with device statistics ids and values.
 *   This parameter can be set to NULL if and only if n is 0.
 * @param n
 *   The size of the xstats array (number of elements).
 *   If lower than the required number of elements, the function returns
 *   the required number of elements.
 *   If equal to zero, the xstats must be NULL, the function returns the
 *   required number of elements.
 * @return
 *   - A positive value lower or equal to n: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than n: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - A negative value on error (invalid port ID).
 */
int rte_eth_xstats_get(uint16_t port_id, struct rte_eth_xstat *xstats,
		unsigned int n);

/**
 * Retrieve names of extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param xstats_names
 *   Array to be filled in with names of requested device statistics.
 *   Must not be NULL if @p ids are specified (not NULL).
 * @param size
 *   Number of elements in @p xstats_names array (if not NULL) and in
 *   @p ids array (if not NULL). Must be 0 if both array pointers are NULL.
 * @param ids
 *   IDs array given by app to retrieve specific statistics. May be NULL to
 *   retrieve names of all available statistics or, if @p xstats_names is
 *   NULL as well, just the number of available statistics.
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than size: success. The given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - A negative value on error.
 */
int
rte_eth_xstats_get_names_by_id(uint16_t port_id,
	struct rte_eth_xstat_name *xstats_names, unsigned int size,
	uint64_t *ids);

/**
 * Retrieve extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ids
 *   IDs array given by app to retrieve specific statistics. May be NULL to
 *   retrieve all available statistics or, if @p values is NULL as well,
 *   just the number of available statistics.
 * @param values
 *   Array to be filled in with requested device statistics.
 *   Must not be NULL if ids are specified (not NULL).
 * @param size
 *   Number of elements in @p values array (if not NULL) and in @p ids
 *   array (if not NULL). Must be 0 if both array pointers are NULL.
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than size: success: The given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - A negative value on error.
 */
int rte_eth_xstats_get_by_id(uint16_t port_id, const uint64_t *ids,
			     uint64_t *values, unsigned int size);

/**
 * Gets the ID of a statistic from its name.
 *
 * This function searches for the statistics using string compares, and
 * as such should not be used on the fast-path. For fast-path retrieval of
 * specific statistics, store the ID as provided in *id* from this function,
 * and pass the ID to rte_eth_xstats_get()
 *
 * @param port_id The port to look up statistics from
 * @param xstat_name The name of the statistic to return
 * @param[out] id A pointer to an app-supplied uint64_t which should be
 *                set to the ID of the stat if the stat exists.
 * @return
 *    0 on success
 *    -ENODEV for invalid port_id,
 *    -EIO if device is removed,
 *    -EINVAL if the xstat_name doesn't exist in port_id
 *    -ENOMEM if bad parameter.
 */
int rte_eth_xstats_get_id_by_name(uint16_t port_id, const char *xstat_name,
		uint64_t *id);

/**
 * Reset extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if device notified to reset extended stats.
 *   - (-ENOTSUP) if pmd doesn't support both
 *     extended stats and basic stats reset.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (<0): Error code of the driver xstats reset function.
 */
int rte_eth_xstats_reset(uint16_t port_id);

/**
 *  Set a mapping for the specified transmit queue to the specified per-queue
 *  statistics counter.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tx_queue_id
 *   The index of the transmit queue for which a queue stats mapping is required.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param stat_idx
 *   The per-queue packet statistics functionality number that the transmit
 *   queue is to be assigned.
 *   The value must be in the range [0, RTE_ETHDEV_QUEUE_STAT_CNTRS - 1].
 *   Max RTE_ETHDEV_QUEUE_STAT_CNTRS being 256.
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
int rte_eth_dev_set_tx_queue_stats_mapping(uint16_t port_id,
		uint16_t tx_queue_id, uint8_t stat_idx);

/**
 *  Set a mapping for the specified receive queue to the specified per-queue
 *  statistics counter.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rx_queue_id
 *   The index of the receive queue for which a queue stats mapping is required.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param stat_idx
 *   The per-queue packet statistics functionality number that the receive
 *   queue is to be assigned.
 *   The value must be in the range [0, RTE_ETHDEV_QUEUE_STAT_CNTRS - 1].
 *   Max RTE_ETHDEV_QUEUE_STAT_CNTRS being 256.
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
int rte_eth_dev_set_rx_queue_stats_mapping(uint16_t port_id,
					   uint16_t rx_queue_id,
					   uint8_t stat_idx);

/**
 * Retrieve the Ethernet address of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   A pointer to a structure of type *ether_addr* to be filled with
 *   the Ethernet address of the Ethernet device.
 * @return
 *   - (0) if successful
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr *mac_addr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve the Ethernet addresses of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ma
 *   A pointer to an array of structures of type *ether_addr* to be filled with
 *   the Ethernet addresses of the Ethernet device.
 * @param num
 *   Number of elements in the @p ma array.
 *   Note that  rte_eth_dev_info::max_mac_addrs can be used to retrieve
 *   max number of Ethernet addresses for given port.
 * @return
 *   - number of retrieved addresses if successful
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_macaddrs_get(uint16_t port_id, struct rte_ether_addr *ma,
	unsigned int num);

/**
 * Retrieve the contextual information of an Ethernet device.
 *
 * This function returns the Ethernet device information based
 * on the values stored internally in the device specific data.
 * For example: number of queues, descriptor limits, device
 * capabilities and offload flags.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dev_info
 *   A pointer to a structure of type *rte_eth_dev_info* to be filled with
 *   the contextual information of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for dev_infos_get() does not exist for the device.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info *dev_info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the configuration of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dev_conf
 *   Location for Ethernet device configuration to be filled in.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_dev_conf_get(uint16_t port_id, struct rte_eth_conf *dev_conf);

/**
 * Retrieve the firmware version of a device.
 *
 * @param port_id
 *   The port identifier of the device.
 * @param fw_version
 *   A pointer to a string array storing the firmware version of a device,
 *   the string includes terminating null. This pointer is allocated by caller.
 * @param fw_size
 *   The size of the string array pointed by fw_version, which should be
 *   large enough to store firmware version of the device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if operation is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if bad parameter.
 *   - (>0) if *fw_size* is not enough to store firmware version, return
 *          the size of the non truncated string.
 */
int rte_eth_dev_fw_version_get(uint16_t port_id,
			       char *fw_version, size_t fw_size);

/**
 * Retrieve the supported packet types of an Ethernet device.
 *
 * When a packet type is announced as supported, it *must* be recognized by
 * the PMD. For instance, if RTE_PTYPE_L2_ETHER, RTE_PTYPE_L2_ETHER_VLAN
 * and RTE_PTYPE_L3_IPV4 are announced, the PMD must return the following
 * packet types for these packets:
 * - Ether/IPv4              -> RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
 * - Ether/VLAN/IPv4         -> RTE_PTYPE_L2_ETHER_VLAN | RTE_PTYPE_L3_IPV4
 * - Ether/[anything else]   -> RTE_PTYPE_L2_ETHER
 * - Ether/VLAN/[anything else] -> RTE_PTYPE_L2_ETHER_VLAN
 *
 * When a packet is received by a PMD, the most precise type must be
 * returned among the ones supported. However a PMD is allowed to set
 * packet type that is not in the supported list, at the condition that it
 * is more precise. Therefore, a PMD announcing no supported packet types
 * can still set a matching packet type in a received packet.
 *
 * @note
 *   Better to invoke this API after the device is already started or Rx burst
 *   function is decided, to obtain correct supported ptypes.
 * @note
 *   if a given PMD does not report what ptypes it supports, then the supported
 *   ptype count is reported as 0.
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ptype_mask
 *   A hint of what kind of packet type which the caller is interested in.
 * @param ptypes
 *   An array pointer to store adequate packet types, allocated by caller.
 * @param num
 *  Size of the array pointed by param ptypes.
 * @return
 *   - (>=0) Number of supported ptypes. If the number of types exceeds num,
 *           only num entries will be filled into the ptypes array, but the full
 *           count of supported ptypes will be returned.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_get_supported_ptypes(uint16_t port_id, uint32_t ptype_mask,
				     uint32_t *ptypes, int num);
/**
 * Inform Ethernet device about reduced range of packet types to handle.
 *
 * Application can use this function to set only specific ptypes that it's
 * interested. This information can be used by the PMD to optimize Rx path.
 *
 * The function accepts an array `set_ptypes` allocated by the caller to
 * store the packet types set by the driver, the last element of the array
 * is set to RTE_PTYPE_UNKNOWN. The size of the `set_ptype` array should be
 * `rte_eth_dev_get_supported_ptypes() + 1` else it might only be filled
 * partially.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ptype_mask
 *   The ptype family that application is interested in should be bitwise OR of
 *   RTE_PTYPE_*_MASK or 0.
 * @param set_ptypes
 *   An array pointer to store set packet types, allocated by caller. The
 *   function marks the end of array with RTE_PTYPE_UNKNOWN.
 * @param num
 *   Size of the array pointed by param ptypes.
 *   Should be rte_eth_dev_get_supported_ptypes() + 1 to accommodate the
 *   set ptypes.
 * @return
 *   - (0) if Success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if *ptype_mask* is invalid (or) set_ptypes is NULL and
 *     num > 0.
 */
int rte_eth_dev_set_ptypes(uint16_t port_id, uint32_t ptype_mask,
			   uint32_t *set_ptypes, unsigned int num);

/**
 * Retrieve the MTU of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mtu
 *   A pointer to a uint16_t where the retrieved MTU is to be stored.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_get_mtu(uint16_t port_id, uint16_t *mtu);

/**
 * Change the MTU of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mtu
 *   A uint16_t for the MTU to be applied.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if operation is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if *mtu* invalid, validation of mtu can occur within
 *     rte_eth_dev_set_mtu if dev_infos_get is supported by the device or
 *     when the mtu is set using dev->dev_ops->mtu_set.
 *   - (-EBUSY) if operation is not allowed when the port is running
 */
int rte_eth_dev_set_mtu(uint16_t port_id, uint16_t mtu);

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
 *   - (-ENOTSUP) if hardware-assisted VLAN filtering not configured.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOSYS) if VLAN filtering on *port_id* disabled.
 *   - (-EINVAL) if *vlan_id* > 4095.
 */
int rte_eth_dev_vlan_filter(uint16_t port_id, uint16_t vlan_id, int on);

/**
 * Enable/Disable hardware VLAN Strip by a Rx queue of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rx_queue_id
 *   The index of the receive queue for which a queue stats mapping is required.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param on
 *   If 1, Enable VLAN Stripping of the receive queue of the Ethernet port.
 *   If 0, Disable VLAN Stripping of the receive queue of the Ethernet port.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware-assisted VLAN stripping not configured.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if *rx_queue_id* invalid.
 */
int rte_eth_dev_set_vlan_strip_on_queue(uint16_t port_id, uint16_t rx_queue_id,
		int on);

/**
 * Set the Outer VLAN Ether Type by an Ethernet device, it can be inserted to
 * the VLAN header.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vlan_type
 *   The VLAN type.
 * @param tag_type
 *   The Tag Protocol ID
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware-assisted VLAN TPID setup is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_set_vlan_ether_type(uint16_t port_id,
				    enum rte_vlan_type vlan_type,
				    uint16_t tag_type);

/**
 * Set VLAN offload configuration on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param offload_mask
 *   The VLAN Offload bit mask can be mixed use with "OR"
 *       RTE_ETH_VLAN_STRIP_OFFLOAD
 *       RTE_ETH_VLAN_FILTER_OFFLOAD
 *       RTE_ETH_VLAN_EXTEND_OFFLOAD
 *       RTE_ETH_QINQ_STRIP_OFFLOAD
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware-assisted VLAN filtering not configured.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_set_vlan_offload(uint16_t port_id, int offload_mask);

/**
 * Read VLAN Offload configuration from an Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (>0) if successful. Bit mask to indicate
 *       RTE_ETH_VLAN_STRIP_OFFLOAD
 *       RTE_ETH_VLAN_FILTER_OFFLOAD
 *       RTE_ETH_VLAN_EXTEND_OFFLOAD
 *       RTE_ETH_QINQ_STRIP_OFFLOAD
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_get_vlan_offload(uint16_t port_id);

/**
 * Set port based Tx VLAN insertion on or off.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param pvid
 *  Port based Tx VLAN identifier together with user priority.
 * @param on
 *  Turn on or off the port based Tx VLAN insertion.
 *
 * @return
 *   - (0) if successful.
 *   - negative if failed.
 */
int rte_eth_dev_set_vlan_pvid(uint16_t port_id, uint16_t pvid, int on);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set Rx queue available descriptors threshold.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The index of the receive queue.
 * @param avail_thresh
 *  The available descriptors threshold is percentage of Rx queue size
 *  which describes the availability of Rx queue for hardware.
 *  If the Rx queue availability is below it,
 *  the event RTE_ETH_EVENT_RX_AVAIL_THRESH is triggered.
 *  [1-99] to set a new available descriptors threshold.
 *  0 to disable threshold monitoring.
 *
 * @return
 *   - 0 if successful.
 *   - (-ENODEV) if @p port_id is invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) if available Rx descriptors threshold is not supported.
 *   - (-EIO) if device is removed.
 */
__rte_experimental
int rte_eth_rx_avail_thresh_set(uint16_t port_id, uint16_t queue_id,
			       uint8_t avail_thresh);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Find Rx queue with RTE_ETH_EVENT_RX_AVAIL_THRESH event pending.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param[inout] queue_id
 *  On input starting Rx queue index to search from.
 *  If the queue_id is bigger than maximum queue ID of the port,
 *  search is started from 0. So that application can keep calling
 *  this function to handle all pending events with a simple increment
 *  of queue_id on the next call.
 *  On output if return value is 1, Rx queue index with the event pending.
 * @param[out] avail_thresh
 *  Location for available descriptors threshold of the found Rx queue.
 *
 * @return
 *   - 1 if an Rx queue with pending event is found.
 *   - 0 if no Rx queue with pending event is found.
 *   - (-ENODEV) if @p port_id is invalid.
 *   - (-EINVAL) if bad parameter (e.g. @p queue_id is NULL).
 *   - (-ENOTSUP) if operation is not supported.
 *   - (-EIO) if device is removed.
 */
__rte_experimental
int rte_eth_rx_avail_thresh_query(uint16_t port_id, uint16_t *queue_id,
				 uint8_t *avail_thresh);

typedef void (*buffer_tx_error_fn)(struct rte_mbuf **unsent, uint16_t count,
		void *userdata);

/**
 * Structure used to buffer packets for future Tx
 * Used by APIs rte_eth_tx_buffer and rte_eth_tx_buffer_flush
 */
struct rte_eth_dev_tx_buffer {
	buffer_tx_error_fn error_callback;
	void *error_userdata;
	uint16_t size;           /**< Size of buffer for buffered Tx */
	uint16_t length;         /**< Number of packets in the array */
	/** Pending packets to be sent on explicit flush or when full */
	struct rte_mbuf *pkts[];
};

/**
 * Calculate the size of the Tx buffer.
 *
 * @param sz
 *   Number of stored packets.
 */
#define RTE_ETH_TX_BUFFER_SIZE(sz) \
	(sizeof(struct rte_eth_dev_tx_buffer) + (sz) * sizeof(struct rte_mbuf *))

/**
 * Initialize default values for buffered transmitting
 *
 * @param buffer
 *   Tx buffer to be initialized.
 * @param size
 *   Buffer size
 * @return
 *   0 if no error
 */
int
rte_eth_tx_buffer_init(struct rte_eth_dev_tx_buffer *buffer, uint16_t size);

/**
 * Configure a callback for buffered packets which cannot be sent
 *
 * Register a specific callback to be called when an attempt is made to send
 * all packets buffered on an Ethernet port, but not all packets can
 * successfully be sent. The callback registered here will be called only
 * from calls to rte_eth_tx_buffer() and rte_eth_tx_buffer_flush() APIs.
 * The default callback configured for each queue by default just frees the
 * packets back to the calling mempool. If additional behaviour is required,
 * for example, to count dropped packets, or to retry transmission of packets
 * which cannot be sent, this function should be used to register a suitable
 * callback function to implement the desired behaviour.
 * The example callback "rte_eth_count_unsent_packet_callback()" is also
 * provided as reference.
 *
 * @param buffer
 *   The port identifier of the Ethernet device.
 * @param callback
 *   The function to be used as the callback.
 * @param userdata
 *   Arbitrary parameter to be passed to the callback function
 * @return
 *   0 on success, or -EINVAL if bad parameter
 */
int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn callback, void *userdata);

/**
 * Callback function for silently dropping unsent buffered packets.
 *
 * This function can be passed to rte_eth_tx_buffer_set_err_callback() to
 * adjust the default behavior when buffered packets cannot be sent. This
 * function drops any unsent packets silently and is used by Tx buffered
 * operations as default behavior.
 *
 * NOTE: this function should not be called directly, instead it should be used
 *       as a callback for packet buffering.
 *
 * NOTE: when configuring this function as a callback with
 *       rte_eth_tx_buffer_set_err_callback(), the final, userdata parameter
 *       should point to an uint64_t value.
 *
 * @param pkts
 *   The previously buffered packets which could not be sent
 * @param unsent
 *   The number of unsent packets in the pkts array
 * @param userdata
 *   Not used
 */
void
rte_eth_tx_buffer_drop_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata);

/**
 * Callback function for tracking unsent buffered packets.
 *
 * This function can be passed to rte_eth_tx_buffer_set_err_callback() to
 * adjust the default behavior when buffered packets cannot be sent. This
 * function drops any unsent packets, but also updates a user-supplied counter
 * to track the overall number of packets dropped. The counter should be an
 * uint64_t variable.
 *
 * NOTE: this function should not be called directly, instead it should be used
 *       as a callback for packet buffering.
 *
 * NOTE: when configuring this function as a callback with
 *       rte_eth_tx_buffer_set_err_callback(), the final, userdata parameter
 *       should point to an uint64_t value.
 *
 * @param pkts
 *   The previously buffered packets which could not be sent
 * @param unsent
 *   The number of unsent packets in the pkts array
 * @param userdata
 *   Pointer to an uint64_t value, which will be incremented by unsent
 */
void
rte_eth_tx_buffer_count_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata);

/**
 * Request the driver to free mbufs currently cached by the driver. The
 * driver will only free the mbuf if it is no longer in use. It is the
 * application's responsibility to ensure rte_eth_tx_buffer_flush(..) is
 * called if needed.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param free_cnt
 *   Maximum number of packets to free. Use 0 to indicate all possible packets
 *   should be freed. Note that a packet may be using multiple mbufs.
 * @return
 *   Failure: < 0
 *     -ENODEV: Invalid interface
 *     -EIO: device is removed
 *     -ENOTSUP: Driver does not support function
 *   Success: >= 0
 *     0-n: Number of packets freed. More packets may still remain in ring that
 *     are in use.
 */
int
rte_eth_tx_done_cleanup(uint16_t port_id, uint16_t queue_id, uint32_t free_cnt);

/**
 * Subtypes for MACsec offload event (@ref RTE_ETH_EVENT_MACSEC)
 * raised by Ethernet device.
 */
enum rte_eth_event_macsec_subtype {
	/** Notifies unknown MACsec subevent. */
	RTE_ETH_SUBEVENT_MACSEC_UNKNOWN,
	/**
	 * Subevent of RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR sectag validation events
	 *	Validation check: SecTag.TCI.V = 1
	 */
	RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1,
	/**
	 * Subevent of RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR sectag validation events
	 *	Validation check: SecTag.TCI.E = 0 && SecTag.TCI.C = 1
	 */
	RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1,
	/**
	 * Subevent of RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR sectag validation events
	 *	Validation check: SecTag.SL >= 'd48
	 */
	RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48,
	/**
	 * Subevent of RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR sectag validation events
	 *	Validation check: SecTag.TCI.ES = 1 && SecTag.TCI.SC = 1
	 */
	RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1,
	/**
	 * Subevent of RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR sectag validation events
	 *	Validation check: SecTag.TCI.SC = 1 && SecTag.TCI.SCB = 1
	 */
	RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1,
};

/**
 * Event types for MACsec offload event (@ref RTE_ETH_EVENT_MACSEC)
 * raised by eth device.
 */
enum rte_eth_event_macsec_type {
	/** Notifies unknown MACsec event. */
	RTE_ETH_EVENT_MACSEC_UNKNOWN,
	/** Notifies Sectag validation failure events. */
	RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR,
	/** Notifies Rx SA hard expiry events. */
	RTE_ETH_EVENT_MACSEC_RX_SA_PN_HARD_EXP,
	/** Notifies Rx SA soft expiry events. */
	RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP,
	/** Notifies Tx SA hard expiry events. */
	RTE_ETH_EVENT_MACSEC_TX_SA_PN_HARD_EXP,
	/** Notifies Tx SA soft events. */
	RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP,
	/** Notifies Invalid SA event. */
	RTE_ETH_EVENT_MACSEC_SA_NOT_VALID,
};

/**
 * Descriptor for @ref RTE_ETH_EVENT_MACSEC event.
 * Used by ethdev to send extra information of the MACsec offload event.
 */
struct rte_eth_event_macsec_desc {
	/** Type of RTE_ETH_EVENT_MACSEC_* event. */
	enum rte_eth_event_macsec_type type;
	/** Type of RTE_ETH_SUBEVENT_MACSEC_* subevent. */
	enum rte_eth_event_macsec_subtype subtype;
	/**
	 * Event specific metadata.
	 *
	 * For the following events, *userdata* registered
	 * with the *rte_security_session* would be returned
	 * as metadata.
	 *
	 * @see struct rte_security_session_conf
	 */
	uint64_t metadata;
};

/**
 * Subtypes for IPsec offload event(@ref RTE_ETH_EVENT_IPSEC) raised by
 * eth device.
 */
enum rte_eth_event_ipsec_subtype {
	/** Unknown event type */
	RTE_ETH_EVENT_IPSEC_UNKNOWN = 0,
	/** Sequence number overflow */
	RTE_ETH_EVENT_IPSEC_ESN_OVERFLOW,
	/** Soft time expiry of SA */
	RTE_ETH_EVENT_IPSEC_SA_TIME_EXPIRY,
	/**
	 * Soft byte expiry of SA determined by
	 * @ref rte_security_ipsec_lifetime::bytes_soft_limit
	 */
	RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY,
	/**
	 * Soft packet expiry of SA determined by
	 * @ref rte_security_ipsec_lifetime::packets_soft_limit
	 */
	RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY,
	/**
	 * Hard byte expiry of SA determined by
	 * @ref rte_security_ipsec_lifetime::bytes_hard_limit
	 */
	RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY,
	/**
	 * Hard packet expiry of SA determined by
	 * @ref rte_security_ipsec_lifetime::packets_hard_limit
	 */
	RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY,
	/** Max value of this enum */
	RTE_ETH_EVENT_IPSEC_MAX
};

/**
 * Descriptor for @ref RTE_ETH_EVENT_IPSEC event. Used by eth dev to send extra
 * information of the IPsec offload event.
 */
struct rte_eth_event_ipsec_desc {
	/** Type of RTE_ETH_EVENT_IPSEC_* event */
	enum rte_eth_event_ipsec_subtype subtype;
	/**
	 * Event specific metadata.
	 *
	 * For the following events, *userdata* registered
	 * with the *rte_security_session* would be returned
	 * as metadata,
	 *
	 * - @ref RTE_ETH_EVENT_IPSEC_ESN_OVERFLOW
	 * - @ref RTE_ETH_EVENT_IPSEC_SA_TIME_EXPIRY
	 * - @ref RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY
	 * - @ref RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY
	 * - @ref RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY
	 * - @ref RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY
	 *
	 * @see struct rte_security_session_conf
	 *
	 */
	uint64_t metadata;
};

/**
 * The eth device event type for interrupt, and maybe others in the future.
 */
enum rte_eth_event_type {
	RTE_ETH_EVENT_UNKNOWN,  /**< unknown event type */
	RTE_ETH_EVENT_INTR_LSC, /**< lsc interrupt event */
	/** queue state event (enabled/disabled) */
	RTE_ETH_EVENT_QUEUE_STATE,
	/** reset interrupt event, sent to VF on PF reset */
	RTE_ETH_EVENT_INTR_RESET,
	RTE_ETH_EVENT_VF_MBOX,  /**< message from the VF received by PF */
	RTE_ETH_EVENT_MACSEC,   /**< MACsec offload related event */
	RTE_ETH_EVENT_INTR_RMV, /**< device removal event */
	RTE_ETH_EVENT_NEW,      /**< port is probed */
	RTE_ETH_EVENT_DESTROY,  /**< port is released */
	RTE_ETH_EVENT_IPSEC,    /**< IPsec offload related event */
	RTE_ETH_EVENT_FLOW_AGED,/**< New aged-out flows is detected */
	/**
	 * Number of available Rx descriptors is smaller than the threshold.
	 * @see rte_eth_rx_avail_thresh_set()
	 */
	RTE_ETH_EVENT_RX_AVAIL_THRESH,
	/** Port recovering from a hardware or firmware error.
	 * If PMD supports proactive error recovery,
	 * it should trigger this event to notify application
	 * that it detected an error and the recovery is being started.
	 * Upon receiving the event, the application should not invoke any control path API
	 * (such as rte_eth_dev_configure/rte_eth_dev_stop...) until receiving
	 * RTE_ETH_EVENT_RECOVERY_SUCCESS or RTE_ETH_EVENT_RECOVERY_FAILED event.
	 * The PMD will set the data path pointers to dummy functions,
	 * and re-set the data path pointers to non-dummy functions
	 * before reporting RTE_ETH_EVENT_RECOVERY_SUCCESS event.
	 * It means that the application cannot send or receive any packets
	 * during this period.
	 * @note Before the PMD reports the recovery result,
	 * the PMD may report the RTE_ETH_EVENT_ERR_RECOVERING event again,
	 * because a larger error may occur during the recovery.
	 */
	RTE_ETH_EVENT_ERR_RECOVERING,
	/** Port recovers successfully from the error.
	 * The PMD already re-configured the port,
	 * and the effect is the same as a restart operation.
	 * a) The following operation will be retained: (alphabetically)
	 *    - DCB configuration
	 *    - FEC configuration
	 *    - Flow control configuration
	 *    - LRO configuration
	 *    - LSC configuration
	 *    - MTU
	 *    - MAC address (default and those supplied by MAC address array)
	 *    - Promiscuous and allmulticast mode
	 *    - PTP configuration
	 *    - Queue (Rx/Tx) settings
	 *    - Queue statistics mappings
	 *    - RSS configuration by rte_eth_dev_rss_xxx() family
	 *    - Rx checksum configuration
	 *    - Rx interrupt settings
	 *    - Traffic management configuration
	 *    - VLAN configuration (including filtering, tpid, strip, pvid)
	 *    - VMDq configuration
	 * b) The following configuration maybe retained
	 *    or not depending on the device capabilities:
	 *    - flow rules
	 *      (@see RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP)
	 *    - shared flow objects
	 *      (@see RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP)
	 * c) Any other configuration will not be stored
	 *    and will need to be re-configured.
	 */
	RTE_ETH_EVENT_RECOVERY_SUCCESS,
	/** Port recovery failed.
	 * It means that the port should not be usable anymore.
	 * The application should close the port.
	 */
	RTE_ETH_EVENT_RECOVERY_FAILED,
	RTE_ETH_EVENT_MAX       /**< max value of this enum */
};

/** User application callback to be registered for interrupts. */
typedef int (*rte_eth_dev_cb_fn)(uint16_t port_id,
		enum rte_eth_event_type event, void *cb_arg, void *ret_param);

/**
 * Register a callback function for port event.
 *
 * @param port_id
 *  Port ID.
 *  RTE_ETH_ALL means register the event for all port ids.
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
int rte_eth_dev_callback_register(uint16_t port_id,
			enum rte_eth_event_type event,
		rte_eth_dev_cb_fn cb_fn, void *cb_arg);

/**
 * Unregister a callback function for port event.
 *
 * @param port_id
 *  Port ID.
 *  RTE_ETH_ALL means unregister the event for all port ids.
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
int rte_eth_dev_callback_unregister(uint16_t port_id,
			enum rte_eth_event_type event,
		rte_eth_dev_cb_fn cb_fn, void *cb_arg);

/**
 * When there is no Rx packet coming in Rx Queue for a long time, we can
 * sleep lcore related to Rx Queue for power saving, and enable Rx interrupt
 * to be triggered when Rx packet arrives.
 *
 * The rte_eth_dev_rx_intr_enable() function enables Rx queue
 * interrupt on specific Rx queue of a port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue from which to retrieve input packets.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support
 *     that operation.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_rx_intr_enable(uint16_t port_id, uint16_t queue_id);

/**
 * When lcore wakes up from Rx interrupt indicating packet coming, disable Rx
 * interrupt and returns to polling mode.
 *
 * The rte_eth_dev_rx_intr_disable() function disables Rx queue
 * interrupt on specific Rx queue of a port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue from which to retrieve input packets.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support
 *     that operation.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_rx_intr_disable(uint16_t port_id, uint16_t queue_id);

/**
 * Rx Interrupt control per port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param epfd
 *   Epoll instance fd which the intr vector associated to.
 *   Using RTE_EPOLL_PER_THREAD allows to use per thread epoll instance.
 * @param op
 *   The operation be performed for the vector.
 *   Operation type of {RTE_INTR_EVENT_ADD, RTE_INTR_EVENT_DEL}.
 * @param data
 *   User raw data.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int rte_eth_dev_rx_intr_ctl(uint16_t port_id, int epfd, int op, void *data);

/**
 * Rx Interrupt control per queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue from which to retrieve input packets.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param epfd
 *   Epoll instance fd which the intr vector associated to.
 *   Using RTE_EPOLL_PER_THREAD allows to use per thread epoll instance.
 * @param op
 *   The operation be performed for the vector.
 *   Operation type of {RTE_INTR_EVENT_ADD, RTE_INTR_EVENT_DEL}.
 * @param data
 *   User raw data.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int rte_eth_dev_rx_intr_ctl_q(uint16_t port_id, uint16_t queue_id,
			      int epfd, int op, void *data);

/**
 * Get interrupt fd per Rx queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the receive queue from which to retrieve input packets.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - (>=0) the interrupt fd associated to the requested Rx queue if
 *           successful.
 *   - (-1) on error.
 */
int
rte_eth_dev_rx_intr_ctl_q_get_fd(uint16_t port_id, uint16_t queue_id);

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
 *   - (-EIO) if device is removed.
 */
int  rte_eth_led_on(uint16_t port_id);

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
 *   - (-EIO) if device is removed.
 */
int  rte_eth_led_off(uint16_t port_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get Forward Error Correction(FEC) capability.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param speed_fec_capa
 *   speed_fec_capa is out only with per-speed capabilities.
 *   If set to NULL, the function returns the required number
 *   of required array entries.
 * @param num
 *   a number of elements in an speed_fec_capa array.
 *
 * @return
 *   - A non-negative value lower or equal to num: success. The return value
 *     is the number of entries filled in the fec capa array.
 *   - A non-negative value higher than num: error, the given fec capa array
 *     is too small. The return value corresponds to the num that should
 *     be given to succeed. The entries in fec capa array are not valid and
 *     shall not be used by the caller.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support.
 *     that operation.
 *   - (-EIO) if device is removed.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EINVAL)  if *num* or *speed_fec_capa* invalid
 */
__rte_experimental
int rte_eth_fec_get_capability(uint16_t port_id,
			       struct rte_eth_fec_capa *speed_fec_capa,
			       unsigned int num);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get current Forward Error Correction(FEC) mode.
 * If link is down and AUTO is enabled, AUTO is returned, otherwise,
 * configured FEC mode is returned.
 * If link is up, current FEC mode is returned.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fec_capa
 *   A bitmask with the current FEC mode.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support.
 *     that operation.
 *   - (-EIO) if device is removed.
 *   - (-ENODEV)  if *port_id* invalid.
 */
__rte_experimental
int rte_eth_fec_get(uint16_t port_id, uint32_t *fec_capa);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Set Forward Error Correction(FEC) mode.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fec_capa
 *   A bitmask of allowed FEC modes.
 *   If only the AUTO bit is set, the decision on which FEC
 *   mode to use will be made by HW/FW or driver.
 *   If the AUTO bit is set with some FEC modes, only specified
 *   FEC modes can be set.
 *   If AUTO bit is clear, specify FEC mode to be used
 *   (only one valid mode per speed may be set).
 * @return
 *   - (0) if successful.
 *   - (-EINVAL) if the FEC mode is not valid.
 *   - (-ENOTSUP) if underlying hardware OR driver doesn't support.
 *   - (-EIO) if device is removed.
 *   - (-ENODEV)  if *port_id* invalid.
 */
__rte_experimental
int rte_eth_fec_set(uint16_t port_id, uint32_t fec_capa);

/**
 * Get current status of the Ethernet link flow control for Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fc_conf
 *   The pointer to the structure where to store the flow control parameters.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow control.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EIO)  if device is removed.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_flow_ctrl_get(uint16_t port_id,
			      struct rte_eth_fc_conf *fc_conf);

/**
 * Configure the Ethernet link flow control for Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param fc_conf
 *   The pointer to the structure of the flow control parameters.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flow control mode.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EINVAL)  if bad parameter
 *   - (-EIO)     if flow control setup failure or device is removed.
 */
int rte_eth_dev_flow_ctrl_set(uint16_t port_id,
			      struct rte_eth_fc_conf *fc_conf);

/**
 * Configure the Ethernet priority flow control under DCB environment
 * for Ethernet device.
 *
 * @param port_id
 * The port identifier of the Ethernet device.
 * @param pfc_conf
 * The pointer to the structure of the priority flow control parameters.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support priority flow control mode.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EINVAL)  if bad parameter
 *   - (-EIO)     if flow control setup failure or device is removed.
 */
int rte_eth_dev_priority_flow_ctrl_set(uint16_t port_id,
				struct rte_eth_pfc_conf *pfc_conf);

/**
 * Add a MAC address to the set used for filtering incoming packets.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   The MAC address to add.
 * @param pool
 *   VMDq pool index to associate address with (if VMDq is enabled). If VMDq is
 *   not enabled, this should be set to 0.
 * @return
 *   - (0) if successfully added or *mac_addr* was already added.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port* is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOSPC) if no more MAC addresses can be added.
 *   - (-EINVAL) if MAC address is invalid.
 */
int rte_eth_dev_mac_addr_add(uint16_t port_id, struct rte_ether_addr *mac_addr,
				uint32_t pool);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the information for queue based PFC.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param pfc_queue_info
 *   A pointer to a structure of type *rte_eth_pfc_queue_info* to be filled with
 *   the information about queue based PFC.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for priority_flow_ctrl_queue_info_get does not exist.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_dev_priority_flow_ctrl_queue_info_get(uint16_t port_id,
		struct rte_eth_pfc_queue_info *pfc_queue_info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Configure the queue based priority flow control for a given queue
 * for Ethernet device.
 *
 * @note When an ethdev port switches to queue based PFC mode, the
 * unconfigured queues shall be configured by the driver with
 * default values such as lower priority value for TC etc.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param pfc_queue_conf
 *   The pointer to the structure of the priority flow control parameters
 *   for the queue.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support queue based PFC mode.
 *   - (-ENODEV)  if *port_id* invalid.
 *   - (-EINVAL)  if bad parameter
 *   - (-EIO)     if flow control setup queue failure
 */
__rte_experimental
int rte_eth_dev_priority_flow_ctrl_queue_configure(uint16_t port_id,
		struct rte_eth_pfc_queue_conf *pfc_queue_conf);

/**
 * Remove a MAC address from the internal array of addresses.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   MAC address to remove.
 * @return
 *   - (0) if successful, or *mac_addr* didn't exist.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EADDRINUSE) if attempting to remove the default MAC address.
 *   - (-EINVAL) if MAC address is invalid.
 */
int rte_eth_dev_mac_addr_remove(uint16_t port_id,
				struct rte_ether_addr *mac_addr);

/**
 * Set the default MAC address.
 * It replaces the address at index 0 of the MAC address list.
 * If the address was already in the MAC address list,
 * please remove it first.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   New default MAC address.
 * @return
 *   - (0) if successful, or *mac_addr* didn't exist.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if MAC address is invalid.
 *   - (-EEXIST) if MAC address was already in the address list.
 */
int rte_eth_dev_default_mac_addr_set(uint16_t port_id,
		struct rte_ether_addr *mac_addr);

/**
 * Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *   RETA to update.
 * @param reta_size
 *   Redirection table size. The table size can be queried by
 *   rte_eth_dev_info_get().
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_rss_reta_update(uint16_t port_id,
				struct rte_eth_rss_reta_entry64 *reta_conf,
				uint16_t reta_size);

/**
 * Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *   RETA to query. For each requested reta entry, corresponding bit
 *   in mask must be set.
 * @param reta_size
 *   Redirection table size. The table size can be queried by
 *   rte_eth_dev_info_get().
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-EIO) if device is removed.
 */
int rte_eth_dev_rss_reta_query(uint16_t port_id,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);

/**
 * Updates unicast hash table for receiving packet with the given destination
 * MAC address, and the packet is routed to all VFs for which the Rx mode is
 * accept packets that match the unicast hash table.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param addr
 *   Unicast MAC address.
 * @param on
 *    1 - Set an unicast hash bit for receiving packets with the MAC address.
 *    0 - Clear an unicast hash bit.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_uc_hash_table_set(uint16_t port_id, struct rte_ether_addr *addr,
				  uint8_t on);

/**
 * Updates all unicast hash bitmaps for receiving packet with any Unicast
 * Ethernet MAC addresses,the packet is routed to all VFs for which the Rx
 * mode is accept packets that match the unicast hash table.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param on
 *    1 - Set all unicast hash bitmaps for receiving all the Ethernet
 *         MAC addresses
 *    0 - Clear all unicast hash bitmaps
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_uc_all_hash_table_set(uint16_t port_id, uint8_t on);

/**
 * Set the rate limitation for a queue on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_idx
 *   The queue ID.
 * @param tx_rate
 *   The Tx rate in Mbps. Allocated from the total port link speed.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_set_queue_rate_limit(uint16_t port_id, uint16_t queue_idx,
			uint32_t tx_rate);

/**
 * Configuration of Receive Side Scaling hash computation of Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rss_conf
 *   The new configuration to use for RSS hash computation on the port.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_hash_update(uint16_t port_id,
				struct rte_eth_rss_conf *rss_conf);

/**
 * Retrieve current configuration of Receive Side Scaling hash computation
 * of Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rss_conf
 *   Where to store the current RSS hash configuration of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if hardware doesn't support RSS.
 *   - (-EINVAL) if bad parameter.
 */
int
rte_eth_dev_rss_hash_conf_get(uint16_t port_id,
			      struct rte_eth_rss_conf *rss_conf);

/**
 * Add UDP tunneling port for a type of tunnel.
 *
 * Some NICs may require such configuration to properly parse a tunnel
 * with any standard or custom UDP port.
 * The packets with this UDP port will be parsed for this type of tunnel.
 * The device parser will also check the rest of the tunnel headers
 * before classifying the packet.
 *
 * With some devices, this API will affect packet classification, i.e.:
 *     - mbuf.packet_type reported on Rx
 *     - rte_flow rules with tunnel items
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tunnel_udp
 *   UDP tunneling configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_udp_tunnel_port_add(uint16_t port_id,
				struct rte_eth_udp_tunnel *tunnel_udp);

/**
 * Delete UDP tunneling port for a type of tunnel.
 *
 * The packets with this UDP port will not be classified as this type of tunnel
 * anymore if the device use such mapping for tunnel packet classification.
 *
 * @see rte_eth_dev_udp_tunnel_port_add
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tunnel_udp
 *   UDP tunneling configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_udp_tunnel_port_delete(uint16_t port_id,
				   struct rte_eth_udp_tunnel *tunnel_udp);

/**
 * Get DCB information on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dcb_info
 *   DCB information.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_get_dcb_info(uint16_t port_id,
			     struct rte_eth_dcb_info *dcb_info);

struct rte_eth_rxtx_callback;

/**
 * Add a callback to be called on packet Rx on a given port and queue.
 *
 * This API configures a function to be called for each burst of
 * packets received on a given NIC port queue. The return value is a pointer
 * that can be used to later remove the callback using
 * rte_eth_remove_rx_callback().
 *
 * Multiple functions are called in the order that they are added.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue on the Ethernet device on which the callback is to be added.
 * @param fn
 *   The callback function
 * @param user_param
 *   A generic pointer parameter which will be passed to each invocation of the
 *   callback function on this port and queue. Inter-thread synchronization
 *   of any user data changes is the responsibility of the user.
 *
 * @return
 *   NULL on error.
 *   On success, a pointer value which can later be used to remove the callback.
 */
const struct rte_eth_rxtx_callback *
rte_eth_add_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param);

/**
 * Add a callback that must be called first on packet Rx on a given port
 * and queue.
 *
 * This API configures a first function to be called for each burst of
 * packets received on a given NIC port queue. The return value is a pointer
 * that can be used to later remove the callback using
 * rte_eth_remove_rx_callback().
 *
 * Multiple functions are called in the order that they are added.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue on the Ethernet device on which the callback is to be added.
 * @param fn
 *   The callback function
 * @param user_param
 *   A generic pointer parameter which will be passed to each invocation of the
 *   callback function on this port and queue. Inter-thread synchronization
 *   of any user data changes is the responsibility of the user.
 *
 * @return
 *   NULL on error.
 *   On success, a pointer value which can later be used to remove the callback.
 */
const struct rte_eth_rxtx_callback *
rte_eth_add_first_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param);

/**
 * Add a callback to be called on packet Tx on a given port and queue.
 *
 * This API configures a function to be called for each burst of
 * packets sent on a given NIC port queue. The return value is a pointer
 * that can be used to later remove the callback using
 * rte_eth_remove_tx_callback().
 *
 * Multiple functions are called in the order that they are added.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue on the Ethernet device on which the callback is to be added.
 * @param fn
 *   The callback function
 * @param user_param
 *   A generic pointer parameter which will be passed to each invocation of the
 *   callback function on this port and queue. Inter-thread synchronization
 *   of any user data changes is the responsibility of the user.
 *
 * @return
 *   NULL on error.
 *   On success, a pointer value which can later be used to remove the callback.
 */
const struct rte_eth_rxtx_callback *
rte_eth_add_tx_callback(uint16_t port_id, uint16_t queue_id,
		rte_tx_callback_fn fn, void *user_param);

/**
 * Remove an Rx packet callback from a given port and queue.
 *
 * This function is used to removed callbacks that were added to a NIC port
 * queue using rte_eth_add_rx_callback().
 *
 * Note: the callback is removed from the callback list but it isn't freed
 * since the it may still be in use. The memory for the callback can be
 * subsequently freed back by the application by calling rte_free():
 *
 * - Immediately - if the port is stopped, or the user knows that no
 *   callbacks are in flight e.g. if called from the thread doing Rx/Tx
 *   on that queue.
 *
 * - After a short delay - where the delay is sufficient to allow any
 *   in-flight callbacks to complete. Alternately, the RCU mechanism can be
 *   used to detect when data plane threads have ceased referencing the
 *   callback memory.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue on the Ethernet device from which the callback is to be removed.
 * @param user_cb
 *   User supplied callback created via rte_eth_add_rx_callback().
 *
 * @return
 *   - 0: Success. Callback was removed.
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: Callback support is not available.
 *   - -EINVAL:  The queue_id is out of range, or the callback
 *               is NULL or not found for the port/queue.
 */
int rte_eth_remove_rx_callback(uint16_t port_id, uint16_t queue_id,
		const struct rte_eth_rxtx_callback *user_cb);

/**
 * Remove a Tx packet callback from a given port and queue.
 *
 * This function is used to removed callbacks that were added to a NIC port
 * queue using rte_eth_add_tx_callback().
 *
 * Note: the callback is removed from the callback list but it isn't freed
 * since the it may still be in use. The memory for the callback can be
 * subsequently freed back by the application by calling rte_free():
 *
 * - Immediately - if the port is stopped, or the user knows that no
 *   callbacks are in flight e.g. if called from the thread doing Rx/Tx
 *   on that queue.
 *
 * - After a short delay - where the delay is sufficient to allow any
 *   in-flight callbacks to complete. Alternately, the RCU mechanism can be
 *   used to detect when data plane threads have ceased referencing the
 *   callback memory.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue on the Ethernet device from which the callback is to be removed.
 * @param user_cb
 *   User supplied callback created via rte_eth_add_tx_callback().
 *
 * @return
 *   - 0: Success. Callback was removed.
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: Callback support is not available.
 *   - -EINVAL:  The queue_id is out of range, or the callback
 *               is NULL or not found for the port/queue.
 */
int rte_eth_remove_tx_callback(uint16_t port_id, uint16_t queue_id,
		const struct rte_eth_rxtx_callback *user_cb);

/**
 * Retrieve information about given port's Rx queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The Rx queue on the Ethernet device for which information
 *   will be retrieved.
 * @param qinfo
 *   A pointer to a structure of type *rte_eth_rxq_info_info* to be filled with
 *   the information of the Ethernet device.
 *
 * @return
 *   - 0: Success
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The queue_id is out of range, or the queue
 *               is hairpin queue.
 */
int rte_eth_rx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

/**
 * Retrieve information about given port's Tx queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The Tx queue on the Ethernet device for which information
 *   will be retrieved.
 * @param qinfo
 *   A pointer to a structure of type *rte_eth_txq_info_info* to be filled with
 *   the information of the Ethernet device.
 *
 * @return
 *   - 0: Success
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The queue_id is out of range, or the queue
 *               is hairpin queue.
 */
int rte_eth_tx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

/**
 * Retrieve information about the Rx packet burst mode.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The Rx queue on the Ethernet device for which information
 *   will be retrieved.
 * @param mode
 *   A pointer to a structure of type *rte_eth_burst_mode* to be filled
 *   with the information of the packet burst mode.
 *
 * @return
 *   - 0: Success
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The queue_id is out of range.
 */
int rte_eth_rx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_burst_mode *mode);

/**
 * Retrieve information about the Tx packet burst mode.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The Tx queue on the Ethernet device for which information
 *   will be retrieved.
 * @param mode
 *   A pointer to a structure of type *rte_eth_burst_mode* to be filled
 *   with the information of the packet burst mode.
 *
 * @return
 *   - 0: Success
 *   - -ENODEV:  If *port_id* is invalid.
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The queue_id is out of range.
 */
int rte_eth_tx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_burst_mode *mode);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the monitor condition for a given receive queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The Rx queue on the Ethernet device for which information
 *   will be retrieved.
 * @param pmc
 *   The pointer to power-optimized monitoring condition structure.
 *
 * @return
 *   - 0: Success.
 *   -ENOTSUP: Operation not supported.
 *   -EINVAL: Invalid parameters.
 *   -ENODEV: Invalid port ID.
 */
__rte_experimental
int rte_eth_get_monitor_addr(uint16_t port_id, uint16_t queue_id,
		struct rte_power_monitor_cond *pmc);

/**
 * Retrieve device registers and register attributes (number of registers and
 * register size)
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param info
 *   Pointer to rte_dev_reg_info structure to fill in. If info->data is
 *   NULL the function fills in the width and length fields. If non-NULL
 *   the registers are put into the buffer pointed at by the data field.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_reg_info(uint16_t port_id, struct rte_dev_reg_info *info);

/**
 * Retrieve size of device EEPROM
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (>=0) EEPROM size if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_eeprom_length(uint16_t port_id);

/**
 * Retrieve EEPROM and EEPROM attribute
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param info
 *   The template includes buffer for return EEPROM data and
 *   EEPROM attributes to be filled.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info);

/**
 * Program EEPROM with provided data
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param info
 *   The template includes EEPROM data for programming and
 *   EEPROM attributes to be filled
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_set_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the type and size of plugin module EEPROM
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param modinfo
 *   The type and size of plugin module EEPROM.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
__rte_experimental
int
rte_eth_dev_get_module_info(uint16_t port_id,
			    struct rte_eth_dev_module_info *modinfo);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the data of plugin module EEPROM
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param info
 *   The template includes the plugin module EEPROM attributes, and the
 *   buffer for return plugin module EEPROM data.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - others depends on the specific operations implementation.
 */
__rte_experimental
int
rte_eth_dev_get_module_eeprom(uint16_t port_id,
			      struct rte_dev_eeprom_info *info);

/**
 * Set the list of multicast addresses to filter on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mc_addr_set
 *   The array of multicast addresses to set. Equal to NULL when the function
 *   is invoked to flush the set of filtered addresses.
 * @param nb_mc_addr
 *   The number of multicast addresses in the *mc_addr_set* array. Equal to 0
 *   when the function is invoked to flush the set of filtered addresses.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-ENOTSUP) if PMD of *port_id* doesn't support multicast filtering.
 *   - (-ENOSPC) if *port_id* has not enough multicast filtering resources.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_set_mc_addr_list(uint16_t port_id,
				 struct rte_ether_addr *mc_addr_set,
				 uint32_t nb_mc_addr);

/**
 * Enable IEEE1588/802.1AS timestamping for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 *
 * @return
 *   - 0: Success.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_enable(uint16_t port_id);

/**
 * Disable IEEE1588/802.1AS timestamping for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 *
 * @return
 *   - 0: Success.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_disable(uint16_t port_id);

/**
 * Read an IEEE1588/802.1AS Rx timestamp from an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param timestamp
 *   Pointer to the timestamp struct.
 * @param flags
 *   Device specific flags. Used to pass the Rx timesync register index to
 *   i40e. Unused in igb/ixgbe, pass 0 instead.
 *
 * @return
 *   - 0: Success.
 *   - -EINVAL: No timestamp is available.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_read_rx_timestamp(uint16_t port_id,
		struct timespec *timestamp, uint32_t flags);

/**
 * Read an IEEE1588/802.1AS Tx timestamp from an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param timestamp
 *   Pointer to the timestamp struct.
 *
 * @return
 *   - 0: Success.
 *   - -EINVAL: No timestamp is available.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_read_tx_timestamp(uint16_t port_id,
		struct timespec *timestamp);

/**
 * Adjust the timesync clock on an Ethernet device.
 *
 * This is usually used in conjunction with other Ethdev timesync functions to
 * synchronize the device time using the IEEE1588/802.1AS protocol.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param delta
 *   The adjustment in nanoseconds.
 *
 * @return
 *   - 0: Success.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_adjust_time(uint16_t port_id, int64_t delta);

/**
 * Read the time from the timesync clock on an Ethernet device.
 *
 * This is usually used in conjunction with other Ethdev timesync functions to
 * synchronize the device time using the IEEE1588/802.1AS protocol.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param time
 *   Pointer to the timespec struct that holds the time.
 *
 * @return
 *   - 0: Success.
 *   - -EINVAL: Bad parameter.
 */
int rte_eth_timesync_read_time(uint16_t port_id, struct timespec *time);

/**
 * Set the time of the timesync clock on an Ethernet device.
 *
 * This is usually used in conjunction with other Ethdev timesync functions to
 * synchronize the device time using the IEEE1588/802.1AS protocol.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param time
 *   Pointer to the timespec struct that holds the time.
 *
 * @return
 *   - 0: Success.
 *   - -EINVAL: No timestamp is available.
 *   - -ENODEV: The port ID is invalid.
 *   - -EIO: if device is removed.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_write_time(uint16_t port_id, const struct timespec *time);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Read the current clock counter of an Ethernet device
 *
 * This returns the current raw clock value of an Ethernet device. It is
 * a raw amount of ticks, with no given time reference.
 * The value returned here is from the same clock than the one
 * filling timestamp field of Rx packets when using hardware timestamp
 * offload. Therefore it can be used to compute a precise conversion of
 * the device clock to the real time.
 *
 * E.g, a simple heuristic to derivate the frequency would be:
 * uint64_t start, end;
 * rte_eth_read_clock(port, start);
 * rte_delay_ms(100);
 * rte_eth_read_clock(port, end);
 * double freq = (end - start) * 10;
 *
 * Compute a common reference with:
 * uint64_t base_time_sec = current_time();
 * uint64_t base_clock;
 * rte_eth_read_clock(port, base_clock);
 *
 * Then, convert the raw mbuf timestamp with:
 * base_time_sec + (double)(*timestamp_dynfield(mbuf) - base_clock) / freq;
 *
 * This simple example will not provide a very good accuracy. One must
 * at least measure multiple times the frequency and do a regression.
 * To avoid deviation from the system time, the common reference can
 * be repeated from time to time. The integer division can also be
 * converted by a multiplication and a shift for better performance.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param clock
 *   Pointer to the uint64_t that holds the raw clock value.
 *
 * @return
 *   - 0: Success.
 *   - -ENODEV: The port ID is invalid.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 *   - -EINVAL: if bad parameter.
 */
__rte_experimental
int
rte_eth_read_clock(uint16_t port_id, uint64_t *clock);

/**
 * Get the port ID from device name.
 * The device name should be specified as below:
 * - PCIe address (Domain:Bus:Device.Function), for example- 0000:2:00.0
 * - SoC device name, for example- fsl-gmac0
 * - vdev dpdk name, for example- net_[pcap0|null0|tap0]
 *
 * @param name
 *   PCI address or name of the device.
 * @param port_id
 *   Pointer to port identifier of the device.
 * @return
 *   - (0) if successful and port_id is filled.
 *   - (-ENODEV or -EINVAL) on failure.
 */
int
rte_eth_dev_get_port_by_name(const char *name, uint16_t *port_id);

/**
 * Get the device name from port ID.
 * The device name is specified as below:
 * - PCIe address (Domain:Bus:Device.Function), for example- 0000:02:00.0
 * - SoC device name, for example- fsl-gmac0
 * - vdev dpdk name, for example- net_[pcap0|null0|tun0|tap0]
 *
 * @param port_id
 *   Port identifier of the device.
 * @param name
 *   Buffer of size RTE_ETH_NAME_MAX_LEN to store the name.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-EINVAL) on failure.
 */
int
rte_eth_dev_get_name_by_port(uint16_t port_id, char *name);

/**
 * Check that numbers of Rx and Tx descriptors satisfy descriptors limits from
 * the Ethernet device information, otherwise adjust them to boundaries.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param nb_rx_desc
 *   A pointer to a uint16_t where the number of receive
 *   descriptors stored.
 * @param nb_tx_desc
 *   A pointer to a uint16_t where the number of transmit
 *   descriptors stored.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP, -ENODEV or -EINVAL) on failure.
 */
int rte_eth_dev_adjust_nb_rx_tx_desc(uint16_t port_id,
				     uint16_t *nb_rx_desc,
				     uint16_t *nb_tx_desc);

/**
 * Test if a port supports specific mempool ops.
 *
 * @param port_id
 *   Port identifier of the Ethernet device.
 * @param [in] pool
 *   The name of the pool operations to test.
 * @return
 *   - 0: best mempool ops choice for this port.
 *   - 1: mempool ops are supported for this port.
 *   - -ENOTSUP: mempool ops not supported for this port.
 *   - -ENODEV: Invalid port Identifier.
 *   - -EINVAL: Pool param is null.
 */
int
rte_eth_dev_pool_ops_supported(uint16_t port_id, const char *pool);

/**
 * Get the security context for the Ethernet device.
 *
 * @param port_id
 *   Port identifier of the Ethernet device
 * @return
 *   - NULL on error.
 *   - pointer to security context on success.
 */
void *
rte_eth_dev_get_sec_ctx(uint16_t port_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Query the device hairpin capabilities.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param cap
 *   Pointer to a structure that will hold the hairpin capabilities.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_dev_hairpin_capability_get(uint16_t port_id,
				       struct rte_eth_hairpin_cap *cap);

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * Ethernet device representor ID range entry
 */
struct rte_eth_representor_range {
	enum rte_eth_representor_type type; /**< Representor type */
	int controller; /**< Controller index */
	int pf; /**< Physical function index */
	__extension__
	union {
		int vf; /**< VF start index */
		int sf; /**< SF start index */
	};
	uint32_t id_base; /**< Representor ID start index */
	uint32_t id_end;  /**< Representor ID end index */
	char name[RTE_DEV_NAME_MAX_LEN]; /**< Representor name */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * Ethernet device representor information
 */
struct rte_eth_representor_info {
	uint16_t controller; /**< Controller ID of caller device. */
	uint16_t pf; /**< Physical function ID of caller device. */
	uint32_t nb_ranges_alloc; /**< Size of the ranges array. */
	uint32_t nb_ranges; /**< Number of initialized ranges. */
	struct rte_eth_representor_range ranges[];/**< Representor ID range. */
};

/**
 * Retrieve the representor info of the device.
 *
 * Get device representor info to be able to calculate a unique
 * representor ID. @see rte_eth_representor_id_get helper.
 *
 * @param port_id
 *   The port identifier of the device.
 * @param info
 *   A pointer to a representor info structure.
 *   NULL to return number of range entries and allocate memory
 *   for next call to store detail.
 *   The number of ranges that were written into this structure
 *   will be placed into its nb_ranges field. This number cannot be
 *   larger than the nb_ranges_alloc that by the user before calling
 *   this function. It can be smaller than the value returned by the
 *   function, however.
 * @return
 *   - (-ENOTSUP) if operation is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (>=0) number of available representor range entries.
 */
__rte_experimental
int rte_eth_representor_info_get(uint16_t port_id,
				 struct rte_eth_representor_info *info);

/** The NIC is able to deliver flag (if set) with packets to the PMD. */
#define RTE_ETH_RX_METADATA_USER_FLAG RTE_BIT64(0)

/** The NIC is able to deliver mark ID with packets to the PMD. */
#define RTE_ETH_RX_METADATA_USER_MARK RTE_BIT64(1)

/** The NIC is able to deliver tunnel ID with packets to the PMD. */
#define RTE_ETH_RX_METADATA_TUNNEL_ID RTE_BIT64(2)

/**
 * Negotiate the NIC's ability to deliver specific kinds of metadata to the PMD.
 *
 * Invoke this API before the first rte_eth_dev_configure() invocation
 * to let the PMD make preparations that are inconvenient to do later.
 *
 * The negotiation process is as follows:
 *
 * - the application requests features intending to use at least some of them;
 * - the PMD responds with the guaranteed subset of the requested feature set;
 * - the application can retry negotiation with another set of features;
 * - the application can pass zero to clear the negotiation result;
 * - the last negotiated result takes effect upon
 *   the ethdev configure and start.
 *
 * @note
 *   The PMD is supposed to first consider enabling the requested feature set
 *   in its entirety. Only if it fails to do so, does it have the right to
 *   respond with a smaller set of the originally requested features.
 *
 * @note
 *   Return code (-ENOTSUP) does not necessarily mean that the requested
 *   features are unsupported. In this case, the application should just
 *   assume that these features can be used without prior negotiations.
 *
 * @param port_id
 *   Port (ethdev) identifier
 *
 * @param[inout] features
 *   Feature selection buffer
 *
 * @return
 *   - (-EBUSY) if the port can't handle this in its current state;
 *   - (-ENOTSUP) if the method itself is not supported by the PMD;
 *   - (-ENODEV) if *port_id* is invalid;
 *   - (-EINVAL) if *features* is NULL;
 *   - (-EIO) if the device is removed;
 *   - (0) on success
 */
int rte_eth_rx_metadata_negotiate(uint16_t port_id, uint64_t *features);

/** Flag to offload IP reassembly for IPv4 packets. */
#define RTE_ETH_DEV_REASSEMBLY_F_IPV4 (RTE_BIT32(0))
/** Flag to offload IP reassembly for IPv6 packets. */
#define RTE_ETH_DEV_REASSEMBLY_F_IPV6 (RTE_BIT32(1))

/**
 * A structure used to get/set IP reassembly configuration. It is also used
 * to get the maximum capability values that a PMD can support.
 *
 * If rte_eth_ip_reassembly_capability_get() returns 0, IP reassembly can be
 * enabled using rte_eth_ip_reassembly_conf_set() and params values lower than
 * capability params can be set in the PMD.
 */
struct rte_eth_ip_reassembly_params {
	/** Maximum time in ms which PMD can wait for other fragments. */
	uint32_t timeout_ms;
	/** Maximum number of fragments that can be reassembled. */
	uint16_t max_frags;
	/**
	 * Flags to enable reassembly of packet types -
	 * RTE_ETH_DEV_REASSEMBLY_F_xxx.
	 */
	uint16_t flags;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Get IP reassembly capabilities supported by the PMD. This is the first API
 * to be called for enabling the IP reassembly offload feature. PMD will return
 * the maximum values of parameters that PMD can support and user can call
 * rte_eth_ip_reassembly_conf_set() with param values lower than capability.
 *
 * @param port_id
 *   The port identifier of the device.
 * @param capa
 *   A pointer to rte_eth_ip_reassembly_params structure.
 * @return
 *   - (-ENOTSUP) if offload configuration is not supported by device.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if device is not configured or *capa* passed is NULL.
 *   - (0) on success.
 */
__rte_experimental
int rte_eth_ip_reassembly_capability_get(uint16_t port_id,
		struct rte_eth_ip_reassembly_params *capa);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Get IP reassembly configuration parameters currently set in PMD.
 * The API will return error if the configuration is not already
 * set using rte_eth_ip_reassembly_conf_set() before calling this API or if
 * the device is not configured.
 *
 * @param port_id
 *   The port identifier of the device.
 * @param conf
 *   A pointer to rte_eth_ip_reassembly_params structure.
 * @return
 *   - (-ENOTSUP) if offload configuration is not supported by device.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if device is not configured or if *conf* passed is NULL or if
 *              configuration is not set using rte_eth_ip_reassembly_conf_set().
 *   - (0) on success.
 */
__rte_experimental
int rte_eth_ip_reassembly_conf_get(uint16_t port_id,
		struct rte_eth_ip_reassembly_params *conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Set IP reassembly configuration parameters if the PMD supports IP reassembly
 * offload. User should first call rte_eth_ip_reassembly_capability_get() to
 * check the maximum values supported by the PMD before setting the
 * configuration. The use of this API is mandatory to enable this feature and
 * should be called before rte_eth_dev_start().
 *
 * In datapath, PMD cannot guarantee that IP reassembly is always successful.
 * Hence, PMD shall register mbuf dynamic field and dynamic flag using
 * rte_eth_ip_reassembly_dynfield_register() to denote incomplete IP reassembly.
 * If dynfield is not successfully registered, error will be returned and
 * IP reassembly offload cannot be used.
 *
 * @param port_id
 *   The port identifier of the device.
 * @param conf
 *   A pointer to rte_eth_ip_reassembly_params structure.
 * @return
 *   - (-ENOTSUP) if offload configuration is not supported by device.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EIO) if device is removed.
 *   - (-EINVAL) if device is not configured or if device is already started or
 *               if *conf* passed is NULL or if mbuf dynfield is not registered
 *               successfully by the PMD.
 *   - (0) on success.
 */
__rte_experimental
int rte_eth_ip_reassembly_conf_set(uint16_t port_id,
		const struct rte_eth_ip_reassembly_params *conf);

/**
 * In case of IP reassembly offload failure, packet will be updated with
 * dynamic flag - RTE_MBUF_DYNFLAG_IP_REASSEMBLY_INCOMPLETE_NAME and packets
 * will be returned without alteration.
 * The application can retrieve the attached fragments using mbuf dynamic field
 * RTE_MBUF_DYNFIELD_IP_REASSEMBLY_NAME.
 */
typedef struct {
	/**
	 * Next fragment packet. Application should fetch dynamic field of
	 * each fragment until a NULL is received and nb_frags is 0.
	 */
	struct rte_mbuf *next_frag;
	/** Time spent(in ms) by HW in waiting for further fragments. */
	uint16_t time_spent;
	/** Number of more fragments attached in mbuf dynamic fields. */
	uint16_t nb_frags;
} rte_eth_ip_reassembly_dynfield_t;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Dump private info from device to a file. Provided data and the order depends
 * on the PMD.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param file
 *   A pointer to a file for output.
 * @return
 *   - (0) on success.
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-EINVAL) if null file.
 *   - (-ENOTSUP) if the device does not support this function.
 *   - (-EIO) if device is removed.
 */
__rte_experimental
int rte_eth_dev_priv_dump(uint16_t port_id, FILE *file);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Dump ethdev Rx descriptor info to a file.
 *
 * This API is used for debugging, not a dataplane API.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   A Rx queue identifier on this port.
 * @param offset
 *  The offset of the descriptor starting from tail. (0 is the next
 *  packet to be received by the driver).
 * @param num
 *   The number of the descriptors to dump.
 * @param file
 *   A pointer to a file for output.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
__rte_experimental
int rte_eth_rx_descriptor_dump(uint16_t port_id, uint16_t queue_id,
			       uint16_t offset, uint16_t num, FILE *file);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Dump ethdev Tx descriptor info to a file.
 *
 * This API is used for debugging, not a dataplane API.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   A Tx queue identifier on this port.
 * @param offset
 *  The offset of the descriptor starting from tail. (0 is the place where
 *  the next packet will be send).
 * @param num
 *   The number of the descriptors to dump.
 * @param file
 *   A pointer to a file for output.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
__rte_experimental
int rte_eth_tx_descriptor_dump(uint16_t port_id, uint16_t queue_id,
			       uint16_t offset, uint16_t num, FILE *file);


/* Congestion management */

/** Enumerate list of ethdev congestion management objects */
enum rte_eth_cman_obj {
	/** Congestion management based on Rx queue depth */
	RTE_ETH_CMAN_OBJ_RX_QUEUE = RTE_BIT32(0),
	/**
	 * Congestion management based on mempool depth associated with Rx queue
	 * @see rte_eth_rx_queue_setup()
	 */
	RTE_ETH_CMAN_OBJ_RX_QUEUE_MEMPOOL = RTE_BIT32(1),
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change, or be removed, without prior notice
 *
 * A structure used to retrieve information of ethdev congestion management.
 */
struct rte_eth_cman_info {
	/**
	 * Set of supported congestion management modes
	 * @see enum rte_cman_mode
	 */
	uint64_t modes_supported;
	/**
	 * Set of supported congestion management objects
	 * @see enum rte_eth_cman_obj
	 */
	uint64_t objs_supported;
	/**
	 * Reserved for future fields. Always returned as 0 when
	 * rte_eth_cman_info_get() is invoked
	 */
	uint8_t rsvd[8];
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change, or be removed, without prior notice
 *
 * A structure used to configure the ethdev congestion management.
 */
struct rte_eth_cman_config {
	/** Congestion management object */
	enum rte_eth_cman_obj obj;
	/** Congestion management mode */
	enum rte_cman_mode mode;
	union {
		/**
		 * Rx queue to configure congestion management.
		 *
		 * Valid when object is RTE_ETH_CMAN_OBJ_RX_QUEUE or
		 * RTE_ETH_CMAN_OBJ_RX_QUEUE_MEMPOOL.
		 */
		uint16_t rx_queue;
		/**
		 * Reserved for future fields.
		 * It must be set to 0 when rte_eth_cman_config_set() is invoked
		 * and will be returned as 0 when rte_eth_cman_config_get() is
		 * invoked.
		 */
		uint8_t rsvd_obj_params[4];
	} obj_param;
	union {
		/**
		 * RED configuration parameters.
		 *
		 * Valid when mode is RTE_CMAN_RED.
		 */
		struct rte_cman_red_params red;
		/**
		 * Reserved for future fields.
		 * It must be set to 0 when rte_eth_cman_config_set() is invoked
		 * and will be returned as 0 when rte_eth_cman_config_get() is
		 * invoked.
		 */
		uint8_t rsvd_mode_params[4];
	} mode_param;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Retrieve the information for ethdev congestion management
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param info
 *   A pointer to a structure of type *rte_eth_cman_info* to be filled with
 *   the information about congestion management.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for cman_info_get does not exist.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_cman_info_get(uint16_t port_id, struct rte_eth_cman_info *info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Initialize the ethdev congestion management configuration structure with default values.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param config
 *   A pointer to a structure of type *rte_eth_cman_config* to be initialized
 *   with default value.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for cman_config_init does not exist.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_cman_config_init(uint16_t port_id, struct rte_eth_cman_config *config);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Configure ethdev congestion management
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param config
 *   A pointer to a structure of type *rte_eth_cman_config* to be configured.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for cman_config_set does not exist.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_cman_config_set(uint16_t port_id, const struct rte_eth_cman_config *config);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Retrieve the applied ethdev congestion management parameters for the given port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param config
 *   A pointer to a structure of type *rte_eth_cman_config* to retrieve
 *   congestion management parameters for the given object.
 *   Application must fill all parameters except mode_param parameter in
 *   struct rte_eth_cman_config.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if support for cman_config_get does not exist.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_cman_config_get(uint16_t port_id, struct rte_eth_cman_config *config);

#include <rte_ethdev_core.h>

/**
 * @internal
 * Helper routine for rte_eth_rx_burst().
 * Should be called at exit from PMD's rte_eth_rx_bulk implementation.
 * Does necessary post-processing - invokes Rx callbacks if any, etc.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The index of the receive queue from which to retrieve input packets.
 * @param rx_pkts
 *   The address of an array of pointers to *rte_mbuf* structures that
 *   have been retrieved from the device.
 * @param nb_rx
 *   The number of packets that were retrieved from the device.
 * @param nb_pkts
 *   The number of elements in @p rx_pkts array.
 * @param opaque
 *   Opaque pointer of Rx queue callback related data.
 *
 * @return
 *  The number of packets effectively supplied to the @p rx_pkts array.
 */
uint16_t rte_eth_call_rx_callbacks(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, uint16_t nb_rx, uint16_t nb_pkts,
		void *opaque);

/**
 *
 * Retrieve a burst of input packets from a receive queue of an Ethernet
 * device. The retrieved packets are stored in *rte_mbuf* structures whose
 * pointers are supplied in the *rx_pkts* array.
 *
 * The rte_eth_rx_burst() function loops, parsing the Rx ring of the
 * receive queue, up to *nb_pkts* packets, and for each completed Rx
 * descriptor in the ring, it performs the following operations:
 *
 * - Initialize the *rte_mbuf* data structure associated with the
 *   Rx descriptor according to the information provided by the NIC into
 *   that Rx descriptor.
 *
 * - Store the *rte_mbuf* data structure into the next entry of the
 *   *rx_pkts* array.
 *
 * - Replenish the Rx descriptor with a new *rte_mbuf* buffer
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
 * A return value equal to *nb_pkts* indicates that the Rx queue contained
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
 *   individually queued by the driver, this approach allows the caller
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
 * @note
 *   Some drivers using vector instructions require that *nb_pkts* is
 *   divisible by 4 or 8, depending on the driver implementation.
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
 *   The value must be divisible by 8 in order to work with any driver.
 * @return
 *   The number of packets actually retrieved, which is the number
 *   of pointers to *rte_mbuf* structures effectively supplied to the
 *   *rx_pkts* array.
 */
static inline uint16_t
rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_RX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		return 0;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->rxq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_RX
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);

	if (qd == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u for port_id=%u\n",
			queue_id, port_id);
		return 0;
	}
#endif

	nb_rx = p->rx_pkt_burst(qd, rx_pkts, nb_pkts);

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
	{
		void *cb;

		/* __ATOMIC_RELEASE memory order was used when the
		 * call back was inserted into the list.
		 * Since there is a clear dependency between loading
		 * cb and cb->fn/cb->next, __ATOMIC_ACQUIRE memory order is
		 * not required.
		 */
		cb = __atomic_load_n((void **)&p->rxq.clbk[queue_id],
				__ATOMIC_RELAXED);
		if (unlikely(cb != NULL))
			nb_rx = rte_eth_call_rx_callbacks(port_id, queue_id,
					rx_pkts, nb_rx, nb_pkts, cb);
	}
#endif

	rte_ethdev_trace_rx_burst(port_id, queue_id, (void **)rx_pkts, nb_rx);
	return nb_rx;
}

/**
 * Get the number of used descriptors of a Rx queue
 *
 * Since it's a dataplane function, no check is performed on port_id and
 * queue_id. The caller must therefore ensure that the port is enabled
 * and the queue is configured and running.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The queue ID on the specific port.
 * @return
 *  The number of used descriptors in the specific queue, or:
 *   - (-ENODEV) if *port_id* is invalid.
 *   - (-EINVAL) if *queue_id* is invalid
 *   - (-ENOTSUP) if the device does not support this function
 */
static inline int
rte_eth_rx_queue_count(uint16_t port_id, uint16_t queue_id)
{
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_RX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		return -EINVAL;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->rxq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_RX
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (qd == NULL)
		return -EINVAL;
#endif

	if (*p->rx_queue_count == NULL)
		return -ENOTSUP;
	return (int)(*p->rx_queue_count)(qd);
}

/**@{@name Rx hardware descriptor states
 * @see rte_eth_rx_descriptor_status
 */
#define RTE_ETH_RX_DESC_AVAIL    0 /**< Desc available for hw. */
#define RTE_ETH_RX_DESC_DONE     1 /**< Desc done, filled by hw. */
#define RTE_ETH_RX_DESC_UNAVAIL  2 /**< Desc used by driver or hw. */
/**@}*/

/**
 * Check the status of a Rx descriptor in the queue
 *
 * It should be called in a similar context than the Rx function:
 * - on a dataplane core
 * - not concurrently on the same queue
 *
 * Since it's a dataplane function, no check is performed on port_id and
 * queue_id. The caller must therefore ensure that the port is enabled
 * and the queue is configured and running.
 *
 * Note: accessing to a random descriptor in the ring may trigger cache
 * misses and have a performance impact.
 *
 * @param port_id
 *  A valid port identifier of the Ethernet device which.
 * @param queue_id
 *  A valid Rx queue identifier on this port.
 * @param offset
 *  The offset of the descriptor starting from tail (0 is the next
 *  packet to be received by the driver).
 *
 * @return
 *  - (RTE_ETH_RX_DESC_AVAIL): Descriptor is available for the hardware to
 *    receive a packet.
 *  - (RTE_ETH_RX_DESC_DONE): Descriptor is done, it is filled by hw, but
 *    not yet processed by the driver (i.e. in the receive queue).
 *  - (RTE_ETH_RX_DESC_UNAVAIL): Descriptor is unavailable, either hold by
 *    the driver and not yet returned to hw, or reserved by the hw.
 *  - (-EINVAL) bad descriptor offset.
 *  - (-ENOTSUP) if the device does not support this function.
 *  - (-ENODEV) bad port or queue (only if compiled with debug).
 */
static inline int
rte_eth_rx_descriptor_status(uint16_t port_id, uint16_t queue_id,
	uint16_t offset)
{
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_RX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		return -EINVAL;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->rxq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_RX
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (qd == NULL)
		return -ENODEV;
#endif
	if (*p->rx_descriptor_status == NULL)
		return -ENOTSUP;
	return (*p->rx_descriptor_status)(qd, offset);
}

/**@{@name Tx hardware descriptor states
 * @see rte_eth_tx_descriptor_status
 */
#define RTE_ETH_TX_DESC_FULL    0 /**< Desc filled for hw, waiting xmit. */
#define RTE_ETH_TX_DESC_DONE    1 /**< Desc done, packet is transmitted. */
#define RTE_ETH_TX_DESC_UNAVAIL 2 /**< Desc used by driver or hw. */
/**@}*/

/**
 * Check the status of a Tx descriptor in the queue.
 *
 * It should be called in a similar context than the Tx function:
 * - on a dataplane core
 * - not concurrently on the same queue
 *
 * Since it's a dataplane function, no check is performed on port_id and
 * queue_id. The caller must therefore ensure that the port is enabled
 * and the queue is configured and running.
 *
 * Note: accessing to a random descriptor in the ring may trigger cache
 * misses and have a performance impact.
 *
 * @param port_id
 *  A valid port identifier of the Ethernet device which.
 * @param queue_id
 *  A valid Tx queue identifier on this port.
 * @param offset
 *  The offset of the descriptor starting from tail (0 is the place where
 *  the next packet will be send).
 *
 * @return
 *  - (RTE_ETH_TX_DESC_FULL) Descriptor is being processed by the hw, i.e.
 *    in the transmit queue.
 *  - (RTE_ETH_TX_DESC_DONE) Hardware is done with this descriptor, it can
 *    be reused by the driver.
 *  - (RTE_ETH_TX_DESC_UNAVAIL): Descriptor is unavailable, reserved by the
 *    driver or the hardware.
 *  - (-EINVAL) bad descriptor offset.
 *  - (-ENOTSUP) if the device does not support this function.
 *  - (-ENODEV) bad port or queue (only if compiled with debug).
 */
static inline int rte_eth_tx_descriptor_status(uint16_t port_id,
	uint16_t queue_id, uint16_t offset)
{
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_TX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		return -EINVAL;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->txq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_TX
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (qd == NULL)
		return -ENODEV;
#endif
	if (*p->tx_descriptor_status == NULL)
		return -ENOTSUP;
	return (*p->tx_descriptor_status)(qd, offset);
}

/**
 * @internal
 * Helper routine for rte_eth_tx_burst().
 * Should be called before entry PMD's rte_eth_tx_bulk implementation.
 * Does necessary pre-processing - invokes Tx callbacks if any, etc.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 * @param tx_pkts
 *   The address of an array of *nb_pkts* pointers to *rte_mbuf* structures
 *   which contain the output packets.
 * @param nb_pkts
 *   The maximum number of packets to transmit.
 * @return
 *   The number of output packets to transmit.
 */
uint16_t rte_eth_call_tx_callbacks(uint16_t port_id, uint16_t queue_id,
	struct rte_mbuf **tx_pkts, uint16_t nb_pkts, void *opaque);

/**
 * Send a burst of output packets on a transmit queue of an Ethernet device.
 *
 * The rte_eth_tx_burst() function is invoked to transmit output packets
 * on the output queue *queue_id* of the Ethernet device designated by its
 * *port_id*.
 * The *nb_pkts* parameter is the number of packets to send which are
 * supplied in the *tx_pkts* array of *rte_mbuf* structures, each of them
 * allocated from a pool created with rte_pktmbuf_pool_create().
 * The rte_eth_tx_burst() function loops, sending *nb_pkts* packets,
 * up to the number of transmit descriptors available in the Tx ring of the
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
 * When the number of free Tx descriptors drops below this threshold, the
 * rte_eth_tx_burst() function must [attempt to] free the *rte_mbuf*  buffers
 * of those packets whose transmission was effectively completed.
 *
 * If the PMD is RTE_ETH_TX_OFFLOAD_MT_LOCKFREE capable, multiple threads can
 * invoke this function concurrently on the same Tx queue without SW lock.
 * @see rte_eth_dev_info_get, struct rte_eth_txconf::offloads
 *
 * @see rte_eth_tx_prepare to perform some prior checks or adjustments
 * for offloads.
 *
 * @note This function must not modify mbufs (including packets data)
 * unless the refcnt is 1.
 * An exception is the bonding PMD, which does not have "Tx prepare" support,
 * in this case, mbufs may be modified.
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
static inline uint16_t
rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_TX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		return 0;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->txq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_TX
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);

	if (qd == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u for port_id=%u\n",
			queue_id, port_id);
		return 0;
	}
#endif

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
	{
		void *cb;

		/* __ATOMIC_RELEASE memory order was used when the
		 * call back was inserted into the list.
		 * Since there is a clear dependency between loading
		 * cb and cb->fn/cb->next, __ATOMIC_ACQUIRE memory order is
		 * not required.
		 */
		cb = __atomic_load_n((void **)&p->txq.clbk[queue_id],
				__ATOMIC_RELAXED);
		if (unlikely(cb != NULL))
			nb_pkts = rte_eth_call_tx_callbacks(port_id, queue_id,
					tx_pkts, nb_pkts, cb);
	}
#endif

	nb_pkts = p->tx_pkt_burst(qd, tx_pkts, nb_pkts);

	rte_ethdev_trace_tx_burst(port_id, queue_id, (void **)tx_pkts, nb_pkts);
	return nb_pkts;
}

/**
 * Process a burst of output packets on a transmit queue of an Ethernet device.
 *
 * The rte_eth_tx_prepare() function is invoked to prepare output packets to be
 * transmitted on the output queue *queue_id* of the Ethernet device designated
 * by its *port_id*.
 * The *nb_pkts* parameter is the number of packets to be prepared which are
 * supplied in the *tx_pkts* array of *rte_mbuf* structures, each of them
 * allocated from a pool created with rte_pktmbuf_pool_create().
 * For each packet to send, the rte_eth_tx_prepare() function performs
 * the following operations:
 *
 * - Check if packet meets devices requirements for Tx offloads.
 *
 * - Check limitations about number of segments.
 *
 * - Check additional requirements when debug is enabled.
 *
 * - Update and/or reset required checksums when Tx offload is set for packet.
 *
 * Since this function can modify packet data, provided mbufs must be safely
 * writable (e.g. modified data cannot be in shared segment).
 *
 * The rte_eth_tx_prepare() function returns the number of packets ready to be
 * sent. A return value equal to *nb_pkts* means that all packets are valid and
 * ready to be sent, otherwise stops processing on the first invalid packet and
 * leaves the rest packets untouched.
 *
 * When this functionality is not implemented in the driver, all packets are
 * are returned untouched.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 *   The value must be a valid port ID.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param tx_pkts
 *   The address of an array of *nb_pkts* pointers to *rte_mbuf* structures
 *   which contain the output packets.
 * @param nb_pkts
 *   The maximum number of packets to process.
 * @return
 *   The number of packets correct and ready to be sent. The return value can be
 *   less than the value of the *tx_pkts* parameter when some packet doesn't
 *   meet devices requirements with rte_errno set appropriately:
 *   - EINVAL: offload flags are not correctly set
 *   - ENOTSUP: the offload feature is not supported by the hardware
 *   - ENODEV: if *port_id* is invalid (with debug enabled only)
 */

#ifndef RTE_ETHDEV_TX_PREPARE_NOOP

static inline uint16_t
rte_eth_tx_prepare(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_fp_ops *p;
	void *qd;

#ifdef RTE_ETHDEV_DEBUG_TX
	if (port_id >= RTE_MAX_ETHPORTS ||
			queue_id >= RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid port_id=%u or queue_id=%u\n",
			port_id, queue_id);
		rte_errno = ENODEV;
		return 0;
	}
#endif

	/* fetch pointer to queue data */
	p = &rte_eth_fp_ops[port_id];
	qd = p->txq.data[queue_id];

#ifdef RTE_ETHDEV_DEBUG_TX
	if (!rte_eth_dev_is_valid_port(port_id)) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx port_id=%u\n", port_id);
		rte_errno = ENODEV;
		return 0;
	}
	if (qd == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u for port_id=%u\n",
			queue_id, port_id);
		rte_errno = EINVAL;
		return 0;
	}
#endif

	if (!p->tx_pkt_prepare)
		return nb_pkts;

	return p->tx_pkt_prepare(qd, tx_pkts, nb_pkts);
}

#else

/*
 * Native NOOP operation for compilation targets which doesn't require any
 * preparations steps, and functional NOOP may introduce unnecessary performance
 * drop.
 *
 * Generally this is not a good idea to turn it on globally and didn't should
 * be used if behavior of tx_preparation can change.
 */

static inline uint16_t
rte_eth_tx_prepare(__rte_unused uint16_t port_id,
		__rte_unused uint16_t queue_id,
		__rte_unused struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return nb_pkts;
}

#endif

/**
 * Send any packets queued up for transmission on a port and HW queue
 *
 * This causes an explicit flush of packets previously buffered via the
 * rte_eth_tx_buffer() function. It returns the number of packets successfully
 * sent to the NIC, and calls the error callback for any unsent packets. Unless
 * explicitly set up otherwise, the default callback simply frees the unsent
 * packets back to the owning mempool.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param buffer
 *   Buffer of packets to be transmit.
 * @return
 *   The number of packets successfully sent to the Ethernet device. The error
 *   callback is called for any packets which could not be sent.
 */
static inline uint16_t
rte_eth_tx_buffer_flush(uint16_t port_id, uint16_t queue_id,
		struct rte_eth_dev_tx_buffer *buffer)
{
	uint16_t sent;
	uint16_t to_send = buffer->length;

	if (to_send == 0)
		return 0;

	sent = rte_eth_tx_burst(port_id, queue_id, buffer->pkts, to_send);

	buffer->length = 0;

	/* All packets sent, or to be dealt with by callback below */
	if (unlikely(sent != to_send))
		buffer->error_callback(&buffer->pkts[sent],
				       (uint16_t)(to_send - sent),
				       buffer->error_userdata);

	return sent;
}

/**
 * Buffer a single packet for future transmission on a port and queue
 *
 * This function takes a single mbuf/packet and buffers it for later
 * transmission on the particular port and queue specified. Once the buffer is
 * full of packets, an attempt will be made to transmit all the buffered
 * packets. In case of error, where not all packets can be transmitted, a
 * callback is called with the unsent packets as a parameter. If no callback
 * is explicitly set up, the unsent packets are just freed back to the owning
 * mempool. The function returns the number of packets actually sent i.e.
 * 0 if no buffer flush occurred, otherwise the number of packets successfully
 * flushed
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The index of the transmit queue through which output packets must be
 *   sent.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @param buffer
 *   Buffer used to collect packets to be sent.
 * @param tx_pkt
 *   Pointer to the packet mbuf to be sent.
 * @return
 *   0 = packet has been buffered for later transmission
 *   N > 0 = packet has been buffered, and the buffer was subsequently flushed,
 *     causing N packets to be sent, and the error callback to be called for
 *     the rest.
 */
static __rte_always_inline uint16_t
rte_eth_tx_buffer(uint16_t port_id, uint16_t queue_id,
		struct rte_eth_dev_tx_buffer *buffer, struct rte_mbuf *tx_pkt)
{
	buffer->pkts[buffer->length++] = tx_pkt;
	if (buffer->length < buffer->size)
		return 0;

	return rte_eth_tx_buffer_flush(port_id, queue_id, buffer);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Get supported header protocols to split on Rx.
 *
 * When a packet type is announced to be split,
 * it *must* be supported by the PMD.
 * For instance, if eth-ipv4, eth-ipv4-udp is announced,
 * the PMD must return the following packet types for these packets:
 * - Ether/IPv4             -> RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
 * - Ether/IPv4/UDP         -> RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP
 *
 * @param port_id
 *   The port identifier of the device.
 * @param[out] ptypes
 *   An array pointer to store supported protocol headers, allocated by caller.
 *   These ptypes are composed with RTE_PTYPE_*.
 * @param num
 *   Size of the array pointed by param ptypes.
 * @return
 *   - (>=0) Number of supported ptypes. If the number of types exceeds num,
 *           only num entries will be filled into the ptypes array,
 *           but the full count of supported ptypes will be returned.
 *   - (-ENOTSUP) if header protocol is not supported by device.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
__rte_experimental
int rte_eth_buffer_split_get_supported_hdr_ptypes(uint16_t port_id, uint32_t *ptypes, int num);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_H_ */
