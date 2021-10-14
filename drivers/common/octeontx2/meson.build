# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources= files(
        'otx2_common.c',
        'otx2_dev.c',
        'otx2_irq.c',
        'otx2_mbox.c',
        'otx2_sec_idev.c',
)

deps = ['eal', 'pci', 'ethdev', 'kvargs']
includes += include_directories(
        '../../common/octeontx2',
        '../../mempool/octeontx2',
        '../../bus/pci',
)
