/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Copyright(c) 2024 Advanced Micro Devices, Inc.
 */

#ifndef POWER_UNCORE_OPS_H
#define POWER_UNCORE_OPS_H

/**
 * @file
 * Uncore Frequency Management
 */

#include <rte_compat.h>
#include <rte_common.h>

#define RTE_POWER_UNCORE_DRIVER_NAMESZ 24

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
 * - 0 on success.
 * - Negative on error.
 */
typedef int (*rte_power_uncore_init_t)(unsigned int pkg, unsigned int die);

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
 * - 0 on success.
 * - Negative on error.
 */
typedef int (*rte_power_uncore_exit_t)(unsigned int pkg, unsigned int die);

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
 * @param index
 *  The index of available frequencies.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
typedef int (*rte_power_set_uncore_freq_t)(unsigned int pkg, unsigned int die, uint32_t index);

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
/**
 * Function pointers for generic frequency change functions.
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
typedef void (*rte_power_uncore_driver_cb_t)(void);

/** Structure defining uncore power operations structure */
struct rte_power_uncore_ops {
	RTE_TAILQ_ENTRY(rte_power_uncore_ops) next;     /**< Next in list. */
	char name[RTE_POWER_UNCORE_DRIVER_NAMESZ];      /**< power mgmt driver. */
	rte_power_uncore_driver_cb_t cb;                /**< Driver specific callbacks. */
	rte_power_uncore_init_t init;                   /**< Initialize power management. */
	rte_power_uncore_exit_t exit;                   /**< Exit power management. */
	rte_power_uncore_get_num_pkgs_t get_num_pkgs;
	rte_power_uncore_get_num_dies_t get_num_dies;
	rte_power_uncore_get_num_freqs_t get_num_freqs; /**< Number of available frequencies. */
	rte_power_uncore_freqs_t get_avail_freqs;       /**< Get the available frequencies. */
	rte_power_get_uncore_freq_t get_freq;           /**< Get frequency index. */
	rte_power_set_uncore_freq_t set_freq;           /**< Set frequency index. */
	rte_power_uncore_freq_change_t freq_max;        /**< Scale up frequency to highest. */
	rte_power_uncore_freq_change_t freq_min;        /**< Scale up frequency to lowest. */
};

/**
 * Register power uncore frequency operations.
 * @param ops
 *   Pointer to an ops structure to register.
 * @return
 *  - 0: Success.
 *  - Negative on error.
 */
__rte_internal
int rte_power_register_uncore_ops(struct rte_power_uncore_ops *ops);

/**
 * Macro to statically register the ops of an uncore driver.
 */
#define RTE_POWER_REGISTER_UNCORE_OPS(ops) \
RTE_INIT(power_hdlr_init_uncore_##ops) \
{ \
	rte_power_register_uncore_ops(&ops); \
}

#endif /* POWER_UNCORE_OPS_H */
