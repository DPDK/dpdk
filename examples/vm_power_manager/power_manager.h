/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif
struct core_details {
	uint64_t last_branches;
	uint64_t last_branch_misses;
	uint16_t global_enabled_cpus;
	uint16_t oob_enabled;
	int msr_fd;
};

struct core_info {
	uint16_t core_count;
	struct core_details *cd;
	float branch_ratio_threshold;
};

#define BRANCH_RATIO_THRESHOLD 0.1

struct core_info *
get_core_info(void);

int
core_info_init(void);

#define RTE_LOGTYPE_POWER_MANAGER RTE_LOGTYPE_USER1

/**
 * Initialize power management.
 * Initializes resources and verifies the number of CPUs on the system.
 * Wraps librte_power int rte_power_init(unsigned lcore_id);
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_manager_init(void);

/**
 * Exit power management. Must be called prior to exiting the application.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_manager_exit(void);

/**
 * Scale up the frequency of the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_scale_mask_up(uint64_t core_mask);

/**
 * Scale down the frequency of the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_scale_mask_down(uint64_t core_mask);

/**
 * Scale to the minimum frequency of the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_scale_mask_min(uint64_t core_mask);

/**
 * Scale to the maximum frequency of the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_scale_mask_max(uint64_t core_mask);

/**
 * Enable Turbo Boost on the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_enable_turbo_mask(uint64_t core_mask);

/**
 * Disable Turbo Boost on the cores specified in core_mask.
 * It is thread-safe.
 *
 * @param core_mask
 *  The uint64_t bit-mask of cores to change frequency.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_disable_turbo_mask(uint64_t core_mask);

/**
 * Scale up frequency for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to change frequency
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_scale_core_up(unsigned core_num);

/**
 * Scale down frequency for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to change frequency
 *
 * @return
 *  - 1 on success.
 *  - 0 if frequency not changed.
 *  - Negative on error.
 */
int power_manager_scale_core_down(unsigned core_num);

/**
 * Scale to minimum frequency for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to change frequency
 *
 * @return
 *  - 1 on success.
 *  - 0 if frequency not changed.
 *  - Negative on error.
 */
int power_manager_scale_core_min(unsigned core_num);

/**
 * Scale to maximum frequency for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to change frequency
 *
 * @return
 *  - 1 on success.
 *  - 0 if frequency not changed.
 *  - Negative on error.
 */
int power_manager_scale_core_max(unsigned core_num);

/**
 * Enable Turbo Boost for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to boost
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_enable_turbo_core(unsigned int core_num);

/**
 * Disable Turbo Boost for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to boost
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_manager_disable_turbo_core(unsigned int core_num);

/**
 * Get the current freuency of the core specified by core_num
 *
 * @param core_num
 *  The core number to get the current frequency
 *
 * @return
 *  - 0  on error
 *  - >0 for current frequency.
 */
uint32_t power_manager_get_current_frequency(unsigned core_num);

/**
 * Scale to medium frequency for the core specified by core_num.
 * It is thread-safe.
 *
 * @param core_num
 *  The core number to change frequency
 *
 * @return
 *  - 1 on success.
 *  - 0 if frequency not changed.
 *  - Negative on error.
 */
int power_manager_scale_core_med(unsigned int core_num);

#ifdef __cplusplus
}
#endif


#endif /* POWER_MANAGER_H_ */
