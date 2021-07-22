# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc

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
