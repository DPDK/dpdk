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

#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of CPUS to manage */
#define POWER_MGR_MAX_CPUS 64
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


#ifdef __cplusplus
}
#endif


#endif /* POWER_MANAGER_H_ */
