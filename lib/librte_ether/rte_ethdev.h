/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
	uint64_t imissed;   /**< Total of RX missed packets (e.g full FIFO). */
	uint64_t ibadcrc;   /**< Total of RX packets with CRC error. */
	uint64_t ibadlen;   /**< Total of RX packets with bad length. */
	uint64_t ierrors;   /**< Total number of erroneous received packets. */
	uint64_t oerrors;   /**< Total number of failed transmitted packets. */
	uint64_t imcasts;   /**< Total number of multicast received packets. */
	uint64_t rx_nombuf; /**< Total number of RX mbuf allocation failures. */
	uint64_t fdirmatch; /**< Total number of RX packets matching a filter. */
	uint64_t fdirmiss;  /**< Total number of RX packets not matching any filter. */
	uint64_t tx_pause_xon;  /**< Total nb. of XON pause frame sent. */
	uint64_t rx_pause_xon;  /**< Total nb. of XON pause frame received. */
	uint64_t tx_pause_xoff; /**< Total nb. of XOFF pause frame sent. */
	uint64_t rx_pause_xoff; /**< Total nb. of XOFF pause frame received. */
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
	uint64_t ilbpackets;
	/**< Total number of good packets received from loopback,VF Only */
	uint64_t olbpackets;
	/**< Total number of good packets transmitted to loopback,VF Only */
	uint64_t ilbbytes;
	/**< Total number of good bytes received from loopback,VF Only */
	uint64_t olbbytes;
	/**< Total number of good bytes transmitted to loopback,VF Only */
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
#define ETH_LINK_SPEED_10G      10000   /**< alias of 10 gigabits/second. */
#define ETH_LINK_SPEED_20G      20000   /**< 20 gigabits/second. */
#define ETH_LINK_SPEED_40G      40000   /**< 40 gigabits/second. */

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
	ETH_MQ_RX_NONE = 0,  /**< None of DCB,RSS or VMDQ mode */

	ETH_MQ_RX_RSS,       /**< For RX side, only RSS is on */
	ETH_MQ_RX_DCB,       /**< For RX side,only DCB is on. */
	ETH_MQ_RX_DCB_RSS,   /**< Both DCB and RSS enable */

	ETH_MQ_RX_VMDQ_ONLY, /**< Only VMDQ, no RSS nor DCB */
	ETH_MQ_RX_VMDQ_RSS,  /**< RSS mode with VMDQ */
	ETH_MQ_RX_VMDQ_DCB,  /**< Use VMDQ+DCB to route traffic to queues */
	ETH_MQ_RX_VMDQ_DCB_RSS, /**< Enable both VMDQ and DCB in VMDq */
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
	uint8_t header_split : 1, /**< Header Split enable. */
		hw_ip_checksum   : 1, /**< IP/UDP/TCP checksum offload enable. */
		hw_vlan_filter   : 1, /**< VLAN filter enable. */
		hw_vlan_strip    : 1, /**< VLAN strip enable. */
		hw_vlan_extend   : 1, /**< Extended VLAN enable. */
		jumbo_frame      : 1, /**< Jumbo Frame Receipt enable. */
		hw_strip_crc     : 1, /**< Enable CRC stripping by hardware. */
		enable_scatter   : 1; /**< Enable scatter packets rx handler */
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

/* Supported RSS offloads */
/* for 1G & 10G */
#define ETH_RSS_IPV4_SHIFT                    0
#define ETH_RSS_IPV4_TCP_SHIFT                1
#define ETH_RSS_IPV6_SHIFT                    2
#define ETH_RSS_IPV6_EX_SHIFT                 3
#define ETH_RSS_IPV6_TCP_SHIFT                4
#define ETH_RSS_IPV6_TCP_EX_SHIFT             5
#define ETH_RSS_IPV4_UDP_SHIFT                6
#define ETH_RSS_IPV6_UDP_SHIFT                7
#define ETH_RSS_IPV6_UDP_EX_SHIFT             8
/* for 40G only */
#define ETH_RSS_NONF_IPV4_UDP_SHIFT           31
#define ETH_RSS_NONF_IPV4_TCP_SHIFT           33
#define ETH_RSS_NONF_IPV4_SCTP_SHIFT          34
#define ETH_RSS_NONF_IPV4_OTHER_SHIFT         35
#define ETH_RSS_FRAG_IPV4_SHIFT               36
#define ETH_RSS_NONF_IPV6_UDP_SHIFT           41
#define ETH_RSS_NONF_IPV6_TCP_SHIFT           43
#define ETH_RSS_NONF_IPV6_SCTP_SHIFT          44
#define ETH_RSS_NONF_IPV6_OTHER_SHIFT         45
#define ETH_RSS_FRAG_IPV6_SHIFT               46
#define ETH_RSS_FCOE_OX_SHIFT                 48
#define ETH_RSS_FCOE_RX_SHIFT                 49
#define ETH_RSS_FCOE_OTHER_SHIFT              50
#define ETH_RSS_L2_PAYLOAD_SHIFT              63

/* for 1G & 10G */
#define ETH_RSS_IPV4                    ((uint16_t)1 << ETH_RSS_IPV4_SHIFT)
#define ETH_RSS_IPV4_TCP                ((uint16_t)1 << ETH_RSS_IPV4_TCP_SHIFT)
#define ETH_RSS_IPV6                    ((uint16_t)1 << ETH_RSS_IPV6_SHIFT)
#define ETH_RSS_IPV6_EX                 ((uint16_t)1 << ETH_RSS_IPV6_EX_SHIFT)
#define ETH_RSS_IPV6_TCP                ((uint16_t)1 << ETH_RSS_IPV6_TCP_SHIFT)
#define ETH_RSS_IPV6_TCP_EX             ((uint16_t)1 << ETH_RSS_IPV6_TCP_EX_SHIFT)
#define ETH_RSS_IPV4_UDP                ((uint16_t)1 << ETH_RSS_IPV4_UDP_SHIFT)
#define ETH_RSS_IPV6_UDP                ((uint16_t)1 << ETH_RSS_IPV6_UDP_SHIFT)
#define ETH_RSS_IPV6_UDP_EX             ((uint16_t)1 << ETH_RSS_IPV6_UDP_EX_SHIFT)
/* for 40G only */
#define ETH_RSS_NONF_IPV4_UDP           ((uint64_t)1 << ETH_RSS_NONF_IPV4_UDP_SHIFT)
#define ETH_RSS_NONF_IPV4_TCP           ((uint64_t)1 << ETH_RSS_NONF_IPV4_TCP_SHIFT)
#define ETH_RSS_NONF_IPV4_SCTP          ((uint64_t)1 << ETH_RSS_NONF_IPV4_SCTP_SHIFT)
#define ETH_RSS_NONF_IPV4_OTHER         ((uint64_t)1 << ETH_RSS_NONF_IPV4_OTHER_SHIFT)
#define ETH_RSS_FRAG_IPV4               ((uint64_t)1 << ETH_RSS_FRAG_IPV4_SHIFT)
#define ETH_RSS_NONF_IPV6_UDP           ((uint64_t)1 << ETH_RSS_NONF_IPV6_UDP_SHIFT)
#define ETH_RSS_NONF_IPV6_TCP           ((uint64_t)1 << ETH_RSS_NONF_IPV6_TCP_SHIFT)
#define ETH_RSS_NONF_IPV6_SCTP          ((uint64_t)1 << ETH_RSS_NONF_IPV6_SCTP_SHIFT)
#define ETH_RSS_NONF_IPV6_OTHER         ((uint64_t)1 << ETH_RSS_NONF_IPV6_OTHER_SHIFT)
#define ETH_RSS_FRAG_IPV6               ((uint64_t)1 << ETH_RSS_FRAG_IPV6_SHIFT)
#define ETH_RSS_FCOE_OX                 ((uint64_t)1 << ETH_RSS_FCOE_OX_SHIFT) /* not used */
#define ETH_RSS_FCOE_RX                 ((uint64_t)1 << ETH_RSS_FCOE_RX_SHIFT) /* not used */
#define ETH_RSS_FCOE_OTHER              ((uint64_t)1 << ETH_RSS_FCOE_OTHER_SHIFT) /* not used */
#define ETH_RSS_L2_PAYLOAD              ((uint64_t)1 << ETH_RSS_L2_PAYLOAD_SHIFT)

#define ETH_RSS_IP ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV6 | \
		ETH_RSS_NONF_IPV4_OTHER | \
		ETH_RSS_FRAG_IPV4 | \
		ETH_RSS_NONF_IPV6_OTHER | \
		ETH_RSS_FRAG_IPV6)
#define ETH_RSS_UDP ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV4_UDP | \
		ETH_RSS_IPV6_UDP | \
		ETH_RSS_IPV6_UDP_EX | \
		ETH_RSS_NONF_IPV4_UDP | \
		ETH_RSS_NONF_IPV6_UDP)
/**< Mask of valid RSS hash protocols */
#define ETH_RSS_PROTO_MASK ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV4_TCP | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV6_EX | \
		ETH_RSS_IPV6_TCP | \
		ETH_RSS_IPV6_TCP_EX | \
		ETH_RSS_IPV4_UDP | \
		ETH_RSS_IPV6_UDP | \
		ETH_RSS_IPV6_UDP_EX | \
		ETH_RSS_NONF_IPV4_UDP | \
		ETH_RSS_NONF_IPV4_TCP | \
		ETH_RSS_NONF_IPV4_SCTP | \
		ETH_RSS_NONF_IPV4_OTHER | \
		ETH_RSS_FRAG_IPV4 | \
		ETH_RSS_NONF_IPV6_UDP | \
		ETH_RSS_NONF_IPV6_TCP | \
		ETH_RSS_NONF_IPV6_SCTP | \
		ETH_RSS_NONF_IPV6_OTHER | \
		ETH_RSS_FRAG_IPV6 | \
		ETH_RSS_L2_PAYLOAD)

/* Definitions used for redirection table entry size */
#define ETH_RSS_RETA_NUM_ENTRIES 128
#define ETH_RSS_RETA_MAX_QUEUE   16

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

/* Definitions used for VMDQ mirror rules setting */
#define ETH_VMDQ_NUM_MIRROR_RULE     4 /**< Maximum nb. of mirror rules. . */

#define ETH_VMDQ_POOL_MIRROR    0x0001 /**< Virtual Pool Mirroring. */
#define ETH_VMDQ_UPLINK_MIRROR  0x0002 /**< Uplink Port Mirroring. */
#define ETH_VMDQ_DOWNLIN_MIRROR 0x0004 /**< Downlink Port Mirroring. */
#define ETH_VMDQ_VLAN_MIRROR    0x0008 /**< VLAN Mirroring. */

/**
 * A structure used to configure VLAN traffic mirror of an Ethernet port.
 */
struct rte_eth_vlan_mirror {
	uint64_t vlan_mask; /**< mask for valid VLAN ID. */
	uint16_t vlan_id[ETH_VMDQ_MAX_VLAN_FILTERS];
	/** VLAN ID list for vlan mirror. */
};

/**
 * A structure used to configure traffic mirror of an Ethernet port.
 */
struct rte_eth_vmdq_mirror_conf {
	uint8_t rule_type_mask; /**< Mirroring rule type mask we want to set */
	uint8_t dst_pool; /**< Destination pool for this mirror rule. */
	uint64_t pool_mask; /**< Bitmap of pool for pool mirroring */
	struct rte_eth_vlan_mirror vlan; /**< VLAN ID setting for VLAN mirroring */
};

/**
 * A structure used to configure Redirection Table of  the Receive Side
 * Scaling (RSS) feature of an Ethernet port.
 */
struct rte_eth_rss_reta {
	/** First 64 mask bits indicate which entry(s) need to updated/queried. */
	uint64_t mask_lo;
	/** Second 64 mask bits indicate which entry(s) need to updated/queried. */
	uint64_t mask_hi;
	uint8_t reta[ETH_RSS_RETA_NUM_ENTRIES];  /**< 128 RETA entries*/
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
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
};

struct rte_eth_vmdq_dcb_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
};

struct rte_eth_dcb_tx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
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
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Selects a queue in a pool */
};

struct rte_eth_vmdq_rx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq only mode, 8 or 64 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t enable_loop_back; /**< Enable VT loop back */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
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
	uint8_t start_rx_per_q; /**< start rx per queue. */
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
/**
 * A structure used to configure a TX ring of an Ethernet port.
 */
struct rte_eth_txconf {
	struct rte_eth_thresh tx_thresh; /**< TX ring threshold registers. */
	uint16_t tx_rs_thresh; /**< Drives the setting of RS bit on TXDs. */
	uint16_t tx_free_thresh; /**< Drives the freeing of TX buffers. */
	uint32_t txq_flags; /**< Set flags for the Tx queue */
	uint8_t start_tx_per_q; /**< start tx per queue. */
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
	/** Offset of flexbytes field in RX packets (in 16-bit word units). */
	uint8_t flexbytes_offset;
	/** RX queue of packets matching a "drop" filter in perfect mode. */
	uint8_t drop_queue;
};

/**
 *  Possible l4type of FDIR filters.
 */
enum rte_l4type {
	RTE_FDIR_L4TYPE_NONE = 0,       /**< None. */
	RTE_FDIR_L4TYPE_UDP,            /**< UDP. */
	RTE_FDIR_L4TYPE_TCP,            /**< TCP. */
	RTE_FDIR_L4TYPE_SCTP,           /**< SCTP. */
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
 *  @note The only_ip_flow field has the opposite meaning compared to other
 *  masks!
 */
struct rte_fdir_masks {
	/** When set to 1, packet l4type is \b NOT relevant in filters, and
	   source and destination port masks must be set to zero. */
	uint8_t only_ip_flow;
	/** If set to 1, vlan_id is relevant in filters. */
	uint8_t vlan_id;
	/** If set to 1, vlan_prio is relevant in filters. */
	uint8_t vlan_prio;
	/** If set to 1, flexbytes is relevant in filters. */
	uint8_t flexbytes;
	/** If set to 1, set the IPv6 masks. Otherwise set the IPv4 masks. */
	uint8_t set_ipv6_mask;
	/** When set to 1, comparison of destination IPv6 address with IP6AT
	    registers is meaningful. */
	uint8_t comp_ipv6_dst;
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
	uint16_t dst_ipv6_mask;
	/** Mask of Destination IPv6 Address. All bits set to 1 define the
	    relevant BYTES to use in the destination address of an IPv6 packet
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
	uint32_t lpbk_mode; /**< Loopback operation mode. By default the value
			         is 0, meaning the loopback mode is disabled.
				 Read the datasheet of given ethernet controller
				 for details. The possible values of this field
				 are defined in implementation of each driver. */
	union {
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
};

struct rte_eth_dev;

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);

#define TCP_UGR_FLAG 0x20
#define TCP_ACK_FLAG 0x10
#define TCP_PSH_FLAG 0x08
#define TCP_RST_FLAG 0x04
#define TCP_SYN_FLAG 0x02
#define TCP_FIN_FLAG 0x01
#define TCP_FLAG_ALL 0x3F

/**
 *  A structure used to define an ethertype filter.
 */
struct rte_ethertype_filter {
	uint16_t ethertype;  /**< little endian. */
	uint8_t priority_en; /**< compare priority enable. */
	uint8_t priority;
};

/**
 *  A structure used to define an syn filter.
 */
struct rte_syn_filter {
	uint8_t hig_pri; /**< 1 means higher pri than 2tuple, 5tupe,
			      and flex filter, 0 means lower pri. */
};

/**
 *  A structure used to define a 2tuple filter.
 */
struct rte_2tuple_filter {
	uint16_t dst_port;        /**< big endian. */
	uint8_t protocol;
	uint8_t tcp_flags;
	uint16_t priority;        /**< used when more than one filter matches. */
	uint8_t dst_port_mask:1,  /**< if mask is 1b, means not compare. */
		protocol_mask:1;
};

/**
 *  A structure used to define a flex filter.
 */
struct rte_flex_filter {
	uint16_t len;
	uint32_t dwords[32];  /**< flex bytes in big endian. */
	uint8_t mask[16];     /**< if mask bit is 1b, do not compare
				   corresponding byte in dwords. */
	uint8_t priority;
};

/**
 *  A structure used to define a 5tuple filter.
 */
struct rte_5tuple_filter {
	uint32_t dst_ip;         /**< destination IP address in big endian. */
	uint32_t src_ip;         /**< source IP address in big endian. */
	uint16_t dst_port;       /**< destination port in big endian. */
	uint16_t src_port;       /**< source Port big endian. */
	uint8_t protocol;        /**< l4 protocol. */
	uint8_t tcp_flags;       /**< tcp flags. */
	uint16_t priority;       /**< seven evels (001b-111b), 111b is highest,
				      used when more than one filter matches. */
	uint8_t dst_ip_mask:1,   /**< if mask is 1b, do not compare dst ip. */
		src_ip_mask:1,   /**< if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /**< if mask is 1b, do not compare dst port. */
		src_port_mask:1, /**< if mask is 1b, do not compare src port. */
		protocol_mask:1; /**< if mask is 1b, do not compare protocol. */
};

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

typedef int (*eth_queue_stats_mapping_set_t)(struct rte_eth_dev *dev,
					     uint16_t queue_id,
					     uint8_t stat_idx,
					     uint8_t is_rx);
/**< @internal Set a queue statistics mapping for a tx/rx queue of an Ethernet device. */

typedef void (*eth_dev_infos_get_t)(struct rte_eth_dev *dev,
				    struct rte_eth_dev_info *dev_info);
/**< @internal Get specific informations of an Ethernet device. */

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

typedef void (*eth_queue_release_t)(void *queue);
/**< @internal Release memory resources allocated by given RX/TX queue. */

typedef uint32_t (*eth_rx_queue_count_t)(struct rte_eth_dev *dev,
					 uint16_t rx_queue_id);
/**< @Get number of available descriptors on a receive queue of an Ethernet device. */

typedef int (*eth_rx_descriptor_done_t)(void *rxq, uint16_t offset);
/**< @Check DD bit of specific RX descriptor */

typedef int (*mtu_set_t)(struct rte_eth_dev *dev, uint16_t mtu);
/**< @internal Set MTU. */

typedef int (*vlan_filter_set_t)(struct rte_eth_dev *dev,
				  uint16_t vlan_id,
				  int on);
/**< @internal filtering of a VLAN Tag Identifier by an Ethernet device. */

typedef void (*vlan_tpid_set_t)(struct rte_eth_dev *dev,
				  uint16_t tpid);
/**< @internal set the outer VLAN-TPID by an Ethernet device. */

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

typedef int (*fdir_add_signature_filter_t)(struct rte_eth_dev *dev,
					   struct rte_fdir_filter *fdir_ftr,
					   uint8_t rx_queue);
/**< @internal Setup a new signature filter rule on an Ethernet device */

typedef int (*fdir_update_signature_filter_t)(struct rte_eth_dev *dev,
					      struct rte_fdir_filter *fdir_ftr,
					      uint8_t rx_queue);
/**< @internal Update a signature filter rule on an Ethernet device */

typedef int (*fdir_remove_signature_filter_t)(struct rte_eth_dev *dev,
					      struct rte_fdir_filter *fdir_ftr);
/**< @internal Remove a  signature filter rule on an Ethernet device */

typedef void (*fdir_infos_get_t)(struct rte_eth_dev *dev,
				 struct rte_eth_fdir *fdir);
/**< @internal Get information about fdir status */

typedef int (*fdir_add_perfect_filter_t)(struct rte_eth_dev *dev,
					 struct rte_fdir_filter *fdir_ftr,
					 uint16_t soft_id, uint8_t rx_queue,
					 uint8_t drop);
/**< @internal Setup a new perfect filter rule on an Ethernet device */

typedef int (*fdir_update_perfect_filter_t)(struct rte_eth_dev *dev,
					    struct rte_fdir_filter *fdir_ftr,
					    uint16_t soft_id, uint8_t rx_queue,
					    uint8_t drop);
/**< @internal Update a perfect filter rule on an Ethernet device */

typedef int (*fdir_remove_perfect_filter_t)(struct rte_eth_dev *dev,
					    struct rte_fdir_filter *fdir_ftr,
					    uint16_t soft_id);
/**< @internal Remove a perfect filter rule on an Ethernet device */

typedef int (*fdir_set_masks_t)(struct rte_eth_dev *dev,
				struct rte_fdir_masks *fdir_masks);
/**< @internal Setup flow director masks on an Ethernet device */

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
				struct rte_eth_rss_reta *reta_conf);
/**< @internal Update RSS redirection table on an Ethernet device */

typedef int (*reta_query_t)(struct rte_eth_dev *dev,
				struct rte_eth_rss_reta *reta_conf);
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
				  struct rte_eth_vmdq_mirror_conf *mirror_conf,
				  uint8_t rule_id,
				  uint8_t on);
/**< @internal Add a traffic mirroring rule on an Ethernet device */

typedef int (*eth_mirror_rule_reset_t)(struct rte_eth_dev *dev,
				  uint8_t rule_id);
/**< @internal Remove a traffic mirroring rule on an Ethernet device */

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

typedef int (*eth_add_syn_filter_t)(struct rte_eth_dev *dev,
			struct rte_syn_filter *filter, uint16_t rx_queue);
/**< @internal add syn filter rule on an Ethernet device */

typedef int (*eth_remove_syn_filter_t)(struct rte_eth_dev *dev);
/**< @internal remove syn filter rule on an Ethernet device */

typedef int (*eth_get_syn_filter_t)(struct rte_eth_dev *dev,
			struct rte_syn_filter *filter, uint16_t *rx_queue);
/**< @internal Get syn filter rule on an Ethernet device */

typedef int (*eth_add_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_ethertype_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new ethertype filter rule on an Ethernet device */

typedef int (*eth_remove_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove an ethertype filter rule on an Ethernet device */

typedef int (*eth_get_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_ethertype_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get an ethertype filter rule on an Ethernet device */

typedef int (*eth_add_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_2tuple_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new 2tuple filter rule on an Ethernet device */

typedef int (*eth_remove_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a 2tuple filter rule on an Ethernet device */

typedef int (*eth_get_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_2tuple_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a 2tuple filter rule on an Ethernet device */

typedef int (*eth_add_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_5tuple_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new 5tuple filter rule on an Ethernet device */

typedef int (*eth_remove_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a 5tuple filter rule on an Ethernet device */

typedef int (*eth_get_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_5tuple_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a 5tuple filter rule on an Ethernet device */

typedef int (*eth_add_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_flex_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new flex filter rule on an Ethernet device */

typedef int (*eth_remove_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a flex filter rule on an Ethernet device */

typedef int (*eth_get_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_flex_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a flex filter rule on an Ethernet device */

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
	eth_stats_get_t            stats_get;     /**< Get device statistics. */
	eth_stats_reset_t          stats_reset;   /**< Reset device statistics. */
	eth_queue_stats_mapping_set_t queue_stats_mapping_set;
	/**< Configure per queue stat counter mapping. */
	eth_dev_infos_get_t        dev_infos_get; /**< Get device info. */
	mtu_set_t                  mtu_set; /**< Set MTU. */
	vlan_filter_set_t          vlan_filter_set;  /**< Filter VLAN Setup. */
	vlan_tpid_set_t            vlan_tpid_set;      /**< Outer VLAN TPID Setup. */
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
	eth_tx_queue_setup_t       tx_queue_setup;/**< Set up device TX queue.*/
	eth_queue_release_t        tx_queue_release;/**< Release TX queue.*/
	eth_dev_led_on_t           dev_led_on;    /**< Turn on LED. */
	eth_dev_led_off_t          dev_led_off;   /**< Turn off LED. */
	flow_ctrl_get_t            flow_ctrl_get; /**< Get flow control. */
	flow_ctrl_set_t            flow_ctrl_set; /**< Setup flow control. */
	priority_flow_ctrl_set_t   priority_flow_ctrl_set; /**< Setup priority flow control.*/
	eth_mac_addr_remove_t      mac_addr_remove; /**< Remove MAC address */
	eth_mac_addr_add_t         mac_addr_add;  /**< Add a MAC address */
	eth_uc_hash_table_set_t    uc_hash_table_set;  /**< Set Unicast Table Array */
	eth_uc_all_hash_table_set_t uc_all_hash_table_set;  /**< Set Unicast hash bitmap */
	eth_mirror_rule_set_t	   mirror_rule_set;  /**< Add a traffic mirror rule.*/
	eth_mirror_rule_reset_t	   mirror_rule_reset;  /**< reset a traffic mirror rule.*/
	eth_set_vf_rx_mode_t       set_vf_rx_mode;   /**< Set VF RX mode */
	eth_set_vf_rx_t            set_vf_rx;  /**< enable/disable a VF receive */
	eth_set_vf_tx_t            set_vf_tx;  /**< enable/disable a VF transmit */
	eth_set_vf_vlan_filter_t   set_vf_vlan_filter;  /**< Set VF VLAN filter */
	eth_set_queue_rate_limit_t set_queue_rate_limit;   /**< Set queue rate limit */
	eth_set_vf_rate_limit_t    set_vf_rate_limit;   /**< Set VF rate limit */

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
	/** Update redirection table. */
	reta_update_t reta_update;
	/** Query redirection table. */
	reta_query_t reta_query;
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
	eth_add_syn_filter_t           add_syn_filter;       /**< add syn filter. */
	eth_remove_syn_filter_t        remove_syn_filter;    /**< remove syn filter. */
	eth_get_syn_filter_t           get_syn_filter;       /**< get syn filter. */
	eth_add_ethertype_filter_t     add_ethertype_filter;    /**< add ethertype filter. */
	eth_remove_ethertype_filter_t  remove_ethertype_filter; /**< remove ethertype filter. */
	eth_get_ethertype_filter_t     get_ethertype_filter;    /**< get ethertype filter. */
	eth_add_2tuple_filter_t        add_2tuple_filter;    /**< add 2tuple filter. */
	eth_remove_2tuple_filter_t     remove_2tuple_filter; /**< remove 2tuple filter. */
	eth_get_2tuple_filter_t        get_2tuple_filter;    /**< get 2tuple filter. */
	eth_add_5tuple_filter_t        add_5tuple_filter;    /**< add 5tuple filter. */
	eth_remove_5tuple_filter_t     remove_5tuple_filter; /**< remove 5tuple filter. */
	eth_get_5tuple_filter_t        get_5tuple_filter;    /**< get 5tuple filter. */
	eth_add_flex_filter_t          add_flex_filter;      /**< add flex filter. */
	eth_remove_flex_filter_t       remove_flex_filter;   /**< remove flex filter. */
	eth_get_flex_filter_t          get_flex_filter;      /**< get flex filter. */
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
	struct eth_dev_ops *dev_ops;    /**< Functions exported by PMD */
	struct rte_pci_device *pci_dev; /**< PCI info. supplied by probing */
	struct rte_eth_dev_cb_list callbacks; /**< User application callbacks */
};

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
		dev_started : 1;   /**< Device state: STARTED(1) / STOPPED(0). */
};

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
 * [0,  rte_eth_dev_count() - 1] can be operated on by network applications.
 *
 * @return
 *   - The total number of usable Ethernet devices.
 */
extern uint8_t rte_eth_dev_count(void);

/**
 * Function for internal use by dummy drivers primarily, e.g. ring-based
 * driver.
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param	name	Unique identifier name for each Ethernet device
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
struct rte_eth_dev *rte_eth_dev_allocate(const char *name);

struct eth_driver;
/**
 * @internal
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
 * - The size of the private data to allocate for each matching device.
 */
struct eth_driver {
	struct rte_pci_driver pci_drv;    /**< The PMD is also a PCI driver. */
	eth_dev_init_t eth_dev_init;      /**< Device init function. */
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
extern void rte_eth_driver_register(struct eth_driver *eth_drv);

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
extern int rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
				  uint16_t nb_tx_desc, unsigned int socket_id,
				  const struct rte_eth_txconf *tx_conf);

/*
 * Return the NUMA socket to which an Ethernet device is connected
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @return
 *   The NUMA socket id to which the Ethernet device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 is returned is the port_id value is out of range.
 */
extern int rte_eth_dev_socket_id(uint8_t port_id);

/*
 * Start specified RX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int rte_eth_dev_rx_queue_start(uint8_t port_id, uint16_t rx_queue_id);

/*
 * Stop specified RX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param rx_queue_id
 *   The index of the rx queue to update the ring.
 *   The value must be in the range [0, nb_rx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int rte_eth_dev_rx_queue_stop(uint8_t port_id, uint16_t rx_queue_id);

/*
 * Start specified TX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int rte_eth_dev_tx_queue_start(uint8_t port_id, uint16_t tx_queue_id);

/*
 * Stop specified TX queue of a port
 *
 * @param port_id
 *   The port identifier of the Ethernet device
 * @param tx_queue_id
 *   The index of the tx queue to update the ring.
 *   The value must be in the range [0, nb_tx_queue - 1] previously supplied
 *   to rte_eth_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The port_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int rte_eth_dev_tx_queue_stop(uint8_t port_id, uint16_t tx_queue_id);



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
extern int rte_eth_dev_set_link_up(uint8_t port_id);

/**
 * Link down an Ethernet device.
 * The device rx/tx functionality will be disabled if success,
 * and it can be re-enabled with a call to
 * rte_eth_dev_set_link_up()
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 */
extern int rte_eth_dev_set_link_down(uint8_t port_id);

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
extern int rte_eth_dev_set_tx_queue_stats_mapping(uint8_t port_id,
						  uint16_t tx_queue_id,
						  uint8_t stat_idx);

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
extern int rte_eth_dev_set_rx_queue_stats_mapping(uint8_t port_id,
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
extern int rte_eth_dev_get_mtu(uint8_t port_id, uint16_t *mtu);

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
 */
extern int rte_eth_dev_set_mtu(uint8_t port_id, uint16_t mtu);

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
extern int rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id , int on);

/**
 * Enable/Disable hardware VLAN Strip by a rx queue of an Ethernet device.
 * 82599/X540 can support VLAN stripping at the rx queue level
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
extern int rte_eth_dev_set_vlan_strip_on_queue(uint8_t port_id,
		uint16_t rx_queue_id, int on);

/**
 * Set the Outer VLAN Ether Type by an Ethernet device, it can be inserted to
 * the VLAN Header. This is a register setup available on some Intel NIC, not
 * but all, please check the data sheet for availability.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param tag_type
 *   The Tag Protocol ID
 * @return
 *   - (0) if successful.
 *   - (-ENOSUP) if hardware-assisted VLAN TPID setup is not supported.
 *   - (-ENODEV) if *port_id* invalid.
 */
extern int rte_eth_dev_set_vlan_ether_type(uint8_t port_id, uint16_t tag_type);

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
extern int rte_eth_dev_set_vlan_offload(uint8_t port_id, int offload_mask);

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
extern int rte_eth_dev_get_vlan_offload(uint8_t port_id);

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
extern int rte_eth_dev_set_vlan_pvid(uint8_t port_id, uint16_t pvid, int on);

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
 * Get the number of used descriptors in a specific queue
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The queue id on the specific port.
 * @return
 *  The number of used descriptors in the specific queue.
 */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
extern uint32_t rte_eth_rx_queue_count(uint8_t port_id, uint16_t queue_id);
#else
static inline uint32_t
rte_eth_rx_queue_count(uint8_t port_id, uint16_t queue_id)
{
        struct rte_eth_dev *dev;

        dev = &rte_eth_devices[port_id];
        return (*dev->dev_ops->rx_queue_count)(dev, queue_id);
}
#endif

/**
 * Check if the DD bit of the specific RX descriptor in the queue has been set
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The queue id on the specific port.
 * @offset
 *  The offset of the descriptor ID from tail.
 * @return
 *  - (1) if the specific DD bit is set.
 *  - (0) if the specific DD bit is not set.
 *  - (-ENODEV) if *port_id* invalid.
 */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
extern int rte_eth_rx_descriptor_done(uint8_t port_id,
				      uint16_t queue_id,
				      uint16_t offset);
#else
static inline int
rte_eth_rx_descriptor_done(uint8_t port_id, uint16_t queue_id, uint16_t offset)
{
	struct rte_eth_dev *dev;

	dev = &rte_eth_devices[port_id];
	return (*dev->dev_ops->rx_descriptor_done)( \
		dev->data->rx_queues[queue_id], offset);
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
 * Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *    RETA to update.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_reta_update(uint8_t port,
			struct rte_eth_rss_reta *reta_conf);

 /**
 * Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reta_conf
 *   RETA to query.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_rss_reta_query(uint8_t port,
			struct rte_eth_rss_reta *reta_conf);

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
			struct rte_eth_vmdq_mirror_conf *mirror_conf,
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
 *   The tx rate allocated from the total link speed for this queue.
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
 * @param state
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
 * @param state
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
 * @param port
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
 * @param port
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
 * add syn filter
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param rx_queue
 *   The index of RX queue where to store RX packets matching the syn filter.
 * @param filter
 *   The pointer to the structure describing the syn filter rule.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_add_syn_filter(uint8_t port_id,
			struct rte_syn_filter *filter, uint16_t rx_queue);

/**
 * remove syn filter
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_remove_syn_filter(uint8_t port_id);

/**
 * get syn filter
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param filter
 *   The pointer to the structure describing the syn filter.
 * @param rx_queue
 *   A pointer to get the queue index of syn filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-EINVAL) if bad parameter.
 */
int rte_eth_dev_get_syn_filter(uint8_t port_id,
			struct rte_syn_filter *filter, uint16_t *rx_queue);

/**
 * Add a new ethertype filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of ethertype filter.
 * @param filter
 *   The pointer to the structure describing the ethertype filter rule.
 *   The *rte_ethertype_filter* structure includes the values of the different
 *   fields to match: ethertype and priority in vlan tag.
 *   priority in vlan tag is not supported for E1000 dev.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   ethertype filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support ethertype filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_add_ethertype_filter(uint8_t port_id, uint16_t index,
			struct rte_ethertype_filter *filter, uint16_t rx_queue);

/**
 * remove an ethertype filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of ethertype filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support ethertype filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_remove_ethertype_filter(uint8_t port_id,
			uint16_t index);

/**
 * Get an ethertype filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of ethertype filter.
 * @param filter
 *   A pointer to a structure of type *rte_ethertype_filter* to be filled with
 *   the information of the Ethertype filter.
 * @param rx_queue
 *   A pointer to get the queue index.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support ethertype filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 *   - (-ENOENT) if no enabled filter in this index.
 */
int rte_eth_dev_get_ethertype_filter(uint8_t port_id, uint16_t index,
			struct rte_ethertype_filter *filter, uint16_t *rx_queue);

/**
 * Add a new 2tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 2tuple filter.
 * @param filter
 *   The pointer to the structure describing the 2tuple filter rule.
 *   The *rte_2tuple_filter* structure includes the values of the different
 *   fields to match: protocol, dst_port and
 *   tcp_flags if the protocol is tcp type.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   2tuple filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 2tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_add_2tuple_filter(uint8_t port_id, uint16_t index,
			struct rte_2tuple_filter *filter, uint16_t rx_queue);

/**
 * remove a 2tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 2tuple filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 2tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_remove_2tuple_filter(uint8_t port_id, uint16_t index);

/**
 * Get an 2tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 2tuple filter.
 * @param filter
 *   A pointer to a structure of type *rte_2tuple_filter* to be filled with
 *   the information of the 2tuple filter.
 * @param rx_queue
 *   A pointer to get the queue index.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 2tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 *   - (-ENOENT) if no enabled filter in this index.
 */
int rte_eth_dev_get_2tuple_filter(uint8_t port_id, uint16_t index,
			struct rte_2tuple_filter *filter, uint16_t *rx_queue);

/**
 * Add a new 5tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 5tuple filter.
 * @param filter
 *   The pointer to the structure describing the 5tuple filter rule.
 *   The *rte_5tuple_filter* structure includes the values of the different
 *   fields to match: dst src IP, dst src port, protocol and relative masks
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   5tuple filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 5tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_add_5tuple_filter(uint8_t port_id, uint16_t index,
			struct rte_5tuple_filter *filter, uint16_t rx_queue);

/**
 * remove a 5tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 5tuple filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 5tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_remove_5tuple_filter(uint8_t port_id, uint16_t index);

/**
 * Get an 5tuple filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of 5tuple filter.
 * @param filter
 *   A pointer to a structure of type *rte_5tuple_filter* to be filled with
 *   the information of the 5tuple filter.
 * @param rx_queue
 *   A pointer to get the queue index.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support 5tuple filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_get_5tuple_filter(uint8_t port_id, uint16_t index,
			struct rte_5tuple_filter *filter, uint16_t *rx_queue);

/**
 * Add a new flex filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of flex filter.
 * @param filter
 *   The pointer to the structure describing the flex filter rule.
 *   The *rte_flex_filter* structure includes the values of the different fields
 *   to match: the dwords (first len bytes of packet ) and relative masks.
 * @param rx_queue
 *   The index of the RX queue where to store RX packets matching the added
 *   flex filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flex filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 *   - (-ENOENT) if no enabled filter in this index.
 */
int rte_eth_dev_add_flex_filter(uint8_t port_id, uint16_t index,
			struct rte_flex_filter *filter, uint16_t rx_queue);

/**
 * remove a flex filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of flex filter.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flex filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 */
int rte_eth_dev_remove_flex_filter(uint8_t port_id, uint16_t index);

/**
 * Get an flex filter rule on an Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param index
 *   The identifier of flex filter.
 * @param filter
 *   A pointer to a structure of type *rte_flex_filter* to be filled with
 *   the information of the flex filter.
 * @param rx_queue
 *   A pointer to get the queue index.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support flex filter.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if the filter information is not correct.
 *   - (-ENOENT) if no enabled filter in this index.
 */
int rte_eth_dev_get_flex_filter(uint8_t port_id, uint16_t index,
			struct rte_flex_filter *filter, uint16_t *rx_queue);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_H_ */
