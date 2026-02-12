# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Advanced Micro Devices, Inc.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
sources = files('intel_pstate_cpufreq.c')

deps += ['power']
