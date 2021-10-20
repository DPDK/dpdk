# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
# only use ioat rawdev driver if we don't have the equivalent dmadev ones
if dpdk_conf.has('RTE_DMA_IDXD') and dpdk_conf.has('RTE_DMA_IOAT')
    build = false
    reason = 'replaced by dmadev drivers'
    subdir_done()
endif

reason = 'only supported on x86'
sources = files(
        'ioat_common.c',
        'ioat_rawdev_test.c',
)

if not dpdk_conf.has('RTE_DMA_IDXD')
    sources += files(
            'idxd_bus.c',
            'idxd_pci.c',
    )
endif

if not dpdk_conf.has('RTE_DMA_IOAT')
    sources += files (
            'ioat_rawdev.c',
    )
endif

deps += ['bus_pci', 'mbuf', 'rawdev']
headers = files(
        'rte_ioat_rawdev.h',
        'rte_idxd_rawdev_fns.h',
        'rte_ioat_rawdev_fns.h',
)
