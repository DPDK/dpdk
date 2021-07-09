/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_POWER_PMD_MGMT_H
#define _RTE_POWER_PMD_MGMT_H

/**
 * @file
 * RTE PMD Power Management
 */

#include <stdint.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_power.h>
#include <rte_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PMD Power Management Type
 */
enum rte_power_pmd_mgmt_type {
	/** Use power-optimized monitoring to wait for incoming traffic */
	RTE_POWER_MGMT_TYPE_MONITOR = 1,
	/** Use power-optimized sleep to avoid busy polling */
	RTE_POWER_MGMT_TYPE_PAUSE,
	/** Use frequency scaling when traffic is low */
	RTE_POWER_MGMT_TYPE_SCALE,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Enable power management on a specified Ethernet device Rx queue and lcore.
 *
 * @note This function is not thread-safe.
 *
 * @warning This function must be called when all affected Ethernet queues are
 *   stopped and no Rx/Tx is in progress!
 *
 * @param lcore_id
 *   The lcore the Rx queue will be polled from.
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue identifier of the Ethernet device.
 * @param mode
 *   The power management scheme to use for specified Rx queue.
 * @return
 *   0 on success
 *   <0 on error
 */
__rte_experimental
int
rte_power_ethdev_pmgmt_queue_enable(unsigned int lcore_id,
		uint16_t port_id, uint16_t queue_id,
		enum rte_power_pmd_mgmt_type mode);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice.
 *
 * Disable power management on a specified Ethernet device Rx queue and lcore.
 *
 * @note This function is not thread-safe.
 *
 * @warning This function must be called when all affected Ethernet queues are
 *   stopped and no Rx/Tx is in progress!
 *
 * @param lcore_id
 *   The lcore the Rx queue is polled from.
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param queue_id
 *   The queue identifier of the Ethernet device.
 * @return
 *   0 on success
 *   <0 on error
 */
__rte_experimental
int
rte_power_ethdev_pmgmt_queue_disable(unsigned int lcore_id,
		uint16_t port_id, uint16_t queue_id);

#ifdef __cplusplus
}
#endif

#endif
