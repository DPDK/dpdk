# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    subdir_done()
endif

drivers = [
        'cnxk',
        'dlb2',
        'dpaa',
        'dpaa2',
        'dsw',
        'octeontx2',
        'opdl',
        'skeleton',
        'sw',
]
if not (toolchain == 'gcc' and cc.version().version_compare('<4.8.6') and
        dpdk_conf.has('RTE_ARCH_ARM64'))
    drivers += 'octeontx'
endif
std_deps = ['eventdev', 'kvargs']
