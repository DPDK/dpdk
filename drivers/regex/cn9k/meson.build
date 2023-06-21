# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'cn9k_regexdev.c',
)

deps += ['bus_pci', 'regexdev']
deps += ['common_cnxk', 'mempool_cnxk']
