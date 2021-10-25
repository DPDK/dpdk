# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['bus_pci']
deps += ['bus_vdev']
deps += ['common_cpt']
deps += ['eventdev']

sources = files(
        'otx_cryptodev.c',
        'otx_cryptodev_capabilities.c',
        'otx_cryptodev_hw_access.c',
        'otx_cryptodev_mbox.c',
        'otx_cryptodev_ops.c',
)

includes += include_directories('../../common/cpt')
includes += include_directories('../../common/octeontx')
includes += include_directories('../../event/octeontx')
includes += include_directories('../../mempool/octeontx')
includes += include_directories('../../net/octeontx')
