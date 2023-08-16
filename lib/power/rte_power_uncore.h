/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Copyright(c) 2023 AMD Corporation
 */

#ifndef RTE_POWER_UNCORE_H
#define RTE_POWER_UNCORE_H

/**
 * @file
 * RTE Uncore Frequency Management
 */

#include <rte_compat.h>
#include "rte_power.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Uncore Power Management Environment */
enum rte_uncore_power_mgmt_env {
	RTE_UNCORE_PM_ENV_NOT_SET,
	RTE_UNCORE_PM_ENV_INTEL_UNCORE,
	RTE_UNCORE_PM_ENV_AMD_HSMP
};

/**
 * Set the default uncore power management implementation. If this is not called prior
 * to rte_power_uncore_init(), then auto-detect of the environment will take place.
 * It is thread safe. New env can be set only in uninitialized state
 * (thus rte_power_unset_uncore_env must be called if different env was already set).
 *
 * @param env
 *  env. The environment in which to initialise Uncore Power Management for.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int rte_power_set_uncore_env(enum rte_uncore_power_mgmt_env env);

/**
 * Unset the global uncore environment configuration.
 * This can only be called after all threads have completed.
 */
__rte_experimental
void rte_power_unset_uncore_env(void);

/**
 * Get the default uncore power management implementation.
 *
 * @return
 *  power_management_env The configured environment.
 */
__rte_experimental
enum rte_uncore_power_mgmt_env rte_power_get_uncore_env(void);

/**
 * Initialize uncore frequency management for specific die on a package.
 * It will get the available frequencies and prepare to set new die frequencies.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int
rte_power_uncore_init(unsigned int pkg, unsigned int die);

/**
 * Exit uncore frequency management on a specific die on a package.
 * It will restore uncore min and* max values to previous values
 * before initialization of API.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int
rte_power_uncore_exit(unsigned int pkg, unsigned int die);

/**
 * Return the current index of available frequencies of a specific die on a package.
 * It should be protected outside of this function for threadsafe.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 *
 * @return
 *  The current index of available frequencies.
 *  If error, it will return 'RTE_POWER_INVALID_FREQ_INDEX = (~0)'.
 */
typedef uint32_t (*rte_power_get_uncore_freq_t)(unsigned int pkg, unsigned int die);

extern rte_power_get_uncore_freq_t rte_power_get_uncore_freq;

/**
 * Set minimum and maximum uncore frequency for specified die on a package
 * to specified index value.
 * It should be protected outside of this function for threadsafe.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 * @param index
 *  The index of available frequencies.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
typedef int (*rte_power_set_uncore_freq_t)(unsigned int pkg, unsigned int die, uint32_t index);

extern rte_power_set_uncore_freq_t rte_power_set_uncore_freq;

/**
 * Function pointer definition for generic frequency change functions.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
typedef int (*rte_power_uncore_freq_change_t)(unsigned int pkg, unsigned int die);

/**
 * Set minimum and maximum uncore frequency for specified die on a package
 * to maximum value according to the available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * This function should NOT be called in the fast path.
 */
extern rte_power_uncore_freq_change_t rte_power_uncore_freq_max;

/**
 * Set minimum and maximum uncore frequency for specified die on a package
 * to minimum value according to the available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * This function should NOT be called in the fast path.
 */
extern rte_power_uncore_freq_change_t rte_power_uncore_freq_min;

/**
 * Return the list of available frequencies in the index array.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 * @param freqs
 *  The buffer array to save the frequencies.
 * @param num
 *  The number of frequencies to get.
 *
 * @return
 *  - The number of available index's in frequency array.
 *  - Negative on error.
 */
typedef int (*rte_power_uncore_freqs_t)(unsigned int pkg, unsigned int die,
		uint32_t *freqs, uint32_t num);

extern rte_power_uncore_freqs_t rte_power_uncore_freqs;

/**
 * Return the list length of available frequencies in the index array.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 * @param die
 *  Die number.
 *  Each package can have several dies connected together via the uncore mesh.
 *
 * @return
 *  - The number of available index's in frequency array.
 *  - Negative on error.
 */
typedef int (*rte_power_uncore_get_num_freqs_t)(unsigned int pkg, unsigned int die);

extern rte_power_uncore_get_num_freqs_t rte_power_uncore_get_num_freqs;

/**
 * Return the number of packages (CPUs) on a system
 * by parsing the uncore sysfs directory.
 *
 * This function should NOT be called in the fast path.
 *
 * @return
 *  - Zero on error.
 *  - Number of package on system on success.
 */
typedef unsigned int (*rte_power_uncore_get_num_pkgs_t)(void);

extern rte_power_uncore_get_num_pkgs_t rte_power_uncore_get_num_pkgs;

/**
 * Return the number of dies for pakckages (CPUs) specified
 * from parsing the uncore sysfs directory.
 *
 * This function should NOT be called in the fast path.
 *
 * @param pkg
 *  Package number.
 *  Each physical CPU in a system is referred to as a package.
 *
 * @return
 *  - Zero on error.
 *  - Number of dies for package on sucecss.
 */
typedef unsigned int (*rte_power_uncore_get_num_dies_t)(unsigned int pkg);

extern rte_power_uncore_get_num_dies_t rte_power_uncore_get_num_dies;

#ifdef __cplusplus
}
#endif

#endif /* RTE_POWER_UNCORE_H */
