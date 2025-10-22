# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 NebulaMatrix Technology Co., Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif

includes += include_directories('nbl_include')

sources = files(
        'nbl_ethdev.c',
        'nbl_core.c',
)
