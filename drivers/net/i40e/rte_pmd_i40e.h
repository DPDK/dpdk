/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2017 Intel Corporation. All rights reserved.
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

#ifndef _PMD_I40E_H_
#define _PMD_I40E_H_

/**
 * @file rte_pmd_i40e.h
 *
 * i40e PMD specific functions.
 *
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#include <rte_ethdev.h>

/**
 * Response sent back to i40e driver from user app after callback
 */
enum rte_pmd_i40e_mb_event_rsp {
	RTE_PMD_I40E_MB_EVENT_NOOP_ACK,  /**< skip mbox request and ACK */
	RTE_PMD_I40E_MB_EVENT_NOOP_NACK, /**< skip mbox request and NACK */
	RTE_PMD_I40E_MB_EVENT_PROCEED,  /**< proceed with mbox request  */
	RTE_PMD_I40E_MB_EVENT_MAX       /**< max value of this enum */
};

/**
 * Data sent to the user application when the callback is executed.
 */
struct rte_pmd_i40e_mb_event_param {
	uint16_t vfid;     /**< Virtual Function number */
	uint16_t msg_type; /**< VF to PF message type, see i40e_virtchnl_ops */
	uint16_t retval;   /**< return value */
	void *msg;         /**< pointer to message */
	uint16_t msglen;   /**< length of the message */
};

/**
 * Notify VF when PF link status changes.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* invalid.
 */
int rte_pmd_i40e_ping_vfs(uint8_t port, uint16_t vf);

/**
 * Enable/Disable VF MAC anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set MAC anti spoofing.
 * @param on
 *    1 - Enable VFs MAC anti spoofing.
 *    0 - Disable VFs MAC anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_mac_anti_spoof(uint8_t port,
				       uint16_t vf_id,
				       uint8_t on);

/**
 * Enable/Disable VF VLAN anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set VLAN anti spoofing.
 * @param on
 *    1 - Enable VFs VLAN anti spoofing.
 *    0 - Disable VFs VLAN anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_vlan_anti_spoof(uint8_t port,
					uint16_t vf_id,
					uint8_t on);

/**
 * Enable/Disable TX loopback on all the PF and VFs.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param on
 *    1 - Enable TX loopback.
 *    0 - Disable TX loopback.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_tx_loopback(uint8_t port,
				 uint8_t on);

/**
 * Enable/Disable VF unicast promiscuous mode.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set.
 * @param on
 *    1 - Enable.
 *    0 - Disable.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_unicast_promisc(uint8_t port,
					uint16_t vf_id,
					uint8_t on);

/**
 * Enable/Disable VF multicast promiscuous mode.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set.
 * @param on
 *    1 - Enable.
 *    0 - Disable.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_multicast_promisc(uint8_t port,
					  uint16_t vf_id,
					  uint8_t on);

/**
 * Set the VF MAC address.
 *
 * PF should set MAC address before VF initialized, if PF sets the MAC
 * address after VF initialized, new MAC address won't be effective until
 * VF reinitialize.
 *
 * This will remove all existing MAC filters.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf_id
 *   VF id.
 * @param mac_addr
 *   VF MAC address.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int rte_pmd_i40e_set_vf_mac_addr(uint8_t port, uint16_t vf_id,
				 struct ether_addr *mac_addr);

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
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int
rte_pmd_i40e_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on);

/**
 * Enable/Disable vf vlan insert
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
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
int rte_pmd_i40e_set_vf_vlan_insert(uint8_t port, uint16_t vf_id,
				    uint16_t vlan_id);

/**
 * Enable/Disable vf broadcast mode
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param on
 *    0 - Disable broadcast.
 *    1 - Enable broadcast.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_broadcast(uint8_t port, uint16_t vf_id,
				  uint8_t on);

/**
 * Enable/Disable vf vlan tag
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param on
 *    0 - Disable VF's vlan tag.
 *    n - Enable VF's vlan tag.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_vlan_tag(uint8_t port, uint16_t vf_id, uint8_t on);

/**
 * Enable/Disable VF VLAN filter
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vlan_id
 *    ID specifying VLAN
 * @param vf_mask
 *    Mask to filter VF's
 * @param on
 *    0 - Disable VF's VLAN filter.
 *    1 - Enable VF's VLAN filter.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id,
				    uint64_t vf_mask, uint8_t on);

/**
 * Get VF's statistics
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @param stats
 *    A pointer to a structure of type *rte_eth_stats* to be filled with
 *    the values of device counters for the following set of statistics:
 *   - *ipackets* with the total of successfully received packets.
 *   - *opackets* with the total of successfully transmitted packets.
 *   - *ibytes*   with the total of successfully received bytes.
 *   - *obytes*   with the total of successfully transmitted bytes.
 *   - *ierrors*  with the total of erroneous received packets.
 *   - *oerrors*  with the total of failed transmitted packets.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */

int rte_pmd_i40e_get_vf_stats(uint8_t port,
			      uint16_t vf_id,
			      struct rte_eth_stats *stats);

/**
 * Clear VF's statistics
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_reset_vf_stats(uint8_t port,
				uint16_t vf_id);

#endif /* _PMD_I40E_H_ */
