/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _POWER_ACPI_CPUFREQ_H
#define _POWER_ACPI_CPUFREQ_H

/**
 * @file
 * RTE Power Management via userspace ACPI cpufreq
 */

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include "rte_power.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if ACPI power management is supported.
 *
 * @return
 *   - 1 if supported
 *   - 0 if unsupported
 *   - -1 if error, with rte_errno indicating reason for error.
 */
int power_acpi_cpufreq_check_supported(void);

/**
 * Initialize power management for a specific lcore. It will check and set the
 * governor to userspace for the lcore, get the available frequencies, and
 * prepare to set new lcore frequency.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_acpi_cpufreq_init(unsigned int lcore_id);

/**
 * Exit power management on a specific lcore. It will set the governor to which
 * is before initialized.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_acpi_cpufreq_exit(unsigned int lcore_id);

/**
 * Get the available frequencies of a specific lcore. The return value will be
 * the minimal one of the total number of available frequencies and the number
 * of buffer. The index of available frequencies used in other interfaces
 * should be in the range of 0 to this return value.
 * It should be protected outside of this function for threadsafe.
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
uint32_t power_acpi_cpufreq_freqs(unsigned int lcore_id, uint32_t *freqs,
		uint32_t num);

/**
 * Return the current index of available frequencies of a specific lcore. It
 * will return 'RTE_POWER_INVALID_FREQ_INDEX = (~0)' if error.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  The current index of available frequencies.
 */
uint32_t power_acpi_cpufreq_get_freq(unsigned int lcore_id);

/**
 * Set the new frequency for a specific lcore by indicating the index of
 * available frequencies.
 * It should be protected outside of this function for threadsafe.
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
int power_acpi_cpufreq_set_freq(unsigned int lcore_id, uint32_t index);

/**
 * Scale up the frequency of a specific lcore according to the available
 * frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int power_acpi_cpufreq_freq_up(unsigned int lcore_id);

/**
 * Scale down the frequency of a specific lcore according to the available
 * frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int power_acpi_cpufreq_freq_down(unsigned int lcore_id);

/**
 * Scale up the frequency of a specific lcore to the highest according to the
 * available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int power_acpi_cpufreq_freq_max(unsigned int lcore_id);

/**
 * Scale down the frequency of a specific lcore to the lowest according to the
 * available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success with frequency changed.
 *  - 0 on success without frequency changed.
 *  - Negative on error.
 */
int power_acpi_cpufreq_freq_min(unsigned int lcore_id);

/**
 * Get the turbo status of a specific lcore.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 Turbo Boost is enabled on this lcore.
 *  - 0 Turbo Boost is disabled on this lcore.
 *  - Negative on error.
 */
int power_acpi_turbo_status(unsigned int lcore_id);

/**
 * Enable Turbo Boost on a specific lcore.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 Turbo Boost is enabled successfully on this lcore.
 *  - Negative on error.
 */
int power_acpi_enable_turbo(unsigned int lcore_id);

/**
 * Disable Turbo Boost on a specific lcore.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 Turbo Boost disabled successfully on this lcore.
 *  - Negative on error.
 */
int power_acpi_disable_turbo(unsigned int lcore_id);

/**
 * Returns power capabilities for a specific lcore.
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
int power_acpi_get_capabilities(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps);

#ifdef __cplusplus
}
#endif

#endif
