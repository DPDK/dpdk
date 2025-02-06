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

cflags += no_wvla_cflag

sources = files(
        'power_common.c',
        'rte_power_cpufreq.c',
        'rte_power_pmd_mgmt.c',
        'rte_power_qos.c',
        'rte_power_uncore.c',
)
headers = files(
        'power_cpufreq.h',
        'power_uncore_ops.h',
        'rte_power_cpufreq.h',
        'rte_power_pmd_mgmt.h',
        'rte_power_qos.h',
        'rte_power_uncore.h',
)

deps += ['timer', 'ethdev']
