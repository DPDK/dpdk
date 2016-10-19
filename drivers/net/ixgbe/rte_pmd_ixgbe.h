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
