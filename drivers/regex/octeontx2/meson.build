# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

lib = cc.find_library('librxp_compiler', required: false)
if lib.found()
    ext_deps += lib
    ext_deps += cc.find_library('libstdc++', required: true)
    cflags += ['-DREE_COMPILER_SDK']
endif

sources = files(
        'otx2_regexdev.c',
        'otx2_regexdev_compiler.c',
        'otx2_regexdev_hw_access.c',
        'otx2_regexdev_mbox.c',
)

deps += ['bus_pci', 'common_octeontx2', 'regexdev']

includes += include_directories('../../common/octeontx2')
