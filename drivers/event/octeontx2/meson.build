# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'otx2_worker.c',
        'otx2_worker_dual.c',
        'otx2_evdev.c',
        'otx2_evdev_adptr.c',
        'otx2_evdev_crypto_adptr.c',
        'otx2_evdev_irq.c',
        'otx2_evdev_selftest.c',
        'otx2_tim_evdev.c',
        'otx2_tim_worker.c',
)

deps += ['bus_pci', 'common_octeontx2', 'crypto_octeontx2', 'mempool_octeontx2', 'net_octeontx2']

includes += include_directories('../../crypto/octeontx2')
includes += include_directories('../../common/cpt')
