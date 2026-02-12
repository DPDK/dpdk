# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2025 Marvell International Ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

headers = files('rte_pmu.h')
sources = files('pmu.c')

if dpdk_conf.has('RTE_ARCH_ARM64')
    indirect_headers += files('rte_pmu_pmc_arm64.h')
    sources += files('pmu_arm64.c')
endif

if dpdk_conf.has('RTE_ARCH_X86_64')
    indirect_headers += files('rte_pmu_pmc_x86_64.h')
endif

deps += ['log']
