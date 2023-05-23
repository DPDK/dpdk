# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'cnxk_mempool.c',
        'cnxk_mempool_ops.c',
        'cnxk_mempool_telemetry.c',
        'cn9k_mempool_ops.c',
        'cn10k_mempool_ops.c',
        'cn10k_hwpool_ops.c',
)

headers = files('rte_pmd_cnxk_mempool.h')
deps += ['eal', 'mbuf', 'kvargs', 'bus_pci', 'common_cnxk', 'mempool']
require_iova_in_mbuf = false
