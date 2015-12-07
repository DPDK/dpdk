/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#ifndef _RTE_ETHTOOL_H_
#define _RTE_ETHTOOL_H_

/*
 * This new interface is designed to provide a user-space shim layer for
 * Ethtool and Netdevice op API.
 *
 * rte_ethtool_get_driver:          ethtool_ops::get_driverinfo
 * rte_ethtool_get_link:            ethtool_ops::get_link
 * rte_ethtool_get_regs_len:        ethtool_ops::get_regs_len
 * rte_ethtool_get_regs:            ethtool_ops::get_regs
 * rte_ethtool_get_eeprom_len:      ethtool_ops::get_eeprom_len
 * rte_ethtool_get_eeprom:          ethtool_ops::get_eeprom
 * rte_ethtool_set_eeprom:          ethtool_ops::set_eeprom
 * rte_ethtool_get_pauseparam:      ethtool_ops::get_pauseparam
 * rte_ethtool_set_pauseparam:      ethtool_ops::set_pauseparam
 *
 * rte_ethtool_net_open:            net_device_ops::ndo_open
 * rte_ethtool_net_stop:            net_device_ops::ndo_stop
 * rte_ethtool_net_set_mac_addr:    net_device_ops::ndo_set_mac_address
 * rte_ethtool_net_validate_addr:   net_device_ops::ndo_validate_addr
 * rte_ethtool_net_change_mtu:      net_device_ops::rte_net_change_mtu
 * rte_ethtool_net_get_stats64:     net_device_ops::ndo_get_stats64
 * rte_ethtool_net_vlan_rx_add_vid  net_device_ops::ndo_vlan_rx_add_vid
 * rte_ethtool_net_vlan_rx_kill_vid net_device_ops::ndo_vlan_rx_kill_vid
 * rte_ethtool_net_set_rx_mode      net_device_ops::ndo_set_rx_mode
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_ethdev.h>
#include <linux/ethtool.h>

/**
 * Retrieve the Ethernet device driver information according to
 * attributes described by ethtool data structure, ethtool_drvinfo.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param drvinfo
 *   A pointer to get driver information
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_ethtool_get_drvinfo(uint8_t port_id, struct ethtool_drvinfo *drvinfo);

/**
 * Retrieve the Ethernet device register length in bytes.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (> 0) # of device registers (in bytes) available for dump
 *   - (0) no registers available for dump.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_regs_len(uint8_t port_id);

/**
 * Retrieve the Ethernet device register information according to
 * attributes described by ethtool data structure, ethtool_regs
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param reg
 *   A pointer to ethtool_regs that has register information
 * @param data
 *   A pointer to a buffer that is used to retrieve device register content
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_regs(uint8_t port_id, struct ethtool_regs *regs,
			    void *data);

/**
 * Retrieve the Ethernet device link status
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (1) if link up.
 *   - (0) if link down.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_link(uint8_t port_id);

/**
 * Retrieve the Ethernet device EEPROM size
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *	 - (> 0) device EEPROM size in bytes
 *   - (0) device has NO EEPROM
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_eeprom_len(uint8_t port_id);

/**
 * Retrieve EEPROM content based upon eeprom range described in ethtool
 * data structure, ethtool_eeprom
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param eeprom
 *	 The pointer of ethtool_eeprom that provides eeprom range
 * @param words
 *	 A buffer that holds data read from eeprom
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_eeprom(uint8_t port_id, struct ethtool_eeprom *eeprom,
			      void *words);

/**
 * Setting EEPROM content based upon eeprom range described in ethtool
 * data structure, ethtool_eeprom
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param eeprom
 *	 The pointer of ethtool_eeprom that provides eeprom range
 * @param words
 *	 A buffer that holds data to be written into eeprom
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_set_eeprom(uint8_t port_id, struct ethtool_eeprom *eeprom,
			      void *words);

/**
 * Retrieve the Ethernet device pause frame configuration according to
 * parameter attributes desribed by ethtool data structure,
 * ethtool_pauseparam.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param pause_param
 *	 The pointer of ethtool_coalesce that gets pause frame
 *	 configuration parameters
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_get_pauseparam(uint8_t port_id,
				   struct ethtool_pauseparam *pause_param);

/**
 * Setting the Ethernet device pause frame configuration according to
 * parameter attributes desribed by ethtool data structure, ethtool_pauseparam.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param pause_param
 *	 The pointer of ethtool_coalesce that gets ring configuration parameters
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_set_pauseparam(uint8_t port_id,
				   struct ethtool_pauseparam *param);

/**
 * Start the Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_open(uint8_t port_id);

/**
 * Stop the Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_ethtool_net_stop(uint8_t port_id);

/**
 * Get the Ethernet device MAC address.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param addr
 *	 MAC address of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 */
int rte_ethtool_net_get_mac_addr(uint8_t port_id, struct ether_addr *addr);

/**
 * Setting the Ethernet device MAC address.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param addr
 *	 The new MAC addr.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_set_mac_addr(uint8_t port_id, struct ether_addr *addr);

/**
 * Validate if the provided MAC address is valid unicast address
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param addr
 *	 A pointer to a buffer (6-byte, 48bit) for the target MAC address
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_validate_addr(uint8_t port_id, struct ether_addr *addr);

/**
 * Setting the Ethernet device maximum Tx unit.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param mtu
 *	 New MTU
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_change_mtu(uint8_t port_id, int mtu);

/**
 * Retrieve the Ethernet device traffic statistics
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param stats
 *	 A pointer to struct rte_eth_stats for statistics parameters
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if parameters invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_get_stats64(uint8_t port_id, struct rte_eth_stats *stats);

/**
 * Update the Ethernet device VLAN filter with new vid
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vid
 *	 A new VLAN id
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_vlan_rx_add_vid(uint8_t port_id, uint16_t vid);

/**
 * Remove VLAN id from Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param vid
 *	 A new VLAN id
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_vlan_rx_kill_vid(uint8_t port_id, uint16_t vid);

/**
 * Setting the Ethernet device rx mode.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 */
int rte_ethtool_net_set_rx_mode(uint8_t port_id);

/**
 * Getting ring paramaters for Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ring_param
 *   Pointer to struct ethrool_ringparam to receive parameters.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 * @note
 *   Only the tx_pending and rx_pending fields of struct ethtool_ringparam
 *   are used, and the function only gets parameters for queue 0.
 */
int rte_ethtool_get_ringparam(uint8_t port_id,
	struct ethtool_ringparam *ring_param);

/**
 * Setting ring paramaters for Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param ring_param
 *   Pointer to struct ethrool_ringparam with parameters to set.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port_id* invalid.
 *   - others depends on the specific operations implementation.
 * @note
 *   Only the tx_pending and rx_pending fields of struct ethtool_ringparam
 *   are used, and the function only sets parameters for queue 0.
 */
int rte_ethtool_set_ringparam(uint8_t port_id,
	struct ethtool_ringparam *ring_param);


#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHTOOL_H_ */
