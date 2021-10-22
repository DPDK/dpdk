# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files('otx_zip.c', 'otx_zip_pmd.c')
includes += include_directories('include')
deps += ['mempool_octeontx', 'bus_pci']
ext_deps += dep
