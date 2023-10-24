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

#include <rte_log.h>
#include <rte_power.h>

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
int
rte_power_ethdev_pmgmt_queue_enable(unsigned int lcore_id,
		uint16_t port_id, uint16_t queue_id,
		enum rte_power_pmd_mgmt_type mode);

/**
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
int
rte_power_ethdev_pmgmt_queue_disable(unsigned int lcore_id,
		uint16_t port_id, uint16_t queue_id);

/**
 * Set a emptypoll_max to specified value. Used to specify the number of empty
 * polls to wait before entering sleep state.
 *
 * @param max
 *   The value to set emptypoll_max to.
 */
void
rte_power_pmd_mgmt_set_emptypoll_max(unsigned int max);

/**
 * Get the current value of emptypoll_max.
 *
 * @return
 *   The current emptypoll_max value
 */
unsigned int
rte_power_pmd_mgmt_get_emptypoll_max(void);

/**
 * Set the pause_duration. Used to adjust the pause mode callback duration.
 *
 * @note Duration must be greater than zero.
 *
 * @param duration
 *   The value to set pause_duration to.
 * @return
 *   0 on success
 *   <0 on error
 */
int
rte_power_pmd_mgmt_set_pause_duration(unsigned int duration);

/**
 * Get the current value of pause_duration.
 *
 * @return
 *   The current pause_duration value.
 */
unsigned int
rte_power_pmd_mgmt_get_pause_duration(void);

/**
 * Set the min frequency to be used for frequency scaling or zero to use defaults.
 *
 * @note Supported by: Pstate mode.
 *
 * @param lcore
 *   The ID of the lcore to set the min frequency for.
 * @param min
 *   The value, in KiloHertz, to set the minimum frequency to.
 * @return
 *   0 on success
 *   <0 on error
 */
int
rte_power_pmd_mgmt_set_scaling_freq_min(unsigned int lcore, unsigned int min);

/**
 * Set the max frequency to be used for frequency scaling or zero to use defaults.
 *
 * @note Supported by: Pstate mode.
 *
 * @param lcore
 *   The ID of the lcore to set the max frequency for.
 * @param max
 *   The value, in KiloHertz, to set the maximum frequency to.
 *   If 'max' is 0, it is considered 'not set'.
 * @return
 *   0 on success
 *   <0 on error
 */
int
rte_power_pmd_mgmt_set_scaling_freq_max(unsigned int lcore, unsigned int max);

/**
 * Get the current configured min frequency used for frequency scaling.
 *
 * @note Supported by: Pstate mode.
 *
 * @param lcore
 *   The ID of the lcore to get the min frequency for.
 * @return
 *   0 if no value has been configured via the 'set' API.
 *   >0 if a minimum frequency has been configured. Value is the minimum frequency
 *   , in KiloHertz, used for frequency scaling.
 *   <0 on error
 */
int
rte_power_pmd_mgmt_get_scaling_freq_min(unsigned int lcore);

/**
 * Get the current configured max frequency used for frequency scaling.
 *
 * @note Supported by: Pstate mode.
 *
 * @param lcore
 *   The ID of the lcore to get the max frequency for.
 * @return
 *   0 if no value has been configured via the 'set' API.
 *   >0 if a maximum frequency has been configured. Value is the maximum frequency
 *   , in KiloHertz, used for frequency scaling.
 *   <0 on error
 */
int
rte_power_pmd_mgmt_get_scaling_freq_max(unsigned int lcore);

#ifdef __cplusplus
}
#endif

#endif
