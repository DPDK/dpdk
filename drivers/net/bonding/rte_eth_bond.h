/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_ETH_BOND_H_
#define _RTE_ETH_BOND_H_

/**
 * @file rte_eth_bond.h
 *
 * RTE Link Bonding Ethernet Device
 * Link Bonding for 1GbE and 10GbE ports to allow the aggregation of multiple
 * (member) NICs into a single logical interface. The bonding device processes
 * these interfaces based on the mode of operation specified and supported.
 * This implementation supports 4 modes of operation round robin, active backup
 * balance and broadcast. Providing redundant links, fault tolerance and/or
 * load balancing of network ports
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ether.h>

/* Supported modes of operation of link bonding library  */

#define BONDING_MODE_ROUND_ROBIN		(0)
/**< Round Robin (Mode 0).
 * In this mode all transmitted packets will be balanced equally across all
 * active members of the bonding in a round robin fashion.
 */
#define BONDING_MODE_ACTIVE_BACKUP		(1)
/**< Active Backup (Mode 1).
 * In this mode all packets transmitted will be transmitted on the primary
 * member until such point as the primary member is no longer available and then
 * transmitted packets will be sent on the next available members. The primary
 * member can be defined by the user but defaults to the first active member
 * available if not specified.
 */
#define BONDING_MODE_BALANCE			(2)
/**< Balance (Mode 2).
 * In this mode all packets transmitted will be balanced across the available
 * members using one of three available transmit policies - l2, l2+3 or l3+4.
 * See BALANCE_XMIT_POLICY macros definitions for further details on transmit
 * policies.
 */
#define BONDING_MODE_BROADCAST			(3)
/**< Broadcast (Mode 3).
 * In this mode all transmitted packets will be transmitted on all available
 * active members of the bonding.
 */
#define BONDING_MODE_8023AD				(4)
/**< 802.3AD (Mode 4).
 *
 * This mode provides auto negotiation/configuration
 * of peers and well as link status changes monitoring using out of band
 * LACP (link aggregation control protocol) messages. For further details of
 * LACP specification see the IEEE 802.3ad/802.1AX standards. It is also
 * described here
 * https://www.kernel.org/doc/Documentation/networking/bonding.txt.
 *
 * Important Usage Notes:
 * - for LACP mode to work the rx/tx burst functions must be invoked
 * at least once every 100ms, otherwise the out-of-band LACP messages will not
 * be handled with the expected latency and this may cause the link status to be
 * incorrectly marked as down or failure to correctly negotiate with peers.
 * - For optimal performance during initial handshaking the array of mbufs provided
 * to rx_burst should be at least 2 times the member count size.
 */
#define BONDING_MODE_TLB	(5)
/**< Adaptive TLB (Mode 5)
 * This mode provides an adaptive transmit load balancing. It dynamically
 * changes the transmitting member, according to the computed load. Statistics
 * are collected in 100ms intervals and scheduled every 10ms.
 */
#define BONDING_MODE_ALB	(6)
/**< Adaptive Load Balancing (Mode 6)
 * This mode includes adaptive TLB and receive load balancing (RLB). In RLB the
 * bonding driver intercepts ARP replies send by local system and overwrites its
 * source MAC address, so that different peers send data to the server on
 * different member interfaces. When local system sends ARP request, it saves IP
 * information from it. When ARP reply from that peer is received, its MAC is
 * stored, one of member MACs assigned and ARP reply send to that peer.
 */

/* Balance Mode Transmit Policies */
#define BALANCE_XMIT_POLICY_LAYER2		(0)
/**< Layer 2 (Ethernet MAC) */
#define BALANCE_XMIT_POLICY_LAYER23		(1)
/**< Layer 2+3 (Ethernet MAC + IP Addresses) transmit load balancing */
#define BALANCE_XMIT_POLICY_LAYER34		(2)
/**< Layer 3+4 (IP Addresses + UDP Ports) transmit load balancing */

/**
 * Create a bonding rte_eth_dev device
 *
 * @param name			Name of new link bonding device.
 * @param mode			Mode to initialize bonding device in.
 * @param socket_id		Socket Id on which to allocate eth_dev resources.
 *
 * @return
 *	Port Id of created rte_eth_dev on success, negative value otherwise
 */
int
rte_eth_bond_create(const char *name, uint8_t mode, uint8_t socket_id);

/**
 * Free a bonding rte_eth_dev device
 *
 * @param name			Name of the link bonding device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_free(const char *name);

/**
 * Add a rte_eth_dev device as a member to the bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param member_port_id		Port ID of member device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
__rte_experimental
int
rte_eth_bond_member_add(uint16_t bonding_port_id, uint16_t member_port_id);

/**
 * Remove a member rte_eth_dev device from the bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param member_port_id		Port ID of member device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
__rte_experimental
int
rte_eth_bond_member_remove(uint16_t bonding_port_id, uint16_t member_port_id);

/**
 * Set link bonding mode of bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param mode				Bonding mode to set
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mode_set(uint16_t bonding_port_id, uint8_t mode);

/**
 * Get link bonding mode of bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *	link bonding mode on success, negative value otherwise
 */
int
rte_eth_bond_mode_get(uint16_t bonding_port_id);

/**
 * Set member rte_eth_dev as primary member of bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param member_port_id		Port ID of member device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_primary_set(uint16_t bonding_port_id, uint16_t member_port_id);

/**
 * Get primary member of bonding device
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *	Port Id of primary member on success, -1 on failure
 */
int
rte_eth_bond_primary_get(uint16_t bonding_port_id);

/**
 * Populate an array with list of the members port id's of the bonding device
 *
 * @param bonding_port_id	Port ID of bonding eth_dev to interrogate
 * @param members			Array to be populated with the current active members
 * @param len				Length of members array
 *
 * @return
 *	Number of members associated with bonding device on success,
 *	negative value otherwise
 */
__rte_experimental
int
rte_eth_bond_members_get(uint16_t bonding_port_id, uint16_t members[],
		uint16_t len);

/**
 * Populate an array with list of the active members port id's of the bonding
 * device.
 *
 * @param bonding_port_id	Port ID of bonding eth_dev to interrogate
 * @param members			Array to be populated with the current active members
 * @param len				Length of members array
 *
 * @return
 *	Number of active members associated with bonding device on success,
 *	negative value otherwise
 */
__rte_experimental
int
rte_eth_bond_active_members_get(uint16_t bonding_port_id, uint16_t members[],
		uint16_t len);

/**
 * Set explicit MAC address to use on bonding device and it's members.
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param mac_addr			MAC Address to use on bonding device overriding
 *							members MAC addresses
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mac_address_set(uint16_t bonding_port_id,
		struct rte_ether_addr *mac_addr);

/**
 * Reset bonding device to use MAC from primary member on bonding device and it's
 * members.
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mac_address_reset(uint16_t bonding_port_id);

/**
 * Set the transmit policy for bonding device to use when it is operating in
 * balance mode, this parameter is otherwise ignored in other modes of
 * operation.
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param policy			Balance mode transmission policy.
 *
 * @return
 *	0 on success, negative value otherwise.
 */
int
rte_eth_bond_xmit_policy_set(uint16_t bonding_port_id, uint8_t policy);

/**
 * Get the transmit policy set on bonding device for balance mode operation
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *	Balance transmit policy on success, negative value otherwise.
 */
int
rte_eth_bond_xmit_policy_get(uint16_t bonding_port_id);

/**
 * Set the link monitoring frequency (in ms) for monitoring the link status of
 * member devices
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param internal_ms		Monitoring interval in milliseconds
 *
 * @return
 *	0 on success, negative value otherwise.
 */

int
rte_eth_bond_link_monitoring_set(uint16_t bonding_port_id, uint32_t internal_ms);

/**
 * Get the current link monitoring frequency (in ms) for monitoring of the link
 * status of member devices
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *	Monitoring interval on success, negative value otherwise.
 */
int
rte_eth_bond_link_monitoring_get(uint16_t bonding_port_id);


/**
 * Set the period in milliseconds for delaying the disabling of a bonding link
 * when the link down status has been detected
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param delay_ms			Delay period in milliseconds.
 *
 * @return
 *  0 on success, negative value otherwise.
 */
int
rte_eth_bond_link_down_prop_delay_set(uint16_t bonding_port_id,
				       uint32_t delay_ms);

/**
 * Get the period in milliseconds set for delaying the disabling of a bonding
 * link when the link down status has been detected
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *  Delay period on success, negative value otherwise.
 */
int
rte_eth_bond_link_down_prop_delay_get(uint16_t bonding_port_id);

/**
 * Set the period in milliseconds for delaying the enabling of a bonding link
 * when the link up status has been detected
 *
 * @param bonding_port_id	Port ID of bonding device.
 * @param delay_ms			Delay period in milliseconds.
 *
 * @return
 *  0 on success, negative value otherwise.
 */
int
rte_eth_bond_link_up_prop_delay_set(uint16_t bonding_port_id,
				    uint32_t delay_ms);

/**
 * Get the period in milliseconds set for delaying the enabling of a bonding
 * link when the link up status has been detected
 *
 * @param bonding_port_id	Port ID of bonding device.
 *
 * @return
 *  Delay period on success, negative value otherwise.
 */
int
rte_eth_bond_link_up_prop_delay_get(uint16_t bonding_port_id);


#ifdef __cplusplus
}
#endif

#endif
