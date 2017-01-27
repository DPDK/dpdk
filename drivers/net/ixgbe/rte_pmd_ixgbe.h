/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Intel Corporation. All rights reserved.
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

/**
 * @file rte_pmd_ixgbe.h
 * ixgbe PMD specific functions.
 *
 **/

#ifndef _PMD_IXGBE_H_
#define _PMD_IXGBE_H_

#include <rte_ethdev.h>

/**
 * Set the VF MAC address.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param mac_addr
 *   VF MAC address.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int rte_pmd_ixgbe_set_vf_mac_addr(uint8_t port, uint16_t vf,
		struct ether_addr *mac_addr);

/**
 * Enable/Disable VF VLAN anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    VF on which to set VLAN anti spoofing.
 * @param on
 *    1 - Enable VFs VLAN anti spoofing.
 *    0 - Disable VFs VLAN anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_set_vf_vlan_anti_spoof(uint8_t port, uint16_t vf, uint8_t on);

/**
 * Enable/Disable VF MAC anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    VF on which to set MAC anti spoofing.
 * @param on
 *    1 - Enable VFs MAC anti spoofing.
 *    0 - Disable VFs MAC anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_set_vf_mac_anti_spoof(uint8_t port, uint16_t vf, uint8_t on);

/**
 * Enable/Disable vf vlan insert
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    ID specifying VF.
 * @param vlan_id
 *    0 - Disable VF's vlan insert.
 *    n - Enable; n is inserted as the vlan id.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_set_vf_vlan_insert(uint8_t port, uint16_t vf,
		uint16_t vlan_id);

/**
 * Enable/Disable tx loopback
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param on
 *    1 - Enable tx loopback.
 *    0 - Disable tx loopback.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_set_tx_loopback(uint8_t port, uint8_t on);

/**
 * set all queues drop enable bit
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param on
 *    1 - set the queue drop enable bit for all pools.
 *    0 - reset the queue drop enable bit for all pools.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_set_all_queues_drop_en(uint8_t port, uint8_t on);

/**
 * set drop enable bit in the VF split rx control register
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    ID specifying VF.
 * @param on
 *    1 - set the drop enable bit in the split rx control register.
 *    0 - reset the drop enable bit in the split rx control register.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */

int rte_pmd_ixgbe_set_vf_split_drop_en(uint8_t port, uint16_t vf, uint8_t on);

/**
 * Enable/Disable vf vlan strip for all queues in a pool
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    ID specifying VF.
 * @param on
 *    1 - Enable VF's vlan strip on RX queues.
 *    0 - Disable VF's vlan strip on RX queues.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int
rte_pmd_ixgbe_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on);

/**
 * Enable MACsec offload.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param en
 *    1 - Enable encryption (encrypt and add integrity signature).
 *    0 - Disable encryption (only add integrity signature).
 * @param rp
 *    1 - Enable replay protection.
 *    0 - Disable replay protection.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 */
int rte_pmd_ixgbe_macsec_enable(uint8_t port, uint8_t en, uint8_t rp);

/**
 * Disable MACsec offload.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 */
int rte_pmd_ixgbe_macsec_disable(uint8_t port);

/**
 * Configure Tx SC (Secure Connection).
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param mac
 *   The MAC address on the local side.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 */
int rte_pmd_ixgbe_macsec_config_txsc(uint8_t port, uint8_t *mac);

/**
 * Configure Rx SC (Secure Connection).
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param mac
 *   The MAC address on the remote side.
 * @param pi
 *   The PI (port identifier) on the remote side.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 */
int rte_pmd_ixgbe_macsec_config_rxsc(uint8_t port, uint8_t *mac, uint16_t pi);

/**
 * Enable Tx SA (Secure Association).
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param idx
 *   The SA to be enabled (0 or 1).
 * @param an
 *   The association number on the local side.
 * @param pn
 *   The packet number on the local side.
 * @param key
 *   The key on the local side.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_macsec_select_txsa(uint8_t port, uint8_t idx, uint8_t an,
		uint32_t pn, uint8_t *key);

/**
 * Enable Rx SA (Secure Association).
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param idx
 *   The SA to be enabled (0 or 1)
 * @param an
 *   The association number on the remote side.
 * @param pn
 *   The packet number on the remote side.
 * @param key
 *   The key on the remote side.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_ixgbe_macsec_select_rxsa(uint8_t port, uint8_t idx, uint8_t an,
		uint32_t pn, uint8_t *key);

/**
* Set RX L2 Filtering mode of a VF of an Ethernet device.
*
* @param port
*   The port identifier of the Ethernet device.
* @param vf
*   VF id.
* @param rx_mask
*    The RX mode mask, which is one or more of accepting Untagged Packets,
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
*   - (-ENODEV) if *port_id* invalid.
*   - (-EINVAL) if bad parameter.
*/
int
rte_pmd_ixgbe_set_vf_rxmode(uint8_t port, uint16_t vf, uint16_t rx_mask, uint8_t on);

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
rte_pmd_ixgbe_set_vf_rx(uint8_t port, uint16_t vf, uint8_t on);

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
rte_pmd_ixgbe_set_vf_tx(uint8_t port, uint16_t vf, uint8_t on);

/**
* Enable/Disable hardware VF VLAN filtering by an Ethernet device of
* received VLAN packets tagged with a given VLAN Tag Identifier.
*
* @param port
*   The port identifier of the Ethernet device.
* @param vlan
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
rte_pmd_ixgbe_set_vf_vlan_filter(uint8_t port, uint16_t vlan, uint64_t vf_mask, uint8_t vlan_on);

/**
 * Set the rate limitation for a vf on an Ethernet device.
 *
 * @param port
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
int rte_pmd_ixgbe_set_vf_rate_limit(uint8_t port, uint16_t vf, uint16_t tx_rate, uint64_t q_msk);

/**
 * Response sent back to ixgbe driver from user app after callback
 */
enum rte_pmd_ixgbe_mb_event_rsp {
	RTE_PMD_IXGBE_MB_EVENT_NOOP_ACK,  /**< skip mbox request and ACK */
	RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK, /**< skip mbox request and NACK */
	RTE_PMD_IXGBE_MB_EVENT_PROCEED,  /**< proceed with mbox request  */
	RTE_PMD_IXGBE_MB_EVENT_MAX       /**< max value of this enum */
};

/**
 * Data sent to the user application when the callback is executed.
 */
struct rte_pmd_ixgbe_mb_event_param {
	uint16_t vfid;     /**< Virtual Function number */
	uint16_t msg_type; /**< VF to PF message type, defined in ixgbe_mbx.h */
	uint16_t retval;   /**< return value */
	void *msg;         /**< pointer to message */
};
#endif /* _PMD_IXGBE_H_ */
