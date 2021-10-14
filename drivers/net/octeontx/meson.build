# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'octeontx_ethdev.c',
        'octeontx_ethdev_ops.c',
        'octeontx_rxtx.c',
)

deps += ['mempool_octeontx', 'eventdev']

includes += include_directories('base')
