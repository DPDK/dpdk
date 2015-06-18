/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_POWER_H
#define _RTE_POWER_H

/**
 * @file
 * RTE Power Management
 */

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Power Management Environment State */
enum power_management_env {PM_ENV_NOT_SET, PM_ENV_ACPI_CPUFREQ, PM_ENV_KVM_VM};

/**
 * Set the default power management implementation. If this is not called prior
 * to rte_power_init(), then auto-detect of the environment will take place.
 * It is not thread safe.
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
int rte_power_init(unsigned lcore_id);

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
int rte_power_exit(unsigned lcore_id);

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
typedef uint32_t (*rte_power_freqs_t)(unsigned lcore_id, uint32_t *freqs,
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
typedef uint32_t (*rte_power_get_freq_t)(unsigned lcore_id);

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
typedef int (*rte_power_set_freq_t)(unsigned lcore_id, uint32_t index);

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
typedef int (*rte_power_freq_change_t)(unsigned lcore_id);

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

#ifdef __cplusplus
}
#endif

#endif
