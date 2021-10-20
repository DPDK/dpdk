# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
reason = 'only supported on x86'

deps += ['bus_pci']
sources = files(
        'idxd_common.c',
        'idxd_pci.c',
)

if is_linux
    sources += files('idxd_bus.c')
endif
