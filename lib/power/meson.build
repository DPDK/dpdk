# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
sources = files(
        'guest_channel.c',
        'power_acpi_cpufreq.c',
        'power_amd_pstate_cpufreq.c',
        'power_common.c',
        'power_cppc_cpufreq.c',
        'power_kvm_vm.c',
        'power_intel_uncore.c',
        'power_pstate_cpufreq.c',
        'rte_power.c',
        'rte_power_uncore.c',
        'rte_power_pmd_mgmt.c',
)
headers = files(
        'rte_power.h',
        'rte_power_guest_channel.h',
        'rte_power_pmd_mgmt.h',
        'rte_power_uncore.h',
)
if cc.has_argument('-Wno-cast-qual')
    cflags += '-Wno-cast-qual'
endif
deps += ['timer', 'ethdev']
