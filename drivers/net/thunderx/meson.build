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
        'nicvf_ethdev.c',
        'nicvf_rxtx.c',
        'nicvf_svf.c',
)

if cc.has_argument('-fno-prefetch-loop-arrays')
    cflags += '-fno-prefetch-loop-arrays'
endif

if cc.has_argument('-Wno-maybe-uninitialized')
    cflags += '-Wno-maybe-uninitialized'
endif

includes += include_directories('base')
