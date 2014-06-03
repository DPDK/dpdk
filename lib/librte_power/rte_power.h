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

#define RTE_POWER_INVALID_FREQ_INDEX (~0)

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
int rte_power_init(unsigned lcore_id);

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
int rte_power_exit(unsigned lcore_id);

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
uint32_t rte_power_freqs(unsigned lcore_id, uint32_t *freqs, uint32_t num);

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
uint32_t rte_power_get_freq(unsigned lcore_id);

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
 *  - 0 on success without frequency chnaged.
 *  - Negative on error.
 */
int rte_power_set_freq(unsigned lcore_id, uint32_t index);

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
 *  - 0 on success without frequency chnaged.
 *  - Negative on error.
 */
int rte_power_freq_up(unsigned lcore_id);

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
 *  - 0 on success without frequency chnaged.
 *  - Negative on error.
 */
int rte_power_freq_down(unsigned lcore_id);

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
 *  - 0 on success without frequency chnaged.
 *  - Negative on error.
 */
int rte_power_freq_max(unsigned lcore_id);

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
 *  - 0 on success without frequency chnaged.
 *  - Negative on error.
 */
int rte_power_freq_min(unsigned lcore_id);

#ifdef __cplusplus
}
#endif

#endif
