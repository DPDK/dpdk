# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
sources = files(
        'guest_channel.c',
        'power_acpi_cpufreq.c',
        'power_common.c',
        'power_kvm_vm.c',
        'power_pstate_cpufreq.c',
        'rte_power.c',
        'rte_power_empty_poll.c',
        'rte_power_pmd_mgmt.c',
)
headers = files(
        'rte_power.h',
        'rte_power_empty_poll.h',
        'rte_power_pmd_mgmt.h',
        'rte_power_guest_channel.h',
)
deps += ['timer', 'ethdev']
