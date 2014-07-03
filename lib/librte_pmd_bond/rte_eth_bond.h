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

#ifndef _RTE_ETH_BOND_H_
#define _RTE_ETH_BOND_H_

/**
 * @file rte_eth_bond.h
 *
 * RTE Link Bonding Ethernet Device
 * Link Bonding for 1GbE and 10GbE ports to allow the aggregation of multiple
 * (slave) NICs into a single logical interface. The bonded device processes
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
 * active slaves of the bonded in a round robin fashion. */
#define BONDING_MODE_ACTIVE_BACKUP		(1)
/**< Active Backup (Mode 1).
 * In this mode all packets transmitted will be transmitted on the primary
 * slave until such point as the primary slave is no longer available and then
 * transmitted packets will be sent on the next available slaves. The primary
 * slave can be defined by the user but defaults to the first active slave
 * available if not specified. */
#define BONDING_MODE_BALANCE			(2)
/**< Balance (Mode 2).
 * In this mode all packets transmitted will be balanced across the available
 * slaves using one of three available transmit policies - l2, l2+3 or l3+4.
 * See BALANCE_XMIT_POLICY macros definitions for further details on transmit
 * policies. */
#define BONDING_MODE_BROADCAST			(3)
/**< Broadcast (Mode 3).
 * In this mode all transmitted packets will be transmitted on all available
 * active slaves of the bonded. */

/* Balance Mode Transmit Policies */
#define BALANCE_XMIT_POLICY_LAYER2		(0)
/**< Layer 2 (Ethernet MAC) */
#define BALANCE_XMIT_POLICY_LAYER23		(1)
/**< Layer 2+3 (Ethernet MAC + IP Addresses) transmit load balancing */
#define BALANCE_XMIT_POLICY_LAYER34		(2)
/**< Layer 3+4 (IP Addresses + UDP Ports) transmit load balancing */

/**
 * Create a bonded rte_eth_dev device
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
 * Add a rte_eth_dev device as a slave to the bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param slave_port_id		Port ID of slave device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_slave_add(uint8_t bonded_port_id, uint8_t slave_port_id);

/**
 * Remove a slave rte_eth_dev device from the bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param slave_port_id		Port ID of slave device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_slave_remove(uint8_t bonded_port_id, uint8_t slave_port_id);

/**
 * Set link bonding mode of bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param mode				Bonding mode to set
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mode_set(uint8_t bonded_port_id, uint8_t mode);

/**
 * Get link bonding mode of bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 *
 * @return
 *	link bonding mode on success, negative value otherwise
 */
int
rte_eth_bond_mode_get(uint8_t bonded_port_id);

/**
 * Set slave rte_eth_dev as primary slave of bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param slave_port_id		Port ID of slave device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_primary_set(uint8_t bonded_port_id, uint8_t slave_port_id);

/**
 * Get primary slave of bonded device
 *
 * @param bonded_port_id	Port ID of bonded device.
 *
 * @return
 *	Port Id of primary slave on success, -1 on failure
 */
int
rte_eth_bond_primary_get(uint8_t bonded_port_id);

/**
 * Populate an array with list of the slaves port id's of the bonded device
 *
 * @param bonded_port_id	Port ID of bonded eth_dev to interrogate
 * @param slaves			Array to be populated with the current active slaves
 * @param len				Length of slaves array
 *
 * @return
 *	Number of slaves associated with bonded device on success,
 *	negative value otherwise
 */
int
rte_eth_bond_slaves_get(uint8_t bonded_port_id, uint8_t slaves[], uint8_t len);

/**
 * Populate an array with list of the active slaves port id's of the bonded
 * device.
 *
 * @param bonded_port_id	Port ID of bonded eth_dev to interrogate
 * @param slaves			Array to be populated with the current active slaves
 * @param len				Length of slaves array
 *
 * @return
 *	Number of active slaves associated with bonded device on success,
 *	negative value otherwise
 */
int
rte_eth_bond_active_slaves_get(uint8_t bonded_port_id, uint8_t slaves[],
		uint8_t len);

/**
 * Set explicit MAC address to use on bonded device and it's slaves.
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param mac_addr			MAC Address to use on bonded device overriding
 *							slaves MAC addresses
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mac_address_set(uint8_t bonded_port_id,
		struct ether_addr *mac_addr);

/**
 * Reset bonded device to use MAC from primary slave on bonded device and it's
 * slaves.
 *
 * @param bonded_port_id	Port ID of bonded device.
 *
 * @return
 *	0 on success, negative value otherwise
 */
int
rte_eth_bond_mac_address_reset(uint8_t bonded_port_id);

/**
 * Set the transmit policy for bonded device to use when it is operating in
 * balance mode, this parameter is otherwise ignored in other modes of
 * operation.
 *
 * @param bonded_port_id	Port ID of bonded device.
 * @param policy			Balance mode transmission policy.
 *
 * @return
 *	0 on success, negative value otherwise.
 */
int
rte_eth_bond_xmit_policy_set(uint8_t bonded_port_id, uint8_t policy);

/**
 * Get the transmit policy set on bonded device for balance mode operation
 *
 * @param bonded_port_id	Port ID of bonded device.
 *
 * @return
 *	Balance transmit policy on success, negative value otherwise.
 */
int
rte_eth_bond_xmit_policy_get(uint8_t bonded_port_id);

#ifdef __cplusplus
}
#endif

#endif
