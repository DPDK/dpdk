/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_POWER_H
#define _RTE_POWER_H

/**
 * @file
 * RTE Power Management
 */

#include <rte_common.h>
#include <rte_log.h>
#include <rte_power_guest_channel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Power Management Environment State */
enum power_management_env {PM_ENV_NOT_SET, PM_ENV_ACPI_CPUFREQ, PM_ENV_KVM_VM,
		PM_ENV_PSTATE_CPUFREQ, PM_ENV_CPPC_CPUFREQ};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Check if a specific power management environment type is supported on a
 * currently running system.
 *
 * @param env
 *   The environment type to check support for.
 *
 * @return
 *   - 1 if supported
 *   - 0 if unsupported
 *   - -1 if error, with rte_errno indicating reason for error.
 */
__rte_experimental
int rte_power_check_env_supported(enum power_management_env env);

/**
 * Set the default power management implementation. If this is not called prior
 * to rte_power_init(), then auto-detect of the environment will take place.
 * It is thread safe. New env can be set only in uninitialized state
 * (thus rte_power_unset_env must be called if different env was already set).
 *
 * @param env
 *  env. The environment in which to initialise Power Management for.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_set_env(enum power_management_env env);

/**
 * Unset the global environment configuration.
 * This can only be called after all threads have completed.
 */
void rte_power_unset_env(void);

/**
 * Get the default power management implementation.
 *
 * @return
 *  power_management_env The configured environment.
 */
enum power_management_env rte_power_get_env(void);

/**
 * Initialize power management for a specific lcore. If rte_power_set_env() has
 * not been called then an auto-detect of the environment will start and
 * initialise the corresponding resources.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_init(unsigned int lcore_id);

/**
 * Exit power management on a specific lcore. This will call the environment
 * dependent exit function.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_exit(unsigned int lcore_id);

/**
 * Get the available frequencies of a specific lcore.
 * Function pointer definition. Review each environments
 * specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 * @param freqs
 *  The buffer array to save the frequencies.
 * @param num
 *  The number of frequencies to get.
 *
 * @return
 *  The number of available frequencies.
 */
typedef uint32_t (*rte_power_freqs_t)(unsigned int lcore_id, uint32_t *freqs,
		uint32_t num);

extern rte_power_freqs_t rte_power_freqs;

/**
 * Return the current index of available frequencies of a specific lcore.
 * Function pointer definition. Review each environments
 * specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  The current index of available frequencies.
 */
typedef uint32_t (*rte_power_get_freq_t)(unsigned int lcore_id);

extern rte_power_get_freq_t rte_power_get_freq;

/**
 * Set the new frequency for a specific lcore by indicating the index of
 * available frequencies.
 * Function pointer definition. Review each environments
 * specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 * @param index
 *  The index of available frequencies.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
typedef int (*rte_power_set_freq_t)(unsigned int lcore_id, uint32_t index);

extern rte_power_set_freq_t rte_power_set_freq;

/**
 * Function pointer definition for generic frequency change functions. Review
 * each environments specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
typedef int (*rte_power_freq_change_t)(unsigned int lcore_id);

/**
 * Scale up the frequency of a specific lcore according to the available
 * frequencies.
 * Review each environments specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_freq_up;

/**
 * Scale down the frequency of a specific lcore according to the available
 * frequencies.
 * Review each environments specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */

extern rte_power_freq_change_t rte_power_freq_down;

/**
 * Scale up the frequency of a specific lcore to the highest according to the
 * available frequencies.
 * Review each environments specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_freq_max;

/**
 * Scale down the frequency of a specific lcore to the lowest according to the
 * available frequencies.
 * Review each environments specific documentation for usage..
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_freq_min;

/**
 * Query the Turbo Boost status of a specific lcore.
 * Review each environments specific documentation for usage..
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 Turbo Boost is enabled for this lcore.
 *  - 0 Turbo Boost is disabled for this lcore.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_turbo_status;

/**
 * Enable Turbo Boost for this lcore.
 * Review each environments specific documentation for usage..
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_freq_enable_turbo;

/**
 * Disable Turbo Boost for this lcore.
 * Review each environments specific documentation for usage..
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
extern rte_power_freq_change_t rte_power_freq_disable_turbo;

/**
 * Power capabilities summary.
 */
struct rte_power_core_capabilities {
	RTE_STD_C11
	union {
		uint64_t capabilities;
		RTE_STD_C11
		struct {
			uint64_t turbo:1;	/**< Turbo can be enabled. */
			uint64_t priority:1;	/**< SST-BF high freq core */
		};
	};
};

/**
 * Returns power capabilities for a specific lcore.
 * Function pointer definition. Review each environments
 * specific documentation for usage.
 *
 * @param lcore_id
 *  lcore id.
 * @param caps
 *  pointer to rte_power_core_capabilities object.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
typedef int (*rte_power_get_capabilities_t)(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps);

extern rte_power_get_capabilities_t rte_power_get_capabilities;

#ifdef __cplusplus
}
#endif

#endif
