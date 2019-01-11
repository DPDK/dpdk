/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _RTE_PMD_DPAA2_H
#define _RTE_PMD_DPAA2_H

/**
 * @file rte_pmd_dpaa2.h
 *
 * NXP dpaa2 PMD specific functions.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#include <rte_flow.h>

enum pmd_dpaa2_ts {
	PMD_DPAA2_DISABLE_TS,
	PMD_DPAA2_ENABLE_TS
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Enable/Disable timestamping update in mbuf for LX2160 kind of devices.
 * For LS2088/LS1088 devices, timestamping will be updated in mbuf without
 * calling this API.
 *
 * @param pmd_dpaa2_ts
 *    Enum to enable/disable timestamp update in mbuf for LX2160 devices.
 */
__rte_experimental
void rte_pmd_dpaa2_set_timestamp(enum pmd_dpaa2_ts);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Create a flow rule to demultiplex ethernet traffic to separate network
 * interfaces.
 *
 * @param dpdmux_id
 *    ID of the DPDMUX MC object.
 * @param[in] pattern
 *    Pattern specification.
 * @param[in] actions
 *    Associated actions.
 *
 * @return
 *    A valid handle in case of success, NULL otherwise.
 */
__rte_experimental
struct rte_flow *
rte_pmd_dpaa2_mux_flow_create(uint32_t dpdmux_id,
			      struct rte_flow_item *pattern[],
			      struct rte_flow_action *actions[]);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Create a custom hash key on basis of offset of start of packet and size.
 * for e.g. if we need GRE packets (non-vlan and without any extra headers)
 * to be hashed on basis of inner IP header, we will provide offset as:
 * 14 (eth) + 20 (IP) + 4 (GRE) + 12 (Inner Src offset) = 50 and size
 * as 8 bytes.
 *
 * @param port_id
 *    The port identifier of the Ethernet device.
 * @param offset
 *    Offset from the start of packet which needs to be included to
 *    calculate hash
 * @param size
 *    Size of the hash input key
 *
 * @return
 *   - 0 if successful.
 *   - Negative in case of failure.
 */
__rte_experimental
int
rte_pmd_dpaa2_set_custom_hash(uint16_t port_id,
			      uint16_t offset,
			      uint8_t size);

#endif /* _RTE_PMD_DPAA2_H */
