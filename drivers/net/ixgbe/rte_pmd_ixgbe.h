/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

/**
 * @file rte_pmd_ixgbe.h
 * ixgbe PMD specific functions.
 *
 **/

#ifndef _PMD_IXGBE_H_
#define _PMD_IXGBE_H_

#include <rte_compat.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

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
int rte_pmd_ixgbe_ping_vf(uint16_t port, uint16_t vf);

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
int rte_pmd_ixgbe_set_vf_mac_addr(uint16_t port, uint16_t vf,
		struct rte_ether_addr *mac_addr);

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
int rte_pmd_ixgbe_set_vf_vlan_anti_spoof(uint16_t port, uint16_t vf,
					 uint8_t on);

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
int rte_pmd_ixgbe_set_vf_mac_anti_spoof(uint16_t port, uint16_t vf, uint8_t on);

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
int rte_pmd_ixgbe_set_vf_vlan_insert(uint16_t port, uint16_t vf,
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
int rte_pmd_ixgbe_set_tx_loopback(uint16_t port, uint8_t on);

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
int rte_pmd_ixgbe_set_all_queues_drop_en(uint16_t port, uint8_t on);

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

int rte_pmd_ixgbe_set_vf_split_drop_en(uint16_t port, uint16_t vf, uint8_t on);

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
rte_pmd_ixgbe_set_vf_vlan_stripq(uint16_t port, uint16_t vf, uint8_t on);

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
int rte_pmd_ixgbe_macsec_enable(uint16_t port, uint8_t en, uint8_t rp);

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
int rte_pmd_ixgbe_macsec_disable(uint16_t port);

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
int rte_pmd_ixgbe_macsec_config_txsc(uint16_t port, uint8_t *mac);

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
int rte_pmd_ixgbe_macsec_config_rxsc(uint16_t port, uint8_t *mac, uint16_t pi);

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
int rte_pmd_ixgbe_macsec_select_txsa(uint16_t port, uint8_t idx, uint8_t an,
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
int rte_pmd_ixgbe_macsec_select_rxsa(uint16_t port, uint8_t idx, uint8_t an,
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
*    RTE_ETH_VMDQ_ACCEPT_UNTAG, RTE_ETH_VMDQ_ACCEPT_HASH_UC,
*    RTE_ETH_VMDQ_ACCEPT_BROADCAST and RTE_ETH_VMDQ_ACCEPT_MULTICAST will be used
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
rte_pmd_ixgbe_set_vf_rxmode(uint16_t port, uint16_t vf, uint16_t rx_mask,
			     uint8_t on);

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
rte_pmd_ixgbe_set_vf_rx(uint16_t port, uint16_t vf, uint8_t on);

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
rte_pmd_ixgbe_set_vf_tx(uint16_t port, uint16_t vf, uint8_t on);

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
rte_pmd_ixgbe_set_vf_vlan_filter(uint16_t port, uint16_t vlan,
				 uint64_t vf_mask, uint8_t vlan_on);

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
int rte_pmd_ixgbe_set_vf_rate_limit(uint16_t port, uint16_t vf,
				     uint16_t tx_rate, uint64_t q_msk);

/**
 * Set all the TCs' bandwidth weight.
 *
 * The bw_weight means the percentage occupied by the TC.
 * It can be taken as the relative min bandwidth setting.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param tc_num
 *    Number of TCs.
 * @param bw_weight
 *    An array of relative bandwidth weight for all the TCs.
 *    The summary of the bw_weight should be 100.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_ixgbe_set_tc_bw_alloc(uint16_t port,
				  uint8_t tc_num,
				  uint8_t *bw_weight);


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
int rte_pmd_ixgbe_bypass_init(uint16_t port);

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
int rte_pmd_ixgbe_bypass_state_show(uint16_t port, uint32_t *state);

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
int rte_pmd_ixgbe_bypass_state_set(uint16_t port, uint32_t *new_state);

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
int rte_pmd_ixgbe_bypass_event_show(uint16_t port,
				    uint32_t event,
				    uint32_t *state);

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
int rte_pmd_ixgbe_bypass_event_store(uint16_t port,
				     uint32_t event,
				     uint32_t state);

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
int rte_pmd_ixgbe_bypass_wd_timeout_store(uint16_t port, uint32_t timeout);

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
int rte_pmd_ixgbe_bypass_ver_show(uint16_t port, uint32_t *ver);

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
int rte_pmd_ixgbe_bypass_wd_timeout_show(uint16_t port, uint32_t *wd_timeout);

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
int rte_pmd_ixgbe_bypass_wd_reset(uint16_t port);

/**
 * Acquire swfw semaphore lock for MDIO access
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (IXGBE_ERR_SWFW_SYNC) If sw/fw semaphore acquisition failed
 */
__rte_experimental
int
rte_pmd_ixgbe_mdio_lock(uint16_t port);

/**
 * Release swfw semaphore lock used for MDIO access
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 */
__rte_experimental
int
rte_pmd_ixgbe_mdio_unlock(uint16_t port);

/**
 * Read PHY register using MDIO without MDIO lock
 * The lock must be taken separately before calling this
 * API
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reg_addr
 *   32 bit PHY Register
 * @param dev_type
 *   Used to define device base address
 * @param phy_data
 *   Pointer for reading PHY register data
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (IXGBE_ERR_PHY) If PHY read command failed
 */
__rte_experimental
int
rte_pmd_ixgbe_mdio_unlocked_read(uint16_t port, uint32_t reg_addr,
				 uint32_t dev_type, uint16_t *phy_data);

/**
 * Write data to PHY register using without MDIO lock
 * The lock must be taken separately before calling this
 * API
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param reg_addr
 *   32 bit PHY Register
 * @param dev_type
 *   Used to define device base address
 * @param phy_data
 *   Data to write to PHY register
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if hardware doesn't support.
 *   - (-ENODEV) if *port* invalid.
 *   - (IXGBE_ERR_PHY) If PHY read command failed
 */
__rte_experimental
int
rte_pmd_ixgbe_mdio_unlocked_write(uint16_t port, uint32_t reg_addr,
				  uint32_t dev_type, uint16_t phy_data);

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
enum {
	RTE_PMD_IXGBE_BYPASS_MODE_NONE,
	RTE_PMD_IXGBE_BYPASS_MODE_NORMAL,
	RTE_PMD_IXGBE_BYPASS_MODE_BYPASS,
	RTE_PMD_IXGBE_BYPASS_MODE_ISOLATE,
	RTE_PMD_IXGBE_BYPASS_MODE_NUM,
};

#define RTE_PMD_IXGBE_BYPASS_MODE_VALID(x)        \
	((x) > RTE_PMD_IXGBE_BYPASS_MODE_NONE &&  \
	(x) < RTE_PMD_IXGBE_BYPASS_MODE_NUM)

enum {
	RTE_PMD_IXGBE_BYPASS_EVENT_NONE,
	RTE_PMD_IXGBE_BYPASS_EVENT_START,
	RTE_PMD_IXGBE_BYPASS_EVENT_OS_ON = RTE_PMD_IXGBE_BYPASS_EVENT_START,
	RTE_PMD_IXGBE_BYPASS_EVENT_POWER_ON,
	RTE_PMD_IXGBE_BYPASS_EVENT_OS_OFF,
	RTE_PMD_IXGBE_BYPASS_EVENT_POWER_OFF,
	RTE_PMD_IXGBE_BYPASS_EVENT_TIMEOUT,
	RTE_PMD_IXGBE_BYPASS_EVENT_NUM
};

#define RTE_PMD_IXGBE_BYPASS_EVENT_VALID(x)       \
	((x) > RTE_PMD_IXGBE_BYPASS_EVENT_NONE && \
	(x) < RTE_PMD_IXGBE_BYPASS_MODE_NUM)

enum {
	RTE_PMD_IXGBE_BYPASS_TMT_OFF,     /* timeout disabled. */
	RTE_PMD_IXGBE_BYPASS_TMT_1_5_SEC, /* timeout for 1.5 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_2_SEC,   /* timeout for 2 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_3_SEC,   /* timeout for 3 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_4_SEC,   /* timeout for 4 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_8_SEC,   /* timeout for 8 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_16_SEC,  /* timeout for 16 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_32_SEC,  /* timeout for 32 seconds */
	RTE_PMD_IXGBE_BYPASS_TMT_NUM
};

#define RTE_PMD_IXGBE_BYPASS_TMT_VALID(x)       \
	((x) == RTE_PMD_IXGBE_BYPASS_TMT_OFF || \
	((x) > RTE_PMD_IXGBE_BYPASS_TMT_OFF &&  \
	(x) < RTE_PMD_IXGBE_BYPASS_TMT_NUM))

/**
 * @param port
 *   The port identifier of the Ethernet device.
 * @param enable
 *    0 to disable and nonzero to enable 'SBP' bit in FCTRL register
 *    to receive all packets
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if hardware doesn't support this feature.
 */
__rte_experimental
int
rte_pmd_ixgbe_upd_fctrl_sbp(uint16_t port, int enable);

/**
 * Get port fdir info
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param fdir_info
 *   The fdir info of the port
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if operation not supported.
 */
__rte_experimental
int
rte_pmd_ixgbe_get_fdir_info(uint16_t port, struct rte_eth_fdir_info *fdir_info);

/**
 * Get port fdir status
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param fdir_stats
 *   The fdir status of the port
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if operation not supported.
 */
__rte_experimental
int
rte_pmd_ixgbe_get_fdir_stats(uint16_t port,
			     struct rte_eth_fdir_stats *fdir_stats);
#endif /* _PMD_IXGBE_H_ */
