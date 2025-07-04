# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2025 Marvell International Ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

headers = files('rte_pmu.h')
sources = files('pmu.c')

indirect_headers += files(
        'rte_pmu_pmc_arm64.h',
        'rte_pmu_pmc_x86_64.h',
)

if dpdk_conf.has('RTE_ARCH_ARM64')
    sources += files('pmu_arm64.c')
endif

deps += ['log']
