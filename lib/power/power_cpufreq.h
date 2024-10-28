/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2024 Advanced Micro Devices, Inc.
 */

#ifndef POWER_CPUFREQ_H
#define POWER_CPUFREQ_H

/**
 * @file
 * CPU Frequency Management
 */

#include <rte_common.h>
#include <rte_log.h>
#include <rte_compat.h>

#define RTE_POWER_DRIVER_NAMESZ 24

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
typedef int (*rte_power_cpufreq_init_t)(unsigned int lcore_id);

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
typedef int (*rte_power_cpufreq_exit_t)(unsigned int lcore_id);

/**
 * Check if a specific power management environment type is supported on a
 * currently running system.
 *
 * @return
 *   - 1 if supported
 *   - 0 if unsupported
 *   - -1 if error, with rte_errno indicating reason for error.
 */
typedef int (*rte_power_check_env_support_t)(void);

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
typedef uint32_t (*rte_power_freqs_t)(unsigned int lcore_id,
		uint32_t *freqs, uint32_t num);

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

/**
 * Power capabilities summary.
 */
struct rte_power_core_capabilities {
	union {
		uint64_t capabilities;
		struct {
			uint64_t turbo:1;       /**< Turbo can be enabled. */
			uint64_t priority:1;    /**< SST-BF high freq core */
		};
	};
};

typedef int (*rte_power_get_capabilities_t)(unsigned int lcore_id,
			struct rte_power_core_capabilities *caps);

/** Structure defining core power operations structure */
struct rte_power_cpufreq_ops {
	RTE_TAILQ_ENTRY(rte_power_cpufreq_ops) next;	/**< Next in list. */
	char name[RTE_POWER_DRIVER_NAMESZ];             /**< power mgmt driver. */
	rte_power_cpufreq_init_t init;                  /**< Initialize power management. */
	rte_power_cpufreq_exit_t exit;                  /**< Exit power management. */
	rte_power_check_env_support_t check_env_support;/**< verify env is supported. */
	rte_power_freqs_t get_avail_freqs;              /**< Get the available frequencies. */
	rte_power_get_freq_t get_freq;                  /**< Get frequency index. */
	rte_power_set_freq_t set_freq;                  /**< Set frequency index. */
	rte_power_freq_change_t freq_up;                /**< Scale up frequency. */
	rte_power_freq_change_t freq_down;              /**< Scale down frequency. */
	rte_power_freq_change_t freq_max;               /**< Scale up frequency to highest. */
	rte_power_freq_change_t freq_min;               /**< Scale up frequency to lowest. */
	rte_power_freq_change_t turbo_status;           /**< Get Turbo status. */
	rte_power_freq_change_t enable_turbo;           /**< Enable Turbo. */
	rte_power_freq_change_t disable_turbo;          /**< Disable Turbo. */
	rte_power_get_capabilities_t get_caps;          /**< power capabilities. */
};

/**
 * Register power cpu frequency operations.
 *
 * @param ops
 *   Pointer to an ops structure to register.
 * @return
 *  - 0: Success.
 *  - Negative on error.
 */
__rte_internal
int rte_power_register_cpufreq_ops(struct rte_power_cpufreq_ops *ops);

/**
 * Macro to statically register the ops of a cpufreq driver.
 */
#define RTE_POWER_REGISTER_CPUFREQ_OPS(ops) \
RTE_INIT(power_hdlr_init_##ops) \
{ \
	rte_power_register_cpufreq_ops(&ops); \
}

#endif /* POWER_CPUFREQ_H */
