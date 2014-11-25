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

#include <rte_atomic.h>

#include "rte_power.h"
#include "rte_power_acpi_cpufreq.h"
#include "rte_power_kvm_vm.h"
#include "rte_power_common.h"

enum power_management_env global_default_env = PM_ENV_NOT_SET;

volatile uint32_t global_env_cfg_status = 0;

/* function pointers */
rte_power_freqs_t rte_power_freqs  = NULL;
rte_power_get_freq_t rte_power_get_freq = NULL;
rte_power_set_freq_t rte_power_set_freq = NULL;
rte_power_freq_change_t rte_power_freq_up = NULL;
rte_power_freq_change_t rte_power_freq_down = NULL;
rte_power_freq_change_t rte_power_freq_max = NULL;
rte_power_freq_change_t rte_power_freq_min = NULL;

int
rte_power_set_env(enum power_management_env env)
{
	if (rte_atomic32_cmpset(&global_env_cfg_status, 0, 1) == 0) {
		return 0;
	}
	if (env == PM_ENV_ACPI_CPUFREQ) {
		rte_power_freqs = rte_power_acpi_cpufreq_freqs;
		rte_power_get_freq = rte_power_acpi_cpufreq_get_freq;
		rte_power_set_freq = rte_power_acpi_cpufreq_set_freq;
		rte_power_freq_up = rte_power_acpi_cpufreq_freq_up;
		rte_power_freq_down = rte_power_acpi_cpufreq_freq_down;
		rte_power_freq_min = rte_power_acpi_cpufreq_freq_min;
		rte_power_freq_max = rte_power_acpi_cpufreq_freq_max;
	} else if (env == PM_ENV_KVM_VM) {
		rte_power_freqs = rte_power_kvm_vm_freqs;
		rte_power_get_freq = rte_power_kvm_vm_get_freq;
		rte_power_set_freq = rte_power_kvm_vm_set_freq;
		rte_power_freq_up = rte_power_kvm_vm_freq_up;
		rte_power_freq_down = rte_power_kvm_vm_freq_down;
		rte_power_freq_min = rte_power_kvm_vm_freq_min;
		rte_power_freq_max = rte_power_kvm_vm_freq_max;
	} else {
		RTE_LOG(ERR, POWER, "Invalid Power Management Environment(%d) set\n",
				env);
		rte_power_unset_env();
		return -1;
	}
	global_default_env = env;
	return 0;

}

void
rte_power_unset_env(void)
{
	if (rte_atomic32_cmpset(&global_env_cfg_status, 1, 0) != 0)
		global_default_env = PM_ENV_NOT_SET;
}

enum power_management_env
rte_power_get_env(void) {
	return global_default_env;
}

int
rte_power_init(unsigned lcore_id)
{
	int ret = -1;

	if (global_default_env == PM_ENV_ACPI_CPUFREQ) {
		return rte_power_acpi_cpufreq_init(lcore_id);
	}
	if (global_default_env == PM_ENV_KVM_VM) {
		return rte_power_kvm_vm_init(lcore_id);
	}
	/* Auto detect Environment */
	RTE_LOG(INFO, POWER, "Attempting to initialise ACPI cpufreq power "
			"management...\n");
	ret = rte_power_acpi_cpufreq_init(lcore_id);
	if (ret == 0) {
		rte_power_set_env(PM_ENV_ACPI_CPUFREQ);
		goto out;
	}

	RTE_LOG(INFO, POWER, "Attempting to initialise VM power management...\n");
	ret = rte_power_kvm_vm_init(lcore_id);
	if (ret == 0) {
		rte_power_set_env(PM_ENV_KVM_VM);
		goto out;
	}
	RTE_LOG(ERR, POWER, "Unable to set Power Management Environment for lcore "
			"%u\n", lcore_id);
out:
	return ret;
}

int
rte_power_exit(unsigned lcore_id)
{
	if (global_default_env == PM_ENV_ACPI_CPUFREQ)
		return rte_power_acpi_cpufreq_exit(lcore_id);
	if (global_default_env == PM_ENV_KVM_VM)
		return rte_power_kvm_vm_exit(lcore_id);

	RTE_LOG(ERR, POWER, "Environment has not been set, unable to exit "
				"gracefully\n");
	return -1;

}
