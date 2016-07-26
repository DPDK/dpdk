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
 * again. The transmit and receive functions should not be invoked when the
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
 *     - NIC queue statistics mappings
 *
 * Any other configuration will not be stored and will need to be re-entered
 * after a call to rte_eth_dev_start().
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

#include <rte_dev.h>

/* Use this macro to check if LRO API is supported */
#define RTE_ETHDEV_HAS_LRO_SUPPORT

#include <rte_log.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include "rte_ether.h"
#include "rte_eth_ctrl.h"
#include "rte_dev_info.h"

struct rte_mbuf;

/**
 * A structure used to retrieve statistics for an Ethernet port.
 */
struct rte_eth_stats {
	uint64_t ipackets;  /**< Total number of successfully received packets. */
	uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
	uint64_t ibytes;    /**< Total number of successfully received bytes. */
	uint64_t obytes;    /**< Total number of successfully transmitted bytes. */
	uint64_t imissed;
	/**< Total of RX packets dropped by the HW,
	 * because there are no available mbufs (i.e. RX queues are full).
	 */
	uint64_t ierrors;   /**< Total number of erroneous received packets. */
	uint64_t oerrors;   /**< Total number of failed transmitted packets. */
	uint64_t rx_nombuf; /**< Total number of RX mbuf allocation failures. */
	uint64_t q_ipackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/**< Total number of queue RX packets. */
	uint64_t q_opackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/**< Total number of queue TX packets. */
	uint64_t q_ibytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/**< Total number of successfully received queue bytes. */
	uint64_t q_obytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/**< Total number of successfully transmitted queue bytes. */
	uint64_t q_errors[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	/**< Total number of queue packets received that are dropped. */
};

/**
 * Device supported speeds bitmap flags
 */
#define ETH_LINK_SPEED_AUTONEG  (0 <<  0)  /**< Autonegotiate (all speeds) */
#define ETH_LINK_SPEED_FIXED    (1 <<  0)  /**< Disable autoneg (fixed speed) */
#define ETH_LINK_SPEED_10M_HD   (1 <<  1)  /**<  10 Mbps half-duplex */
#define ETH_LINK_SPEED_10M      (1 <<  2)  /**<  10 Mbps full-duplex */
#define ETH_LINK_SPEED_100M_HD  (1 <<  3)  /**< 100 Mbps half-duplex */
#define ETH_LINK_SPEED_100M     (1 <<  4)  /**< 100 Mbps full-duplex */
#define ETH_LINK_SPEED_1G       (1 <<  5)  /**<   1 Gbps */
#define ETH_LINK_SPEED_2_5G     (1 <<  6)  /**< 2.5 Gbps */
#define ETH_LINK_SPEED_5G       (1 <<  7)  /**<   5 Gbps */
#define ETH_LINK_SPEED_10G      (1 <<  8)  /**<  10 Gbps */
#define ETH_LINK_SPEED_20G      (1 <<  9)  /**<  20 Gbps */
#define ETH_LINK_SPEED_25G      (1 << 10)  /**<  25 Gbps */
#define ETH_LINK_SPEED_40G      (1 << 11)  /**<  40 Gbps */
#define ETH_LINK_SPEED_50G      (1 << 12)  /**<  50 Gbps */
#define ETH_LINK_SPEED_56G      (1 << 13)  /**<  56 Gbps */
#define ETH_LINK_SPEED_100G     (1 << 14)  /**< 100 Gbps */

/**
 * Ethernet numeric link speeds in Mbps
 */
#define ETH_SPEED_NUM_NONE         0 /**< Not defined */
#define ETH_SPEED_NUM_10M         10 /**<  10 Mbps */
#define ETH_SPEED_NUM_100M       100 /**< 100 Mbps */
#define ETH_SPEED_NUM_1G        1000 /**<   1 Gbps */
#define ETH_SPEED_NUM_2_5G      2500 /**< 2.5 Gbps */
#define ETH_SPEED_NUM_5G        5000 /**<   5 Gbps */
#define ETH_SPEED_NUM_10G      10000 /**<  10 Gbps */
#define ETH_SPEED_NUM_20G      20000 /**<  20 Gbps */
#define ETH_SPEED_NUM_25G      25000 /**<  25 Gbps */
#define ETH_SPEED_NUM_40G      40000 /**<  40 Gbps */
#define ETH_SPEED_NUM_50G      50000 /**<  50 Gbps */
#define ETH_SPEED_NUM_56G      56000 /**<  56 Gbps */
#define ETH_SPEED_NUM_100G    100000 /**< 100 Gbps */

/**
 * A structure used to retrieve link-level information of an Ethernet port.
 */
struct rte_eth_link {
	uint32_t link_speed;        /**< ETH_SPEED_NUM_ */
	uint16_t link_duplex  : 1;  /**< ETH_LINK_[HALF/FULL]_DUPLEX */
	uint16_t link_autoneg : 1;  /**< ETH_LINK_SPEED_[AUTONEG/FIXED] */
	uint16_t link_status  : 1;  /**< ETH_LINK_[DOWN/UP] */
} __attribute__((aligned(8)));      /**< aligned for atomic64 read/write */

/* Utility constants */
#define ETH_LINK_HALF_DUPLEX    0 /**< Half-duplex connection. */
#define ETH_LINK_FULL_DUPLEX    1 /**< Full-duplex connection. */
#define ETH_LINK_DOWN           0 /**< Link is down. */
#define ETH_LINK_UP             1 /**< Link is up. */
#define ETH_LINK_FIXED          0 /**< No autonegotiation. */
#define ETH_LINK_AUTONEG        1 /**< Autonegotiated. */

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
 *  Simple flags are used for rte_eth_conf.rxmode.mq_mode.
 */
#define ETH_MQ_RX_RSS_FLAG  0x1
#define ETH_MQ_RX_DCB_FLAG  0x2
#define ETH_MQ_RX_VMDQ_FLAG 0x4

/**
 *  A set of values to identify what method is to be used to route
 *  packets to multiple queues.
 */
enum rte_eth_rx_mq_mode {
	/** None of DCB,RSS or VMDQ mode */
	ETH_MQ_RX_NONE = 0,

	/** For RX side, only RSS is on */
	ETH_MQ_RX_RSS = ETH_MQ_RX_RSS_FLAG,
	/** For RX side,only DCB is on. */
	ETH_MQ_RX_DCB = ETH_MQ_RX_DCB_FLAG,
	/** Both DCB and RSS enable */
	ETH_MQ_RX_DCB_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_DCB_FLAG,

	/** Only VMDQ, no RSS nor DCB */
	ETH_MQ_RX_VMDQ_ONLY = ETH_MQ_RX_VMDQ_FLAG,
	/** RSS mode with VMDQ */
	ETH_MQ_RX_VMDQ_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_VMDQ_FLAG,
	/** Use VMDQ+DCB to route traffic to queues */
	ETH_MQ_RX_VMDQ_DCB = ETH_MQ_RX_VMDQ_FLAG | ETH_MQ_RX_DCB_FLAG,
	/** Enable both VMDQ and DCB in VMDq */
	ETH_MQ_RX_VMDQ_DCB_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_DCB_FLAG |
				 ETH_MQ_RX_VMDQ_FLAG,
};

/**
 * for rx mq mode backward compatible
 */
#define ETH_RSS                       ETH_MQ_RX_RSS
#define VMDQ_DCB                      ETH_MQ_RX_VMDQ_DCB
#define ETH_DCB_RX                    ETH_MQ_RX_DCB

/**
 * A set of values to identify what method is to be used to transmit
 * packets using multi-TCs.
 */
enum rte_eth_tx_mq_mode {
	ETH_MQ_TX_NONE    = 0,  /**< It is in neither DCB nor VT mode. */
	ETH_MQ_TX_DCB,          /**< For TX side,only DCB is on. */
	ETH_MQ_TX_VMDQ_DCB,	/**< For TX side,both DCB and VT is on. */
	ETH_MQ_TX_VMDQ_ONLY,    /**< Only VT on, no DCB */
};

/**
 * for tx mq mode backward compatible
 */
#define ETH_DCB_NONE                ETH_MQ_TX_NONE
#define ETH_VMDQ_DCB_TX             ETH_MQ_TX_VMDQ_DCB
#define ETH_DCB_TX                  ETH_MQ_TX_DCB

/**
 * A structure used to configure the RX features of an Ethernet port.
 */
struct rte_eth_rxmode {
	/** The multi-queue packet distribution mode to be used, e.g. RSS. */
	enum rte_eth_rx_mq_mode mq_mode;
	uint32_t max_rx_pkt_len;  /**< Only used if jumbo_frame enabled. */
	uint16_t split_hdr_size;  /**< hdr buf size (header_split enabled).*/
	uint16_t header_split : 1, /**< Header Split enable. */
		hw_ip_checksum   : 1, /**< IP/UDP/TCP checksum offload enable. */
		hw_vlan_filter   : 1, /**< VLAN filter enable. */
		hw_vlan_strip    : 1, /**< VLAN strip enable. */
		hw_vlan_extend   : 1, /**< Extended VLAN enable. */
		jumbo_frame      : 1, /**< Jumbo Frame Receipt enable. */
		hw_strip_crc     : 1, /**< Enable CRC stripping by hardware. */
		enable_scatter   : 1, /**< Enable scatter packets rx handler */
		enable_lro       : 1; /**< Enable LRO */
};

/**
 * VLAN types to indicate if it is for single VLAN, inner VLAN or outer VLAN.
 * Note that single VLAN is treated the same as inner VLAN.
 */
enum rte_vlan_type {
	ETH_VLAN_TYPE_UNKNOWN = 0,
	ETH_VLAN_TYPE_INNER, /**< Inner VLAN. */
	ETH_VLAN_TYPE_OUTER, /**< Single VLAN, or outer VLAN. */
	ETH_VLAN_TYPE_MAX,
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
 * The RSS offload types are defined based on flow types which are defined
 * in rte_eth_ctrl.h. Different NIC hardwares may support different RSS offload
 * types. The supported flow types or RSS offload types can be queried by
 * rte_eth_dev_info_get().
 */
#define ETH_RSS_IPV4               (1ULL << RTE_ETH_FLOW_IPV4)
#define ETH_RSS_FRAG_IPV4          (1ULL << RTE_ETH_FLOW_FRAG_IPV4)
#define ETH_RSS_NONFRAG_IPV4_TCP   (1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_TCP)
#define ETH_RSS_NONFRAG_IPV4_UDP   (1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_UDP)
#define ETH_RSS_NONFRAG_IPV4_SCTP  (1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP)
#define ETH_RSS_NONFRAG_IPV4_OTHER (1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER)
#define ETH_RSS_IPV6               (1ULL << RTE_ETH_FLOW_IPV6)
#define ETH_RSS_FRAG_IPV6          (1ULL << RTE_ETH_FLOW_FRAG_IPV6)
#define ETH_RSS_NONFRAG_IPV6_TCP   (1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_TCP)
#define ETH_RSS_NONFRAG_IPV6_UDP   (1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_UDP)
#define ETH_RSS_NONFRAG_IPV6_SCTP  (1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP)
#define ETH_RSS_NONFRAG_IPV6_OTHER (1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER)
#define ETH_RSS_L2_PAYLOAD         (1ULL << RTE_ETH_FLOW_L2_PAYLOAD)
#define ETH_RSS_IPV6_EX            (1ULL << RTE_ETH_FLOW_IPV6_EX)
#define ETH_RSS_IPV6_TCP_EX        (1ULL << RTE_ETH_FLOW_IPV6_TCP_EX)
#define ETH_RSS_IPV6_UDP_EX        (1ULL << RTE_ETH_FLOW_IPV6_UDP_EX)
#define ETH_RSS_PORT               (1ULL << RTE_ETH_FLOW_PORT)
#define ETH_RSS_VXLAN              (1ULL << RTE_ETH_FLOW_VXLAN)
#define ETH_RSS_GENEVE             (1ULL << RTE_ETH_FLOW_GENEVE)
#define ETH_RSS_NVGRE              (1ULL << RTE_ETH_FLOW_NVGRE)

#define ETH_RSS_IP ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_OTHER | \
	ETH_RSS_IPV6 | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_OTHER | \
	ETH_RSS_IPV6_EX)

#define ETH_RSS_UDP ( \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_UDP_EX)

#define ETH_RSS_TCP ( \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_IPV6_TCP_EX)

#define ETH_RSS_SCTP ( \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV6_SCTP)

#define ETH_RSS_TUNNEL ( \
	ETH_RSS_VXLAN  | \
	ETH_RSS_GENEVE | \
	ETH_RSS_NVGRE)


/**< Mask of valid RSS hash protocols */
#define ETH_RSS_PROTO_MASK ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV4_OTHER | \
	ETH_RSS_IPV6 | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_NONFRAG_IPV6_SCTP | \
	ETH_RSS_NONFRAG_IPV6_OTHER | \
	ETH_RSS_L2_PAYLOAD | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX | \
	ETH_RSS_PORT  | \
	ETH_RSS_VXLAN | \
	ETH_RSS_GENEVE | \
	ETH_RSS_NVGRE)

/*
 * Definitions used for redirection table entry size.
 * Some RSS RETA sizes may not be supported by some drivers, check the
 * documentation or the description of relevant functions for more details.
 */
#define ETH_RSS_RETA_SIZE_64  64
#define ETH_RSS_RETA_SIZE_128 128
#define ETH_RSS_RETA_SIZE_256 256
#define ETH_RSS_RETA_SIZE_512 512
#define RTE_RETA_GROUP_SIZE   64

/* Definitions used for VMDQ and DCB functionality */
#define ETH_VMDQ_MAX_VLAN_FILTERS   64 /**< Maximum nb. of VMDQ vlan filters. */
#define ETH_DCB_NUM_USER_PRIORITIES 8  /**< Maximum nb. of DCB priorities. */
#define ETH_VMDQ_DCB_NUM_QUEUES     128 /**< Maximum nb. of VMDQ DCB queues. */
#define ETH_DCB_NUM_QUEUES          128 /**< Maximum nb. of DCB queues. */

/* DCB capability defines */
#define ETH_DCB_PG_SUPPORT      0x00000001 /**< Priority Group(ETS) support. */
#define ETH_DCB_PFC_SUPPORT     0x00000002 /**< Priority Flow Control support. */

/* Definitions used for VLAN Offload functionality */
#define ETH_VLAN_STRIP_OFFLOAD   0x0001 /**< VLAN Strip  On/Off */
#define ETH_VLAN_FILTER_OFFLOAD  0x0002 /**< VLAN Filter On/Off */
#define ETH_VLAN_EXTEND_OFFLOAD  0x0004 /**< VLAN Extend On/Off */

/* Definitions used for mask VLAN setting */
#define ETH_VLAN_STRIP_MASK   0x0001 /**< VLAN Strip  setting mask */
#define ETH_VLAN_FILTER_MASK  0x0002 /**< VLAN Filter  setting mask*/
#define ETH_VLAN_EXTEND_MASK  0x0004 /**< VLAN Extend  setting mask*/
#define ETH_VLAN_ID_MAX       0x0FFF /**< VLAN ID is in lower 12 bits*/

/* Definitions used for receive MAC address   */
#define ETH_NUM_RECEIVE_MAC_ADDR  128 /**< Maximum nb. of receive mac addr. */

/* Definitions used for unicast hash  */
#define ETH_VMDQ_NUM_UC_HASH_ARRAY  128 /**< Maximum nb. of UC hash array. */

/* Definitions used for VMDQ pool rx mode setting */
#define ETH_VMDQ_ACCEPT_UNTAG   0x0001 /**< accept untagged packets. */
#define ETH_VMDQ_ACCEPT_HASH_MC 0x0002 /**< accept packets in multicast table . */
#define ETH_VMDQ_ACCEPT_HASH_UC 0x0004 /**< accept packets in unicast table. */
#define ETH_VMDQ_ACCEPT_BROADCAST   0x0008 /**< accept broadcast packets. */
#define ETH_VMDQ_ACCEPT_MULTICAST   0x0010 /**< multicast promiscuous. */

/** Maximum nb. of vlan per mirror rule */
#define ETH_MIRROR_MAX_VLANS       64

#define ETH_MIRROR_VIRTUAL_POOL_UP     0x01  /**< Virtual Pool uplink Mirroring. */
#define ETH_MIRROR_UPLINK_PORT         0x02  /**< Uplink Port Mirroring. */
#define ETH_MIRROR_DOWNLINK_PORT       0x04  /**< Downlink Port Mirroring. */
#define ETH_MIRROR_VLAN                0x08  /**< VLAN Mirroring. */
#define ETH_MIRROR_VIRTUAL_POOL_DOWN   0x10  /**< Virtual Pool downlink Mirroring. */

/**
 * A structure used to configure VLAN traffic mirror of an Ethernet port.
 */
struct rte_eth_vlan_mirror {
	uint64_t vlan_mask; /**< mask for valid VLAN ID. */
	/** VLAN ID list for vlan mirroring. */
	uint16_t vlan_id[ETH_MIRROR_MAX_VLANS];
};

/**
 * A structure used to configure traffic mirror of an Ethernet port.
 */
struct rte_eth_mirror_conf {
	uint8_t rule_type; /**< Mirroring rule type */
	uint8_t dst_pool;  /**< Destination pool for this mirror rule. */
	uint64_t pool_mask; /**< Bitmap of pool for pool mirroring */
	/** VLAN ID setting for VLAN mirroring. */
	struct rte_eth_vlan_mirror vlan;
};

/**
 * A structure used to configure 64 entries of Redirection Table of the
 * Receive Side Scaling (RSS) feature of an Ethernet port. To configure
 * more than 64 entries supported by hardware, an array of this structure
 * is needed.
 */
struct rte_eth_rss_reta_entry64 {
	uint64_t mask;
	/**< Mask bits indicate which entries need to be updated/queried. */
	uint16_t reta[RTE_RETA_GROUP_SIZE];
	/**< Group of 64 redirection table entries. */
};

/**
 * This enum indicates the possible number of traffic classes
 * in DCB configratioins
 */
enum rte_eth_nb_tcs {
	ETH_4_TCS = 4, /**< 4 TCs with DCB. */
	ETH_8_TCS = 8  /**< 8 TCs with DCB. */
};

/**
 * This enum indicates the possible number of queue pools
 * in VMDQ configurations.
 */
enum rte_eth_nb_pools {
	ETH_8_POOLS = 8,    /**< 8 VMDq pools. */
	ETH_16_POOLS = 16,  /**< 16 VMDq pools. */
	ETH_32_POOLS = 32,  /**< 32 VMDq pools. */
	ETH_64_POOLS = 64   /**< 64 VMDq pools. */
};

/* This structure may be extended in future. */
struct rte_eth_dcb_rx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_vmdq_dcb_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools. */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_dcb_tx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs. */
	/** Traffic class each UP mapped to. */
	uint8_t dcb_tc[ETH_DCB_NUM_USER_PRIORITIES];
};

struct rte_eth_vmdq_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq mode, 64 pools. */
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
	uint8_t dcb_tc[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Selects a queue in a pool */
};

struct rte_eth_vmdq_rx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq only mode, 8 or 64 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t enable_loop_back; /**< Enable VT loop back */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	uint32_t rx_mode; /**< Flags from ETH_VMDQ_ACCEPT_* */
	struct {
		uint16_t vlan_id; /**< The vlan id of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet rx */
	} pool_map[ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq vlan pool maps. */
};

/**
 * A structure used to configure the TX features of an Ethernet port.
 */
struct rte_eth_txmode {
	enum rte_eth_tx_mq_mode mq_mode; /**< TX multi-queues mode. */

	/* For i40e specifically */
	uint16_t pvid;
	uint8_t hw_vlan_reject_tagged : 1,
		/**< If set, reject sending out tagged pkts */
		hw_vlan_reject_untagged : 1,
		/**< If set, reject sending out untagged pkts */
		hw_vlan_insert_pvid : 1;
		/**< If set, enable port based VLAN insertion */
};

/**
 * A structure used to configure an RX ring of an Ethernet port.
 */
struct rte_eth_rxconf {
	struct rte_eth_thresh rx_thresh; /**< RX ring threshold registers. */
	uint16_t rx_free_thresh; /**< Drives the freeing of RX descriptors. */
	uint8_t rx_drop_en; /**< Drop packets if no descriptors are available. */
	uint8_t rx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
};

#define ETH_TXQ_FLAGS_NOMULTSEGS 0x0001 /**< nb_segs=1 for all mbufs */
#define ETH_TXQ_FLAGS_NOREFCOUNT 0x0002 /**< refcnt can be ignored */
#define ETH_TXQ_FLAGS_NOMULTMEMP 0x0004 /**< all bufs come from same mempool */
#define ETH_TXQ_FLAGS_NOVLANOFFL 0x0100 /**< disable VLAN offload */
#define ETH_TXQ_FLAGS_NOXSUMSCTP 0x0200 /**< disable SCTP checksum offload */
#define ETH_TXQ_FLAGS_NOXSUMUDP  0x0400 /**< disable UDP checksum offload */
#define ETH_TXQ_FLAGS_NOXSUMTCP  0x0800 /**< disable TCP checksum offload */
#define ETH_TXQ_FLAGS_NOOFFLOADS \
		(ETH_TXQ_FLAGS_NOVLANOFFL | ETH_TXQ_FLAGS_NOXSUMSCTP | \
		 ETH_TXQ_FLAGS_NOXSUMUDP  | ETH_TXQ_FLAGS_NOXSUMTCP)
#define ETH_TXQ_FLAGS_NOXSUMS \
		(ETH_TXQ_FLAGS_NOXSUMSCTP | ETH_TXQ_FLAGS_NOXSUMUDP | \
		 ETH_TXQ_FLAGS_NOXSUMTCP)
/**
 * A structure used to configure a TX ring of an Ethernet port.
 */
struct rte_eth_txconf {
	struct rte_eth_thresh tx_thresh; /**< TX ring threshold registers. */
	uint16_t tx_rs_thresh; /**< Drives the setting of RS bit on TXDs. */
	uint16_t tx_free_thresh; /**< Start freeing TX buffers if there are
				      less free descriptors than this value. */

	uint32_t txq_flags; /**< Set flags for the Tx queue */
	uint8_t tx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
};

/**
 * A structure contains information about HW descriptor ring limitations.
 */
struct rte_eth_desc_lim {
	uint16_t nb_max;   /**< Max allowed number of descriptors. */
	uint16_t nb_min;   /**< Min allowed number of descriptors. */
	uint16_t nb_align; /**< Number of descriptors should be aligned to. */
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
	/** RX queue of packets matching a "drop" filter in perfect mode. */
	uint8_t drop_queue;
	struct rte_eth_fdir_masks mask;
	struct rte_eth_fdir_flex_conf flex_conf;
	/**< Flex payload configuration. */
};

/**
 * UDP tunneling configuration.
 * Used to config the UDP port for a type of tunnel.
 * NICs need the UDP port to identify the tunnel type.
 * Normally a type of tunnel has a default UDP port, this structure can be used
 * in case if the users want to change or support more UDP port.
 */
struct rte_eth_udp_tunnel {
	uint16_t udp_port; /**< UDP port used for the tunnel. */
	uint8_t prot_type; /**< Tunnel type. Defined in rte_eth_tunnel_type. */
};

/**
 * A structure used to enable/disable specific device interrupts.
 */
struct rte_intr_conf {
	/** enable/disable lsc interrupt. 0 (default) - disable, 1 enable */
	uint16_t lsc;
	/** enable/disable rxq interrupt. 0 (default) - disable, 1 enable */
	uint16_t rxq;
};

/**
 * A structure used to configure an Ethernet port.
 * Depending upon the RX multi-queue mode, extra advanced
 * configuration settings may be needed.
 */
struct rte_eth_conf {
	uint32_t link_speeds; /**< bitmap of ETH_LINK_SPEED_XXX of speeds to be
				used. ETH_LINK_SPEED_FIXED disables link
				autonegotiation, and a unique speed shall be
				set. Otherwise, the bitmap defines the set of
				speeds to be advertised. If the special value
				ETH_LINK_SPEED_AUTONEG (0) is used, all speeds
				supported are advertised. */
	struct rte_eth_rxmode rxmode; /**< Port RX configuration. */
	struct rte_eth_txmode txmode; /**< Port TX configuration. */
	uint32_t lpbk_mode; /**< Loopback operation mode. By default the value
			         is 0, meaning the loopback mode is disabled.
				 Read the datasheet of given ethernet controller
				 for details. The possible values of this field
				 are defined in implementation of each driver. */
	struct {
		struct rte_eth_rss_conf rss_conf; /**< Port RSS configuration */
		struct rte_eth_vmdq_dcb_conf vmdq_dcb_conf;
		/**< Port vmdq+dcb configuration. */
		struct rte_eth_dcb_rx_conf dcb_rx_conf;
		/**< Port dcb RX configuration. */
		struct rte_eth_vmdq_rx_conf vmdq_rx_conf;
		/**< Port vmdq RX configuration. */
	} rx_adv_conf; /**< Port RX filtering configuration (union). */
	union {
		struct rte_eth_vmdq_dcb_tx_conf vmdq_dcb_tx_conf;
		/**< Port vmdq+dcb TX configuration. */
		struct rte_eth_dcb_tx_conf dcb_tx_conf;
		/**< Port dcb TX configuration. */
		struct rte_eth_vmdq_tx_conf vmdq_tx_conf;
		/**< Port vmdq TX configuration. */
	} tx_adv_conf; /**< Port TX DCB configuration (union). */
	/** Currently,Priority Flow Control(PFC) are supported,if DCB with PFC
	    is needed,and the variable must be set ETH_DCB_PFC_SUPPORT. */
	uint32_t dcb_capability_en;
	struct rte_fdir_conf fdir_conf; /**< FDIR configuration. */
	struct rte_intr_conf intr_conf; /**< Interrupt mode configuration. */
};

/**
 * A structure used to retrieve the contextual information of
 * an Ethernet device, such as the controlling driver of the device,
 * its PCI context, etc...
 */

/**
 * RX offload capabilities of a device.
 */
#define DEV_RX_OFFLOAD_VLAN_STRIP  0x00000001
#define DEV_RX_OFFLOAD_IPV4_CKSUM  0x00000002
#define DEV_RX_OFFLOAD_UDP_CKSUM   0x00000004
#define DEV_RX_OFFLOAD_TCP_CKSUM   0x00000008
#define DEV_RX_OFFLOAD_TCP_LRO     0x00000010
#define DEV_RX_OFFLOAD_QINQ_STRIP  0x00000020
#define DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM 0x00000040

/**
 * TX offload capabilities of a device.
 */
#define DEV_TX_OFFLOAD_VLAN_INSERT 0x00000001
#define DEV_TX_OFFLOAD_IPV4_CKSUM  0x00000002
#define DEV_TX_OFFLOAD_UDP_CKSUM   0x00000004
#define DEV_TX_OFFLOAD_TCP_CKSUM   0x00000008
#define DEV_TX_OFFLOAD_SCTP_CKSUM  0x00000010
#define DEV_TX_OFFLOAD_TCP_TSO     0x00000020
#define DEV_TX_OFFLOAD_UDP_TSO     0x00000040
#define DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM 0x00000080 /**< Used for tunneling packet. */
#define DEV_TX_OFFLOAD_QINQ_INSERT 0x00000100

/**
 * Ethernet device information
 */
struct rte_eth_dev_info {
	struct rte_pci_device *pci_dev; /**< Device PCI information. */
	const char *driver_name; /**< Device Driver name. */
	unsigned int if_index; /**< Index to bound host interface, or 0 if none.
		Use if_indextoname() to translate into an interface name. */
	uint32_t min_rx_bufsize; /**< Minimum size of RX buffer. */
	uint32_t max_rx_pktlen; /**< Maximum configurable length of RX pkt. */
	uint16_t max_rx_queues; /**< Maximum number of RX queues. */
	uint16_t max_tx_queues; /**< Maximum number of TX queues. */
	uint32_t max_mac_addrs; /**< Maximum number of MAC addresses. */
	uint32_t max_hash_mac_addrs;
	/** Maximum number of hash MAC addresses for MTA and UTA. */
	uint16_t max_vfs; /**< Maximum number of VFs. */
	uint16_t max_vmdq_pools; /**< Maximum number of VMDq pools. */
	uint32_t rx_offload_capa; /**< Device RX offload capabilities. */
	uint32_t tx_offload_capa; /**< Device TX offload capabilities. */
	uint16_t reta_size;
	/**< Device redirection table size, the total number of entries. */
	uint8_t hash_key_size; /**< Hash key size in bytes */
	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;
	struct rte_eth_rxconf default_rxconf; /**< Default RX configuration */
	struct rte_eth_txconf default_txconf; /**< Default TX configuration */
	uint16_t vmdq_queue_base; /**< First queue ID for VMDQ pools. */
	uint16_t vmdq_queue_num;  /**< Queue number for VMDQ pools. */
	uint16_t vmdq_pool_base;  /**< First ID of VMDQ pools. */
	struct rte_eth_desc_lim rx_desc_lim;  /**< RX descriptors limits */
	struct rte_eth_desc_lim tx_desc_lim;  /**< TX descriptors limits */
	uint32_t speed_capa;  /**< Supported speeds bitmap (ETH_LINK_SPEED_). */
	/** Configured number of rx/tx queues */
	uint16_t nb_rx_queues; /**< Number of RX queues. */
	uint16_t nb_tx_queues; /**< Number of TX queues. */
};

/**
 * Ethernet device RX queue information structure.
 * Used to retieve information about configured queue.
 */
struct rte_eth_rxq_info {
	struct rte_mempool *mp;     /**< mempool used by that queue. */
	struct rte_eth_rxconf conf; /**< queue config parameters. */
	uint8_t scattered_rx;       /**< scattered packets RX supported. */
	uint16_t nb_desc;           /**< configured number of RXDs. */
} __rte_cache_min_aligned;

/**
 * Ethernet device TX queue information structure.
 * Used to retieve information about configured queue.
 */
struct rte_eth_txq_info {
	struct rte_eth_txconf conf; /**< queue config parameters. */
	uint16_t nb_desc;           /**< configured number of TXDs. */
} __rte_cache_min_aligned;

/** Maximum name length for extended statistics counters */
#define RTE_ETH_XSTATS_NAME_SIZE 64

/**
 * An Ethernet device extended statistic structure
 *
 * This structure is used by ethdev->eth_xstats_get() to provide
 * statistics that are not provided in the generic rte_eth_stats
 * structure.
 */
struct rte_eth_xstat {
	uint64_t id;
	uint64_t value;
};

/**
 * A name-key lookup element for extended statistics.
 *
 * This structure is used to map between names and ID numbers
 * for extended ethernet statistics.
 */
struct rte_eth_xstat_name {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
};

#define ETH_DCB_NUM_TCS    8
#define ETH_MAX_VMDQ_POOL  64

/**
 * A structure used to get the information of queue and
 * TC mapping on both TX and RX paths.
 */
struct rte_eth_dcb_tc_queue_mapping {
	/** rx queues assigned to tc per Pool */
	struct {
		uint8_t base;
		uint8_t nb_queue;
	} tc_rxq[ETH_MAX_VMDQ_POOL][ETH_DCB_NUM_TCS];
	/** rx queues assigned to tc per Pool */
	struct {
		uint8_t base;
		uint8_t nb_queue;
	} tc_txq[ETH_MAX_VMDQ_POOL][ETH_DCB_NUM_TCS];
};

/**
 * A structure used to get the information of DCB.
 * It includes TC UP mapping and queue TC mapping.
 */
struct rte_eth_dcb_info {
	uint8_t nb_tcs;        /**< number of TCs */
	uint8_t prio_tc[ETH_DCB_NUM_USER_PRIORITIES]; /**< Priority to tc */
	uint8_t tc_bws[ETH_DCB_NUM_TCS]; /**< TX BW percentage for each TC */
	/** rx queues assigned to tc */
	struct rte_eth_dcb_tc_queue_mapping tc_queue;
};

/**
 * RX/TX queue states
 */
#define RTE_ETH_QUEUE_STATE_STOPPED 0
#define RTE_ETH_QUEUE_STATE_STARTED 1

struct rte_eth_dev;

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);


#ifdef RTE_LIBRTE_ETHDEV_DEBUG
#define RTE_PMD_DEBUG_TRACE(...) \
	rte_pmd_debug_trace(__func__, __VA_ARGS__)
#else
#define RTE_PMD_DEBUG_TRACE(...)
#endif


/* Macros to check for valid port */
#define RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, retval) do { \
	if (!rte_eth_dev_is_valid_port(port_id)) { \
		RTE_PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id); \
		return retval; \
	} \
} while (0)

#define RTE_ETH_VALID_PORTID_OR_RET(port_id) do { \
	if (!rte_eth_dev_is_valid_port(port_id)) { \
		RTE_PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id); \
		return; \
	} \
} while (0)

/**
 * l2 tunnel configuration.
 */

/**< l2 tunnel enable mask */
#define ETH_L2_TUNNEL_ENABLE_MASK       0x00000001
/**< l2 tunnel insertion mask */
#define ETH_L2_TUNNEL_INSERTION_MASK    0x00000002
/**< l2 tunnel stripping mask */
#define ETH_L2_TUNNEL_STRIPPING_MASK    0x00000004
/**< l2 tunnel forwarding mask */
#define ETH_L2_TUNNEL_FORWARDING_MASK   0x00000008

/*
 * Definitions of all functions exported by an Ethernet driver through the
 * the generic structure of type *eth_dev_ops* supplied in the *rte_eth_dev*
 * structure associated with an Ethernet device.
 */

typedef int  (*eth_dev_configure_t)(struct rte_eth_dev *dev);
/**< @internal Ethernet device configuration. */

typedef int  (*eth_dev_start_t)(struct rte_eth_dev *dev);
/**< @internal Function used to start a configured Ethernet device. */

typedef void (*eth_dev_stop_t)(struct rte_eth_dev *dev);
/**< @internal Function used to stop a configured Ethernet device. */

typedef int  (*eth_dev_set_link_up_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link up a configured Ethernet device. */

typedef int  (*eth_dev_set_link_down_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link down a configured Ethernet device. */

typedef void (*eth_dev_close_t)(struct rte_eth_dev *dev);
/**< @internal Function used to close a configured Ethernet device. */

typedef void (*eth_promiscuous_enable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to enable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_promiscuous_disable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to disable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_allmulticast_enable_t)(struct rte_eth_dev *dev);
/**< @internal Enable the receipt of all multicast packets by an Ethernet device. */

typedef void (*eth_allmulticast_disable_t)(struct rte_eth_dev *dev);
/**< @internal Disable the receipt of all multicast packets by an Ethernet device. */

typedef int (*eth_link_update_t)(struct rte_eth_dev *dev,
				int wait_to_complete);
/**< @internal Get link speed, duplex mode and state (up/down) of an Ethernet device. */

typedef void (*eth_stats_get_t)(struct rte_eth_dev *dev,
				struct rte_eth_stats *igb_stats);
/**< @internal Get global I/O statistics of an Ethernet device. */

typedef void (*eth_stats_reset_t)(struct rte_eth_dev *dev);
/**< @internal Reset global I/O statistics of an Ethernet device to 0. */

typedef int (*eth_xstats_get_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat *stats, unsigned n);
/**< @internal Get extended stats of an Ethernet device. */

typedef void (*eth_xstats_reset_t)(struct rte_eth_dev *dev);
/**< @internal Reset extended stats of an Ethernet device. */

typedef int (*eth_xstats_get_names_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned size);
/**< @internal Get names of extended stats of an Ethernet device. */

typedef int (*eth_queue_stats_mapping_set_t)(struct rte_eth_dev *dev,
					     uint16_t queue_id,
					     uint8_t stat_idx,
					     uint8_t is_rx);
/**< @internal Set a queue statistics mapping for a tx/rx queue of an Ethernet device. */

typedef void (*eth_dev_infos_get_t)(struct rte_eth_dev *dev,
				    struct rte_eth_dev_info *dev_info);
/**< @internal Get specific informations of an Ethernet device. */

typedef const uint32_t *(*eth_dev_supported_ptypes_get_t)(struct rte_eth_dev *dev);
/**< @internal Get supported ptypes of an Ethernet device. */

typedef int (*eth_queue_start_t)(struct rte_eth_dev *dev,
				    uint16_t queue_id);
/**< @internal Start rx and tx of a queue of an Ethernet device. */

typedef int (*eth_queue_stop_t)(struct rte_eth_dev *dev,
				    uint16_t queue_id);
/**< @internal Stop rx and tx of a queue of an Ethernet device. */

typedef int (*eth_rx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id,
				    uint16_t nb_rx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_rxconf *rx_conf,
				    struct rte_mempool *mb_pool);
/**< @internal Set up a receive queue of an Ethernet device. */

typedef int (*eth_tx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t tx_queue_id,
				    uint16_t nb_tx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_txconf *tx_conf);
/**< @internal Setup a transmit queue of an Ethernet device. */

typedef int (*eth_rx_enable_intr_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
/**< @internal Enable interrupt of a receive queue of an Ethernet device. */

typedef int (*eth_rx_disable_intr_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
/**< @internal Disable interrupt of a receive queue of an Ethernet device. */

typedef void (*eth_queue_release_t)(void *queue);
/**< @internal Release memory resources allocated by given RX/TX queue. */

typedef uint32_t (*eth_rx_queue_count_t)(struct rte_eth_dev *dev,
					 uint16_t rx_queue_id);
/**< @internal Get number of available descriptors on a receive queue of an Ethernet device. */

typedef int (*eth_rx_descriptor_done_t)(void *rxq, uint16_t offset);
/**< @internal Check DD bit of specific RX descriptor */

typedef void (*eth_rxq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t rx_queue_id, struct rte_eth_rxq_info *qinfo);

typedef void (*eth_txq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t tx_queue_id, struct rte_eth_txq_info *qinfo);

typedef int (*mtu_set_t)(struct rte_eth_dev *dev, uint16_t mtu);
/**< @internal Set MTU. */

typedef int (*vlan_filter_set_t)(struct rte_eth_dev *dev,
				  uint16_t vlan_id,
				  int on);
/**< @internal filtering of a VLAN Tag Identifier by an Ethernet device. */

typedef int (*vlan_tpid_set_t)(struct rte_eth_dev *dev,
			       enum rte_vlan_type type, uint16_t tpid);
/**< @internal set the outer/inner VLAN-TPID by an Ethernet device. */

typedef void (*vlan_offload_set_t)(struct rte_eth_dev *dev, int mask);
/**< @internal set VLAN offload function by an Ethernet device. */

typedef int (*vlan_pvid_set_t)(struct rte_eth_dev *dev,
			       uint16_t vlan_id,
			       int on);
/**< @internal set port based TX VLAN insertion by an Ethernet device. */

typedef void (*vlan_strip_queue_set_t)(struct rte_eth_dev *dev,
				  uint16_t rx_queue_id,
				  int on);
/**< @internal VLAN stripping enable/disable by an queue of Ethernet device. */

typedef uint16_t (*eth_rx_burst_t)(void *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);
/**< @internal Retrieve input packets from a receive queue of an Ethernet device. */

typedef uint16_t (*eth_tx_burst_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< @internal Send output packets on a transmit queue of an Ethernet device. */

typedef int (*flow_ctrl_get_t)(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);
/**< @internal Get current flow control parameter on an Ethernet device */

typedef int (*flow_ctrl_set_t)(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);
/**< @internal Setup flow control parameter on an Ethernet device */

typedef int (*priority_flow_ctrl_set_t)(struct rte_eth_dev *dev,
				struct rte_eth_pfc_conf *pfc_conf);
/**< @internal Setup priority flow control parameter on an Ethernet device */

typedef int (*reta_update_t)(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);
/**< @internal Update RSS redirection table on an Ethernet device */

typedef int (*reta_query_t)(struct rte_eth_dev *dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
/**< @internal Query RSS redirection table on an Ethernet device */

typedef int (*rss_hash_update_t)(struct rte_eth_dev *dev,
				 struct rte_eth_rss_conf *rss_conf);
/**< @internal Update RSS hash configuration of an Ethernet device */

typedef int (*rss_hash_conf_get_t)(struct rte_eth_dev *dev,
				   struct rte_eth_rss_conf *rss_conf);
/**< @internal Get current RSS hash configuration of an Ethernet device */

typedef int (*eth_dev_led_on_t)(struct rte_eth_dev *dev);
/**< @internal Turn on SW controllable LED on an Ethernet device */

typedef int (*eth_dev_led_off_t)(struct rte_eth_dev *dev);
/**< @internal Turn off SW controllable LED on an Ethernet device */

typedef void (*eth_mac_addr_remove_t)(struct rte_eth_dev *dev, uint32_t index);
/**< @internal Remove MAC address from receive address register */

typedef void (*eth_mac_addr_add_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr,
				  uint32_t index,
				  uint32_t vmdq);
/**< @internal Set a MAC address into Receive Address Address Register */

typedef void (*eth_mac_addr_set_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr);
/**< @internal Set a MAC address into Receive Address Address Register */

typedef int (*eth_uc_hash_table_set_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr,
				  uint8_t on);
/**< @internal Set a Unicast Hash bitmap */

typedef int (*eth_uc_all_hash_table_set_t)(struct rte_eth_dev *dev,
				  uint8_t on);
/**< @internal Set all Unicast Hash bitmap */

typedef int (*eth_set_vf_rx_mode_t)(struct rte_eth_dev *dev,
				  uint16_t vf,
				  uint16_t rx_mode,
				  uint8_t on);
/**< @internal Set a VF receive mode */

typedef int (*eth_set_vf_rx_t)(struct rte_eth_dev *dev,
				uint16_t vf,
				uint8_t on);
/**< @internal Set a VF receive  mode */

typedef int (*eth_set_vf_tx_t)(struct rte_eth_dev *dev,
				uint16_t vf,
				uint8_t on);
/**< @internal Enable or disable a VF transmit   */

typedef int (*eth_set_vf_vlan_filter_t)(struct rte_eth_dev *dev,
				  uint16_t vlan,
				  uint64_t vf_mask,
				  uint8_t vlan_on);
/**< @internal Set VF VLAN pool filter */

typedef int (*eth_set_queue_rate_limit_t)(struct rte_eth_dev *dev,
				uint16_t queue_idx,
				uint16_t tx_rate);
/**< @internal Set queue TX rate */

typedef int (*eth_set_vf_rate_limit_t)(struct rte_eth_dev *dev,
				uint16_t vf,
				uint16_t tx_rate,
				uint64_t q_msk);
/**< @internal Set VF TX rate */

typedef int (*eth_mirror_rule_set_t)(struct rte_eth_dev *dev,
				  struct rte_eth_mirror_conf *mirror_conf,
				  uint8_t rule_id,
				  uint8_t on);
/**< @internal Add a traffic mirroring rule on an Ethernet device */

typedef int (*eth_mirror_rule_reset_t)(struct rte_eth_dev *dev,
				  uint8_t rule_id);
/**< @internal Remove a traffic mirroring rule on an Ethernet device */

typedef int (*eth_udp_tunnel_port_add_t)(struct rte_eth_dev *dev,
					 struct rte_eth_udp_tunnel *tunnel_udp);
/**< @internal Add tunneling UDP port */

typedef int (*eth_udp_tunnel_port_del_t)(struct rte_eth_dev *dev,
					 struct rte_eth_udp_tunnel *tunnel_udp);
/**< @internal Delete tunneling UDP port */

typedef int (*eth_set_mc_addr_list_t)(struct rte_eth_dev *dev,
				      struct ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
/**< @internal set the list of multicast addresses on an Ethernet device */

typedef int (*eth_timesync_enable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to enable IEEE1588/802.1AS timestamping. */

typedef int (*eth_timesync_disable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to disable IEEE1588/802.1AS timestamping. */

typedef int (*eth_timesync_read_rx_timestamp_t)(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags);
/**< @internal Function used to read an RX IEEE1588/802.1AS timestamp. */

typedef int (*eth_timesync_read_tx_timestamp_t)(struct rte_eth_dev *dev,
						struct timespec *timestamp);
/**< @internal Function used to read a TX IEEE1588/802.1AS timestamp. */

typedef int (*eth_timesync_adjust_time)(struct rte_eth_dev *dev, int64_t);
/**< @internal Function used to adjust the device clock */

typedef int (*eth_timesync_read_time)(struct rte_eth_dev *dev,
				      struct timespec *timestamp);
/**< @internal Function used to get time from the device clock. */

typedef int (*eth_timesync_write_time)(struct rte_eth_dev *dev,
				       const struct timespec *timestamp);
/**< @internal Function used to get time from the device clock */

typedef int (*eth_get_reg_t)(struct rte_eth_dev *dev,
				struct rte_dev_reg_info *info);
/**< @internal Retrieve registers  */

typedef int (*eth_get_eeprom_length_t)(struct rte_eth_dev *dev);
/**< @internal Retrieve eeprom size  */

typedef int (*eth_get_eeprom_t)(struct rte_eth_dev *dev,
				struct rte_dev_eeprom_info *info);
/**< @internal Retrieve eeprom data  */

typedef int (*eth_set_eeprom_t)(struct rte_eth_dev *dev,
				struct rte_dev_eeprom_info *info);
/**< @internal Program eeprom data  */

typedef int (*eth_l2_tunnel_eth_type_conf_t)
	(struct rte_eth_dev *dev, struct rte_eth_l2_tunnel_conf *l2_tunnel);
/**< @internal config l2 tunnel ether type */

typedef int (*eth_l2_tunnel_offload_set_t)
	(struct rte_eth_dev *dev,
	 struct rte_eth_l2_tunnel_conf *l2_tunnel,
	 uint32_t mask,
	 uint8_t en);
/**< @internal enable/disable the l2 tunnel offload functions */

#ifdef RTE_NIC_BYPASS

enum {
	RTE_BYPASS_MODE_NONE,
	RTE_BYPASS_MODE_NORMAL,
	RTE_BYPASS_MODE_BYPASS,
	RTE_BYPASS_MODE_ISOLATE,
	RTE_BYPASS_MODE_NUM,
};

#define	RTE_BYPASS_MODE_VALID(x)	\
	((x) > RTE_BYPASS_MODE_NONE && (x) < RTE_BYPASS_MODE_NUM)

enum {
	RTE_BYPASS_EVENT_NONE,
	RTE_BYPASS_EVENT_START,
	RTE_BYPASS_EVENT_OS_ON = RTE_BYPASS_EVENT_START,
	RTE_BYPASS_EVENT_POWER_ON,
	RTE_BYPASS_EVENT_OS_OFF,
	RTE_BYPASS_EVENT_POWER_OFF,
	RTE_BYPASS_EVENT_TIMEOUT,
	RTE_BYPASS_EVENT_NUM
};

#define	RTE_BYPASS_EVENT_VALID(x)	\
	((x) > RTE_BYPASS_EVENT_NONE && (x) < RTE_BYPASS_MODE_NUM)

enum {
	RTE_BYPASS_TMT_OFF,     /* timeout disabled. */
	RTE_BYPASS_TMT_1_5_SEC, /* timeout for 1.5 seconds */
	RTE_BYPASS_TMT_2_SEC,   /* timeout for 2 seconds */
	RTE_BYPASS_TMT_3_SEC,   /* timeout for 3 seconds */
	RTE_BYPASS_TMT_4_SEC,   /* timeout for 4 seconds */
	RTE_BYPASS_TMT_8_SEC,   /* timeout for 8 seconds */
	RTE_BYPASS_TMT_16_SEC,  /* timeout for 16 seconds */
	RTE_BYPASS_TMT_32_SEC,  /* timeout for 32 seconds */
	RTE_BYPASS_TMT_NUM
};

#define	RTE_BYPASS_TMT_VALID(x)	\
	((x) == RTE_BYPASS_TMT_OFF || \
	((x) > RTE_BYPASS_TMT_OFF && (x) < RTE_BYPASS_TMT_NUM))

typedef void (*bypass_init_t)(struct rte_eth_dev *dev);
typedef int32_t (*bypass_state_set_t)(struct rte_eth_dev *dev, uint32_t *new_state);
typedef int32_t (*bypass_state_show_t)(struct rte_eth_dev *dev, uint32_t *state);
typedef int32_t (*bypass_event_set_t)(struct rte_eth_dev *dev, uint32_t state, uint32_t event);
typedef int32_t (*bypass_event_show_t)(struct rte_eth_dev *dev, uint32_t event_shift, uint32_t *event);
typedef int32_t (*bypass_wd_timeout_set_t)(struct rte_eth_dev *dev, uint32_t timeout);
typedef int32_t (*bypass_wd_timeout_show_t)(struct rte_eth_dev *dev, uint32_t *wd_timeout);
typedef int32_t (*bypass_ver_show_t)(struct rte_eth_dev *dev, uint32_t *ver);
typedef int32_t (*bypass_wd_reset_t)(struct rte_eth_dev *dev);
#endif

typedef int (*eth_filter_ctrl_t)(struct rte_eth_dev *dev,
				 enum rte_filter_type filter_type,
				 enum rte_filter_op filter_op,
				 void *arg);
/**< @internal Take operations to assigned filter type on an Ethernet device */

typedef int (*eth_get_dcb_info)(struct rte_eth_dev *dev,
				 struct rte_eth_dcb_info *dcb_info);
/**< @internal Get dcb information on an Ethernet device */

/**
 * @internal A structure containing the functions exported by an Ethernet driver.
 */
struct eth_dev_ops {
	eth_dev_configure_t        dev_configure; /**< Configure device. */
	eth_dev_start_t            dev_start;     /**< Start device. */
	eth_dev_stop_t             dev_stop;      /**< Stop device. */
	eth_dev_set_link_up_t      dev_set_link_up;   /**< Device link up. */
	eth_dev_set_link_down_t    dev_set_link_down; /**< Device link down. */
	eth_dev_close_t            dev_close;     /**< Close device. */
	eth_promiscuous_enable_t   promiscuous_enable; /**< Promiscuous ON. */
	eth_promiscuous_disable_t  promiscuous_disable;/**< Promiscuous OFF. */
	eth_allmulticast_enable_t  allmulticast_enable;/**< RX multicast ON. */
	eth_allmulticast_disable_t allmulticast_disable;/**< RX multicast OF. */
	eth_link_update_t          link_update;   /**< Get device link state. */
	eth_stats_get_t            stats_get;     /**< Get generic device statistics. */
	eth_stats_reset_t          stats_reset;   /**< Reset generic device statistics. */
	eth_xstats_get_t           xstats_get;    /**< Get extended device statistics. */
	eth_xstats_reset_t         xstats_reset;  /**< Reset extended device statistics. */
	eth_xstats_get_names_t     xstats_get_names;
	/**< Get names of extended statistics. */
	eth_queue_stats_mapping_set_t queue_stats_mapping_set;
	/**< Configure per queue stat counter mapping. */
	eth_dev_infos_get_t        dev_infos_get; /**< Get device info. */
	eth_dev_supported_ptypes_get_t dev_supported_ptypes_get;
	/**< Get packet types supported and identified by device*/
	mtu_set_t                  mtu_set; /**< Set MTU. */
	vlan_filter_set_t          vlan_filter_set;  /**< Filter VLAN Setup. */
	vlan_tpid_set_t            vlan_tpid_set;      /**< Outer/Inner VLAN TPID Setup. */
	vlan_strip_queue_set_t     vlan_strip_queue_set; /**< VLAN Stripping on queue. */
	vlan_offload_set_t         vlan_offload_set; /**< Set VLAN Offload. */
	vlan_pvid_set_t            vlan_pvid_set; /**< Set port based TX VLAN insertion */
	eth_queue_start_t          rx_queue_start;/**< Start RX for a queue.*/
	eth_queue_stop_t           rx_queue_stop;/**< Stop RX for a queue.*/
	eth_queue_start_t          tx_queue_start;/**< Start TX for a queue.*/
	eth_queue_stop_t           tx_queue_stop;/**< Stop TX for a queue.*/
	eth_rx_queue_setup_t       rx_queue_setup;/**< Set up device RX queue.*/
	eth_queue_release_t        rx_queue_release;/**< Release RX queue.*/
	eth_rx_queue_count_t       rx_queue_count; /**< Get Rx queue count. */
	eth_rx_descriptor_done_t   rx_descriptor_done;  /**< Check rxd DD bit */
	/**< Enable Rx queue interrupt. */
	eth_rx_enable_intr_t       rx_queue_intr_enable;
	/**< Disable Rx queue interrupt.*/
	eth_rx_disable_intr_t      rx_queue_intr_disable;
	eth_tx_queue_setup_t       tx_queue_setup;/**< Set up device TX queue.*/
	eth_queue_release_t        tx_queue_release;/**< Release TX queue.*/
	eth_dev_led_on_t           dev_led_on;    /**< Turn on LED. */
	eth_dev_led_off_t          dev_led_off;   /**< Turn off LED. */
	flow_ctrl_get_t            flow_ctrl_get; /**< Get flow control. */
	flow_ctrl_set_t            flow_ctrl_set; /**< Setup flow control. */
	priority_flow_ctrl_set_t   priority_flow_ctrl_set; /**< Setup priority flow control.*/
	eth_mac_addr_remove_t      mac_addr_remove; /**< Remove MAC address */
	eth_mac_addr_add_t         mac_addr_add;  /**< Add a MAC address */
	eth_mac_addr_set_t         mac_addr_set;  /**< Set a MAC address */
	eth_uc_hash_table_set_t    uc_hash_table_set;  /**< Set Unicast Table Array */
	eth_uc_all_hash_table_set_t uc_all_hash_table_set;  /**< Set Unicast hash bitmap */
	eth_mirror_rule_set_t	   mirror_rule_set;  /**< Add a traffic mirror rule.*/
	eth_mirror_rule_reset_t	   mirror_rule_reset;  /**< reset a traffic mirror rule.*/
	eth_set_vf_rx_mode_t       set_vf_rx_mode;   /**< Set VF RX mode */
	eth_set_vf_rx_t            set_vf_rx;  /**< enable/disable a VF receive */
	eth_set_vf_tx_t            set_vf_tx;  /**< enable/disable a VF transmit */
	eth_set_vf_vlan_filter_t   set_vf_vlan_filter;  /**< Set VF VLAN filter */
	/** Add UDP tunnel port. */
	eth_udp_tunnel_port_add_t udp_tunnel_port_add;
	/** Del UDP tunnel port. */
	eth_udp_tunnel_port_del_t udp_tunnel_port_del;
	eth_set_queue_rate_limit_t set_queue_rate_limit;   /**< Set queue rate limit */
	eth_set_vf_rate_limit_t    set_vf_rate_limit;   /**< Set VF rate limit */
	/** Update redirection table. */
	reta_update_t reta_update;
	/** Query redirection table. */
	reta_query_t reta_query;

	eth_get_reg_t get_reg;
	/**< Get registers */
	eth_get_eeprom_length_t get_eeprom_length;
	/**< Get eeprom length */
	eth_get_eeprom_t get_eeprom;
	/**< Get eeprom data */
	eth_set_eeprom_t set_eeprom;
	/**< Set eeprom */
  /* bypass control */
#ifdef RTE_NIC_BYPASS
  bypass_init_t bypass_init;
  bypass_state_set_t bypass_state_set;
  bypass_state_show_t bypass_state_show;
  bypass_event_set_t bypass_event_set;
  bypass_event_show_t bypass_event_show;
  bypass_wd_timeout_set_t bypass_wd_timeout_set;
  bypass_wd_timeout_show_t bypass_wd_timeout_show;
  bypass_ver_show_t bypass_ver_show;
  bypass_wd_reset_t bypass_wd_reset;
#endif

	/** Configure RSS hash protocols. */
	rss_hash_update_t rss_hash_update;
	/** Get current RSS hash configuration. */
	rss_hash_conf_get_t rss_hash_conf_get;
	eth_filter_ctrl_t              filter_ctrl;
	/**< common filter control. */
	eth_set_mc_addr_list_t set_mc_addr_list; /**< set list of mcast addrs */
	eth_rxq_info_get_t rxq_info_get;
	/**< retrieve RX queue information. */
	eth_txq_info_get_t txq_info_get;
	/**< retrieve TX queue information. */
	/** Turn IEEE1588/802.1AS timestamping on. */
	eth_timesync_enable_t timesync_enable;
	/** Turn IEEE1588/802.1AS timestamping off. */
	eth_timesync_disable_t timesync_disable;
	/** Read the IEEE1588/802.1AS RX timestamp. */
	eth_timesync_read_rx_timestamp_t timesync_read_rx_timestamp;
	/** Read the IEEE1588/802.1AS TX timestamp. */
	eth_timesync_read_tx_timestamp_t timesync_read_tx_timestamp;

	/** Get DCB information */
	eth_get_dcb_info get_dcb_info;
	/** Adjust the device clock.*/
	eth_timesync_adjust_time timesync_adjust_time;
	/** Get the device clock time. */
	eth_timesync_read_time timesync_read_time;
	/** Set the device clock time. */
	eth_timesync_write_time timesync_write_time;
	/** Config ether type of l2 tunnel */
	eth_l2_tunnel_eth_type_conf_t l2_tunnel_eth_type_conf;
	/** Enable/disable l2 tunnel offload functions */
	eth_l2_tunnel_offload_set_t l2_tunnel_offload_set;
};

/**
 * Function type used for RX packet processing packet callbacks.
 *
 * The callback function is called on RX with a burst of packets that have
 * been received on the given port and queue.
 *
 * @param port
 *   The Ethernet port on which RX is being performed.
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
typedef uint16_t (*rte_rx_callback_fn)(uint8_t port, uint16_t queue,
	struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
	void *user_param);

/**
 * Function type used for TX packet processing packet callbacks.
 *
 * The callback function is called on TX with a burst of packets immediately
 * before the packets are put onto the hardware queue for transmission.
 *
 * @param port
 *   The Ethernet port on which TX is being performed.
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
typedef uint16_t (*rte_tx_callback_fn)(uint8_t port, uint16_t queue,
	struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param);

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
 * The eth device type.
 */
enum rte_eth_dev_type {
	RTE_ETH_DEV_UNKNOWN,	/**< unknown device type */
	RTE_ETH_DEV_PCI,
		/**< Physical function and Virtual function of PCI devices */
	RTE_ETH_DEV_VIRTUAL,	/**< non hardware device */
	RTE_ETH_DEV_MAX		/**< max value of this enum */
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
	struct rte_eth_dev_data *data;  /**< Pointer to device data */
	const struct eth_driver *driver;/**< Driver for this device */
	const struct eth_dev_ops *dev_ops; /**< Functions exported by PMD */
	struct rte_pci_device *pci_dev; /**< PCI info. supplied by probing */
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
	uint8_t attached; /**< Flag indicating the port is attached */
	enum rte_eth_dev_type dev_type; /**< Flag indicating the device type */
} __rte_cache_aligned;

struct rte_eth_dev_sriov {
	uint8_t active;               /**< SRIOV is active with 16, 32 or 64 pools */
	uint8_t nb_q_per_pool;        /**< rx queue number per pool */
	uint16_t def_vmdq_idx;        /**< Default pool num used for PF */
	uint16_t def_pool_q_idx;      /**< Default pool queue start reg index */
};
#define RTE_ETH_DEV_SRIOV(dev)         ((dev)->data->sriov)

#define RTE_ETH_NAME_MAX_LEN (32)

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

	void *dev_private;              /**< PMD-specific private data */

	struct rte_eth_link dev_link;
	/**< Link-level information & status */

	struct rte_eth_conf dev_conf;   /**< Configuration applied to device. */
	uint16_t mtu;                   /**< Maximum Transmission Unit. */

	uint32_t min_rx_buf_size;
	/**< Common rx buffer size handled by all queues */

	uint64_t rx_mbuf_alloc_failed; /**< RX ring mbuf allocation failures. */
	struct ether_addr* mac_addrs;/**< Device Ethernet Link address. */
	uint64_t mac_pool_sel[ETH_NUM_RECEIVE_MAC_ADDR];
	/** bitmap array of associating Ethernet MAC addresses to pools */
	struct ether_addr* hash_mac_addrs;
	/** Device Ethernet MAC addresses of hash filtering. */
	uint8_t port_id;           /**< Device [external] port identifier. */
	uint8_t promiscuous   : 1, /**< RX promiscuous mode ON(1) / OFF(0). */
		scattered_rx : 1,  /**< RX of scattered packets is ON(1) / OFF(0) */
		all_multicast : 1, /**< RX all multicast mode ON(1) / OFF(0). */
		dev_started : 1,   /**< Device state: STARTED(1) / STOPPED(0). */
		lro         : 1;   /**< RX LRO is ON(1) / OFF(0) */
	uint8_t rx_queue_state[RTE_MAX_QUEUES_PER_PORT];
	/** Queues state: STARTED(1) / STOPPED(0) */
	uint8_t tx_queue_state[RTE_MAX_QUEUES_PER_PORT];
	/** Queues state: STARTED(1) / STOPPED(0) */
	uint32_t dev_flags; /**< Capabilities */
	enum rte_kernel_driver kdrv;    /**< Kernel driver passthrough */
	int numa_node;  /**< NUMA node connection */
	const char *drv_name;   /**< Driver name */
};

/** Device supports hotplug detach */
#define RTE_ETH_DEV_DETACHABLE   0x0001
/** Device supports link state interrupt */
#define RTE_ETH_DEV_INTR_LSC     0x0002
/** Device is a bonded slave */
#define RTE_ETH_DEV_BONDED_SLAVE 0x0004

/**
 * @internal
 * The pool of *rte_eth_dev* structures. The size of the pool
 * is configured at compile-time in the <rte_ethdev.c> file.
 */
extern struct rte_eth_dev rte_eth_devices[];

/**
 * Get the total number of Ethernet devices that have been successfully
 * initialized by the [matching] Ethernet driver during the PCI probing phase.
 * All devices whose port identifier is in the range
 * [0,  rte_eth_dev_count() - 1] can be operated on by network applications
 * immediately after invoking rte_eal_init().
 * If the application unplugs a port using hotplug function, The enabled port
 * numbers may be noncontiguous. In the case, the applications need to manage
 * enabled port by themselves.
 *
 * @return
 *   - The total number of usable Ethernet devices.
 */
uint8_t rte_eth_dev_count(void);

/**
 * @internal
 * Returns a ethdev slot specified by the unique identifier name.
 *
 * @param	name
 *  The pointer to the Unique identifier name for each Ethernet device
 * @return
 *   - The pointer to the ethdev slot, on success. NULL on error
 */
struct rte_eth_dev *rte_eth_dev_allocated(const char *name);

/**
 * @internal
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param	name	Unique identifier name for each Ethernet device
 * @param	type	Device type of this Ethernet device
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
struct rte_eth_dev *rte_eth_dev_allocate(const char *name,
		enum rte_eth_dev_type type);

/**
 * @internal
 * Release the specified ethdev port.
 *
 * @param eth_dev
 * The *eth_dev* pointer is the address of the *rte_eth_dev* structure.
 * @return
 *   - 0 on success, negative on error
 */
int rte_eth_dev_release_port(struct rte_eth_dev *eth_dev);

/**
 * Attach a new Ethernet device specified by aruguments.
 *
 * @param devargs
 *  A pointer to a strings array describing the new device
 *  to be attached. The strings should be a pci address like
 *  '0000:01:00.0' or virtual device name like 'eth_pcap0'.
 * @param port_id
 *  A pointer to a port identifier actually attached.
 * @return
 *  0 on success and port_id is filled, negative on error
 */
int rte_eth_dev_attach(const char *devargs, uint8_t *port_id);

/**
 * Detach a Ethernet device specified by port identifier.
 * This function must be called when the device is in the
 * closed state.
 *
 * @param port_id
 *   The port identifier of the device to detach.
 * @param devname
 *  A pointer to a device name actually detached.
 * @return
 *  0 on success and devname is filled, negative on error
 */
int rte_eth_dev_detach(uint8_t port_id, char *devname);

struct eth_driver;
/**
 * @internal
 * Initialization function of an Ethernet driver invoked for each matching
 * Ethernet PCI device detected during the PCI probing phase.
 *
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
 *   - *driver*: Holds the pointer to the *eth_driver* structure.
 *
 *   - *dev_private*: Holds a pointer to the device private data structure.
 *
 *   - *mtu*: Contains the default Ethernet maximum frame length (1500).
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
typedef int (*eth_dev_init_t)(struct rte_eth_dev *eth_dev);

/**
 * @internal
 * Finalization function of an Ethernet driver invoked for each matching
 * Ethernet PCI device detected during the PCI closing phase.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 *   associated with the matching device and which have been [automatically]
 *   allocated in the *rte_eth_devices* array.
 * @return
 *   - 0: Success, the device is properly finalized by the driver.
 *        In particular, the driver MUST free the *dev_ops* pointer
 *        of the *eth_dev* structure.
 *   - <0: Error code of the device initialization failure.
 */
typedef int (*eth_dev_uninit_t)(struct rte_eth_dev *eth_dev);

/**
 * @internal
 * The structure associated with a PMD Ethernet driver.
 *
 * Each Ethernet driver acts as a PCI driver and is represented by a generic
 * *eth_driver* structure that holds:
 *
 * - An *rte_pci_driver* structure (which must be the first field).
 *
 * - The *eth_dev_init* function invoked for each matching PCI device.
 *
 * - The *eth_dev_uninit* function invoked for each matching PCI device.
 *
 * - The size of the private data to allocate for each matching device.
 */
struct eth_driver {
	struct rte_pci_driver pci_drv;    /**< The PMD is also a PCI driver. */
	eth_dev_init_t eth_dev_init;      /**< Device init function. */
	eth_dev_uninit_t eth_dev_uninit;  /**< Device uninit function. */
	unsigned int dev_private_size;    /**< Size of device private data. */
};

/**
 * @internal
 * A function invoked by the initialization function of an Ethernet driver
 * to simultaneously register itself as a PCI driver and as an Ethernet
 * Poll Mode Driver (PMD).
 *
 * @param eth_drv
 *   The pointer to the *eth_driver* structure associated with
 *   the Ethernet driver.
 */
void rte_eth_driver_register(struct eth_driver *eth_drv);

/**
 * Convert a numerical speed in Mbps to a bitmap flag that can be used in
 * the bitmap link_speeds of the struct rte_eth_conf
 *
 * @param speed
 *   Numerical speed value in Mbps
 * @param duplex
 *   ETH_LINK_[HALF/FULL]_DUPLEX (only for 10/100M speeds)
 * @return
 *   0 if the speed cannot be mapped
 */
uint32_t rte_eth_speed_bitflag(uint32_t speed, int duplex);

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
int rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue,
		uint16_t nb_tx_queue, const struct rte_eth_conf *eth_conf);

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
 *   NULL value is allowed, in which case default RX configuration
 *   will be used.
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
int rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
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
 *   NULL value is allowed, in which case default RX configuration
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
 *   - The *txq_flags* member contains flags to pass to the TX queue setup
 *     function to configure the behavior of the TX queue. This should be set
 *     to 0 if no special configuration is required.
 *
 *     Note that setting *tx_free_thresh* or *tx_rs_thresh* value to 0 forces
 *     the transmit function to use default values.
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -ENOMEM: Unable to allocate the transmit ring descriptors.
 */
int rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

/**
 * Return the NUMA socket to which an Ethernet device is connected
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @return
 *   The NUMA socket id to which the Ethernet device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 is returned is the port_id value is out of range.
 */
int rte_eth_dev_socket_id(uint8_t port_id);

/**
 * Check if port_id of device is attached
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @return
 *   - 0 if port is out of range or not attached
 *   - 1 if device is attached
 */
int rte_eth_dev_is_valid_port(uint8_t port_id);

/**
 * Start specified RX queue of a port. It is used when rx_deferred_start
 * flag of the specified queue is true.
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the receive queue is started.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
int rte_eth_dev_rx_queue_start(uint8_t port_id, uint16_t rx_queue_id);

/**
 * Stop specified RX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the receive queue is stopped.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
int rte_eth_dev_rx_queue_stop(uint8_t port_id, uint16_t rx_queue_id);

/**
 * Start TX for specified queue of a port. It is used when tx_deferred_start
 * flag of the specified queue is true.
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is started.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
int rte_eth_dev_tx_queue_start(uint8_t port_id, uint16_t tx_queue_id);

/**
 * Stop specified TX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is stopped.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
int rte_eth_dev_tx_queue_stop(uint8_t port_id, uint16_t tx_queue_id);



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
int rte_eth_dev_start(uint8_t port_id);

/**
 * Stop an Ethernet device. The device can be restarted with a call to
 * rte_eth_dev_start()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_dev_stop(uint8_t port_id);


/**
 * Link up an Ethernet device.
 *
 * Set device link up will re-enable the device rx/tx
 * functionality after it is previously set device linked down.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - 0: Success, Ethernet device linked up.
 *   - <0: Error code of the driver device link up function.
 */
int rte_eth_dev_set_link_up(uint8_t port_id);

/**
 * Link down an Ethernet device.
 * The device rx/tx functionality will be disabled if success,
 * and it can be re-enabled with a call to
 * rte_eth_dev_set_link_up()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
int rte_eth_dev_set_link_down(uint8_t port_id);

/**
 * Close a stopped Ethernet device. The device cannot be restarted!
 * The function frees all resources except for needed by the
 * closed state. To free these resources, call rte_eth_dev_detach().
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_dev_close(uint8_t port_id);

/**
 * Enable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_promiscuous_enable(uint8_t port_id);

/**
 * Disable receipt in promiscuous mode for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_promiscuous_disable(uint8_t port_id);

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
int rte_eth_promiscuous_get(uint8_t port_id);

/**
 * Enable the receipt of any multicast frame by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_allmulticast_enable(uint8_t port_id);

/**
 * Disable the receipt of all multicast frames by an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_allmulticast_disable(uint8_t port_id);

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
int rte_eth_allmulticast_get(uint8_t port_id);

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
void rte_eth_link_get(uint8_t port_id, struct rte_eth_link *link);

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
void rte_eth_link_get_nowait(uint8_t port_id, struct rte_eth_link *link);

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
int rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats);

/**
 * Reset the general I/O statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_stats_reset(uint8_t port_id);

/**
 * Retrieve names of extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param xstats_names
 *  Block of memory to insert names into. Must be at least size in capacity.
 *  If set to NULL, function returns required capacity.
 * @param size
 *  Capacity of xstats_names (number of names).
 * @return
 *   - positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - negative value on error (invalid port id)
 */
int rte_eth_xstats_get_names(uint8_t port_id,
		struct rte_eth_xstat_name *xstats_names,
		unsigned size);

/**
 * Retrieve extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param xstats
 *   A pointer to a table of structure of type *rte_eth_xstat*
 *   to be filled with device statistics ids and values.
 *   This parameter can be set to NULL if n is 0.
 * @param n
 *   The size of the stats table, which should be large enough to store
 *   all the statistics of the device.
 * @return
 *   - positive value lower or equal to n: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than n: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - negative value on error (invalid port id)
 */
int rte_eth_xstats_get(uint8_t port_id, struct rte_eth_xstat *xstats,
		unsigned n);

/**
 * Reset extended statistics of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
void rte_eth_xstats_reset(uint8_t port_id);

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
 *   The value must be in the range [0, RTE_MAX_ETHPORT_QUEUE_STATS_MAPS - 1].
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
int rte_eth_dev_set_tx_queue_stats_mapping(uint8_t port_id,
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
 *   The value must be in the range [0, RTE_MAX_ETHPORT_QUEUE_STATS_MAPS - 1].
 * @return
 *   Zero if successful. Non-zero otherwise.
 */
int rte_eth_dev_set_rx_queue_stats_mapping(uint8_t port_id,
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
 */
void rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr);

/**
 * Retrieve the contextual information of an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dev_info
 *   A pointer to a structure of type *rte_eth_dev_info* to be filled with
 *   the contextual information of the Ethernet device.
 */
void rte_eth_dev_info_get(uint8_t port_id, struct rte_eth_dev_info *dev_info);

/**
 * Retrieve the supported packet types of an Ethernet device.
 *
 * When a packet type is announced as supported, it *must* be recognized by
 * the PMD. For instance, if RTE_PTYPE_L2_ETHER, RTE_PTYPE_L2_ETHER_VLAN
 * and RTE_PTYPE_L3_IPV4 are announced, the PMD must return the following
 * packet types for these packets:
 * - Ether/IPv4              -> RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
 * - Ether/Vlan/IPv4         -> RTE_PTYPE_L2_ETHER_VLAN | RTE_PTYPE_L3_IPV4
 * - Ether/[anything else]   -> RTE_PTYPE_L2_ETHER
 * - Ether/Vlan/[anything else] -> RTE_PTYPE_L2_ETHER_VLAN
 *
 * When a packet is received by a PMD, the most precise type must be
 * returned among the ones supported. However a PMD is allowed to set
 * packet type that is not in the supported list, at the condition that it
 * is more precise. Therefore, a PMD announcing no supported packet types
 * can still set a matching packet type in a received packet.
 *
 * @note
 *   Better to invoke this API after the device is already started or rx burst
 *   function is decided, to obtain correct supported ptypes.
 * @note
 *   if a given PMD does not report what ptypes it supports, then the supported
 *   ptype count is reported as 0.
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ptype_mask
 *   A hint of what kind of packet type which the caller is interested in.
 * @param ptypes
 *   An array pointer to store adequent packet types, allocated by caller.
 * @param num
 *  Size of the array pointed by param ptypes.
 * @return
 *   - (>=0) Number of supported ptypes. If the number of types exceeds num,
 *           only num entries will be filled into the ptypes array, but the full
 *           count of supported ptypes will be returned.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_get_supported_ptypes(uint8_t port_id, uint32_t ptype_mask,
				     uint32_t *ptypes, int num);

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
 */
int rte_eth_dev_get_mtu(uint8_t port_id, uint16_t *mtu);

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
 *   - (-EINVAL) if *mtu* invalid.
 *   - (-EBUSY) if operation is not allowed when the port is running
 */
int rte_eth_dev_set_mtu(uint8_t port_id, uint16_t mtu);

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
int rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id, int on);

/**
 * Enable/Disable hardware VLAN Strip by a rx queue of an Ethernet device.
 * 82599/X540/X550 can support VLAN stripping at the rx queue level
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
 *   - (-ENOSUP) if hardware-assisted VLAN stripping not configured.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if *rx_queue_id* invalid.
 */
int rte_eth_dev_set_vlan_strip_on_queue(uint8_t port_id, uint16_t rx_queue_id,
		int on);

/**
 * Set the Outer VLAN Ether Type by an Ethernet device, it can be inserted to
 * the VLAN Header. This is a register setup available on some Intel NIC, not
 * but all, please check the data sheet for availability.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vlan_type
 *   The vlan type.
 * @param tag_type
 *   The Tag Protocol ID
 * @return
 *   - (0) if successful.
 *   - (-ENOSUP) if hardware-assisted VLAN TPID setup is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_set_vlan_ether_type(uint8_t port_id,
				    enum rte_vlan_type vlan_type,
				    uint16_t tag_type);

/**
 * Set VLAN offload configuration on an Ethernet device
 * Enable/Disable Extended VLAN by an Ethernet device, This is a register setup
 * available on some Intel NIC, not but all, please check the data sheet for
 * availability.
 * Enable/Disable VLAN Strip can be done on rx queue for certain NIC, but here
 * the configuration is applied on the port level.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param offload_mask
 *   The VLAN Offload bit mask can be mixed use with "OR"
 *       ETH_VLAN_STRIP_OFFLOAD
 *       ETH_VLAN_FILTER_OFFLOAD
 *       ETH_VLAN_EXTEND_OFFLOAD
 * @return
 *   - (0) if successful.
 *   - (-ENOSUP) if hardware-assisted VLAN filtering not configured.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_set_vlan_offload(uint8_t port_id, int offload_mask);

/**
 * Read VLAN Offload configuration from an Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (>0) if successful. Bit mask to indicate
 *       ETH_VLAN_STRIP_OFFLOAD
 *       ETH_VLAN_FILTER_OFFLOAD
 *       ETH_VLAN_EXTEND_OFFLOAD
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_get_vlan_offload(uint8_t port_id);

/**
 * Set port based TX VLAN insersion on or off.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param pvid
 *  Port based TX VLAN identifier togeth with user priority.
 * @param on
 *  Turn on or off the port based TX VLAN insertion.
 *
 * @return
 *   - (0) if successful.
 *   - negative if failed.
 */
int rte_eth_dev_set_vlan_pvid(uint8_t port_id, uint16_t pvid, int on);

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
static inline uint16_t
rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->rx_pkt_burst, 0);

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", queue_id);
		return 0;
	}
#endif
	int16_t nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id],
			rx_pkts, nb_pkts);

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
	struct rte_eth_rxtx_callback *cb = dev->post_rx_burst_cbs[queue_id];

	if (unlikely(cb != NULL)) {
		do {
			nb_rx = cb->fn.rx(port_id, queue_id, rx_pkts, nb_rx,
						nb_pkts, cb->param);
			cb = cb->next;
		} while (cb != NULL);
	}
#endif

	return nb_rx;
}

/**
 * Get the number of used descriptors in a specific queue
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The queue id on the specific port.
 * @return
 *  The number of used descriptors in the specific queue, or:
 *     (-EINVAL) if *port_id* is invalid
 *     (-ENOTSUP) if the device does not support this function
 */
static inline int
rte_eth_rx_queue_count(uint8_t port_id, uint16_t queue_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_count, -ENOTSUP);
        return (*dev->dev_ops->rx_queue_count)(dev, queue_id);
}

/**
 * Check if the DD bit of the specific RX descriptor in the queue has been set
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The queue id on the specific port.
 * @param offset
 *  The offset of the descriptor ID from tail.
 * @return
 *  - (1) if the specific DD bit is set.
 *  - (0) if the specific DD bit is not set.
 *  - (-ENODEV) if *port_id* invalid.
 *  - (-ENOTSUP) if the device does not support this function
 */
static inline int
rte_eth_rx_descriptor_done(uint8_t port_id, uint16_t queue_id, uint16_t offset)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_descriptor_done, -ENOTSUP);
	return (*dev->dev_ops->rx_descriptor_done)( \
		dev->data->rx_queues[queue_id], offset);
}

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
 * When the number of free TX descriptors drops below this threshold, the
 * rte_eth_tx_burst() function must [attempt to] free the *rte_mbuf*  buffers
 * of those packets whose transmission was effectively completed.
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
rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->tx_pkt_burst, 0);

	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", queue_id);
		return 0;
	}
#endif

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
	struct rte_eth_rxtx_callback *cb = dev->pre_tx_burst_cbs[queue_id];

	if (unlikely(cb != NULL)) {
		do {
			nb_pkts = cb->fn.tx(port_id, queue_id, tx_pkts, nb_pkts,
					cb->param);
			cb = cb->next;
		} while (cb != NULL);
	}
#endif

	return (*dev->tx_pkt_burst)(dev->data->tx_queues[queue_id], tx_pkts, nb_pkts);
}

typedef void (*buffer_tx_error_fn)(struct rte_mbuf **unsent, uint16_t count,
		void *userdata);

/**
 * Structure used to buffer packets for future TX
 * Used by APIs rte_eth_tx_buffer and rte_eth_tx_buffer_flush
 */
struct rte_eth_dev_tx_buffer {
	buffer_tx_error_fn error_callback;
	void *error_userdata;
	uint16_t size;           /**< Size of buffer for buffered tx */
	uint16_t length;         /**< Number of packets in the array */
	struct rte_mbuf *pkts[];
	/**< Pending packets to be sent on explicit flush or when full */
};

/**
 * Calculate the size of the tx buffer.
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
rte_eth_tx_buffer_flush(uint8_t port_id, uint16_t queue_id,
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
		buffer->error_callback(&buffer->pkts[sent], to_send - sent,
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
static inline uint16_t __attribute__((always_inline))
rte_eth_tx_buffer(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_dev_tx_buffer *buffer, struct rte_mbuf *tx_pkt)
{
	buffer->pkts[buffer->length++] = tx_pkt;
	if (buffer->length < buffer->size)
		return 0;

	return rte_eth_tx_buffer_flush(port_id, queue_id, buffer);
}

/**
 * Configure a callback for buffered packets which cannot be sent
 *
 * Register a specific callback to be called when an attempt is made to send
 * all packets buffered on an ethernet port, but not all packets can
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
 *   0 on success, or -1 on error with rte_errno set appropriately
 */
int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn callback, void *userdata);

/**
 * Callback function for silently dropping unsent buffered packets.
 *
 * This function can be passed to rte_eth_tx_buffer_set_err_callback() to
 * adjust the default behavior when buffered packets cannot be sent. This
 * function drops any unsent packets silently and is used by tx buffered
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
 * The eth device event type for interrupt, and maybe others in the future.
 */
enum rte_eth_event_type {
	RTE_ETH_EVENT_UNKNOWN,  /**< unknown event type */
	RTE_ETH_EVENT_INTR_LSC, /**< lsc interrupt event */
	RTE_ETH_EVENT_QUEUE_STATE,
				/**< queue state event (enabled/disabled) */
	RTE_ETH_EVENT_INTR_RESET,
			/**< reset interrupt event, sent to VF on PF reset */
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
 * When there is no rx packet coming in Rx Queue for a long time, we can
 * sleep lcore related to RX Queue for power saving, and enable rx interrupt
 * to be triggered when rx packect arrives.
 *
 * The rte_eth_dev_rx_intr_enable() function enables rx queue
 * interrupt on specific rx queue of a port.
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
 */
int rte_eth_dev_rx_intr_enable(uint8_t port_id, uint16_t queue_id);

/**
 * When lcore wakes up from rx interrupt indicating packet coming, disable rx
 * interrupt and returns to polling mode.
 *
 * The rte_eth_dev_rx_intr_disable() function disables rx queue
 * interrupt on specific rx queue of a port.
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
 */
int rte_eth_dev_rx_intr_disable(uint8_t port_id, uint16_t queue_id);

/**
 * RX Interrupt control per port.
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
int rte_eth_dev_rx_intr_ctl(uint8_t port_id, int epfd, int op, void *data);

/**
 * RX Interrupt control per queue.
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
int rte_eth_dev_rx_intr_ctl_q(uint8_t port_id, uint16_t queue_id,
			      int epfd, int op, void *data);

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
 */
int rte_eth_dev_flow_ctrl_get(uint8_t port_id,
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
 *   - (-EIO)     if flow control setup failure
 */
int rte_eth_dev_flow_ctrl_set(uint8_t port_id,
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
 *   - (-EIO)     if flow control setup failure
 */
int rte_eth_dev_priority_flow_ctrl_set(uint8_t port_id,
				struct rte_eth_pfc_conf *pfc_conf);

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

/**
 * Set the default MAC address.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param mac_addr
 *   New default MAC address.
 * @return
 *   - (0) if successful, or *mac_addr* didn't exist.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if MAC address is invalid.
 */
int rte_eth_dev_default_mac_addr_set(uint8_t port, struct ether_addr *mac_addr);


/**
 * Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *   RETA to update.
 * @param reta_size
 *   Redirection table size. The table size can be queried by
 *   rte_eth_dev_info_get().
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_reta_update(uint8_t port,
				struct rte_eth_rss_reta_entry64 *reta_conf,
				uint16_t reta_size);

 /**
 * Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *   RETA to query.
 * @param reta_size
 *   Redirection table size. The table size can be queried by
 *   rte_eth_dev_info_get().
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_reta_query(uint8_t port,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);

 /**
 * Updates unicast hash table for receiving packet with the given destination
 * MAC address, and the packet is routed to all VFs for which the RX mode is
 * accept packets that match the unicast hash table.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param addr
 *   Unicast MAC address.
 * @param on
 *    1 - Set an unicast hash bit for receiving packets with the MAC address.
 *    0 - Clear an unicast hash bit.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
  *  - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_uc_hash_table_set(uint8_t port,struct ether_addr *addr,
					uint8_t on);

 /**
 * Updates all unicast hash bitmaps for receiving packet with any Unicast
 * Ethernet MAC addresses,the packet is routed to all VFs for which the RX
 * mode is accept packets that match the unicast hash table.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param on
 *    1 - Set all unicast hash bitmaps for receiving all the Ethernet
 *         MAC addresses
 *    0 - Clear all unicast hash bitmaps
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
  *  - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_uc_all_hash_table_set(uint8_t port,uint8_t on);

 /**
 * Set RX L2 Filtering mode of a VF of an Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param rx_mode
 *    The RX mode mask, which  is one or more of  accepting Untagged Packets,
 *    packets that match the PFUTA table, Broadcast and Multicast Promiscuous.
 *    ETH_VMDQ_ACCEPT_UNTAG,ETH_VMDQ_ACCEPT_HASH_UC,
 *    ETH_VMDQ_ACCEPT_BROADCAST and ETH_VMDQ_ACCEPT_MULTICAST will be used
 *    in rx_mode.
 * @param on
 *    1 - Enable a VF RX mode.
 *    0 - Disable a VF RX mode.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_set_vf_rxmode(uint8_t port, uint16_t vf, uint16_t rx_mode,
				uint8_t on);

/**
* Enable or disable a VF traffic transmit of the Ethernet device.
*
* @param port
*   The port identifier of the Ethernet device.
* @param vf
*   VF id.
* @param on
*    1 - Enable a VF traffic transmit.
*    0 - Disable a VF traffic transmit.
* @return
*   - (0) if successful.
*   - (-ENODEV) if *port_id* invalid.
*   - (-ENOTSUP) if hardware doesn't support.
*   - (-EINVAL) if bad parameter.
*/
int
rte_eth_dev_set_vf_tx(uint8_t port,uint16_t vf, uint8_t on);

/**
* Enable or disable a VF traffic receive of an Ethernet device.
*
* @param port
*   The port identifier of the Ethernet device.
* @param vf
*   VF id.
* @param on
*    1 - Enable a VF traffic receive.
*    0 - Disable a VF traffic receive.
* @return
*   - (0) if successful.
*   - (-ENOTSUP) if hardware doesn't support.
*   - (-ENODEV) if *port_id* invalid.
*   - (-EINVAL) if bad parameter.
*/
int
rte_eth_dev_set_vf_rx(uint8_t port,uint16_t vf, uint8_t on);

/**
* Enable/Disable hardware VF VLAN filtering by an Ethernet device of
* received VLAN packets tagged with a given VLAN Tag Identifier.
*
* @param port id
*   The port identifier of the Ethernet device.
* @param vlan_id
*   The VLAN Tag Identifier whose filtering must be enabled or disabled.
* @param vf_mask
*    Bitmap listing which VFs participate in the VLAN filtering.
* @param vlan_on
*    1 - Enable VFs VLAN filtering.
*    0 - Disable VFs VLAN filtering.
* @return
*   - (0) if successful.
*   - (-ENOTSUP) if hardware doesn't support.
*   - (-ENODEV) if *port_id* invalid.
*   - (-EINVAL) if bad parameter.
*/
int
rte_eth_dev_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id,
				uint64_t vf_mask,
				uint8_t vlan_on);

/**
 * Set a traffic mirroring rule on an Ethernet device
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mirror_conf
 *   The pointer to the traffic mirroring structure describing the mirroring rule.
 *   The *rte_eth_vm_mirror_conf* structure includes the type of mirroring rule,
 *   destination pool and the value of rule if enable vlan or pool mirroring.
 *
 * @param rule_id
 *   The index of traffic mirroring rule, we support four separated rules.
 * @param on
 *   1 - Enable a mirroring rule.
 *   0 - Disable a mirroring rule.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the mr_conf information is not correct.
 */
int rte_eth_mirror_rule_set(uint8_t port_id,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t rule_id,
			uint8_t on);

/**
 * Reset a traffic mirroring rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rule_id
 *   The index of traffic mirroring rule, we support four separated rules.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_mirror_rule_reset(uint8_t port_id,
					 uint8_t rule_id);

/**
 * Set the rate limitation for a queue on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_idx
 *   The queue id.
 * @param tx_rate
 *   The tx rate in Mbps. Allocated from the total port link speed.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_set_queue_rate_limit(uint8_t port_id, uint16_t queue_idx,
			uint16_t tx_rate);

/**
 * Set the rate limitation for a vf on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param tx_rate
 *   The tx rate allocated from the total link speed for this VF id.
 * @param q_msk
 *   The queue mask which need to set the rate.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_set_vf_rate_limit(uint8_t port_id, uint16_t vf,
			uint16_t tx_rate, uint64_t q_msk);

/**
 * Initialize bypass logic. This function needs to be called before
 * executing any other bypass API.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_init(uint8_t port);

/**
 * Return bypass state.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param state
 *   The return bypass state.
 *   - (1) Normal mode
 *   - (2) Bypass mode
 *   - (3) Isolate mode
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_state_show(uint8_t port, uint32_t *state);

/**
 * Set bypass state
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param new_state
 *   The current bypass state.
 *   - (1) Normal mode
 *   - (2) Bypass mode
 *   - (3) Isolate mode
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_state_set(uint8_t port, uint32_t *new_state);

/**
 * Return bypass state when given event occurs.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param event
 *   The bypass event
 *   - (1) Main power on (power button is pushed)
 *   - (2) Auxiliary power on (power supply is being plugged)
 *   - (3) Main power off (system shutdown and power supply is left plugged in)
 *   - (4) Auxiliary power off (power supply is being unplugged)
 *   - (5) Display or set the watchdog timer
 * @param state
 *   The bypass state when given event occurred.
 *   - (1) Normal mode
 *   - (2) Bypass mode
 *   - (3) Isolate mode
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_event_show(uint8_t port, uint32_t event, uint32_t *state);

/**
 * Set bypass state when given event occurs.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param event
 *   The bypass event
 *   - (1) Main power on (power button is pushed)
 *   - (2) Auxiliary power on (power supply is being plugged)
 *   - (3) Main power off (system shutdown and power supply is left plugged in)
 *   - (4) Auxiliary power off (power supply is being unplugged)
 *   - (5) Display or set the watchdog timer
 * @param state
 *   The assigned state when given event occurs.
 *   - (1) Normal mode
 *   - (2) Bypass mode
 *   - (3) Isolate mode
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_event_store(uint8_t port, uint32_t event, uint32_t state);

/**
 * Set bypass watchdog timeout count.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param timeout
 *   The timeout to be set.
 *   - (0) 0 seconds (timer is off)
 *   - (1) 1.5 seconds
 *   - (2) 2 seconds
 *   - (3) 3 seconds
 *   - (4) 4 seconds
 *   - (5) 8 seconds
 *   - (6) 16 seconds
 *   - (7) 32 seconds
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_wd_timeout_store(uint8_t port, uint32_t timeout);

/**
 * Get bypass firmware version.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param ver
 *   The firmware version
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_ver_show(uint8_t port, uint32_t *ver);

/**
 * Return bypass watchdog timeout in seconds
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param wd_timeout
 *   The return watchdog timeout. "0" represents timer expired
 *   - (0) 0 seconds (timer is off)
 *   - (1) 1.5 seconds
 *   - (2) 2 seconds
 *   - (3) 3 seconds
 *   - (4) 4 seconds
 *   - (5) 8 seconds
 *   - (6) 16 seconds
 *   - (7) 32 seconds
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_wd_timeout_show(uint8_t port, uint32_t *wd_timeout);

/**
 * Reset bypass watchdog timer
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_bypass_wd_reset(uint8_t port);

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
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_hash_update(uint8_t port_id,
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
 *   - (-ENOTSUP) if hardware doesn't support RSS.
 */
int
rte_eth_dev_rss_hash_conf_get(uint8_t port_id,
			      struct rte_eth_rss_conf *rss_conf);

 /**
 * Add UDP tunneling port for a specific type of tunnel.
 * The packets with this UDP port will be identified as this type of tunnel.
 * Before enabling any offloading function for a tunnel, users can call this API
 * to change or add more UDP port for the tunnel. So the offloading function
 * can take effect on the packets with the sepcific UDP port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tunnel_udp
 *   UDP tunneling configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_udp_tunnel_port_add(uint8_t port_id,
				struct rte_eth_udp_tunnel *tunnel_udp);

 /**
 * Delete UDP tunneling port a specific type of tunnel.
 * The packets with this UDP port will not be identified as this type of tunnel
 * any more.
 * Before enabling any offloading function for a tunnel, users can call this API
 * to delete a UDP port for the tunnel. So the offloading function will not take
 * effect on the packets with the sepcific UDP port.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tunnel_udp
 *   UDP tunneling configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_udp_tunnel_port_delete(uint8_t port_id,
				   struct rte_eth_udp_tunnel *tunnel_udp);

/**
 * Check whether the filter type is supported on an Ethernet device.
 * All the supported filter types are defined in 'rte_eth_ctrl.h'.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param filter_type
 *   Filter type.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this filter type.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_eth_dev_filter_supported(uint8_t port_id, enum rte_filter_type filter_type);

/**
 * Take operations to assigned filter type on an Ethernet device.
 * All the supported operations and filter types are defined in 'rte_eth_ctrl.h'.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Type of operation.
 * @param arg
 *   A pointer to arguments defined specifically for the operation.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_filter_ctrl(uint8_t port_id, enum rte_filter_type filter_type,
			enum rte_filter_op filter_op, void *arg);

/**
 * Get DCB information on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param dcb_info
 *   dcb information.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-ENOTSUP) if hardware doesn't support.
 */
int rte_eth_dev_get_dcb_info(uint8_t port_id,
			     struct rte_eth_dcb_info *dcb_info);

/**
 * Add a callback to be called on packet RX on a given port and queue.
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
 *   callback function on this port and queue.
 *
 * @return
 *   NULL on error.
 *   On success, a pointer value which can later be used to remove the callback.
 */
void *rte_eth_add_rx_callback(uint8_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param);

/*
* Add a callback that must be called first on packet RX on a given port
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
*   callback function on this port and queue.
*
* @return
*   NULL on error.
*   On success, a pointer value which can later be used to remove the callback.
*/
void *rte_eth_add_first_rx_callback(uint8_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param);

/**
 * Add a callback to be called on packet TX on a given port and queue.
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
 *   callback function on this port and queue.
 *
 * @return
 *   NULL on error.
 *   On success, a pointer value which can later be used to remove the callback.
 */
void *rte_eth_add_tx_callback(uint8_t port_id, uint16_t queue_id,
		rte_tx_callback_fn fn, void *user_param);

/**
 * Remove an RX packet callback from a given port and queue.
 *
 * This function is used to removed callbacks that were added to a NIC port
 * queue using rte_eth_add_rx_callback().
 *
 * Note: the callback is removed from the callback list but it isn't freed
 * since the it may still be in use. The memory for the callback can be
 * subsequently freed back by the application by calling rte_free():
 *
 * - Immediately - if the port is stopped, or the user knows that no
 *   callbacks are in flight e.g. if called from the thread doing RX/TX
 *   on that queue.
 *
 * - After a short delay - where the delay is sufficient to allow any
 *   in-flight callbacks to complete.
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
 *   - -ENOTSUP: Callback support is not available.
 *   - -EINVAL:  The port_id or the queue_id is out of range, or the callback
 *               is NULL or not found for the port/queue.
 */
int rte_eth_remove_rx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb);

/**
 * Remove a TX packet callback from a given port and queue.
 *
 * This function is used to removed callbacks that were added to a NIC port
 * queue using rte_eth_add_tx_callback().
 *
 * Note: the callback is removed from the callback list but it isn't freed
 * since the it may still be in use. The memory for the callback can be
 * subsequently freed back by the application by calling rte_free():
 *
 * - Immediately - if the port is stopped, or the user knows that no
 *   callbacks are in flight e.g. if called from the thread doing RX/TX
 *   on that queue.
 *
 * - After a short delay - where the delay is sufficient to allow any
 *   in-flight callbacks to complete.
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
 *   - -ENOTSUP: Callback support is not available.
 *   - -EINVAL:  The port_id or the queue_id is out of range, or the callback
 *               is NULL or not found for the port/queue.
 */
int rte_eth_remove_tx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb);

/**
 * Retrieve information about given port's RX queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The RX queue on the Ethernet device for which information
 *   will be retrieved.
 * @param qinfo
 *   A pointer to a structure of type *rte_eth_rxq_info_info* to be filled with
 *   the information of the Ethernet device.
 *
 * @return
 *   - 0: Success
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The port_id or the queue_id is out of range.
 */
int rte_eth_rx_queue_info_get(uint8_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

/**
 * Retrieve information about given port's TX queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The TX queue on the Ethernet device for which information
 *   will be retrieved.
 * @param qinfo
 *   A pointer to a structure of type *rte_eth_txq_info_info* to be filled with
 *   the information of the Ethernet device.
 *
 * @return
 *   - 0: Success
 *   - -ENOTSUP: routine is not supported by the device PMD.
 *   - -EINVAL:  The port_id or the queue_id is out of range.
 */
int rte_eth_tx_queue_info_get(uint8_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

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
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_reg_info(uint8_t port_id, struct rte_dev_reg_info *info);

/**
 * Retrieve size of device EEPROM
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (>=0) EEPROM size if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_eeprom_length(uint8_t port_id);

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
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_get_eeprom(uint8_t port_id, struct rte_dev_eeprom_info *info);

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
 *   - others depends on the specific operations implementation.
 */
int rte_eth_dev_set_eeprom(uint8_t port_id, struct rte_dev_eeprom_info *info);

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
 *   - (-ENOTSUP) if PMD of *port_id* doesn't support multicast filtering.
 *   - (-ENOSPC) if *port_id* has not enough multicast filtering resources.
 */
int rte_eth_dev_set_mc_addr_list(uint8_t port_id,
				 struct ether_addr *mc_addr_set,
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
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_enable(uint8_t port_id);

/**
 * Disable IEEE1588/802.1AS timestamping for an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 *
 * @return
 *   - 0: Success.
 *   - -ENODEV: The port ID is invalid.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_disable(uint8_t port_id);

/**
 * Read an IEEE1588/802.1AS RX timestamp from an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param timestamp
 *   Pointer to the timestamp struct.
 * @param flags
 *   Device specific flags. Used to pass the RX timesync register index to
 *   i40e. Unused in igb/ixgbe, pass 0 instead.
 *
 * @return
 *   - 0: Success.
 *   - -EINVAL: No timestamp is available.
 *   - -ENODEV: The port ID is invalid.
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_read_rx_timestamp(uint8_t port_id,
		struct timespec *timestamp, uint32_t flags);

/**
 * Read an IEEE1588/802.1AS TX timestamp from an Ethernet device.
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
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_read_tx_timestamp(uint8_t port_id,
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
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_adjust_time(uint8_t port_id, int64_t delta);

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
 */
int rte_eth_timesync_read_time(uint8_t port_id, struct timespec *time);

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
 *   - -ENOTSUP: The function is not supported by the Ethernet driver.
 */
int rte_eth_timesync_write_time(uint8_t port_id, const struct timespec *time);

/**
 * Copy pci device info to the Ethernet device data.
 *
 * @param eth_dev
 * The *eth_dev* pointer is the address of the *rte_eth_dev* structure.
 * @param pci_dev
 * The *pci_dev* pointer is the address of the *rte_pci_device* structure.
 *
 * @return
 *   - 0 on success, negative on error
 */
void rte_eth_copy_pci_info(struct rte_eth_dev *eth_dev,
		struct rte_pci_device *pci_dev);

/**
 * Create memzone for HW rings.
 * malloc can't be used as the physical address is needed.
 * If the memzone is already created, then this function returns a ptr
 * to the old one.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 * @param name
 *   The name of the memory zone
 * @param queue_id
 *   The index of the queue to add to name
 * @param size
 *   The sizeof of the memory area
 * @param align
 *   Alignment for resulting memzone. Must be a power of 2.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 */
const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *eth_dev, const char *name,
			 uint16_t queue_id, size_t size,
			 unsigned align, int socket_id);

/**
 * Config l2 tunnel ether type of an Ethernet device for filtering specific
 * tunnel packets by ether type.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param l2_tunnel
 *   l2 tunnel configuration.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_l2_tunnel_eth_type_conf(uint8_t port_id,
				    struct rte_eth_l2_tunnel_conf *l2_tunnel);

/**
 * Enable/disable l2 tunnel offload functions. Include,
 * 1, The ability of parsing a type of l2 tunnel of an Ethernet device.
 *    Filtering, forwarding and offloading this type of tunnel packets depend on
 *    this ability.
 * 2, Stripping the l2 tunnel tag.
 * 3, Insertion of the l2 tunnel tag.
 * 4, Forwarding the packets based on the l2 tunnel tag.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param l2_tunnel
 *   l2 tunnel parameters.
 * @param mask
 *   Indicate the offload function.
 * @param en
 *   Enable or disable this function.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if port identifier is invalid.
 *   - (-ENOTSUP) if hardware doesn't support tunnel type.
 */
int
rte_eth_dev_l2_tunnel_offload_set(uint8_t port_id,
				  struct rte_eth_l2_tunnel_conf *l2_tunnel,
				  uint32_t mask,
				  uint8_t en);

/**
* Get the port id from pci adrress or device name
* Ex: 0000:2:00.0 or vdev name eth_pcap0
*
* @param name
*  pci address or name of the device
* @param port_id
*   pointer to port identifier of the device
* @return
*   - (0) if successful.
*   - (-ENODEV or -EINVAL) on failure.
*/
int
rte_eth_dev_get_port_by_name(const char *name, uint8_t *port_id);

/**
* Get the device name from port id
*
* @param port_id
*   pointer to port identifier of the device
* @param name
*  pci address or name of the device
* @return
*   - (0) if successful.
*   - (-EINVAL) on failure.
*/
int
rte_eth_dev_get_name_by_port(uint8_t port_id, char *name);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_H_ */
