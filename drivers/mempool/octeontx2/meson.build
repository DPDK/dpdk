# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'otx2_mempool.c',
        'otx2_mempool_debug.c',
        'otx2_mempool_irq.c',
        'otx2_mempool_ops.c',
)

deps += ['eal', 'mbuf', 'kvargs', 'bus_pci', 'common_octeontx2', 'mempool']
