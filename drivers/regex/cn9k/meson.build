# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

lib = cc.find_library('rxp_compiler', required: false)
if lib.found()
    ext_deps += lib
    ext_deps += cc.find_library('stdc++', required: true)
    cflags += ['-DREE_COMPILER_SDK']
endif

sources = files(
        'cn9k_regexdev.c',
        'cn9k_regexdev_compiler.c',
)

deps += ['bus_pci', 'regexdev']
deps += ['common_cnxk', 'mempool_cnxk']
