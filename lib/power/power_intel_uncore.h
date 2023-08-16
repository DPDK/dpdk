/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef POWER_INTEL_UNCORE_H
#define POWER_INTEL_UNCORE_H

/**
 * @file
 * RTE Intel Uncore Frequency Management
 */

#include "rte_power.h"
#include "rte_power_uncore.h"

#ifdef __cplusplus
extern "C" {
#endif

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
int
power_intel_uncore_init(unsigned int pkg, unsigned int die);

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
int
power_intel_uncore_exit(unsigned int pkg, unsigned int die);

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
uint32_t
power_get_intel_uncore_freq(unsigned int pkg, unsigned int die);

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
int
power_set_intel_uncore_freq(unsigned int pkg, unsigned int die, uint32_t index);

/**
 * Set minimum and maximum uncore frequency for specified die on a package
 * to maximum value according to the available frequencies.
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
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int
power_intel_uncore_freq_max(unsigned int pkg, unsigned int die);

/**
 * Set minimum and maximum uncore frequency for specified die on a package
 * to minimum value according to the available frequencies.
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
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int
power_intel_uncore_freq_min(unsigned int pkg, unsigned int die);

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
int
power_intel_uncore_freqs(unsigned int pkg, unsigned int die,
		unsigned int *freqs, unsigned int num);

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
int
power_intel_uncore_get_num_freqs(unsigned int pkg, unsigned int die);

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
unsigned int
power_intel_uncore_get_num_pkgs(void);

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
unsigned int
power_intel_uncore_get_num_dies(unsigned int pkg);

#ifdef __cplusplus
}
#endif

#endif /* POWER_INTEL_UNCORE_H */
