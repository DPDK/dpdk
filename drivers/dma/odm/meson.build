# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2024 Marvell.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

deps += ['bus_pci', 'dmadev', 'eal', 'mempool', 'pci']

sources = files('odm_dmadev.c', 'odm.c')

pmd_supports_disable_iova_as_pa = true
