/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _POWER_KVM_VM_H
#define _POWER_KVM_VM_H

/**
 * @file
 * RTE Power Management KVM VM
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
 * Initialize power management for a specific lcore.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_kvm_vm_init(unsigned int lcore_id);

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
int power_kvm_vm_exit(unsigned int lcore_id);

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
uint32_t power_kvm_vm_freqs(unsigned int lcore_id, uint32_t *freqs,
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
uint32_t power_kvm_vm_get_freq(unsigned int lcore_id);

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
int power_kvm_vm_set_freq(unsigned int lcore_id, uint32_t index);

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
int power_kvm_vm_freq_up(unsigned int lcore_id);

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
int power_kvm_vm_freq_down(unsigned int lcore_id);

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
int power_kvm_vm_freq_max(unsigned int lcore_id);

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
int power_kvm_vm_freq_min(unsigned int lcore_id);

/**
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  -ENOTSUP
 */
int power_kvm_vm_turbo_status(unsigned int lcore_id);

/**
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_kvm_vm_enable_turbo(unsigned int lcore_id);

/**
 * It should be protected outside of this function for threadsafe.
 *
 * @param lcore_id
 *  lcore id.
 *
 * @return
 *  - 1 on success.
 *  - Negative on error.
 */
int power_kvm_vm_disable_turbo(unsigned int lcore_id);

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
int power_kvm_vm_get_capabilities(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps);

#ifdef __cplusplus
}
#endif
#endif
