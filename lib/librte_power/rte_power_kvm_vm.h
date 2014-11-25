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

#ifndef _RTE_POWER_KVM_VM_H
#define _RTE_POWER_KVM_VM_H

/**
 * @file
 * RTE Power Management KVM VM
 */

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize power management for a specific lcore.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_init(unsigned lcore_id);

/**
 * Exit power management on a specific lcore.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_exit(unsigned lcore_id);

/**
 * Get the available frequencies of a specific lcore.
 * It is not currently supported for VM Power Management.
 *
 * @param lcore_id
 *  lcore id.
 * @param freqs
 *  The buffer array to save the frequencies.
 * @param num
 *  The number of frequencies to get.
 *
 * @return
 *  -ENOTSUP
 */
uint32_t rte_power_kvm_vm_freqs(unsigned lcore_id, uint32_t *freqs,
		uint32_t num);

/**
 * Return the current index of available frequencies of a specific lcore.
 * It is not currently supported for VM Power Management.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  -ENOTSUP
 */
uint32_t rte_power_kvm_vm_get_freq(unsigned lcore_id);

/**
 * Set the new frequency for a specific lcore by indicating the index of
 * available frequencies.
 * It is not currently supported for VM Power Management.
 *
 * @param lcore_id
 *  lcore id.
 * @param index
 *  The index of available frequencies.
 *
 * @return
 *  -ENOTSUP
 */
int rte_power_kvm_vm_set_freq(unsigned lcore_id, uint32_t index);

/**
 * Scale up the frequency of a specific lcore. This request is forwarded to the
 * host monitor.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_freq_up(unsigned lcore_id);

/**
 * Scale down the frequency of a specific lcore according to the available
 * frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_freq_down(unsigned lcore_id);

/**
 * Scale up the frequency of a specific lcore to the highest according to the
 * available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_freq_max(unsigned lcore_id);

/**
 * Scale down the frequency of a specific lcore to the lowest according to the
 * available frequencies.
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int rte_power_kvm_vm_freq_min(unsigned lcore_id);

#ifdef __cplusplus
}
#endif

#endif
