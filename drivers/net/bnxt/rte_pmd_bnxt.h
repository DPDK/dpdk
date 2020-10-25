/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Broadcom
 * All rights reserved.
 */

#ifndef _PMD_BNXT_H_
#define _PMD_BNXT_H_

#include <rte_ethdev.h>
#include <rte_ether.h>

/** mbuf dynamic field where CFA code is stored */
#define RTE_PMD_BNXT_CFA_CODE_DYNFIELD_NAME "rte_net_bnxt_dynfield_cfa_code"

/*
 * Response sent back to the caller after callback
 */
enum rte_pmd_bnxt_mb_event_rsp {
	RTE_PMD_BNXT_MB_EVENT_NOOP_ACK,  /**< skip mbox request and ACK */
	RTE_PMD_BNXT_MB_EVENT_NOOP_NACK, /**< skip mbox request and NACK */
	RTE_PMD_BNXT_MB_EVENT_PROCEED,  /**< proceed with mbox request  */
	RTE_PMD_BNXT_MB_EVENT_MAX       /**< max value of this enum */
};

/* mailbox message types */
#define BNXT_VF_RESET			0x01 /* VF requests reset */
#define BNXT_VF_SET_MAC_ADDR	0x02 /* VF requests PF to set MAC addr */
#define BNXT_VF_SET_VLAN		0x03 /* VF requests PF to set VLAN */
#define BNXT_VF_SET_MTU			0x04 /* VF requests PF to set MTU */
#define BNXT_VF_SET_MRU			0x05 /* VF requests PF to set MRU */

/*
 * Data sent to the caller when the callback is executed.
 */
struct rte_pmd_bnxt_mb_event_param {
	uint16_t vf_id;	/* Virtual Function number */
	int	retval;	/* return value */
	void	*msg;	/* pointer to message */
};

/**
 * Enable/Disable VF MAC anti spoof
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param on
 *    1 - Enable VF MAC anti spoof.
 *    0 - Disable VF MAC anti spoof.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_bnxt_set_vf_mac_anti_spoof(uint16_t port, uint16_t vf, uint8_t on);

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
int rte_pmd_bnxt_set_vf_mac_addr(uint16_t port, uint16_t vf,
		struct rte_ether_addr *mac_addr);

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
rte_pmd_bnxt_set_vf_vlan_stripq(uint16_t port, uint16_t vf, uint8_t on);

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
int
rte_pmd_bnxt_set_vf_vlan_insert(uint16_t port, uint16_t vf,
		uint16_t vlan_id);

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
int rte_pmd_bnxt_set_vf_vlan_filter(uint16_t port, uint16_t vlan,
				    uint64_t vf_mask, uint8_t vlan_on);

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
int rte_pmd_bnxt_set_tx_loopback(uint16_t port, uint8_t on);

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
int rte_pmd_bnxt_set_all_queues_drop_en(uint16_t port, uint8_t on);

/**
 * Set the VF rate limit.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param tx_rate
 *   Tx rate for the VF
 * @param q_msk
 *   Mask of the Tx queue
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int rte_pmd_bnxt_set_vf_rate_limit(uint16_t port, uint16_t vf,
				uint16_t tx_rate, uint64_t q_msk);

/**
 * Get VF's statistics
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @param stats
 *    A pointer to a structure of type *rte_eth_stats* to be filled with
 *    the values of device counters supported statistics:
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */

int rte_pmd_bnxt_get_vf_stats(uint16_t port,
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
int rte_pmd_bnxt_reset_vf_stats(uint16_t port,
				uint16_t vf_id);

/**
 * Enable/Disable VF VLAN anti spoof
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param on
 *    1 - Enable VF VLAN anti spoof.
 *    0 - Disable VF VLAN anti spoof.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_bnxt_set_vf_vlan_anti_spoof(uint16_t port, uint16_t vf, uint8_t on);

/**
 * Set RX L2 Filtering mode of a VF of an Ethernet device.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param rx_mask
 *    The RX mode mask
 * @param on
 *    1 - Enable a VF RX mode.
 *    0 - Disable a VF RX mode.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_bnxt_set_vf_rxmode(uint16_t port, uint16_t vf,
				uint16_t rx_mask, uint8_t on);

/**
 * Returns the number of default RX queues on a VF
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @return
 *   - Non-negative value - Number of default RX queues
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) if on a function without VFs
 *   - (-ENOMEM) on an allocation failure
 *   - (-1) firmware interface error
 */
int rte_pmd_bnxt_get_vf_rx_status(uint16_t port, uint16_t vf_id);

/**
 * Queries the TX drop counter for the function
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @param count
 *    Pointer to a uint64_t that will be populated with the counter value.
 * @return
 *   - Positive Non-zero value - Error code from HWRM
 *   - (-EINVAL) invalid vf_id specified.
 *   - (-ENOTSUP) Ethernet device is not a PF
 */
int rte_pmd_bnxt_get_vf_tx_drop_count(uint16_t port, uint16_t vf_id,
				      uint64_t *count);

/**
 * Programs the MAC address for the function specified
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param mac_addr
 *    The MAC address to be programmed in the filter.
 * @param vf_id
 *    VF on which to get.
 * @return
 *   - Positive Non-zero value - Error code from HWRM
 *   - (-EINVAL) invalid vf_id specified.
 *   - (-ENOTSUP) Ethernet device is not a PF
 *   - (-ENOMEM) on an allocation failure
 */
int rte_pmd_bnxt_mac_addr_add(uint16_t port, struct rte_ether_addr *mac_addr,
				uint32_t vf_id);

/**
 * Enable/Disable VF statistics retention
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @param on
 *    1 - Prevent VF statistics from automatically resetting
 *    0 - Allow VF statistics to automatically reset
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_bnxt_set_vf_persist_stats(uint16_t port, uint16_t vf, uint8_t on);
#endif /* _PMD_BNXT_H_ */
