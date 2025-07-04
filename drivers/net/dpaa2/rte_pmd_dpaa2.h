/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2024 NXP
 */

#ifndef _RTE_PMD_DPAA2_H
#define _RTE_PMD_DPAA2_H

/**
 * @file rte_pmd_dpaa2.h
 *
 * NXP dpaa2 PMD specific functions.
 */

#include <rte_compat.h>
#include <rte_flow.h>

/**
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
 *    0 in case of success,  otherwise failure.
 */
int
rte_pmd_dpaa2_mux_flow_create(uint32_t dpdmux_id,
	struct rte_flow_item pattern[],
	struct rte_flow_action actions[]);
int
rte_pmd_dpaa2_mux_flow_destroy(uint32_t dpdmux_id,
	uint16_t entry_index);
int
rte_pmd_dpaa2_mux_flow_l2(uint32_t dpdmux_id,
	uint8_t mac_addr[6], uint16_t vlan_id, int dest_if);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Dump demultiplex ethernet traffic counters
 *
 * @param f
 *    output stream
 * @param dpdmux_id
 *    ID of the DPDMUX MC object.
 * @param num_if
 *    number of interface in dpdmux object
 *
 */
__rte_experimental
void
rte_pmd_dpaa2_mux_dump_counter(FILE *f, uint32_t dpdmux_id, int num_if);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * demultiplex interface max rx frame length configure
 *
 * @param dpdmux_id
 *    ID of the DPDMUX MC object.
 * @param max_rx_frame_len
 *    maximum receive frame length (will be checked to be minimux of all dpnis)
 *
 */
__rte_experimental
int
rte_pmd_dpaa2_mux_rx_frame_len(uint32_t dpdmux_id, uint16_t max_rx_frame_len);

/**
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
int
rte_pmd_dpaa2_set_custom_hash(uint16_t port_id,
			      uint16_t offset,
			      uint8_t size);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Do thread specific initialization
 */
__rte_experimental
void
rte_pmd_dpaa2_thread_init(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Generate the DPAA2 WRIOP based hash value
 *
 * @param key
 *    Array of key data
 * @param size
 *    Size of the hash input key in bytes
 *
 * @return
 *   - 0 if successful.
 *   - Negative in case of failure.
 */

__rte_experimental
uint32_t
rte_pmd_dpaa2_get_tlu_hash(uint8_t *key, int size);

__rte_experimental
int
rte_pmd_dpaa2_dev_is_dpaa2(uint32_t eth_id);
__rte_experimental
const char *
rte_pmd_dpaa2_ep_name(uint32_t eth_id);

#if defined(RTE_LIBRTE_IEEE1588)
__rte_experimental
int
rte_pmd_dpaa2_set_one_step_ts(uint16_t port_id, uint16_t offset, uint8_t ch_update);

__rte_experimental
int
rte_pmd_dpaa2_get_one_step_ts(uint16_t port_id, bool mc_query);
#endif
#endif /* _RTE_PMD_DPAA2_H */
